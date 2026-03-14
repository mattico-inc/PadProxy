#include "ir_protocol.h"

/**
 * IR protocol encoding — pure logic, no hardware dependencies.
 *
 * Each encoder produces an array of mark/space timing pairs that describe
 * the IR waveform.  The final element always has space_us == 0 to signal
 * end-of-transmission.
 *
 * References:
 *   NEC:     https://www.sbprojects.net/knowledge/ir/nec.php
 *   Samsung: https://www.sbprojects.net/knowledge/ir/samsung.php
 *   Sony:    https://www.sbprojects.net/knowledge/ir/sirc.php
 *   RC5:     https://www.sbprojects.net/knowledge/ir/rc5.php
 */

/* ── NEC Protocol ───────────────────────────────────────────────────────── */

/*
 * NEC timing (38 kHz carrier):
 *   Header:    9000 µs mark + 4500 µs space
 *   Bit '0':    562 µs mark +  562 µs space
 *   Bit '1':    562 µs mark + 1687 µs space
 *   Stop:       562 µs mark + 0 space (end)
 *
 * Frame: header + 8-bit addr + 8-bit ~addr + 8-bit cmd + 8-bit ~cmd + stop
 *      = 1 + 32 + 1 = 34 timing pairs
 *
 * Extended NEC: header + 16-bit addr + 8-bit cmd + 8-bit ~cmd + stop
 *             = 1 + 32 + 1 = 34 timing pairs
 */

#define NEC_HDR_MARK    9000
#define NEC_HDR_SPACE   4500
#define NEC_BIT_MARK     562
#define NEC_ZERO_SPACE   562
#define NEC_ONE_SPACE   1687

/** Encode 8 bits LSB-first into the timing buffer. */
static size_t encode_byte_lsb(uint8_t byte, ir_timing_t *buf)
{
    for (int i = 0; i < 8; i++) {
        buf[i].mark_us  = NEC_BIT_MARK;
        buf[i].space_us = (byte & 1) ? NEC_ONE_SPACE : NEC_ZERO_SPACE;
        byte >>= 1;
    }
    return 8;
}

size_t ir_protocol_encode_nec(uint16_t address, uint8_t command,
                              ir_timing_t *buf, size_t buf_len)
{
    /* Standard NEC: 34 pairs.  Extended NEC: also 34. */
    if (!buf || buf_len < 34)
        return 0;

    size_t n = 0;

    /* Header */
    buf[n].mark_us  = NEC_HDR_MARK;
    buf[n].space_us = NEC_HDR_SPACE;
    n++;

    if (address <= 0xFF) {
        /* Standard NEC: 8-bit address + complement */
        n += encode_byte_lsb((uint8_t)address, &buf[n]);
        n += encode_byte_lsb((uint8_t)~address, &buf[n]);
    } else {
        /* Extended NEC: 16-bit address (no complement) */
        n += encode_byte_lsb((uint8_t)(address & 0xFF), &buf[n]);
        n += encode_byte_lsb((uint8_t)(address >> 8), &buf[n]);
    }

    /* Command + complement */
    n += encode_byte_lsb(command, &buf[n]);
    n += encode_byte_lsb((uint8_t)~command, &buf[n]);

    /* Stop bit */
    buf[n].mark_us  = NEC_BIT_MARK;
    buf[n].space_us = 0;
    n++;

    return n;
}

size_t ir_protocol_encode_nec_repeat(ir_timing_t *buf, size_t buf_len)
{
    if (!buf || buf_len < 2)
        return 0;

    /* NEC repeat: 9000 µs mark + 2250 µs space + 562 µs mark */
    buf[0].mark_us  = NEC_HDR_MARK;
    buf[0].space_us = 2250;
    buf[1].mark_us  = NEC_BIT_MARK;
    buf[1].space_us = 0;

    return 2;
}

/* ── Samsung Protocol ───────────────────────────────────────────────────── */

