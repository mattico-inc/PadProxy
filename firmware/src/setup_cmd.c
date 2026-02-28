#include "setup_cmd.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ── Module state ───────────────────────────────────────────────────── */

static const char *s_version = "0.0.0";
static const char *s_status  = "";

void setup_cmd_set_version(const char *version_str)
{
    s_version = version_str ? version_str : "0.0.0";
}

void setup_cmd_set_status(const char *status_str)
{
    s_status = status_str ? status_str : "";
}

/* ── Helpers ────────────────────────────────────────────────────────── */

/** Write formatted response, respecting buffer limits. */
static int out_printf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return (n < 0) ? 0 : (n >= (int)size) ? (int)size - 1 : n;
}

/** Skip leading whitespace, return pointer into same buffer. */
static const char *skip_ws(const char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/** Parse a uint16 from string. Returns false on invalid input. */
static bool parse_u16(const char *s, uint16_t *out)
{
    char *end;
    unsigned long v = strtoul(s, &end, 10);
    if (end == s || *end != '\0' || v > 65535)
        return false;
    *out = (uint16_t)v;
    return true;
}

/* ── Config key table ───────────────────────────────────────────────── */

typedef enum {
    KEY_WIFI_SSID,
    KEY_WIFI_PASSWORD,
    KEY_POWER_PULSE_MS,
    KEY_BOOT_TIMEOUT_MS,
    KEY_DEVICE_NAME,
    KEY_COUNT,
    KEY_UNKNOWN = -1,
} config_key_t;

static const char *key_names[KEY_COUNT] = {
    [KEY_WIFI_SSID]       = "wifi_ssid",
    [KEY_WIFI_PASSWORD]   = "wifi_password",
    [KEY_POWER_PULSE_MS]  = "power_pulse_ms",
    [KEY_BOOT_TIMEOUT_MS] = "boot_timeout_ms",
    [KEY_DEVICE_NAME]     = "device_name",
};

static config_key_t find_key(const char *name)
{
    for (int i = 0; i < KEY_COUNT; i++) {
        if (strcmp(name, key_names[i]) == 0)
            return (config_key_t)i;
    }
    return KEY_UNKNOWN;
}

/* ── Get/Set handlers ───────────────────────────────────────────────── */

static int cmd_get(config_key_t key, const device_config_t *cfg,
                   char *out, size_t size)
{
    switch (key) {
    case KEY_WIFI_SSID:
        return out_printf(out, size, "OK %s\n", cfg->wifi_ssid);
    case KEY_WIFI_PASSWORD:
        return out_printf(out, size, "OK ********\n");
    case KEY_POWER_PULSE_MS:
        return out_printf(out, size, "OK %u\n", cfg->power_pulse_ms);
    case KEY_BOOT_TIMEOUT_MS:
        return out_printf(out, size, "OK %u\n", cfg->boot_timeout_ms);
    case KEY_DEVICE_NAME:
        return out_printf(out, size, "OK %s\n", cfg->device_name);
    default:
        return out_printf(out, size, "ERR unknown key\n");
    }
}

static int cmd_set(config_key_t key, const char *value,
                   device_config_t *cfg, char *out, size_t size)
{
    uint16_t u16val;

    switch (key) {
    case KEY_WIFI_SSID:
        if (strlen(value) > DEVICE_CONFIG_WIFI_SSID_MAX)
            return out_printf(out, size, "ERR value too long (max %d)\n",
                              DEVICE_CONFIG_WIFI_SSID_MAX);
        strncpy(cfg->wifi_ssid, value, DEVICE_CONFIG_WIFI_SSID_MAX);
        cfg->wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX] = '\0';
        return out_printf(out, size, "OK\n");

    case KEY_WIFI_PASSWORD:
        if (strlen(value) > DEVICE_CONFIG_WIFI_PASSWORD_MAX)
            return out_printf(out, size, "ERR value too long (max %d)\n",
                              DEVICE_CONFIG_WIFI_PASSWORD_MAX);
        strncpy(cfg->wifi_password, value, DEVICE_CONFIG_WIFI_PASSWORD_MAX);
        cfg->wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX] = '\0';
        return out_printf(out, size, "OK\n");

    case KEY_POWER_PULSE_MS:
        if (!parse_u16(value, &u16val))
            return out_printf(out, size, "ERR invalid number\n");
        if (u16val < DEVICE_CONFIG_POWER_PULSE_MIN ||
            u16val > DEVICE_CONFIG_POWER_PULSE_MAX)
            return out_printf(out, size, "ERR out of range (%d-%d)\n",
                              DEVICE_CONFIG_POWER_PULSE_MIN,
                              DEVICE_CONFIG_POWER_PULSE_MAX);
        cfg->power_pulse_ms = u16val;
        return out_printf(out, size, "OK\n");

    case KEY_BOOT_TIMEOUT_MS:
        if (!parse_u16(value, &u16val))
            return out_printf(out, size, "ERR invalid number\n");
        if (u16val < DEVICE_CONFIG_BOOT_TIMEOUT_MIN ||
            u16val > DEVICE_CONFIG_BOOT_TIMEOUT_MAX)
            return out_printf(out, size, "ERR out of range (%d-%d)\n",
                              DEVICE_CONFIG_BOOT_TIMEOUT_MIN,
                              DEVICE_CONFIG_BOOT_TIMEOUT_MAX);
        cfg->boot_timeout_ms = u16val;
        return out_printf(out, size, "OK\n");

    case KEY_DEVICE_NAME:
        if (strlen(value) == 0)
            return out_printf(out, size, "ERR device name cannot be empty\n");
        if (strlen(value) > DEVICE_CONFIG_DEVICE_NAME_MAX)
            return out_printf(out, size, "ERR value too long (max %d)\n",
                              DEVICE_CONFIG_DEVICE_NAME_MAX);
        strncpy(cfg->device_name, value, DEVICE_CONFIG_DEVICE_NAME_MAX);
        cfg->device_name[DEVICE_CONFIG_DEVICE_NAME_MAX] = '\0';
        return out_printf(out, size, "OK\n");

    default:
        return out_printf(out, size, "ERR unknown key\n");
    }
}

