/*
 * Device Integration Tests
 *
 * End-to-end tests for PadProxy with mocked hardware interfaces.
 * Exercises the full event pipeline: BT input → state machine → USB output.
 *
 * Real modules:  pc_power_state.c, usb_hid_report.c, gamepad.h
 * Mocked:        pc_power_hal, bt_gamepad, usb_hid_gamepad
 *
 * The test harness replicates main.c's orchestration logic so we can drive
 * realistic scenarios (controller connect, PC wake, gamepad input forwarding)
 * without any hardware dependencies.
 */

#include "unity.h"
#include "gamepad.h"
#include "pc_power_state.h"
#include "pc_power_hal.h"
#include "usb_hid_gamepad.h"
#include "bt_gamepad.h"
#include "usb_hid_report.h"

#include <string.h>

/* ── Mock: PC power HAL ─────────────────────────────────────────────── */

static struct {
    uint32_t millis;
    bool     power_led;
    bool     boot_timer_running;
    uint32_t boot_timer_start_ms;
    uint32_t boot_timer_timeout_ms;
    int      power_btn_trigger_count;
    uint32_t power_btn_last_duration_ms;
} s_hal;

void pc_power_hal_init(void) { }
bool pc_power_hal_read_power_led(void) { return s_hal.power_led; }
uint32_t pc_power_hal_millis(void) { return s_hal.millis; }

void pc_power_hal_trigger_power_button(uint32_t duration_ms)
{
    s_hal.power_btn_trigger_count++;
    s_hal.power_btn_last_duration_ms = duration_ms;
}

void pc_power_hal_start_boot_timer(uint32_t timeout_ms)
{
    s_hal.boot_timer_running    = true;
    s_hal.boot_timer_start_ms   = s_hal.millis;
    s_hal.boot_timer_timeout_ms = timeout_ms;
}

void pc_power_hal_cancel_boot_timer(void)
{
    s_hal.boot_timer_running = false;
}

bool pc_power_hal_boot_timer_expired(void)
{
    if (s_hal.boot_timer_running &&
        (s_hal.millis - s_hal.boot_timer_start_ms) >=
            s_hal.boot_timer_timeout_ms) {
        s_hal.boot_timer_running = false;
        return true;
    }
    return false;
}

/* ── Mock: Bluetooth gamepad ────────────────────────────────────────── */

static struct {
    bool                  connected;
    gamepad_report_t      report;
    bt_gamepad_event_cb_t event_cb;
} s_bt;

void bt_gamepad_init(bt_gamepad_event_cb_t cb) { s_bt.event_cb = cb; }
bool bt_gamepad_is_connected(uint8_t idx)  { (void)idx; return s_bt.connected; }
void bt_gamepad_set_pairing(bool enabled)  { (void)enabled; }

bool bt_gamepad_get_report(uint8_t idx, gamepad_report_t *report)
{
    (void)idx;
    if (!s_bt.connected) return false;
    *report = s_bt.report;
    return true;
}

/* ── Mock: USB HID gamepad ──────────────────────────────────────────── */

static struct {
    usb_hid_state_t      state;
    usb_hid_state_cb_t   state_cb;
    usb_gamepad_report_t last_report;
    bool                 report_sent;
    int                  report_count;
} s_usb;

void usb_hid_gamepad_init(usb_hid_state_cb_t cb)
{
    s_usb.state_cb = cb;
    s_usb.state    = USB_HID_NOT_MOUNTED;
}

void usb_hid_gamepad_task(void) { }

usb_hid_state_t usb_hid_gamepad_get_state(void) { return s_usb.state; }

bool usb_hid_gamepad_send_report(const gamepad_report_t *report)
{
    if (s_usb.state != USB_HID_MOUNTED) return false;
    usb_hid_report_from_gamepad(report, &s_usb.last_report);
    s_usb.report_sent = true;
    s_usb.report_count++;
    return true;
}

/* ── Device orchestration (mirrors main.c) ──────────────────────────── */

#define POWER_PULSE_MS  200
#define BOOT_TIMEOUT_MS 30000

static pc_power_sm_t    s_sm;
static gamepad_report_t s_prev_report;
static bool             s_prev_report_valid;
static bool             s_prev_led;

