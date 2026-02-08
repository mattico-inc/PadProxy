#include "unity.h"
#include "pc_power_state.h"

static pc_power_sm_t sm;

void setUp(void)
{
    pc_power_sm_init(&sm);
}

void tearDown(void)
{
}

/* ── Initialization ───────────────────────────────────────────────────── */

void test_init_state_is_off(void)
{
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&sm));
}

void test_init_timestamp_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, sm.last_transition_ms);
}

/* ── OFF state transitions ────────────────────────────────────────────── */

void test_off_wake_requested_transitions_to_booting(void)
{
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 1000);

    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&sm));
}

void test_off_wake_requested_triggers_power_and_timer(void)
{
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 1000);

    TEST_ASSERT_BITS(PC_ACTION_TRIGGER_POWER, PC_ACTION_TRIGGER_POWER, r.actions);
    TEST_ASSERT_BITS(PC_ACTION_START_BOOT_TIMER, PC_ACTION_START_BOOT_TIMER, r.actions);
}

void test_off_button_pressed_transitions_to_booting(void)
{
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, 500);

    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_off_button_pressed_starts_timer_no_trigger(void)
{
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, 500);

    /* Physical button passes through hardware; no need to trigger optocoupler */
    TEST_ASSERT_BITS_LOW(PC_ACTION_TRIGGER_POWER, r.actions);
    TEST_ASSERT_BITS(PC_ACTION_START_BOOT_TIMER, PC_ACTION_START_BOOT_TIMER, r.actions);
}

void test_off_power_led_on_transitions_to_booting(void)
{
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_POWER_LED_ON, 200);

    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_off_usb_enumerated_jumps_to_on(void)
{
    /* Missed the boot; USB appeared while we thought PC was off */
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 300);

    TEST_ASSERT_EQUAL(PC_STATE_ON, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_off_ignores_irrelevant_events(void)
{
    pc_power_event_t ignore[] = {
        PC_EVENT_USB_SUSPENDED,
        PC_EVENT_POWER_LED_OFF,
        PC_EVENT_BOOT_TIMEOUT,
        PC_EVENT_BUTTON_LONG_PRESSED,
    };
    for (int i = 0; i < (int)(sizeof(ignore) / sizeof(ignore[0])); i++) {
        pc_power_sm_init(&sm);
        pc_power_result_t r = pc_power_sm_process(&sm, ignore[i], 100);
        TEST_ASSERT_FALSE_MESSAGE(r.transitioned, pc_power_event_name(ignore[i]));
        TEST_ASSERT_EQUAL_MESSAGE(PC_STATE_OFF, r.new_state, pc_power_event_name(ignore[i]));
    }
}

/* ── BOOTING state transitions ────────────────────────────────────────── */

static void enter_booting(uint32_t at_ms)
{
    pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, at_ms);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&sm));
}

