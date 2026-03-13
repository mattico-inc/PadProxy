#include "unity.h"
#include "bt_gamepad_convert.h"

void setUp(void) {}
void tearDown(void) {}

/* ── bt_gamepad_scale_axis ───────────────────────────────────────────── */

void test_scale_axis_zero(void)
{
    TEST_ASSERT_EQUAL_INT16(0, bt_gamepad_scale_axis(0));
}

void test_scale_axis_positive_mid(void)
{
    /* 256 * 64 = 16384 */
    TEST_ASSERT_EQUAL_INT16(16384, bt_gamepad_scale_axis(256));
}

void test_scale_axis_negative_mid(void)
{
    /* -256 * 64 = -16384 */
    TEST_ASSERT_EQUAL_INT16(-16384, bt_gamepad_scale_axis(-256));
}

void test_scale_axis_max_in_range(void)
{
    /* 511 * 64 = 32704 — within int16 range, no clamping */
    TEST_ASSERT_EQUAL_INT16(32704, bt_gamepad_scale_axis(511));
}

void test_scale_axis_min_in_range(void)
{
    /* -512 * 64 = -32768 — exactly INT16_MIN */
    TEST_ASSERT_EQUAL_INT16(-32768, bt_gamepad_scale_axis(-512));
}

void test_scale_axis_clamps_high(void)
{
    /* 512 * 64 = 32768 — overflows int16, clamped to 32767 */
    TEST_ASSERT_EQUAL_INT16(32767, bt_gamepad_scale_axis(512));
}

void test_scale_axis_clamps_high_large(void)
{
    TEST_ASSERT_EQUAL_INT16(32767, bt_gamepad_scale_axis(10000));
}

void test_scale_axis_clamps_low(void)
{
    /* -513 * 64 = -32832 — below INT16_MIN, clamped to -32768 */
    TEST_ASSERT_EQUAL_INT16(-32768, bt_gamepad_scale_axis(-513));
}

void test_scale_axis_clamps_low_large(void)
{
    TEST_ASSERT_EQUAL_INT16(-32768, bt_gamepad_scale_axis(-10000));
}

void test_scale_axis_one(void)
{
    TEST_ASSERT_EQUAL_INT16(64, bt_gamepad_scale_axis(1));
}

void test_scale_axis_minus_one(void)
{
    TEST_ASSERT_EQUAL_INT16(-64, bt_gamepad_scale_axis(-1));
}

/* ── bt_gamepad_clamp_trigger ────────────────────────────────────────── */

void test_clamp_trigger_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, bt_gamepad_clamp_trigger(0));
}

void test_clamp_trigger_max(void)
{
    TEST_ASSERT_EQUAL_UINT16(1023, bt_gamepad_clamp_trigger(1023));
}

void test_clamp_trigger_midpoint(void)
{
    TEST_ASSERT_EQUAL_UINT16(512, bt_gamepad_clamp_trigger(512));
}

void test_clamp_trigger_negative_clamped(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, bt_gamepad_clamp_trigger(-1));
}

void test_clamp_trigger_large_negative_clamped(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, bt_gamepad_clamp_trigger(-32768));
}

void test_clamp_trigger_one_over_max_clamped(void)
{
    TEST_ASSERT_EQUAL_UINT16(1023, bt_gamepad_clamp_trigger(1024));
}

void test_clamp_trigger_large_over_max_clamped(void)
{
    TEST_ASSERT_EQUAL_UINT16(1023, bt_gamepad_clamp_trigger(65535));
}

void test_clamp_trigger_one(void)
{
    TEST_ASSERT_EQUAL_UINT16(1, bt_gamepad_clamp_trigger(1));
}

void test_clamp_trigger_one_below_max(void)
{
    TEST_ASSERT_EQUAL_UINT16(1022, bt_gamepad_clamp_trigger(1022));
}

/* ── Test runner ─────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* scale_axis */
    RUN_TEST(test_scale_axis_zero);
    RUN_TEST(test_scale_axis_positive_mid);
    RUN_TEST(test_scale_axis_negative_mid);
    RUN_TEST(test_scale_axis_max_in_range);
    RUN_TEST(test_scale_axis_min_in_range);
    RUN_TEST(test_scale_axis_clamps_high);
    RUN_TEST(test_scale_axis_clamps_high_large);
    RUN_TEST(test_scale_axis_clamps_low);
    RUN_TEST(test_scale_axis_clamps_low_large);
    RUN_TEST(test_scale_axis_one);
    RUN_TEST(test_scale_axis_minus_one);

    /* clamp_trigger */
    RUN_TEST(test_clamp_trigger_zero);
    RUN_TEST(test_clamp_trigger_max);
    RUN_TEST(test_clamp_trigger_midpoint);
    RUN_TEST(test_clamp_trigger_negative_clamped);
    RUN_TEST(test_clamp_trigger_large_negative_clamped);
    RUN_TEST(test_clamp_trigger_one_over_max_clamped);
    RUN_TEST(test_clamp_trigger_large_over_max_clamped);
    RUN_TEST(test_clamp_trigger_one);
    RUN_TEST(test_clamp_trigger_one_below_max);

    return UNITY_END();
}
