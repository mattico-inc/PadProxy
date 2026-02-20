#ifndef USB_HID_GAMEPAD_H
#define USB_HID_GAMEPAD_H

#include <stdbool.h>
#include "gamepad.h"

/**
 * USB HID Gamepad Device
 *
 * Presents the Pico 2 W as a USB HID gamepad to the host PC using TinyUSB.
 * The native USB peripheral connects directly to one port on the
 * motherboard's internal USB header for low-latency input.
 *
 * USB state transitions feed into the PC power state machine:
 *   mounted   → PC_EVENT_USB_ENUMERATED  (OS is running)
 *   suspended → PC_EVENT_USB_SUSPENDED   (PC entering sleep)
 *   unmounted → PC_EVENT_USB_SUSPENDED   (PC shut down)
 */

typedef enum {
    USB_HID_NOT_MOUNTED,
    USB_HID_MOUNTED,
    USB_HID_SUSPENDED,
} usb_hid_state_t;

/**
 * Callback for USB device state changes.
 */
typedef void (*usb_hid_state_cb_t)(usb_hid_state_t state);

/**
 * Initialize the USB HID gamepad device.
 *
 * Configures the TinyUSB stack with our gamepad HID descriptor and
 * installs the USB driver on the RP2350 native USB peripheral.
 *
 * @param state_cb  Callback for mount/suspend/resume events (may be NULL).
 */
void usb_hid_gamepad_init(usb_hid_state_cb_t state_cb);

/**
 * Process TinyUSB device events. Call from the main loop.
 */
void usb_hid_gamepad_task(void);

/**
 * Send a gamepad report to the host PC.
 *
 * Converts the report to USB HID wire format and queues it for
 * transmission.  Drops the report silently if USB is not mounted
 * or the previous report has not finished sending.
 *
 * @param report  Current gamepad state.
 * @return true if the report was queued, false if dropped.
 */
bool usb_hid_gamepad_send_report(const gamepad_report_t *report);

/**
 * Get the current USB connection state.
 */
usb_hid_state_t usb_hid_gamepad_get_state(void);

#endif /* USB_HID_GAMEPAD_H */
