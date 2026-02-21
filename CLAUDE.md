# CLAUDE.md

## Project Overview

PadProxy is an open-source hardware device (Raspberry Pi Pico 2 W / RP2350) that lives inside a PC case and provides wireless Bluetooth gamepad connectivity with automatic PC wake. Press the home button on a controller and the PC wakes up; controller input is proxied to the PC as a standard USB HID gamepad.

**License:** MIT (software/firmware), CERN-OHL-P-2.0 (hardware)

## Repository Structure

```
PadProxy/
├── firmware/          # RP2350 firmware (C11, Pico SDK + CMake)
│   ├── src/           # Source files
│   ├── include/       # Public headers
│   ├── test/          # Native unit tests (Unity framework)
│   ├── CMakeLists.txt # Pico SDK cmake config (cross-compile for RP2350)
│   ├── Makefile       # Top-level build: firmware + unit tests
│   └── pico_sdk_import.cmake
├── pcb/               # KiCad PCB design files
├── case/              # 3D printable enclosure (FreeCAD/STL)
├── design/            # Logos, graphics, renders
├── docs/              # Documentation and design docs
│   └── power-management-design.md  # Detailed hardware design rationale
└── .github/workflows/ # CI configuration
```

## Build & Test Commands

### Unit Tests (primary development workflow)

Tests run natively on the host machine (no hardware required):

```bash
cd firmware && make test
```

This compiles and runs all test binaries under `firmware/build/test/`. The test framework is Unity (vendored in `firmware/test/unity/`).

### Firmware Build (cross-compile for RP2350)

Requires `arm-none-eabi-gcc` (the ARM cross-compiler). Pico SDK and Bluepad32 are cloned automatically on first build:

```bash
cd firmware && make firmware
```

Dependencies are cloned to `firmware/lib/` (gitignored). To clone them separately: `make deps`. If you have the Pico SDK installed elsewhere, override with `PICO_SDK_PATH`:

```bash
PICO_SDK_PATH=~/pico-sdk make firmware
```

Output: `firmware/build/firmware/padproxy.uf2`

### Flashing

Hold BOOTSEL on the Pico 2 W, plug in USB, then:

```bash
cd firmware && make flash
```

Or manually: drag `padproxy.uf2` onto the mounted `RP2350` / `RPI-RP2` drive.

### CI

GitHub Actions runs on pushes to `main` and PRs touching `firmware/**`:
- **Unit Tests:** `make test` in `firmware/`
- **Build Firmware:** CMake cross-compile for RP2350 (Pico 2 W)

## Architecture

### Firmware Modules

The firmware follows a layered architecture with clear separation between hardware and logic:

| Module | Files | Purpose |
|--------|-------|---------|
| **Main loop** | `src/main.c` | Initialization, polling loop (~1ms period), event dispatch |
| **Bluetooth gamepad** | `src/bt_gamepad.c`, `include/bt_gamepad.h` | Bluepad32 platform integration, BT controller pairing/input |
| **USB HID gamepad** | `src/usb_hid_gamepad.c`, `include/usb_hid_gamepad.h` | TinyUSB device, USB descriptor callbacks, report sending |
| **USB HID report** | `src/usb_hid_report.c`, `include/usb_hid_report.h` | HID report descriptor, wire-format conversion (pure logic) |
| **Gamepad common** | `include/gamepad.h` | Canonical `gamepad_report_t` struct, button/dpad definitions |
| **PC power state machine** | `src/pc_power_state.c`, `include/pc_power_state.h` | Pure-logic state machine (OFF/BOOTING/ON/SLEEPING) |
| **PC power HAL** | `src/pc_power_hal.c`, `include/pc_power_hal.h` | RP2350 GPIO/timer implementation; interface mocked in tests |
| **TinyUSB config** | `src/tusb_config.h` | USB device class configuration (HID only) |

### Data Flow

```
BT Controller → Bluepad32 → bt_gamepad (critical section) → main loop
    → gamepad_report_t → usb_hid_report conversion → TinyUSB → USB Host (PC)
```

### PC Power State Machine

The state machine in `pc_power_state.c` is a pure-logic module with no hardware dependencies. It takes events as input and produces actions as output:

- **States:** `PC_STATE_OFF` → `PC_STATE_BOOTING` → `PC_STATE_ON` → `PC_STATE_SLEEPING`
- **Events:** Button presses, USB enumeration/suspension, power LED changes, boot timeout
- **Actions (bitmask):** `PC_ACTION_TRIGGER_POWER`, `PC_ACTION_START_BOOT_TIMER`, `PC_ACTION_CANCEL_BOOT_TIMER`

