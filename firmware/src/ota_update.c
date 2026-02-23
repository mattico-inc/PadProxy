#include "ota_update.h"
#include "ota_version.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/flash.h"
#include "hardware/resets.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"

#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"

/* ── Compile-time configuration ──────────────────────────────────────── */

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef GITHUB_OTA_OWNER
#define GITHUB_OTA_OWNER "mattico-inc"
#endif
#ifndef GITHUB_OTA_REPO
#define GITHUB_OTA_REPO "PadProxy"
#endif

#define WIFI_CONNECT_TIMEOUT_MS   15000
#define HTTP_TIMEOUT_MS           20000
#define MAX_REDIRECTS             3

/* ── Flash layout ────────────────────────────────────────────────────── */

/*
 * The Pico 2 W has 4 MB of flash.  We reserve the upper half as a
 * staging area for OTA downloads.  After a successful download the
 * staging image is copied over the active application area and the
 * device reboots.
 *
 *   0x000000 – 0x0FFFFF  Active application (1 MB)
 *   0x100000 – 0x1FFFFF  OTA staging area   (1 MB)
 */
#define OTA_STAGING_OFFSET  (1024 * 1024)      /* 1 MB into flash */
#define OTA_MAX_IMAGE_SIZE  (1024 * 1024)      /* 1 MB max firmware */
#define FLASH_SECTOR_SZ     FLASH_SECTOR_SIZE  /* 4 KB */
#define FLASH_PAGE_SZ       FLASH_PAGE_SIZE    /* 256 B */

/* ── Internal HTTPS client ───────────────────────────────────────────── */

typedef enum {
    HTTP_IDLE,
    HTTP_DNS_WAIT,
    HTTP_CONNECTING,
    HTTP_SENDING,
    HTTP_RECV_HEADERS,
    HTTP_RECV_BODY,
    HTTP_COMPLETE,
    HTTP_ERROR,
} http_state_t;

/**
 * Context for a single HTTPS GET request.
 *
 * The body can be collected into a buffer (for the API call) or
 * streamed through a callback (for the firmware download).
 */
typedef struct {
    http_state_t state;
    struct altcp_pcb *pcb;
    struct altcp_tls_config *tls_cfg;
    ip_addr_t server_ip;

    /* Request */
    char host[128];
    char path[512];
    uint16_t port;

    /* Response */
    int status_code;
    int content_length;

    /* Header accumulation (to find end-of-headers & Location) */
    char hdr_buf[2048];
    int  hdr_len;
    bool headers_done;

    /* Body – buffer mode */
    uint8_t *body_buf;
    int      body_len;
    int      body_cap;

    /* Body – streaming mode */
    bool (*body_cb)(const uint8_t *data, int len, void *ctx);
    void *body_cb_ctx;

    /* Redirect */
    char location[512];

    /* Completion flags */
    bool complete;
    bool error;
    absolute_time_t deadline;
} http_ctx_t;

/* Forward declarations for lwIP callbacks */
static err_t  http_connected_cb(void *arg, struct altcp_pcb *pcb, err_t err);
static err_t  http_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err);
static void   http_err_cb(void *arg, err_t err);
static void   dns_found_cb(const char *name, const ip_addr_t *addr, void *arg);

/* ── URL parsing ─────────────────────────────────────────────────────── */

/**
 * Extract host, port, and path from an https:// URL.
 */
static bool parse_url(const char *url, char *host, int host_len,
                      uint16_t *port, char *path, int path_len)
{
    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else {
        return false;
    }

    /* Host (up to '/', ':', or end) */
    const char *host_start = p;
    while (*p && *p != '/' && *p != ':') p++;
    int hlen = (int)(p - host_start);
    if (hlen == 0 || hlen >= host_len) return false;
    memcpy(host, host_start, (size_t)hlen);
    host[hlen] = '\0';

    /* Port */
    if (*p == ':') {
        p++;
        *port = (uint16_t)atoi(p);
        while (*p >= '0' && *p <= '9') p++;
    } else {
        *port = 443;
    }

    /* Path */
    if (*p == '/') {
        int plen = (int)strlen(p);
        if (plen >= path_len) return false;
        memcpy(path, p, (size_t)(plen + 1));
    } else {
        if (path_len < 2) return false;
        path[0] = '/';
        path[1] = '\0';
    }

    return true;
}

