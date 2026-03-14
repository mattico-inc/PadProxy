# PCB v2 Design Notes

This document captures the design direction and open questions for PadProxy PCB v2, which
moves from a Raspberry Pi Pico 2 W module to a fully custom board.

## Table of Contents

1. [Motivation](#motivation)
2. [Key Changes from v1](#key-changes-from-v1)
3. [Custom MCU + Wireless Architecture](#custom-mcu--wireless-architecture)
4. [USB Connector Strategy](#usb-connector-strategy)
5. [Dual-Port Power Management](#dual-port-power-management)
6. [Open Design Questions](#open-design-questions)

---

## Motivation

V1 uses the Raspberry Pi Pico 2 W as the MCU module, which bundles the RP2350 and
CYW43439 wireless chip onto a pre-certified sub-module. This works well for prototyping
but has limitations for a production custom board:

- **Form factor**: The Pico 2 W module footprint (51×21mm) dominates the PCB area.
  Integrating the RP2350 directly allows a much more compact layout.
- **Connector constraints**: The Pico 2 W exposes USB only through its own micro-USB
  port. A custom board can route USB data to any connector geometry, specifically USB-C.
- **Power routing**: On the Pico 2 W, VSYS and VBUS are commingled through the onboard
  RT6154 buck-boost. A custom board can implement a proper power mux before the regulator.
- **Cost at scale**: Individual ICs (RP2350B + wireless module) are cheaper than the
  Pico 2 W module at volume.

---

## Key Changes from v1

| Feature | V1 | V2 |
|---------|----|----|
| MCU | Pico 2 W module (castellated) | Bare RP2350B QFN-60 |
| Wireless | CYW43439 (on Pico module) | Separate certified wireless module |
| USB native port | Pico 2 W micro-USB (internal) | USB-C |
| Secondary port | USB-C receptacle | USB-C receptacle |
| Power mux | Diode OR (passive) | Active power mux IC |
| USB connector legacy | 9-pin header + USB-C | USB-C only (drop 9-pin header) |
| IR blaster | GPIO 5 + NPN + through-hole LED | GPIO + NPN + SMD IR LED (board-edge) |
| WiFi | Persistent STA via CYW43439 | Same (via wireless module SPI) |

---

## Custom MCU + Wireless Architecture

### MCU: RP2350B

Use the **RP2350B** (QFN-60) directly. The B variant has more GPIOs than the A (48 vs
30 user GPIOs), giving headroom for future features without changing the footprint.

Key design considerations:
- **Crystal**: 12 MHz crystal required (same as Pico 2 W); place with minimal trace
  length to XIN/XOUT.
- **Flash**: 16 Mbit (2 MB) QSPI NOR flash (e.g., W25Q16JV) is sufficient; same as
  Pico 2 W.
- **Decoupling**: Follow RP2350 hardware design guide — 100nF on each IOVDD pin, bulk
  10µF on 3V3 rail.
- **USB data lines**: Route directly from DP/DM pins to the native USB-C port.
  No isolation needed on the USB data pair — the RP2350 is the USB device.
- **SWD debug**: Expose SWDIO/SWDCLK on a small 3-pin header (or test pads) for
  programming and debugging without requiring USB bootloader.

### Wireless Module

Rather than placing the CYW43439 die directly (which requires a Bluetooth/WiFi antenna
design and RF certification from scratch), use a **small certified wireless module** that
exposes an SPI or UART interface.

Candidate modules (all pre-certified, small footprint):

| Module | Chip | Interface | Size | Notes |
|--------|------|-----------|------|-------|
| Murata LBAD0ZZ1WR2 | CYW43439 | SPI | 9.1×6.0mm | Exact chip used in Pico 2 W |
| U-blox ANNA-B402 | nRF52833 | UART/SPI | 9.1×8.0mm | BT 5.1, no WiFi |
| Espressif ESP32-C3-MINI-1 | ESP32-C3 | SPI/UART | 13.2×12.5mm | BT 5.0 + 2.4GHz WiFi |

**Preferred:** Murata LBAD0ZZ1WR2 (or the equivalent module that the Pico 2 W uses
internally). This preserves Bluepad32 compatibility since Bluepad32 already targets the
CYW43439 via the btstack SPI transport. No firmware changes needed beyond hardware init.

**Interface to RP2350B:** SPI + a few control GPIOs (CS, IRQ, RESET, BT_REG_ON).
Match the existing Pico SDK / cyw43-driver pin assignments so the firmware can use the
same driver without modification.

---

## USB Connector Strategy

### Drop USB Mini and Micro

V2 uses **USB-C exclusively**. No USB mini-B or micro-B connectors anywhere on the board.
Reasons:
- USB-C is the current standard for new hardware designs.
- Better mechanical durability (no top/bottom orientation, symmetrical).
- USB-C can carry USB 2.0, power delivery negotiation, and still be routed to the
  motherboard via a USB-C to 9-pin header cable (these adapter cables are widely
  available).

### Two USB-C Ports

V2 has **two USB-C receptacles**:

| Port | Label | Purpose |
|------|-------|---------|
| `USB_NATIVE` | "PC" | RP2350 USB device lines — connects to motherboard; carries HID gamepad data |
| `USB_PWR` | "PWR" | Power input only — external 5V supply when PC is off / dev use |

`USB_NATIVE` wires D+/D- directly to the RP2350B USB DP/DM pins. This port provides
power to the board when the PC is on. When the PC is off, VBUS on this port is 0V (or
5VSB depending on motherboard settings).

`USB_PWR` carries only VBUS and GND to the power mux; the D+/D- lines are left open or
pulled to a fixed configuration to identify as a charging port (CDP/DCP) so USB power
adapters provide maximum current.

---

## Dual-Port Power Management

### Problem Statement

The device must stay powered at all times — including when the PC is fully off — to
receive Bluetooth controller input and trigger a PC wake. Power can come from:

1. **`USB_NATIVE` VBUS** (5V from the motherboard's USB port) — available when PC is
   on, may also be available as 5VSB when PC is off (depends on motherboard BIOS setting).
2. **`USB_PWR` VBUS** (5V from an external USB charger, USB hub, or 5VSB tap cable) —
   always available if connected; the "backup" supply for when the PC is off.

A passive diode OR (v1 approach) has drawbacks:
- Forward voltage drop (~0.3–0.6V for Schottky) reduces VSYS headroom.
- Higher-impedance source "wins" in edge cases; no visibility into which source is active.
- No protection against backfeeding 5V into a powered-off PC's USB port.

### Active Power Mux

V2 uses an **active power mux IC** to select between the two supplies with zero forward
voltage drop, intelligent priority, and backfeed blocking.

**Recommended part:** TPS2116 (Texas Instruments) or equivalent.

Key properties:
- Automatic priority selection (higher-voltage input wins, or fixed priority via SEL pin).
- Near-zero dropout vs. diode OR.
- Integrated reverse current blocking on both inputs — prevents backfeeding.
- Adjustable priority threshold via external resistors.
- Small package (SOT-23-6 or WSON-6, ~1.5×1.5mm).

#### Power Mux Strategy

```
USB_NATIVE VBUS ──► IN1 ─┐
                          ├──► TPS2116 ──► VSYS ──► 3V3 Regulator ──► RP2350B
USB_PWR VBUS    ──► IN2 ─┘               (power mux output)           CYW module
                                                                        Peripherals
```

**Priority logic (fixed):**

| USB_NATIVE VBUS | USB_PWR VBUS | Selected Source | Notes |
|-----------------|--------------|-----------------|-------|
| 5V (PC on) | Any | `USB_NATIVE` | PC is on; use its USB power |
| 5VSB (PC off, BIOS enabled) | Any | `USB_NATIVE` | Motherboard provides standby |
| 0V (PC off, no 5VSB) | 5V | `USB_PWR` | External supply keeps device alive |
| 0V | 0V | None | Device off (no power available) |

Set TPS2116 SEL to prefer IN1 (`USB_NATIVE`) when both are present above the threshold.
If IN1 falls below ~4.5V, TPS2116 automatically switches to IN2 (`USB_PWR`) with no
glitch on VSYS.

#### Backfeed Protection

When the PC is off and `USB_PWR` is active, the TPS2116 blocks current from flowing
back through IN1 into the motherboard's USB port. This is critical — without it, an
external 5V supply could backfeed into the PC and cause issues.

#### VBUS Monitoring

Expose both VBUS rails to RP2350B ADC or GPIO pins (through a voltage divider to stay
within 3.3V ADC range) so firmware can:
- Know which power source is active.
- Detect PC power-on event via `USB_NATIVE` VBUS rising edge (faster than USB enumeration).
- Expose power source status via the WS2812B LED.

Suggested GPIOs:
| GPIO | Signal | Notes |
|------|--------|-------|
| 5 | IR_BLASTER | NPN transistor → IR LED (38 kHz carrier via PIO) |
| 6 | IR_RECEIVER (optional) | TSOP38238 for future IR learning |
| ADC0 (GPIO 26) | USB_NATIVE_VBUS_SENSE | Divider: 100kΩ / 47kΩ → ÷3.1 (5V → 1.6V) |
| ADC1 (GPIO 27) | USB_PWR_VBUS_SENSE | Same divider |

### 3V3 Regulator

Replace the Pico 2 W's onboard RT6154 buck-boost with an external regulator on the
custom board. Since VSYS is always ≥4.5V (guaranteed by the TPS2116 selection threshold),
a simple synchronous buck regulator is sufficient — no need for a boost stage.

**Recommended:** AP2112K (LDO, 600mA, SOT-25) for simplicity, or TPS62827 (buck, 2A)
if efficiency matters during BT active periods.

Power budget is ~170mA peak, so either part is adequate.

---

## Open Design Questions

### Q1: Wireless Module Final Selection

Decide between:
- **Murata LBAD0ZZ1WR2** (CYW43439): preserves Bluepad32 driver compatibility exactly,
  but Murata modules can have long lead times.
- **Alternative BT-only module** (e.g., ANNA-B402 or BM83): drops WiFi (not needed for
  v1 use case) for a smaller BOM; would require replacing Bluepad32 with a different BT
  stack or porting.

Current preference: CYW43439-based module for zero firmware risk, revisit if supply
availability is a problem.

### Q2: 9-Pin Header Compatibility

V1 ships with a USB 2.0 9-pin header socket so the device connects internally without
any adapter. V2 drops this for USB-C only.

Consequence: users who want an internal install need a **USB-C to 9-pin header cable**
(these exist but are less common than USB-C cables). Alternatively, include such a cable
in the kit or add a 9-pin footprint as DNP.

Decide before layout freeze.

### Q3: USB-C Power Delivery on USB_PWR

If `USB_PWR` uses a USB-C PD adapter, the device will only draw 5V/0.9A at first (USB
default current) unless PD negotiation occurs. At ~170mA peak the default current limit
is fine, but adding a PD controller (e.g., FUSB302) would allow explicit 5V/3A contract
and remove any ambiguity.

Current preference: skip PD controller; 900mA is more than sufficient. Pull CC1/CC2 to
GND through 5.1kΩ resistors (UFP default current advertisement per USB-C spec) so
chargers recognize it as a standard sink.

### Q4: USB ESD Protection

V1 notes recommend USBLC6-2 on USB data lines. Confirm placement for both USB-C ports
in v2 schematic.

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-12 | Initial v2 design notes: custom RP2350B, separate wireless module, USB-C only, active power mux |
