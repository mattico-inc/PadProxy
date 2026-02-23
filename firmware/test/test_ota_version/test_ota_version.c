#include "unity.h"
#include "ota_version.h"

void setUp(void) {}
void tearDown(void) {}

/* ── Parsing ─────────────────────────────────────────────────────────── */

void test_parse_basic(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("1.2.3", &v));
    TEST_ASSERT_EQUAL_UINT16(1, v.major);
    TEST_ASSERT_EQUAL_UINT16(2, v.minor);
    TEST_ASSERT_EQUAL_UINT16(3, v.patch);
}

void test_parse_v_prefix(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("v1.0.0", &v));
    TEST_ASSERT_EQUAL_UINT16(1, v.major);
    TEST_ASSERT_EQUAL_UINT16(0, v.minor);
    TEST_ASSERT_EQUAL_UINT16(0, v.patch);
}

void test_parse_capital_v_prefix(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("V2.10.5", &v));
    TEST_ASSERT_EQUAL_UINT16(2, v.major);
    TEST_ASSERT_EQUAL_UINT16(10, v.minor);
    TEST_ASSERT_EQUAL_UINT16(5, v.patch);
}

void test_parse_trailing_suffix(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("v1.2.3-rc1", &v));
    TEST_ASSERT_EQUAL_UINT16(1, v.major);
    TEST_ASSERT_EQUAL_UINT16(2, v.minor);
    TEST_ASSERT_EQUAL_UINT16(3, v.patch);
}

void test_parse_trailing_plus(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("3.0.0+build.42", &v));
    TEST_ASSERT_EQUAL_UINT16(3, v.major);
    TEST_ASSERT_EQUAL_UINT16(0, v.minor);
    TEST_ASSERT_EQUAL_UINT16(0, v.patch);
}

void test_parse_large_numbers(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("65535.65535.65535", &v));
    TEST_ASSERT_EQUAL_UINT16(65535, v.major);
    TEST_ASSERT_EQUAL_UINT16(65535, v.minor);
    TEST_ASSERT_EQUAL_UINT16(65535, v.patch);
}

void test_parse_zeros(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("0.0.0", &v));
    TEST_ASSERT_EQUAL_UINT16(0, v.major);
    TEST_ASSERT_EQUAL_UINT16(0, v.minor);
    TEST_ASSERT_EQUAL_UINT16(0, v.patch);
}

/* ── Parsing failures ────────────────────────────────────────────────── */

void test_parse_null_string(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse(NULL, &v));
}

void test_parse_null_out(void)
{
    TEST_ASSERT_FALSE(ota_version_parse("1.0.0", NULL));
}

void test_parse_empty_string(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse("", &v));
}

void test_parse_just_v(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse("v", &v));
}

void test_parse_missing_patch(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse("1.2", &v));
}

void test_parse_missing_minor_and_patch(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse("1", &v));
}

void test_parse_letters_in_version(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse("1.abc.3", &v));
}

void test_parse_negative(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse("-1.0.0", &v));
}

void test_parse_overflow(void)
{
    ota_version_t v;
    TEST_ASSERT_FALSE(ota_version_parse("65536.0.0", &v));
}

/* ── Comparison ──────────────────────────────────────────────────────── */

void test_compare_equal(void)
{
    ota_version_t a = {1, 2, 3};
    ota_version_t b = {1, 2, 3};
    TEST_ASSERT_EQUAL_INT(0, ota_version_compare(&a, &b));
}

void test_compare_major_greater(void)
{
    ota_version_t a = {2, 0, 0};
    ota_version_t b = {1, 9, 9};
    TEST_ASSERT_TRUE(ota_version_compare(&a, &b) > 0);
}

void test_compare_major_less(void)
{
    ota_version_t a = {1, 9, 9};
    ota_version_t b = {2, 0, 0};
    TEST_ASSERT_TRUE(ota_version_compare(&a, &b) < 0);
}