/* ── Header parsing helpers ──────────────────────────────────────────── */

static int parse_status_code(const char *hdr)
{
    /* "HTTP/1.1 200 OK\r\n" */
    const char *p = strstr(hdr, " ");
    if (!p) return 0;
    return atoi(p + 1);
}

/**
 * Find a header value (case-insensitive key match).
 * Copies the value into buf (up to buf_len - 1 chars).
 */
static bool find_header(const char *headers, const char *key,
                        char *buf, int buf_len)
{
    int klen = (int)strlen(key);
    const char *p = headers;
    while (*p) {
        /* Start of a line */
        if (strncasecmp(p, key, (size_t)klen) == 0 && p[klen] == ':') {
            p += klen + 1;
            while (*p == ' ') p++;
            const char *end = strstr(p, "\r\n");
            if (!end) end = p + strlen(p);
            int vlen = (int)(end - p);
            if (vlen >= buf_len) vlen = buf_len - 1;
            memcpy(buf, p, (size_t)vlen);
            buf[vlen] = '\0';
            return true;
        }
        /* Skip to next line */
        const char *nl = strstr(p, "\r\n");
        if (!nl) break;
        p = nl + 2;
    }
    return false;
}

static int find_content_length(const char *headers)
{
    char val[32];
    if (find_header(headers, "Content-Length", val, sizeof(val))) {
        return atoi(val);
    }
    return -1; /* unknown */
}

/* ── Process received data (headers + body) ──────────────────────────── */

static void process_data(http_ctx_t *ctx, const uint8_t *data, int len)
{
    if (ctx->error || ctx->complete) return;

    int offset = 0;

    /* Still accumulating headers? */
    if (!ctx->headers_done) {
        while (offset < len) {
            if (ctx->hdr_len < (int)sizeof(ctx->hdr_buf) - 1) {
                ctx->hdr_buf[ctx->hdr_len++] = (char)data[offset];
                ctx->hdr_buf[ctx->hdr_len] = '\0';
            }
            offset++;

            /* Check for end of headers */
            if (ctx->hdr_len >= 4 &&
                memcmp(&ctx->hdr_buf[ctx->hdr_len - 4], "\r\n\r\n", 4) == 0) {
                ctx->headers_done = true;
                ctx->status_code = parse_status_code(ctx->hdr_buf);
                ctx->content_length = find_content_length(ctx->hdr_buf);
                find_header(ctx->hdr_buf, "Location",
                            ctx->location, sizeof(ctx->location));
                break;
            }
        }
    }

    /* Body data */
    if (ctx->headers_done && offset < len) {
        int body_chunk_len = len - offset;
        const uint8_t *body_data = data + offset;

        if (ctx->body_cb) {
            /* Streaming mode */
            if (!ctx->body_cb(body_data, body_chunk_len, ctx->body_cb_ctx)) {
                ctx->error = true;
                return;
            }
        } else if (ctx->body_buf) {
            /* Buffer mode */
            int room = ctx->body_cap - ctx->body_len - 1; /* leave NUL */
            if (body_chunk_len > room) body_chunk_len = room;
            if (body_chunk_len > 0) {
                memcpy(ctx->body_buf + ctx->body_len, body_data,
                       (size_t)body_chunk_len);
                ctx->body_len += body_chunk_len;
                ctx->body_buf[ctx->body_len] = '\0';
            }
        }
    }
}

/* ── lwIP callbacks ──────────────────────────────────────────────────── */

