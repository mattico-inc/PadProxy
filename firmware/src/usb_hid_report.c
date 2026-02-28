#include "usb_hid_report.h"
#include <string.h>

/*
 * USB HID Report Descriptor for a standard gamepad.
 *
 * Presents as a Generic Desktop / Gamepad with:
 *   - 2 analog sticks (X/Y + Rx/Ry), 16-bit signed
 *   - 2 analog triggers (Z/Rz), 8-bit unsigned
 *   - 1 hat switch (d-pad), 4-bit
 *   - 16 buttons
 *
 * This descriptor is designed for broad OS compatibility (Windows DirectInput,
 * Linux evdev, macOS IOKit) without requiring custom drivers.
 */
const uint8_t usb_hid_report_descriptor[USB_HID_REPORT_DESCRIPTOR_LEN] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop) */
    0x09, 0x05,        /* Usage (Gamepad) */
    0xA1, 0x01,        /* Collection (Application) */

    /* ── Analog sticks ──────────────────────────────────── */
    0x05, 0x01,        /*   Usage Page (Generic Desktop) */
    0x09, 0x30,        /*   Usage (X) - left stick X */
    0x09, 0x31,        /*   Usage (Y) - left stick Y */
    0x09, 0x33,        /*   Usage (Rx) - right stick X */
    0x09, 0x34,        /*   Usage (Ry) - right stick Y */
    0x16, 0x00, 0x80,  /*   Logical Minimum (-32768) */
    0x26, 0xFF, 0x7F,  /*   Logical Maximum (32767) */
    0x75, 0x10,        /*   Report Size (16) */
    0x95, 0x04,        /*   Report Count (4) */
    0x81, 0x02,        /*   Input (Data, Variable, Absolute) */

    /* ── Triggers ───────────────────────────────────────── */
    0x09, 0x32,        /*   Usage (Z) - left trigger */
    0x09, 0x35,        /*   Usage (Rz) - right trigger */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255) */
    0x75, 0x08,        /*   Report Size (8) */
    0x95, 0x02,        /*   Report Count (2) */
    0x81, 0x02,        /*   Input (Data, Variable, Absolute) */

    /* ── Hat switch (D-pad) ─────────────────────────────── */
    0x05, 0x01,        /*   Usage Page (Generic Desktop) */
    0x09, 0x39,        /*   Usage (Hat Switch) */
    0x15, 0x01,        /*   Logical Minimum (1) */
    0x25, 0x08,        /*   Logical Maximum (8) */
    0x35, 0x00,        /*   Physical Minimum (0) */
    0x46, 0x3B, 0x01,  /*   Physical Maximum (315) */
    0x65, 0x14,        /*   Unit (Degrees, English Rotation) */
    0x75, 0x04,        /*   Report Size (4) */
    0x95, 0x01,        /*   Report Count (1) */
    0x81, 0x42,        /*   Input (Data, Variable, Absolute, Null State) */

    /* ── Hat padding ────────────────────────────────────── */
    0x75, 0x04,        /*   Report Size (4) */
    0x95, 0x01,        /*   Report Count (1) */
    0x81, 0x01,        /*   Input (Constant) */

    /* ── Reset unit ─────────────────────────────────────── */
    0x45, 0x00,        /*   Physical Maximum (0) - reset */
    0x65, 0x00,        /*   Unit (None) - reset */

    /* ── Buttons ────────────────────────────────────────── */
    0x05, 0x09,        /*   Usage Page (Button) */
    0x19, 0x01,        /*   Usage Minimum (Button 1) */
    0x29, 0x10,        /*   Usage Maximum (Button 16) */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x25, 0x01,        /*   Logical Maximum (1) */
    0x75, 0x01,        /*   Report Size (1) */
    0x95, 0x10,        /*   Report Count (16) */
    0x81, 0x02,        /*   Input (Data, Variable, Absolute) */

    0xC0,              /* End Collection */
};


void usb_hid_report_from_gamepad(const gamepad_report_t *in,
                                  usb_gamepad_report_t *out)
{
    memset(out, 0, sizeof(*out));

    /* Sticks: 1:1 copy (both int16 full range) */
    out->lx = in->lx;
    out->ly = in->ly;
    out->rx = in->rx;
    out->ry = in->ry;

    /* Triggers: 10-bit (0..1023) → 8-bit (0..255) */
    out->lt = (uint8_t)(in->lt >> 2);
    out->rt = (uint8_t)(in->rt >> 2);

    /*
     * Hat switch: our format uses 0=N..7=NW, 8=centered.
     * USB HID hat uses 1=N..8=NW, 0=centered (null state).
     *
     * Conversion: if centered (8) → 0, otherwise value + 1.
     */
    if (in->dpad == GAMEPAD_DPAD_CENTERED) {
        out->hat = 0;
    } else {
        out->hat = in->dpad + 1;
    }

    /* Buttons: 1:1 */
    out->buttons = in->buttons;
}
