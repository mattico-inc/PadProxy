/*
 * TinyUSB configuration for PadProxy on Raspberry Pi Pico 2 W.
 *
 * USB composite device: 3 HID interfaces (gamepad, mouse, keyboard) +
 * CDC serial (setup interface). Each HID interface has its own report
 * descriptor and endpoint, matching the corresponding Bluetooth device
 * class forwarded from Bluepad32.
 * The CDC interface provides a virtual serial port for device setup
 * via Chrome Web Serial or any terminal emulator.
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

/* ── Board / MCU ─────────────────────────────────────────────────────── */
/* Handled by the Pico SDK; no explicit MCU define needed here. */

/* ── USB mode ────────────────────────────────────────────────────────── */
#define CFG_TUSB_RHPORT0_MODE    OPT_MODE_DEVICE

/* ── Device class enables ────────────────────────────────────────────── */
#define CFG_TUD_HID     3   /* gamepad, mouse, keyboard */
#define CFG_TUD_CDC     1
#define CFG_TUD_MSC     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

/* ── Endpoint / buffer sizes ─────────────────────────────────────────── */
#define CFG_TUD_ENDPOINT0_SIZE   64
#define CFG_TUD_HID_EP_BUFSIZE   64
#define CFG_TUD_CDC_RX_BUFSIZE   256
#define CFG_TUD_CDC_TX_BUFSIZE   256

#endif /* TUSB_CONFIG_H */
