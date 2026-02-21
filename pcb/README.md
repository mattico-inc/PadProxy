# PCB Design

KiCad project files for the PadProxy hardware.

## Board Features

### Microcontroller
- Raspberry Pi Pico 2 W module (RP2350 + CYW43439 BT/WiFi)
- Onboard antenna (no external antenna connector required)
- Castellated pads for direct soldering to carrier PCB (or pin-header socketed)

### USB Architecture
- **USB 2.0 9-pin header input** - Primary connection from motherboard
- **USB-C port** - Alternative connection for development/fallback
- **FE1.1s USB Hub IC** - Provides port passthrough for user devices
- **USB 2.0 9-pin header output** - Passthrough ports for user's existing USB devices

The Pico 2 W USB data lines connect directly to USB Port 1 for optimal HID latency. Port 2 feeds the hub IC, which provides passthrough ports so users don't lose USB connectivity.

### Power Management
- USB standby power (5VSB) support - stays powered when PC is off
- Optional 5VSB tap header for motherboards without USB standby
- Pico 2 W has an onboard 3.3V buck-boost regulator (no external LDO needed)

### Front Panel Interface
- **Power button passthrough** - Physical button still works
- **Power button trigger** - Photo-MOSFET optocoupler (TLP222A) for polarity-agnostic "press"
- **Power button sense** - Schottky bridge rectifier + PC817 optocoupler for polarity-agnostic detection
- **Power LED sense** - Detect PC power state
- All connections pass through to motherboard header
- **Polarity agnostic** - All active circuits work regardless of wire orientation

### Other Features
- Status LEDs (RGB)
- Optional OLED display header (I2C)
- Compact form factor for internal PC mounting

## Connectors

| Connector | Type | Purpose |
|-----------|------|---------|
| USB_IN | 9-pin header socket | From motherboard USB header |
| USB_OUT | 9-pin header pins | To user's USB devices |
| USB_C | USB-C receptacle | Alt connection / development |
| FP_IN | 2.54mm pads/header | From case front panel |
| FP_OUT | 2.54mm pads/header | To motherboard front panel |
| 5VSB | 2-pin header | Optional backup power |

## Key Components

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | Raspberry Pi Pico 2 W | Main controller (RP2350 + CYW43439 BT/WiFi) |
| Hub IC | FE1.1s | USB 2.0 hub (4-port) |
| Photo-MOSFET Optocoupler | TLP222A | Power button trigger (polarity-agnostic) |
| Optocoupler | PC817 | Power button sense (with bridge rectifier) |
| Schottky Diodes | BAT54S (x2) or BAT54 (x4) | Bridge rectifier for sense circuit |

## Files

- `PadProxy.kicad_pro` - KiCad project file
- `PadProxy.kicad_sch` - Schematic
- `PadProxy.kicad_pcb` - PCB layout
- `gerbers/` - Manufacturing files
- `bom/` - Bill of materials

## Manufacturing

Gerber files are provided for PCB fabrication. The design is optimized for:
- 2-layer PCB
- Standard 1.6mm thickness
- HASL or ENIG finish
- 90Ω differential impedance for USB traces

## Assembly

See `bom/` for component list and placement files for automated assembly.

## Design Documentation

See [Power Management Design](../docs/power-management-design.md) for detailed design rationale, GPIO assignments, and circuit descriptions.
