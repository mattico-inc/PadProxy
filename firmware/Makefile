# Makefile for native (host) unit tests
# For ESP32 firmware builds, use PlatformIO: pio run -e esp32s3

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O0
CFLAGS += -Iinclude -Itest/unity

BUILD_DIR = build/test
UNITY_SRC = test/unity/unity.c

TEST_BINS = $(BUILD_DIR)/test_pc_power_state

.PHONY: test clean

test: $(TEST_BINS)
	@echo "=== Running all tests ==="
	@failed=0; \
	for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		./$$t || failed=1; \
	done; \
	if [ $$failed -eq 1 ]; then echo "SOME TESTS FAILED"; exit 1; fi
	@echo "=== All tests passed ==="

$(BUILD_DIR)/test_pc_power_state: test/test_pc_power_state/test_pc_power_state.c src/pc_power_state.c $(UNITY_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf build/
