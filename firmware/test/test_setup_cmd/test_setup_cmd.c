#include "unity.h"
#include "setup_cmd.h"
#include <string.h>
#include <stdio.h>

static device_config_t cfg;
static char out[1024];

void setUp(void)
{
    device_config_init(&cfg);
    memset(out, 0, sizeof(out));
    setup_cmd_set_version("1.2.3");
    setup_cmd_set_status("pc_state=OFF bt_connected=false");
}

void tearDown(void)
{
}

/* ── Helper ──────────────────────────────────────────────────────────── */

static setup_cmd_result_t run(const char *line)
{
    memset(out, 0, sizeof(out));
    return setup_cmd_process(line, &cfg, out, sizeof(out));
}

/** Check that output starts with "OK" */
static void assert_ok(void)
{
    TEST_ASSERT_EQUAL_UINT8('O', out[0]);
    TEST_ASSERT_EQUAL_UINT8('K', out[1]);
}

/** Check that output starts with "ERR" */
static void assert_err(void)
{
    TEST_ASSERT_EQUAL_UINT8('E', out[0]);
    TEST_ASSERT_EQUAL_UINT8('R', out[1]);
    TEST_ASSERT_EQUAL_UINT8('R', out[2]);
}

/* ── get command ─────────────────────────────────────────────────────── */

void test_get_wifi_ssid_default(void)
{
    setup_cmd_result_t r = run("get wifi_ssid");
    TEST_ASSERT_TRUE(r.out_len > 0);
    assert_ok();
    /* Default is empty, so "OK \n" */
    TEST_ASSERT_EQUAL_STRING("OK \n", out);
}

void test_get_wifi_ssid_after_set(void)
{
    strncpy(cfg.wifi_ssid, "TestNet", DEVICE_CONFIG_WIFI_SSID_MAX);
    run("get wifi_ssid");
    TEST_ASSERT_EQUAL_STRING("OK TestNet\n", out);
}

void test_get_wifi_password_masked(void)
{
    strncpy(cfg.wifi_password, "secret", DEVICE_CONFIG_WIFI_PASSWORD_MAX);
    run("get wifi_password");
    TEST_ASSERT_EQUAL_STRING("OK ********\n", out);
}

void test_get_power_pulse_ms(void)
{
    run("get power_pulse_ms");
    TEST_ASSERT_EQUAL_STRING("OK 200\n", out);
}

void test_get_boot_timeout_ms(void)
{
    run("get boot_timeout_ms");
    TEST_ASSERT_EQUAL_STRING("OK 30000\n", out);
}

void test_get_device_name(void)
{
    run("get device_name");
    TEST_ASSERT_EQUAL_STRING("OK PadProxy\n", out);
}

void test_get_unknown_key(void)
{
    run("get nonexistent");
    assert_err();
}

void test_get_no_key(void)
{
    run("get");
    assert_err();
}

/* ── set command ─────────────────────────────────────────────────────── */

void test_set_wifi_ssid(void)
{
    setup_cmd_result_t r = run("set wifi_ssid MyNetwork");
    assert_ok();
    TEST_ASSERT_EQUAL(SETUP_ACTION_NONE, r.action);
    TEST_ASSERT_EQUAL_STRING("MyNetwork", cfg.wifi_ssid);
}

void test_set_wifi_password(void)
{
    run("set wifi_password hunter2");
    assert_ok();
    TEST_ASSERT_EQUAL_STRING("hunter2", cfg.wifi_password);
}

void test_set_power_pulse_ms(void)
{
    run("set power_pulse_ms 500");
    assert_ok();
    TEST_ASSERT_EQUAL_UINT16(500, cfg.power_pulse_ms);
}

void test_set_power_pulse_ms_at_min(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "set power_pulse_ms %d",
             DEVICE_CONFIG_POWER_PULSE_MIN);
    run(cmd);
    assert_ok();
    TEST_ASSERT_EQUAL_UINT16(DEVICE_CONFIG_POWER_PULSE_MIN,
                             cfg.power_pulse_ms);
}

void test_set_power_pulse_ms_at_max(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "set power_pulse_ms %d",
             DEVICE_CONFIG_POWER_PULSE_MAX);
    run(cmd);
    assert_ok();
    TEST_ASSERT_EQUAL_UINT16(DEVICE_CONFIG_POWER_PULSE_MAX,
                             cfg.power_pulse_ms);
}

