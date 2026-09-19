#ifndef BT_GAMEPAD_H
#define BT_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>
#include "gamepad.h"
#include "usb_hid_report.h"

/**
 * Bluetooth HID Interface
 *
 * Manages Bluetooth Classic and BLE connections via Bluepad32, for
 * gamepads, mice, and keyboards alike (Bluepad32 delivers all three
 * device classes through the same connect/disconnect/data callbacks).
 * Supports Xbox, PlayStation, Switch Pro, 8BitDo, generic BT gamepads,
 * and standard BT/BLE mice and keyboards. Balance boards are not
 * supported and are silently ignored.
 *
 * Thread safety: Bluepad32 callbacks run on its internal task. This module
 * uses a spinlock to protect the shared report data so the main loop can
 * safely read it.
 */

/**
 * Maximum simultaneous Bluetooth HID connections (mixed device classes,
 * e.g. a gamepad + a keyboard + a mouse at once). Matches
 * CONFIG_BLUEPAD32_MAX_DEVICES in src/sdkconfig.h -- Bluepad32 itself
 * doesn't support tracking more devices than that.
 */
#define BT_DEVICE_MAX 4

typedef enum {
    BT_GAMEPAD_DISCONNECTED,
    BT_GAMEPAD_CONNECTED,
} bt_gamepad_state_t;

/** Device class currently occupying a slot. */
typedef enum {
    BT_DEVICE_CLASS_NONE,
    BT_DEVICE_CLASS_GAMEPAD,
    BT_DEVICE_CLASS_MOUSE,
    BT_DEVICE_CLASS_KEYBOARD,
} bt_device_class_t;

/**
 * Callback for gamepad connection/disconnection events.
 *
 * @param idx    Gamepad slot index (0-based).
 * @param state  New connection state.
 */
typedef void (*bt_gamepad_event_cb_t)(uint8_t idx, bt_gamepad_state_t state);

/**
 * Initialize the Bluetooth gamepad subsystem.
 *
 * Registers the PadProxy platform with Bluepad32, starts the BT stack,
 * and begins scanning for controllers.
 *
 * @param event_cb  Connection/disconnection callback (may be NULL).
 */
void bt_gamepad_init(bt_gamepad_event_cb_t event_cb);

/**
 * Check if any Bluetooth HID device (gamepad, mouse, or keyboard) is
 * connected at the given slot.
 */
bool bt_gamepad_is_connected(uint8_t idx);

/**
 * Get the device class occupying a slot.
 * @return BT_DEVICE_CLASS_NONE if the slot is disconnected or holds an
 *         unsupported device class (e.g. a balance board).
 */
bt_device_class_t bt_gamepad_get_class(uint8_t idx);

/**
 * Get the latest gamepad report for the given slot.
 *
 * @param idx     Slot (0-based, must be < BT_DEVICE_MAX).
 * @param report  Output: filled with current controller state (may be NULL
 *                to just check connectivity/class).
 * @return true if the slot holds a connected gamepad; false otherwise.
 */
bool bt_gamepad_get_report(uint8_t idx, gamepad_report_t *report);

/**
 * Get the latest mouse report for the given slot, in USB wire format.
 *
 * @param idx     Slot (0-based, must be < BT_DEVICE_MAX).
 * @param report  Output: filled with current mouse state (may be NULL to
 *                just check connectivity/class).
 * @return true if the slot holds a connected mouse; false otherwise.
 */
bool bt_gamepad_get_mouse_report(uint8_t idx, usb_mouse_report_t *report);

/**
 * Get the latest keyboard report for the given slot, in USB wire format.
 *
 * @param idx     Slot (0-based, must be < BT_DEVICE_MAX).
 * @param report  Output: filled with current keyboard state (may be NULL
 *                to just check connectivity/class).
 * @return true if the slot holds a connected keyboard; false otherwise.
 */
bool bt_gamepad_get_keyboard_report(uint8_t idx, usb_keyboard_report_t *report);

/**
 * Enable or disable discovery of new Bluetooth controllers.
 *
 * @param enabled  true to accept new pairings, false to stop scanning.
 */
void bt_gamepad_set_pairing(bool enabled);

/**
 * Disconnect a connected Bluetooth HID device.
 *
 * Called when the PC powers off to save battery and allow the device to
 * pair elsewhere. It will auto-sleep after disconnect.
 *
 * @param idx  Slot (0-based, must be < BT_DEVICE_MAX).
 */
void bt_gamepad_disconnect(uint8_t idx);

#endif /* BT_GAMEPAD_H */
