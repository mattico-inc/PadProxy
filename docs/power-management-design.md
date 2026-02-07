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
│ PWR BTN A ──│─────────►│─── Sense (bridge+opto)─│─►GPIO  │             │
│ PWR BTN B ──│─────────►│───────────┬────────────│───────►│ PWR_BTN     │
└─────────────┘          │           │            │───────►│             │
                         │    Trigger (photo-MOSFET)       │             │
                         │    via TLP222A         │        │             │
                         │           └────────────│───────►│             │
┌─────────────┐          │                        │        │             │
│ PWR LED + ──│─────────►│─── Sense (Zener clamp)─│─►GPIO  │             │
│             │          │───────────────────────►│───────►│ PWR_LED     │
│ PWR LED - ──│─────────►│───────────────────────►│───────►│             │
└─────────────┘          │                        │        │             │
                         │                        │        │             │
┌─────────────┐          │                        │        │             │
│ RESET A ────│─────────►│─── Passthrough ────────│───────►│ RESET       │
│ RESET B ────│─────────►│───────────┬────────────│───────►│             │
└─────────────┘          │   Optional Trigger     │        │             │
                         │   (TLP222A, DNP)       │        │             │
                         │           └────────────│───────►│             │
┌─────────────┐          │                        │        │             │
│ HDD LED     │─────────►│─── (Passthrough) ─────►│───────►│ HDD_LED     │
└─────────────┘          │                        │        └─────────────┘
                         └────────────────────────┘

DNP = Do Not Populate (footprint on PCB, component not soldered by default)
PWR BTN / RESET labels use A/B instead of +/- (polarity agnostic)
PWR LED retains +/- markings (user must connect correctly)
```

### Design Constraint: Polarity Agnosticism

A critical design goal is that **all front panel connections must work regardless of
how the user connects the wires**. PC front panel connectors vary by manufacturer, and
users should not need to identify +/- polarity for the power button or LED wires.

This drives two key component choices:
- **Photo-MOSFET optocoupler** for triggering (MOSFET output conducts bidirectionally)
- **Schottky bridge rectifier + optocoupler** for sensing (rectifies unknown polarity)

### Power Button Trigger Circuit (Polarity-Agnostic)

Uses a **photo-MOSFET optocoupler** (e.g., TLP222A, AQY212, or CPC1017N) instead of
a standard phototransistor optocoupler. The MOSFET output is inherently bidirectional,
acting as a true analog switch regardless of which direction current flows.

```
                    ┌──────────────────┐
ESP32 GPIO ──[330Ω]──►│ LED     MOSFET  │
                    │                  │
GND ─────────────────►│       (bidir)  │
                    └───┬──────────┬───┘
                        │          │
  To Mobo PWR_BTN_A ───┘          └─── To Mobo PWR_BTN_B
```

**Operation:**
- GPIO HIGH -> LED illuminates -> MOSFET turns on -> PWR_BTN pins shorted
- GPIO LOW -> MOSFET off -> PWR_BTN pins open (normal state)
- Pulse duration: 100-500ms (configurable in firmware)
- Complete optical isolation protects ESP32 from motherboard
- **Polarity agnostic:** MOSFET conducts in both directions when on, exactly like
  a mechanical switch. Works regardless of which pin the motherboard pulls high.

**Why not a phototransistor optocoupler (PC817)?**
The PC817's phototransistor output only conducts collector-to-emitter. If the user
connects the wires backwards relative to the motherboard's internal pull-up polarity,
the trigger circuit would fail. The photo-MOSFET eliminates this issue.

**Why not a mechanical relay?**
A relay's dry contacts are also polarity-agnostic, but relays are larger (~15x12mm
vs DIP-4), draw 15-80mA for the coil (vs ~10mA for an LED), produce an audible click,
require a flyback diode, and have mechanical wear. For a signal that only needs to
short two low-current pins, the photo-MOSFET is the better fit.

| Property | PC817 (Phototransistor) | TLP222A (Photo-MOSFET) | Mechanical Relay |
|----------|------------------------|----------------------|-----------------|
| Polarity agnostic (load) | No | **Yes** | **Yes** |
| Electrical isolation | Yes | Yes | Yes |
| Size | DIP-4 | DIP-4 | ~15x12mm |
| Drive current | ~10mA | ~10mA | 15-80mA |
| Flyback diode needed | No | No | Yes |
| Mechanical wear | None | None | Millions of cycles |
| Audible noise | None | None | Click |
| On-resistance | N/A (transistor) | ~4Ω (irrelevant for signal) | <0.1Ω |
| Cost | $0.10 | $0.50-1.00 | $0.50-2.00 |

### Power Button Sense Circuit (Polarity-Agnostic)

Uses a **Schottky bridge rectifier** feeding a standard optocoupler to detect when the
physical power button is pressed. The bridge rectifier ensures correct polarity to the
optocoupler LED regardless of which wire is +/- from the motherboard.

```
                         ┌──────────────────────────────────┐
                         │      Schottky Bridge Rectifier   │
                         │          (4x BAT54 / SS14)       │
                         │                                  │