void test_set_power_pulse_ms_too_low(void)
{
    run("set power_pulse_ms 10");
    assert_err();
    /* Value unchanged */
    TEST_ASSERT_EQUAL_UINT16(DEVICE_CONFIG_DEFAULT_POWER_PULSE_MS,
                             cfg.power_pulse_ms);
}

void test_set_power_pulse_ms_too_high(void)
{
    run("set power_pulse_ms 5000");
    assert_err();
}

void test_set_power_pulse_ms_not_a_number(void)
{
    run("set power_pulse_ms abc");
    assert_err();
}

void test_set_boot_timeout_ms(void)
{
    run("set boot_timeout_ms 10000");
    assert_ok();
    TEST_ASSERT_EQUAL_UINT16(10000, cfg.boot_timeout_ms);
}

void test_set_device_name(void)
{
    run("set device_name MyPad");
    assert_ok();
    TEST_ASSERT_EQUAL_STRING("MyPad", cfg.device_name);
}

void test_set_device_name_empty(void)
{
    /* "set device_name " — value is empty string after key */
    run("set device_name ");
    assert_err();
}

void test_set_unknown_key(void)
{
    run("set nonexistent value");
    assert_err();
}

void test_set_no_value(void)
{
    run("set wifi_ssid");
    assert_err();
}

void test_set_no_key(void)
{
    run("set");
    assert_err();
}

/* ── set with spaces in value ────────────────────────────────────────── */

void test_set_wifi_ssid_with_spaces(void)
{
    /* "set wifi_ssid My Network" — value should be "My Network" */
    /* Our parser splits on first space after key, so value = "My Network" */
    /* Actually: set<sp>wifi_ssid<sp>My Network
     * cmd = "set", arg = "wifi_ssid My Network"
     * key = "wifi_ssid", value = "My Network" */
    run("set wifi_ssid My Network");
    assert_ok();
    TEST_ASSERT_EQUAL_STRING("My Network", cfg.wifi_ssid);
}

/* ── list command ────────────────────────────────────────────────────── */

void test_list_contains_all_keys(void)
{
    setup_cmd_result_t r = run("list");
    TEST_ASSERT_TRUE(r.out_len > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "wifi_ssid="));
    TEST_ASSERT_NOT_NULL(strstr(out, "wifi_password=********"));
    TEST_ASSERT_NOT_NULL(strstr(out, "power_pulse_ms="));
    TEST_ASSERT_NOT_NULL(strstr(out, "boot_timeout_ms="));
    TEST_ASSERT_NOT_NULL(strstr(out, "device_name="));
}

void test_list_shows_current_values(void)
{
    cfg.power_pulse_ms = 300;
    strncpy(cfg.device_name, "TestDev", DEVICE_CONFIG_DEVICE_NAME_MAX);
    run("list");
    TEST_ASSERT_NOT_NULL(strstr(out, "power_pulse_ms=300"));
    TEST_ASSERT_NOT_NULL(strstr(out, "device_name=TestDev"));
}

/* ── save command ────────────────────────────────────────────────────── */

void test_save_returns_save_action(void)
{
    setup_cmd_result_t r = run("save");
    assert_ok();
    TEST_ASSERT_EQUAL(SETUP_ACTION_SAVE, r.action);
}

/* ── defaults command ────────────────────────────────────────────────── */

