#ifndef PC_POWER_HAL_H
#define PC_POWER_HAL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Hardware Abstraction Layer for PC power management.
 *
 * This interface decouples the state machine from RP2350 hardware, allowing
 * the state machine to be tested on the host with mock implementations.
 *
 * GPIO mapping (from design doc):
 *   GPIO 2 - PWR_BTN_TRIGGER (output, drives TLP222A photo-MOSFET)
 *   GPIO 3 - PWR_LED_SENSE   (input, PC817 optocoupler, active LOW = PC on)
 */

/**
 * Initialize power management GPIOs.
 *   - PWR_BTN_TRIGGER (GPIO 2): output, initially LOW
 *   - PWR_LED_SENSE (GPIO 3): input with pull-up
 */
void pc_power_hal_init(void);

/**
 * Read the power LED sense line.
 * The PC817 optocoupler output is active LOW (phototransistor pulls to GND
 * when LED current flows). This function inverts the GPIO so callers see:
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
 * On RP2350 this wraps time_us_64(). In tests, it's mocked.
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

/**
 * Check if the boot timer has expired since the last call.
 *
 * Returns true exactly once per expiration, then resets.  The main loop
 * polls this and feeds PC_EVENT_BOOT_TIMEOUT into the state machine.
 */
bool pc_power_hal_boot_timer_expired(void);

#endif /* PC_POWER_HAL_H */
