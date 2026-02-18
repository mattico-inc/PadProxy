#include "unity.h"
#include "gamepad.h"
#include "usb_hid_report.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── D-pad bitmask → hat conversion ──────────────────────────────────── */

void test_dpad_no_input_is_centered(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_CENTERED, gamepad_dpad_to_hat(0));
}

void test_dpad_up(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_UP, gamepad_dpad_to_hat(1));
}

void test_dpad_down(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_DOWN, gamepad_dpad_to_hat(2));
}

void test_dpad_right(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_RIGHT, gamepad_dpad_to_hat(4));
}

void test_dpad_left(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_LEFT, gamepad_dpad_to_hat(8));
}

void test_dpad_up_right(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_UP_RIGHT, gamepad_dpad_to_hat(1 | 4));
}

void test_dpad_down_right(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_DOWN_RIGHT, gamepad_dpad_to_hat(2 | 4));
}

void test_dpad_down_left(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_DOWN_LEFT, gamepad_dpad_to_hat(2 | 8));
}

void test_dpad_up_left(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_UP_LEFT, gamepad_dpad_to_hat(1 | 8));
}

void test_dpad_invalid_up_down_is_centered(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_CENTERED, gamepad_dpad_to_hat(1 | 2));
}

void test_dpad_invalid_left_right_is_centered(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_CENTERED, gamepad_dpad_to_hat(4 | 8));
}

void test_dpad_invalid_all_is_centered(void)
{
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_CENTERED, gamepad_dpad_to_hat(0x0F));
}

void test_dpad_masks_upper_nibble(void)
{
    /* Only the low 4 bits should be used. */
    TEST_ASSERT_EQUAL(GAMEPAD_DPAD_UP, gamepad_dpad_to_hat(0xF1));
}

/* ── Guide button helper ─────────────────────────────────────────────── */

void test_guide_pressed_when_set(void)
{
    gamepad_report_t r = {0};
    r.dpad = GAMEPAD_DPAD_CENTERED;
    r.buttons = GAMEPAD_BTN_GUIDE;
    TEST_ASSERT_TRUE(gamepad_report_guide_pressed(&r));
}

void test_guide_pressed_when_clear(void)
{
    gamepad_report_t r = {0};
    r.dpad = GAMEPAD_DPAD_CENTERED;
    r.buttons = GAMEPAD_BTN_A | GAMEPAD_BTN_START;
    TEST_ASSERT_FALSE(gamepad_report_guide_pressed(&r));
}

void test_guide_pressed_zero_buttons(void)
{
    gamepad_report_t r = {0};
    r.dpad = GAMEPAD_DPAD_CENTERED;
    TEST_ASSERT_FALSE(gamepad_report_guide_pressed(&r));
}

/* ── USB HID report conversion ───────────────────────────────────────── */

void test_hid_report_sticks_pass_through(void)
{
    gamepad_report_t in = {
        .lx = -32768, .ly = 32767,
        .rx = -1,     .ry = 0,
        .dpad = GAMEPAD_DPAD_CENTERED,
    };
    usb_gamepad_report_t out;
    usb_hid_report_from_gamepad(&in, &out);

    TEST_ASSERT_EQUAL_INT16(-32768, out.lx);
    TEST_ASSERT_EQUAL_INT16(32767,  out.ly);
    TEST_ASSERT_EQUAL_INT16(-1,     out.rx);
    TEST_ASSERT_EQUAL_INT16(0,      out.ry);
}

void test_hid_report_triggers_scale(void)
{
    gamepad_report_t in = {0};
    in.dpad = GAMEPAD_DPAD_CENTERED;
    in.lt = 0;
    in.rt = 1023;

    usb_gamepad_report_t out;
    usb_hid_report_from_gamepad(&in, &out);

    TEST_ASSERT_EQUAL_UINT8(0,   out.lt);
    TEST_ASSERT_EQUAL_UINT8(255, out.rt);
}

void test_hid_report_triggers_midpoint(void)
{
    gamepad_report_t in = {0};
    in.dpad = GAMEPAD_DPAD_CENTERED;
    in.lt = 512;
    in.rt = 256;

    usb_gamepad_report_t out;
    usb_hid_report_from_gamepad(&in, &out);

    TEST_ASSERT_EQUAL_UINT8(128, out.lt);
    TEST_ASSERT_EQUAL_UINT8(64,  out.rt);
}

void test_hid_report_hat_centered(void)
{
    gamepad_report_t in = {0};
    in.dpad = GAMEPAD_DPAD_CENTERED;

    usb_gamepad_report_t out;
    usb_hid_report_from_gamepad(&in, &out);

    /* USB hat: 0 = centered (null state) */
    TEST_ASSERT_EQUAL_UINT8(0, out.hat);
}

void test_hid_report_hat_up(void)
{
    gamepad_report_t in = {0};
    in.dpad = GAMEPAD_DPAD_UP;  /* our format: 0 = north */

    usb_gamepad_report_t out;
    usb_hid_report_from_gamepad(&in, &out);

    /* USB hat: 1 = north */
    TEST_ASSERT_EQUAL_UINT8(1, out.hat);
}

