#include "unity.h"
#include "bt_keyboard_convert.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── bt_keyboard_map_modifiers ───────────────────────────────────────── */

void test_map_modifiers_none(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, bt_keyboard_map_modifiers(0));
}

void test_map_modifiers_identity(void)
{
    /* Bluepad32's modifier bit layout matches USB HID exactly. */
    TEST_ASSERT_EQUAL_UINT8(0x01, bt_keyboard_map_modifiers(0x01));
    TEST_ASSERT_EQUAL_UINT8(0xFF, bt_keyboard_map_modifiers(0xFF));
}

/* ── bt_keyboard_convert_report ──────────────────────────────────────── */

void test_convert_report_no_keys(void)
{
    usb_keyboard_report_t out;
    memset(&out, 0xFF, sizeof(out)); /* poison */

    bt_keyboard_convert_report(0, NULL, 0, &out);

    TEST_ASSERT_EQUAL_UINT8(0, out.modifier);
    TEST_ASSERT_EQUAL_UINT8(0, out.reserved);
    for (int i = 0; i < USB_KEYBOARD_MAX_KEYS; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, out.keycode[i]);
    }
}

void test_convert_report_modifier_passthrough(void)
{
    usb_keyboard_report_t out;
    bt_keyboard_convert_report(0x03, NULL, 0, &out);
    TEST_ASSERT_EQUAL_UINT8(0x03, out.modifier);
}

void test_convert_report_single_key(void)
{
    uint8_t keys[] = { 0x04 }; /* HID_USAGE_KB_A */
    usb_keyboard_report_t out;

    bt_keyboard_convert_report(0, keys, 1, &out);

    TEST_ASSERT_EQUAL_UINT8(0x04, out.keycode[0]);
    for (int i = 1; i < USB_KEYBOARD_MAX_KEYS; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, out.keycode[i]);
    }
}

void test_convert_report_exactly_max_keys(void)
{
    uint8_t keys[USB_KEYBOARD_MAX_KEYS] = { 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };
    usb_keyboard_report_t out;

    bt_keyboard_convert_report(0, keys, USB_KEYBOARD_MAX_KEYS, &out);

    for (int i = 0; i < USB_KEYBOARD_MAX_KEYS; i++) {
        TEST_ASSERT_EQUAL_UINT8(keys[i], out.keycode[i]);
    }
}

void test_convert_report_truncates_beyond_max_keys(void)
{
    /* Bluepad32 can report up to 10 simultaneous keys; the boot report
     * only has room for 6 (6-key rollover), so extras must be dropped
     * rather than overflow the output buffer. */
    uint8_t keys[10] = { 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D };
    usb_keyboard_report_t out;

    bt_keyboard_convert_report(0, keys, 10, &out);

    for (int i = 0; i < USB_KEYBOARD_MAX_KEYS; i++) {
        TEST_ASSERT_EQUAL_UINT8(keys[i], out.keycode[i]);
    }
}

void test_convert_report_reserved_always_zero(void)
{
    uint8_t keys[] = { 0x04 };
    usb_keyboard_report_t out;
    memset(&out, 0xFF, sizeof(out));

    bt_keyboard_convert_report(0xFF, keys, 1, &out);

    TEST_ASSERT_EQUAL_UINT8(0, out.reserved);
}

void test_convert_report_null_pressed_keys_with_nonzero_count(void)
{
    /* Defensive: a NULL array with a nonzero count must not crash. */
    usb_keyboard_report_t out;
    bt_keyboard_convert_report(0x02, NULL, 3, &out);

    TEST_ASSERT_EQUAL_UINT8(0x02, out.modifier);
    for (int i = 0; i < USB_KEYBOARD_MAX_KEYS; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, out.keycode[i]);
    }
}

/* ── Test runner ─────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_map_modifiers_none);
    RUN_TEST(test_map_modifiers_identity);

    RUN_TEST(test_convert_report_no_keys);
    RUN_TEST(test_convert_report_modifier_passthrough);
    RUN_TEST(test_convert_report_single_key);
    RUN_TEST(test_convert_report_exactly_max_keys);
    RUN_TEST(test_convert_report_truncates_beyond_max_keys);
    RUN_TEST(test_convert_report_reserved_always_zero);
    RUN_TEST(test_convert_report_null_pressed_keys_with_nonzero_count);

    return UNITY_END();
}
