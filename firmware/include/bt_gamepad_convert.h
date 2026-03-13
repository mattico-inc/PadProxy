#ifndef BT_GAMEPAD_CONVERT_H
#define BT_GAMEPAD_CONVERT_H

#include <stdint.h>

/**
 * Scale a Bluepad32 axis value (-512..511) to int16_t (-32768..32767).
 *
 * Multiplies by 64 to map the Bluepad32 range onto the full int16 range,
 * then clamps to [-32768, 32767].  Values outside -512..511 are also
 * clamped (e.g. -513 → -32768, 512 → 32767).
 */
int16_t bt_gamepad_scale_axis(int32_t value);

/**
 * Clamp a Bluepad32 trigger/brake value to 0..1023.
 *
 * Bluepad32 reports throttle and brake in this range.  Negative values
 * are clamped to 0; values above 1023 are clamped to 1023.
 */
uint16_t bt_gamepad_clamp_trigger(int32_t value);

#endif /* BT_GAMEPAD_CONVERT_H */