static void dns_found_cb(const char *name, const ip_addr_t *addr, void *arg)
{
    (void)name;
    http_ctx_t *ctx = (http_ctx_t *)arg;
    if (addr) {
        ctx->server_ip = *addr;
        ctx->state = HTTP_CONNECTING;
    } else {
        ctx->error = true;
        ctx->state = HTTP_ERROR;
    }
}

static err_t http_connected_cb(void *arg, struct altcp_pcb *pcb, err_t err)
{
    http_ctx_t *ctx = (http_ctx_t *)arg;
    if (err != ERR_OK) {
        ctx->error = true;
        ctx->state = HTTP_ERROR;
        return ERR_OK;
    }

    /* Build and send the HTTP request */
    char req[768];
    int n = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: PadProxy-OTA/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "\r\n",
        ctx->path, ctx->host);

    err_t wr = altcp_write(pcb, req, (uint16_t)n, TCP_WRITE_FLAG_COPY);
    if (wr != ERR_OK) {
        ctx->error = true;
        ctx->state = HTTP_ERROR;
        return ERR_OK;
    }
    altcp_output(pcb);
    ctx->state = HTTP_RECV_HEADERS;
    return ERR_OK;
}

static err_t http_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *p,
                          err_t err)
{
    http_ctx_t *ctx = (http_ctx_t *)arg;

    if (!p || err != ERR_OK) {
        /* Connection closed by server */
        ctx->complete = true;
        ctx->state = HTTP_COMPLETE;
        if (p) pbuf_free(p);
        return ERR_OK;
    }

    /* Walk the pbuf chain */
    struct pbuf *q;
    for (q = p; q != NULL; q = q->next) {
        process_data(ctx, (const uint8_t *)q->payload, (int)q->len);
    }

    altcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void http_err_cb(void *arg, err_t err)
{
    (void)err;
    http_ctx_t *ctx = (http_ctx_t *)arg;
    ctx->pcb = NULL; /* lwIP freed the PCB already */
    ctx->error = true;
    ctx->state = HTTP_ERROR;
}

/* ── Blocking HTTPS GET ──────────────────────────────────────────────── */

/**
 * Perform a blocking HTTPS GET.  Spins the lwIP stack until the
 * request completes, errors out, or times out.
 */
static bool https_get(http_ctx_t *ctx)
{
    ctx->state = HTTP_DNS_WAIT;
    ctx->headers_done = false;
    ctx->hdr_len = 0;
    ctx->body_len = 0;
    ctx->status_code = 0;
    ctx->content_length = -1;
    ctx->location[0] = '\0';
    ctx->complete = false;
    ctx->error = false;
    ctx->deadline = make_timeout_time_ms(HTTP_TIMEOUT_MS);

    /* TLS config (no certificate verification for v1) */
    ctx->tls_cfg = altcp_tls_create_config_client(NULL, 0);
    if (!ctx->tls_cfg) {
        printf("[ota] TLS config allocation failed\n");
        return false;
    }

    /* DNS lookup */
    err_t dns_err = dns_gethostbyname(ctx->host, &ctx->server_ip,
                                       dns_found_cb, ctx);
    if (dns_err == ERR_OK) {
        /* Already cached */
        ctx->state = HTTP_CONNECTING;
    } else if (dns_err != ERR_INPROGRESS) {
        printf("[ota] DNS lookup failed: %d\n", dns_err);
        altcp_tls_free_config(ctx->tls_cfg);
        return false;
    }

    /* Wait for DNS */
    while (ctx->state == HTTP_DNS_WAIT) {
        if (absolute_time_diff_us(get_absolute_time(), ctx->deadline) <= 0) {
            printf("[ota] DNS timeout\n");
            altcp_tls_free_config(ctx->tls_cfg);
            return false;
        }
        cyw43_arch_poll();
        sleep_ms(10);
    }
    if (ctx->error) {
        altcp_tls_free_config(ctx->tls_cfg);
        return false;
    }

    /* Create TLS connection */
    ctx->pcb = altcp_tls_new(ctx->tls_cfg, IPADDR_TYPE_V4);
    if (!ctx->pcb) {
        printf("[ota] Failed to create TLS PCB\n");
        altcp_tls_free_config(ctx->tls_cfg);
        return false;
    }

    /* Set SNI hostname for TLS */
    mbedtls_ssl_set_hostname(altcp_tls_context(ctx->pcb), ctx->host);

    altcp_arg(ctx->pcb, ctx);
    altcp_recv(ctx->pcb, http_recv_cb);
    altcp_err(ctx->pcb, http_err_cb);

    /* Connect */
    err_t conn_err = altcp_connect(ctx->pcb, &ctx->server_ip, ctx->port,
                                    http_connected_cb);
    if (conn_err != ERR_OK) {
        printf("[ota] Connect failed: %d\n", conn_err);
        altcp_close(ctx->pcb);
        altcp_tls_free_config(ctx->tls_cfg);
        return false;
    }

    /* Spin until complete */
    while (!ctx->complete && !ctx->error) {
        if (absolute_time_diff_us(get_absolute_time(), ctx->deadline) <= 0) {
            printf("[ota] HTTP timeout\n");
            if (ctx->pcb) {
                altcp_abort(ctx->pcb);
                ctx->pcb = NULL;
            }
            altcp_tls_free_config(ctx->tls_cfg);
            return false;
        }
        cyw43_arch_poll();
        sleep_ms(1);
    }

    /* Cleanup */
    if (ctx->pcb) {
        altcp_close(ctx->pcb);
        ctx->pcb = NULL;
    }
    altcp_tls_free_config(ctx->tls_cfg);

    return !ctx->error;
}

/**
 * HTTPS GET with redirect following.
 */
static bool https_get_follow(http_ctx_t *ctx, const char *url)
{
    char current_url[512];
    snprintf(current_url, sizeof(current_url), "%s", url);

    for (int i = 0; i < MAX_REDIRECTS; i++) {
        if (!parse_url(current_url, ctx->host, sizeof(ctx->host),
                       &ctx->port, ctx->path, sizeof(ctx->path))) {
            printf("[ota] Bad URL: %s\n", current_url);
            return false;
        }

        printf("[ota] GET https://%s%s\n", ctx->host, ctx->path);
        if (!https_get(ctx)) return false;

        if (ctx->status_code >= 300 && ctx->status_code < 400 &&
            ctx->location[0]) {
            printf("[ota] Redirect %d -> %s\n", ctx->status_code,
                   ctx->location);
            snprintf(current_url, sizeof(current_url), "%s", ctx->location);
            /* Reset body for the next request */
            ctx->body_len = 0;
            continue;
        }
        return true;
    }
    printf("[ota] Too many redirects\n");
    return false;
}

/* ── GitHub release JSON parsing ─────────────────────────────────────── */

/**
 * Extract a JSON string value by key.  Minimal parser — just finds
 * "key":"value" and copies value into buf.
 */
static bool json_find_string(const char *json, const char *key,
                             char *buf, int buf_len)
{
    /* Build search pattern: "key":" */
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);

    const char *p = strstr(json, pattern);
    if (!p) {
        /* Try with space after colon: "key": " */
        snprintf(pattern, sizeof(pattern), "\"%s\": \"", key);
        p = strstr(json, pattern);
        if (!p) return false;
    }
    p += strlen(pattern);

    const char *end = strchr(p, '"');
    if (!end) return false;

    int vlen = (int)(end - p);
    if (vlen >= buf_len) vlen = buf_len - 1;
    memcpy(buf, p, (size_t)vlen);
    buf[vlen] = '\0';
    return true;
}