/*
 * Samsung timing (38 kHz carrier):
 *   Header:    4500 µs mark + 4500 µs space
 *   Bits:      Same as NEC (562 µs mark, 562/1687 µs space)
 *   Frame:     header + 8-bit addr + 8-bit addr (repeated) +
 *              8-bit cmd + 8-bit ~cmd + stop
 *            = 1 + 32 + 1 = 34 timing pairs
 */

#define SAM_HDR_MARK    4500
#define SAM_HDR_SPACE   4500

size_t ir_protocol_encode_samsung(uint8_t address, uint8_t command,
                                  ir_timing_t *buf, size_t buf_len)
{
    if (!buf || buf_len < 34)
        return 0;

    size_t n = 0;

    /* Header */
    buf[n].mark_us  = SAM_HDR_MARK;
    buf[n].space_us = SAM_HDR_SPACE;
    n++;

    /* Address sent twice (no complement) */
    n += encode_byte_lsb(address, &buf[n]);
    n += encode_byte_lsb(address, &buf[n]);

    /* Command + complement */
    n += encode_byte_lsb(command, &buf[n]);
    n += encode_byte_lsb((uint8_t)~command, &buf[n]);

    /* Stop bit */
    buf[n].mark_us  = NEC_BIT_MARK;
    buf[n].space_us = 0;
    n++;

    return n;
}

/* ── Sony SIRC Protocol ─────────────────────────────────────────────────── */

/*
 * Sony SIRC 12-bit timing (40 kHz carrier):
 *   Header:    2400 µs mark + 600 µs space
 *   Bit '0':    600 µs mark + 600 µs space
 *   Bit '1':   1200 µs mark + 600 µs space
 *   Frame:     header + 7-bit cmd (LSB first) + 5-bit addr (LSB first)
 *            = 1 + 12 + 0 = 13 timing pairs (last has space=0)
 *
 * Note: Sony protocol requires sending the frame 3 times with 25ms gaps.
 * The caller is responsible for repeating.
 */

#define SONY_HDR_MARK   2400
#define SONY_BIT_SPACE   600
#define SONY_ZERO_MARK   600
#define SONY_ONE_MARK   1200

size_t ir_protocol_encode_sony(uint8_t address, uint8_t command,
                               ir_timing_t *buf, size_t buf_len)
{
    /* 1 header + 7 cmd bits + 5 addr bits = 13 pairs */
    if (!buf || buf_len < 13)
        return 0;

    size_t n = 0;

    /* Header */
    buf[n].mark_us  = SONY_HDR_MARK;
    buf[n].space_us = SONY_BIT_SPACE;
    n++;

    /* 7-bit command, LSB first */
    uint8_t cmd = command & 0x7F;
    for (int i = 0; i < 7; i++) {
        buf[n].mark_us  = (cmd & 1) ? SONY_ONE_MARK : SONY_ZERO_MARK;
        buf[n].space_us = SONY_BIT_SPACE;
        cmd >>= 1;
        n++;
    }

    /* 5-bit address, LSB first */
    uint8_t addr = address & 0x1F;
    for (int i = 0; i < 5; i++) {
        buf[n].mark_us  = (addr & 1) ? SONY_ONE_MARK : SONY_ZERO_MARK;
        /* Last bit has space_us = 0 (end of frame) */
        buf[n].space_us = (i == 4) ? 0 : SONY_BIT_SPACE;
        addr >>= 1;
        n++;
    }

    return n;
}

/* ── RC5 Protocol ───────────────────────────────────────────────────────── */