Mobo PWR_BTN_A ──┬──────│──►D1──┬───[470Ω]──►│ PC817 LED+ │
                 │       │      │            │             │
                 │       │    + DC out       │  Optocoupler│
                 │       │      │            │             │
                 │       │      │◄──[LED-]───│◄────────────│
                 │       │      │            │             │
Mobo PWR_BTN_B ──┬──────│──►D3──┘           │  Phototrans │──► ESP32 GPIO
                 │       │                   │  (output)   │     (input, pull-up)
                 │       │  D2, D4 complete  │             │
                 │       │  the bridge       └─────────────┘
                 │       └──────────────────────────────────┘
                 │
                 └──── (passthrough to case button wires)
```

**Simplified schematic:**

```
Mobo Pin A ──┬──────────────────────────────── Case Button Wire A
             │
             ├──┤D1►──┬──[470Ω]──[PC817 LED]──┬──◄D4├──┤
             │        │                        │        │
             │        │    (bridge provides    │        │
             │        │     correct polarity)  │        │
             │        │                        │        │
             ├──◄D2├──┘                        └──┤D3►──┤
             │                                          │
Mobo Pin B ──┴──────────────────────────────── Case Button Wire B


PC817 Output (phototransistor):
  VCC ──[10kΩ]──┬──► ESP32 GPIO (PWR_BTN_SENSE)
                │
           [collector]
                │
              [GND]
```

**Operation:**
- Button **not pressed**: Motherboard has voltage across the two pins (typically
  3.3V or 5V from internal pull-up). Current flows through bridge + optocoupler.
  PC817 phototransistor conducts. GPIO reads LOW.
- Button **pressed**: Both pins shorted together, no voltage differential, no current
  through optocoupler. Phototransistor off. GPIO pulled HIGH by pull-up resistor.
- **Logic is inverted**: GPIO HIGH = button pressed, GPIO LOW = button not pressed.
- Debounce in firmware: ~50ms

**Voltage budget (worst case at 3.3V motherboard pull-up):**
- Schottky bridge: 2 x 0.3V = 0.6V
- PC817 LED Vf: ~1.2V
- Remaining for resistor: 3.3V - 0.6V - 1.2V = 1.5V
- With 470Ω resistor: ~3.2mA (sufficient for PC817, min ~1mA for reliable turn-on)
- At 5V pull-up: ~6.8mA (well within limits)

**Why Schottky diodes?** Standard silicon diodes (1N4148) have ~0.7V forward drop,
giving a bridge drop of 1.4V. At 3.3V that leaves only 0.7V after the LED -- too
tight for reliable operation. Schottky diodes (BAT54, BAT43, SS14) drop only ~0.3V.

### Power LED Sense Circuit

The power LED header has a defined polarity from the motherboard, so the user is
expected to connect +/- correctly (clearly marked on the PCB silkscreen).

**IMPORTANT: 5V Tolerance.** The ESP32-S3 GPIOs are NOT 5V tolerant (absolute max
is VDD+0.3V = 3.9V). Motherboard LED headers can output 5V. A **3.3V Zener diode
clamp** (BZX84C3V3) protects the GPIO while allowing correct sensing at both 3.3V
and 5V motherboard voltages.

```
PWR_LED+ ─────┬─────────────────────► To Case LED (optional)
              │
              └────[10kΩ]──┬─────────► ESP32 GPIO (PWR_LED_SENSE, input)
                           │
                       [3.3V Zener]   (BZX84C3V3 - clamps to 3.3V if input >3.3V)
                           │
                          GND

