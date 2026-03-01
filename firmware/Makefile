# PadProxy Firmware Build — Zero System Dependencies!
#
# Just run `make firmware` and everything downloads automatically:
#   ✓ CMake (portable binary)
#   ✓ ARM GCC toolchain (portable binary)
#   ✓ Pico SDK (git clone)
#   ✓ Bluepad32 (git clone)
#
# Targets:
#   make firmware  - Build firmware (auto-downloads everything)
#   make test      - Build and run host-native unit tests
#   make flash     - Flash UF2 to Pico 2 W in BOOTSEL mode
#   make tools     - Download CMake and ARM toolchain only
#   make deps      - Clone Pico SDK and Bluepad32 only
#   make clean     - Remove build artifacts (keeps lib/ and tools/)
#   make distclean - Remove everything (build/, lib/, tools/)
#
# Requirements: git, make, curl/wget (that's it!)
#
# Variables:
#   CMAKE_VERSION     - CMake version (default: 3.28.3)
#   ARM_TOOLCHAIN_VER - ARM GCC version (default: 13.2.rel1)
#   PICO_SDK_PATH     - Pico SDK path (default: lib/pico-sdk)
#   PICO_MOUNT        - Pico mount point (auto-detected)

# ── Host tooling (for tests) ─────────────────────────────────────────────

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g -O0
CFLAGS += -Iinclude -Itest/unity

# ── Directories ──────────────────────────────────────────────────────────

TEST_BUILD_DIR = build/test
FW_BUILD_DIR   = build/firmware
UNITY_SRC      = test/unity/unity.c

# ── Dependencies ─────────────────────────────────────────────────────────

PICO_SDK_TAG      ?= 2.1.0
BLUEPAD32_TAG     ?= 4.2.0
CMAKE_VERSION     ?= 3.28.3
ARM_TOOLCHAIN_VER ?= 13.2.rel1

BLUEPAD32_DIR    = lib/bluepad32
PICO_SDK_DIR     = lib/pico-sdk
TOOLS_DIR        = tools

# Use PICO_SDK_PATH if set by the user, otherwise clone to lib/pico-sdk
PICO_SDK_PATH  ?= $(PICO_SDK_DIR)

# ── Detect OS and architecture ──────────────────────────────────────────

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# ── Portable CMake (auto-downloaded, no system install needed) ──────────

ifeq ($(UNAME_S),Darwin)
    CMAKE_OS = macos-universal
    CMAKE_EXT = tar.gz
else ifeq ($(UNAME_S),Linux)
    ifeq ($(UNAME_M),x86_64)
        CMAKE_OS = linux-x86_64
    else ifeq ($(UNAME_M),aarch64)
        CMAKE_OS = linux-aarch64
    else
        $(error Unsupported Linux architecture: $(UNAME_M))
    endif
    CMAKE_EXT = tar.gz
else
    $(error Unsupported OS: $(UNAME_S))
endif

CMAKE_ARCHIVE = cmake-$(CMAKE_VERSION)-$(CMAKE_OS).$(CMAKE_EXT)
CMAKE_URL = https://github.com/Kitware/CMake/releases/download/v$(CMAKE_VERSION)/$(CMAKE_ARCHIVE)
CMAKE_DIR = $(TOOLS_DIR)/cmake-$(CMAKE_VERSION)-$(CMAKE_OS)
CMAKE = $(CMAKE_DIR)/CMake.app/Contents/bin/cmake

# On Linux, cmake is directly in bin/
ifeq ($(UNAME_S),Linux)
    CMAKE = $(CMAKE_DIR)/bin/cmake
endif

# ── Portable ARM Toolchain (auto-downloaded, no system install needed) ──

ifeq ($(UNAME_S),Darwin)
    ifeq ($(UNAME_M),arm64)
        ARM_HOST = darwin-arm64
    else ifeq ($(UNAME_M),x86_64)
        ARM_HOST = darwin-x86_64
    else
        $(error Unsupported macOS architecture: $(UNAME_M))
    endif
    ARM_EXT = tar.xz