/**
 * Find the browser_download_url for an asset named "padproxy.bin"
 * in the GitHub releases JSON.
 */
static bool find_bin_asset_url(const char *json, char *url_buf, int url_len)
{
    /* Look for "padproxy.bin" in the assets array, then find its
     * browser_download_url. The JSON structure is:
     *   "assets":[{..."name":"padproxy.bin",...
     *     "browser_download_url":"https://..."}]
     */
    const char *asset = strstr(json, "\"padproxy.bin\"");
    if (!asset) return false;

    /* The browser_download_url should appear nearby (same object) */
    const char *search_start = asset;
    const char *url_key = strstr(search_start, "\"browser_download_url\":\"");
    if (!url_key) {
        url_key = strstr(search_start, "\"browser_download_url\": \"");
        if (!url_key) return false;
    }

    /* Skip to the value */
    const char *val = strchr(url_key + 1, ':');
    if (!val) return false;
    val++;
    while (*val == ' ') val++;
    if (*val != '"') return false;
    val++;

    const char *end = strchr(val, '"');
    if (!end) return false;

    int vlen = (int)(end - val);
    if (vlen >= url_len) return false;
    memcpy(url_buf, val, (size_t)vlen);
    url_buf[vlen] = '\0';
    return true;
}

/* ── Flash operations ────────────────────────────────────────────────── */