static void dispatch_actions(uint32_t actions)
{
    if (actions & PC_ACTION_TRIGGER_POWER)
        pc_power_hal_trigger_power_button(POWER_PULSE_MS);
    if (actions & PC_ACTION_START_BOOT_TIMER)
        pc_power_hal_start_boot_timer(BOOT_TIMEOUT_MS);
    if (actions & PC_ACTION_CANCEL_BOOT_TIMER)
        pc_power_hal_cancel_boot_timer();
}

static void on_usb_state_change(usb_hid_state_t state)
{
    uint32_t now = pc_power_hal_millis();
    pc_power_result_t r;

    switch (state) {
    case USB_HID_MOUNTED:
        r = pc_power_sm_process(&s_sm, PC_EVENT_USB_ENUMERATED, now);
        dispatch_actions(r.actions);
        break;
    case USB_HID_SUSPENDED:
    case USB_HID_NOT_MOUNTED:
        r = pc_power_sm_process(&s_sm, PC_EVENT_USB_SUSPENDED, now);
        dispatch_actions(r.actions);
        break;
    }
}

static void on_bt_event(uint8_t idx, bt_gamepad_state_t state)
{
    (void)idx;
    if (state == BT_GAMEPAD_DISCONNECTED)
        s_prev_report_valid = false;
}

static void device_poll_hardware(uint32_t now_ms)
{
    bool led = pc_power_hal_read_power_led();
    pc_power_result_t r;

    if (led && !s_prev_led) {
        r = pc_power_sm_process(&s_sm, PC_EVENT_POWER_LED_ON, now_ms);
        dispatch_actions(r.actions);
    } else if (!led && s_prev_led) {
        r = pc_power_sm_process(&s_sm, PC_EVENT_POWER_LED_OFF, now_ms);
        dispatch_actions(r.actions);
    }

    if (pc_power_hal_boot_timer_expired()) {
        r = pc_power_sm_process(&s_sm, PC_EVENT_BOOT_TIMEOUT, now_ms);
        dispatch_actions(r.actions);
    }

    s_prev_led = led;
}

static void device_process_gamepad(const gamepad_report_t *report,
                                   uint32_t now_ms)
{
    pc_power_state_t st = pc_power_sm_get_state(&s_sm);

    bool guide_now  = gamepad_report_guide_pressed(report);
    bool guide_prev = s_prev_report_valid &&
                      gamepad_report_guide_pressed(&s_prev_report);

    if (guide_now && !guide_prev) {
        if (st == PC_STATE_OFF || st == PC_STATE_SLEEPING) {
            pc_power_result_t r = pc_power_sm_process(
                &s_sm, PC_EVENT_WAKE_REQUESTED, now_ms);
            dispatch_actions(r.actions);
        }
    }

    if (st == PC_STATE_ON)
        usb_hid_gamepad_send_report(report);

    s_prev_report       = *report;
    s_prev_report_valid = true;
}

/* ── Device lifecycle ───────────────────────────────────────────────── */

static void device_init(void)
{
    memset(&s_hal, 0, sizeof(s_hal));
    memset(&s_bt,  0, sizeof(s_bt));
    memset(&s_usb, 0, sizeof(s_usb));
    memset(&s_prev_report, 0, sizeof(s_prev_report));
    s_prev_report_valid = false;

    pc_power_sm_init(&s_sm);
    s_prev_led = pc_power_hal_read_power_led();

    usb_hid_gamepad_init(on_usb_state_change);
    bt_gamepad_init(on_bt_event);
}

/** One iteration of the main loop. */
static void device_tick(uint32_t now_ms)
{
    s_hal.millis = now_ms;
    usb_hid_gamepad_task();
    device_poll_hardware(now_ms);

    gamepad_report_t report;
    if (bt_gamepad_get_report(0, &report))
        device_process_gamepad(&report, now_ms);
}

/* ── Test injection helpers ─────────────────────────────────────────── */

static void inject_bt_connect(void)
{
    s_bt.connected = true;
    memset(&s_bt.report, 0, sizeof(s_bt.report));
    s_bt.report.dpad = GAMEPAD_DPAD_CENTERED;
    if (s_bt.event_cb) s_bt.event_cb(0, BT_GAMEPAD_CONNECTED);
}

static void inject_bt_disconnect(void)
{
    s_bt.connected = false;
    if (s_bt.event_cb) s_bt.event_cb(0, BT_GAMEPAD_DISCONNECTED);
}

static void inject_bt_report(const gamepad_report_t *r)
{
    s_bt.report = *r;
}

