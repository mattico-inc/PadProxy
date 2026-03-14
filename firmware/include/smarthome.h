#ifndef SMARTHOME_H
#define SMARTHOME_H

#include <stdbool.h>
#include "pc_power_state.h"
#include "device_config.h"

/**
 * Smart home platform abstraction.
 *
 * Each smart home platform (Home Assistant, HomeKit, Alexa, etc.) implements
 * this interface.  The active platform is selected at compile time via the
 * SMARTHOME cmake variable.
 *
 * The HTTP API (wifi_service) is always available regardless of which smart
 * home platform is active.
 */

/** Callback for commands received from the smart home platform. */
typedef enum {
    SMARTHOME_CMD_POWER_TOGGLE,    /**< Toggle PC power */
    SMARTHOME_CMD_POWER_ON,        /**< Turn PC on */
    SMARTHOME_CMD_POWER_OFF,       /**< Turn PC off / shutdown */
    SMARTHOME_CMD_IR_SEND,         /**< Send configured IR command */
} smarthome_cmd_t;

typedef void (*smarthome_cmd_cb_t)(smarthome_cmd_t cmd);

/**
 * Smart home platform interface.
 *
 * Each platform implements these function pointers.  The main loop calls
 * init(), task(), and the publish functions.  The platform calls the
 * command callback when it receives commands from the smart home system.
 */
typedef struct {
    /**
     * Initialize the platform.
     * @param config   Device config (contains MQTT broker, etc.).
     * @param cmd_cb   Callback for received commands.
     * @return true if initialization succeeded.
     */
    bool (*init)(const device_config_t *config, smarthome_cmd_cb_t cmd_cb);

    /**
     * Poll the platform (called from main loop).
     * Handles keepalives, incoming messages, reconnection, etc.
     */
    void (*task)(uint32_t now_ms);

    /**
     * Shut down the platform.
     */
    void (*deinit)(void);

    /**
     * Publish the current PC power state.
     * Called whenever the state machine transitions.
     */
    void (*publish_pc_state)(pc_power_state_t state);

    /**
     * Publish Bluetooth gamepad connection status.
     */
    void (*publish_bt_status)(bool connected);
} smarthome_platform_t;

/**
 * Get the active smart home platform.
 *
 * Returns NULL if no smart home platform is compiled in (HTTP-only mode).
 * The returned pointer is a static const — do not free.
 */
const smarthome_platform_t *smarthome_get_platform(void);

#endif /* SMARTHOME_H */