typedef struct {
    uint32_t flash_offset;     /* Current write offset in flash */
    uint8_t  sector_buf[FLASH_SECTOR_SZ];
    int      sector_pos;       /* Bytes buffered in sector_buf */
    uint32_t total_written;
    bool     error;
} flash_writer_t;

static void flash_writer_init(flash_writer_t *fw, uint32_t start_offset)
{
    fw->flash_offset = start_offset;
    fw->sector_pos = 0;
    fw->total_written = 0;
    fw->error = false;
}

/**
 * Flush the current sector buffer to flash.
 * Erases the sector, then programs it page by page.
 */
static bool flash_writer_flush(flash_writer_t *fw)
{
    if (fw->sector_pos == 0) return true;

    /* Pad remainder of sector with 0xFF (erased state) */
    if (fw->sector_pos < FLASH_SECTOR_SZ) {
        memset(fw->sector_buf + fw->sector_pos, 0xFF,
               (size_t)(FLASH_SECTOR_SZ - fw->sector_pos));
    }

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(fw->flash_offset, FLASH_SECTOR_SZ);
    flash_range_program(fw->flash_offset, fw->sector_buf, FLASH_SECTOR_SZ);
    restore_interrupts(ints);

    fw->flash_offset += FLASH_SECTOR_SZ;
    fw->sector_pos = 0;
    return true;
}

/**
 * Streaming callback: write incoming firmware data to flash staging area.
 */
static bool flash_write_cb(const uint8_t *data, int len, void *ctx)
{
    flash_writer_t *fw = (flash_writer_t *)ctx;

    if (fw->total_written + (uint32_t)len > OTA_MAX_IMAGE_SIZE) {
        printf("[ota] Firmware too large\n");
        fw->error = true;
        return false;
    }

    int offset = 0;
    while (offset < len) {
        int room = FLASH_SECTOR_SZ - fw->sector_pos;
        int chunk = (len - offset < room) ? (len - offset) : room;
        memcpy(fw->sector_buf + fw->sector_pos, data + offset, (size_t)chunk);
        fw->sector_pos += chunk;
        fw->total_written += (uint32_t)chunk;
        offset += chunk;

        if (fw->sector_pos == FLASH_SECTOR_SZ) {
            if (!flash_writer_flush(fw)) {
                fw->error = true;
                return false;
            }
        }
    }
    return true;
}

/**
 * Copy the staging image to the active application area.
 * Runs with interrupts disabled since we're overwriting running code.
 */
