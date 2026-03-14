# WiFi, IR Blaster & Smart Home Integration Design

This document describes the architecture for persistent WiFi connectivity, IR
blaster TV control, and smart home platform integration in PadProxy.

## Table of Contents

1. [Overview](#overview)
2. [WiFi Service](#wifi-service)
3. [IR Blaster](#ir-blaster)
4. [Smart Home Integration](#smart-home-integration)
5. [WiFi / Bluetooth Coexistence](#wifi--bluetooth-coexistence)
6. [GPIO Assignments](#gpio-assignments)
7. [Configuration](#configuration)
8. [Build Variants](#build-variants)
9. [Hardware Changes](#hardware-changes)

---

## Overview

PadProxy gains three new capabilities:

| Feature | Purpose |
|---------|---------|
| **Persistent WiFi** | Expose PC state, accept power toggle commands, smart home integration |
| **IR Blaster** | Turn TV on/off via infrared when PC wakes/sleeps |
| **Smart Home** | Integrate with Home Assistant, HomeKit, Alexa, or Google Home |

### Data Flow (Updated)

```
BT Controller → Bluepad32 → bt_gamepad → main loop
    ├── gamepad_report_t → USB HID → PC
    ├── Guide button → pc_power_sm → power button pulse → PC
    │                              → ir_blaster_send() → TV
    └── (wake event)

WiFi ← HTTP/MQTT ← Smart Home Platform
    ├── GET /status → { pc_state, bt_connected, wifi_rssi }
    ├── POST /power → pc_power_sm(WAKE_REQUESTED)
    └── POST /ir/send → ir_blaster_send(code)
```

---

## WiFi Service

### Architecture

The WiFi service maintains a persistent station (STA) connection using the
CYW43439 radio. It runs an HTTP server (lwIP httpd) for local API access and
optionally connects to an MQTT broker for smart home integration.

### HTTP API

Lightweight REST-style API served by lwIP's built-in httpd with CGI handlers:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | JSON: PC state, BT connection, WiFi RSSI, firmware version |
| `/api/power` | POST | Toggle PC power (triggers WAKE_REQUESTED or shutdown) |
| `/api/ir/send` | POST | Send an IR command (protocol + code) |
| `/api/ir/learn` | POST | Learn an IR code from a remote (future) |
| `/api/config` | GET/POST | Read/write device configuration |

### Status Response Example

```json
{
  "pc_state": "on",
  "bt_connected": true,
  "wifi_rssi": -45,
  "firmware": "1.2.3",
  "ir_enabled": true
}
```

### Connection Management

- Connect to configured WiFi AP on boot (after OTA check, before BT init)
- Reconnect automatically on disconnect (exponential backoff: 1s, 2s, 4s, 8s,
  max 30s)
- mDNS advertisement: `padproxy.local` for easy discovery
- WiFi credentials stored in `device_config_t` (already implemented)

### Module Design

```
wifi_service.h / wifi_service.c
├── wifi_service_init(config)     — Connect to WiFi, start httpd
├── wifi_service_deinit()         — Disconnect, stop httpd
├── wifi_service_is_connected()   — Check WiFi status
├── wifi_service_get_rssi()       — Get signal strength
├── wifi_service_set_state_cb()   — Register callback for state queries
└── wifi_service_task()           — Poll (reconnect logic, called from main loop)
```

---

## IR Blaster

### Hardware

An IR LED driven by an NPN transistor on a GPIO pin. The transistor provides
sufficient current drive (~100mA) for the IR LED, which the GPIO alone cannot
supply (~12mA max).

```
GPIO 5 ──[1kΩ]──► NPN base (2N2222/BC547)
                   │
                   ├── Collector ──[22Ω]──► IR LED anode
                   │                         │
                   │                    IR LED cathode ──► VCC (3.3V)
                   │
                   └── Emitter ──► GND
```

**Note:** The IR LED is connected between VCC and the transistor collector
(common-anode configuration). When the transistor turns on, current flows
through the LED. The 22Ω resistor limits peak current to ~100mA at 3.3V
(accounting for ~1.2V LED Vf and ~0.2V Vce_sat).

### IR Protocol Encoding (Pure Logic Module)

The `ir_protocol` module is pure logic with no hardware dependencies, enabling
host-native testing. It converts IR protocol parameters into a sequence of
mark/space timing pairs.

**Supported Protocols:**

| Protocol | Carrier | Encoding | Common Devices |
|----------|---------|----------|----------------|
| NEC | 38 kHz | Pulse distance | Most TVs, universal |
| Samsung | 38 kHz | Pulse distance (NEC variant) | Samsung TVs |
| Sony SIRC | 40 kHz | Pulse width | Sony TVs |
| RC5 | 36 kHz | Manchester | Philips, European |

**Module Design:**

```
ir_protocol.h / ir_protocol.c  (pure logic, testable on host)
├── ir_protocol_encode_nec(addr, cmd, buf, buf_len)
├── ir_protocol_encode_samsung(addr, cmd, buf, buf_len)
├── ir_protocol_encode_sony(addr, cmd, buf, buf_len)
├── ir_protocol_encode_rc5(addr, cmd, toggle, buf, buf_len)
└── Returns: array of ir_timing_t { uint16_t mark_us; uint16_t space_us; }

ir_blaster.h / ir_blaster.c  (hardware driver, uses PIO for carrier)
├── ir_blaster_init(gpio)
├── ir_blaster_send(protocol, address, command)
├── ir_blaster_send_raw(timings, count)
└── ir_blaster_send_nec_repeat()
```

### PIO Carrier Generation

The RP2350's PIO (Programmable I/O) generates the 38 kHz carrier signal with
precise timing. The PIO program:

1. Accepts mark/space durations via the TX FIFO
2. During marks: outputs 38 kHz square wave (carrier modulated)
3. During spaces: holds output LOW (no carrier)

This approach is more accurate than bit-banging and doesn't consume CPU cycles
during transmission.

### IR Code Storage

IR codes are stored in `device_config_t`:

```c
typedef struct {
    uint8_t  protocol;    /* IR_PROTO_NEC, IR_PROTO_SAMSUNG, etc. */
    uint16_t address;     /* Device address */
    uint16_t command;     /* Command code (e.g., power toggle) */
} ir_code_t;
```

Default: NEC protocol, address 0x00, command 0x02 (common TV power toggle).
Configurable via CDC serial `set ir_*` commands and smart home API.

### Automatic TV Control

When the PC power state changes:

| Transition | Action |
|------------|--------|
| OFF → BOOTING | Send TV power-on IR code |
| ON → OFF (shutdown detected) | Send TV power-off IR code (configurable) |
| SLEEPING → BOOTING (wake) | Send TV power-on IR code |

This is opt-in via config (`ir_auto_tv = true`). The IR code is sent once per
transition with no repeat (most TVs toggle power with a single code).

---

## Smart Home Integration

### Architecture

Smart home integration uses a compile-time abstraction layer. The core firmware
exposes a `smarthome.h` interface that each platform implements:

```c
/* smarthome.h — abstract interface */

typedef struct {
    void (*init)(const device_config_t *config);
    void (*task)(void);     /* called from main loop */
    void (*deinit)(void);
    void (*publish_pc_state)(pc_power_state_t state);
    void (*publish_bt_status)(bool connected);
} smarthome_platform_t;
```

### Build Variants

Different smart home platforms are selected at compile time:

| Variant | CMake Flag | Protocol | Cloud Required? |
|---------|------------|----------|-----------------|
| **None** (default) | — | HTTP API only | No |
| **Home Assistant** | `-DSMARTHOME=hass` | MQTT | No (local broker) |
| **HomeKit** | `-DSMARTHOME=homekit` | HAP (mDNS + HTTP) | No |
| **Alexa** | `-DSMARTHOME=alexa` | Espalexa (UPnP/SSDP) | No (local) |
| **Google Home** | `-DSMARTHOME=google` | Local Home SDK | Yes (cloud) |

The HTTP API is always available regardless of smart home variant.

### Home Assistant (MQTT)

The primary smart home integration. Uses lwIP's MQTT client to connect to a
local MQTT broker (Mosquitto, etc.).

**MQTT Topics:**

```
padproxy/status          → JSON payload (published on change)
padproxy/pc/state        → "off" | "booting" | "on" | "sleeping"
padproxy/bt/connected    → "true" | "false"
padproxy/command/power   → "toggle" (subscribe — triggers power SM)
padproxy/command/ir      → JSON { "protocol": "nec", "address": 0, "command": 2 }
padproxy/availability    → "online" | "offline" (LWT)
```

**Home Assistant Auto-Discovery:**

Publishes MQTT discovery messages on connect so Home Assistant automatically
creates entities:

```
homeassistant/switch/padproxy/pc_power/config
homeassistant/sensor/padproxy/pc_state/config
homeassistant/binary_sensor/padproxy/bt_connected/config
homeassistant/button/padproxy/ir_send/config
```

**Configuration:**

```
mqtt_broker_host   — MQTT broker hostname/IP (e.g., "192.168.1.100")
mqtt_broker_port   — MQTT broker port (default: 1883)
mqtt_username      — MQTT username (optional)
mqtt_password      — MQTT password (optional)
```

### HomeKit (Future)

Uses the HomeKit Accessory Protocol (HAP) over WiFi. Exposes PadProxy as a
HomeKit bridge with:

- **Switch:** PC Power (on/off)
- **Sensor:** PC State (occupancy sensor or custom characteristic)
- **Switch:** TV Power (IR)

Requires a HAP library (e.g., esp-homekit ported to Pico, or HomeKitADK).
This is the most complex integration due to SRP authentication and TLV parsing.

### Alexa (Future)

Uses Espalexa-style local UPnP/SSDP emulation. Exposes PadProxy as a Belkin
WeMo device that Alexa discovers on the local network. Supports on/off commands
for PC power and TV power.

No cloud account or Alexa skill required — uses local LAN discovery.

### Google Home (Future)

Requires the Local Home SDK or cloud integration via Google Cloud. Most complex
option. Deferred to a future release.

---

## WiFi / Bluetooth Coexistence

### CYW43439 Radio Architecture

The Pico 2 W's CYW43439 shares a single 2.4 GHz radio between WiFi and
Bluetooth. The chip includes an internal coexistence arbiter that time-slices
the radio between protocols. The Pico SDK's
`pico_cyw43_arch_lwip_threadsafe_background` library handles this transparently.

### Coexistence Strategy

**Default mode: WiFi + BT concurrent**

Both WiFi and BT run simultaneously. The CYW43439's internal arbiter handles
time-slicing. This works well for:

- Low-bandwidth WiFi (MQTT messages, HTTP API responses)
- Standard BT Classic HID (gamepad reports at ~60 Hz)

**Gamepad-priority mode (optional, configurable):**

When a BT controller is actively connected and sending input, WiFi can be
temporarily disabled to eliminate any radio contention that might affect input
latency:

| Event | Action |
|-------|--------|
| BT gamepad connected | Disable WiFi (deassociate from AP) |
| BT gamepad disconnected | Re-enable WiFi (reconnect to AP) |
| No BT input for 30s | Re-enable WiFi (idle controller) |

This mode is opt-in via config (`wifi_bt_priority = true`). Most users should
leave the default concurrent mode — the CYW43439 handles coexistence well and
the latency impact is typically <1ms.

### Initialization Order

```
1. stdio_init_all()
2. device_config_init()
3. cyw43_arch_init()              ← Initialize CYW43 (shared by WiFi + BT)
4. WiFi connect (STA mode)        ← Join AP, get DHCP
5. OTA update check               ← Use WiFi for firmware check
6. wifi_service_init()            ← Start HTTP server, mDNS
7. smarthome_init()               ← Connect MQTT / start HAP
8. bt_gamepad_init()              ← Start Bluepad32 (BT Classic)
9. Main loop                      ← Poll everything
```

**Key change from current code:** WiFi is no longer deinitialized after OTA.
The `ota_update` module is refactored to not call `cyw43_arch_deinit()` — it
uses the already-connected WiFi from `wifi_service`.

---

## GPIO Assignments

### Updated Pin Allocation

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 0 | UART TX | Output | Debug output (reserved) |
| 1 | UART RX | Input | Debug input (reserved) |
| 2 | PWR_BTN_TRIGGER | Output | TLP222A photo-MOSFET (power button) |
| 3 | PWR_LED_SENSE | Input | PC817 optocoupler (power LED, active LOW) |
| 4 | STATUS_LED | Output | WS2812B addressable LED data |
| **5** | **IR_BLASTER** | **Output** | **NPN transistor → IR LED (38 kHz carrier via PIO)** |

### IR Blaster GPIO Selection

GPIO 5 is chosen because:
- Adjacent to existing GPIO assignments (2-4)
- No conflict with UART (0-1), SPI, or I2C defaults
- Supports PIO output for carrier generation
- Available on Pico 2 W module castellated pads

---

## Configuration

### New Config Fields

Added to `device_config_t`:

```c
/* IR Blaster */
uint8_t  ir_protocol;        /* IR_PROTO_NEC (default), SAMSUNG, SONY, RC5 */
uint16_t ir_address;          /* Device address (default: 0x00) */
uint16_t ir_command;          /* Power command (default: 0x02) */
bool     ir_auto_tv;          /* Auto send IR on PC state change (default: false) */

/* Smart Home (MQTT for Home Assistant) */
char     mqtt_broker[64];     /* MQTT broker host (empty = disabled) */
uint16_t mqtt_port;           /* MQTT broker port (default: 1883) */
char     mqtt_username[33];   /* MQTT username (optional) */
char     mqtt_password[64];   /* MQTT password (optional) */

/* WiFi/BT coexistence */
bool     wifi_bt_priority;    /* Disable WiFi during BT gameplay (default: false) */
```

### New Setup Commands

```
set ir_protocol <nec|samsung|sony|rc5>
set ir_address <0-65535>
set ir_command <0-65535>
set ir_auto_tv <true|false>
set mqtt_broker <host>
set mqtt_port <1-65535>
set mqtt_username <user>
set mqtt_password <pass>
set wifi_bt_priority <true|false>
```

---

## Build Variants

### CMake Configuration

```bash
# Default build (HTTP API only, no smart home)
make firmware

# Home Assistant build (MQTT)
cmake -DSMARTHOME=hass ...

# HomeKit build
cmake -DSMARTHOME=homekit ...

# Alexa build
cmake -DSMARTHOME=alexa ...
```

### Makefile Targets

```bash
make firmware                    # Default (HTTP API only)
make firmware SMARTHOME=hass     # Home Assistant (MQTT)
make firmware SMARTHOME=homekit  # HomeKit (future)
```

### Conditional Compilation

```c
#if defined(SMARTHOME_HASS)
    #include "smarthome_hass.h"
    static const smarthome_platform_t *s_smarthome = &smarthome_hass;
#elif defined(SMARTHOME_HOMEKIT)
    #include "smarthome_homekit.h"
    static const smarthome_platform_t *s_smarthome = &smarthome_homekit;
#else
    static const smarthome_platform_t *s_smarthome = NULL;  /* HTTP only */
#endif
```

---

## Hardware Changes

### New BOM Items

| Component | Value | Qty | Purpose | Est. Cost |
|-----------|-------|-----|---------|-----------|
| IR LED | TSAL6200 or VS1838B | 1 | 940nm high-power IR emitter | $0.10 |
| NPN Transistor | 2N2222 or BC547 | 1 | IR LED current driver | $0.05 |
| Resistor | 1kΩ | 1 | Base current limit for NPN | $0.01 |
| Resistor | 22Ω | 1 | IR LED current limit (~100mA peak) | $0.01 |

**Updated BOM total:** ~$11.00 per unit (was ~$10.80)

### PCB Changes

- Add IR LED + driver circuit footprint near board edge (IR must have
  line-of-sight to TV)
- Route GPIO 5 to NPN transistor base pad
- Consider adding an IR receiver (TSOP38238) footprint for future IR learning
  capability (GPIO 6, input)

### Enclosure Changes

- Add IR LED window/cutout in the case (clear plastic or open hole)
- Orient IR LED toward the front of the PC case for line-of-sight to TV
- For internal PC mounting, may need an external IR blaster on a wire
  (3.5mm jack or JST connector to external IR LED)

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-14 | Initial design: WiFi service, IR blaster, smart home integration |