/* ── List command ───────────────────────────────────────────────────── */

static int cmd_list(const device_config_t *cfg, char *out, size_t size)
{
    int total = 0;
    int n;

    n = out_printf(out + total, size - total,
                   "OK wifi_ssid=%s\n", cfg->wifi_ssid);
    total += n;

    n = out_printf(out + total, size - total,
                   "OK wifi_password=********\n");
    total += n;

    n = out_printf(out + total, size - total,
                   "OK power_pulse_ms=%u\n", cfg->power_pulse_ms);
    total += n;

    n = out_printf(out + total, size - total,
                   "OK boot_timeout_ms=%u\n", cfg->boot_timeout_ms);
    total += n;

    n = out_printf(out + total, size - total,
                   "OK device_name=%s\n", cfg->device_name);
    total += n;

    return total;
}

/* ── Main dispatch ──────────────────────────────────────────────────── */

setup_cmd_result_t setup_cmd_process(const char *line,
                                     device_config_t *cfg,
                                     char *out_buf, size_t out_size)
{
    setup_cmd_result_t result = { .action = SETUP_ACTION_NONE, .out_len = 0 };

    if (!line || !cfg || !out_buf || out_size == 0)
        return result;

    /* Strip leading whitespace */
    const char *p = skip_ws(line);

    /* Copy to mutable buffer, strip trailing whitespace/newline */
    char buf[256];
    size_t len = strlen(p);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    while (len > 0 && isspace((unsigned char)buf[len - 1])) {
        buf[--len] = '\0';
    }

    /* Empty line — no output */
    if (len == 0)
        return result;

    /* Extract command word */
    char *space = strchr(buf, ' ');
    char *arg = NULL;
    if (space) {
        *space = '\0';
        arg = space + 1;
        /* Skip whitespace after command */
        while (*arg && isspace((unsigned char)*arg)) arg++;
    }

    const char *cmd = buf;

    if (strcmp(cmd, "get") == 0) {
        if (!arg || *arg == '\0') {
            result.out_len = out_printf(out_buf, out_size,
                                        "ERR usage: get <key>\n");
            return result;
        }
        config_key_t key = find_key(arg);
        if (key == KEY_UNKNOWN) {
            result.out_len = out_printf(out_buf, out_size,
                                        "ERR unknown key: %s\n", arg);
            return result;
        }
        result.out_len = cmd_get(key, cfg, out_buf, out_size);

    } else if (strcmp(cmd, "set") == 0) {
        if (!arg || *arg == '\0') {
            result.out_len = out_printf(out_buf, out_size,
                                        "ERR usage: set <key> <value>\n");
            return result;
        }
        /* Split arg into key and value */
        char *val_sep = strchr(arg, ' ');
        if (!val_sep) {
            result.out_len = out_printf(out_buf, out_size,
                                        "ERR usage: set <key> <value>\n");
            return result;
        }
        *val_sep = '\0';
        char *value = val_sep + 1;
        while (*value && isspace((unsigned char)*value)) value++;

        config_key_t key = find_key(arg);
        if (key == KEY_UNKNOWN) {
            result.out_len = out_printf(out_buf, out_size,
                                        "ERR unknown key: %s\n", arg);
            return result;
        }
        result.out_len = cmd_set(key, value, cfg, out_buf, out_size);

    } else if (strcmp(cmd, "list") == 0) {
        result.out_len = cmd_list(cfg, out_buf, out_size);

    } else if (strcmp(cmd, "save") == 0) {
        result.out_len = out_printf(out_buf, out_size, "OK\n");
        result.action = SETUP_ACTION_SAVE;

    } else if (strcmp(cmd, "defaults") == 0) {
        device_config_init(cfg);
        result.out_len = out_printf(out_buf, out_size, "OK\n");

    } else if (strcmp(cmd, "version") == 0) {
        result.out_len = out_printf(out_buf, out_size, "OK %s\n", s_version);

    } else if (strcmp(cmd, "status") == 0) {
        result.out_len = out_printf(out_buf, out_size, "OK %s\n", s_status);

    } else if (strcmp(cmd, "reboot") == 0) {
        result.out_len = out_printf(out_buf, out_size, "OK\n");
        result.action = SETUP_ACTION_REBOOT;

    } else {
        result.out_len = out_printf(out_buf, out_size,
                                    "ERR unknown command: %s\n", cmd);
    }

    return result;
}
