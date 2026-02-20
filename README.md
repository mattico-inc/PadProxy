# PadProxy

Open source hardware that manages PC power and gamepad connectivity for a console-like experience.

## What is PadProxy?

PadProxy is a Raspberry Pi Pico 2 W based device that lives inside your PC case and provides seamless wireless gamepad connectivity. Simply press the home button on your controller and your PC wakes up, ready to play.

### Key Features

- **Wireless Gamepad Support** - Connect Xbox, PlayStation, Switch Pro, and other Bluetooth controllers
- **Automatic PC Wake** - Press a button on your controller to wake your PC from sleep/off
- **USB Proxy** - Your gamepad appears as a native USB controller to the PC
- **External Antenna** - Route the WiFi/BT antenna outside your case for better range
- **Internal Mounting** - Designed to install cleanly inside a PC case
- **Open Source** - Hardware and firmware are fully open source

## How It Works

1. PadProxy stays powered via USB standby power (or optional external power)
2. Your Bluetooth controller connects to PadProxy
3. When you press a button, PadProxy wakes the PC via power button header or Wake-on-LAN
4. Controller input is forwarded to the PC as a standard USB gamepad
5. Enjoy your games with minimal latency

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
- USB connection to PC
- Connection to PC power button header

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
