# Power Management & Hardware Design

This document captures the hardware design decisions for PadProxy's power management, USB architecture, and PC control interfaces.

## Table of Contents

1. [Design Goals](#design-goals)
2. [Power Architecture](#power-architecture)
3. [USB Architecture](#usb-architecture)
4. [Front Panel Interface](#front-panel-interface)
5. [PC State Detection](#pc-state-detection)
6. [PC Wake Methods](#pc-wake-methods)
7. [Connector Specifications](#connector-specifications)
8. [Component Selection](#component-selection)
9. [GPIO Assignments](#gpio-assignments)

---

## Design Goals

1. **Stay powered when PC is off** - Device must remain active to receive controller input and wake PC
2. **Detect PC power state** - Know when PC is on/off/sleeping to manage behavior
3. **Wake PC reliably** - Support wake from full power-off (S5), not just sleep
4. **Non-destructive installation** - Pass through existing connections, don't consume limited ports
5. **Flexible power options** - Work with various motherboard configurations

---

## Power Architecture

### Power Sources

PadProxy supports multiple power input options for maximum compatibility:

| Source | Available When PC Off? | Connector | Notes |
|--------|------------------------|-----------|-------|
| USB Standby Power (5VSB) | Yes (if BIOS enabled) | USB 2.0 Header | Primary source |
| Direct 5VSB Tap | Yes (always) | 2-pin header | Backup option |
| USB-C (external) | Depends on port | USB-C receptacle | Dev/alt mounting |

### Power Budget

| Component | Typical | Peak |
|-----------|---------|------|
| ESP32-S3 (active, BT+WiFi) | 100mA | 350mA |
| ESP32-S3 (light sleep, BT listening) | 2-5mA | - |
| FE1.1s USB Hub | 20mA | 50mA |
| Status LEDs | 5mA | 20mA |
| **Total (active)** | **~125mA** | **~420mA** |
| **Total (standby)** | **~25mA** | - |

USB standby power typically provides 500mA-2A depending on motherboard, which is sufficient.

### Power Path Design

```
                         ┌─────────────────────────────────┐
                         │         Power Management        │
                         │                                 │
USB 2.0 Header ─── 5V ──►│──┬──► 3.3V LDO ──► ESP32-S3   │
(Primary)                │  │                              │
                         │  └──► 5V Rail ──► USB Hub      │
                         │         │                       │
5VSB Tap (Optional) ────►│────────┘ (diode OR)            │
                         │                                 │
                         └─────────────────────────────────┘
```

**Note:** Simple diode OR configuration allows either source to power the device. When both present, higher voltage source wins (minimal difference in practice).

---

## USB Architecture

### Design Rationale

The ESP32-S3 connects **directly** to the motherboard USB port for optimal HID performance. The USB hub is a separate convenience feature that doesn't affect gamepad functionality.

**Key principle:** Hub issues should never break gamepad functionality.

### Block Diagram

```
Motherboard USB 2.0 Header (9-pin)
            │
            ├── Port 1 ──────────────► ESP32-S3 USB OTG
            │                          (TinyUSB HID Device)
            │                          - Gamepad emulation
            │                          - Latency critical
            │                          - Direct connection
            │
            └── Port 2 ──► FE1.1s ──┬──► USB Header OUT (Port A)
                         USB Hub    ├──► USB Header OUT (Port B)
                         IC         └──► (Spare - test pad)
```

### Port Allocation Summary

| Connection | Source | Destination | Purpose |
|------------|--------|-------------|---------|
| Motherboard Port 1 | 9-pin header | ESP32-S3 | Gamepad HID (direct) |
| Motherboard Port 2 | 9-pin header | FE1.1s upstream | Hub connection |
| Hub Downstream 1 | FE1.1s | 9-pin OUT header | User device passthrough |
| Hub Downstream 2 | FE1.1s | 9-pin OUT header | User device passthrough |
| Hub Downstream 3 | FE1.1s | Test pad | Debug/expansion |
| Hub Downstream 4 | FE1.1s | NC | Reserved |

### USB Enumeration Behavior

**When PC boots:**
1. ESP32-S3 enumerates as USB HID Gamepad
2. FE1.1s enumerates as USB Hub
3. Downstream devices enumerate through hub

**When PC is off (standby power):**
1. ESP32-S3 remains powered, no USB host activity
2. Hub remains powered but idle
3. ESP32 monitors for USB host connection to detect PC wake

---

## Front Panel Interface

### Passthrough Design

PadProxy sits between the case's front panel connectors and the motherboard, allowing:
- Physical power button still works
- PadProxy can detect button presses
- PadProxy can trigger "virtual" button presses
- LED signals passed through for normal case operation

### Wiring Diagram

```
Case Front Panel              PadProxy PCB                 Motherboard
Connectors                                                 Front Panel Header
                         ┌────────────────────────┐
┌─────────────┐          │                        │        ┌─────────────┐
│ PWR BTN + ──│─────────►│─── Sense (GPIO) ──────►│───────►│ PWR_BTN+    │
│ PWR BTN - ──│─────────►│─────────┬─────────────►│───────►│ PWR_BTN-    │
└─────────────┘          │         │              │        │             │
                         │    Trigger (GPIO)      │        │             │
                         │    via Optocoupler     │        │             │
                         │         │              │        │             │
                         │         └──────────────│───────►│             │
┌─────────────┐          │                        │        │             │
│ PWR LED + ──│─────────►│─── Sense (GPIO) ──────►│───────►│ PWR_LED+    │
│ PWR LED - ──│─────────►│───────────────────────►│───────►│ PWR_LED-    │
└─────────────┘          │                        │        │             │
                         │                        │        │             │
┌─────────────┐          │                        │        │             │
│ RESET BTN   │─────────►│─── (Passthrough) ─────►│───────►│ RESET       │
└─────────────┘          │                        │        │             │
                         │                        │        │             │
┌─────────────┐          │                        │        │             │
│ HDD LED     │─────────►│─── (Passthrough) ─────►│───────►│ HDD_LED     │
└─────────────┘          │                        │        └─────────────┘
                         └────────────────────────┘
```

### Power Button Control Circuit

**Optocoupler design** for electrical isolation:

```
ESP32 GPIO ────[330Ω]────┐
                         │
                    ┌────┴────┐
                    │ PC817   │
                    │ Opto-   │
                    │ coupler │
                    └────┬────┘
                         │
PWR_BTN+ ────────────────┼──────────► To Motherboard
                         │
                    (collector)
                         │
PWR_BTN- ────────────────┴──────────► To Motherboard
```

**Operation:**
- GPIO HIGH → Optocoupler conducts → PWR_BTN pins shorted → "Button pressed"
- Pulse duration: 100-500ms (configurable in firmware)
- Complete electrical isolation protects ESP32 from motherboard

### Power Button Sense Circuit

```
PWR_BTN+ ─────┬─────────────────────► To Motherboard
              │
              └────[10kΩ]────┬────► ESP32 GPIO (input, pull-up)
                             │
                          [100nF]
                             │
                            GND

Debounce in firmware: ~50ms
```

### Power LED Sense Circuit

```
PWR_LED+ ─────┬─────────────────────► To Case LED (optional)
              │
              └────[10kΩ]────────────► ESP32 GPIO (input)

LED+ HIGH = PC is ON
LED+ LOW = PC is OFF (or sleeping, depending on motherboard)
```

---

## PC State Detection

### Detection Methods

| Method | Reliability | What It Detects | Notes |
|--------|-------------|-----------------|-------|
| Power LED sense | High | PC on/off | Primary method |
| USB enumeration | High | PC on + booted | Confirms OS running |
| USB VBUS monitoring | Medium | Power state changes | Supplementary |
| Physical button press | High | User interaction | For state tracking |

### State Machine

```
                    ┌──────────────────┐
                    │                  │
         ┌─────────►│    PC_OFF (S5)   │◄──────────┐
         │          │                  │           │
         │          └────────┬─────────┘           │
         │                   │                     │
         │          Controller HOME pressed        │
         │          or physical button             │
         │                   │                     │
         │                   ▼                     │
         │          ┌──────────────────┐           │
         │          │                  │           │
         │          │   PC_BOOTING     │           │
         │          │                  │           │
         │          └────────┬─────────┘           │
         │                   │                     │
         │          USB enumeration detected       │
         │                   │                     │
         │                   ▼                     │
         │          ┌──────────────────┐           │
  Shutdown         │                  │      Shutdown
  detected          │     PC_ON        │      detected
         │          │                  │           │
         │          └────────┬─────────┘           │
         │                   │                     │
         │          USB suspended / LED off        │
         │                   │                     │
         │                   ▼                     │
         │          ┌──────────────────┐           │
         │          │                  │           │
         └──────────│   PC_SLEEPING    │───────────┘
                    │                  │
                    └──────────────────┘
```

---

## PC Wake Methods

### Method 1: Power Button Emulation (Primary)

**Trigger:** Controller HOME/Guide/PS button pressed

**Sequence:**
1. ESP32 receives button press via Bluetooth
2. Check current PC state (should be OFF or SLEEPING)
3. Pulse optocoupler GPIO HIGH for 200ms
4. Motherboard receives "power button press"
5. PC begins boot sequence
6. Monitor for USB enumeration to confirm success

**Advantages:**
- Works from full power-off (S5 state)
- Universal - works with any motherboard
- No BIOS/OS configuration required
- Reliable and immediate

### Method 2: Wake-on-LAN (Secondary/Optional)

**Trigger:** Controller button press (configurable)

**Sequence:**
1. ESP32 receives button press via Bluetooth
2. Construct WoL magic packet (6x 0xFF + 16x MAC address)
3. Send UDP broadcast on port 9
4. NIC receives packet and signals motherboard
5. PC wakes from sleep/hibernate

**Requirements:**
- WoL enabled in BIOS
- WoL enabled in OS network adapter settings
- ESP32 connected to same network/VLAN
- Only works from S3 (sleep) or S4 (hibernate) on most systems

**Advantages:**
- No physical connection to motherboard (besides USB)
- Can wake PC remotely (not just from controller)

---

## Connector Specifications

### Input Connectors

| Connector | Type | Pins | Purpose |
|-----------|------|------|---------|
| USB_IN | USB 2.0 9-pin header socket | 9 | From motherboard (primary) |
| USB_C | USB-C receptacle | 24 | Alt data+power / dev / fallback |
| FP_IN | 2.54mm headers or pads | 8+ | From case front panel |
| 5VSB_IN | 2-pin header | 2 | Optional backup power |

### Output Connectors

| Connector | Type | Pins | Purpose |
|-----------|------|------|---------|
| USB_OUT | USB 2.0 9-pin header pins | 9 | To user's USB devices (via hub) |
| FP_OUT | 2.54mm headers or pads | 8+ | To motherboard front panel header |

### Connection Modes

PadProxy supports multiple connection configurations:

#### Mode A: Full Internal Install (Recommended)

```
Motherboard          9-pin        PadProxy         9-pin         User's
USB 2.0 Header ════► cable ════► 9-pin IN         OUT ════════► Front Panel
                                     │                          USB / AIO / etc
                                     ├── Port 1 → ESP32
                                     └── Port 2 → Hub → USB_OUT

Features: ESP32 direct connection + Hub passthrough
```

#### Mode B: Rear USB with Adapter Cable

For systems where internal USB headers are fully occupied:

```
Rear USB          USB-A to       PadProxy         9-pin         User's
Port        ════► 9-pin cable ══► 9-pin IN         OUT ════════► Devices
                                     │
                                     ├── Port 1 → ESP32
                                     └── Port 2 → Hub → USB_OUT

Features: ESP32 direct connection + Hub passthrough
Requires: USB-A male to 9-pin female adapter cable
```

#### Mode C: Minimal / Development

For development, testing, or minimal installations:

```
Any USB           USB-C          PadProxy
Port        ════► cable    ════► USB-C port
                                     │
                                     └── ESP32 only

Features: ESP32/gamepad functionality only
Limitation: Hub passthrough NOT available (only 1 USB port)
```

### Recommended Cables

| Cable | Description | Use Case |
|-------|-------------|----------|
| 9-pin to 9-pin | Standard internal USB extension | Mode A: Internal header |
| USB-A to 9-pin | USB-A male → 9-pin female adapter | Mode B: Rear USB |
| USB-C to USB-A | Standard USB cable | Mode C: Development |

**Note:** USB-A to 9-pin adapter cables are available from various sources (search "USB motherboard header adapter"). Alternatively, this cable could be included with PadProxy kits.

### USB 2.0 Internal Header Pinout

Standard motherboard pinout (active low accent pin):

```
        ┌───┬───┬───┬───┬───┐
Pin 1 → │5V │D- │D+ │GND│KEY│ ← Pin 5 (blocked)
        ├───┼───┼───┼───┼───┤
Pin 6 → │5V │D- │D+ │GND│NC │ ← Pin 10
        └───┴───┴───┴───┴───┘
         P1  P1  P1  P1     P2  P2  P2  P2

P1 = USB Port 1 (D-/D+ directly to ESP32)
P2 = USB Port 2 (D-/D+ to Hub upstream)
5V = Shared power rail
KEY = Blocked pin for keying
```

### Front Panel Header Pinout

Standard ATX front panel header:

```
┌─────────────────────────────────┐
│ PWR_LED+  PWR_LED-  PWR_BTN+  │  Row 1
│ HDD_LED+  HDD_LED-  PWR_BTN-  │  Row 2
│ RESET+    RESET-    GND       │  Row 3 (optional)
└─────────────────────────────────┘
```

**Note:** Pinout varies by motherboard manufacturer. Design should accommodate common variations or use individual wires/pads.

---

## Component Selection

### Core Components

| Component | Part Number | Package | Purpose | Est. Cost |
|-----------|-------------|---------|---------|-----------|
| MCU | ESP32-S3-WROOM-1 | Module | Main controller | $3.50 |
| USB Hub | FE1.1s | SSOP-28 | USB port expansion | $0.50 |
| Optocoupler | PC817 | DIP-4/SMD | Power button isolation | $0.10 |
| LDO | AMS1117-3.3 | SOT-223 | 3.3V regulation | $0.10 |
| Crystal | 12MHz | HC49/SMD | Hub clock | $0.15 |

### USB Hub Support Components

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| Crystal | 12MHz | 1 | FE1.1s clock |
| Load capacitors | 22pF | 2 | Crystal circuit |
| Bypass capacitors | 100nF | 3 | Power filtering |
| Upstream resistors | 1.5kΩ | 1 | USB pull-up |
| ESD protection | USBLC6-2 | 1 | Optional, recommended |

### Connectors

| Component | Part Number | Purpose | Est. Cost |
|-----------|-------------|---------|-----------|
| USB 2.0 Header (F) | Generic 9-pin | Input from motherboard | $0.30 |
| USB 2.0 Header (M) | Generic 9-pin | Output to user devices | $0.30 |
| USB-C Receptacle | Generic 16-pin | Alt connection | $0.40 |
| 2.54mm Headers | Generic | Front panel connections | $0.20 |
| U.FL Connector | Generic | External antenna | $0.30 |

### Estimated Total BOM Cost

| Category | Cost |
|----------|------|
| Active components | ~$4.50 |
| Passive components | ~$0.50 |
| Connectors | ~$1.50 |
| PCB (qty 5) | ~$2.00 each |
| **Total per unit** | **~$8.50** |

---

## GPIO Assignments

### ESP32-S3 Pin Allocation

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 19 | USB_D- | Bidir | USB OTG Data- |
| 20 | USB_D+ | Bidir | USB OTG Data+ |
| 1 | PWR_BTN_SENSE | Input | Detect physical button press |
| 2 | PWR_BTN_TRIGGER | Output | Optocoupler drive |
| 3 | PWR_LED_SENSE | Input | Detect PC power state |
| 4 | STATUS_LED_R | Output | RGB status - Red |
| 5 | STATUS_LED_G | Output | RGB status - Green |
| 6 | STATUS_LED_B | Output | RGB status - Blue |
| 7 | I2C_SDA | Bidir | Optional OLED display |
| 8 | I2C_SCL | Output | Optional OLED display |
| 38 | BOOT_BTN | Input | Factory reset / pairing mode |
| EN | RESET | Input | Hardware reset |

**Reserved for antenna:**
- ESP32-S3-WROOM-1 uses internal antenna or U.FL connector variant

### Notes on GPIO Selection

1. **GPIOs 19, 20** - Dedicated USB pins on ESP32-S3, cannot be reassigned
2. **GPIOs 1-6** - General purpose, suitable for digital I/O
3. **GPIO 38** - Has internal pull-up, good for buttons
4. **Avoid GPIO 0** - Boot mode selection, keep floating or high
5. **Avoid GPIO 45, 46** - Strapping pins on some variants

---

## Design Considerations

### EMI/EMC

- Keep USB differential pairs matched length (±0.5mm)
- 90Ω differential impedance for USB traces
- Ground plane under USB traces
- Place bypass capacitors close to IC power pins

### Thermal

- ESP32-S3 may need thermal relief if sustained WiFi+BT use
- FE1.1s generates minimal heat
- No special cooling required for typical use

### ESD Protection

- USBLC6-2 on USB data lines recommended
- TVS diode on 5V input recommended
- Front panel connections through optocoupler provide isolation

### Mechanical

- PCB sized for internal mounting (max ~60x40mm)
- Mounting holes for M3 screws or standoffs
- Antenna connector near board edge for external routing

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-02-02 | Initial design document |
