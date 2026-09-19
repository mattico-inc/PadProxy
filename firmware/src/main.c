#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/watchdog.h"
#include "tusb.h"

#include "gamepad.h"
#include "bt_gamepad.h"
#include "usb_hid_gamepad.h"
#include "pc_power_state.h"
#include "pc_power_hal.h"
#include "ota_update.h"
#include "device_config.h"
#include "device_config_flash.h"
#include "setup_cmd.h"
#include "wifi_service.h"
#include "ir_blaster.h"
#include "ir_protocol.h"
#include "smarthome.h"

/* ── Compile-time WiFi fallback ──────────────────────────────────────── */

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

/* ── Shared state ────────────────────────────────────────────────────── */

static pc_power_sm_t s_power_sm;
static device_config_t s_config;

/**
 * Previous gamepad report per BT slot, used to detect edges (e.g. guide
 * button press). Sending the same unchanged report repeatedly is fine for
 * USB HID, but we only want to fire a wake event on the *press* edge, not
 * every poll.
 */
static gamepad_report_t s_prev_report[BT_DEVICE_MAX];
static bool s_prev_report_valid[BT_DEVICE_MAX];

/* ── CDC setup serial ───────────────────────────────────────────────── */

#define CDC_LINE_MAX 256

static char    s_cdc_line[CDC_LINE_MAX];
static uint8_t s_cdc_line_pos;

/* ── Action dispatch ─────────────────────────────────────────────────── */

/**
 * How long the power LED must remain in a new state (on or off) before
 * the state machine sees the change.  Filters out motherboard sleep-mode
 * LED blinking so it doesn't destabilise the power state machine.
 * Increase this value for boards with very slow blink patterns.
 */
#define POWER_LED_STABLE_MS 3000

/**
 * Execute hardware actions requested by a power state machine transition.
 * Timing values come from the runtime device config.
 */
static void dispatch_actions(uint32_t actions)
{
    if (actions & PC_ACTION_TRIGGER_POWER) {
        printf("[padproxy] Triggering power button (%u ms)\n",
               s_config.power_pulse_ms);
        pc_power_hal_trigger_power_button(s_config.power_pulse_ms);

        /* Auto-TV: send IR power-on when waking the PC */
        if (s_config.ir_auto_tv) {
            printf("[padproxy] Auto-TV: sending IR power code\n");
            ir_blaster_send((ir_protocol_id_t)s_config.ir_protocol,
                            s_config.ir_address, s_config.ir_command);
        }
    }
    if (actions & PC_ACTION_START_BOOT_TIMER) {
        pc_power_hal_start_boot_timer(s_config.boot_timeout_ms);
    }
    if (actions & PC_ACTION_CANCEL_BOOT_TIMER) {
        pc_power_hal_cancel_boot_timer();
    }

    /* Publish state changes to smart home */
    const smarthome_platform_t *sh = smarthome_get_platform();
    if (sh && sh->publish_pc_state)
        sh->publish_pc_state(pc_power_sm_get_state(&s_power_sm));
}

/* ── Callbacks ───────────────────────────────────────────────────────── */

/**
 * True when the PC (or, in HID_RECEIVER mode, the connected USB host) is
 * ready to receive HID input.
 *
 * DEVICE_MODE_PC_PROXY:    gated on the pc_power_sm reaching PC_STATE_ON.
 * DEVICE_MODE_HID_RECEIVER: no PC to gate on -- forward as soon as USB is
 *                           enumerated (there's no front-panel wiring to
 *                           sense in this mode).
 */
static bool pc_ready_for_input(void)
{
    if (s_config.device_mode == DEVICE_MODE_HID_RECEIVER) {
        return usb_hid_gamepad_get_state() == USB_HID_MOUNTED;
    }
    return pc_power_sm_get_state(&s_power_sm) == PC_STATE_ON;
}

/**
 * USB state change → power state machine events.
 *
 * Only meaningful in DEVICE_MODE_PC_PROXY; in DEVICE_MODE_HID_RECEIVER
 * there's no PC power state to track, so this is a no-op and
 * pc_ready_for_input() checks the USB state directly instead.
 */
