#include "bt_gamepad.h"

/*
 * Bluepad32 ESP-IDF integration.
 *
 * Bluepad32 uses a "platform" callback model: we register a uni_platform_t
 * and receive callbacks when controllers connect, disconnect, or send data.
 * Data arrives on Bluepad32's internal task, so we copy it under a spinlock
 * for the main loop to read safely.
 */

#include <string.h>
#include <uni.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

/* ── Shared state ────────────────────────────────────────────────────── */

static bt_gamepad_event_cb_t s_event_cb;
static gamepad_report_t      s_reports[BT_GAMEPAD_MAX];
static bool                  s_connected[BT_GAMEPAD_MAX];
static portMUX_TYPE          s_lock = portMUX_INITIALIZER_UNLOCKED;

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

    if (bp_buttons & UNI_GAMEPAD_BUTTON_A)          out |= GAMEPAD_BTN_A;
    if (bp_buttons & UNI_GAMEPAD_BUTTON_B)          out |= GAMEPAD_BTN_B;
    if (bp_buttons & UNI_GAMEPAD_BUTTON_X)          out |= GAMEPAD_BTN_X;
    if (bp_buttons & UNI_GAMEPAD_BUTTON_Y)          out |= GAMEPAD_BTN_Y;
    if (bp_buttons & UNI_GAMEPAD_BUTTON_SHOULDER_L) out |= GAMEPAD_BTN_L1;
    if (bp_buttons & UNI_GAMEPAD_BUTTON_SHOULDER_R) out |= GAMEPAD_BTN_R1;
    if (bp_buttons & UNI_GAMEPAD_BUTTON_THUMB_L)    out |= GAMEPAD_BTN_L3;
    if (bp_buttons & UNI_GAMEPAD_BUTTON_THUMB_R)    out |= GAMEPAD_BTN_R3;

    if (bp_misc & UNI_GAMEPAD_MISC_BUTTON_START)    out |= GAMEPAD_BTN_START;
    if (bp_misc & UNI_GAMEPAD_MISC_BUTTON_SELECT)   out |= GAMEPAD_BTN_SELECT;
    if (bp_misc & UNI_GAMEPAD_MISC_BUTTON_SYSTEM)   out |= GAMEPAD_BTN_GUIDE;
    if (bp_misc & UNI_GAMEPAD_MISC_BUTTON_HOME)     out |= GAMEPAD_BTN_GUIDE;

    return out;
}

/**
 * Scale Bluepad32 axis value (-512..511) to int16_t (-32768..32767).
 */
static int16_t scale_axis(int32_t value)
{
    int32_t scaled = value * 64;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;
    return (int16_t)scaled;
}

/**
 * Clamp a trigger value to 0..1023.
 */
static uint16_t clamp_trigger(int32_t value)
{
    if (value < 0) return 0;
    if (value > 1023) return 1023;
    return (uint16_t)value;
}

static void convert_report(const uni_gamepad_t *gp, gamepad_report_t *out)
{
    out->lx = scale_axis(gp->axis_x);
    out->ly = scale_axis(gp->axis_y);
    out->rx = scale_axis(gp->axis_rx);
    out->ry = scale_axis(gp->axis_ry);
    out->lt = clamp_trigger(gp->brake);
    out->rt = clamp_trigger(gp->throttle);
    out->buttons = map_buttons(gp->buttons, gp->misc_buttons);
    out->dpad = gamepad_dpad_to_hat(gp->dpad);
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
    uni_bt_enable_new_connections_unsafe(true);
}

static void platform_on_device_connected(uni_hid_device_t *d)
{
    (void)d;
}

static void platform_on_device_disconnected(uni_hid_device_t *d)
{
    (void)d;
    /*
     * Mark slot 0 as disconnected. With BT_GAMEPAD_MAX == 1 we only
     * track one controller.
     */
    portENTER_CRITICAL(&s_lock);
    s_connected[0] = false;
    memset(&s_reports[0], 0, sizeof(s_reports[0]));
    s_reports[0].dpad = GAMEPAD_DPAD_CENTERED;
    portEXIT_CRITICAL(&s_lock);

    if (s_event_cb) {
        s_event_cb(0, BT_GAMEPAD_DISCONNECTED);
    }

    /* Re-enable scanning so another controller can connect. */
    uni_bt_enable_new_connections_unsafe(true);
}

static uni_error_t platform_on_device_ready(uni_hid_device_t *d)
{
    (void)d;

    portENTER_CRITICAL(&s_lock);
    s_connected[0] = true;
    portEXIT_CRITICAL(&s_lock);

    if (s_event_cb) {
        s_event_cb(0, BT_GAMEPAD_CONNECTED);
    }

    /* Stop scanning once we have a controller. */
    uni_bt_enable_new_connections_unsafe(false);

    return UNI_ERROR_SUCCESS;
}

static void platform_on_controller_data(uni_hid_device_t *d,
                                         uni_controller_t *ctl)
{
    (void)d;

    if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD)
        return;

    gamepad_report_t report;
    convert_report(&ctl->gamepad, &report);

    portENTER_CRITICAL(&s_lock);
    s_reports[0] = report;
    portEXIT_CRITICAL(&s_lock);
}

static int32_t platform_get_property(uni_platform_property_t key)
{
    /* Only relevant property: max # of devices. */
    if (key == UNI_PLATFORM_PROPERTY_DELETE_STORED_KEYS)
        return 0;
    return 0;
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

    for (int i = 0; i < BT_GAMEPAD_MAX; i++) {
        s_connected[i] = false;
        memset(&s_reports[i], 0, sizeof(s_reports[i]));
        s_reports[i].dpad = GAMEPAD_DPAD_CENTERED;
    }

    uni_platform_set_custom(&s_platform);
    uni_init(0, NULL);
}

bool bt_gamepad_is_connected(uint8_t idx)
{
    if (idx >= BT_GAMEPAD_MAX)
        return false;

    bool connected;
    portENTER_CRITICAL(&s_lock);
    connected = s_connected[idx];
    portEXIT_CRITICAL(&s_lock);
    return connected;
}

bool bt_gamepad_get_report(uint8_t idx, gamepad_report_t *report)
{
    if (idx >= BT_GAMEPAD_MAX)
        return false;

    portENTER_CRITICAL(&s_lock);
    bool connected = s_connected[idx];
    if (connected) {
        *report = s_reports[idx];
    }
    portEXIT_CRITICAL(&s_lock);

    return connected;
}

void bt_gamepad_set_pairing(bool enabled)
{
    uni_bt_enable_new_connections_unsafe(enabled);
}
