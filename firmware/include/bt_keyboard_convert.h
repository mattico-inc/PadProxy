#ifndef BT_KEYBOARD_CONVERT_H
#define BT_KEYBOARD_CONVERT_H

#include <stdint.h>
#include <stddef.h>
#include "usb_hid_report.h"

/**
 * Bluepad32 keyboard sample -> USB wire format conversion.
 *
 * Pure logic, no hardware or Bluepad32 header dependencies (mirrors
 * bt_gamepad_convert.h), so it can be unit-tested on the host. Callers
 * pass the raw scalar fields from uni_keyboard_t rather than the struct
 * itself.
 */

/**
 * Map Bluepad32 keyboard modifier bits to the USB HID keyboard modifier
 * byte. Bluepad32's UNI_KEYBOARD_MODIFIER_* bits (uni_keyboard.h) already
 * use the same bit order as the USB HID spec (left ctrl/shift/alt/gui,
 * then right ctrl/shift/alt/gui), so this is currently an identity
 * mapping -- kept as a named function for clarity and future-proofing.
 */
uint8_t bt_keyboard_map_modifiers(uint8_t bp_modifiers);

/**
 * Convert a Bluepad32-sourced keyboard sample into the USB wire-format
 * (6-key rollover boot protocol) report.
 *
 * Bluepad32 reports pressed keys as standard USB HID keyboard usage IDs
 * (see hid_usage.h HID_USAGE_KB_*) in an array of up to
 * UNI_KEYBOARD_PRESSED_KEYS_MAX (10) entries. The boot report only has
 * room for USB_KEYBOARD_MAX_KEYS (6); any keys beyond that are silently
 * dropped, matching the boot report's own 6-key-rollover limit.
 *
 * @param modifiers      Bluepad32 modifier bitmask (uni_keyboard_t.modifiers).
 * @param pressed_keys   Array of pressed-key USB HID usage IDs.
 * @param pressed_count  Number of valid entries in pressed_keys.
 * @param out            Output: filled with the USB wire-format report.
 */
void bt_keyboard_convert_report(uint8_t modifiers,
                                 const uint8_t *pressed_keys,
                                 size_t pressed_count,
                                 usb_keyboard_report_t *out);

#endif /* BT_KEYBOARD_CONVERT_H */
