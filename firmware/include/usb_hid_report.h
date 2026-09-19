#ifndef USB_HID_REPORT_H
#define USB_HID_REPORT_H

#include <stdint.h>
#include "gamepad.h"

/**
 * USB HID Gamepad Report
 *
 * This module contains the USB HID report descriptor and the packed wire
 * struct that matches it.  The conversion function is pure logic with no
 * hardware dependencies so it can be unit-tested on the host.
 *
 * Report layout (13 bytes):
 *   Offset  Size  Field
 *   0       2     Left stick X  (int16, -32768..32767)
 *   2       2     Left stick Y  (int16)
 *   4       2     Right stick X (int16)
 *   6       2     Right stick Y (int16)
 *   8       1     Left trigger  (uint8, 0..255)
 *   9       1     Right trigger (uint8, 0..255)
 *   10      1     Hat switch (4 bits, 1-8 clockwise + null) + padding (4 bits)
 *   11      2     Buttons (16 bits)
 *
 * The HID descriptor uses:
 *   - Generic Desktop / Gamepad
 *   - X, Y axes for left stick; Rx, Ry for right stick
 *   - Z, Rz for triggers
 *   - Hat Switch for d-pad
 *   - 16 buttons
 */

/** Wire-format report sent to the USB host. Must match the HID descriptor. */
typedef struct __attribute__((packed)) {
    int16_t  lx;        /* Left stick X */
    int16_t  ly;        /* Left stick Y */
    int16_t  rx;        /* Right stick X */
    int16_t  ry;        /* Right stick Y */
    uint8_t  lt;        /* Left trigger  (0..255) */
    uint8_t  rt;        /* Right trigger (0..255) */
    uint8_t  hat;       /* Hat switch (low nibble: 1-8 or 0=centered) */
    uint16_t buttons;   /* 16 buttons */
} usb_gamepad_report_t;

/**
 * Convert a gamepad_report_t to the USB wire format.
 *
 * Scaling:
 *   - Stick axes: copied 1:1 (both use int16 full range).
 *   - Triggers:   10-bit (0..1023) → 8-bit (0..255), divide by 4.
 *   - Hat:        our 0-8 (0=N, 8=centered) → USB 1-8 (1=N, 0=centered).
 *   - Buttons:    copied 1:1.
 */
void usb_hid_report_from_gamepad(const gamepad_report_t *in,
                                  usb_gamepad_report_t *out);

/** Size of the HID report descriptor in bytes (compile-time constant). */
#define USB_HID_REPORT_DESCRIPTOR_LEN 91

/** The HID report descriptor. */
extern const uint8_t usb_hid_report_descriptor[USB_HID_REPORT_DESCRIPTOR_LEN];

/**
 * USB HID Mouse Report (5 bytes)
 *
 * Matches TinyUSB's TUD_HID_REPORT_DESC_MOUSE() template: standard
 * boot-compatible relative mouse layout plus a vertical wheel and a
 * horizontal pan byte (always 0; Bluepad32 mice don't report pan).
 */
typedef struct __attribute__((packed)) {
    uint8_t buttons; /* bit0=left, bit1=right, bit2=middle, bit3=backward, bit4=forward */
    int8_t  x;       /* Relative X movement, -127..127 */
    int8_t  y;       /* Relative Y movement, -127..127 */
    int8_t  wheel;   /* Vertical scroll delta, -127..127 */
    int8_t  pan;     /* Horizontal scroll delta; unused, always 0 */
} usb_mouse_report_t;

/** Maximum simultaneously-reported keys in the boot-protocol keyboard report. */
#define USB_KEYBOARD_MAX_KEYS 6

/**
 * USB HID Keyboard Report (8 bytes)
 *
 * Standard HID boot-protocol keyboard report: modifier byte, reserved
 * byte, and up to 6 simultaneously pressed keys (6-key rollover).
 * Keycodes are standard USB HID keyboard usage IDs.
 */
typedef struct __attribute__((packed)) {
    uint8_t modifier;                       /* KEYBOARD_MODIFIER_* bitmask */
    uint8_t reserved;                       /* Always 0 */
    uint8_t keycode[USB_KEYBOARD_MAX_KEYS]; /* USB HID keyboard usage IDs */
} usb_keyboard_report_t;

#endif /* USB_HID_REPORT_H */
