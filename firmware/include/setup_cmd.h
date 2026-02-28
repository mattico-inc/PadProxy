#ifndef SETUP_CMD_H
#define SETUP_CMD_H

#include <stddef.h>
#include "device_config.h"

/**
 * Setup Command Processor
 *
 * Parses line-based text commands from the CDC serial interface and
 * reads/writes the device config struct.  This module is pure logic
 * with no hardware dependencies — all I/O is through function
 * parameters — so it can be unit-tested on the host.
 *
 * Protocol:
 *   → get <key>           Read a setting
 *   → set <key> <value>   Write a setting
 *   → list                List all settings
 *   → save                Request flash persist (action returned)
 *   → defaults            Reset config to defaults
 *   → version             Show firmware version
 *   → reboot              Request device reboot (action returned)
 *
 * Responses:
 *   ← OK [data]           Success
 *   ← ERR <message>       Error
 */

/**
 * Side-effect actions the caller must perform after processing a command.
 * The command processor cannot do these itself because they involve
 * hardware (flash write, system reset).
 */
typedef enum {
    SETUP_ACTION_NONE   = 0,
    SETUP_ACTION_SAVE   = 1,
    SETUP_ACTION_REBOOT = 2,
} setup_cmd_action_t;

typedef struct {
    /** Action the caller should perform after sending the response. */
    setup_cmd_action_t action;
    /** Number of bytes written to the output buffer (excluding NUL). */
    int out_len;
} setup_cmd_result_t;

/**
 * Firmware version string passed to "version" command.
 * Set once at init; must outlive all calls to setup_cmd_process().
 */
void setup_cmd_set_version(const char *version_str);

/**
 * Device status string passed to "status" command.
 * Set before each main-loop iteration; must outlive the call.
 */
void setup_cmd_set_status(const char *status_str);

/**
 * Process one line of input and produce a response.
 *
 * Leading/trailing whitespace and the trailing newline (if any) are
 * stripped before parsing.  Empty lines produce no output.
 *
 * @param line      NUL-terminated input line.
 * @param cfg       Config struct to read/modify.
 * @param out_buf   Buffer for the response text.
 * @param out_size  Size of out_buf.
 * @return          Result with action code and output length.
 */
setup_cmd_result_t setup_cmd_process(const char *line,
                                     device_config_t *cfg,
                                     char *out_buf, size_t out_size);

#endif /* SETUP_CMD_H */