static bool flash_apply_update(uint32_t image_size)
{
    printf("[ota] Applying update (%u bytes)...\n", (unsigned)image_size);

    /* Round up to sector boundary */
    uint32_t sectors = (image_size + FLASH_SECTOR_SZ - 1) / FLASH_SECTOR_SZ;
    uint32_t copy_size = sectors * FLASH_SECTOR_SZ;

    /*
     * Read from staging, erase+program active area, one sector at a time.
     * The flash read uses XIP (memory-mapped), erase/program use ROM routines.
     */
    uint8_t buf[FLASH_SECTOR_SZ];
    const uint8_t *staging = (const uint8_t *)(XIP_BASE + OTA_STAGING_OFFSET);

    for (uint32_t off = 0; off < copy_size; off += FLASH_SECTOR_SZ) {
        /* Read from staging via XIP */
        memcpy(buf, staging + off, FLASH_SECTOR_SZ);

        /* Write to active area */
        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(off, FLASH_SECTOR_SZ);
        flash_range_program(off, buf, FLASH_SECTOR_SZ);
        restore_interrupts(ints);

        if ((off % (64 * 1024)) == 0) {
            printf("[ota] Flashed %u / %u KB\n",
                   (unsigned)(off / 1024), (unsigned)(copy_size / 1024));
        }
    }

    return true;
}

/* ── WiFi helpers ────────────────────────────────────────────────────── */

static bool wifi_connect(void)
{
    printf("[ota] Connecting to WiFi '%s'...\n", WIFI_SSID);
    cyw43_arch_enable_sta_mode();

    int err = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK,
        WIFI_CONNECT_TIMEOUT_MS);

    if (err != 0) {
        printf("[ota] WiFi connect failed: %d\n", err);
        return false;
    }
    printf("[ota] WiFi connected\n");
    return true;
}

static void wifi_disconnect(void)
{
    cyw43_arch_disable_sta_mode();
    printf("[ota] WiFi disconnected\n");
}

/* ── Public API ──────────────────────────────────────────────────────── */

