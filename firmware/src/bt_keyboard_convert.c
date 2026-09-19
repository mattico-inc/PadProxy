#include "bt_keyboard_convert.h"
#include <string.h>

uint8_t bt_keyboard_map_modifiers(uint8_t bp_modifiers)
{
    /* Bluepad32's modifier bit layout matches USB HID exactly. */
    return bp_modifiers;
}

void bt_keyboard_convert_report(uint8_t modifiers,
                                 const uint8_t *pressed_keys,
                                 size_t pressed_count,
                                 usb_keyboard_report_t *out)
{
    memset(out, 0, sizeof(*out));
    out->modifier = bt_keyboard_map_modifiers(modifiers);
    out->reserved = 0;

    if (!pressed_keys)
        return;

    size_t n = pressed_count;
    if (n > USB_KEYBOARD_MAX_KEYS)
        n = USB_KEYBOARD_MAX_KEYS;

    for (size_t i = 0; i < n; i++) {
        out->keycode[i] = pressed_keys[i];
    }
}