static void inject_usb_mount(void)
{
    s_usb.state = USB_HID_MOUNTED;
    if (s_usb.state_cb) s_usb.state_cb(USB_HID_MOUNTED);
}

static void inject_usb_suspend(void)
{
    s_usb.state = USB_HID_SUSPENDED;
    if (s_usb.state_cb) s_usb.state_cb(USB_HID_SUSPENDED);
}

static void inject_usb_unmount(void)
{
    s_usb.state = USB_HID_NOT_MOUNTED;
    if (s_usb.state_cb) s_usb.state_cb(USB_HID_NOT_MOUNTED);
}

/* ── Report builders ────────────────────────────────────────────────── */

static gamepad_report_t make_idle_report(void)
{
    gamepad_report_t r;
    memset(&r, 0, sizeof(r));
    r.dpad = GAMEPAD_DPAD_CENTERED;
    return r;
}

static gamepad_report_t make_guide_report(void)
{
    gamepad_report_t r = make_idle_report();
    r.buttons = GAMEPAD_BTN_GUIDE;
    return r;
}

/* ── State helpers ──────────────────────────────────────────────────── */

/** Drive the device from OFF → ON via power LED edge + USB mount. */
static void drive_to_on(uint32_t at_ms)
{
    s_hal.power_led = true;
    device_tick(at_ms);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));

    s_hal.millis = at_ms + 5000;
    inject_usb_mount();
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&s_sm));
}

/** Drive the device from OFF → ON → SLEEPING. */
static void drive_to_sleeping(uint32_t at_ms)
{
    drive_to_on(at_ms);
    s_hal.millis = at_ms + 10000;
    inject_usb_suspend();
    TEST_ASSERT_EQUAL(PC_STATE_SLEEPING, pc_power_sm_get_state(&s_sm));
}

/* ── Unity fixtures ─────────────────────────────────────────────────── */

void setUp(void)
{
    device_init();
}

void tearDown(void) { }

/* ── BT connect and wake from OFF ───────────────────────────────────── */

void test_bt_connect_guide_press_wakes_pc_from_off(void)
{
    inject_bt_connect();

    /* Press guide button */
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);

    device_tick(1000);

    /* Should have transitioned OFF → BOOTING via WAKE_REQUESTED */
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
}

void test_wake_from_off_triggers_power_button(void)
{
    inject_bt_connect();
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);

    TEST_ASSERT_EQUAL(0, s_hal.power_btn_trigger_count);
    device_tick(1000);

    TEST_ASSERT_EQUAL(1, s_hal.power_btn_trigger_count);
    TEST_ASSERT_EQUAL_UINT32(POWER_PULSE_MS, s_hal.power_btn_last_duration_ms);
    TEST_ASSERT_TRUE(s_hal.boot_timer_running);
}

void test_boot_sequence_completes_to_on(void)
{
    inject_bt_connect();
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);

    /* Guide press → BOOTING */
    device_tick(1000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));

    /* Power LED comes on (simulated PC boot) */
    s_hal.power_led = true;
    device_tick(2000);
    /* POWER_LED_ON ignored in BOOTING — stays in BOOTING */
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));

    /* USB host enumerates device (OS finished booting) */
    s_hal.millis = 8000;
    inject_usb_mount();
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&s_sm));

    /* Boot timer should have been cancelled */
    TEST_ASSERT_FALSE(s_hal.boot_timer_running);
}

/* ── Wake from sleep ────────────────────────────────────────────────── */

void test_wake_sleeping_pc_with_guide(void)
{
    inject_bt_connect();
    drive_to_sleeping(0);

    /* Press guide while PC sleeps */
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);

    int triggers_before = s_hal.power_btn_trigger_count;
    s_hal.millis = 20000;
    device_tick(20000);

    /* Should have triggered power button and transitioned to BOOTING */
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_EQUAL(triggers_before + 1, s_hal.power_btn_trigger_count);
    TEST_ASSERT_TRUE(s_hal.boot_timer_running);
}

void test_wake_from_sleep_completes_to_on(void)
{
    inject_bt_connect();
    drive_to_sleeping(0);

    /* Guide press → BOOTING */
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);
    device_tick(20000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));

    /* USB re-enumerates → ON */
    s_hal.millis = 23000;
    inject_usb_mount();
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&s_sm));

    /* Input forwarding should work: send a button press */
    gamepad_report_t play = make_idle_report();
    play.buttons = GAMEPAD_BTN_A;
    inject_bt_report(&play);
    s_usb.report_sent = false;
    device_tick(24000);

    TEST_ASSERT_TRUE(s_usb.report_sent);
    TEST_ASSERT_EQUAL_UINT16(GAMEPAD_BTN_A, s_usb.last_report.buttons);
}

