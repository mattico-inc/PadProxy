/*
 * TinyUSB configuration for PadProxy on Raspberry Pi Pico 2 W.
 *
 * Only the HID device class is enabled — the USB port is dedicated to
 * presenting a gamepad to the host PC.
 */

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

/* ── Board / MCU ─────────────────────────────────────────────────────── */
/* Handled by the Pico SDK; no explicit MCU define needed here. */

/* ── USB mode ────────────────────────────────────────────────────────── */
#define CFG_TUSB_RHPORT0_MODE    OPT_MODE_DEVICE

/* ── Device class enables ────────────────────────────────────────────── */
#define CFG_TUD_HID     1
#define CFG_TUD_CDC     0
#define CFG_TUD_MSC     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

/* ── Endpoint / buffer sizes ─────────────────────────────────────────── */
#define CFG_TUD_ENDPOINT0_SIZE   64
#define CFG_TUD_HID_EP_BUFSIZE   64

#endif /* TUSB_CONFIG_H */
