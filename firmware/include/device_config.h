#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Device Configuration
 *
 * Persistent settings stored in flash.  The struct is serialized to a
 * fixed-size binary blob with a magic number and CRC-32 for integrity.
 * Serialization/deserialization is pure logic — no hardware access —
 * so it can be unit-tested on the host.
 *
 * On boot the firmware attempts to deserialize from the config flash
 * sector; on failure (empty flash, corrupt data) it falls back to
 * compiled-in defaults.
 */

#define DEVICE_CONFIG_WIFI_SSID_MAX     32
#define DEVICE_CONFIG_WIFI_PASSWORD_MAX 63
#define DEVICE_CONFIG_DEVICE_NAME_MAX   32
#define DEVICE_CONFIG_MQTT_BROKER_MAX   63
#define DEVICE_CONFIG_MQTT_USERNAME_MAX 32
#define DEVICE_CONFIG_MQTT_PASSWORD_MAX 63

#define DEVICE_CONFIG_DEFAULT_POWER_PULSE_MS   200
#define DEVICE_CONFIG_DEFAULT_BOOT_TIMEOUT_MS  30000
#define DEVICE_CONFIG_DEFAULT_DEVICE_NAME      "PadProxy"
#define DEVICE_CONFIG_DEFAULT_IR_PROTOCOL      0   /* NEC */
#define DEVICE_CONFIG_DEFAULT_IR_ADDRESS        0
#define DEVICE_CONFIG_DEFAULT_IR_COMMAND        2   /* Common TV power */
#define DEVICE_CONFIG_DEFAULT_MQTT_PORT        1883

#define DEVICE_CONFIG_POWER_PULSE_MIN   50
#define DEVICE_CONFIG_POWER_PULSE_MAX   2000
#define DEVICE_CONFIG_BOOT_TIMEOUT_MIN  5000
#define DEVICE_CONFIG_BOOT_TIMEOUT_MAX  60000

typedef struct {
    /* WiFi */
    char     wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX + 1];
    char     wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1];

    /* Power management */
    uint16_t power_pulse_ms;
    uint16_t boot_timeout_ms;

    /* Device identity */
    char     device_name[DEVICE_CONFIG_DEVICE_NAME_MAX + 1];

    /* IR Blaster */
    uint8_t  ir_protocol;          /**< IR_PROTO_NEC, SAMSUNG, SONY, RC5 */
    uint16_t ir_address;           /**< IR device address */
    uint16_t ir_command;           /**< IR power command code */
    uint8_t  ir_auto_tv;           /**< Auto-send IR on PC state change */

    /* Smart Home (MQTT) */
    char     mqtt_broker[DEVICE_CONFIG_MQTT_BROKER_MAX + 1];
    uint16_t mqtt_port;
    char     mqtt_username[DEVICE_CONFIG_MQTT_USERNAME_MAX + 1];
    char     mqtt_password[DEVICE_CONFIG_MQTT_PASSWORD_MAX + 1];

    /* WiFi/BT coexistence */
    uint8_t  wifi_bt_priority;     /**< Disable WiFi during BT gameplay */
} device_config_t;

/**
 * Initialize a config struct with compiled-in defaults.
 */
void device_config_init(device_config_t *cfg);

/**
 * Validate config values are within allowed ranges.
 * Returns true if valid, false otherwise.
 */
bool device_config_validate(const device_config_t *cfg);

/**
 * Required buffer size for serialization.
 */
#define DEVICE_CONFIG_SERIAL_SIZE 512

/**
 * Serialize config to a binary buffer suitable for flash storage.
 *
 * Format: [magic: 4B][version: 2B][payload][crc32: 4B]
 *
 * @param cfg  Config to serialize.
 * @param buf  Output buffer (must be >= DEVICE_CONFIG_SERIAL_SIZE bytes).
 * @param len  Size of buf.
 * @return     Number of bytes written, or -1 on error.
 */
int device_config_serialize(const device_config_t *cfg,
                            uint8_t *buf, size_t len);

/**
 * Deserialize config from a binary buffer.
 *
 * Validates magic, version, and CRC.  On failure the output struct is
 * left unchanged and false is returned (caller should fall back to
 * defaults).
 *
 * @param cfg  Output config struct.
 * @param buf  Input buffer.
 * @param len  Size of buf.
 * @return     true on success, false if data is invalid/corrupt.
 */
bool device_config_deserialize(device_config_t *cfg,
                               const uint8_t *buf, size_t len);

#endif /* DEVICE_CONFIG_H */
