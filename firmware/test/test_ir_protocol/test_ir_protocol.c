#include "unity.h"
#include "ir_protocol.h"
#include <stdbool.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

static ir_timing_t buf[IR_TIMING_MAX];

void setUp(void)
{
    memset(buf, 0, sizeof(buf));
}

void tearDown(void) {}

/* ── NEC Protocol Tests ─────────────────────────────────────────────────── */

void test_nec_standard_frame_length(void)
{
    /* Standard NEC: 1 header + 32 data bits + 1 stop = 34 pairs */
    size_t n = ir_protocol_encode_nec(0x00, 0x02, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL(34, n);
}

void test_nec_header_timing(void)
{
    ir_protocol_encode_nec(0x00, 0x02, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(9000, buf[0].mark_us);
    TEST_ASSERT_EQUAL_UINT16(4500, buf[0].space_us);
}

void test_nec_stop_bit(void)
{
    size_t n = ir_protocol_encode_nec(0x00, 0x02, buf, IR_TIMING_MAX);
    /* Last element should be a stop bit: mark with no space */
    TEST_ASSERT_EQUAL_UINT16(562, buf[n - 1].mark_us);
    TEST_ASSERT_EQUAL_UINT16(0, buf[n - 1].space_us);
}

void test_nec_bit_zero_timing(void)
{
    /* Address 0x00 → all zeros in first byte, so bits 1-8 should be '0' */
    ir_protocol_encode_nec(0x00, 0x00, buf, IR_TIMING_MAX);
    /* First data bit (buf[1]) should be a '0' bit */
    TEST_ASSERT_EQUAL_UINT16(562, buf[1].mark_us);
    TEST_ASSERT_EQUAL_UINT16(562, buf[1].space_us);
}

void test_nec_bit_one_timing(void)
{
    /* Address 0xFF → all ones in first byte, so bits 1-8 should be '1' */
    ir_protocol_encode_nec(0xFF, 0x00, buf, IR_TIMING_MAX);
    /* First data bit (buf[1]) should be a '1' bit */
    TEST_ASSERT_EQUAL_UINT16(562, buf[1].mark_us);
    TEST_ASSERT_EQUAL_UINT16(1687, buf[1].space_us);
}

void test_nec_address_complement(void)
{
    /* Standard NEC: addr 0x55 → complement is 0xAA
     * 0x55 = 01010101, LSB first: 1,0,1,0,1,0,1,0
     * 0xAA = 10101010, LSB first: 0,1,0,1,0,1,0,1
     * Complement bits should be inverted */
    ir_protocol_encode_nec(0x55, 0x00, buf, IR_TIMING_MAX);

    /* Check first addr bit (buf[1]) is '1' (LSB of 0x55) */
    TEST_ASSERT_EQUAL_UINT16(1687, buf[1].space_us);
    /* Check first complement bit (buf[9]) is '0' (LSB of 0xAA) */
    TEST_ASSERT_EQUAL_UINT16(562, buf[9].space_us);
}

void test_nec_command_complement(void)
{
    /* Command 0x02 = 00000010, LSB first: 0,1,0,0,0,0,0,0
     * Complement 0xFD = 11111101, LSB first: 1,0,1,1,1,1,1,1 */
    ir_protocol_encode_nec(0x00, 0x02, buf, IR_TIMING_MAX);

    /* Command starts at buf[17] (after header + 16 addr bits) */
    /* Bit 0 of command (buf[17]): 0 */
    TEST_ASSERT_EQUAL_UINT16(562, buf[17].space_us);
    /* Bit 1 of command (buf[18]): 1 */
    TEST_ASSERT_EQUAL_UINT16(1687, buf[18].space_us);

    /* Complement starts at buf[25] */
    /* Bit 0 of ~command (buf[25]): 1 (LSB of 0xFD) */
    TEST_ASSERT_EQUAL_UINT16(1687, buf[25].space_us);
    /* Bit 1 of ~command (buf[26]): 0 */
    TEST_ASSERT_EQUAL_UINT16(562, buf[26].space_us);
}

void test_nec_extended_16bit_address(void)
{
    /* Extended NEC: address > 0xFF → 16-bit, no complement */
    size_t n = ir_protocol_encode_nec(0x1234, 0x56, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL(34, n);

    /* Still starts with NEC header */
    TEST_ASSERT_EQUAL_UINT16(9000, buf[0].mark_us);
    TEST_ASSERT_EQUAL_UINT16(4500, buf[0].space_us);
}

void test_nec_null_buffer_returns_zero(void)
{
    TEST_ASSERT_EQUAL(0, ir_protocol_encode_nec(0, 0, NULL, IR_TIMING_MAX));
}

void test_nec_buffer_too_small_returns_zero(void)
{
    TEST_ASSERT_EQUAL(0, ir_protocol_encode_nec(0, 0, buf, 10));
}

/* ── NEC Repeat Tests ───────────────────────────────────────────────────── */

void test_nec_repeat_frame_length(void)
{
    size_t n = ir_protocol_encode_nec_repeat(buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL(2, n);
}

void test_nec_repeat_timing(void)
{
    ir_protocol_encode_nec_repeat(buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(9000, buf[0].mark_us);
    TEST_ASSERT_EQUAL_UINT16(2250, buf[0].space_us);
    TEST_ASSERT_EQUAL_UINT16(562, buf[1].mark_us);
    TEST_ASSERT_EQUAL_UINT16(0, buf[1].space_us);
}

/* ── Samsung Protocol Tests ─────────────────────────────────────────────── */

void test_samsung_frame_length(void)
{
    size_t n = ir_protocol_encode_samsung(0x07, 0x02, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL(34, n);
}

void test_samsung_header_timing(void)
{
    ir_protocol_encode_samsung(0x07, 0x02, buf, IR_TIMING_MAX);
    /* Samsung header: 4500 µs mark + 4500 µs space */
    TEST_ASSERT_EQUAL_UINT16(4500, buf[0].mark_us);
    TEST_ASSERT_EQUAL_UINT16(4500, buf[0].space_us);
}

void test_samsung_address_repeated(void)
{
    /* Samsung sends address twice (no complement) */
    ir_protocol_encode_samsung(0xAA, 0x00, buf, IR_TIMING_MAX);

    /* First address byte (buf[1..8]) should match second (buf[9..16]) */
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_UINT16(buf[1 + i].mark_us, buf[9 + i].mark_us);
        TEST_ASSERT_EQUAL_UINT16(buf[1 + i].space_us, buf[9 + i].space_us);
    }
}

void test_samsung_stop_bit(void)
{
    size_t n = ir_protocol_encode_samsung(0x07, 0x02, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(562, buf[n - 1].mark_us);
    TEST_ASSERT_EQUAL_UINT16(0, buf[n - 1].space_us);
}

/* ── Sony SIRC Protocol Tests ───────────────────────────────────────────── */

void test_sony_frame_length(void)
{
    /* 12-bit SIRC: 1 header + 7 cmd + 5 addr = 13 */
    size_t n = ir_protocol_encode_sony(0x01, 0x15, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL(13, n);
}

void test_sony_header_timing(void)
{
    ir_protocol_encode_sony(0x01, 0x15, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(2400, buf[0].mark_us);
    TEST_ASSERT_EQUAL_UINT16(600, buf[0].space_us);
}

void test_sony_bit_zero_mark(void)
{
    /* Command 0x00: all zeros → 600 µs marks */
    ir_protocol_encode_sony(0x00, 0x00, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(600, buf[1].mark_us);
}

void test_sony_bit_one_mark(void)
{
    /* Command 0x7F: all ones → 1200 µs marks */
    ir_protocol_encode_sony(0x00, 0x7F, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(1200, buf[1].mark_us);
}

void test_sony_last_bit_no_space(void)
{
    size_t n = ir_protocol_encode_sony(0x01, 0x15, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(0, buf[n - 1].space_us);
}

void test_sony_spaces_all_600(void)
{
    /* All spaces in Sony are 600 µs (except the last which is 0) */
    size_t n = ir_protocol_encode_sony(0x01, 0x15, buf, IR_TIMING_MAX);
    for (size_t i = 0; i < n - 1; i++) {
        TEST_ASSERT_EQUAL_UINT16(600, buf[i].space_us);
    }
}

/* ── RC5 Protocol Tests ─────────────────────────────────────────────────── */

void test_rc5_produces_output(void)
{
    size_t n = ir_protocol_encode_rc5(0x00, 0x0C, 0, buf, IR_TIMING_MAX);
    TEST_ASSERT_GREATER_THAN(0, n);
}

void test_rc5_timing_multiples_of_889(void)
{
    /* All mark/space durations should be multiples of 889 µs */
    size_t n = ir_protocol_encode_rc5(0x05, 0x0C, 0, buf, IR_TIMING_MAX);
    for (size_t i = 0; i < n; i++) {
        if (buf[i].mark_us > 0) {
            TEST_ASSERT_EQUAL(0, buf[i].mark_us % 889);
        }
        if (buf[i].space_us > 0) {
            TEST_ASSERT_EQUAL(0, buf[i].space_us % 889);
        }
    }
}

void test_rc5_last_space_is_zero(void)
{
    size_t n = ir_protocol_encode_rc5(0x00, 0x0C, 0, buf, IR_TIMING_MAX);
    TEST_ASSERT_EQUAL_UINT16(0, buf[n - 1].space_us);
}

void test_rc5_toggle_changes_output(void)
{
    ir_timing_t buf2[IR_TIMING_MAX];
    size_t n1 = ir_protocol_encode_rc5(0x00, 0x0C, 0, buf, IR_TIMING_MAX);
    size_t n2 = ir_protocol_encode_rc5(0x00, 0x0C, 1, buf2, IR_TIMING_MAX);

    /* Different toggle bit should produce different output */
    bool different = (n1 != n2);
    if (!different) {
        for (size_t i = 0; i < n1 && !different; i++) {
            if (buf[i].mark_us != buf2[i].mark_us ||
                buf[i].space_us != buf2[i].space_us) {
                different = true;
            }
        }
    }
    TEST_ASSERT_TRUE(different);
}

void test_rc5_null_buffer(void)
{
    TEST_ASSERT_EQUAL(0, ir_protocol_encode_rc5(0, 0, 0, NULL, 28));
}

/* ── Carrier Frequency Tests ────────────────────────────────────────────── */

void test_carrier_nec(void)
{
    TEST_ASSERT_EQUAL_UINT32(38000, ir_protocol_carrier_hz(IR_PROTO_NEC));
}

void test_carrier_samsung(void)
{
    TEST_ASSERT_EQUAL_UINT32(38000, ir_protocol_carrier_hz(IR_PROTO_SAMSUNG));
}

void test_carrier_sony(void)
{
    TEST_ASSERT_EQUAL_UINT32(40000, ir_protocol_carrier_hz(IR_PROTO_SONY));
}

void test_carrier_rc5(void)
{
    TEST_ASSERT_EQUAL_UINT32(36000, ir_protocol_carrier_hz(IR_PROTO_RC5));
}

/* ── Protocol Name Tests ────────────────────────────────────────────────── */

void test_protocol_name_nec(void)
{
    TEST_ASSERT_EQUAL_STRING("nec", ir_protocol_name(IR_PROTO_NEC));
}

void test_protocol_name_samsung(void)
{
    TEST_ASSERT_EQUAL_STRING("samsung", ir_protocol_name(IR_PROTO_SAMSUNG));
}

void test_protocol_name_sony(void)
{
    TEST_ASSERT_EQUAL_STRING("sony", ir_protocol_name(IR_PROTO_SONY));
}

void test_protocol_name_rc5(void)
{
    TEST_ASSERT_EQUAL_STRING("rc5", ir_protocol_name(IR_PROTO_RC5));
}

void test_protocol_name_invalid(void)
{
    TEST_ASSERT_EQUAL_STRING("unknown", ir_protocol_name(99));
}

/* ── Total Duration Sanity Tests ────────────────────────────────────────── */

static uint32_t total_duration_us(const ir_timing_t *t, size_t n)
{
    uint32_t total = 0;
    for (size_t i = 0; i < n; i++) {
        total += t[i].mark_us + t[i].space_us;
    }
    return total;
}

void test_nec_total_duration_reasonable(void)
{
    /* NEC frame should be ~67.5ms total */
    size_t n = ir_protocol_encode_nec(0x00, 0x02, buf, IR_TIMING_MAX);
    uint32_t dur = total_duration_us(buf, n);
    /* Should be between 40ms and 100ms */
    TEST_ASSERT_GREATER_THAN(40000, dur);
    TEST_ASSERT_LESS_THAN(100000, dur);
}

void test_sony_total_duration_reasonable(void)
{
    /* Sony 12-bit frame should be ~15-25ms */
    size_t n = ir_protocol_encode_sony(0x01, 0x15, buf, IR_TIMING_MAX);
    uint32_t dur = total_duration_us(buf, n);
    TEST_ASSERT_GREATER_THAN(10000, dur);
    TEST_ASSERT_LESS_THAN(30000, dur);
}

void test_rc5_total_duration_reasonable(void)
{
    /* RC5 frame: 14 bits × 1778 µs ≈ 24.9ms */
    size_t n = ir_protocol_encode_rc5(0x05, 0x0C, 0, buf, IR_TIMING_MAX);
    uint32_t dur = total_duration_us(buf, n);
    TEST_ASSERT_GREATER_THAN(15000, dur);
    TEST_ASSERT_LESS_THAN(35000, dur);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* NEC */
    RUN_TEST(test_nec_standard_frame_length);
    RUN_TEST(test_nec_header_timing);
    RUN_TEST(test_nec_stop_bit);
    RUN_TEST(test_nec_bit_zero_timing);
    RUN_TEST(test_nec_bit_one_timing);
    RUN_TEST(test_nec_address_complement);
    RUN_TEST(test_nec_command_complement);
    RUN_TEST(test_nec_extended_16bit_address);
    RUN_TEST(test_nec_null_buffer_returns_zero);
    RUN_TEST(test_nec_buffer_too_small_returns_zero);

    /* NEC repeat */
    RUN_TEST(test_nec_repeat_frame_length);
    RUN_TEST(test_nec_repeat_timing);

    /* Samsung */
    RUN_TEST(test_samsung_frame_length);
    RUN_TEST(test_samsung_header_timing);
    RUN_TEST(test_samsung_address_repeated);
    RUN_TEST(test_samsung_stop_bit);

    /* Sony */
    RUN_TEST(test_sony_frame_length);
    RUN_TEST(test_sony_header_timing);
    RUN_TEST(test_sony_bit_zero_mark);
    RUN_TEST(test_sony_bit_one_mark);
    RUN_TEST(test_sony_last_bit_no_space);
    RUN_TEST(test_sony_spaces_all_600);

    /* RC5 */
    RUN_TEST(test_rc5_produces_output);
    RUN_TEST(test_rc5_timing_multiples_of_889);
    RUN_TEST(test_rc5_last_space_is_zero);
    RUN_TEST(test_rc5_toggle_changes_output);
    RUN_TEST(test_rc5_null_buffer);

    /* Carrier frequency */
    RUN_TEST(test_carrier_nec);
    RUN_TEST(test_carrier_samsung);
    RUN_TEST(test_carrier_sony);
    RUN_TEST(test_carrier_rc5);

    /* Protocol names */
    RUN_TEST(test_protocol_name_nec);
    RUN_TEST(test_protocol_name_samsung);
    RUN_TEST(test_protocol_name_sony);
    RUN_TEST(test_protocol_name_rc5);
    RUN_TEST(test_protocol_name_invalid);

    /* Duration sanity */
    RUN_TEST(test_nec_total_duration_reasonable);
    RUN_TEST(test_sony_total_duration_reasonable);
    RUN_TEST(test_rc5_total_duration_reasonable);

    return UNITY_END();
}
