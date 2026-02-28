#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "tusb.h"

#include "gamepad.h"
#include "bt_gamepad.h"
#include "usb_hid_gamepad.h"
#include "pc_power_state.h"
#include "pc_power_hal.h"
#include "ota_update.h"
#include "device_config.h"
#include "setup_cmd.h"

/* ── Shared state ────────────────────────────────────────────────────── */

static pc_power_sm_t s_power_sm;
static device_config_t s_config;

/**
 * Previous gamepad report, used to detect edges (e.g. guide button press).
 * Sending the same unchanged report repeatedly is fine for USB HID, but
 * we only want to fire a wake event on the *press* edge, not every poll.
 */
static gamepad_report_t s_prev_report;
static bool s_prev_report_valid;

/* ── CDC setup serial ───────────────────────────────────────────────── */

#define CDC_LINE_MAX 256

static char    s_cdc_line[CDC_LINE_MAX];
static uint8_t s_cdc_line_pos;

/* ── Action dispatch ─────────────────────────────────────────────────── */

#define POWER_PULSE_MS      200
#define BOOT_TIMEOUT_MS     30000

/**
 * How long the power LED must remain in a new state (on or off) before
 * the state machine sees the change.  Filters out motherboard sleep-mode
 * LED blinking so it doesn't destabilise the power state machine.
 * Increase this value for boards with very slow blink patterns.
 */
#define POWER_LED_STABLE_MS 3000

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

/** Debounced power-LED state. */
static bool     s_led_reading;     /* latest raw GPIO sample             */
static bool     s_led_debounced;   /* accepted (stable) LED state        */
static uint32_t s_led_changed_ms;  /* when s_led_reading last changed    */

/**
 * Poll GPIO inputs and timers, feed edge-triggered events into the power SM.
 *
 * The power LED is debounced: a new reading must persist for at least
 * POWER_LED_STABLE_MS before the state machine is notified.  This
 * prevents motherboard sleep-blink patterns from bouncing the SM.
 */
static void poll_hardware(uint32_t now_ms)
{
    bool led = pc_power_hal_read_power_led();
    pc_power_result_t r;

    /* Reset debounce timer whenever the raw reading changes */
    if (led != s_led_reading) {
        s_led_reading   = led;
        s_led_changed_ms = now_ms;
    }

    /* Emit an edge event only after the reading has been stable */
    if (s_led_reading != s_led_debounced &&
        (now_ms - s_led_changed_ms) >= POWER_LED_STABLE_MS) {
        s_led_debounced = s_led_reading;
        if (s_led_debounced) {
            r = pc_power_sm_process(&s_power_sm, PC_EVENT_POWER_LED_ON,
                                    now_ms);
        } else {
            r = pc_power_sm_process(&s_power_sm, PC_EVENT_POWER_LED_OFF,
                                    now_ms);
        }
        dispatch_actions(r.actions);
    }

    /* Boot timer expiry */
    if (pc_power_hal_boot_timer_expired()) {
        r = pc_power_sm_process(&s_power_sm, PC_EVENT_BOOT_TIMEOUT, now_ms);
        dispatch_actions(r.actions);
    }
}

/* ── CDC setup serial ─────────────────────────────────────────────── */

/**
 * Poll the CDC serial interface for complete lines and process them
 * through the setup command handler.
 */
static void poll_cdc_setup(void)
{
    while (tud_cdc_available()) {
        int ch = tud_cdc_read_char();
        if (ch < 0) break;

        if (ch == '\n' || ch == '\r') {
            if (s_cdc_line_pos == 0)
                continue;  /* skip empty lines / lone CR or LF */

            s_cdc_line[s_cdc_line_pos] = '\0';

            char response[512];
            setup_cmd_result_t r = setup_cmd_process(
                s_cdc_line, &s_config, response, sizeof(response));

            if (r.out_len > 0) {
                tud_cdc_write(response, (uint32_t)r.out_len);
                tud_cdc_write_flush();
            }

            if (r.action == SETUP_ACTION_SAVE) {
                printf("[setup] Saving config to flash\n");
                /* TODO: persist s_config to flash sector */
            } else if (r.action == SETUP_ACTION_REBOOT) {
                printf("[setup] Rebooting...\n");
                tud_cdc_write_flush();
                sleep_ms(100);
                /* TODO: watchdog_reboot() or rom reboot */
            }

            s_cdc_line_pos = 0;
        } else if (s_cdc_line_pos < CDC_LINE_MAX - 1) {
            s_cdc_line[s_cdc_line_pos++] = (char)ch;
        }
        /* else: line too long, silently drop excess characters */
    }
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

    /* Load device config (defaults until flash persistence is wired up) */
    device_config_init(&s_config);
    /* TODO: attempt device_config_deserialize() from flash sector */

    /* Setup command handler version string */
    static char version_str[20];
    snprintf(version_str, sizeof(version_str), "%d.%d.%d",
             PADPROXY_VERSION_MAJOR, PADPROXY_VERSION_MINOR,
             PADPROXY_VERSION_PATCH);
    setup_cmd_set_version(version_str);

    /* Initialize power management */
    pc_power_hal_init();
    pc_power_sm_init(&s_power_sm);
    s_led_reading   = pc_power_hal_read_power_led();
    s_led_debounced = s_led_reading;
    s_led_changed_ms = 0;

    /* Initialize USB HID gamepad + CDC setup serial */
    usb_hid_gamepad_init(on_usb_state_change);

    /* Initialize Bluetooth gamepad */
    bt_gamepad_init(on_bt_event);

    printf("[padproxy] Initialization complete, entering main loop\n");

    /* Update status string for setup command handler */
    static char status_buf[64];

    /* Main loop: poll BT → proxy to USB, poll hardware → power SM */
    for (;;) {
        uint32_t now_ms = pc_power_hal_millis();

        /* Service USB stack */
        usb_hid_gamepad_task();

        /* Poll CDC setup serial */
        snprintf(status_buf, sizeof(status_buf), "pc_state=%s bt_connected=%s",
                 pc_power_state_name(pc_power_sm_get_state(&s_power_sm)),
                 bt_gamepad_get_report(0, NULL) ? "true" : "false");
        setup_cmd_set_status(status_buf);
        poll_cdc_setup();

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
