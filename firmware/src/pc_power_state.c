#include "pc_power_state.h"

void pc_power_sm_init(pc_power_sm_t *sm)
{
    sm->state = PC_STATE_OFF;
    sm->last_transition_ms = 0;
}

pc_power_state_t pc_power_sm_get_state(const pc_power_sm_t *sm)
{
    return sm->state;
}

/**
 * Helper: build a result that transitions to a new state.
 */
static pc_power_result_t transition(pc_power_sm_t *sm,
                                     pc_power_state_t new_state,
                                     uint32_t actions,
                                     uint32_t now_ms)
{
    sm->state = new_state;
    sm->last_transition_ms = now_ms;
    return (pc_power_result_t){
        .new_state = new_state,
        .actions = actions,
        .transitioned = true,
    };
}

/**
 * Helper: build a result with no state change.
 */
static pc_power_result_t no_change(const pc_power_sm_t *sm)
{
    return (pc_power_result_t){
        .new_state = sm->state,
        .actions = PC_ACTION_NONE,
        .transitioned = false,
    };
}

static pc_power_result_t handle_off(pc_power_sm_t *sm,
                                     pc_power_event_t event,
                                     uint32_t now_ms)
{
    switch (event) {
    case PC_EVENT_WAKE_REQUESTED:
        /*
         * Controller HOME button pressed while PC is off.
         * Trigger the power button and start waiting for USB enumeration.
         */
        return transition(sm, PC_STATE_BOOTING,
                          PC_ACTION_TRIGGER_POWER | PC_ACTION_START_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_BUTTON_PRESSED:
        /*
         * Physical power button pressed. The button signal passes through
         * to the motherboard via the hardware passthrough. We just track
         * the state change.
         */
        return transition(sm, PC_STATE_BOOTING,
                          PC_ACTION_START_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_POWER_LED_ON:
        /*
         * Power LED came on without us seeing a button press (e.g., WoL,
         * BIOS auto-power-on, or we missed the button event). Transition
         * to BOOTING and wait for USB.
         */
        return transition(sm, PC_STATE_BOOTING,
                          PC_ACTION_START_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_USB_ENUMERATED:
        /*
         * USB enumerated while we thought PC was off. Likely we missed
         * the boot sequence. Jump straight to ON.
         */
        return transition(sm, PC_STATE_ON,
                          PC_ACTION_NONE,
                          now_ms);

    default:
        return no_change(sm);
    }
}

static pc_power_result_t handle_booting(pc_power_sm_t *sm,
                                         pc_power_event_t event,
                                         uint32_t now_ms)
{
    switch (event) {
    case PC_EVENT_USB_ENUMERATED:
        /*
         * USB host detected - OS is running. Boot successful.
         */
        return transition(sm, PC_STATE_ON,
                          PC_ACTION_CANCEL_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_POWER_LED_OFF:
        /*
         * LED turned off during boot. The PC shut down before finishing
         * boot (e.g., user tapped power button again, or PSU issue).
         */
        return transition(sm, PC_STATE_OFF,
                          PC_ACTION_CANCEL_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_BOOT_TIMEOUT:
        /*
         * Timed out waiting for USB enumeration. The PC may have booted
         * into BIOS or failed. Return to OFF so the user can try again.
         */
        return transition(sm, PC_STATE_OFF,
                          PC_ACTION_NONE,
                          now_ms);

    case PC_EVENT_BUTTON_LONG_PRESSED:
        /*
         * User force-shutdown during boot. PC will power off.
         */
        return transition(sm, PC_STATE_OFF,
                          PC_ACTION_CANCEL_BOOT_TIMER,
                          now_ms);

    default:
        return no_change(sm);
    }
}

static pc_power_result_t handle_on(pc_power_sm_t *sm,
                                    pc_power_event_t event,
                                    uint32_t now_ms)
{
    switch (event) {
    case PC_EVENT_USB_SUSPENDED:
        /*
         * USB suspended. PC is likely entering sleep.
         */
        return transition(sm, PC_STATE_SLEEPING,
                          PC_ACTION_NONE,
                          now_ms);

    case PC_EVENT_POWER_LED_OFF:
        /*
         * Power LED went off while PC was on. This means shutdown.
         */
        return transition(sm, PC_STATE_OFF,
                          PC_ACTION_NONE,
                          now_ms);

    case PC_EVENT_BUTTON_LONG_PRESSED:
        /*
         * Force shutdown via long press.
         */
        return transition(sm, PC_STATE_OFF,
                          PC_ACTION_NONE,
                          now_ms);

    default:
        return no_change(sm);
    }
}

static pc_power_result_t handle_sleeping(pc_power_sm_t *sm,
                                          pc_power_event_t event,
                                          uint32_t now_ms)
{
    switch (event) {
    case PC_EVENT_WAKE_REQUESTED:
        /*
         * Controller button pressed while PC is sleeping.
         * Trigger power button to wake from sleep.
         */
        return transition(sm, PC_STATE_BOOTING,
                          PC_ACTION_TRIGGER_POWER | PC_ACTION_START_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_BUTTON_PRESSED:
        /*
         * Physical button pressed while sleeping. Hardware passes it
         * through; we track the state change.
         */
        return transition(sm, PC_STATE_BOOTING,
                          PC_ACTION_START_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_USB_ENUMERATED:
        /*
         * USB re-enumerated. PC woke up (e.g., keyboard/mouse wake,
         * scheduled wake, or WoL).
         */
        return transition(sm, PC_STATE_ON,
                          PC_ACTION_NONE,
                          now_ms);

    case PC_EVENT_POWER_LED_ON:
        /*
         * LED came back on. PC is waking up. Go to BOOTING until
         * USB confirms OS is running.
         */
        return transition(sm, PC_STATE_BOOTING,
                          PC_ACTION_START_BOOT_TIMER,
                          now_ms);

    case PC_EVENT_POWER_LED_OFF:
        /*
         * LED off while sleeping - PC transitioned from sleep to full
         * shutdown (e.g., hibernate timeout, or power loss).
         */
        return transition(sm, PC_STATE_OFF,
                          PC_ACTION_NONE,
                          now_ms);

    case PC_EVENT_BUTTON_LONG_PRESSED:
        /*
         * Force shutdown from sleep.
         */
        return transition(sm, PC_STATE_OFF,
                          PC_ACTION_NONE,
                          now_ms);

    default:
        return no_change(sm);
    }
}

pc_power_result_t pc_power_sm_process(pc_power_sm_t *sm,
                                       pc_power_event_t event,
                                       uint32_t now_ms)
{
    switch (sm->state) {
    case PC_STATE_OFF:      return handle_off(sm, event, now_ms);
    case PC_STATE_BOOTING:  return handle_booting(sm, event, now_ms);
    case PC_STATE_ON:       return handle_on(sm, event, now_ms);
    case PC_STATE_SLEEPING: return handle_sleeping(sm, event, now_ms);
    default:                return no_change(sm);
    }
}

const char *pc_power_state_name(pc_power_state_t state)
{
    switch (state) {
    case PC_STATE_OFF:      return "PC_OFF";
    case PC_STATE_BOOTING:  return "PC_BOOTING";
    case PC_STATE_ON:       return "PC_ON";
    case PC_STATE_SLEEPING: return "PC_SLEEPING";
    default:                return "UNKNOWN";
    }
}

const char *pc_power_event_name(pc_power_event_t event)
{
    switch (event) {
    case PC_EVENT_WAKE_REQUESTED:     return "WAKE_REQUESTED";
    case PC_EVENT_BUTTON_PRESSED:     return "BUTTON_PRESSED";
    case PC_EVENT_BUTTON_LONG_PRESSED: return "BUTTON_LONG_PRESSED";
    case PC_EVENT_USB_ENUMERATED:     return "USB_ENUMERATED";
    case PC_EVENT_USB_SUSPENDED:      return "USB_SUSPENDED";
    case PC_EVENT_POWER_LED_ON:       return "POWER_LED_ON";
    case PC_EVENT_POWER_LED_OFF:      return "POWER_LED_OFF";
    case PC_EVENT_BOOT_TIMEOUT:       return "BOOT_TIMEOUT";
    default:                          return "UNKNOWN";
    }
}