ota_update_result_t ota_update_check_and_apply(void)
{
    /* Skip if WiFi not configured */
    if (strlen(WIFI_SSID) == 0) {
        printf("[ota] No WiFi SSID configured, skipping update check\n");
        return OTA_RESULT_ERROR_NO_WIFI_CONFIG;
    }

    char ver_str[16];
    ota_version_format(&OTA_CURRENT_VERSION, ver_str, sizeof(ver_str));
    printf("[ota] Current firmware version: %s\n", ver_str);

    /* Initialize CYW43 for WiFi */
    if (cyw43_arch_init()) {
        printf("[ota] CYW43 init failed\n");
        return OTA_RESULT_ERROR_WIFI;
    }

    if (!wifi_connect()) {
        cyw43_arch_deinit();
        return OTA_RESULT_ERROR_WIFI;
    }

    /* Allocate buffers */
    static uint8_t api_buf[8192];
    static http_ctx_t http_ctx;
    memset(&http_ctx, 0, sizeof(http_ctx));

    /* Step 1: Query GitHub releases API */
    char api_url[256];
    snprintf(api_url, sizeof(api_url),
             "https://api.github.com/repos/%s/%s/releases/latest",
             GITHUB_OTA_OWNER, GITHUB_OTA_REPO);

    http_ctx.body_buf = api_buf;
    http_ctx.body_cap = (int)sizeof(api_buf);

    ota_update_result_t result = OTA_RESULT_ERROR_HTTP;

    if (!https_get_follow(&http_ctx, api_url)) {
        printf("[ota] Failed to fetch release info\n");
        goto cleanup;
    }

    if (http_ctx.status_code != 200) {
        printf("[ota] GitHub API returned %d\n", http_ctx.status_code);
        goto cleanup;
    }

    /* Step 2: Parse version from tag_name */
    char tag[64];
    if (!json_find_string((char *)api_buf, "tag_name", tag, sizeof(tag))) {
        printf("[ota] No tag_name in release\n");
        result = OTA_RESULT_ERROR_VERSION;
        goto cleanup;
    }

    ota_version_t remote_ver;
    if (!ota_version_parse(tag, &remote_ver)) {
        printf("[ota] Cannot parse version from tag '%s'\n", tag);
        result = OTA_RESULT_ERROR_VERSION;
        goto cleanup;
    }

    char remote_str[16];
    ota_version_format(&remote_ver, remote_str, sizeof(remote_str));
    printf("[ota] Latest release: %s (tag: %s)\n", remote_str, tag);

    if (ota_version_compare(&remote_ver, &OTA_CURRENT_VERSION) <= 0) {
        printf("[ota] Already up to date\n");
        result = OTA_RESULT_NO_UPDATE;
        goto cleanup;
    }

    /* Step 3: Find the .bin asset download URL */
    char bin_url[512];
    if (!find_bin_asset_url((char *)api_buf, bin_url, sizeof(bin_url))) {
        printf("[ota] No padproxy.bin asset in release\n");
        result = OTA_RESULT_ERROR_HTTP;
        goto cleanup;
    }

    printf("[ota] Downloading %s\n", bin_url);

    /* Step 4: Download firmware to staging flash */
    flash_writer_t fw;
    flash_writer_init(&fw, OTA_STAGING_OFFSET);

    memset(&http_ctx, 0, sizeof(http_ctx));
    http_ctx.body_cb = flash_write_cb;
    http_ctx.body_cb_ctx = &fw;

    if (!https_get_follow(&http_ctx, bin_url)) {
        printf("[ota] Download failed\n");
        result = OTA_RESULT_ERROR_HTTP;
        goto cleanup;
    }

    if (http_ctx.status_code != 200) {
        printf("[ota] Download returned %d\n", http_ctx.status_code);
        result = OTA_RESULT_ERROR_HTTP;
        goto cleanup;
    }

    /* Flush remaining data */
    if (!flash_writer_flush(&fw) || fw.error) {
        printf("[ota] Flash write failed\n");
        result = OTA_RESULT_ERROR_FLASH;
        goto cleanup;
    }

    printf("[ota] Downloaded %u bytes to staging\n",
           (unsigned)fw.total_written);

    if (fw.total_written == 0) {
        printf("[ota] Empty firmware image\n");
        result = OTA_RESULT_ERROR_HTTP;
        goto cleanup;
    }

    /* Step 5: Disconnect WiFi before flashing */
    wifi_disconnect();

    /* Step 6: Copy staging -> active */
    if (!flash_apply_update(fw.total_written)) {
        printf("[ota] Flash apply failed\n");
        result = OTA_RESULT_ERROR_FLASH;
        goto cleanup;
    }

    printf("[ota] Update applied! Rebooting...\n");
    sleep_ms(100); /* Let UART drain */
    watchdog_reboot(0, 0, 0);

    /* Should not reach here */
    result = OTA_RESULT_UPDATE_APPLIED;

cleanup:
    wifi_disconnect();
    cyw43_arch_deinit();
    return result;
}

const char *ota_update_result_name(ota_update_result_t result)
{
    switch (result) {
    case OTA_RESULT_NO_UPDATE:            return "NO_UPDATE";
    case OTA_RESULT_UPDATE_APPLIED:       return "UPDATE_APPLIED";
    case OTA_RESULT_ERROR_NO_WIFI_CONFIG: return "ERROR_NO_WIFI_CONFIG";
    case OTA_RESULT_ERROR_WIFI:           return "ERROR_WIFI";
    case OTA_RESULT_ERROR_HTTP:           return "ERROR_HTTP";
    case OTA_RESULT_ERROR_VERSION:        return "ERROR_VERSION";
    case OTA_RESULT_ERROR_FLASH:          return "ERROR_FLASH";
    default:                              return "UNKNOWN";
    }
}