/* ── Sleep with blinking LED ────────────────────────────────────────── */

void test_sleep_blinking_led_wake_with_guide(void)
{
    /*
     * Some motherboards blink the power LED during sleep. Each LED edge
     * triggers the state machine (SLEEPING → OFF on LED-off, OFF → BOOTING
     * on LED-on, BOOTING → OFF on LED-off). The guide button still wakes
     * the PC because WAKE_REQUESTED in OFF triggers the real power button.
     */
    inject_bt_connect();
    drive_to_sleeping(0);
    /* After drive_to_sleeping: LED=on, prev_led=on, state=SLEEPING */

    /* Blink 1: LED off → SLEEPING → OFF */
    s_hal.power_led = false;
    device_tick(12000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));

    /* Blink 1: LED on → OFF → BOOTING (starts boot timer, no power pulse) */
    s_hal.power_led = true;
    device_tick(13000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_EQUAL(0, s_hal.power_btn_trigger_count);

    /* Blink 2: LED off → BOOTING → OFF (cancels boot timer) */
    s_hal.power_led = false;
    device_tick(14000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_FALSE(s_hal.boot_timer_running);

    /* Now press guide during an OFF phase */
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);
    device_tick(15000);

    /* WAKE_REQUESTED triggers the real power button */
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_EQUAL(1, s_hal.power_btn_trigger_count);

    /* PC wakes for real — LED stays on, USB mounts */
    s_hal.power_led = true;
    device_tick(16000);
    s_hal.millis = 20000;
    inject_usb_mount();
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&s_sm));
}

/* ── Gamepad input forwarding ───────────────────────────────────────── */

void test_input_forwarded_when_pc_on(void)
{
    inject_bt_connect();
    drive_to_on(0);

    gamepad_report_t play = make_idle_report();
    play.buttons = GAMEPAD_BTN_A | GAMEPAD_BTN_Y;
    inject_bt_report(&play);

    s_usb.report_sent = false;
    device_tick(6000);

    TEST_ASSERT_TRUE(s_usb.report_sent);
    TEST_ASSERT_EQUAL_UINT16(GAMEPAD_BTN_A | GAMEPAD_BTN_Y,
                             s_usb.last_report.buttons);
}

void test_full_report_conversion(void)
{
    inject_bt_connect();
    drive_to_on(0);

    gamepad_report_t in = {
        .lx = 1000,  .ly = -2000,
        .rx = 3000,  .ry = -4000,
        .lt = 512,   .rt = 1023,
        .buttons = GAMEPAD_BTN_A | GAMEPAD_BTN_B,
        .dpad = GAMEPAD_DPAD_RIGHT,
    };
    inject_bt_report(&in);
    s_usb.report_sent = false;
    device_tick(6000);

    TEST_ASSERT_TRUE(s_usb.report_sent);

    usb_gamepad_report_t *out = &s_usb.last_report;
    TEST_ASSERT_EQUAL_INT16(1000,  out->lx);
    TEST_ASSERT_EQUAL_INT16(-2000, out->ly);
    TEST_ASSERT_EQUAL_INT16(3000,  out->rx);
    TEST_ASSERT_EQUAL_INT16(-4000, out->ry);
    TEST_ASSERT_EQUAL_UINT8(128,   out->lt);   /* 512 / 4 */
    TEST_ASSERT_EQUAL_UINT8(255,   out->rt);   /* 1023 / 4 */
    TEST_ASSERT_EQUAL_UINT8(3,     out->hat);  /* RIGHT=2 → USB 2+1=3 */
    TEST_ASSERT_EQUAL_UINT16(GAMEPAD_BTN_A | GAMEPAD_BTN_B, out->buttons);
}

void test_input_not_forwarded_when_pc_off(void)
{
    inject_bt_connect();
    /* PC is OFF (default) */

    gamepad_report_t play = make_idle_report();
    play.buttons = GAMEPAD_BTN_A;
    inject_bt_report(&play);

    s_usb.report_count = 0;
    device_tick(1000);

    TEST_ASSERT_EQUAL(0, s_usb.report_count);
}

