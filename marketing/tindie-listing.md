# Tindie Listing Draft

## Listing Title

**PadProxy -- Wireless Gamepad to USB with PC Wake**

(70 char limit on Tindie titles -- this is 49 chars)

## Short Description

Wake your PC with your gamepad. PadProxy bridges Bluetooth controllers to USB and wakes your PC when you press the home button. Open-source hardware.

## Full Description

### Wake. Connect. Play.

On a console, you press the button on the controller and you're gaming. On PC? Get up from the couch. Walk across the room. Press the power button. Walk back. Sit down. Pick up the controller. Wait for Bluetooth to pair. Hope it actually connects this time.

Nothing existed to fix this, so we built PadProxy.

**PadProxy** is a tiny open-source device that mounts inside your PC case. It connects to your Bluetooth gamepad wirelessly and appears to your PC as a standard wired USB controller. When your PC is off or sleeping, just press the home button on your controller -- PadProxy wakes the PC and you're gaming in seconds. Console-level convenience on your gaming PC.

### How It Works

1. Install PadProxy inside your PC case (connects to front panel header + USB)
2. Pair your Bluetooth controller once
3. When you want to play, press the home button on your controller
4. PadProxy wakes the PC by triggering the power button
5. Your controller input flows through PadProxy as a standard USB gamepad
6. The PC sees a regular wired controller -- zero driver issues, works with every game

### Supported Controllers

- Xbox Wireless Controllers (Xbox One, Series X|S)
- PlayStation DualShock 4 and DualSense
- Nintendo Switch Pro Controller
- 8BitDo controllers (Pro 2, Ultimate, etc.)
- Generic Bluetooth HID gamepads

### Key Features

- **PC Wake:** Press the home button to wake your PC from off or sleep -- the thing that should have existed years ago
- **Zero Drivers:** Appears as a standard USB HID gamepad -- works out of the box on Windows, Linux, macOS
- **Bluetooth to USB Bridge:** Reliable, low-latency controller input without the pairing headaches
- **Internal Mount:** Sits inside your PC case, out of sight
- **Fully Open Source:** Hardware design files, firmware, enclosure -- everything is on GitHub (MIT + CERN-OHL-P-2.0). Build your own or contribute improvements.
- **Based on Raspberry Pi Pico 2 W:** Well-documented, hackable platform

### What's in the Box

**Assembled Version:**
- PadProxy board (Raspberry Pi Pico 2 W + carrier PCB) in 3D printed enclosure
- USB-A cable (connects to PC motherboard internal header)
- Front panel passthrough cable
- Mounting hardware (zip tie mounts or double-sided tape)
- Quick start card with QR code to online docs

**Kit Version:**
- PadProxy carrier PCB (SMD components pre-soldered)
- Through-hole components + pin headers
- Front panel passthrough cable
- Quick start card
- *(Raspberry Pi Pico 2 W not included -- available from most electronics retailers)*

### Specifications

- **MCU:** Raspberry Pi Pico 2 W (RP2350 + CYW43439 BT/WiFi)
- **Input:** Bluetooth Classic (gamepad profile)
- **Output:** USB 2.0 Full Speed (HID gamepad device)
- **Power:** USB bus powered (from PC motherboard)
- **Dimensions:** [TBD] mm x [TBD] mm x [TBD] mm
- **PC Interface:** Front panel header passthrough (directly triggers power button)

### Installation

Detailed setup guide with photos available on GitHub. Installation takes about 10 minutes:
1. Connect USB cable to an internal motherboard USB header
2. Splice PadProxy into the front panel power button header (passthrough -- your power button still works normally)
3. Mount with included hardware
4. Power on PC, pair your controller once via the pairing process in the docs

### Open Source

PadProxy is fully open source. You don't have to buy one -- everything you need to build your own is in the repo:
- **Firmware:** MIT License -- [GitHub link]
- **Hardware:** CERN Open Hardware Licence v2 (Permissive) -- [GitHub link]
- KiCad schematic and PCB files included
- 3D printable enclosure files (STL + source) included
- Contributions welcome!

We sell assembled units and kits for anyone who'd rather not source parts and solder. Pricing covers components and assembly -- this is a passion project, not a business.

---

## Listing Metadata

**Category:** Electronics > Microcontrollers
**Tags:** raspberry pi, pico, gamepad, controller, bluetooth, usb, hid, pc gaming, couch gaming, open source hardware

## Pricing

| SKU | Price | Shipping (US) | Shipping (International) |
|-----|-------|---------------|--------------------------|
| Assembled | $42 | $5 | $12 |
| Kit (no Pico) | $22 | $4 | $9 |

Pricing covers BOM + fabrication + assembly + Tindie's cut. The design files are free on GitHub for anyone who wants to DIY the whole thing.

## Photos Needed (in order of importance)

1. Hero shot: device on clean background, slight angle, logo visible
2. Scale shot: device next to a quarter or USB drive
3. Installed shot: device mounted inside a PC case
4. Use case shot: person on couch with controller, PC in background
5. Components shot: board close-up showing Pico 2 W + carrier PCB
6. What's in the box: all included items laid out neatly
7. Controller lineup: device with 3-4 supported controllers around it
