#include "bt_mouse_convert.h"

/*
 * Bluepad32's UNI_MOUSE_BUTTON_* bits (uni_mouse.h) are:
 *   LEFT=bit0, RIGHT=bit1, MIDDLE=bit2, AUX_0(back)=bit3, AUX_1(fwd)=bit4
 * which is exactly the USB HID mouse button bit order (left, right,
 * middle, backward, forward), so mapping is a mask of the low 5 bits.
 */
#define BP_MOUSE_BUTTON_MASK 0x1F

int8_t bt_mouse_clamp_delta(int32_t value)
{
    if (value > 127)  return 127;
    if (value < -127) return -127;
    return (int8_t)value;
}

uint8_t bt_mouse_map_buttons(uint16_t bp_buttons)
{
    return (uint8_t)(bp_buttons & BP_MOUSE_BUTTON_MASK);
}

void bt_mouse_convert_report(int32_t delta_x, int32_t delta_y,
                              uint16_t bp_buttons, int32_t scroll_wheel,
                              usb_mouse_report_t *out)
{
    out->buttons = bt_mouse_map_buttons(bp_buttons);
    out->x       = bt_mouse_clamp_delta(delta_x);
    out->y       = bt_mouse_clamp_delta(delta_y);
    out->wheel   = bt_mouse_clamp_delta(scroll_wheel);
    out->pan     = 0;
}
