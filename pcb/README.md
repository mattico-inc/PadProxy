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

The Pico 2 W USB data lines connect directly to USB Port 1 for optimal HID latency.

### Power Management
- USB standby power (5VSB) support - stays powered when PC is off
- Optional 5VSB tap header for motherboards without USB standby
- Pico 2 W has an onboard 3.3V buck-boost regulator (no external LDO needed)

### Front Panel Interface (4-Wire Cable)
- **Power button trigger** - Photo-MOSFET optocoupler (TLP222A) for polarity-agnostic "press"
- **Power LED sense** - PC817 optocoupler for optically-isolated PC state detection
- **4-wire cable** to motherboard front panel header (PWR_BTN trigger + PWR_LED sense)
- **Full optical isolation** on all signals between PadProxy and motherboard

### Other Features
- WS2812B addressable status LED (single GPIO, any color)
- Compact form factor for internal PC mounting

## Connectors

| Connector | Type | Purpose |
|-----------|------|---------|
| USB_IN | 9-pin header socket | From motherboard USB header |
| USB_C | USB-C receptacle | Alt connection / development |
| FP_CABLE | 4-pin header | 4-wire cable to motherboard front panel header |
| 5VSB | 2-pin header | Optional backup power |

## Key Components

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | Raspberry Pi Pico 2 W | Main controller (RP2350 + CYW43439 BT/WiFi) |
| Photo-MOSFET Optocoupler | TLP222A | Power button trigger (polarity-agnostic) |
| Optocoupler | PC817 | Power LED sense (optically isolated) |
| Addressable LED | WS2812B / SK6812 | Status indicator |

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
