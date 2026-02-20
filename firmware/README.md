# Firmware

Raspberry Pi Pico 2 W (RP2350) firmware for PadProxy.

## Structure

- `src/` - Source code
- `include/` - Header files

## Features

- Bluetooth gamepad pairing and connection
- USB HID device emulation (gamepad proxy to PC)
- PC power management (power button control)

## Building

This project uses the Pico SDK with CMake. First, clone Bluepad32 into `lib/`:

```bash
cd firmware
git clone https://gitlab.com/ricardoquesada/bluepad32.git lib/bluepad32
```

Then build:

```bash
mkdir build && cd build
cmake -DPICO_BOARD=pico2_w ..
make -j$(nproc)
```

To flash, hold BOOTSEL on the Pico 2 W while plugging it in, then copy:

```bash
cp build/padproxy.uf2 /media/$USER/RP2350/
```

## Running Tests

Unit tests run natively on the host (no hardware required):

```bash
make test
```

## Dependencies

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.0+ (set `PICO_SDK_PATH` or `PICO_SDK_FETCH_FROM_GIT=ON`)
- [Bluepad32](https://gitlab.com/ricardoquesada/bluepad32) (Bluetooth gamepad library)
- TinyUSB (bundled with Pico SDK)
