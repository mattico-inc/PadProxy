# PadProxy firmware build
#
# Targets:
#   make firmware  - Cross-compile for RP2350 (output: build/firmware/padproxy.uf2)
#   make test      - Build and run host-native unit tests
#   make flash     - Copy UF2 to Pico 2 W in BOOTSEL mode
#   make deps      - Clone Pico SDK and Bluepad32 into lib/
#   make clean     - Remove build artifacts (keeps lib/)
#   make distclean - Remove build artifacts and cloned dependencies
#
# Variables:
#   PICO_SDK_PATH  - Path to Pico SDK (default: lib/pico-sdk)
#   PICO_MOUNT     - Mount point of Pico in BOOTSEL mode (auto-detected)

# ── Tooling ──────────────────────────────────────────────────────────────

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g -O0
CFLAGS += -Iinclude -Itest/unity

# ── Directories ──────────────────────────────────────────────────────────

TEST_BUILD_DIR = build/test
FW_BUILD_DIR   = build/firmware
UNITY_SRC      = test/unity/unity.c

# ── Dependencies ─────────────────────────────────────────────────────────

PICO_SDK_TAG    ?= 2.1.0
BLUEPAD32_TAG   ?= 4.2.0
BLUEPAD32_DIR    = lib/bluepad32
PICO_SDK_DIR     = lib/pico-sdk

# Use PICO_SDK_PATH if set by the user, otherwise clone to lib/pico-sdk
PICO_SDK_PATH  ?= $(PICO_SDK_DIR)

# ── Test binaries ────────────────────────────────────────────────────────

TEST_BINS = $(TEST_BUILD_DIR)/test_pc_power_state $(TEST_BUILD_DIR)/test_gamepad $(TEST_BUILD_DIR)/test_ota_version $(TEST_BUILD_DIR)/test_device_integration

# ── Firmware cmake arguments ─────────────────────────────────────────────

CMAKE_ARGS  = -DPICO_BOARD=pico2_w
CMAKE_ARGS += -DPICO_SDK_PATH=$(abspath $(PICO_SDK_PATH))

# Auto-detect Pico mount point for flashing
PICO_MOUNT ?= $(firstword $(wildcard /media/$(USER)/RP2350 /media/$(USER)/RPI-RP2 /run/media/$(USER)/RP2350 /run/media/$(USER)/RPI-RP2 /Volumes/RP2350 /Volumes/RPI-RP2))

# ── Phony targets ────────────────────────────────────────────────────────

.PHONY: all firmware test flash deps clean distclean

all: firmware

# ── Dependencies ─────────────────────────────────────────────────────────

deps: $(BLUEPAD32_DIR) $(PICO_SDK_DIR)

$(BLUEPAD32_DIR):
	git clone --branch $(BLUEPAD32_TAG) --depth 1 https://gitlab.com/ricardoquesada/bluepad32.git $(BLUEPAD32_DIR)

$(PICO_SDK_DIR):
	git clone --branch $(PICO_SDK_TAG) --depth 1 https://github.com/raspberrypi/pico-sdk.git $(PICO_SDK_DIR)
	cd $(PICO_SDK_DIR) && git submodule update --init --recursive

# ── Firmware (cross-compile for RP2350) ──────────────────────────────────

firmware: $(BLUEPAD32_DIR) $(PICO_SDK_PATH) | $(FW_BUILD_DIR)
	cd $(FW_BUILD_DIR) && cmake $(CMAKE_ARGS) ../..
	$(MAKE) -C $(FW_BUILD_DIR) -j$(shell nproc)
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

$(TEST_BUILD_DIR)/test_device_integration: test/test_device_integration/test_device_integration.c src/pc_power_state.c src/usb_hid_report.c $(UNITY_SRC) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

# ── Clean ────────────────────────────────────────────────────────────────

clean:
	rm -rf build/

distclean: clean
	rm -rf lib/
