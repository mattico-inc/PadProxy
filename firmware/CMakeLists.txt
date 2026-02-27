# PadProxy firmware — Raspberry Pi Pico 2 W (RP2350 + CYW43439)
#
# Quick build (from firmware/):
#   make firmware          # auto-fetches Pico SDK & Bluepad32
#   make flash             # copy UF2 to Pico in BOOTSEL mode
#
# Manual cmake build (set PICO_SDK_PATH first):
#   mkdir build && cd build
#   cmake -DPICO_BOARD=pico2_w -DPICO_SDK_PATH=/path/to/pico-sdk ..
#   make -j$(nproc)
#
# The resulting padproxy.uf2 can be dragged onto the Pico 2 W in BOOTSEL mode.
#
# Prerequisites:
#   - ARM toolchain:  arm-none-eabi-gcc (apt: gcc-arm-none-eabi)
#   - Pico SDK 2.0+:  set PICO_SDK_PATH, or `make deps` to clone to lib/
#   - Bluepad32:      auto-cloned by `make deps`/`make firmware`, or:
#                      git clone https://gitlab.com/ricardoquesada/bluepad32.git lib/bluepad32

cmake_minimum_required(VERSION 3.13)

# Default to Pico 2 W (RP2350 with wireless)
set(PICO_BOARD pico2_w CACHE STRING "Target board")

include(pico_sdk_import.cmake)

project(padproxy C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# ── BTstack ────────────────────────────────────────────────────────────
# Use BTstack bundled with Pico SDK.  btstack_config.h and sdkconfig.h
# live in src/ — use include_directories() (global) so the bluepad32
# library target can find them too.
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/src)

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
    src/pc_power_hal.c
    src/ota_version.c
    src/ota_update.c
)

target_include_directories(padproxy PRIVATE include src)

# ── OTA configuration (override with cmake -D flags) ────────────────────
# WiFi credentials for OTA update checks
set(WIFI_SSID "" CACHE STRING "WiFi SSID for OTA updates")
set(WIFI_PASSWORD "" CACHE STRING "WiFi password for OTA updates")
# GitHub repository for firmware releases
set(GITHUB_OTA_OWNER "mattico-inc" CACHE STRING "GitHub owner for OTA releases")
set(GITHUB_OTA_REPO "PadProxy" CACHE STRING "GitHub repo for OTA releases")
# Firmware version (set automatically from git tags in CI)
set(PADPROXY_VERSION_MAJOR 0 CACHE STRING "Firmware major version")
set(PADPROXY_VERSION_MINOR 0 CACHE STRING "Firmware minor version")
set(PADPROXY_VERSION_PATCH 0 CACHE STRING "Firmware patch version")

target_compile_definitions(padproxy PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASSWORD="${WIFI_PASSWORD}"
    GITHUB_OTA_OWNER="${GITHUB_OTA_OWNER}"
    GITHUB_OTA_REPO="${GITHUB_OTA_REPO}"
    PADPROXY_VERSION_MAJOR=${PADPROXY_VERSION_MAJOR}
    PADPROXY_VERSION_MINOR=${PADPROXY_VERSION_MINOR}
    PADPROXY_VERSION_PATCH=${PADPROXY_VERSION_PATCH}
    # Mark this image as "Try Before You Buy" — the boot ROM will roll
    # back to the previous partition unless rom_explicit_buy() is called.
    PICO_CRT0_IMAGE_TYPE_TBYB=1
)

target_link_libraries(padproxy
    pico_stdlib
    pico_sync
    tinyusb_device
    pico_btstack_classic
    pico_btstack_cyw43
    pico_cyw43_arch_threadsafe_background
    pico_lwip_mbedtls
    pico_lwip_http
    pico_bootrom
    bluepad32
)

# Send printf to UART (USB is occupied by the HID gamepad device)
pico_enable_stdio_usb(padproxy 0)
pico_enable_stdio_uart(padproxy 1)

# Generate .uf2, .bin, .hex alongside the ELF
pico_add_extra_outputs(padproxy)