void test_compare_minor_greater(void)
{
    ota_version_t a = {1, 3, 0};
    ota_version_t b = {1, 2, 9};
    TEST_ASSERT_TRUE(ota_version_compare(&a, &b) > 0);
}

void test_compare_minor_less(void)
{
    ota_version_t a = {1, 2, 9};
    ota_version_t b = {1, 3, 0};
    TEST_ASSERT_TRUE(ota_version_compare(&a, &b) < 0);
}

void test_compare_patch_greater(void)
{
    ota_version_t a = {1, 2, 4};
    ota_version_t b = {1, 2, 3};
    TEST_ASSERT_TRUE(ota_version_compare(&a, &b) > 0);
}

void test_compare_patch_less(void)
{
    ota_version_t a = {1, 2, 3};
    ota_version_t b = {1, 2, 4};
    TEST_ASSERT_TRUE(ota_version_compare(&a, &b) < 0);
}

void test_compare_zero(void)
{
    ota_version_t a = {0, 0, 0};
    ota_version_t b = {0, 0, 1};
    TEST_ASSERT_TRUE(ota_version_compare(&a, &b) < 0);
}

/* ── Formatting ──────────────────────────────────────────────────────── */

void test_format_basic(void)
{
    ota_version_t v = {1, 2, 3};
    char buf[32];
    int n = ota_version_format(&v, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1.2.3", buf);
    TEST_ASSERT_EQUAL_INT(5, n);
}

void test_format_zeros(void)
{
    ota_version_t v = {0, 0, 0};
    char buf[32];
    ota_version_format(&v, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.0.0", buf);
}

void test_format_large(void)
{
    ota_version_t v = {100, 200, 300};
    char buf[32];
    ota_version_format(&v, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100.200.300", buf);
}

void test_format_null_version(void)
{
    char buf[16];
    TEST_ASSERT_EQUAL_INT(-1, ota_version_format(NULL, buf, sizeof(buf)));
}

void test_format_null_buf(void)
{
    ota_version_t v = {1, 0, 0};
    TEST_ASSERT_EQUAL_INT(-1, ota_version_format(&v, NULL, 16));
}

/* ── Round-trip: parse → format ──────────────────────────────────────── */

void test_roundtrip(void)
{
    ota_version_t v;
    TEST_ASSERT_TRUE(ota_version_parse("v12.34.56", &v));

    char buf[32];
    ota_version_format(&v, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("12.34.56", buf);
}

/* ── Test runner ─────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Parsing */
    RUN_TEST(test_parse_basic);
    RUN_TEST(test_parse_v_prefix);
    RUN_TEST(test_parse_capital_v_prefix);
    RUN_TEST(test_parse_trailing_suffix);
    RUN_TEST(test_parse_trailing_plus);
    RUN_TEST(test_parse_large_numbers);
    RUN_TEST(test_parse_zeros);

    /* Parsing failures */
    RUN_TEST(test_parse_null_string);
    RUN_TEST(test_parse_null_out);
    RUN_TEST(test_parse_empty_string);
    RUN_TEST(test_parse_just_v);
    RUN_TEST(test_parse_missing_patch);
    RUN_TEST(test_parse_missing_minor_and_patch);
    RUN_TEST(test_parse_letters_in_version);
    RUN_TEST(test_parse_negative);
    RUN_TEST(test_parse_overflow);

    /* Comparison */
    RUN_TEST(test_compare_equal);
    RUN_TEST(test_compare_major_greater);
    RUN_TEST(test_compare_major_less);
    RUN_TEST(test_compare_minor_greater);
    RUN_TEST(test_compare_minor_less);
    RUN_TEST(test_compare_patch_greater);
    RUN_TEST(test_compare_patch_less);
    RUN_TEST(test_compare_zero);

    /* Formatting */
    RUN_TEST(test_format_basic);
    RUN_TEST(test_format_zeros);
    RUN_TEST(test_format_large);
    RUN_TEST(test_format_null_version);
    RUN_TEST(test_format_null_buf);

    /* Round-trip */
    RUN_TEST(test_roundtrip);

    return UNITY_END();
}