else ifeq ($(UNAME_S),Linux)
    ifeq ($(UNAME_M),x86_64)
        ARM_HOST = x86_64-linux
    else ifeq ($(UNAME_M),aarch64)
        ARM_HOST = aarch64-linux
    else
        $(error Unsupported Linux architecture: $(UNAME_M))
    endif
    ARM_EXT = tar.xz
else
    $(error Unsupported OS: $(UNAME_S))
endif

ARM_TOOLCHAIN_NAME = arm-gnu-toolchain-$(ARM_TOOLCHAIN_VER)-$(ARM_HOST)-arm-none-eabi
ARM_ARCHIVE = $(ARM_TOOLCHAIN_NAME).$(ARM_EXT)
ARM_URL = https://developer.arm.com/-/media/Files/downloads/gnu/$(ARM_TOOLCHAIN_VER)/binrel/$(ARM_ARCHIVE)
ARM_TOOLCHAIN_DIR = $(TOOLS_DIR)/$(ARM_TOOLCHAIN_NAME)
ARM_TOOLCHAIN_BIN = $(ARM_TOOLCHAIN_DIR)/bin

# ARM toolchain binaries
ARM_CC      = $(ARM_TOOLCHAIN_BIN)/arm-none-eabi-gcc
ARM_CXX     = $(ARM_TOOLCHAIN_BIN)/arm-none-eabi-g++
ARM_OBJCOPY = $(ARM_TOOLCHAIN_BIN)/arm-none-eabi-objcopy
ARM_SIZE    = $(ARM_TOOLCHAIN_BIN)/arm-none-eabi-size
ARM_GDB     = $(ARM_TOOLCHAIN_BIN)/arm-none-eabi-gdb

# ── Test binaries ────────────────────────────────────────────────────────

TEST_BINS = $(TEST_BUILD_DIR)/test_pc_power_state $(TEST_BUILD_DIR)/test_gamepad $(TEST_BUILD_DIR)/test_ota_version $(TEST_BUILD_DIR)/test_device_config $(TEST_BUILD_DIR)/test_setup_cmd $(TEST_BUILD_DIR)/test_device_integration

# ── Firmware cmake arguments ─────────────────────────────────────────────

CMAKE_ARGS  = -DPICO_BOARD=pico2_w
CMAKE_ARGS += -DPICO_SDK_PATH=$(abspath $(PICO_SDK_PATH))

# Auto-detect Pico mount point for flashing
PICO_MOUNT ?= $(firstword $(wildcard /media/$(USER)/RP2350 /media/$(USER)/RPI-RP2 /run/media/$(USER)/RP2350 /run/media/$(USER)/RPI-RP2 /Volumes/RP2350 /Volumes/RPI-RP2))

# ── Phony targets ────────────────────────────────────────────────────────

.PHONY: all firmware test flash deps tools clean distclean

all: firmware

tools: $(CMAKE) $(ARM_CC)
	@echo "All tools installed:"
	@echo "  CMake: $(CMAKE)"
	@echo "  ARM GCC: $(ARM_CC)"

# ── Tool installation ────────────────────────────────────────────────────

# Download and extract portable CMake
$(CMAKE):
	@echo "Downloading CMake $(CMAKE_VERSION) for $(CMAKE_OS)..."
	@mkdir -p $(TOOLS_DIR)
	@if command -v curl >/dev/null 2>&1; then \
		curl -L -o $(TOOLS_DIR)/$(CMAKE_ARCHIVE) $(CMAKE_URL); \
	elif command -v wget >/dev/null 2>&1; then \
		wget -O $(TOOLS_DIR)/$(CMAKE_ARCHIVE) $(CMAKE_URL); \
	else \
		echo "Error: Neither curl nor wget found. Please install one."; \
		exit 1; \
	fi
	@echo "Extracting CMake..."
	@cd $(TOOLS_DIR) && tar xzf $(CMAKE_ARCHIVE)
	@rm $(TOOLS_DIR)/$(CMAKE_ARCHIVE)
	@echo "CMake installed at $(CMAKE)"