void test_booting_usb_enumerated_transitions_to_on(void)
{
    enter_booting(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 5000);

    TEST_ASSERT_EQUAL(PC_STATE_ON, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_booting_usb_enumerated_cancels_timer(void)
{
    enter_booting(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 5000);

    TEST_ASSERT_BITS(PC_ACTION_CANCEL_BOOT_TIMER, PC_ACTION_CANCEL_BOOT_TIMER, r.actions);
}

void test_booting_power_led_off_returns_to_off(void)
{
    enter_booting(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_POWER_LED_OFF, 2000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
    TEST_ASSERT_BITS(PC_ACTION_CANCEL_BOOT_TIMER, PC_ACTION_CANCEL_BOOT_TIMER, r.actions);
}

void test_booting_timeout_returns_to_off(void)
{
    enter_booting(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BOOT_TIMEOUT, 31000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_booting_long_press_returns_to_off(void)
{
    enter_booting(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_LONG_PRESSED, 5000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
    TEST_ASSERT_BITS(PC_ACTION_CANCEL_BOOT_TIMER, PC_ACTION_CANCEL_BOOT_TIMER, r.actions);
}

void test_booting_ignores_irrelevant_events(void)
{
    pc_power_event_t ignore[] = {
        PC_EVENT_WAKE_REQUESTED,
        PC_EVENT_BUTTON_PRESSED,
        PC_EVENT_USB_SUSPENDED,
        PC_EVENT_POWER_LED_ON,
    };
    for (int i = 0; i < (int)(sizeof(ignore) / sizeof(ignore[0])); i++) {
        pc_power_sm_init(&sm);
        enter_booting(100);
        pc_power_result_t r = pc_power_sm_process(&sm, ignore[i], 200);
        TEST_ASSERT_FALSE_MESSAGE(r.transitioned, pc_power_event_name(ignore[i]));
        TEST_ASSERT_EQUAL_MESSAGE(PC_STATE_BOOTING, r.new_state, pc_power_event_name(ignore[i]));
    }
}

/* ── ON state transitions ─────────────────────────────────────────────── */

static void enter_on(uint32_t at_ms)
{
    pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, at_ms);
    pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, at_ms + 5000);
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&sm));
}

void test_on_usb_suspended_transitions_to_sleeping(void)
{
    enter_on(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_USB_SUSPENDED, 10000);

    TEST_ASSERT_EQUAL(PC_STATE_SLEEPING, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_on_power_led_off_transitions_to_off(void)
{
    enter_on(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_POWER_LED_OFF, 10000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_on_long_press_transitions_to_off(void)
{
    enter_on(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_LONG_PRESSED, 10000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_on_ignores_irrelevant_events(void)
{
    pc_power_event_t ignore[] = {
        PC_EVENT_WAKE_REQUESTED,
        PC_EVENT_BUTTON_PRESSED,
        PC_EVENT_USB_ENUMERATED,
        PC_EVENT_POWER_LED_ON,
        PC_EVENT_BOOT_TIMEOUT,
    };
    for (int i = 0; i < (int)(sizeof(ignore) / sizeof(ignore[0])); i++) {
        pc_power_sm_init(&sm);
        enter_on(100);
        pc_power_result_t r = pc_power_sm_process(&sm, ignore[i], 20000);
        TEST_ASSERT_FALSE_MESSAGE(r.transitioned, pc_power_event_name(ignore[i]));
        TEST_ASSERT_EQUAL_MESSAGE(PC_STATE_ON, r.new_state, pc_power_event_name(ignore[i]));
    }
}

/* ── SLEEPING state transitions ───────────────────────────────────────── */

static void enter_sleeping(uint32_t at_ms)
{
    enter_on(at_ms);
    pc_power_sm_process(&sm, PC_EVENT_USB_SUSPENDED, at_ms + 10000);
    TEST_ASSERT_EQUAL(PC_STATE_SLEEPING, pc_power_sm_get_state(&sm));
}

void test_sleeping_wake_requested_transitions_to_booting(void)
{
    enter_sleeping(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 20000);

    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_sleeping_wake_requested_triggers_power_and_timer(void)
{
    enter_sleeping(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 20000);

    TEST_ASSERT_BITS(PC_ACTION_TRIGGER_POWER, PC_ACTION_TRIGGER_POWER, r.actions);
    TEST_ASSERT_BITS(PC_ACTION_START_BOOT_TIMER, PC_ACTION_START_BOOT_TIMER, r.actions);
}

void test_sleeping_button_pressed_transitions_to_booting(void)
{
    enter_sleeping(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, 20000);

    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
    /* Physical button - no trigger needed, but timer starts */
    TEST_ASSERT_BITS_LOW(PC_ACTION_TRIGGER_POWER, r.actions);
    TEST_ASSERT_BITS(PC_ACTION_START_BOOT_TIMER, PC_ACTION_START_BOOT_TIMER, r.actions);
}

void test_sleeping_usb_enumerated_transitions_to_on(void)
{
    enter_sleeping(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 20000);

    TEST_ASSERT_EQUAL(PC_STATE_ON, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_sleeping_power_led_on_transitions_to_booting(void)
{
    enter_sleeping(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_POWER_LED_ON, 20000);

    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_sleeping_power_led_off_transitions_to_off(void)
{
    enter_sleeping(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_POWER_LED_OFF, 20000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_sleeping_long_press_transitions_to_off(void)
{
    enter_sleeping(1000);
    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_LONG_PRESSED, 20000);

    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_sleeping_ignores_irrelevant_events(void)
{
    pc_power_event_t ignore[] = {
        PC_EVENT_USB_SUSPENDED,
        PC_EVENT_BOOT_TIMEOUT,
    };
    for (int i = 0; i < (int)(sizeof(ignore) / sizeof(ignore[0])); i++) {
        pc_power_sm_init(&sm);
        enter_sleeping(100);
        pc_power_result_t r = pc_power_sm_process(&sm, ignore[i], 30000);
        TEST_ASSERT_FALSE_MESSAGE(r.transitioned, pc_power_event_name(ignore[i]));
        TEST_ASSERT_EQUAL_MESSAGE(PC_STATE_SLEEPING, r.new_state, pc_power_event_name(ignore[i]));
    }
}

/* ── Timestamp tracking ───────────────────────────────────────────────── */

void test_transition_updates_timestamp(void)
{
    pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, 1234);
    TEST_ASSERT_EQUAL_UINT32(1234, sm.last_transition_ms);

    pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 5678);
    TEST_ASSERT_EQUAL_UINT32(5678, sm.last_transition_ms);
}

void test_no_transition_preserves_timestamp(void)
{
    pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, 1000);
    TEST_ASSERT_EQUAL_UINT32(1000, sm.last_transition_ms);

    /* BOOT_TIMEOUT is ignored in BOOTING... wait, it's not - it transitions.
     * Use USB_SUSPENDED which IS ignored in BOOTING. */
    pc_power_sm_process(&sm, PC_EVENT_USB_SUSPENDED, 9999);
    TEST_ASSERT_EQUAL_UINT32(1000, sm.last_transition_ms);
}

/* ── Full lifecycle sequences ─────────────────────────────────────────── */

void test_full_cycle_off_boot_on_sleep_off(void)
{
    /* OFF -> BOOTING (controller wake) */
    pc_power_result_t r;
    r = pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 0);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);

    /* BOOTING -> ON (USB detected) */
    r = pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 5000);
    TEST_ASSERT_EQUAL(PC_STATE_ON, r.new_state);

    /* ON -> SLEEPING (USB suspended) */
    r = pc_power_sm_process(&sm, PC_EVENT_USB_SUSPENDED, 60000);
    TEST_ASSERT_EQUAL(PC_STATE_SLEEPING, r.new_state);

    /* SLEEPING -> OFF (LED off = full shutdown from sleep) */
    r = pc_power_sm_process(&sm, PC_EVENT_POWER_LED_OFF, 60500);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
}

void test_full_cycle_off_boot_on_shutdown(void)
{
    /* OFF -> BOOTING */
    pc_power_sm_process(&sm, PC_EVENT_BUTTON_PRESSED, 0);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&sm));

    /* BOOTING -> ON */
    pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 8000);
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&sm));

    /* ON -> OFF (clean shutdown, LED goes off) */
    pc_power_sm_process(&sm, PC_EVENT_POWER_LED_OFF, 120000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&sm));
}

void test_full_cycle_sleep_wake_cycle(void)
{
    enter_sleeping(0);

    /* SLEEPING -> BOOTING (controller wake) */
    pc_power_result_t r;
    r = pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 20000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, r.new_state);
    TEST_ASSERT_BITS(PC_ACTION_TRIGGER_POWER, PC_ACTION_TRIGGER_POWER, r.actions);

    /* BOOTING -> ON */
    r = pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 23000);
    TEST_ASSERT_EQUAL(PC_STATE_ON, r.new_state);

    /* ON -> SLEEPING again */
    r = pc_power_sm_process(&sm, PC_EVENT_USB_SUSPENDED, 80000);
    TEST_ASSERT_EQUAL(PC_STATE_SLEEPING, r.new_state);

    /* SLEEPING -> ON (wake from keyboard, USB re-enumerates directly) */
    r = pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 85000);
    TEST_ASSERT_EQUAL(PC_STATE_ON, r.new_state);
}

