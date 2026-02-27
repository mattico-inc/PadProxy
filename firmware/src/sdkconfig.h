/**
 * sdkconfig.h — Bluepad32 compile-time configuration for PadProxy
 *
 * Emulates the ESP-IDF "menuconfig" system that Bluepad32 expects.
 * On Pico W builds these defines are provided directly via this header.
 *
 * Adapted from the Bluepad32 Pico W example:
 *   https://gitlab.com/ricardoquesada/bluepad32/-/blob/main/examples/pico_w/src/sdkconfig.h
 */

#ifndef SDKCONFIG_H
#define SDKCONFIG_H

#define CONFIG_BLUEPAD32_MAX_DEVICES 4
#define CONFIG_BLUEPAD32_MAX_ALLOWLIST 4
#define CONFIG_BLUEPAD32_GAP_SECURITY 1
#define CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT 1

#define CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#define CONFIG_TARGET_PICO_W

/* 2 == Info */
#define CONFIG_BLUEPAD32_LOG_LEVEL 2

#endif /* SDKCONFIG_H */
