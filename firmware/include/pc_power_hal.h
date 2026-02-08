#ifndef PC_POWER_HAL_H
#define PC_POWER_HAL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Hardware Abstraction Layer for PC power management.
 *
 * This interface decouples the state machine from ESP32 hardware, allowing
 * the state machine to be tested on the host with mock implementations.
 *
 * GPIO mapping (from design doc):
 *   GPIO 1 - PWR_BTN_SENSE   (input, active HIGH = pressed)
 *   GPIO 2 - PWR_BTN_TRIGGER (output, drives TLP222A photo-MOSFET)
 *   GPIO 3 - PWR_LED_SENSE   (input, HIGH = PC on, Zener-clamped)
 */

/**
 * Initialize power management GPIOs.
 *   - PWR_BTN_SENSE (GPIO 1): input with pull-up
 *   - PWR_BTN_TRIGGER (GPIO 2): output, initially LOW
 *   - PWR_LED_SENSE (GPIO 3): input
 */
void pc_power_hal_init(void);

/**
 * Read the physical power button state.
 * @return true if button is currently pressed (GPIO HIGH after optocoupler).
 */
bool pc_power_hal_read_button(void);

/**
 * Read the power LED sense line.
 * @return true if the motherboard power LED output is active (PC is on).
 */
bool pc_power_hal_read_power_led(void);

/**
 * Pulse the power button trigger optocoupler.
 * Drives GPIO 2 HIGH for the specified duration, then LOW.
 * @param duration_ms Pulse width in milliseconds (typical: 200ms).
 */
void pc_power_hal_trigger_power_button(uint32_t duration_ms);

/**
 * Get the current system time in milliseconds.
 * On ESP32 this wraps esp_timer_get_time(). In tests, it's mocked.
 */
uint32_t pc_power_hal_millis(void);

/**
 * Start the boot timeout timer.
 * @param timeout_ms Duration before PC_EVENT_BOOT_TIMEOUT fires.
 */
void pc_power_hal_start_boot_timer(uint32_t timeout_ms);

/**
 * Cancel a running boot timeout timer.
 */
void pc_power_hal_cancel_boot_timer(void);

#endif /* PC_POWER_HAL_H */
