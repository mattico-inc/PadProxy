#ifndef IR_BLASTER_H
#define IR_BLASTER_H

#include <stdint.h>
#include <stdbool.h>
#include "ir_protocol.h"

/**
 * IR blaster hardware driver.
 *
 * Uses PIO on RP2350 to generate a modulated carrier signal (36-40 kHz)
 * with precise mark/space timing.  The PIO program handles carrier
 * generation so the CPU is free during transmission.
 *
 * Hardware: GPIO → 1kΩ → NPN base → collector → 22Ω → IR LED → VCC
 *
 * GPIO assignment: GPIO 5 (IR_BLASTER_GPIO)
 */

#define IR_BLASTER_GPIO 5

/**
 * Initialize the IR blaster hardware.
 * Configures the PIO state machine for carrier generation on the given GPIO.
 */
void ir_blaster_init(void);

/**
 * Send an IR command using a specified protocol.
 *
 * Encodes the address/command pair for the given protocol, then transmits
 * the resulting mark/space waveform via PIO.  Blocks until transmission
 * is complete (~30-100ms depending on protocol).
 *
 * @param protocol  IR protocol (NEC, Samsung, Sony, RC5).
 * @param address   Device address.
 * @param command   Command code.
 * @return true on success, false on encoding error.
 */
bool ir_blaster_send(ir_protocol_id_t protocol, uint16_t address,
                     uint16_t command);

/**
 * Send a raw sequence of mark/space timing pairs.
 *
 * @param carrier_hz  Carrier frequency (e.g., 38000).
 * @param timings     Array of mark/space pairs.
 * @param count       Number of elements in timings.
 */
void ir_blaster_send_raw(uint32_t carrier_hz, const ir_timing_t *timings,
                         size_t count);

#endif /* IR_BLASTER_H */
