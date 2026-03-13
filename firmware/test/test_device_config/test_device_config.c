#include "unity.h"
#include "device_config.h"
#include <string.h>

static device_config_t cfg;
static uint8_t buf[DEVICE_CONFIG_SERIAL_SIZE];

void setUp(void)
{
    device_config_init(&cfg);
    memset(buf, 0, sizeof(buf));
}

void tearDown(void)
{
}

/* ── Defaults ────────────────────────────────────────────────────────── */

void test_defaults_power_pulse(void)
{
    TEST_ASSERT_EQUAL_UINT16(DEVICE_CONFIG_DEFAULT_POWER_PULSE_MS,
                             cfg.power_pulse_ms);
}

void test_defaults_boot_timeout(void)
{
    TEST_ASSERT_EQUAL_UINT16(DEVICE_CONFIG_DEFAULT_BOOT_TIMEOUT_MS,
                             cfg.boot_timeout_ms);
}

void test_defaults_device_name(void)
{
    TEST_ASSERT_EQUAL_STRING(DEVICE_CONFIG_DEFAULT_DEVICE_NAME,
                             cfg.device_name);
}

void test_defaults_wifi_empty(void)
{
    TEST_ASSERT_EQUAL_STRING("", cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("", cfg.wifi_password);
}

void test_defaults_are_valid(void)
{
    TEST_ASSERT_TRUE(device_config_validate(&cfg));
}

/* ── Validation ──────────────────────────────────────────────────────── */

void test_validate_null(void)
{
    TEST_ASSERT_FALSE(device_config_validate(NULL));
}

void test_validate_power_pulse_too_low(void)
{
    cfg.power_pulse_ms = DEVICE_CONFIG_POWER_PULSE_MIN - 1;
    TEST_ASSERT_FALSE(device_config_validate(&cfg));
}

void test_validate_power_pulse_too_high(void)
{
    cfg.power_pulse_ms = DEVICE_CONFIG_POWER_PULSE_MAX + 1;
    TEST_ASSERT_FALSE(device_config_validate(&cfg));
}

void test_validate_power_pulse_at_min(void)
{
    cfg.power_pulse_ms = DEVICE_CONFIG_POWER_PULSE_MIN;
    TEST_ASSERT_TRUE(device_config_validate(&cfg));
}

void test_validate_power_pulse_at_max(void)
{
    cfg.power_pulse_ms = DEVICE_CONFIG_POWER_PULSE_MAX;
    TEST_ASSERT_TRUE(device_config_validate(&cfg));
}

void test_validate_boot_timeout_too_low(void)
{
    cfg.boot_timeout_ms = DEVICE_CONFIG_BOOT_TIMEOUT_MIN - 1;
    TEST_ASSERT_FALSE(device_config_validate(&cfg));
}

void test_validate_boot_timeout_too_high(void)
{
    cfg.boot_timeout_ms = DEVICE_CONFIG_BOOT_TIMEOUT_MAX + 1;
    TEST_ASSERT_FALSE(device_config_validate(&cfg));
}

void test_validate_boot_timeout_at_min(void)
{
    cfg.boot_timeout_ms = DEVICE_CONFIG_BOOT_TIMEOUT_MIN;
    TEST_ASSERT_TRUE(device_config_validate(&cfg));
}

void test_validate_boot_timeout_at_max(void)
{
    cfg.boot_timeout_ms = DEVICE_CONFIG_BOOT_TIMEOUT_MAX;
    TEST_ASSERT_TRUE(device_config_validate(&cfg));
}

void test_validate_empty_device_name(void)
{
    cfg.device_name[0] = '\0';
    TEST_ASSERT_FALSE(device_config_validate(&cfg));
}

/* ── Serialization roundtrip ─────────────────────────────────────────── */

void test_serialize_returns_positive_length(void)
{
    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
}

void test_roundtrip_defaults(void)
{
    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    device_config_t loaded;
    memset(&loaded, 0xFF, sizeof(loaded));  /* poison */
    TEST_ASSERT_TRUE(device_config_deserialize(&loaded, buf, (size_t)n));

    TEST_ASSERT_EQUAL_STRING(cfg.wifi_ssid, loaded.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING(cfg.wifi_password, loaded.wifi_password);
    TEST_ASSERT_EQUAL_UINT16(cfg.power_pulse_ms, loaded.power_pulse_ms);
    TEST_ASSERT_EQUAL_UINT16(cfg.boot_timeout_ms, loaded.boot_timeout_ms);
    TEST_ASSERT_EQUAL_STRING(cfg.device_name, loaded.device_name);
}

void test_roundtrip_with_wifi(void)
{
    strncpy(cfg.wifi_ssid, "MyNetwork", DEVICE_CONFIG_WIFI_SSID_MAX);
    strncpy(cfg.wifi_password, "secret123", DEVICE_CONFIG_WIFI_PASSWORD_MAX);

    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    device_config_t loaded;
    TEST_ASSERT_TRUE(device_config_deserialize(&loaded, buf, (size_t)n));

    TEST_ASSERT_EQUAL_STRING("MyNetwork", loaded.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("secret123", loaded.wifi_password);
}

void test_roundtrip_custom_values(void)
{
    strncpy(cfg.wifi_ssid, "TestSSID", DEVICE_CONFIG_WIFI_SSID_MAX);
    strncpy(cfg.wifi_password, "TestPass", DEVICE_CONFIG_WIFI_PASSWORD_MAX);
    cfg.power_pulse_ms = 500;
    cfg.boot_timeout_ms = 10000;
    strncpy(cfg.device_name, "MyPad", DEVICE_CONFIG_DEVICE_NAME_MAX);

    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    device_config_t loaded;
    TEST_ASSERT_TRUE(device_config_deserialize(&loaded, buf, (size_t)n));

    TEST_ASSERT_EQUAL_STRING("TestSSID", loaded.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("TestPass", loaded.wifi_password);
    TEST_ASSERT_EQUAL_UINT16(500, loaded.power_pulse_ms);
    TEST_ASSERT_EQUAL_UINT16(10000, loaded.boot_timeout_ms);
    TEST_ASSERT_EQUAL_STRING("MyPad", loaded.device_name);
}

void test_roundtrip_max_length_strings(void)
{
    /* Fill SSID to max length */
    memset(cfg.wifi_ssid, 'A', DEVICE_CONFIG_WIFI_SSID_MAX);
    cfg.wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX] = '\0';

    /* Fill password to max length */
    memset(cfg.wifi_password, 'B', DEVICE_CONFIG_WIFI_PASSWORD_MAX);
    cfg.wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX] = '\0';

    /* Fill device name to max length */
    memset(cfg.device_name, 'C', DEVICE_CONFIG_DEVICE_NAME_MAX);
    cfg.device_name[DEVICE_CONFIG_DEVICE_NAME_MAX] = '\0';

    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);

    device_config_t loaded;
    TEST_ASSERT_TRUE(device_config_deserialize(&loaded, buf, (size_t)n));

    TEST_ASSERT_EQUAL_UINT(DEVICE_CONFIG_WIFI_SSID_MAX,
                           strlen(loaded.wifi_ssid));
    TEST_ASSERT_EQUAL_UINT(DEVICE_CONFIG_WIFI_PASSWORD_MAX,
                           strlen(loaded.wifi_password));
    TEST_ASSERT_EQUAL_UINT(DEVICE_CONFIG_DEVICE_NAME_MAX,
                           strlen(loaded.device_name));
}

/* ── Deserialization failure cases ───────────────────────────────────── */

void test_deserialize_null_cfg(void)
{
    device_config_serialize(&cfg, buf, sizeof(buf));
    TEST_ASSERT_FALSE(device_config_deserialize(NULL, buf, sizeof(buf)));
}

void test_deserialize_null_buf(void)
{
    device_config_t loaded;
    TEST_ASSERT_FALSE(device_config_deserialize(&loaded, NULL, 100));
}

void test_deserialize_too_short(void)
{
    device_config_serialize(&cfg, buf, sizeof(buf));
    device_config_t loaded;
    TEST_ASSERT_FALSE(device_config_deserialize(&loaded, buf, 5));
}

void test_deserialize_bad_magic(void)
{
    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    buf[0] = 0xFF;  /* corrupt magic */

    device_config_t loaded;
    TEST_ASSERT_FALSE(device_config_deserialize(&loaded, buf, (size_t)n));
}

void test_deserialize_bad_crc(void)
{
    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    /* Corrupt a payload byte (the device name area) */
    buf[10] ^= 0xFF;

    device_config_t loaded;
    TEST_ASSERT_FALSE(device_config_deserialize(&loaded, buf, (size_t)n));
}

void test_deserialize_all_zeros(void)
{
    memset(buf, 0, sizeof(buf));
    device_config_t loaded;
    TEST_ASSERT_FALSE(device_config_deserialize(&loaded, buf, sizeof(buf)));
}

void test_deserialize_all_ones(void)
{
    memset(buf, 0xFF, sizeof(buf));
    device_config_t loaded;
    TEST_ASSERT_FALSE(device_config_deserialize(&loaded, buf, sizeof(buf)));
}

void test_deserialize_bad_version(void)
{
    /* Format: [magic: 4B][version: 2B][payload][crc32: 4B]
     * Corrupt the version field (bytes 4-5) so the version check fails. */
    int n = device_config_serialize(&cfg, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    buf[4] ^= 0xFF;  /* corrupt version low byte */

    device_config_t loaded;
    TEST_ASSERT_FALSE(device_config_deserialize(&loaded, buf, (size_t)n));
}

/* ── Serialize error cases ───────────────────────────────────────────── */

void test_serialize_null_cfg(void)
{
    TEST_ASSERT_EQUAL_INT(-1, device_config_serialize(NULL, buf, sizeof(buf)));
}

void test_serialize_null_buf(void)
{
    TEST_ASSERT_EQUAL_INT(-1, device_config_serialize(&cfg, NULL, 256));
}

void test_serialize_buf_too_small(void)
{
    TEST_ASSERT_EQUAL_INT(-1, device_config_serialize(&cfg, buf, 10));
}

/* ── Test runner ──────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Defaults */
    RUN_TEST(test_defaults_power_pulse);
    RUN_TEST(test_defaults_boot_timeout);
    RUN_TEST(test_defaults_device_name);
    RUN_TEST(test_defaults_wifi_empty);
    RUN_TEST(test_defaults_are_valid);

    /* Validation */
    RUN_TEST(test_validate_null);
    RUN_TEST(test_validate_power_pulse_too_low);
    RUN_TEST(test_validate_power_pulse_too_high);
    RUN_TEST(test_validate_power_pulse_at_min);
    RUN_TEST(test_validate_power_pulse_at_max);
    RUN_TEST(test_validate_boot_timeout_too_low);
    RUN_TEST(test_validate_boot_timeout_too_high);
    RUN_TEST(test_validate_boot_timeout_at_min);
    RUN_TEST(test_validate_boot_timeout_at_max);
    RUN_TEST(test_validate_empty_device_name);

    /* Serialization roundtrip */
    RUN_TEST(test_serialize_returns_positive_length);
    RUN_TEST(test_roundtrip_defaults);
    RUN_TEST(test_roundtrip_with_wifi);
    RUN_TEST(test_roundtrip_custom_values);
    RUN_TEST(test_roundtrip_max_length_strings);

    /* Deserialization failures */
    RUN_TEST(test_deserialize_null_cfg);
    RUN_TEST(test_deserialize_null_buf);
    RUN_TEST(test_deserialize_too_short);
    RUN_TEST(test_deserialize_bad_magic);
    RUN_TEST(test_deserialize_bad_crc);
    RUN_TEST(test_deserialize_all_zeros);
    RUN_TEST(test_deserialize_all_ones);
    RUN_TEST(test_deserialize_bad_version);

    /* Serialize errors */
    RUN_TEST(test_serialize_null_cfg);
    RUN_TEST(test_serialize_null_buf);
    RUN_TEST(test_serialize_buf_too_small);

    return UNITY_END();
}