void test_input_not_forwarded_when_pc_booting(void)
{
    inject_bt_connect();

    /* Get to BOOTING via LED */
    s_hal.power_led = true;
    device_tick(0);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));

    /* Send non-guide input */
    gamepad_report_t play = make_idle_report();
    play.buttons = GAMEPAD_BTN_A;
    inject_bt_report(&play);

    s_usb.report_count = 0;
    device_tick(1000);

    TEST_ASSERT_EQUAL(0, s_usb.report_count);
}

/* ── Boot timeout ───────────────────────────────────────────────────── */

void test_boot_timeout_returns_to_off(void)
{
    inject_bt_connect();
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);

    /* Guide press → BOOTING */
    device_tick(0);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_TRUE(s_hal.boot_timer_running);

    /* Release guide so no repeated wake on next tick */
    gamepad_report_t idle = make_idle_report();
    inject_bt_report(&idle);

    /* Advance past 30 s boot timeout */
    device_tick(31000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_FALSE(s_hal.boot_timer_running);
}

void test_boot_timeout_retry_succeeds(void)
{
    inject_bt_connect();
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);

    /* First attempt: guide → BOOTING */
    device_tick(0);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));

    /* Release guide, timeout → OFF */
    gamepad_report_t idle = make_idle_report();
    inject_bt_report(&idle);
    device_tick(31000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));

    /* Retry: press guide again → BOOTING */
    inject_bt_report(&guide);
    device_tick(35000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_EQUAL(2, s_hal.power_btn_trigger_count);

    /* This time USB mounts → ON */
    s_hal.power_led = true;
    device_tick(36000);
    s_hal.millis = 40000;
    inject_usb_mount();
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&s_sm));
}

/* ── Guide button edge detection ────────────────────────────────────── */

void test_guide_held_does_not_retrigger_wake(void)
{
    inject_bt_connect();
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);

    /* First tick: rising edge → WAKE_REQUESTED */
    device_tick(0);
    TEST_ASSERT_EQUAL(1, s_hal.power_btn_trigger_count);

    /* Release guide so we go back to OFF for next test of holding */
    gamepad_report_t idle = make_idle_report();
    inject_bt_report(&idle);
    device_tick(31000);  /* timeout → OFF */

    /* Press and hold guide */
    inject_bt_report(&guide);
    device_tick(35000);
    TEST_ASSERT_EQUAL(2, s_hal.power_btn_trigger_count);

    /* Continue holding guide — no new trigger */
    device_tick(36000);
    device_tick(37000);
    device_tick(38000);
    TEST_ASSERT_EQUAL(2, s_hal.power_btn_trigger_count);
}

void test_guide_release_and_repress_triggers_new_wake(void)
{
    inject_bt_connect();

    /* Press guide → BOOTING */
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);
    device_tick(0);
    TEST_ASSERT_EQUAL(1, s_hal.power_btn_trigger_count);

    /* Let it timeout → OFF */
    gamepad_report_t idle = make_idle_report();
    inject_bt_report(&idle);
    device_tick(31000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));

    /* Release guide */
    device_tick(32000);

    /* Re-press guide → new rising edge → WAKE_REQUESTED */
    inject_bt_report(&guide);
    device_tick(33000);
    TEST_ASSERT_EQUAL(2, s_hal.power_btn_trigger_count);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
}

/* ── BT disconnect behaviour ───────────────────────────────────────── */

void test_bt_disconnect_clears_prev_report(void)
{
    inject_bt_connect();

    /* Press guide → BOOTING */
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);
    device_tick(0);
    TEST_ASSERT_EQUAL(1, s_hal.power_btn_trigger_count);

    /* Let it timeout → OFF, guide still held */
    device_tick(31000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));

    /* Disconnect controller (clears prev_report_valid) */
    inject_bt_disconnect();

    /* Reconnect with guide already held */
    inject_bt_connect();
    s_bt.report = guide;

    /* Because prev_report was cleared, this is seen as a new rising edge */
    device_tick(35000);
    TEST_ASSERT_EQUAL(2, s_hal.power_btn_trigger_count);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
}

/* ── USB HID descriptor and report format ───────────────────────────── */

