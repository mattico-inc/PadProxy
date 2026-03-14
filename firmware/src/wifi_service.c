#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/httpd.h"
#include "lwip/dns.h"

#include "wifi_service.h"

/**
 * WiFi service — persistent connection management and HTTP API.
 *
 * Uses the CYW43439 in STA mode with lwIP httpd for the REST API.
 * Reconnects automatically with exponential backoff on disconnect.
 */

/* ── State ──────────────────────────────────────────────────────────────── */

static wifi_state_t       s_state = WIFI_STATE_DISABLED;
static wifi_action_cb_t   s_action_cb;
static wifi_status_cb_t   s_status_cb;
static wifi_service_config_t s_config;
static bool               s_enabled = true;

/** Reconnection backoff state. */
static uint32_t s_reconnect_at_ms;    /* next reconnect attempt time */
static uint32_t s_backoff_ms;         /* current backoff interval */

#define BACKOFF_INITIAL_MS   1000
#define BACKOFF_MAX_MS      30000

/* ── HTTP CGI Handlers ──────────────────────────────────────────────────── */

/*
 * lwIP httpd uses CGI handlers for dynamic content.  We register URL
 * patterns and provide callbacks that return response data.
 *
 * The httpd SSI (Server Side Include) mechanism is used to inject
 * dynamic values into JSON templates.
 */

/** SSI tags for dynamic content. */
static const char *ssi_tags[] = {
    "pcstate",     /* 0 */
    "btconn",      /* 1 */
    "wifirssi",    /* 2 */
    "fwver",       /* 3 */
    "iren",        /* 4 */
};

#define SSI_TAG_COUNT (sizeof(ssi_tags) / sizeof(ssi_tags[0]))

static uint16_t ssi_handler(int tag_index, char *insert, int insert_len
#if LWIP_HTTPD_SSI_INCLUDE_TAG
    , uint16_t current_tag_part, uint16_t *next_tag_part
#endif
)
{
    wifi_device_status_t status = {0};
    if (s_status_cb)
        s_status_cb(&status);

    switch (tag_index) {
    case 0: /* pcstate */
        return (uint16_t)snprintf(insert, (size_t)insert_len, "%s",
                                  pc_power_state_name(status.pc_state));
    case 1: /* btconn */
        return (uint16_t)snprintf(insert, (size_t)insert_len, "%s",
                                  status.bt_connected ? "true" : "false");
    case 2: /* wifirssi */
        return (uint16_t)snprintf(insert, (size_t)insert_len, "%d",
                                  status.wifi_rssi);
    case 3: /* fwver */
        return (uint16_t)snprintf(insert, (size_t)insert_len, "%s",
                                  status.firmware_version ? status.firmware_version : "0.0.0");
    case 4: /* iren */
        return (uint16_t)snprintf(insert, (size_t)insert_len, "%s",
                                  status.ir_enabled ? "true" : "false");
    default:
        return 0;
    }
}

/** CGI handler for /api/power — toggles PC power. */
static const char *cgi_power_handler(int index, int num_params,
                                     char *params[], char *values[])
{
    (void)index; (void)num_params; (void)params; (void)values;
    printf("[wifi] API: power toggle requested\n");
    if (s_action_cb)
        s_action_cb(WIFI_ACTION_POWER_TOGGLE);
    return "/api/ok.json";
}

/** CGI handler for /api/ir/send — sends IR command. */
static const char *cgi_ir_handler(int index, int num_params,
                                  char *params[], char *values[])
{
    (void)index; (void)num_params; (void)params; (void)values;
    printf("[wifi] API: IR send requested\n");
    if (s_action_cb)
        s_action_cb(WIFI_ACTION_IR_SEND);
    return "/api/ok.json";
}

static const tCGI cgi_handlers[] = {
    { "/api/power",   cgi_power_handler },
    { "/api/ir/send", cgi_ir_handler },
};

#define CGI_HANDLER_COUNT (sizeof(cgi_handlers) / sizeof(cgi_handlers[0]))

/* ── Connection management ──────────────────────────────────────────────── */

