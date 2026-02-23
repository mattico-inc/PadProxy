#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "gamepad.h"
#include "bt_gamepad.h"
#include "usb_hid_gamepad.h"
#include "pc_power_state.h"
#include "pc_power_hal.h"
#include "ota_update.h"

/* ── Shared state ────────────────────────────────────────────────────── */

static pc_power_sm_t s_power_sm;

/**
 * Previous gamepad report, used to detect edges (e.g. guide button press).
 * Sending the same unchanged report repeatedly is fine for USB HID, but
 * we only want to fire a wake event on the *press* edge, not every poll.
 */
static gamepad_report_t s_prev_report;
static bool s_prev_report_valid;

/* ── Action dispatch ─────────────────────────────────────────────────── */

#define POWER_PULSE_MS      200
#define BOOT_TIMEOUT_MS     30000

/**
 * Execute hardware actions requested by a power state machine transition.
 */
static void dispatch_actions(uint32_t actions)
{
    if (actions & PC_ACTION_TRIGGER_POWER) {
        printf("[padproxy] Triggering power button (%d ms)\n", POWER_PULSE_MS);
        pc_power_hal_trigger_power_button(POWER_PULSE_MS);
    }
    if (actions & PC_ACTION_START_BOOT_TIMER) {
        pc_power_hal_start_boot_timer(BOOT_TIMEOUT_MS);
    }
    if (actions & PC_ACTION_CANCEL_BOOT_TIMER) {
        pc_power_hal_cancel_boot_timer();
    }
}

/* ── Callbacks ───────────────────────────────────────────────────────── */

/**
 * USB state change → power state machine events.
 */
static void on_usb_state_change(usb_hid_state_t state)
{
    uint32_t now = pc_power_hal_millis();
    pc_power_result_t r;

    switch (state) {
    case USB_HID_MOUNTED:
        printf("[padproxy] USB mounted -> PC_EVENT_USB_ENUMERATED\n");
        r = pc_power_sm_process(&s_power_sm, PC_EVENT_USB_ENUMERATED, now);
        dispatch_actions(r.actions);
        break;
    case USB_HID_SUSPENDED:
    case USB_HID_NOT_MOUNTED:
        printf("[padproxy] USB suspended/unmounted -> PC_EVENT_USB_SUSPENDED\n");
        r = pc_power_sm_process(&s_power_sm, PC_EVENT_USB_SUSPENDED, now);
        dispatch_actions(r.actions);
        break;
    }
}

/**
 * Bluetooth gamepad connection events.
 */
static void on_bt_event(uint8_t idx, bt_gamepad_state_t state)
{
    if (state == BT_GAMEPAD_CONNECTED) {
        printf("[padproxy] Gamepad %d connected\n", idx);
    } else {
        printf("[padproxy] Gamepad %d disconnected\n", idx);
        s_prev_report_valid = false;
    }
}

/* ── Hardware polling ────────────────────────────────────────────────── */

static bool s_prev_led;

/**
 * Poll GPIO inputs and timers, feed edge-triggered events into the power SM.
 */
static void poll_hardware(uint32_t now_ms)
{
    bool led = pc_power_hal_read_power_led();
    pc_power_result_t r;

    /* Power LED edges */
    if (led && !s_prev_led) {
        r = pc_power_sm_process(&s_power_sm, PC_EVENT_POWER_LED_ON, now_ms);
        dispatch_actions(r.actions);
    } else if (!led && s_prev_led) {
        r = pc_power_sm_process(&s_power_sm, PC_EVENT_POWER_LED_OFF, now_ms);
        dispatch_actions(r.actions);
    }

    /* Boot timer expiry */
    if (pc_power_hal_boot_timer_expired()) {
        r = pc_power_sm_process(&s_power_sm, PC_EVENT_BOOT_TIMEOUT, now_ms);
        dispatch_actions(r.actions);
    }

    s_prev_led = led;
}

/* ── Main loop ───────────────────────────────────────────────────────── */

/**
 * Process one gamepad report: check for wake triggers, forward to USB.
 */
static void process_gamepad(const gamepad_report_t *report, uint32_t now_ms)
{
    pc_power_state_t pc_state = pc_power_sm_get_state(&s_power_sm);

    /*
     * Wake-on-controller: fire WAKE_REQUESTED on the *rising edge* of
     * the guide button when the PC is off or sleeping.
     */
    bool guide_now  = gamepad_report_guide_pressed(report);
    bool guide_prev = s_prev_report_valid &&
                      gamepad_report_guide_pressed(&s_prev_report);

    if (guide_now && !guide_prev) {
        if (pc_state == PC_STATE_OFF || pc_state == PC_STATE_SLEEPING) {
            printf("[padproxy] Guide button -> wake request (PC %s)\n",
                     pc_power_state_name(pc_state));
            pc_power_result_t r = pc_power_sm_process(
                &s_power_sm, PC_EVENT_WAKE_REQUESTED, now_ms);
            dispatch_actions(r.actions);
        }
    }

    /* Forward to USB only when the PC is on and USB is enumerated. */
    if (pc_state == PC_STATE_ON) {
        usb_hid_gamepad_send_report(report);
    }

    s_prev_report = *report;
    s_prev_report_valid = true;
}

int main(void)
{
    stdio_init_all();
    printf("[padproxy] PadProxy starting\n");

    /* Accept this image immediately so the boot ROM does not roll back
     * a TBYB (Try Before You Buy) flash-update boot.  Must happen
     * within ~16.7 s of reset — do it first thing. */
    ota_accept_current_image();

    /* Check for OTA firmware update over WiFi (before BT init).
     * Uses the CYW43 radio for WiFi, then deinits so Bluepad32 can
     * reinitialize it for Bluetooth.  Skipped instantly if no WiFi
     * SSID is configured at compile time. */
    ota_update_result_t ota = ota_update_check_and_apply();
    printf("[padproxy] OTA check result: %s\n", ota_update_result_name(ota));

    /* Initialize power management */
    pc_power_hal_init();
    pc_power_sm_init(&s_power_sm);
    s_prev_led = pc_power_hal_read_power_led();

    /* Initialize USB HID gamepad (must be before BT so USB is ready) */
    usb_hid_gamepad_init(on_usb_state_change);

    /* Initialize Bluetooth gamepad */
    bt_gamepad_init(on_bt_event);

    printf("[padproxy] Initialization complete, entering main loop\n");

    /* Main loop: poll BT → proxy to USB, poll hardware → power SM */
    for (;;) {
        uint32_t now_ms = pc_power_hal_millis();

        /* Service USB stack */
        usb_hid_gamepad_task();

        /* Poll hardware inputs (power LED, boot timer) */
        poll_hardware(now_ms);

        /* Read BT gamepad and forward */
        gamepad_report_t report;
        if (bt_gamepad_get_report(0, &report)) {
            process_gamepad(&report, now_ms);
        }

        /* ~1ms loop period for responsive input */
        sleep_ms(1);
    }
}