void test_hid_descriptor_structure(void)
{
    TEST_ASSERT_GREATER_THAN(0, USB_HID_REPORT_DESCRIPTOR_LEN);

    /* Usage Page (Generic Desktop): 0x05 0x01 */
    TEST_ASSERT_EQUAL_HEX8(0x05, usb_hid_report_descriptor[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, usb_hid_report_descriptor[1]);

    /* Ends with End Collection: 0xC0 */
    TEST_ASSERT_EQUAL_HEX8(0xC0,
        usb_hid_report_descriptor[USB_HID_REPORT_DESCRIPTOR_LEN - 1]);

    /* Wire report is exactly 13 bytes */
    TEST_ASSERT_EQUAL(13, sizeof(usb_gamepad_report_t));
}

void test_usb_report_all_dpad_directions(void)
{
    inject_bt_connect();
    drive_to_on(0);

    /* Internal 0-7 → USB 1-8, internal 8 → USB 0 */
    for (uint8_t dir = 0; dir <= 8; dir++) {
        gamepad_report_t in = make_idle_report();
        in.dpad = dir;
        inject_bt_report(&in);
        s_usb.report_sent = false;
        device_tick(6000 + dir * 100);

        TEST_ASSERT_TRUE(s_usb.report_sent);
        uint8_t expected = (dir == 8) ? 0 : (dir + 1);
        TEST_ASSERT_EQUAL_UINT8(expected, s_usb.last_report.hat);
    }
}

void test_usb_report_extreme_stick_values(void)
{
    inject_bt_connect();
    drive_to_on(0);

    gamepad_report_t in = {
        .lx = -32768, .ly = 32767,
        .rx = -32768, .ry = 32767,
        .lt = 0,      .rt = 0,
        .buttons = 0,
        .dpad = GAMEPAD_DPAD_CENTERED,
    };
    inject_bt_report(&in);
    s_usb.report_sent = false;
    device_tick(6000);

    TEST_ASSERT_TRUE(s_usb.report_sent);
    TEST_ASSERT_EQUAL_INT16(-32768, s_usb.last_report.lx);
    TEST_ASSERT_EQUAL_INT16(32767,  s_usb.last_report.ly);
    TEST_ASSERT_EQUAL_INT16(-32768, s_usb.last_report.rx);
    TEST_ASSERT_EQUAL_INT16(32767,  s_usb.last_report.ry);
}

void test_usb_report_all_buttons(void)
{
    inject_bt_connect();
    drive_to_on(0);

    uint16_t all_buttons = GAMEPAD_BTN_A | GAMEPAD_BTN_B | GAMEPAD_BTN_X |
                           GAMEPAD_BTN_Y | GAMEPAD_BTN_L1 | GAMEPAD_BTN_R1 |
                           GAMEPAD_BTN_L3 | GAMEPAD_BTN_R3 |
                           GAMEPAD_BTN_START | GAMEPAD_BTN_SELECT |
                           GAMEPAD_BTN_GUIDE | GAMEPAD_BTN_MISC;

    gamepad_report_t in = make_idle_report();
    in.buttons = all_buttons;
    inject_bt_report(&in);
    s_usb.report_sent = false;
    device_tick(6000);

    TEST_ASSERT_TRUE(s_usb.report_sent);
    TEST_ASSERT_EQUAL_UINT16(all_buttons, s_usb.last_report.buttons);
}

/* ── PC shutdown ────────────────────────────────────────────────────── */

void test_pc_shutdown_from_on_via_led_off(void)
{
    drive_to_on(0);

    /* Motherboard shuts down → LED goes off */
    s_hal.power_led = false;
    device_tick(20000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));
}

void test_pc_shutdown_from_sleep_via_usb_unmount(void)
{
    drive_to_sleeping(0);

    /* USB fully unmounts (power lost) */
    s_hal.millis = 20000;
    inject_usb_unmount();

    /* USB_SUSPENDED in SLEEPING is ignored */
    TEST_ASSERT_EQUAL(PC_STATE_SLEEPING, pc_power_sm_get_state(&s_sm));

    /* But LED off means full shutdown */
    s_hal.power_led = false;
    device_tick(21000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));
}

/* ── Full lifecycle ─────────────────────────────────────────────────── */