void test_boot_failure_timeout_then_retry(void)
{
    /* First attempt: OFF -> BOOTING -> timeout -> OFF */
    pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 0);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&sm));

    pc_power_sm_process(&sm, PC_EVENT_BOOT_TIMEOUT, 30000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, pc_power_sm_get_state(&sm));

    /* Retry: OFF -> BOOTING -> ON (success this time) */
    pc_power_sm_process(&sm, PC_EVENT_WAKE_REQUESTED, 35000);
    TEST_ASSERT_EQUAL(PC_STATE_BOOTING, pc_power_sm_get_state(&sm));

    pc_power_sm_process(&sm, PC_EVENT_USB_ENUMERATED, 42000);
    TEST_ASSERT_EQUAL(PC_STATE_ON, pc_power_sm_get_state(&sm));
}

void test_force_shutdown_from_on(void)
{
    enter_on(0);

    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_LONG_PRESSED, 20000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

void test_force_shutdown_from_sleeping(void)
{
    enter_sleeping(0);

    pc_power_result_t r = pc_power_sm_process(&sm, PC_EVENT_BUTTON_LONG_PRESSED, 30000);
    TEST_ASSERT_EQUAL(PC_STATE_OFF, r.new_state);
    TEST_ASSERT_TRUE(r.transitioned);
}

/* ── Name helpers ─────────────────────────────────────────────────────── */

void test_state_names(void)
{
    TEST_ASSERT_EQUAL_STRING("PC_OFF",      pc_power_state_name(PC_STATE_OFF));
    TEST_ASSERT_EQUAL_STRING("PC_BOOTING",  pc_power_state_name(PC_STATE_BOOTING));
    TEST_ASSERT_EQUAL_STRING("PC_ON",       pc_power_state_name(PC_STATE_ON));
    TEST_ASSERT_EQUAL_STRING("PC_SLEEPING", pc_power_state_name(PC_STATE_SLEEPING));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN",     pc_power_state_name(PC_STATE_COUNT));
}

void test_event_names(void)
{
    TEST_ASSERT_EQUAL_STRING("WAKE_REQUESTED",     pc_power_event_name(PC_EVENT_WAKE_REQUESTED));
    TEST_ASSERT_EQUAL_STRING("BUTTON_PRESSED",     pc_power_event_name(PC_EVENT_BUTTON_PRESSED));
    TEST_ASSERT_EQUAL_STRING("BUTTON_LONG_PRESSED", pc_power_event_name(PC_EVENT_BUTTON_LONG_PRESSED));
    TEST_ASSERT_EQUAL_STRING("USB_ENUMERATED",     pc_power_event_name(PC_EVENT_USB_ENUMERATED));
    TEST_ASSERT_EQUAL_STRING("USB_SUSPENDED",      pc_power_event_name(PC_EVENT_USB_SUSPENDED));
    TEST_ASSERT_EQUAL_STRING("POWER_LED_ON",       pc_power_event_name(PC_EVENT_POWER_LED_ON));
    TEST_ASSERT_EQUAL_STRING("POWER_LED_OFF",      pc_power_event_name(PC_EVENT_POWER_LED_OFF));
    TEST_ASSERT_EQUAL_STRING("BOOT_TIMEOUT",       pc_power_event_name(PC_EVENT_BOOT_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN",            pc_power_event_name(PC_EVENT_COUNT));
}

/* ── Test runner ──────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Initialization */
    RUN_TEST(test_init_state_is_off);
    RUN_TEST(test_init_timestamp_is_zero);

    /* OFF transitions */
    RUN_TEST(test_off_wake_requested_transitions_to_booting);
    RUN_TEST(test_off_wake_requested_triggers_power_and_timer);
    RUN_TEST(test_off_button_pressed_transitions_to_booting);
    RUN_TEST(test_off_button_pressed_starts_timer_no_trigger);
    RUN_TEST(test_off_power_led_on_transitions_to_booting);
    RUN_TEST(test_off_usb_enumerated_jumps_to_on);
    RUN_TEST(test_off_ignores_irrelevant_events);

    /* BOOTING transitions */
    RUN_TEST(test_booting_usb_enumerated_transitions_to_on);
    RUN_TEST(test_booting_usb_enumerated_cancels_timer);
    RUN_TEST(test_booting_power_led_off_returns_to_off);
    RUN_TEST(test_booting_timeout_returns_to_off);
    RUN_TEST(test_booting_long_press_returns_to_off);
    RUN_TEST(test_booting_ignores_irrelevant_events);

    /* ON transitions */
    RUN_TEST(test_on_usb_suspended_transitions_to_sleeping);
    RUN_TEST(test_on_power_led_off_transitions_to_off);
    RUN_TEST(test_on_long_press_transitions_to_off);
    RUN_TEST(test_on_ignores_irrelevant_events);

    /* SLEEPING transitions */
    RUN_TEST(test_sleeping_wake_requested_transitions_to_booting);
    RUN_TEST(test_sleeping_wake_requested_triggers_power_and_timer);
    RUN_TEST(test_sleeping_button_pressed_transitions_to_booting);
    RUN_TEST(test_sleeping_usb_enumerated_transitions_to_on);
    RUN_TEST(test_sleeping_power_led_on_transitions_to_booting);
    RUN_TEST(test_sleeping_power_led_off_transitions_to_off);
    RUN_TEST(test_sleeping_long_press_transitions_to_off);
    RUN_TEST(test_sleeping_ignores_irrelevant_events);

    /* Timestamp tracking */
    RUN_TEST(test_transition_updates_timestamp);
    RUN_TEST(test_no_transition_preserves_timestamp);

    /* Full lifecycle sequences */
    RUN_TEST(test_full_cycle_off_boot_on_sleep_off);
    RUN_TEST(test_full_cycle_off_boot_on_shutdown);
    RUN_TEST(test_full_cycle_sleep_wake_cycle);
    RUN_TEST(test_boot_failure_timeout_then_retry);
    RUN_TEST(test_force_shutdown_from_on);
    RUN_TEST(test_force_shutdown_from_sleeping);

    /* Name helpers */
    RUN_TEST(test_state_names);
    RUN_TEST(test_event_names);

    return UNITY_END();
}