static void on_usb_state_change(usb_hid_state_t state)
{
    if (s_config.device_mode != DEVICE_MODE_PC_PROXY) {
        return;
    }

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
 * Bluetooth HID device connection events (gamepad, mouse, or keyboard).
 */
static void on_bt_event(uint8_t idx, bt_gamepad_state_t state)
{
    if (state == BT_GAMEPAD_CONNECTED) {
        printf("[padproxy] Device %d connected (class=%d)\n",
               idx, bt_gamepad_get_class(idx));
        /* WiFi/BT priority mode: disable WiFi during gameplay */
        if (s_config.wifi_bt_priority)
            wifi_service_disable();
    } else {
        printf("[padproxy] Device %d disconnected\n", idx);
        if (idx < BT_DEVICE_MAX) {
            s_prev_report_valid[idx] = false;
        }
        /* WiFi/BT priority mode: re-enable WiFi */
        if (s_config.wifi_bt_priority)
            wifi_service_enable();
    }

    /* Publish BT status to smart home */
    const smarthome_platform_t *sh = smarthome_get_platform();
    if (sh && sh->publish_bt_status)
        sh->publish_bt_status(state == BT_GAMEPAD_CONNECTED);
}

/* ── WiFi API callbacks ─────────────────────────────────────────────────── */

/**
 * Handle actions triggered by the WiFi HTTP API.
 */
static void on_wifi_action(wifi_action_t action)
{
    uint32_t now = pc_power_hal_millis();
    pc_power_result_t r;
    pc_power_state_t pc_state = pc_power_sm_get_state(&s_power_sm);

    switch (action) {
    case WIFI_ACTION_POWER_TOGGLE:
        if (pc_state == PC_STATE_OFF || pc_state == PC_STATE_SLEEPING) {
            printf("[padproxy] WiFi API -> wake request\n");
            r = pc_power_sm_process(&s_power_sm, PC_EVENT_WAKE_REQUESTED,
                                    now);
            dispatch_actions(r.actions);
        }
        break;
    case WIFI_ACTION_IR_SEND:
        printf("[padproxy] WiFi API -> IR send\n");
        ir_blaster_send((ir_protocol_id_t)s_config.ir_protocol,
                        s_config.ir_address, s_config.ir_command);
        break;
    }
}

/**
 * Provide current device status to the WiFi HTTP API.
 */
static void on_wifi_status_query(wifi_device_status_t *status)
{
    status->pc_state    = pc_power_sm_get_state(&s_power_sm);
    status->bt_connected = bt_gamepad_is_connected(0);
    status->wifi_rssi   = wifi_service_get_rssi();
    status->ir_enabled  = (s_config.ir_protocol < IR_PROTO_COUNT);
    /* firmware_version is set later in main */
}

/**
 * Handle commands from the smart home platform.
 */
static void on_smarthome_cmd(smarthome_cmd_t cmd)
{
    uint32_t now = pc_power_hal_millis();
    pc_power_result_t r;
    pc_power_state_t pc_state = pc_power_sm_get_state(&s_power_sm);

    switch (cmd) {
    case SMARTHOME_CMD_POWER_TOGGLE:
    case SMARTHOME_CMD_POWER_ON:
        if (pc_state == PC_STATE_OFF || pc_state == PC_STATE_SLEEPING) {
            printf("[padproxy] Smart home -> wake request\n");
            r = pc_power_sm_process(&s_power_sm, PC_EVENT_WAKE_REQUESTED,
                                    now);
            dispatch_actions(r.actions);
        }
        break;
    case SMARTHOME_CMD_POWER_OFF:
        if (pc_state == PC_STATE_ON) {
            printf("[padproxy] Smart home -> shutdown request\n");
            r = pc_power_sm_process(&s_power_sm, PC_EVENT_WAKE_REQUESTED,
                                    now);
            dispatch_actions(r.actions);
        }
        break;
    case SMARTHOME_CMD_IR_SEND:
        printf("[padproxy] Smart home -> IR send\n");
        ir_blaster_send((ir_protocol_id_t)s_config.ir_protocol,
                        s_config.ir_address, s_config.ir_command);
        break;
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
    pc_power_state_t prev_state = pc_power_sm_get_state(&s_power_sm);

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

        /*
         * Disconnect all connected BT devices when PC powers off.
         * This saves battery and allows them to pair with other devices.
         * Only disconnect when we transition TO off, not if we were already off.
         */
        pc_power_state_t new_state = pc_power_sm_get_state(&s_power_sm);
        if (new_state == PC_STATE_OFF && prev_state != PC_STATE_OFF) {
            for (uint8_t i = 0; i < BT_DEVICE_MAX; i++) {
                if (bt_gamepad_is_connected(i)) {
                    printf("[padproxy] PC powered off (LED stable) -> disconnecting device %d\n", i);
                    bt_gamepad_disconnect(i);
                }
            }
        }
    }

    /* Boot timer expiry */
    if (pc_power_hal_boot_timer_expired()) {
        r = pc_power_sm_process(&s_power_sm, PC_EVENT_BOOT_TIMEOUT, now_ms);
        dispatch_actions(r.actions);

        /* Also disconnect on boot timeout (BOOTING → OFF transition) */
        pc_power_state_t new_state = pc_power_sm_get_state(&s_power_sm);
        if (new_state == PC_STATE_OFF && prev_state != PC_STATE_OFF) {
            for (uint8_t i = 0; i < BT_DEVICE_MAX; i++) {
                if (bt_gamepad_is_connected(i)) {
                    printf("[padproxy] Boot timeout -> disconnecting device %d\n", i);
                    bt_gamepad_disconnect(i);
                }
            }
        }
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
                if (!device_config_flash_save(&s_config)) {
                    printf("[setup] ERROR: failed to save config to flash\n");
                }
            } else if (r.action == SETUP_ACTION_REBOOT) {
                printf("[setup] Rebooting...\n");
                tud_cdc_write_flush();
                sleep_ms(100);
                watchdog_reboot(0, 0, 0);
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
 * Process one gamepad report from a given BT slot: check for wake
 * triggers, forward to USB.
 */
static void process_gamepad(uint8_t idx, const gamepad_report_t *report,
                            uint32_t now_ms)
{
    /*
     * Wake-on-controller only applies in DEVICE_MODE_PC_PROXY: there's no
     * PC to wake (and no LED/button wiring to sense) in HID_RECEIVER mode.
     */
    if (s_config.device_mode == DEVICE_MODE_PC_PROXY) {
        pc_power_state_t pc_state = pc_power_sm_get_state(&s_power_sm);

        /*
         * Wake-on-controller: fire WAKE_REQUESTED on the *rising edge* of
         * the guide button when the PC is off or sleeping.
         */
        bool guide_now  = gamepad_report_guide_pressed(report);
        bool guide_prev = s_prev_report_valid[idx] &&
                          gamepad_report_guide_pressed(&s_prev_report[idx]);

        if (guide_now && !guide_prev) {
            if (pc_state == PC_STATE_OFF || pc_state == PC_STATE_SLEEPING) {
                printf("[padproxy] Guide button -> wake request (PC %s)\n",
                         pc_power_state_name(pc_state));
                pc_power_result_t r = pc_power_sm_process(
                    &s_power_sm, PC_EVENT_WAKE_REQUESTED, now_ms);
                dispatch_actions(r.actions);
            }
        }
    }

    /* Forward to USB only when the PC/host is ready for input. */
    if (pc_ready_for_input()) {
        usb_hid_gamepad_send_report(report);
    }

    s_prev_report[idx] = *report;
    s_prev_report_valid[idx] = true;
}

int main(void)
{
    stdio_init_all();
    printf("[padproxy] PadProxy starting\n");

    /* Load device config before OTA so WiFi credentials are available.
    * Falls back to compiled-in defaults on first boot or flash error. */
   device_config_init(&s_config);
   if (device_config_flash_load(&s_config)) {
       printf("[padproxy] Loaded device config from flash\n");
   } else {
       printf("[padproxy] No valid config in flash, using defaults\n");
   }

   /* Accept this image immediately so the boot ROM does not roll back
    * a TBYB (Try Before You Buy) flash-update boot.  Must happen
    * within ~16.7 s of reset — do it first thing. */
   ota_accept_current_image();

    /* Build WiFi credentials: prefer runtime config, fall back to
     * compile-time defines (which may be empty). */
    ota_wifi_creds_t wifi_creds;
    if (s_config.wifi_ssid[0] != '\0') {
        wifi_creds.ssid     = s_config.wifi_ssid;
        wifi_creds.password = s_config.wifi_password;
    } else {
        wifi_creds.ssid     = WIFI_SSID;
        wifi_creds.password = WIFI_PASSWORD;
    }

    /* Check for OTA firmware update over WiFi (before BT init).
     * WiFi is kept alive after OTA for the persistent WiFi service.
     * Skipped instantly if no WiFi SSID is configured. */
    ota_update_result_t ota = ota_update_check_and_apply(&wifi_creds);
    printf("[padproxy] OTA check result: %s\n", ota_update_result_name(ota));

    /*
     * TODO: USB firmware update support
     *
     * In addition to WiFi OTA, support firmware updates over the CDC
     * serial interface.  This would let users flash new firmware via
     * the Web Serial setup page without needing WiFi configured.
     *
     * Approach: add a "firmware" command to setup_cmd that:
     *   1. Accepts a binary size ("firmware <nbytes>")
     *   2. Switches CDC to binary receive mode
     *   3. Streams the .bin payload into the inactive flash partition
     *      (reusing the flash_writer from ota_update.c)
     *   4. On completion, verifies CRC and issues a TBYB reboot
     *
     * The Web Serial page would read a local .bin file and stream it
     * over the CDC serial port using the Web Serial API's WritableStream.
     *
     * This makes the device fully configurable and updatable over USB
     * with zero WiFi dependency — WiFi becomes optional (for automatic
     * background OTA checks only).
     */

    /* Setup command handler version string */
    static char version_str[20];
    snprintf(version_str, sizeof(version_str), "%d.%d.%d",
             PADPROXY_VERSION_MAJOR, PADPROXY_VERSION_MINOR,
             PADPROXY_VERSION_PATCH);
    setup_cmd_set_version(version_str);

    /* Initialize power management.
     * pc_power_sm_init() is pure logic and harmless to call in either
     * mode. The GPIO-backed HAL init and LED state are only meaningful
     * in DEVICE_MODE_PC_PROXY -- HID_RECEIVER mode has no front-panel
     * wiring to read. */
    pc_power_sm_init(&s_power_sm);
    if (s_config.device_mode == DEVICE_MODE_PC_PROXY) {
        pc_power_hal_init();
        s_led_reading   = pc_power_hal_read_power_led();
        s_led_debounced = s_led_reading;
        s_led_changed_ms = 0;
    }

    /* Initialize IR blaster */
    ir_blaster_init();

    /* Initialize persistent WiFi service (HTTP API, mDNS) */
    wifi_service_config_t wifi_svc_cfg = {
        .ssid     = wifi_creds.ssid,
        .password = wifi_creds.password,
        .hostname = s_config.device_name,
    };
    wifi_service_init(&wifi_svc_cfg, on_wifi_action, on_wifi_status_query);

    /* Initialize smart home platform (MQTT for Home Assistant, etc.) */
    const smarthome_platform_t *sh = smarthome_get_platform();
    if (sh && sh->init) {
        if (sh->init(&s_config, on_smarthome_cmd)) {
            printf("[padproxy] Smart home platform initialized\n");
        } else {
            printf("[padproxy] Smart home platform init failed (will retry)\n");
        }
    }

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
        snprintf(status_buf, sizeof(status_buf),
                 "pc_state=%s bt_connected=%s device_mode=%s wifi=%s",
                 pc_power_state_name(pc_power_sm_get_state(&s_power_sm)),
                 bt_gamepad_is_connected(0) ? "true" : "false",
                 s_config.device_mode == DEVICE_MODE_HID_RECEIVER
                     ? "hid_receiver" : "pc_proxy",
                 wifi_state_name(wifi_service_get_state()));
        setup_cmd_set_status(status_buf);
        poll_cdc_setup();

        /* Poll hardware inputs (power LED, boot timer) -- PC_PROXY only;
         * HID_RECEIVER has no front-panel wiring to sense. */
        if (s_config.device_mode == DEVICE_MODE_PC_PROXY) {
            poll_hardware(now_ms);
        }

        /* Poll WiFi service (reconnection, HTTP serving) */
        wifi_service_task(now_ms);

        /* Poll smart home platform (MQTT keepalive, etc.) */
        if (sh && sh->task)
            sh->task(now_ms);

        /* Read BT devices (gamepad/mouse/keyboard, any mix) and forward. */
        for (uint8_t idx = 0; idx < BT_DEVICE_MAX; idx++) {
            switch (bt_gamepad_get_class(idx)) {
            case BT_DEVICE_CLASS_GAMEPAD: {
                gamepad_report_t report;
                if (bt_gamepad_get_report(idx, &report)) {
                    process_gamepad(idx, &report, now_ms);
                }
                break;
            }
            case BT_DEVICE_CLASS_MOUSE: {
                usb_mouse_report_t report;
                if (bt_gamepad_get_mouse_report(idx, &report) &&
                    pc_ready_for_input()) {
                    usb_hid_mouse_send_report(&report);
                }
                break;
            }
            case BT_DEVICE_CLASS_KEYBOARD: {
                usb_keyboard_report_t report;
                if (bt_gamepad_get_keyboard_report(idx, &report) &&
                    pc_ready_for_input()) {
                    usb_hid_keyboard_send_report(&report);
                }
                break;
            }
            case BT_DEVICE_CLASS_NONE:
            default:
                break;
            }
        }

        /* ~1ms loop period for responsive input */
        sleep_ms(1);
    }
}