PWR_LED- ─────────────────────────────► To Case LED GND (passthrough)
```

**Operation:**
- Mobo LED output HIGH (3.3V or 5V) -> Zener clamps if needed -> GPIO reads HIGH -> PC is ON
- Mobo LED output LOW (0V) -> GPIO reads LOW -> PC is OFF (or sleeping)
- At 5V input: Zener clamps to ~3.3V, current through 10kΩ = (5-3.3)/10k = 0.17mA (negligible)
- At 3.3V input: Zener does not conduct (below breakdown), GPIO sees full 3.3V

**Polarity:** User must connect +/- correctly. Silkscreen markings should be clear.
If connected backwards, the LED on the case won't light and the sense circuit won't
detect the motherboard's signal -- but no damage will occur (the GPIO is protected
by the Zener and the input resistor).

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
| Photo-MOSFET Optocoupler | TLP222A (or AQY212 / CPC1017N) | DIP-4/SOP-4 | Power button trigger (polarity-agnostic) | $0.60 |
| Optocoupler (sense) | PC817 | DIP-4/SMD | Power button sense (with bridge rectifier) | $0.10 |
| Schottky diodes | BAT54S (dual) or 4x BAT54 | SOT-23 / SOD-323 | Bridge rectifier for sense circuit | $0.15 |
| Zener diode | BZX84C3V3 | SOT-23 | 5V clamp for LED sense GPIO | $0.02 |
| LDO | AMS1117-3.3 | SOT-223 | 3.3V regulation | $0.10 |
| Crystal | 12MHz | HC49/SMD | Hub clock | $0.15 |
| Photo-MOSFET Optocoupler | TLP222A | DIP-4/SOP-4 | *Optional:* Reset button trigger | $0.60 |

### Power Button Circuit Components

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| Photo-MOSFET optocoupler | TLP222A | 1 | Trigger - shorts motherboard PWR_BTN pins |
| Resistor | 330Ω | 1 | Current limit for trigger optocoupler LED |
| PC817 optocoupler | PC817 | 1 | Sense - detects physical button press |
| Schottky diodes | BAT54 | 4 (or 2x BAT54S dual) | Bridge rectifier for sense polarity agnosticism |
| Resistor | 470Ω | 1 | Current limit for sense optocoupler LED |
| Resistor | 10kΩ | 1 | Pull-up for sense optocoupler output to ESP32 GPIO |

### Power LED Sense Components

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| Resistor | 10kΩ | 1 | Current limit / sense resistor for LED signal |
| Zener diode | BZX84C3V3 (3.3V) | 1 | Clamp to protect ESP32 GPIO from 5V motherboard output |

### Optional: Reset Button Trigger Components

Identical circuit to power button trigger. Allows firmware to trigger a hardware
reset remotely (e.g., watchdog recovery, remote management). The reset button still
passes through for normal physical use.

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| Photo-MOSFET optocoupler | TLP222A | 1 | Trigger - shorts motherboard RESET pins |
| Resistor | 330Ω | 1 | Current limit for trigger optocoupler LED |

**BOM impact of optional reset trigger: ~$0.61** (one TLP222A + one resistor).
PCB footprint is present but component can be left unpopulated (DNP) to save cost
on builds that don't need remote reset capability.

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
| 2.54mm Pin Headers | Generic | Front panel connections (default) | $0.20 |
| U.FL Connector | Generic | External antenna | $0.30 |
| Screw Terminals | 2-pos 2.54mm pitch | *Optional:* Front panel alt (not populated by default) | $0.40 |

### Front Panel Connector Design

The PCB provides **dual-footprint pads** for each front panel signal pair (PWR_BTN,
PWR_LED, RESET, HDD_LED):

- **Default: 2.54mm pin headers** - Standard 2-pin headers per signal, matching the
  individual DuPont-style wires that come from PC cases. One pair of pins per signal.
- **Optional: Screw terminals** - Same pad locations accept 2.54mm pitch screw terminals
  for users who prefer tool-less wire connection. Not soldered by default to save cost
  and board height. User can solder on themselves if preferred.

Both connector types share the same footprint location -- only one can be populated
per signal pair.

### Estimated Total BOM Cost

| Category | Cost | Notes |
|----------|------|-------|
| Active components | ~$5.50 | MCU, hub, optocouplers, LDO |
| Passive components | ~$0.65 | Resistors, caps, crystal, Zener |
| Connectors | ~$1.50 | USB headers, USB-C, pin headers, U.FL |
| PCB (qty 5) | ~$2.00 each | |
| **Total per unit (base)** | **~$9.65** | Without optional reset trigger |
| **Total per unit (full)** | **~$10.25** | With optional reset trigger |

---

## GPIO Assignments

### ESP32-S3 Pin Allocation

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 19 | USB_D- | Bidir | USB OTG Data- |
| 20 | USB_D+ | Bidir | USB OTG Data+ |
| 1 | PWR_BTN_SENSE | Input | Detect physical button press (PC817 output, active HIGH = pressed, pull-up) |
| 2 | PWR_BTN_TRIGGER | Output | Photo-MOSFET optocoupler drive (TLP222A LED) |
| 3 | PWR_LED_SENSE | Input | Detect PC power state (Zener-clamped for 5V tolerance) |
| 4 | STATUS_LED_R | Output | RGB status - Red |
| 5 | STATUS_LED_G | Output | RGB status - Green |
| 6 | STATUS_LED_B | Output | RGB status - Blue |
| 7 | I2C_SDA | Bidir | Optional OLED display |
| 8 | I2C_SCL | Output | Optional OLED display |
| 9 | RST_BTN_TRIGGER | Output | Optional: Photo-MOSFET optocoupler drive for reset (TLP222A LED) |
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
- 3.3V Zener clamp (BZX84C3V3) on PWR_LED_SENSE GPIO for 5V motherboard compatibility

### Mechanical

- PCB sized for internal mounting (max ~60x40mm)
- Mounting holes for M3 screws or standoffs
- Antenna connector near board edge for external routing

---

## Design Decisions (Resolved)

### D1: Power LED Sense - Polarity (RESOLVED)

**Decision: Keep simple resistive sense with Zener clamp.** User connects +/- correctly
per silkscreen markings. The LED header has a defined polarity from the motherboard,
so users are expected to know which wire is + (unlike the power button which is an
unpolarized switch). A 3.3V Zener diode clamp protects the GPIO from 5V motherboards.
Reverse connection causes no damage but disables sensing.

### D2: Reset Button Trigger (RESOLVED)

**Decision: Include as optional (DNP by default).** The PCB footprint includes a
second TLP222A photo-MOSFET optocoupler and 330Ω resistor for reset trigger
capability. These components are **not populated by default** to save ~$0.61.
Users who want remote reset (e.g., watchdog recovery) can solder them on.
GPIO 9 is reserved for this function.

### D3: Front Panel Connector Format (RESOLVED)

**Decision: Dual-footprint -- standard 2.54mm pin headers (default) with optional
screw terminal pads.** Individual 2-pin headers per signal match how PC case wires
come. The PCB pads also accept 2.54mm pitch screw terminals for users who prefer
them. Only one type can be populated per signal pair.

### D4: Voltage Compatibility (RESOLVED)

**Research findings:** Most ATX motherboards pull PWR_BTN to **5V** (from the 5VSB
standby rail via the SuperIO/EC chip). Some use **3.3V**. No evidence of voltages
below 3.3V on standard ATX boards. Some industrial boards (e.g., ASRock IMB-140D)
offer jumper selection between 3.3V and 5V.

**Impact on design:**
- **Button sense circuit (bridge + optocoupler):** Works at both 3.3V and 5V.
  At 3.3V: ~3.2mA through optocoupler LED (above PC817 minimum). At 5V: ~6.8mA.
  The optocoupler provides complete isolation, so 5V never reaches the ESP32.
- **Button trigger circuit (photo-MOSFET):** Load side is completely isolated from
  the ESP32. Works at any voltage up to the TLP222A's 60V rating.
- **LED sense circuit:** 5V motherboard output clamped to 3.3V by Zener diode.
  ESP32 GPIO protected.
- **ESP32-S3 is NOT 5V tolerant** (absolute max VDD+0.3V = 3.9V). All GPIO inputs
  from motherboard signals are now protected either by optocoupler isolation or
  Zener clamping.

## Open Design Questions

### Q1: Photo-MOSFET Part Selection

Candidate parts for the trigger optocoupler(s):

| Part | Ron | Max Load V | Max Load I | Package | Notes |
|------|-----|-----------|-----------|---------|-------|
| TLP222A | 4Ω | 60V | 500mA | DIP-4 | Common, well-documented |
| AQY212 | 2.5Ω | 60V | 500mA | SOP-4 | Smaller package |
| CPC1017N | 8Ω | 60V | 200mA | SOP-4 | Cheapest, adequate |

All are massively overspec for a button signal (~3.3-5V, <1mA when shorted). The
choice is mainly about availability and package preference. Defaulting to TLP222A
for now due to wide availability and DIP-4 hand-soldering friendliness.

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-02-02 | Initial design document |
| 0.2 | 2026-02-06 | Revised to polarity-agnostic design: photo-MOSFET optocoupler for trigger, Schottky bridge + PC817 for sense. Added open design questions. Updated BOM. |
| 0.3 | 2026-02-07 | Added 5V tolerance protection (Zener clamp on LED sense). Added optional reset trigger circuit (DNP). Dual-footprint connectors (headers + screw terminals). Resolved design questions D1-D4. Updated BOM and GPIO table. |
