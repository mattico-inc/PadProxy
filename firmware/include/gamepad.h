#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Common gamepad report structure.
 *
 * This is the canonical representation of gamepad state shared between the
 * Bluetooth input side (Bluepad32) and the USB output side (TinyUSB HID).
 * All values use full-range integers so conversion to wire formats is a
 * simple scale operation.
 */

/* Button bitmask definitions (Xbox-style naming) */
#define GAMEPAD_BTN_A         (1 << 0)
#define GAMEPAD_BTN_B         (1 << 1)
#define GAMEPAD_BTN_X         (1 << 2)
#define GAMEPAD_BTN_Y         (1 << 3)
#define GAMEPAD_BTN_L1        (1 << 4)   /* Left bumper / LB */
#define GAMEPAD_BTN_R1        (1 << 5)   /* Right bumper / RB */
#define GAMEPAD_BTN_L3        (1 << 6)   /* Left stick click */
#define GAMEPAD_BTN_R3        (1 << 7)   /* Right stick click */
#define GAMEPAD_BTN_START     (1 << 8)   /* Start / Options / Menu */
#define GAMEPAD_BTN_SELECT    (1 << 9)   /* Back / Share / View */
#define GAMEPAD_BTN_GUIDE     (1 << 10)  /* Home / PS / Xbox */
#define GAMEPAD_BTN_MISC      (1 << 11)  /* Touchpad click, Capture, etc. */

/* D-pad as a hat switch (0=N, clockwise, 8=centered/released) */
#define GAMEPAD_DPAD_UP         0
#define GAMEPAD_DPAD_UP_RIGHT   1
#define GAMEPAD_DPAD_RIGHT      2
#define GAMEPAD_DPAD_DOWN_RIGHT 3
#define GAMEPAD_DPAD_DOWN       4
#define GAMEPAD_DPAD_DOWN_LEFT  5
#define GAMEPAD_DPAD_LEFT       6
#define GAMEPAD_DPAD_UP_LEFT    7
#define GAMEPAD_DPAD_CENTERED   8

typedef struct {
    int16_t lx;         /* Left stick X:  -32768 .. 32767 */
    int16_t ly;         /* Left stick Y:  -32768 .. 32767 */
    int16_t rx;         /* Right stick X: -32768 .. 32767 */
    int16_t ry;         /* Right stick Y: -32768 .. 32767 */
    uint16_t lt;        /* Left trigger:  0 .. 1023 */
    uint16_t rt;        /* Right trigger: 0 .. 1023 */
    uint16_t buttons;   /* Bitmask of GAMEPAD_BTN_* */
    uint8_t dpad;       /* GAMEPAD_DPAD_* value (hat switch) */
} gamepad_report_t;

/**
 * Convert a 4-bit d-pad bitmask (UP=1, DOWN=2, RIGHT=4, LEFT=8) to a
 * hat-switch value (0-7 clockwise from N, 8=centered).
 *
 * This is useful for converting from Bluepad32's d-pad format.
 */
static inline uint8_t gamepad_dpad_to_hat(uint8_t dpad_bits)
{
    /*
     * Index by bitmask [UP|DOWN|RIGHT|LEFT] → hat value.
     * Invalid combos (e.g., UP+DOWN) map to centered.
     */
    static const uint8_t table[16] = {
        /*  0: ----  */ GAMEPAD_DPAD_CENTERED,
        /*  1: U---  */ GAMEPAD_DPAD_UP,
        /*  2: -D--  */ GAMEPAD_DPAD_DOWN,
        /*  3: UD--  */ GAMEPAD_DPAD_CENTERED,  /* invalid */
        /*  4: --R-  */ GAMEPAD_DPAD_RIGHT,
        /*  5: U-R-  */ GAMEPAD_DPAD_UP_RIGHT,
        /*  6: -DR-  */ GAMEPAD_DPAD_DOWN_RIGHT,
        /*  7: UDR-  */ GAMEPAD_DPAD_CENTERED,  /* invalid */
        /*  8: ---L  */ GAMEPAD_DPAD_LEFT,
        /*  9: U--L  */ GAMEPAD_DPAD_UP_LEFT,
        /* 10: -D-L  */ GAMEPAD_DPAD_DOWN_LEFT,
        /* 11: UD-L  */ GAMEPAD_DPAD_CENTERED,  /* invalid */
        /* 12: --RL  */ GAMEPAD_DPAD_CENTERED,  /* invalid */
        /* 13: U-RL  */ GAMEPAD_DPAD_CENTERED,  /* invalid */
        /* 14: -DRL  */ GAMEPAD_DPAD_CENTERED,  /* invalid */
        /* 15: UDRL  */ GAMEPAD_DPAD_CENTERED,  /* invalid */
    };
    return table[dpad_bits & 0x0F];
}

/**
 * Check if the guide/home button is pressed.
 * Used to trigger PC wake from off/sleep states.
 */
static inline bool gamepad_report_guide_pressed(const gamepad_report_t *report)
{
    return (report->buttons & GAMEPAD_BTN_GUIDE) != 0;
}

#endif /* GAMEPAD_H */
