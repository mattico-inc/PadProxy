#ifndef BT_MOUSE_CONVERT_H
#define BT_MOUSE_CONVERT_H

#include <stdint.h>
#include "usb_hid_report.h"

/**
 * Bluepad32 mouse sample -> USB wire format conversion.
 *
 * Pure logic, no hardware or Bluepad32 header dependencies (mirrors
 * bt_gamepad_convert.h), so it can be unit-tested on the host. Callers
 * pass the raw scalar fields from uni_mouse_t rather than the struct
 * itself.
 */

/**
 * Clamp a Bluepad32 mouse delta (arbitrary int32 range) to the HID
 * report's relative-motion range of -127..127.
 */
int8_t bt_mouse_clamp_delta(int32_t value);

/**
 * Map Bluepad32 mouse button bits (UNI_MOUSE_BUTTON_LEFT/RIGHT/MIDDLE/
 * AUX_0/AUX_1) to the USB HID mouse button bitmask (bit0=left, bit1=right,
 * bit2=middle, bit3=backward, bit4=forward).
 *
 * Bluepad32's bit layout already matches the USB HID mouse button order,
 * so this is a direct pass-through of the low 5 bits -- kept as a named
 * function for clarity and to isolate callers from that assumption.
 */
uint8_t bt_mouse_map_buttons(uint16_t bp_buttons);

/**
 * Convert a Bluepad32-sourced mouse sample into the USB wire-format report.
 *
 * @param delta_x       Relative X motion (uni_mouse_t.delta_x).
 * @param delta_y       Relative Y motion (uni_mouse_t.delta_y).
 * @param bp_buttons    Bluepad32 button bitmask (uni_mouse_t.buttons).
 * @param scroll_wheel  Vertical scroll delta (uni_mouse_t.scroll_wheel).
 * @param out           Output: filled with the USB wire-format report.
 */
void bt_mouse_convert_report(int32_t delta_x, int32_t delta_y,
                              uint16_t bp_buttons, int32_t scroll_wheel,
                              usb_mouse_report_t *out);

#endif /* BT_MOUSE_CONVERT_H */
