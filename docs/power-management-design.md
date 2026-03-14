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
4. **Simple installation** - 4-wire cable to motherboard, no passthrough needed
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
USB 2.0 Header ─── 5V ──►│──┬──► VSYS ──► Pico 2 W        │
(Primary)                │  │    (onboard 3.3V regulator)  │
                         │  │                              │
5VSB Tap (Optional) ────►│──┘ (diode OR)                   │
                         │                                 │
                         └─────────────────────────────────┘
```

**Note:** Simple diode OR configuration allows either source to power the device. When both present, higher voltage source wins (minimal difference in practice). The Pico 2 W has an onboard buck-boost regulator (RT6154) that generates 3.3V from VSYS, so no external LDO is needed.

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

**Advantage over previous Zener clamp design:** Full optical isolation eliminates
any ground reference dependency between PadProxy and the motherboard. Inherently
5V tolerant without a Zener diode, since the optocoupler provides galvanic separation.

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
| Active components | ~$7.90 | MCU, optocouplers, LED, IR LED, NPN transistor |
| Passive components | ~$0.17 | Resistors (5× total) |
| Connectors | ~$0.90 | USB header, USB-C, 4-pin header |
| PCB (qty 5) | ~$2.00 each | |
| **Total per unit** | **~$10.97** | |

---

## GPIO Assignments

### Pico 2 W Pin Allocation

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 2 | PWR_BTN_TRIGGER | Output | Photo-MOSFET optocoupler drive (TLP222A LED) |
| 3 | PWR_LED_SENSE | Input | PC817 output (active LOW = PC on, pull-up) |
| 4 | STATUS_LED | Output | WS2812B addressable LED data line |
| 5 | IR_BLASTER | Output | NPN transistor → IR LED (38 kHz carrier) |

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

### Mechanical

- PCB sized for internal mounting
- Mounting holes for M3 screws or standoffs
- Pico 2 W module can be castellated-soldered or socketed with pin headers

---

## Design Decisions (Resolved)

### D1: Power LED Sense (RESOLVED → REVISED v0.5)

**Decision: Use PC817 optocoupler for full optical isolation.** Both PWR_LED+ and
PWR_LED- are brought to PadProxy via the 4-wire cable. The optocoupler provides
galvanic separation and inherent 5V tolerance without a Zener diode. User connects
+/- correctly per silkscreen. Reverse connection causes no damage but disables sensing.

*Previous (v0.3): Zener clamp with resistive sense. Changed because the 4-wire
connection eliminates shared ground, making a direct resistive sense impossible.*

### D2: Reset Button Trigger (RESOLVED → REMOVED v0.5)

**Decision: Removed entirely.** Not worth the complexity and extra wires. Long-press
power button handles forced shutdown. The 4-wire cable design has no passthrough, so
there is no natural place to intercept the reset signal.

*Previous (v0.3): Optional DNP footprint with TLP222A. Removed to simplify the
4-wire external connector design.*

### D3: Front Panel Connector Format (RESOLVED → REVISED v0.5)

**Decision: Single 4-pin connector** (JST-XH or 2.54mm header) on the PadProxy PCB.
The cable terminates in individual DuPont-style 2-pin connectors on the motherboard
end, plugging into the standard front panel header alongside the case's own wires.

*Previous (v0.3): Dual-footprint per signal (header + screw terminal) for 8+ wires.
Simplified to 4-wire cable with single connector.*

### D4: Voltage Compatibility (RESOLVED)

**Research findings:** Most ATX motherboards pull PWR_BTN to **5V** (from the 5VSB
standby rail via the SuperIO/EC chip). Some use **3.3V**. No evidence of voltages
below 3.3V on standard ATX boards. Some industrial boards (e.g., ASRock IMB-140D)
offer jumper selection between 3.3V and 5V.

**Impact on design:**
- **Button trigger circuit (photo-MOSFET):** Load side is completely isolated from
  the Pico 2 W. Works at any voltage up to the TLP222A's 60V rating.
- **LED sense circuit (optocoupler):** PC817 input handles both 3.3V and 5V.
  At 3.3V: ~4.5mA through LED (reliable). At 5V: ~8.1mA (well within limits).
  Full optical isolation, so motherboard voltage never reaches PadProxy.
- **RP2350 is NOT 5V tolerant** (absolute max 3.63V). All GPIO connections to
  the motherboard use optocoupler isolation — no direct electrical connection.

### D5: Power Button Sense (RESOLVED v0.5)

**Decision: Removed.** Physical button press detection is unnecessary. The power LED
sense and USB enumeration/suspension signals fully cover all PC state transitions.
The button sense circuit (Schottky bridge rectifier + PC817) only provided marginally
faster state tracking (~ms) at the cost of 6 extra components and 2 GPIOs.

### D6: OLED Display Header (RESOLVED v0.5)

**Decision: Removed.** No practical use case for a display on an internal PC device.
The WS2812B addressable LED provides sufficient status indication (multiple colors,
patterns) using a single GPIO instead of 2 (I2C SDA/SCL).

### D7: Status LED (RESOLVED v0.5)

**Decision: WS2812B addressable LED.** Replaces 3 discrete LEDs + 3 resistors with
a single integrated package. Any color, 1 GPIO (data line), ~$0.05. The WS2812B
protocol is well-supported with simple bit-banging or PIO on RP2350.

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
| 0.4 | 2026-02-21 | Migrated from ESP32-S3-WROOM-1 to Raspberry Pi Pico 2 W (RP2350). Removed external LDO and antenna connector. Updated GPIO assignments, power budget, BOM, and all circuit descriptions. |
| 0.5 | 2026-02-22 | Simplified front panel interface: removed power button sense circuit (bridge rectifier + PC817), replaced LED Zener clamp with PC817 optocoupler for full optical isolation, 4-wire cable design (PWR_BTN trigger + PWR_LED sense). Removed reset trigger, OLED header. Replaced discrete RGB LEDs with WS2812B addressable LED. Reduced GPIO count from 8 to 3. Updated BOM, connector specs, design decisions D1-D7. |
| 0.6 | 2026-03-14 | Added IR blaster circuit (GPIO 5, NPN transistor + IR LED) for TV power control. Added IR components to BOM. GPIO count now 4. See [wifi-ir-smarthome-design.md](wifi-ir-smarthome-design.md) for WiFi service, IR protocol, and smart home integration details. |

**Note:** PCB v2 (custom RP2350B, separate wireless module, USB-C only, active power mux)
is tracked in [pcb-v2-design.md](pcb-v2-design.md).