void test_defaults_resets_config(void)
{
    cfg.power_pulse_ms = 999;
    strncpy(cfg.wifi_ssid, "Modified", DEVICE_CONFIG_WIFI_SSID_MAX);

    run("defaults");
    assert_ok();

    TEST_ASSERT_EQUAL_UINT16(DEVICE_CONFIG_DEFAULT_POWER_PULSE_MS,
                             cfg.power_pulse_ms);
    TEST_ASSERT_EQUAL_STRING("", cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING(DEVICE_CONFIG_DEFAULT_DEVICE_NAME,
                             cfg.device_name);
}

/* ── version command ─────────────────────────────────────────────────── */

void test_version(void)
{
    run("version");
    TEST_ASSERT_EQUAL_STRING("OK 1.2.3\n", out);
}

/* ── status command ──────────────────────────────────────────────────── */

void test_status(void)
{
    run("status");
    TEST_ASSERT_EQUAL_STRING("OK pc_state=OFF bt_connected=false\n", out);
}

/* ── reboot command ──────────────────────────────────────────────────── */

void test_reboot_returns_reboot_action(void)
{
    setup_cmd_result_t r = run("reboot");
    assert_ok();
    TEST_ASSERT_EQUAL(SETUP_ACTION_REBOOT, r.action);
}

/* ── Unknown command ─────────────────────────────────────────────────── */

void test_unknown_command(void)
{
    run("foobar");
    assert_err();
}

/* ── Edge cases ──────────────────────────────────────────────────────── */

void test_empty_line(void)
{
    setup_cmd_result_t r = run("");
    TEST_ASSERT_EQUAL_INT(0, r.out_len);
    TEST_ASSERT_EQUAL(SETUP_ACTION_NONE, r.action);
}

void test_whitespace_only(void)
{
    setup_cmd_result_t r = run("   \t  ");
    TEST_ASSERT_EQUAL_INT(0, r.out_len);
}

void test_trailing_newline(void)
{
    run("version\n");
    TEST_ASSERT_EQUAL_STRING("OK 1.2.3\n", out);
}

void test_trailing_crlf(void)
{
    run("version\r\n");
    TEST_ASSERT_EQUAL_STRING("OK 1.2.3\n", out);
}

void test_leading_whitespace(void)
{
    run("  version");
    TEST_ASSERT_EQUAL_STRING("OK 1.2.3\n", out);
}

void test_null_inputs(void)
{
    setup_cmd_result_t r;

    r = setup_cmd_process(NULL, &cfg, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, r.out_len);

    r = setup_cmd_process("version", NULL, out, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, r.out_len);

    r = setup_cmd_process("version", &cfg, NULL, sizeof(out));
    TEST_ASSERT_EQUAL_INT(0, r.out_len);

    r = setup_cmd_process("version", &cfg, out, 0);
    TEST_ASSERT_EQUAL_INT(0, r.out_len);
}

/* ── Test runner ──────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* get */
    RUN_TEST(test_get_wifi_ssid_default);
    RUN_TEST(test_get_wifi_ssid_after_set);
    RUN_TEST(test_get_wifi_password_masked);
    RUN_TEST(test_get_power_pulse_ms);
    RUN_TEST(test_get_boot_timeout_ms);
    RUN_TEST(test_get_device_name);
    RUN_TEST(test_get_unknown_key);
    RUN_TEST(test_get_no_key);

    /* set */
    RUN_TEST(test_set_wifi_ssid);
    RUN_TEST(test_set_wifi_password);
    RUN_TEST(test_set_power_pulse_ms);
    RUN_TEST(test_set_power_pulse_ms_at_min);
    RUN_TEST(test_set_power_pulse_ms_at_max);
    RUN_TEST(test_set_power_pulse_ms_too_low);
    RUN_TEST(test_set_power_pulse_ms_too_high);
    RUN_TEST(test_set_power_pulse_ms_not_a_number);
    RUN_TEST(test_set_boot_timeout_ms);
    RUN_TEST(test_set_device_name);
    RUN_TEST(test_set_device_name_empty);
    RUN_TEST(test_set_unknown_key);
    RUN_TEST(test_set_no_value);
    RUN_TEST(test_set_no_key);
    RUN_TEST(test_set_wifi_ssid_with_spaces);

    /* list */
    RUN_TEST(test_list_contains_all_keys);
    RUN_TEST(test_list_shows_current_values);

    /* save */
    RUN_TEST(test_save_returns_save_action);

    /* defaults */
    RUN_TEST(test_defaults_resets_config);

    /* version */
    RUN_TEST(test_version);

    /* status */
    RUN_TEST(test_status);

    /* reboot */
    RUN_TEST(test_reboot_returns_reboot_action);

    /* Unknown command */
    RUN_TEST(test_unknown_command);

    /* Edge cases */
    RUN_TEST(test_empty_line);
    RUN_TEST(test_whitespace_only);
    RUN_TEST(test_trailing_newline);
    RUN_TEST(test_trailing_crlf);
    RUN_TEST(test_leading_whitespace);
    RUN_TEST(test_null_inputs);

    return UNITY_END();
}
