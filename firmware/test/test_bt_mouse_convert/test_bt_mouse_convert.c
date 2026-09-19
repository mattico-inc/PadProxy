#include "unity.h"
#include "bt_mouse_convert.h"

void setUp(void) {}
void tearDown(void) {}

/* ── bt_mouse_clamp_delta ────────────────────────────────────────────── */

void test_clamp_delta_zero(void)
{
    TEST_ASSERT_EQUAL_INT8(0, bt_mouse_clamp_delta(0));
}

void test_clamp_delta_positive_in_range(void)
{
    TEST_ASSERT_EQUAL_INT8(50, bt_mouse_clamp_delta(50));
}

void test_clamp_delta_negative_in_range(void)
{
    TEST_ASSERT_EQUAL_INT8(-50, bt_mouse_clamp_delta(-50));
}

void test_clamp_delta_max_in_range(void)
{
    TEST_ASSERT_EQUAL_INT8(127, bt_mouse_clamp_delta(127));
}

void test_clamp_delta_min_in_range(void)
{
    TEST_ASSERT_EQUAL_INT8(-127, bt_mouse_clamp_delta(-127));
}

void test_clamp_delta_clamps_high(void)
{
    TEST_ASSERT_EQUAL_INT8(127, bt_mouse_clamp_delta(128));
}

void test_clamp_delta_clamps_high_large(void)
{
    TEST_ASSERT_EQUAL_INT8(127, bt_mouse_clamp_delta(100000));
}

void test_clamp_delta_clamps_low(void)
{
    TEST_ASSERT_EQUAL_INT8(-127, bt_mouse_clamp_delta(-128));
}

void test_clamp_delta_clamps_low_large(void)
{
    TEST_ASSERT_EQUAL_INT8(-127, bt_mouse_clamp_delta(-100000));
}

/* ── bt_mouse_map_buttons ────────────────────────────────────────────── */

void test_map_buttons_none(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, bt_mouse_map_buttons(0));
}

void test_map_buttons_left(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x01, bt_mouse_map_buttons(0x01));
}

void test_map_buttons_right(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x02, bt_mouse_map_buttons(0x02));
}

void test_map_buttons_middle(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x04, bt_mouse_map_buttons(0x04));
}

void test_map_buttons_backward_forward(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x18, bt_mouse_map_buttons(0x18));
}

void test_map_buttons_all_low_five(void)
{
    TEST_ASSERT_EQUAL_UINT8(0x1F, bt_mouse_map_buttons(0x1F));
}

void test_map_buttons_ignores_bits_above_five(void)
{
    /* Bits above the 5 defined mouse buttons must be masked off. */
    TEST_ASSERT_EQUAL_UINT8(0x03, bt_mouse_map_buttons(0xFFE3));
}

/* ── bt_mouse_convert_report ─────────────────────────────────────────── */

void test_convert_report_idle(void)
{
    usb_mouse_report_t out;
    bt_mouse_convert_report(0, 0, 0, 0, &out);

    TEST_ASSERT_EQUAL_UINT8(0, out.buttons);
    TEST_ASSERT_EQUAL_INT8(0, out.x);
    TEST_ASSERT_EQUAL_INT8(0, out.y);
    TEST_ASSERT_EQUAL_INT8(0, out.wheel);
    TEST_ASSERT_EQUAL_INT8(0, out.pan);
}

void test_convert_report_motion_and_buttons(void)
{
    usb_mouse_report_t out;
    bt_mouse_convert_report(10, -20, 0x03, 5, &out);

    TEST_ASSERT_EQUAL_UINT8(0x03, out.buttons);
    TEST_ASSERT_EQUAL_INT8(10, out.x);
    TEST_ASSERT_EQUAL_INT8(-20, out.y);
    TEST_ASSERT_EQUAL_INT8(5, out.wheel);
    TEST_ASSERT_EQUAL_INT8(0, out.pan);
}

void test_convert_report_clamps_large_motion(void)
{
    usb_mouse_report_t out;
    bt_mouse_convert_report(9999, -9999, 0, -9999, &out);

    TEST_ASSERT_EQUAL_INT8(127, out.x);
    TEST_ASSERT_EQUAL_INT8(-127, out.y);
    TEST_ASSERT_EQUAL_INT8(-127, out.wheel);
}

/* ── Test runner ─────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_clamp_delta_zero);
    RUN_TEST(test_clamp_delta_positive_in_range);
    RUN_TEST(test_clamp_delta_negative_in_range);
    RUN_TEST(test_clamp_delta_max_in_range);
    RUN_TEST(test_clamp_delta_min_in_range);
    RUN_TEST(test_clamp_delta_clamps_high);
    RUN_TEST(test_clamp_delta_clamps_high_large);
    RUN_TEST(test_clamp_delta_clamps_low);
    RUN_TEST(test_clamp_delta_clamps_low_large);

    RUN_TEST(test_map_buttons_none);
    RUN_TEST(test_map_buttons_left);
    RUN_TEST(test_map_buttons_right);
    RUN_TEST(test_map_buttons_middle);
    RUN_TEST(test_map_buttons_backward_forward);
    RUN_TEST(test_map_buttons_all_low_five);
    RUN_TEST(test_map_buttons_ignores_bits_above_five);

    RUN_TEST(test_convert_report_idle);
    RUN_TEST(test_convert_report_motion_and_buttons);
    RUN_TEST(test_convert_report_clamps_large_motion);

    return UNITY_END();
}