The HAL (`pc_power_hal.h` / `pc_power_hal.c`) abstracts GPIO and timer hardware. The RP2350 implementation uses Pico SDK alarm APIs for non-blocking power button pulses and boot timeouts. Tests exercise the state machine directly without needing the HAL.

### Thread Safety

Bluepad32 callbacks run on its internal task. `bt_gamepad.c` uses a Pico SDK `critical_section_t` (spinlock) to protect shared report data read by the main loop.

## Code Conventions

### Language & Standard

- **C11** (`-std=c11`), compiled with `-Wall -Wextra -Werror` for tests
- No C++ in firmware source (CMakeLists.txt enables CXX for Pico SDK compatibility only)

### Naming

- **Types:** `snake_case_t` suffix (e.g., `gamepad_report_t`, `pc_power_sm_t`)
- **Enums:** `UPPER_SNAKE_CASE` with module prefix (e.g., `PC_STATE_OFF`, `GAMEPAD_BTN_A`)
- **Functions:** `module_verb_noun()` pattern (e.g., `pc_power_sm_process()`, `bt_gamepad_get_report()`)
- **Static globals:** `s_` prefix (e.g., `s_power_sm`, `s_reports`)
- **Constants/defines:** `UPPER_SNAKE_CASE` (e.g., `BT_GAMEPAD_MAX`, `POWER_PULSE_MS`)
- **Header guards:** `FILENAME_H` pattern (e.g., `#ifndef PC_POWER_STATE_H`)

### File Organization

- One module per `.c`/`.h` pair
- Public headers in `include/`, private headers (like `tusb_config.h`) in `src/`
- Section dividers use `/* ── Section Name ──── */` comment style
- Doxygen-style `/** */` comments on public API functions

### Architecture Patterns

- **HAL pattern:** Hardware-dependent code isolated behind abstract interfaces (`pc_power_hal.h`) enabling host-native testing
- **Callback pattern:** Modules accept callback function pointers for event notification (e.g., `usb_hid_state_cb_t`, `bt_gamepad_event_cb_t`)
- **State machine:** Event-driven with explicit state/event/action types; returns result structs rather than modifying global state directly
- **Critical sections:** Used for cross-context data sharing (BT task ↔ main loop)

## Testing

### Test Structure

Tests use the [Unity](https://github.com/ThrowTheSwitch/Unity) framework (vendored):

```
firmware/test/
├── unity/                          # Unity test framework (vendored)
│   ├── unity.c
│   ├── unity.h
│   └── unity_internals.h
├── test_pc_power_state/
│   └── test_pc_power_state.c       # Power state machine tests
└── test_gamepad/
    └── test_gamepad.c              # Gamepad report/HID conversion tests
```

### Test Conventions

- Each test file has `setUp()` / `tearDown()` functions
- Test function names: `test_<module>_<scenario>` (e.g., `test_off_wake_requested_transitions_to_booting`)
- Helper functions for reaching specific states (e.g., `enter_booting()`, `enter_on()`, `enter_sleeping()`)
- Tests cover: state transitions, action outputs, edge cases, invalid inputs, full lifecycle sequences
- Tests compile and run natively on the host (no hardware mocking framework -- the state machine is pure logic)

### Adding a New Test

1. Create `firmware/test/test_<name>/test_<name>.c`
2. Add the test binary to `firmware/Makefile` following existing patterns in `TEST_BINS`
3. Run `make test` from `firmware/`

## Hardware Context

- **MCU:** Raspberry Pi Pico 2 W (RP2350 + CYW43439 wireless)
- **BT Library:** Bluepad32 (supports Xbox, PlayStation, Switch Pro, 8BitDo, generic HID gamepads)
- **USB Stack:** TinyUSB (bundled with Pico SDK), device mode only (HID gamepad)
- **USB IDs:** VID 0x1209 (pid.codes shared), PID 0x0001 (placeholder)
- **GPIO:** PWR_BTN_SENSE (GPIO 2), PWR_BTN_TRIGGER (GPIO 3), PWR_LED_SENSE (GPIO 4)
- **UART:** printf output goes to UART (USB is occupied by HID device)

## Key Dependencies

| Dependency | Source | Purpose |
|------------|--------|---------|
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.0+ | External / FetchContent | RP2350 HAL, stdlib, USB |
| [Bluepad32](https://gitlab.com/ricardoquesada/bluepad32) | Clone to `firmware/lib/bluepad32` | Bluetooth gamepad support |
| [TinyUSB](https://github.com/hathach/tinyusb) | Bundled with Pico SDK | USB HID device stack |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | Vendored in `firmware/test/unity/` | Unit test framework |