void test_full_lifecycle(void)
{
    inject_bt_connect();

    /* 1. OFF: controller connected, idle */
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));

    /* 2. Press guide → BOOTING */
    gamepad_report_t guide = make_guide_report();
    inject_bt_report(&guide);
    device_tick(1000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_EQUAL(1, s_hal.power_btn_trigger_count);

    /* 3. PC boots: LED on, USB mounts → ON */
    s_hal.power_led = true;
    device_tick(3000);
    s_hal.millis = 8000;
    inject_usb_mount();
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&s_sm));

    /* 4. Play: send gamepad input, verify forwarded */
    gamepad_report_t play = make_idle_report();
    play.buttons = GAMEPAD_BTN_X;
    play.lx = 10000;
    play.lt = 800;
    inject_bt_report(&play);
    s_usb.report_sent = false;
    device_tick(9000);
    TEST_ASSERT_TRUE(s_usb.report_sent);
    TEST_ASSERT_EQUAL_UINT16(GAMEPAD_BTN_X, s_usb.last_report.buttons);
    TEST_ASSERT_EQUAL_INT16(10000, s_usb.last_report.lx);
    TEST_ASSERT_EQUAL_UINT8(200, s_usb.last_report.lt);  /* 800 / 4 */

    /* 5. PC sleeps: USB suspends → SLEEPING */
    s_hal.millis = 60000;
    inject_usb_suspend();
    TEST_ASSERT_EQUAL(PC_STATE_SLEEPING, pc_power_sm_get_state(&s_sm));

    /* 6. Input NOT forwarded while sleeping */
    play.buttons = GAMEPAD_BTN_B;
    inject_bt_report(&play);
    s_usb.report_count = 0;
    device_tick(61000);
    TEST_ASSERT_EQUAL(0, s_usb.report_count);

    /* 7. Press guide → wake from sleep → BOOTING */
    inject_bt_report(&guide);
    device_tick(62000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&s_sm));
    TEST_ASSERT_EQUAL(2, s_hal.power_btn_trigger_count);

    /* 8. USB re-mounts → ON */
    s_hal.millis = 65000;
    inject_usb_mount();
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&s_sm));

    /* 9. Play again: input forwarded */
    play.buttons = GAMEPAD_BTN_A | GAMEPAD_BTN_B;
    inject_bt_report(&play);
    s_usb.report_sent = false;
    device_tick(66000);
    TEST_ASSERT_TRUE(s_usb.report_sent);
    TEST_ASSERT_EQUAL_UINT16(GAMEPAD_BTN_A | GAMEPAD_BTN_B,
                             s_usb.last_report.buttons);

    /* 10. Clean shutdown: LED off → OFF */
    s_hal.power_led = false;
    device_tick(120000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&s_sm));

    /* 11. No more input forwarded */
    play.buttons = GAMEPAD_BTN_Y;
    inject_bt_report(&play);
    s_usb.report_count = 0;
    device_tick(121000);
    TEST_ASSERT_EQUAL(0, s_usb.report_count);
}

/* ── Test runner ──────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* BT connect and wake from OFF */
    RUN_TEST(test_bt_connect_guide_press_wakes_pc_from_off);
    RUN_TEST(test_wake_from_off_triggers_power_button);
    RUN_TEST(test_boot_sequence_completes_to_on);

    /* Wake from sleep */
    RUN_TEST(test_wake_sleeping_pc_with_guide);
    RUN_TEST(test_wake_from_sleep_completes_to_on);

    /* Sleep with blinking LED */
    RUN_TEST(test_sleep_blinking_led_wake_with_guide);

    /* Gamepad input forwarding */
    RUN_TEST(test_input_forwarded_when_pc_on);
    RUN_TEST(test_full_report_conversion);
    RUN_TEST(test_input_not_forwarded_when_pc_off);
    RUN_TEST(test_input_not_forwarded_when_pc_booting);

    /* Boot timeout */
    RUN_TEST(test_boot_timeout_returns_to_off);
    RUN_TEST(test_boot_timeout_retry_succeeds);

    /* Guide button edge detection */
    RUN_TEST(test_guide_held_does_not_retrigger_wake);
    RUN_TEST(test_guide_release_and_repress_triggers_new_wake);

    /* BT disconnect behaviour */
    RUN_TEST(test_bt_disconnect_clears_prev_report);

    /* USB HID descriptor and report format */
    RUN_TEST(test_hid_descriptor_structure);
    RUN_TEST(test_usb_report_all_dpad_directions);
    RUN_TEST(test_usb_report_extreme_stick_values);
    RUN_TEST(test_usb_report_all_buttons);

    /* PC shutdown */
    RUN_TEST(test_pc_shutdown_from_on_via_led_off);
    RUN_TEST(test_pc_shutdown_from_sleep_via_usb_unmount);

    /* Full lifecycle */
    RUN_TEST(test_full_lifecycle);

    return UNITY_END();
}