# Download and extract portable ARM toolchain
$(ARM_CC):
	@echo "Downloading ARM toolchain $(ARM_TOOLCHAIN_VER) for $(ARM_HOST)..."
	@mkdir -p $(TOOLS_DIR)
	@if command -v curl >/dev/null 2>&1; then \
		curl -L -o $(TOOLS_DIR)/$(ARM_ARCHIVE) $(ARM_URL); \
	elif command -v wget >/dev/null 2>&1; then \
		wget -O $(TOOLS_DIR)/$(ARM_ARCHIVE) $(ARM_URL); \
	else \
		echo "Error: Neither curl nor wget found. Please install one."; \
		exit 1; \
	fi
	@echo "Extracting ARM toolchain (this may take a minute)..."
	@cd $(TOOLS_DIR) && tar xJf $(ARM_ARCHIVE)
	@rm $(TOOLS_DIR)/$(ARM_ARCHIVE)
	@echo "ARM toolchain installed at $(ARM_TOOLCHAIN_BIN)"

# ── Dependencies ─────────────────────────────────────────────────────────

deps: $(BLUEPAD32_DIR) $(PICO_SDK_DIR)

$(BLUEPAD32_DIR):
	git clone --branch $(BLUEPAD32_TAG) --depth 1 https://gitlab.com/ricardoquesada/bluepad32.git $(BLUEPAD32_DIR)

$(PICO_SDK_DIR):
	git clone --branch $(PICO_SDK_TAG) --depth 1 https://github.com/raspberrypi/pico-sdk.git $(PICO_SDK_DIR)
	cd $(PICO_SDK_DIR) && git submodule update --init --recursive

# ── Firmware (cross-compile for RP2350) ──────────────────────────────────

firmware: $(BLUEPAD32_DIR) $(PICO_SDK_PATH) $(CMAKE) $(ARM_CC) | $(FW_BUILD_DIR)
	PATH="$(abspath $(ARM_TOOLCHAIN_BIN)):$$PATH" $(abspath $(CMAKE)) -B $(FW_BUILD_DIR) -S . $(CMAKE_ARGS)
	PATH="$(abspath $(ARM_TOOLCHAIN_BIN)):$$PATH" $(MAKE) -C $(FW_BUILD_DIR) -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "=== Firmware built: $(FW_BUILD_DIR)/padproxy.uf2 ==="

$(FW_BUILD_DIR):
	mkdir -p $(FW_BUILD_DIR)

# ── Flash ────────────────────────────────────────────────────────────────

flash: firmware
	@if [ -z "$(PICO_MOUNT)" ]; then \
		echo "Error: Pico 2 W not found in BOOTSEL mode."; \
		echo "Hold BOOTSEL, plug in USB, then retry."; \
		echo "Or set PICO_MOUNT=/path/to/mount"; \
		exit 1; \
	fi
	cp $(FW_BUILD_DIR)/padproxy.uf2 "$(PICO_MOUNT)/"
	@echo "=== Flashed padproxy.uf2 to $(PICO_MOUNT) ==="

# ── Unit tests (host-native) ────────────────────────────────────────────

test: $(TEST_BINS)
	@echo "=== Running all tests ==="
	@failed=0; \
	for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		./$$t || failed=1; \
	done; \
	if [ $$failed -eq 1 ]; then echo "SOME TESTS FAILED"; exit 1; fi
	@echo "=== All tests passed ==="

$(TEST_BUILD_DIR)/test_pc_power_state: test/test_pc_power_state/test_pc_power_state.c src/pc_power_state.c $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BUILD_DIR)/test_gamepad: test/test_gamepad/test_gamepad.c src/usb_hid_report.c $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BUILD_DIR)/test_ota_version: test/test_ota_version/test_ota_version.c src/ota_version.c $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BUILD_DIR)/test_device_config: test/test_device_config/test_device_config.c src/device_config.c $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BUILD_DIR)/test_setup_cmd: test/test_setup_cmd/test_setup_cmd.c src/setup_cmd.c src/device_config.c $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BUILD_DIR)/test_device_integration: test/test_device_integration/test_device_integration.c src/pc_power_state.c src/usb_hid_report.c $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

# ── Clean ────────────────────────────────────────────────────────────────

clean:
	rm -rf build/

distclean: clean
	rm -rf lib/ tools/
