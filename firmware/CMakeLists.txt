# PadProxy firmware — Raspberry Pi Pico 2 W (RP2350 + CYW43439)
#
# Build:
#   mkdir build && cd build
#   cmake -DPICO_BOARD=pico2_w ..
#   make -j$(nproc)
#
# The resulting padproxy.uf2 can be dragged onto the Pico 2 W in BOOTSEL mode.
#
# Prerequisites:
#   - Pico SDK 2.0+: set PICO_SDK_PATH or PICO_SDK_FETCH_FROM_GIT=ON
#   - Bluepad32:     git clone https://gitlab.com/ricardoquesada/bluepad32.git lib/bluepad32

cmake_minimum_required(VERSION 3.13)

# Default to Pico 2 W (RP2350 with wireless)
set(PICO_BOARD pico2_w CACHE STRING "Target board")

include(pico_sdk_import.cmake)

project(padproxy C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# ── Bluepad32 ────────────────────────────────────────────────────────────
# Clone into lib/bluepad32, or pass -DBLUEPAD32_ROOT=/path/to/bluepad32
set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/lib/bluepad32
    CACHE PATH "Path to Bluepad32 repository root")

add_subdirectory(${BLUEPAD32_ROOT}/src/components/bluepad32 bluepad32)

# ── Firmware executable ──────────────────────────────────────────────────
add_executable(padproxy
    src/main.c
    src/bt_gamepad.c
    src/usb_hid_gamepad.c
    src/usb_hid_report.c
    src/pc_power_state.c
)

target_include_directories(padproxy PRIVATE include src)

target_link_libraries(padproxy
    pico_stdlib
    pico_sync
    tinyusb_device
    pico_btstack_cyw43
    pico_cyw43_arch_threadsafe_background
    bluepad32
)

# Send printf to UART (USB is occupied by the HID gamepad device)
pico_enable_stdio_usb(padproxy 0)
pico_enable_stdio_uart(padproxy 1)

# Generate .uf2, .bin, .hex alongside the ELF
pico_add_extra_outputs(padproxy)