/*
 * RC5 timing (36 kHz carrier, Manchester encoding):
 *   Bit period: 1778 µs (half-bit = 889 µs)
 *   Bit '0':    889 µs mark + 889 µs space  (rising edge at mid-bit)
 *   Bit '1':    889 µs space + 889 µs mark  (falling edge at mid-bit)
 *
 * Frame (14 bits, MSB first):
 *   S1(1) + S2(1) + Toggle + 5-bit address + 6-bit command
 *
 * Manchester encoding produces variable-length timing pairs because
 * consecutive same-value bits merge their edges.
 *
 * Encoding approach: convert the 14-bit frame to a sequence of half-bit
 * symbols, then merge adjacent same-polarity symbols into mark/space pairs.
 */

#define RC5_HALF_BIT 889

size_t ir_protocol_encode_rc5(uint8_t address, uint8_t command, uint8_t toggle,
                              ir_timing_t *buf, size_t buf_len)
{
    if (!buf || buf_len < 28)
        return 0;

    /* Build the 14-bit RC5 frame MSB first:
     * S1=1, S2=1, T, A4..A0, C5..C0 */
    uint16_t frame = 0;
    frame |= (1 << 13);                          /* S1 = 1 */
    frame |= (1 << 12);                          /* S2 = 1 */
    frame |= ((toggle & 1) << 11);               /* Toggle */
    frame |= ((uint16_t)(address & 0x1F) << 6);  /* 5-bit address */
    frame |= (command & 0x3F);                    /* 6-bit command */

    /*
     * Convert to half-bit symbols.  Each Manchester bit produces two
     * half-bit symbols:
     *   bit=0 → mark, space  (high then low)
     *   bit=1 → space, mark  (low then high)
     *
     * We represent symbols as: 1 = mark (carrier on), 0 = space (off).
     */
    uint8_t symbols[28]; /* 14 bits × 2 half-bits */
    for (int i = 0; i < 14; i++) {
        int bit = (frame >> (13 - i)) & 1;
        if (bit == 0) {
            symbols[i * 2]     = 1; /* mark */
            symbols[i * 2 + 1] = 0; /* space */
        } else {
            symbols[i * 2]     = 0; /* space */
            symbols[i * 2 + 1] = 1; /* mark */
        }
    }

    /*
     * Merge consecutive same-polarity symbols into timing pairs.
     * Walk the symbol array, accumulating runs of mark/space.
     */
    size_t n = 0;
    int idx = 0;

    /* Skip any leading space symbols (for RC5, S1=1 so first symbol is
     * space — we start the transmission at the first mark). */
    while (idx < 28 && symbols[idx] == 0)
        idx++;

    while (idx < 28) {
        /* Accumulate mark duration */
        uint16_t mark = 0;
        while (idx < 28 && symbols[idx] == 1) {
            mark += RC5_HALF_BIT;
            idx++;
        }

        /* Accumulate space duration */
        uint16_t space = 0;
        while (idx < 28 && symbols[idx] == 0) {
            space += RC5_HALF_BIT;
            idx++;
        }

        if (n >= buf_len)
            return 0;

        buf[n].mark_us  = mark;
        buf[n].space_us = space;
        n++;
    }

    /* Ensure last element has space_us = 0 (end of transmission) */
    if (n > 0)
        buf[n - 1].space_us = 0;

    return n;
}

/* ── Utilities ──────────────────────────────────────────────────────────── */

uint32_t ir_protocol_carrier_hz(ir_protocol_id_t protocol)
{
    switch (protocol) {
    case IR_PROTO_NEC:     return 38000;
    case IR_PROTO_SAMSUNG: return 38000;
    case IR_PROTO_SONY:    return 40000;
    case IR_PROTO_RC5:     return 36000;
    default:               return 38000;
    }
}

static const char *s_protocol_names[] = {
    [IR_PROTO_NEC]     = "nec",
    [IR_PROTO_SAMSUNG] = "samsung",
    [IR_PROTO_SONY]    = "sony",
    [IR_PROTO_RC5]     = "rc5",
};

const char *ir_protocol_name(ir_protocol_id_t protocol)
{
    if (protocol < IR_PROTO_COUNT)
        return s_protocol_names[protocol];
    return "unknown";
}
