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
10. [Design Considerations](#design-considerations)

---

## Design Goals

1. **Stay powered when PC is off** - Device must remain active to receive controller input and wake PC
2. **Detect PC power state** - Know when PC is on/off/sleeping to manage behavior
3. **Wake PC reliably** - Support wake from full power-off (S5), not just sleep
4. **Simple installation** - 4-wire cable to motherboard, no passthrough needed
5. **Flexible power options** - Work with various motherboard configurations

---

## Power Architecture

### Power Sources

PadProxy supports multiple power input options for maximum compatibility:

| Source | Available When PC Off? | Connector | Notes |
|--------|------------------------|-----------|-------|
| USB Standby Power (5VSB) | Yes (if BIOS enabled) | USB 2.0 Header | Primary source — power mux IN1 (priority) |
| Direct 5VSB Tap | Yes (always) | 2-pin header | Backup source — power mux IN2 |
| USB-C (external) | Depends on port | USB-C receptacle | Dev/alt mounting — feeds Pico 2 W directly, not muxed |

### Power Budget

| Component | Typical | Peak |
|-----------|---------|------|
| Pico 2 W (active, BT) | 50mA | 150mA |
| Pico 2 W (dormant, BT listening) | 5-10mA | - |
| Status LEDs | 5mA | 20mA |
| **Total (active)** | **~55mA** | **~170mA** |
| **Total (standby)** | **~15mA** | - |

USB standby power typically provides 500mA-2A depending on motherboard, which is sufficient.

### Power Path Design

```
                         ┌─────────────────────────────────┐
                         │         Power Management        │
                         │                                 │
USB 2.0 Header ─── 5V ──►│─ IN1 ─┐                          │
(Primary, mux IN1)       │       ├─► TPS2116 ─► VSYS ──► Pico 2 W
                         │       │  (active mux)  (onboard 3.3V regulator)
5VSB Tap (Optional) ────►│─ IN2 ─┘                          │
(Backup, mux IN2)        │                                 │
                         └─────────────────────────────────┘
```

**Note:** An active power mux (TPS2116) replaces the previous passive diode-OR — see
[Active Power Mux](#active-power-mux) below. The Pico 2 W has an onboard buck-boost
regulator (RT6154) that generates 3.3V from VSYS, so no external LDO is needed.

### Active Power Mux

v1 uses an **active power mux IC — TPS2116** (Texas Instruments) or equivalent small
2:1 power mux — in place of the passive diode-OR. This is the same part recommended in
[pcb-v2-design.md](pcb-v2-design.md)'s "Dual-Port Power Management" section for the v2
redesign, but retrofitted here onto the **existing Pico 2 W module carrier board**, not
the custom board: v2's version assumes a custom RP2350B board with two USB-C ports
(`USB_NATIVE`/`USB_PWR`), whereas this addendum keeps the Pico 2 W module and its
current connector set (USB 2.0 9-pin header + USB-C + 5VSB tap header) — no RP2350B
migration.

**Connections:**
- **IN1 (priority):** USB standby power from the USB 2.0 9-pin header (`USB_IN`, 5V
  pin) — primary source, since it tracks PC power state directly (a rising edge signals
  PC power-on) and is the connector engaged in normal internal-install use.
- **IN2 (backup):** the optional 5VSB tap header (`5VSB_IN`) — used on motherboards
  that don't expose 5VSB on the USB header.
- **OUT → VSYS:** feeds the Pico 2 W module's onboard buck-boost, unchanged.
- The discrete `USB_C` receptacle (Mode C, dev/fallback) is **not** wired into the
  mux — it connects to the Pico 2 W's own USB port directly, as today, since it's a
  mutually exclusive alternate connection mode rather than a concurrent power source.

**Priority & backfeed blocking:** SEL is fixed to prefer IN1; the TPS2116 switches to
IN2 automatically if IN1 drops below ~4.5V, with no glitch on VSYS. Critically, it
blocks reverse current on the non-selected input — if a 5VSB tap cable is left
connected to a powered source while the 9-pin header is plugged into a different,
unpowered or unrelated host, the mux prevents that tap voltage from backfeeding through
the header into the host's USB port. This matters more now that PadProxy is routinely
unplugged and moved between systems rather than staying wired into one PC, where
connector combinations are less predictable than a single fixed install.

**VBUS sense:** route both mux inputs to Pico 2 W ADC-capable GPIOs through the same
100kΩ/47kΩ divider used in pcb-v2-design.md (5V → ~1.6V, safely under the 3.3V ADC
range). The Pico 2 W module exposes ADC0–ADC2 only on GPIO26–28 (fewer ADC options than
the bare RP2350B assumed in v2). None of GPIO26–28 are used elsewhere on this board
(existing assignments are GPIO2/3/4 — see [GPIO Assignments](#gpio-assignments)), so:

| GPIO | Signal | Notes |
|------|--------|-------|
| 26 (ADC0) | USB_IN_VBUS_SENSE | 9-pin header (mux IN1) divider |
| 27 (ADC1) | TAP_VBUS_SENSE | 5VSB tap header (mux IN2) divider |

GPIO28 (ADC2) remains free for future use.

---

## USB Architecture

### Design Rationale

The Pico 2 W connects **directly** to the motherboard USB port for optimal HID performance. Only one USB port is used — dedicated to the gamepad HID device.

### Block Diagram

```
Motherboard USB 2.0 Header (9-pin)
            │
            └── Port 1 ──────────────► Pico 2 W USB Device
                                       (TinyUSB HID Device)
                                       - Gamepad emulation
                                       - Latency critical
                                       - Direct connection
```

### USB Enumeration Behavior

**When PC boots:**
1. Pico 2 W enumerates as USB HID Gamepad

**When PC is off (standby power):**
1. Pico 2 W remains powered, no USB host activity
2. Pico 2 W monitors for USB host connection to detect PC wake

---

## Front Panel Interface

### 4-Wire Connection

PadProxy connects to the motherboard's front panel header via a **4-wire cable**.
The case's existing front panel connectors (power button, power LED, reset, HDD LED)
connect directly to the motherboard header as normal — PadProxy does **not** sit in
the signal path.

PadProxy's 4 wires connect **in parallel** with the case's front panel wires on the
motherboard header pins:

| Wire | Motherboard Pin | Circuit | Purpose |
|------|----------------|---------|---------|
| 1 | PWR_BTN A | TLP222A output | Power button trigger |
| 2 | PWR_BTN B | TLP222A output | Power button trigger |
| 3 | PWR_LED+ | PC817 input (+) | Power LED sense |
| 4 | PWR_LED- | PC817 input (-) | Power LED sense |

Both circuits use optocouplers for **full optical isolation** between PadProxy and
the motherboard. No shared ground reference is needed.

### Wiring Diagram

```
PadProxy                      4-Wire Cable                Motherboard
                                                          Front Panel Header
┌──────────────────────┐                                 ┌─────────────┐
│                      │                                 │             │
│ TLP222A  ────────────│── Wire 1 ── PWR_BTN A ─────────│─ PWR_BTN    │
│ (MOSFET out) ────────│── Wire 2 ── PWR_BTN B ─────────│─            │
│                      │                                 │             │
│ PC817    ◄───────────│── Wire 3 ── PWR_LED+  ◄────────│─ PWR_LED    │
│ (LED in) ◄───────────│── Wire 4 ── PWR_LED-  ◄────────│─            │
│                      │                                 │             │
└──────────────────────┘                                 └─────────────┘

Case front panel (power button, LED) also connects to the same
motherboard header pins — not through PadProxy.
PWR BTN labels use A/B instead of +/- (polarity agnostic).
PWR LED retains +/- markings (user must connect correctly).
```

### Polarity Agnosticism

The **power button trigger** circuit is fully polarity-agnostic: the TLP222A
photo-MOSFET conducts bidirectionally, so it works regardless of which PWR_BTN pin
the motherboard pulls high.

The **power LED sense** circuit requires correct polarity: the user connects
PWR_LED+ and PWR_LED- per silkscreen markings. Reverse connection causes no damage
(the PC817 LED is simply reverse-biased within its rated voltage) but disables sensing.

### Voltage Compatibility

Most ATX motherboards pull PWR_BTN to **5V** (from the 5VSB standby rail via the
SuperIO/EC chip); some use **3.3V**. The **RP2350 is NOT 5V tolerant** (absolute max
3.63V), so both front panel circuits use optocoupler isolation with no direct
electrical connection to the motherboard:

- **Button trigger (photo-MOSFET):** load side is fully isolated from the Pico 2 W;
  works at any voltage up to the TLP222A's 60V rating.
- **LED sense (PC817):** handles both 3.3V and 5V (~4.5mA and ~8.1mA through the
  LED respectively, both within spec), with full optical isolation.

### Power Button Trigger Circuit (Polarity-Agnostic)

Uses a **photo-MOSFET optocoupler** (e.g., TLP222A, AQY212, or CPC1017N). The MOSFET
output is inherently bidirectional, acting as a true analog switch regardless of which
direction current flows through the motherboard's PWR_BTN pins.

```
                    ┌──────────────────┐
Pico GPIO ──[330Ω]──►│ LED     MOSFET  │
                    │                  │
GND ─────────────────►│       (bidir)  │
                    └───┬──────────┬───┘
                        │          │
  To Mobo PWR_BTN_A ───┘          └─── To Mobo PWR_BTN_B
  (Wire 1)                              (Wire 2)
```

**Operation:**
- GPIO HIGH -> LED illuminates -> MOSFET turns on -> PWR_BTN pins shorted
- GPIO LOW -> MOSFET off -> PWR_BTN pins open (normal state)
- Pulse duration: 100-500ms (configurable in firmware)
- Complete optical isolation protects Pico 2 W from motherboard
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

### Power LED Sense Circuit (Optocoupler-Isolated)

Uses a **PC817 phototransistor optocoupler** to detect the motherboard's power LED
signal. Both PWR_LED+ and PWR_LED- are brought to PadProxy via the 4-wire cable,
providing a differential measurement with full optical isolation. No shared ground
reference between PadProxy and the motherboard is needed.

```
PWR_LED+ (Wire 3) ──[470Ω]──► PC817 LED anode
                                            │
PWR_LED- (Wire 4) ──────────► PC817 LED cathode


PC817 Output (phototransistor):
  VCC ──[10kΩ]──┬──► Pico GPIO (PWR_LED_SENSE, input)
                │
           [collector]
                │
              [GND]
```

**Operation:**
- Mobo LED output active → current flows through PC817 LED → phototransistor
  conducts → GPIO reads LOW → PC is ON
- Mobo LED output off → no current → phototransistor off → GPIO pulled HIGH
  by pull-up resistor → PC is OFF (or sleeping)
- **Logic is inverted**: GPIO HIGH = PC off, GPIO LOW = PC on
- Full optical isolation: motherboard voltage never reaches PadProxy GPIOs

**Voltage budget:**
- PC817 LED Vf: ~1.2V
- At 3.3V motherboard: (3.3V - 1.2V) / 470Ω = ~4.5mA (reliable, min ~1mA)
- At 5V motherboard: (5V - 1.2V) / 470Ω = ~8.1mA (well within limits)

**Polarity:** User must connect PWR_LED+ and PWR_LED- correctly per silkscreen.
Reverse connection causes no damage (PC817 LED is reverse-biased, max reverse
voltage rating 6V) but disables sensing.

---

## PC State Detection

### Detection Methods

| Method | Reliability | What It Detects | Notes |
|--------|-------------|-----------------|-------|
| Power LED sense | High | PC on/off | Primary method |
| USB enumeration | High | PC on + booted | Confirms OS running |
| USB VBUS monitoring | Medium | Power state changes | Supplementary |
| Physical button press | N/A | User interaction | Not monitored (covered by LED + USB) |

### State Machine

```
                    ┌──────────────────┐
                    │                  │
         ┌─────────►│    PC_OFF (S5)   │◄──────────┐
         │          │                  │           │
         │          └────────┬─────────┘           │
         │                   │                     │
         │          Controller HOME pressed        │
         │          or power LED on                │
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
1. Pico 2 W receives button press via Bluetooth
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
1. Pico 2 W receives button press via Bluetooth
2. Construct WoL magic packet (6x 0xFF + 16x MAC address)
3. Send UDP broadcast on port 9
4. NIC receives packet and signals motherboard
5. PC wakes from sleep/hibernate

**Requirements:**
- WoL enabled in BIOS
- WoL enabled in OS network adapter settings
- Pico 2 W connected to same network/VLAN
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
| FP_CABLE | 4-pin header (JST-XH or 2.54mm) | 4 | 4-wire cable to motherboard front panel header |
| 5VSB_IN | 2-pin header | 2 | Optional backup power |

### Connection Modes

PadProxy supports multiple connection configurations:

#### Mode A: Internal Install (Recommended)

```
Motherboard          9-pin        PadProxy
USB 2.0 Header ════► cable ════► 9-pin IN
                                     │
                                     └── Port 1 → Pico 2 W
```

#### Mode B: Rear USB with Adapter Cable

For systems where internal USB headers are fully occupied:

```
Rear USB          USB-A to       PadProxy
Port        ════► 9-pin cable ══► 9-pin IN
                                     │
                                     └── Port 1 → Pico 2 W

Requires: USB-A male to 9-pin female adapter cable
```

#### Mode C: Minimal / Development

For development, testing, or minimal installations:

```
Any USB           USB-C          PadProxy
Port        ════► cable    ════► USB-C port
                                     │
                                     └── Pico 2 W only
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

P1 = USB Port 1 (D-/D+ directly to Pico 2 W)
P2 = USB Port 2 (unused)
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
| MCU | Raspberry Pi Pico 2 W | Module | Main controller (RP2350 + CYW43439 BT/WiFi) | $7.00 |
| Active Power Mux | TPS2116 (or equivalent) | SOT-23-6 | Priority power source selection + backfeed blocking (replaces diode-OR) | $0.50 |
| Photo-MOSFET Optocoupler | TLP222A (or AQY212 / CPC1017N) | DIP-4/SOP-4 | Power button trigger (polarity-agnostic) | $0.60 |
| Optocoupler (sense) | PC817 | DIP-4/SMD | Power LED sense (optically isolated) | $0.10 |
| Addressable LED | WS2812B or SK6812 | 5050 | Status indicator (single-wire protocol) | $0.05 |
| IR LED | TSAL6200 or VSLY5850 | 5mm/SMD | 940nm IR emitter for TV control | $0.10 |
| NPN Transistor | 2N2222 or BC547 | TO-92/SOT-23 | IR LED current driver (~100mA) | $0.05 |

### Power Button Trigger Components

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| Photo-MOSFET optocoupler | TLP222A | 1 | Trigger - shorts motherboard PWR_BTN pins |
| Resistor | 330Ω | 1 | Current limit for trigger optocoupler LED |

### Power LED Sense Components

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| PC817 optocoupler | PC817 | 1 | Sense - detects power LED state (optically isolated) |
| Resistor | 470Ω | 1 | Current limit for sense optocoupler LED input |
| Resistor | 10kΩ | 1 | Pull-up for sense optocoupler output to Pico GPIO |

### IR Blaster Components

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| IR LED | TSAL6200 | 1 | 940nm high-power IR emitter |
| NPN transistor | 2N2222 / BC547 | 1 | Current driver for IR LED |
| Resistor | 1kΩ | 1 | Base current limit for NPN transistor |
| Resistor | 22Ω | 1 | IR LED current limit (~100mA peak at 3.3V) |

### Active Power Mux Components

| Component | Value | Qty | Purpose |
|-----------|-------|-----|---------|
| Active power mux IC | TPS2116 | 1 | Priority-based source selection with backfeed blocking (replaces diode-OR) |
| Resistor | 100kΩ | 2 | VBUS sense divider, top (one per input) |
| Resistor | 47kΩ | 2 | VBUS sense divider, bottom (one per input) |
| TVS diode | e.g. PESD5V0S1BA | 2 | Optional: surge/ESD protection on mux VBUS inputs (see [ESD Protection](#esd-protection)) |

### Connectors

| Component | Part Number | Purpose | Est. Cost |
|-----------|-------------|---------|-----------|
| USB 2.0 Header (F) | Generic 9-pin | Input from motherboard | $0.30 |
| USB-C Receptacle | Generic 16-pin | Alt connection | $0.40 |
| 4-pin connector | JST-XH or 2.54mm header | 4-wire cable to motherboard front panel header | $0.20 |

### 4-Wire Cable Connector

The PadProxy PCB has a single **4-pin connector** (JST-XH or 2.54mm pin header) for
the cable to the motherboard's front panel header. The cable carries two pairs of
signals: PWR_BTN trigger (wires 1-2) and PWR_LED sense (wires 3-4).

On the motherboard end, the cable terminates in individual DuPont-style 2-pin
connectors that plug into the standard front panel header pins alongside the case's
own front panel wires.

### Estimated Total BOM Cost

| Category | Cost | Notes |
|----------|------|-------|
| Active components | ~$8.40 | MCU, optocouplers, LED, IR LED, NPN transistor, TPS2116 active power mux |
| Passive components | ~$0.21 | Resistors, incl. IR driver and VBUS sense dividers |
| Connectors | ~$0.90 | USB header, USB-C, 4-pin header |
| PCB (qty 5) | ~$2.00 each | |
| **Total per unit** | **~$11.51** | |

**Delta vs. diode-OR:** adds one TPS2116 (~$0.50) and four sense-divider resistors
(~$0.04) for VBUS monitoring; removes the (previously uncosted, board-level) Schottky
diodes used for the passive OR. Net BOM impact is roughly +$0.54/unit for priority
selection, backfeed blocking, and VBUS source visibility.

---

## GPIO Assignments

### Pico 2 W Pin Allocation

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 2 | PWR_BTN_TRIGGER | Output | Photo-MOSFET optocoupler drive (TLP222A LED) |
| 3 | PWR_LED_SENSE | Input | PC817 output (active LOW = PC on, pull-up) |
| 4 | STATUS_LED | Output | WS2812B addressable LED data line |
| 5 | IR_BLASTER | Output | NPN transistor → IR LED (38 kHz carrier) |
| 26 (ADC0) | USB_IN_VBUS_SENSE | Input (analog) | Power mux IN1 sense, via 100kΩ/47kΩ divider |
| 27 (ADC1) | TAP_VBUS_SENSE | Input (analog) | Power mux IN2 sense, via 100kΩ/47kΩ divider |

**USB:** Handled by the Pico 2 W module internally. USB D+/D- are routed from the
module's castellated pads (or test points TP1/TP3) to the carrier PCB.

**Bluetooth:** Handled by the CYW43439 wireless module onboard the Pico 2 W. Uses
the onboard PCB antenna — no external antenna connector needed.

### Notes on GPIO Selection

1. **GPIO 2** - Power button trigger
2. **GPIO 3** - Power LED sense (optocoupler output)
3. **GPIO 4** - WS2812B addressable LED data
4. **GPIO 5** - IR blaster output (NPN transistor driver)
5. **Avoid GPIO 0, 1** - Reserved for UART TX/RX (debug output)
6. **GPIO 26-28 (ADC0-2)** - The only ADC-capable GPIOs the Pico 2 W module exposes;
   26 and 27 are used for VBUS sense (unused elsewhere on this board), leaving 28 free

---

## Design Considerations

### EMI/EMC

- Keep USB differential pairs matched length (±0.5mm)
- 90Ω differential impedance for USB traces
- Ground plane under USB traces
- Place bypass capacitors close to IC power pins

### Thermal

- Pico 2 W generates minimal heat in typical operation (~50mA)
- No special cooling required

### ESD Protection

- USBLC6-2 on USB data lines recommended
- TVS diode on 5V input recommended
- All front panel connections through optocouplers provide full galvanic isolation

Since PadProxy now moves between hosts (laptops, docks, other PCs) more often than a
one-time internal install, hot-plug robustness matters more than it used to. Recommend
a small TVS diode (e.g. PESD5V0S1BA) on each active-power-mux VBUS input, in addition to
the existing USBLC6-2 on data lines. Inrush current on repeated plug/unplug cycles
should also be kept modest — the TPS2116's internal current limiting helps here, and a
small bulk capacitor at each mux input is worth budgeting board area for.

### Mechanical

- PCB sized for internal mounting
- Mounting holes for M3 screws or standoffs
- Pico 2 W module can be castellated-soldered or socketed with pin headers

---

This document reflects the current PadProxy v1 hardware design (Pico 2 W based).
For the design's evolution, see git history. PCB v2 (custom RP2350B, separate
wireless module, USB-C only, dual-port active power mux) is tracked in
[pcb-v2-design.md](pcb-v2-design.md).
