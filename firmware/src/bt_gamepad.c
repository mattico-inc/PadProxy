#include "bt_gamepad.h"

/*
 * Bluepad32 Pico W integration.
 *
 * Bluepad32 uses a "platform" callback model: we register a uni_platform_t
 * and receive callbacks when controllers connect, disconnect, or send data.
 * Data arrives on Bluepad32's internal task, so we copy it under a critical
 * section for the main loop to read safely.
 *
 * Bluepad32 delivers gamepads, mice, and keyboards through the same
 * callbacks (distinguished by uni_controller_t.klass); this module tracks
 * up to BT_DEVICE_MAX simultaneous connections of any mix of those classes.
 * Balance boards are not supported and are silently ignored.
 *
 * Slot indices here are the same indices Bluepad32 itself uses internally
 * (uni_hid_device_get_idx_for_instance()), so a slot maps 1:1 onto a
 * Bluepad32 device instance.
 */

#include <string.h>
#include <uni.h>
#include "pico/critical_section.h"
#include "bt_gamepad_convert.h"
#include "bt_mouse_convert.h"
#include "bt_keyboard_convert.h"

/* ── Shared state ────────────────────────────────────────────────────── */

static bt_gamepad_event_cb_t s_event_cb;
static bt_device_class_t     s_class[BT_DEVICE_MAX];
static gamepad_report_t      s_gamepad_reports[BT_DEVICE_MAX];
static usb_mouse_report_t    s_mouse_reports[BT_DEVICE_MAX];
static usb_keyboard_report_t s_keyboard_reports[BT_DEVICE_MAX];
static bool                  s_connected[BT_DEVICE_MAX];
static critical_section_t    s_lock;

/* ── Helpers: Bluepad32 → gamepad_report_t conversion ────────────────── */

/**
 * Map Bluepad32 button bits to our GAMEPAD_BTN_* bitmask.
 *
 * Bluepad32 splits buttons into "buttons" (face/shoulder) and
 * "misc_buttons" (guide/start/select), so we merge both.
 */
static uint16_t map_buttons(uint32_t bp_buttons, uint32_t bp_misc)
{
    uint16_t out = 0;

    if (bp_buttons & BUTTON_A)          out |= GAMEPAD_BTN_A;
    if (bp_buttons & BUTTON_B)          out |= GAMEPAD_BTN_B;
    if (bp_buttons & BUTTON_X)          out |= GAMEPAD_BTN_X;
    if (bp_buttons & BUTTON_Y)          out |= GAMEPAD_BTN_Y;
    if (bp_buttons & BUTTON_SHOULDER_L) out |= GAMEPAD_BTN_L1;
    if (bp_buttons & BUTTON_SHOULDER_R) out |= GAMEPAD_BTN_R1;
    if (bp_buttons & BUTTON_THUMB_L)    out |= GAMEPAD_BTN_L3;
    if (bp_buttons & BUTTON_THUMB_R)    out |= GAMEPAD_BTN_R3;

    if (bp_misc & MISC_BUTTON_START)    out |= GAMEPAD_BTN_START;
    if (bp_misc & MISC_BUTTON_SELECT)   out |= GAMEPAD_BTN_SELECT;
    if (bp_misc & MISC_BUTTON_SYSTEM)   out |= GAMEPAD_BTN_GUIDE;

    return out;
}

static void convert_report(const uni_gamepad_t *gp, gamepad_report_t *out)
{
    out->lx = bt_gamepad_scale_axis(gp->axis_x);
    out->ly = bt_gamepad_scale_axis(gp->axis_y);
    out->rx = bt_gamepad_scale_axis(gp->axis_rx);
    out->ry = bt_gamepad_scale_axis(gp->axis_ry);
    out->lt = bt_gamepad_clamp_trigger(gp->brake);
    out->rt = bt_gamepad_clamp_trigger(gp->throttle);
    out->buttons = map_buttons(gp->buttons, gp->misc_buttons);
    out->dpad = gamepad_dpad_to_hat(gp->dpad);
}

/* ── Slot bookkeeping ────────────────────────────────────────────────── */

/** True if at least one slot is free for a new connection. */
static bool has_free_slot(void)
{
    bool free_slot = false;

    critical_section_enter_blocking(&s_lock);
    for (int i = 0; i < BT_DEVICE_MAX; i++) {
        if (!s_connected[i]) {
            free_slot = true;
            break;
        }
    }
    critical_section_exit(&s_lock);

    return free_slot;
}

