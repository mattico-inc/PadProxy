# Device Setup Design — USB CDC + Web Serial

## Overview

PadProxy needs a user-facing setup mechanism for configuring WiFi credentials
(OTA updates), controller pairing preferences, and hardware tuning parameters.
Since the device is always USB-connected to the host PC, we add a CDC (serial)
interface alongside the existing HID gamepad as a USB composite device. Users
configure PadProxy through Chrome's Web Serial API or any serial terminal.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Chrome / Serial Terminal                               │
│  ┌─────────────────┐                                    │
│  │  Web Serial API  │  ←─ or any serial terminal        │
│  └────────┬────────┘                                    │
│           │ USB CDC (virtual COM port)                  │
├───────────┼─────────────────────────────────────────────┤
│  Pico 2 W │ (USB Composite: HID + CDC)                 │
│           ▼                                             │
│  ┌─────────────────┐     ┌──────────────────┐          │
│  │  setup_cmd.c     │────▶│  device_config.c  │         │
│  │  (line parser)   │◀────│  (config struct)  │         │
│  └─────────────────┘     └────────┬─────────┘          │
│                                   │ serialize/          │
│                                   │ deserialize         │
│                                   ▼                     │
│                          ┌──────────────────┐          │
│                          │  Flash storage    │          │
│                          │  (4 KB sector)    │          │
│                          └──────────────────┘          │
└─────────────────────────────────────────────────────────┘
```

## USB Composite Device

The existing HID-only device becomes a composite with two interfaces:

| Interface | Class | Purpose |
|-----------|-------|---------|
| 0         | HID   | Gamepad reports (unchanged) |
| 1–2       | CDC   | Setup serial port (115200 baud) |

CDC uses two interfaces (CDC control + CDC data) per USB spec, so the
composite device has 3 interfaces total.

### TinyUSB Changes

- `tusb_config.h`: Enable `CFG_TUD_CDC 1`, add CDC buffer sizes
- `usb_hid_gamepad.c`: Update device descriptor to composite class (0xEF),
  add CDC descriptors, update interface numbering and endpoint allocation

## Configurable Settings

| Key | Type | Default | Range | Description |
|-----|------|---------|-------|-------------|
| `wifi_ssid` | string | `""` | 0–32 chars | WiFi SSID for OTA updates |
| `wifi_password` | string | `""` | 0–63 chars | WiFi password |
| `power_pulse_ms` | uint16 | `200` | 50–2000 | Power button pulse duration |
| `boot_timeout_ms` | uint16 | `30000` | 5000–60000 | Boot timeout before giving up |
| `device_name` | string | `"PadProxy"` | 1–32 chars | Device name (USB product string) |

## Serial Command Protocol

Line-based text protocol. Commands are newline-terminated. Responses prefixed
with `OK` or `ERR`.

```
→ get wifi_ssid
← OK MyNetwork

→ set wifi_ssid NewNetwork
← OK

→ list
← OK wifi_ssid=MyNetwork
← OK wifi_password=********
← OK power_pulse_ms=200
← OK boot_timeout_ms=30000
← OK device_name=PadProxy

→ save
← OK

→ defaults
← OK

→ version
← OK 0.1.0

→ status
← OK pc_state=OFF bt_connected=false

→ reboot
← OK
```

### Security

- `wifi_password` is masked in `list` and `get` output (shows `********`)
- No authentication (device is inside the PC case, physical access = trust)

## Module Design

### device_config (pure logic, testable)

```c
/* include/device_config.h */
typedef struct {
    char     wifi_ssid[33];
    char     wifi_password[64];
    uint16_t power_pulse_ms;
    uint16_t boot_timeout_ms;
    char     device_name[33];
} device_config_t;

void device_config_init(device_config_t *cfg);              /* Load defaults */
bool device_config_serialize(const device_config_t *cfg,
                             uint8_t *buf, size_t len);     /* → flash */
bool device_config_deserialize(device_config_t *cfg,
                               const uint8_t *buf, size_t len); /* ← flash */
```

Serialization uses a fixed-size binary format with a magic number and CRC for
integrity checking. If deserialization fails (bad magic, CRC mismatch, etc.),
the caller falls back to defaults.

### setup_cmd (pure logic, testable)

```c
/* include/setup_cmd.h */
typedef enum {
    SETUP_ACTION_NONE,
    SETUP_ACTION_SAVE,
    SETUP_ACTION_REBOOT,
} setup_cmd_action_t;

typedef struct {
    setup_cmd_action_t action;
    int out_len;
} setup_cmd_result_t;

setup_cmd_result_t setup_cmd_process(const char *line,
                                     device_config_t *cfg,
                                     char *out_buf, size_t out_size);
```

The command processor is a pure function: input line → config changes + output
text + action code. No I/O, no flash, no USB — fully testable on the host.

### Integration (firmware only, not unit-tested)

`main.c` polls `tud_cdc_available()` in the main loop, accumulates lines,
feeds complete lines to `setup_cmd_process()`, writes responses back via
`tud_cdc_write()`, and dispatches actions (save to flash, reboot).

## Flash Storage

- Uses a dedicated 4 KB sector near the end of flash (before any OTA
  partition)
- Binary format: `[magic: 4B][version: 2B][data: sizeof(device_config_t)][crc32: 4B]`
- On boot: attempt deserialize from flash → fall back to defaults on failure
- On `save` command: serialize config → erase sector → program sector

The flash read/write happens in firmware-only code (Pico SDK
`flash_range_erase` / `flash_range_program`). The serialize/deserialize
functions in `device_config.c` are pure logic tested on the host.

## Web Serial Setup Page (future)

A static HTML/JS page (hosted on GitHub Pages) that uses the Web Serial API:

1. User clicks "Connect" → browser shows serial port picker
2. Page sends `list` → displays current settings in a form
3. User edits settings → page sends `set key value` for each
4. User clicks "Save" → page sends `save`

This is a follow-up task — the firmware-side CDC + command protocol is
the foundation.

## Test Plan

Unit tests (host-native, Unity framework):

- **test_device_config**: defaults, serialize/deserialize roundtrip, corrupt
  data handling, boundary values, CRC validation
- **test_setup_cmd**: all commands, invalid input, buffer overflow protection,
  password masking, edge cases
