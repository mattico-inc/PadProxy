# Documentation

Technical design documentation for PadProxy hardware and firmware.

## Contents

- **[power-management-design.md](power-management-design.md)** - Current hardware design: power architecture, front panel interface, GPIO assignments
- **[device-setup-design.md](device-setup-design.md)** - USB CDC setup interface and device config storage
- **[pcb-v2-design.md](pcb-v2-design.md)** - Proposed next PCB revision (custom RP2350B board, USB-C only) — not yet built

## Supported Controllers

PadProxy aims to support:
- Xbox Wireless Controllers
- PlayStation DualShock 4 / DualSense
- Nintendo Switch Pro Controller
- 8BitDo controllers
- Generic Bluetooth gamepads

## Quick Start

1. Install PadProxy inside your PC case
2. Connect USB cable to motherboard internal header or rear USB
3. Connect 4-wire cable to motherboard front panel header (PWR_BTN + PWR_LED)
4. Power on and pair your controller
5. Your PC will wake automatically when you press the controller's home button