/* ── Bluepad32 platform callbacks ────────────────────────────────────── */

static void platform_init(int argc, const char **argv)
{
    (void)argc;
    (void)argv;
    /* Nothing to do; our init happens in bt_gamepad_init(). */
}

static void platform_on_init_complete(void)
{
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

static void platform_on_device_connected(uni_hid_device_t *d)
{
    (void)d;
}

static void platform_on_device_disconnected(uni_hid_device_t *d)
{
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= BT_DEVICE_MAX)
        return;

    critical_section_enter_blocking(&s_lock);
    s_connected[idx] = false;
    s_class[idx] = BT_DEVICE_CLASS_NONE;
    memset(&s_gamepad_reports[idx], 0, sizeof(s_gamepad_reports[idx]));
    s_gamepad_reports[idx].dpad = GAMEPAD_DPAD_CENTERED;
    memset(&s_mouse_reports[idx], 0, sizeof(s_mouse_reports[idx]));
    memset(&s_keyboard_reports[idx], 0, sizeof(s_keyboard_reports[idx]));
    critical_section_exit(&s_lock);

    if (s_event_cb) {
        s_event_cb((uint8_t)idx, BT_GAMEPAD_DISCONNECTED);
    }

    /* A slot freed up -- resume scanning for new controllers. */
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

static uni_error_t platform_on_device_ready(uni_hid_device_t *d)
{
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= BT_DEVICE_MAX)
        return UNI_ERROR_SUCCESS;

    bt_device_class_t klass = BT_DEVICE_CLASS_NONE;
    if (uni_hid_device_is_gamepad(d)) {
        klass = BT_DEVICE_CLASS_GAMEPAD;
    } else if (uni_hid_device_is_mouse(d)) {
        klass = BT_DEVICE_CLASS_MOUSE;
    } else if (uni_hid_device_is_keyboard(d)) {
        klass = BT_DEVICE_CLASS_KEYBOARD;
    }

    critical_section_enter_blocking(&s_lock);
    s_connected[idx] = true;
    s_class[idx] = klass;
    critical_section_exit(&s_lock);

    if (s_event_cb) {
        s_event_cb((uint8_t)idx, BT_GAMEPAD_CONNECTED);
    }

    /* Keep scanning until every slot is full. */
    if (!has_free_slot()) {
        uni_bt_stop_scanning_unsafe();
    }

    return UNI_ERROR_SUCCESS;
}

static void platform_on_controller_data(uni_hid_device_t *d,
                                         uni_controller_t *ctl)
{
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= BT_DEVICE_MAX)
        return;

    switch (ctl->klass) {
    case UNI_CONTROLLER_CLASS_GAMEPAD: {
        gamepad_report_t report;
        convert_report(&ctl->gamepad, &report);
        critical_section_enter_blocking(&s_lock);
        s_gamepad_reports[idx] = report;
        critical_section_exit(&s_lock);
        break;
    }
    case UNI_CONTROLLER_CLASS_MOUSE: {
        usb_mouse_report_t report;
        bt_mouse_convert_report(ctl->mouse.delta_x, ctl->mouse.delta_y,
                                 ctl->mouse.buttons, ctl->mouse.scroll_wheel,
                                 &report);
        critical_section_enter_blocking(&s_lock);
        s_mouse_reports[idx] = report;
        critical_section_exit(&s_lock);
        break;
    }
    case UNI_CONTROLLER_CLASS_KEYBOARD: {
        usb_keyboard_report_t report;
        bt_keyboard_convert_report(ctl->keyboard.modifiers,
                                    ctl->keyboard.pressed_keys,
                                    UNI_KEYBOARD_PRESSED_KEYS_MAX, &report);
        critical_section_enter_blocking(&s_lock);
        s_keyboard_reports[idx] = report;
        critical_section_exit(&s_lock);
        break;
    }
    default:
        /* Balance boards and anything else: unsupported, ignore. */
        break;
    }
}

static const uni_property_t *platform_get_property(uni_property_idx_t idx)
{
    (void)idx;
    /* No custom properties; Bluepad32 defaults are fine. */
    return NULL;
}

static void platform_on_oob_event(uni_platform_oob_event_t event, void *data)
{
    (void)event;
    (void)data;
}

/* ── Platform registration ───────────────────────────────────────────── */

