#ifndef PC_POWER_STATE_H
#define PC_POWER_STATE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * PC Power State Machine
 *
 * Tracks the PC's power state based on hardware signals (power LED, USB
 * enumeration, button presses) and drives actions (power button trigger,
 * status LED updates).
 *
 * States:
 *   PC_OFF      - PC is powered off (S5). ESP32 is on standby power.
 *   PC_BOOTING  - Power button was triggered, waiting for USB enumeration.
 *   PC_ON       - PC is running. USB enumerated, power LED active.
 *   PC_SLEEPING - PC entered sleep/hibernate (S3/S4). LED off, USB suspended.
 *
 * The state machine is designed as a pure-logic module with no direct hardware
 * access. All I/O is handled through events (input) and actions (output).
 */

typedef enum {
    PC_STATE_OFF,
    PC_STATE_BOOTING,
    PC_STATE_ON,
    PC_STATE_SLEEPING,
    PC_STATE_COUNT
} pc_power_state_t;

typedef enum {
    /** Controller HOME/Guide/PS button pressed while PC is off or sleeping */
    PC_EVENT_WAKE_REQUESTED,
    /** Physical front-panel power button pressed (short press) */
    PC_EVENT_BUTTON_PRESSED,
    /** Physical power button long press (>=4s, force shutdown) */
    PC_EVENT_BUTTON_LONG_PRESSED,
    /** USB host enumeration detected (OS is running) */
    PC_EVENT_USB_ENUMERATED,
    /** USB host connection lost / suspended */
    PC_EVENT_USB_SUSPENDED,
    /** Power LED turned on */
    PC_EVENT_POWER_LED_ON,
    /** Power LED turned off */
    PC_EVENT_POWER_LED_OFF,
    /** Boot timeout expired (no USB enumeration within deadline) */
    PC_EVENT_BOOT_TIMEOUT,
    PC_EVENT_COUNT
} pc_power_event_t;

/**
 * Actions the state machine requests the hardware layer to perform.
 * Multiple actions can be ORed together in a single transition.
 */
typedef enum {
    PC_ACTION_NONE            = 0,
    /** Pulse the power button optocoupler (100-500ms) */
    PC_ACTION_TRIGGER_POWER   = (1 << 0),
    /** Start the boot timeout timer */
    PC_ACTION_START_BOOT_TIMER = (1 << 1),
    /** Cancel the boot timeout timer */
    PC_ACTION_CANCEL_BOOT_TIMER = (1 << 2),
} pc_power_action_t;

typedef struct {
    pc_power_state_t state;
    /** Timestamp (ms) of the last state transition, set by caller */
    uint32_t last_transition_ms;
} pc_power_sm_t;

typedef struct {
    pc_power_state_t new_state;
    /** Bitmask of pc_power_action_t */
    uint32_t actions;
    /** True if a state transition occurred */
    bool transitioned;
} pc_power_result_t;

/**
 * Initialize the state machine to PC_OFF.
 */
void pc_power_sm_init(pc_power_sm_t *sm);

/**
 * Process an event and return the resulting state + actions.
 *
 * @param sm      Pointer to the state machine context.
 * @param event   The event to process.
 * @param now_ms  Current timestamp in milliseconds (for transition tracking).
 * @return        Result containing new state, actions, and whether a
 *                transition occurred.
 */
pc_power_result_t pc_power_sm_process(pc_power_sm_t *sm,
                                       pc_power_event_t event,
                                       uint32_t now_ms);

/**
 * Get the current state.
 */
pc_power_state_t pc_power_sm_get_state(const pc_power_sm_t *sm);

/**
 * Get a human-readable name for a state.
 */
const char *pc_power_state_name(pc_power_state_t state);

/**
 * Get a human-readable name for an event.
 */
const char *pc_power_event_name(pc_power_event_t event);

#endif /* PC_POWER_STATE_H */
