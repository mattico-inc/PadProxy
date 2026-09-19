# PadProxy

Open source hardware that manages PC power and gamepad connectivity for a console-like experience.

## What is PadProxy?

PadProxy is a Raspberry Pi Pico 2 W based device that lives inside your PC case. It pairs with your Bluetooth controller and re-presents it to the PC as a standard USB HID gamepad — and because it stays powered even when the PC is off, pressing the controller's guide button can wake the PC itself.

### What It Does

- **Bluetooth gamepad proxy** - Pairs with Xbox, PlayStation, Switch Pro, 8BitDo, and other Bluetooth HID controllers and forwards their input to the PC as a native USB gamepad
- **Wake from off or sleep** - Pressing the controller's guide/home button pulses the motherboard's power button header, waking the PC from full power-off (S5) or sleep
- **PC state tracking** - Senses the motherboard's power LED and watches USB enumeration to know when the PC is off/booting/on, so it only forwards input once the PC is actually ready
- **Over-the-air firmware updates** - Checks GitHub Releases over WiFi and applies updates using the RP2350's built-in safe-rollback (A/B partition) mechanism
- **USB-serial setup** - Configure WiFi credentials and power/boot timing over a plain USB CDC serial connection — no separate app required
- **Open Source** - Hardware and firmware are fully open source

## How It Works

1. PadProxy stays powered via USB standby power (5VSB), even when the PC is fully off
2. Your Bluetooth controller pairs with PadProxy
3. Press the controller's guide/home button — PadProxy pulses the motherboard's power button header to wake the PC
4. PadProxy watches the motherboard's power LED and USB enumeration to confirm the PC has booted
5. Once the PC is on, controller input is forwarded as a standard USB HID gamepad report

## Project Structure

```
PadProxy/
├── firmware/     # RP2350 firmware (Pico SDK / CMake)
├── pcb/          # KiCad PCB design files
├── case/         # 3D printable enclosure
├── design/       # Logos, graphics, renders
└── docs/         # Documentation and guides
```

## Hardware Requirements

- Raspberry Pi Pico 2 W (RP2350 with CYW43439 wireless)
- USB connection to the PC (internal header or rear port)
- 4-wire connection to the motherboard's front panel header (power button trigger + power LED sense), optically isolated on the PadProxy PCB

## Getting Started

See the [documentation](docs/README.md) for setup instructions.

## Supported Controllers

- Xbox Wireless Controller
- PlayStation DualShock 4 / DualSense
- Nintendo Switch Pro Controller
- 8BitDo controllers
- Most Bluetooth HID gamepads

## License

This project is open source hardware. See [LICENSE](LICENSE) for details.

## Contributing

Contributions welcome! Please open an issue or pull request.

## Buy on Tindie

Coming soon!
