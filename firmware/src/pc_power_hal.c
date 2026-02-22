#include "pc_power_hal.h"

#include "hardware/gpio.h"
#include "pico/time.h"

/* ── GPIO assignments (from hardware design) ─────────────────────────── */

#define GPIO_PWR_BTN_TRIGGER  2   /* Output: drives TLP222A photo-MOSFET  */
#define GPIO_PWR_LED_SENSE    3   /* Input: PC817 optocoupler, active LOW  */

/* ── Internal state ──────────────────────────────────────────────────── */

static alarm_id_t    s_power_btn_alarm;
static alarm_id_t    s_boot_timer_alarm;
static volatile bool s_boot_timer_expired;

/* ── Alarm callbacks (run from timer IRQ context) ────────────────────── */

static int64_t power_btn_alarm_cb(alarm_id_t id, void *user_data)
{
    (void)id;
    (void)user_data;
    gpio_put(GPIO_PWR_BTN_TRIGGER, false);
    s_power_btn_alarm = 0;
    return 0; /* don't reschedule */
}

static int64_t boot_timer_alarm_cb(alarm_id_t id, void *user_data)
{
    (void)id;
    (void)user_data;
    s_boot_timer_expired = true;
    s_boot_timer_alarm = 0;
    return 0;
}

/* ── Public API ──────────────────────────────────────────────────────── */

void pc_power_hal_init(void)
{
    /* PWR_BTN_TRIGGER (GPIO 2): output, initially LOW (optocoupler off) */
    gpio_init(GPIO_PWR_BTN_TRIGGER);
    gpio_set_dir(GPIO_PWR_BTN_TRIGGER, GPIO_OUT);
    gpio_put(GPIO_PWR_BTN_TRIGGER, false);

    /* PWR_LED_SENSE (GPIO 3): input with pull-up (PC817 active LOW) */
    gpio_init(GPIO_PWR_LED_SENSE);
    gpio_set_dir(GPIO_PWR_LED_SENSE, GPIO_IN);
    gpio_pull_up(GPIO_PWR_LED_SENSE);

    s_power_btn_alarm = 0;
    s_boot_timer_alarm = 0;
    s_boot_timer_expired = false;
}

bool pc_power_hal_read_power_led(void)
{
    /* PC817 phototransistor pulls GPIO LOW when LED current flows (PC on).
     * Invert so callers see: true = PC on, false = PC off. */
    return !gpio_get(GPIO_PWR_LED_SENSE);
}

void pc_power_hal_trigger_power_button(uint32_t duration_ms)
{
    /* Cancel any in-flight pulse so we don't stack up */
    if (s_power_btn_alarm) {
        cancel_alarm(s_power_btn_alarm);
        s_power_btn_alarm = 0;
    }

    gpio_put(GPIO_PWR_BTN_TRIGGER, true);
    s_power_btn_alarm = add_alarm_in_ms(
        duration_ms, power_btn_alarm_cb, NULL, true);
}

uint32_t pc_power_hal_millis(void)
{
    return (uint32_t)(time_us_64() / 1000);
}

void pc_power_hal_start_boot_timer(uint32_t timeout_ms)
{
    pc_power_hal_cancel_boot_timer();
    s_boot_timer_expired = false;
    s_boot_timer_alarm = add_alarm_in_ms(
        timeout_ms, boot_timer_alarm_cb, NULL, true);
}

void pc_power_hal_cancel_boot_timer(void)
{
    if (s_boot_timer_alarm) {
        cancel_alarm(s_boot_timer_alarm);
        s_boot_timer_alarm = 0;
    }
    s_boot_timer_expired = false;
}

bool pc_power_hal_boot_timer_expired(void)
{
    if (s_boot_timer_expired) {
        s_boot_timer_expired = false;
        return true;
    }
    return false;
}