static struct uni_platform s_platform = {
    .name                   = "PadProxy",
    .init                   = platform_init,
    .on_init_complete       = platform_on_init_complete,
    .on_device_connected    = platform_on_device_connected,
    .on_device_disconnected = platform_on_device_disconnected,
    .on_device_ready        = platform_on_device_ready,
    .on_controller_data     = platform_on_controller_data,
    .get_property           = platform_get_property,
    .on_oob_event           = platform_on_oob_event,
};

/* ── Public API ──────────────────────────────────────────────────────── */

void bt_gamepad_init(bt_gamepad_event_cb_t event_cb)
{
    s_event_cb = event_cb;

    critical_section_init(&s_lock);

    for (int i = 0; i < BT_DEVICE_MAX; i++) {
        s_connected[i] = false;
        s_class[i] = BT_DEVICE_CLASS_NONE;
        memset(&s_gamepad_reports[i], 0, sizeof(s_gamepad_reports[i]));
        s_gamepad_reports[i].dpad = GAMEPAD_DPAD_CENTERED;
        memset(&s_mouse_reports[i], 0, sizeof(s_mouse_reports[i]));
        memset(&s_keyboard_reports[i], 0, sizeof(s_keyboard_reports[i]));
    }

    uni_platform_set_custom(&s_platform);
    uni_init(0, NULL);
}

bool bt_gamepad_is_connected(uint8_t idx)
{
    if (idx >= BT_DEVICE_MAX)
        return false;

    bool connected;
    critical_section_enter_blocking(&s_lock);
    connected = s_connected[idx];
    critical_section_exit(&s_lock);
    return connected;
}

bt_device_class_t bt_gamepad_get_class(uint8_t idx)
{
    if (idx >= BT_DEVICE_MAX)
        return BT_DEVICE_CLASS_NONE;

    critical_section_enter_blocking(&s_lock);
    bt_device_class_t klass = s_connected[idx] ? s_class[idx] : BT_DEVICE_CLASS_NONE;
    critical_section_exit(&s_lock);
    return klass;
}

bool bt_gamepad_get_report(uint8_t idx, gamepad_report_t *report)
{
    if (idx >= BT_DEVICE_MAX)
        return false;

    critical_section_enter_blocking(&s_lock);
    bool ok = s_connected[idx] && s_class[idx] == BT_DEVICE_CLASS_GAMEPAD;
    if (ok && report) {
        *report = s_gamepad_reports[idx];
    }
    critical_section_exit(&s_lock);

    return ok;
}

bool bt_gamepad_get_mouse_report(uint8_t idx, usb_mouse_report_t *report)
{
    if (idx >= BT_DEVICE_MAX)
        return false;

    critical_section_enter_blocking(&s_lock);
    bool ok = s_connected[idx] && s_class[idx] == BT_DEVICE_CLASS_MOUSE;
    if (ok && report) {
        *report = s_mouse_reports[idx];
    }
    critical_section_exit(&s_lock);

    return ok;
}

bool bt_gamepad_get_keyboard_report(uint8_t idx, usb_keyboard_report_t *report)
{
    if (idx >= BT_DEVICE_MAX)
        return false;

    critical_section_enter_blocking(&s_lock);
    bool ok = s_connected[idx] && s_class[idx] == BT_DEVICE_CLASS_KEYBOARD;
    if (ok && report) {
        *report = s_keyboard_reports[idx];
    }
    critical_section_exit(&s_lock);

    return ok;
}

void bt_gamepad_set_pairing(bool enabled)
{
    if (enabled) {
        uni_bt_start_scanning_and_autoconnect_unsafe();
    } else {
        uni_bt_stop_scanning_unsafe();
    }
}

void bt_gamepad_disconnect(uint8_t idx)
{
    if (idx >= BT_DEVICE_MAX)
        return;

    critical_section_enter_blocking(&s_lock);
    bool was_connected = s_connected[idx];
    critical_section_exit(&s_lock);

    if (!was_connected)
        return;

    /*
     * Our slot index is the same index Bluepad32 uses internally
     * (uni_hid_device_get_idx_for_instance()), so we can look the
     * device up directly instead of scanning for "the" connected device.
     */
    uni_hid_device_t *device = uni_hid_device_get_instance_for_idx(idx);
    if (device && uni_bt_conn_is_connected(&device->conn)) {
        uni_hid_device_disconnect(device);
        /* platform_on_device_disconnected() will be called by Bluepad32 */
    }
}
