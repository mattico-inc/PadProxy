#include "device_config.h"
#include <string.h>

/* ── Wire format ────────────────────────────────────────────────────── */

#define CONFIG_MAGIC   0x50434647  /* "PCFG" */
#define CONFIG_VERSION 1

/*
 * Binary layout (fixed size):
 *   [0..3]   magic    (uint32, little-endian)
 *   [4..5]   version  (uint16, little-endian)
 *   [6..N]   payload  (device_config_t fields, packed)
 *   [N..N+3] crc32    (uint32, little-endian)
 *
 * Payload fields are written in order with explicit sizes so the format
 * is stable across compiler struct-layout differences.
 */

#define HEADER_SIZE  6  /* magic (4) + version (2) */
#define CRC_SIZE     4
#define PAYLOAD_SIZE (                               \
    (DEVICE_CONFIG_WIFI_SSID_MAX + 1) +              \
    (DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1) +          \
    2 + /* power_pulse_ms */                         \
    2 + /* boot_timeout_ms */                        \
    (DEVICE_CONFIG_DEVICE_NAME_MAX + 1)              \
)
#define TOTAL_SIZE (HEADER_SIZE + PAYLOAD_SIZE + CRC_SIZE)

/* Static assert that our advertised serial size is large enough */
_Static_assert(DEVICE_CONFIG_SERIAL_SIZE >= TOTAL_SIZE,
               "DEVICE_CONFIG_SERIAL_SIZE too small");

/* ── CRC-32 (ISO 3309 / zlib) ──────────────────────────────────────── */

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

/* ── Little-endian helpers ──────────────────────────────────────────── */

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* ── Public API ─────────────────────────────────────────────────────── */

void device_config_init(device_config_t *cfg)
{
    if (!cfg) return;

    memset(cfg, 0, sizeof(*cfg));
    cfg->power_pulse_ms  = DEVICE_CONFIG_DEFAULT_POWER_PULSE_MS;
    cfg->boot_timeout_ms = DEVICE_CONFIG_DEFAULT_BOOT_TIMEOUT_MS;
    strncpy(cfg->device_name, DEVICE_CONFIG_DEFAULT_DEVICE_NAME,
            DEVICE_CONFIG_DEVICE_NAME_MAX);
    cfg->device_name[DEVICE_CONFIG_DEVICE_NAME_MAX] = '\0';
}

bool device_config_validate(const device_config_t *cfg)
{
    if (!cfg) return false;

    if (cfg->power_pulse_ms < DEVICE_CONFIG_POWER_PULSE_MIN ||
        cfg->power_pulse_ms > DEVICE_CONFIG_POWER_PULSE_MAX)
        return false;

    if (cfg->boot_timeout_ms < DEVICE_CONFIG_BOOT_TIMEOUT_MIN ||
        cfg->boot_timeout_ms > DEVICE_CONFIG_BOOT_TIMEOUT_MAX)
        return false;

    if (cfg->device_name[0] == '\0')
        return false;

    /* Ensure strings are null-terminated within bounds */
    if (cfg->wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX] != '\0')
        return false;
    if (cfg->wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX] != '\0')
        return false;
    if (cfg->device_name[DEVICE_CONFIG_DEVICE_NAME_MAX] != '\0')
        return false;

    return true;
}

int device_config_serialize(const device_config_t *cfg,
                            uint8_t *buf, size_t len)
{
    if (!cfg || !buf || len < TOTAL_SIZE)
        return -1;

    memset(buf, 0, TOTAL_SIZE);

    uint8_t *p = buf;

    /* Header */
    put_u32(p, CONFIG_MAGIC);   p += 4;
    put_u16(p, CONFIG_VERSION); p += 2;

    /* Payload — fixed-size fields in stable order */
    memcpy(p, cfg->wifi_ssid, DEVICE_CONFIG_WIFI_SSID_MAX + 1);
    p += DEVICE_CONFIG_WIFI_SSID_MAX + 1;

    memcpy(p, cfg->wifi_password, DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1);
    p += DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1;

    put_u16(p, cfg->power_pulse_ms);  p += 2;
    put_u16(p, cfg->boot_timeout_ms); p += 2;

    memcpy(p, cfg->device_name, DEVICE_CONFIG_DEVICE_NAME_MAX + 1);
    p += DEVICE_CONFIG_DEVICE_NAME_MAX + 1;

    /* CRC over header + payload */
    uint32_t crc = crc32_update(0, buf, HEADER_SIZE + PAYLOAD_SIZE);
    put_u32(p, crc);

    return TOTAL_SIZE;
}

bool device_config_deserialize(device_config_t *cfg,
                               const uint8_t *buf, size_t len)
{
    if (!cfg || !buf || len < TOTAL_SIZE)
        return false;

    const uint8_t *p = buf;

    /* Check magic */
    if (get_u32(p) != CONFIG_MAGIC)
        return false;
    p += 4;

    /* Check version */
    uint16_t ver = get_u16(p);
    if (ver != CONFIG_VERSION)
        return false;
    p += 2;

    /* Verify CRC */
    uint32_t expected_crc = get_u32(buf + HEADER_SIZE + PAYLOAD_SIZE);
    uint32_t actual_crc = crc32_update(0, buf, HEADER_SIZE + PAYLOAD_SIZE);
    if (actual_crc != expected_crc)
        return false;

    /* Deserialize payload into a temporary so we can validate before
     * committing to the output struct */
    device_config_t tmp;
    memset(&tmp, 0, sizeof(tmp));

    memcpy(tmp.wifi_ssid, p, DEVICE_CONFIG_WIFI_SSID_MAX + 1);
    p += DEVICE_CONFIG_WIFI_SSID_MAX + 1;

    memcpy(tmp.wifi_password, p, DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1);
    p += DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1;

    tmp.power_pulse_ms  = get_u16(p); p += 2;
    tmp.boot_timeout_ms = get_u16(p); p += 2;

    memcpy(tmp.device_name, p, DEVICE_CONFIG_DEVICE_NAME_MAX + 1);

    if (!device_config_validate(&tmp))
        return false;

    *cfg = tmp;
    return true;
}
