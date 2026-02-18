#ifndef BT_GAMEPAD_H
#define BT_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>
#include "gamepad.h"

/**
 * Bluetooth Gamepad Interface
 *
 * Manages Bluetooth Classic and BLE gamepad connections via Bluepad32.
 * Supports Xbox, PlayStation, Switch Pro, 8BitDo, and generic BT gamepads.
 *
 * Thread safety: Bluepad32 callbacks run on its internal task. This module
 * uses a spinlock to protect the shared report data so the main loop can
 * safely read it.
 */

/** Maximum simultaneous Bluetooth gamepads (1 keeps things simple). */
#define BT_GAMEPAD_MAX 1

typedef enum {
    BT_GAMEPAD_DISCONNECTED,
    BT_GAMEPAD_CONNECTED,
} bt_gamepad_state_t;

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
 * Check if a gamepad is connected at the given slot.
 */
bool bt_gamepad_is_connected(uint8_t idx);

/**
 * Get the latest gamepad report for the given slot.
 *
 * @param idx     Gamepad slot (0-based, must be < BT_GAMEPAD_MAX).
 * @param report  Output: filled with current controller state.
 * @return true if a connected gamepad provided data; false if no gamepad.
 */
bool bt_gamepad_get_report(uint8_t idx, gamepad_report_t *report);

/**
 * Enable or disable discovery of new Bluetooth controllers.
 *
 * @param enabled  true to accept new pairings, false to stop scanning.
 */
void bt_gamepad_set_pairing(bool enabled);

#endif /* BT_GAMEPAD_H */