void test_hid_report_hat_all_directions(void)
{
    /*
     * Our format:    0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
     * USB HID hat:   1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW
     * Mapping:       usb = ours + 1
     */
    for (uint8_t dir = 0; dir <= 7; dir++) {
        gamepad_report_t in = {0};
        in.dpad = dir;

        usb_gamepad_report_t out;
        usb_hid_report_from_gamepad(&in, &out);

        TEST_ASSERT_EQUAL_UINT8(dir + 1, out.hat);
    }
}

void test_hid_report_buttons_pass_through(void)
{
    gamepad_report_t in = {0};
    in.dpad = GAMEPAD_DPAD_CENTERED;
    in.buttons = GAMEPAD_BTN_A | GAMEPAD_BTN_Y | GAMEPAD_BTN_GUIDE;

    usb_gamepad_report_t out;
    usb_hid_report_from_gamepad(&in, &out);

    TEST_ASSERT_EQUAL_UINT16(GAMEPAD_BTN_A | GAMEPAD_BTN_Y | GAMEPAD_BTN_GUIDE,
                             out.buttons);
}

void test_hid_report_zeroed_input(void)
{
    gamepad_report_t in;
    memset(&in, 0, sizeof(in));
    in.dpad = GAMEPAD_DPAD_CENTERED;

    usb_gamepad_report_t out;
    usb_hid_report_from_gamepad(&in, &out);

    TEST_ASSERT_EQUAL_INT16(0, out.lx);
    TEST_ASSERT_EQUAL_INT16(0, out.ly);
    TEST_ASSERT_EQUAL_INT16(0, out.rx);
    TEST_ASSERT_EQUAL_INT16(0, out.ry);
    TEST_ASSERT_EQUAL_UINT8(0, out.lt);
    TEST_ASSERT_EQUAL_UINT8(0, out.rt);
    TEST_ASSERT_EQUAL_UINT8(0, out.hat);
    TEST_ASSERT_EQUAL_UINT16(0, out.buttons);
}

/* ── HID report descriptor sanity ────────────────────────────────────── */

void test_hid_descriptor_not_empty(void)
{
    TEST_ASSERT_GREATER_THAN(0, usb_hid_report_descriptor_len);
}

void test_hid_descriptor_starts_with_usage_page(void)
{
    /* 0x05 0x01 = Usage Page (Generic Desktop) */
    TEST_ASSERT_EQUAL_HEX8(0x05, usb_hid_report_descriptor[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, usb_hid_report_descriptor[1]);
}

void test_hid_descriptor_ends_with_end_collection(void)
{
    /* 0xC0 = End Collection */
    TEST_ASSERT_EQUAL_HEX8(0xC0,
        usb_hid_report_descriptor[usb_hid_report_descriptor_len - 1]);
}

void test_hid_report_struct_is_13_bytes(void)
{
    TEST_ASSERT_EQUAL(13, sizeof(usb_gamepad_report_t));
}

/* ── Test runner ─────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* D-pad conversion */
    RUN_TEST(test_dpad_no_input_is_centered);
    RUN_TEST(test_dpad_up);
    RUN_TEST(test_dpad_down);
    RUN_TEST(test_dpad_right);
    RUN_TEST(test_dpad_left);
    RUN_TEST(test_dpad_up_right);
    RUN_TEST(test_dpad_down_right);
    RUN_TEST(test_dpad_down_left);
    RUN_TEST(test_dpad_up_left);
    RUN_TEST(test_dpad_invalid_up_down_is_centered);
    RUN_TEST(test_dpad_invalid_left_right_is_centered);
    RUN_TEST(test_dpad_invalid_all_is_centered);
    RUN_TEST(test_dpad_masks_upper_nibble);

    /* Guide button */
    RUN_TEST(test_guide_pressed_when_set);
    RUN_TEST(test_guide_pressed_when_clear);
    RUN_TEST(test_guide_pressed_zero_buttons);

    /* HID report conversion */
    RUN_TEST(test_hid_report_sticks_pass_through);
    RUN_TEST(test_hid_report_triggers_scale);
    RUN_TEST(test_hid_report_triggers_midpoint);
    RUN_TEST(test_hid_report_hat_centered);
    RUN_TEST(test_hid_report_hat_up);
    RUN_TEST(test_hid_report_hat_all_directions);
    RUN_TEST(test_hid_report_buttons_pass_through);
    RUN_TEST(test_hid_report_zeroed_input);

    /* HID descriptor sanity */
    RUN_TEST(test_hid_descriptor_not_empty);
    RUN_TEST(test_hid_descriptor_starts_with_usage_page);
    RUN_TEST(test_hid_descriptor_ends_with_end_collection);
    RUN_TEST(test_hid_report_struct_is_13_bytes);

    return UNITY_END();
}
