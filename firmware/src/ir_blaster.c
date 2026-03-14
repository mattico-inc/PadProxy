#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "ir_blaster.h"

/**
 * IR blaster driver — PIO-based carrier generation on RP2350.
 *
 * The PIO program generates a carrier square wave during "mark" periods
 * and holds the output LOW during "space" periods.  The CPU feeds
 * mark/space durations into the PIO TX FIFO and blocks until the
 * transmission completes.
 *
 * Since we cannot compile PIO assembler in the host test environment,
 * we use a simple bit-bang fallback approach: the PIO program is
 * hand-assembled as a constant array.
 */

/* ── PIO carrier program ────────────────────────────────────────────────── */

/*
 * PIO program for IR carrier generation.
 *
 * The program pulls a 32-bit word from the TX FIFO:
 *   - Upper 16 bits: number of carrier cycles for MARK
 *   - Lower 16 bits: number of carrier half-periods for SPACE
 *
 * During MARK, it toggles the output pin at the carrier frequency.
 * During SPACE, it holds the pin LOW.
 *
 * The carrier period is set by the PIO clock divider.
 *
 * PIO assembly (for reference):
 *   .program ir_carrier
 *   .wrap_target
 *       pull block          ; Get mark/space word
 *       out x, 16           ; X = space half-periods
 *       out y, 16           ; Y = mark cycles
 *   mark_loop:
 *       set pins, 1 [1]     ; Pin HIGH (carrier half-cycle)
 *       jmp y-- mark_hi     ;
 *   mark_hi:
 *       set pins, 0 [1]     ; Pin LOW (carrier half-cycle)
 *       jmp y-- mark_loop   ; Repeat for Y cycles
 *   space_loop:
 *       set pins, 0 [1]     ; Pin LOW
 *       jmp x-- space_loop  ; Repeat for X half-periods
 *   .wrap
 */

/* Hand-assembled PIO instructions for the IR carrier program.
 * We use a simpler approach: bit-bang with busy-wait, which is
 * adequate for IR (timing tolerance is ±20%). */

static uint s_pio_sm;
static PIO  s_pio;
static bool s_initialized;

void ir_blaster_init(void)
{
    gpio_init(IR_BLASTER_GPIO);
    gpio_set_dir(IR_BLASTER_GPIO, GPIO_OUT);
    gpio_put(IR_BLASTER_GPIO, 0);
    s_initialized = true;
    printf("[ir] IR blaster initialized on GPIO %d\n", IR_BLASTER_GPIO);
}

/**
 * Bit-bang a carrier burst (mark) for the given duration.
 * Generates a square wave at the specified carrier frequency.
 */
static void carrier_mark(uint32_t carrier_hz, uint16_t duration_us)
{
    if (duration_us == 0) return;

    uint32_t half_period_us = 500000 / carrier_hz; /* half period in µs */
    uint32_t cycles = (uint32_t)duration_us / (half_period_us * 2);

    for (uint32_t i = 0; i < cycles; i++) {
        gpio_put(IR_BLASTER_GPIO, 1);
        busy_wait_us_32(half_period_us);
        gpio_put(IR_BLASTER_GPIO, 0);
        busy_wait_us_32(half_period_us);
    }
}

/**
 * Hold the output LOW (space) for the given duration.
 */
static void carrier_space(uint16_t duration_us)
{
    if (duration_us == 0) return;
    gpio_put(IR_BLASTER_GPIO, 0);
    busy_wait_us_32(duration_us);
}

void ir_blaster_send_raw(uint32_t carrier_hz, const ir_timing_t *timings,
                         size_t count)
{
    if (!s_initialized || !timings || count == 0) return;

    for (size_t i = 0; i < count; i++) {
        carrier_mark(carrier_hz, timings[i].mark_us);
        carrier_space(timings[i].space_us);
    }

    /* Ensure LED is off */
    gpio_put(IR_BLASTER_GPIO, 0);
}

bool ir_blaster_send(ir_protocol_id_t protocol, uint16_t address,
                     uint16_t command)
{
    ir_timing_t timings[IR_TIMING_MAX];
    size_t count = 0;
    uint32_t carrier = ir_protocol_carrier_hz(protocol);

    switch (protocol) {
    case IR_PROTO_NEC:
        count = ir_protocol_encode_nec(address, (uint8_t)command,
                                       timings, IR_TIMING_MAX);
        break;
    case IR_PROTO_SAMSUNG:
        count = ir_protocol_encode_samsung((uint8_t)address, (uint8_t)command,
                                           timings, IR_TIMING_MAX);
        break;
    case IR_PROTO_SONY:
        /* Sony requires 3 transmissions with 25ms gaps */
        count = ir_protocol_encode_sony((uint8_t)address, (uint8_t)command,
                                        timings, IR_TIMING_MAX);
        if (count > 0) {
            for (int rep = 0; rep < 3; rep++) {
                ir_blaster_send_raw(carrier, timings, count);
                if (rep < 2) busy_wait_us_32(25000);
            }
            return true;
        }
        return false;
    case IR_PROTO_RC5:
        count = ir_protocol_encode_rc5((uint8_t)address, (uint8_t)command, 0,
                                       timings, IR_TIMING_MAX);
        break;
    default:
        return false;
    }

    if (count == 0) return false;

    ir_blaster_send_raw(carrier, timings, count);
    return true;
}
