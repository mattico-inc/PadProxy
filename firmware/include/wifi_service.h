#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "pc_power_state.h"

/**
 * Persistent WiFi service — connection management and HTTP API.
 *
 * Maintains a WiFi STA connection to the configured AP, provides an HTTP
 * API for querying device status and triggering actions (power toggle,
 * IR commands), and supports optional mDNS advertisement.
 *
 * The HTTP API is always available.  Smart home integration (MQTT, HomeKit,
 * etc.) is layered on top via the smarthome abstraction.
 */

/** WiFi connection state. */
typedef enum {
    WIFI_STATE_DISABLED,       /**< No SSID configured, WiFi off */
    WIFI_STATE_CONNECTING,     /**< Attempting to join AP */
    WIFI_STATE_CONNECTED,      /**< Associated and got DHCP */
    WIFI_STATE_DISCONNECTED,   /**< Lost connection, will retry */
} wifi_state_t;

/** WiFi configuration (subset of device_config_t). */
typedef struct {
    const char *ssid;
    const char *password;
    const char *hostname;      /**< mDNS hostname (default: "padproxy") */
} wifi_service_config_t;

/**
 * Callback for actions triggered by the HTTP API.
 * The main loop registers this to receive power toggle requests, etc.
 */
typedef enum {
    WIFI_ACTION_POWER_TOGGLE,     /**< Toggle PC power */
    WIFI_ACTION_IR_SEND,          /**< Send IR command (uses default config) */
} wifi_action_t;

typedef void (*wifi_action_cb_t)(wifi_action_t action);

/**
 * Callback to query current device state for the status API.
 */
typedef struct {
    pc_power_state_t pc_state;
    bool             bt_connected;
    int8_t           wifi_rssi;
    const char      *firmware_version;
    bool             ir_enabled;
} wifi_device_status_t;

typedef void (*wifi_status_cb_t)(wifi_device_status_t *status);

/**
 * Initialize the WiFi service.
 *
 * Connects to the configured WiFi AP and starts the HTTP server.
 * Must be called after cyw43_arch_init() and before bt_gamepad_init().
 *
 * @param config     WiFi credentials and hostname.
 * @param action_cb  Callback for API-triggered actions (power, IR).
 * @param status_cb  Callback to query current device state.
 * @return true if WiFi connection succeeded.
 */
bool wifi_service_init(const wifi_service_config_t *config,
                       wifi_action_cb_t action_cb,
                       wifi_status_cb_t status_cb);

/**
 * Shut down the WiFi service.
 * Stops the HTTP server and disconnects from WiFi.
 */
void wifi_service_deinit(void);

/**
 * Get the current WiFi connection state.
 */
wifi_state_t wifi_service_get_state(void);

/**
 * Check if WiFi is connected and has an IP address.
 */
bool wifi_service_is_connected(void);

/**
 * Get the WiFi signal strength (RSSI in dBm).
 * Returns 0 if not connected.
 */
int8_t wifi_service_get_rssi(void);

/**
 * Poll the WiFi service from the main loop.
 *
 * Handles reconnection on disconnect (exponential backoff) and services
 * any pending HTTP requests.  Should be called every main loop iteration.
 *
 * @param now_ms  Current system time in milliseconds.
 */
void wifi_service_task(uint32_t now_ms);

/**
 * Temporarily disable WiFi (e.g., for BT priority mode).
 * WiFi will be re-enabled when wifi_service_enable() is called.
 */
void wifi_service_disable(void);

/**
 * Re-enable WiFi after a disable.
 */
void wifi_service_enable(void);

/**
 * Get the name of a WiFi state.
 */
const char *wifi_state_name(wifi_state_t state);

#endif /* WIFI_SERVICE_H */