static bool attempt_connect(void)
{
    printf("[wifi] Connecting to '%s'...\n", s_config.ssid);
    s_state = WIFI_STATE_CONNECTING;

    int err = cyw43_arch_wifi_connect_timeout_ms(
        s_config.ssid, s_config.password, CYW43_AUTH_WPA2_AES_PSK, 10000);

    if (err == 0) {
        printf("[wifi] Connected! IP: %s\n",
               ip4addr_ntoa(netif_ip4_addr(netif_list)));
        s_state = WIFI_STATE_CONNECTED;
        s_backoff_ms = BACKOFF_INITIAL_MS;
        return true;
    }

    printf("[wifi] Connection failed (err %d)\n", err);
    s_state = WIFI_STATE_DISCONNECTED;
    return false;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

bool wifi_service_init(const wifi_service_config_t *config,
                       wifi_action_cb_t action_cb,
                       wifi_status_cb_t status_cb)
{
    if (!config || !config->ssid || config->ssid[0] == '\0') {
        printf("[wifi] No SSID configured, WiFi disabled\n");
        s_state = WIFI_STATE_DISABLED;
        return false;
    }

    s_config    = *config;
    s_action_cb = action_cb;
    s_status_cb = status_cb;
    s_backoff_ms = BACKOFF_INITIAL_MS;

    /* Set hostname for mDNS */
    const char *hostname = config->hostname ? config->hostname : "padproxy";
    cyw43_arch_lwip_begin();
    struct netif *n = netif_list;
    if (n) netif_set_hostname(n, hostname);
    cyw43_arch_lwip_end();

    /* Enable STA mode */
    cyw43_arch_enable_sta_mode();

    /* Attempt initial connection */
    bool connected = attempt_connect();

    /* Start HTTP server regardless (will serve once connected) */
    httpd_init();
    http_set_cgi_handlers(cgi_handlers, CGI_HANDLER_COUNT);
    http_set_ssi_handler(ssi_handler, ssi_tags, SSI_TAG_COUNT);

    printf("[wifi] HTTP server started\n");
    return connected;
}

void wifi_service_deinit(void)
{
    cyw43_arch_disable_sta_mode();
    s_state = WIFI_STATE_DISABLED;
    printf("[wifi] WiFi service stopped\n");
}

wifi_state_t wifi_service_get_state(void)
{
    return s_state;
}

bool wifi_service_is_connected(void)
{
    return s_state == WIFI_STATE_CONNECTED;
}

int8_t wifi_service_get_rssi(void)
{
    if (s_state != WIFI_STATE_CONNECTED) return 0;

    int32_t rssi = 0;
    cyw43_wifi_get_rssi(&cyw43_state, &rssi);
    return (int8_t)rssi;
}

void wifi_service_task(uint32_t now_ms)
{
    if (s_state == WIFI_STATE_DISABLED || !s_enabled)
        return;

    /* Check if we lost connection */
    if (s_state == WIFI_STATE_CONNECTED) {
        int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if (status != CYW43_LINK_UP) {
            printf("[wifi] Connection lost (status %d)\n", status);
            s_state = WIFI_STATE_DISCONNECTED;
            s_reconnect_at_ms = now_ms + s_backoff_ms;
        }
        return;
    }

    /* Reconnect with exponential backoff */
    if (s_state == WIFI_STATE_DISCONNECTED) {
        if (now_ms >= s_reconnect_at_ms) {
            if (attempt_connect()) {
                s_backoff_ms = BACKOFF_INITIAL_MS;
            } else {
                s_backoff_ms *= 2;
                if (s_backoff_ms > BACKOFF_MAX_MS)
                    s_backoff_ms = BACKOFF_MAX_MS;
                s_reconnect_at_ms = now_ms + s_backoff_ms;
                printf("[wifi] Next retry in %lu ms\n",
                       (unsigned long)s_backoff_ms);
            }
        }
    }
}

void wifi_service_disable(void)
{
    if (!s_enabled) return;
    s_enabled = false;
    printf("[wifi] WiFi disabled (BT priority mode)\n");
    cyw43_arch_disable_sta_mode();
    s_state = WIFI_STATE_DISABLED;
}

void wifi_service_enable(void)
{
    if (s_enabled) return;
    s_enabled = true;
    printf("[wifi] WiFi re-enabled\n");
    cyw43_arch_enable_sta_mode();
    s_state = WIFI_STATE_DISCONNECTED;
    s_reconnect_at_ms = 0; /* try immediately */
    s_backoff_ms = BACKOFF_INITIAL_MS;
}

const char *wifi_state_name(wifi_state_t state)
{
    switch (state) {
    case WIFI_STATE_DISABLED:     return "disabled";
    case WIFI_STATE_CONNECTING:   return "connecting";
    case WIFI_STATE_CONNECTED:    return "connected";
    case WIFI_STATE_DISCONNECTED: return "disconnected";
    default:                      return "unknown";
    }
}
