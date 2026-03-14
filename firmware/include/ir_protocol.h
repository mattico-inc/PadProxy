#ifndef IR_PROTOCOL_H
#define IR_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/**
 * IR protocol encoding — pure logic module.
 *
 * Converts IR protocol parameters (address + command) into a sequence of
 * mark/space timing pairs suitable for driving an IR LED with a carrier
 * frequency.  No hardware dependencies — fully testable on the host.
 *
 * Supported protocols:
 *   NEC      — 38 kHz, pulse-distance encoding (most TVs)
 *   Samsung  — 38 kHz, NEC variant with different header
 *   Sony     — 40 kHz, pulse-width encoding (SIRC 12/15/20-bit)
 *   RC5      — 36 kHz, Manchester encoding (Philips)
 */

/** IR protocol identifiers. */
typedef enum {
    IR_PROTO_NEC     = 0,
    IR_PROTO_SAMSUNG = 1,
    IR_PROTO_SONY    = 2,
    IR_PROTO_RC5     = 3,
    IR_PROTO_COUNT
} ir_protocol_id_t;

/**
 * One mark/space timing pair.
 *
 * mark_us  — duration (microseconds) the carrier is ON  (IR LED pulsing)
 * space_us — duration (microseconds) the carrier is OFF (IR LED dark)
 *
 * A trailing element with space_us == 0 signals end-of-transmission (the
 * final mark is still sent, then the LED turns off).
 */
typedef struct {
    uint16_t mark_us;
    uint16_t space_us;
} ir_timing_t;

/** Maximum number of timing pairs any supported protocol can produce. */
#define IR_TIMING_MAX 72

/**
 * Carrier frequency for each protocol (Hz).
 */
uint32_t ir_protocol_carrier_hz(ir_protocol_id_t protocol);

/**
 * Encode a NEC IR frame.
 *
 * Standard NEC: 8-bit address, 8-bit command.
 * Extended NEC: 16-bit address (no address complement), 8-bit command.
 *
 * @param address  Device address (0-255 standard, 0-65535 extended).
 * @param command  Command code (0-255).
 * @param buf      Output buffer for timing pairs.
 * @param buf_len  Buffer capacity (elements).  Must be >= 68.
 * @return         Number of timing pairs written, or 0 on error.
 */
size_t ir_protocol_encode_nec(uint16_t address, uint8_t command,
                              ir_timing_t *buf, size_t buf_len);

/**
 * Encode a NEC repeat code (sent when a button is held).
 *
 * @param buf      Output buffer (needs >= 2 elements).
 * @param buf_len  Buffer capacity.
 * @return         Number of timing pairs written, or 0 on error.
 */
size_t ir_protocol_encode_nec_repeat(ir_timing_t *buf, size_t buf_len);

/**
 * Encode a Samsung IR frame.
 *
 * Similar to NEC but uses a different header timing and sends the address
 * twice (no complement).
 *
 * @param address  Device address (0-255).
 * @param command  Command code (0-255).
 * @param buf      Output buffer (needs >= 68 elements).
 * @param buf_len  Buffer capacity.
 * @return         Number of timing pairs written, or 0 on error.
 */
size_t ir_protocol_encode_samsung(uint8_t address, uint8_t command,
                                  ir_timing_t *buf, size_t buf_len);

/**
 * Encode a Sony SIRC 12-bit IR frame.
 *
 * 7-bit command + 5-bit address.  Must be sent 3 times with 25ms gaps
 * (caller responsibility).
 *
 * @param address  Device address (0-31, 5 bits).
 * @param command  Command code (0-127, 7 bits).
 * @param buf      Output buffer (needs >= 14 elements).
 * @param buf_len  Buffer capacity.
 * @return         Number of timing pairs written, or 0 on error.
 */
size_t ir_protocol_encode_sony(uint8_t address, uint8_t command,
                               ir_timing_t *buf, size_t buf_len);

/**
 * Encode an RC5 IR frame (Manchester encoding).
 *
 * 5-bit address + 6-bit command + toggle bit.  The toggle bit alternates
 * with each new key press (caller tracks this).
 *
 * @param address  Device address (0-31, 5 bits).
 * @param command  Command code (0-63, 6 bits).
 * @param toggle   Toggle bit (0 or 1, alternates per key press).
 * @param buf      Output buffer (needs >= 28 elements).
 * @param buf_len  Buffer capacity.
 * @return         Number of timing pairs written, or 0 on error.
 */
size_t ir_protocol_encode_rc5(uint8_t address, uint8_t command, uint8_t toggle,
                              ir_timing_t *buf, size_t buf_len);

/**
 * Get a human-readable name for a protocol.
 */
const char *ir_protocol_name(ir_protocol_id_t protocol);

#endif /* IR_PROTOCOL_H */
