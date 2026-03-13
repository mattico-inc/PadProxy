#include "bt_gamepad_convert.h"

int16_t bt_gamepad_scale_axis(int32_t value)
{
    int32_t scaled = value * 64;
    if (scaled >  32767)  scaled =  32767;
    if (scaled < -32768)  scaled = -32768;
    return (int16_t)scaled;
}

uint16_t bt_gamepad_clamp_trigger(int32_t value)
{
    if (value < 0)    return 0;
    if (value > 1023) return 1023;
    return (uint16_t)value;
}
