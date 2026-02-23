#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * OTA Firmware Update via GitHub Releases
 *
 * Checks for new firmware releases on GitHub and applies updates
 * over WiFi. The update process:
 *
 *   1. Connect to WiFi
 *   2. Query GitHub Releases API for the latest version tag
 *   3. Compare against the running firmware version
 *   4. If newer: download the .bin asset to a staging area in flash
 *   5. Copy from staging to the active application area
 *   6. Reboot into the new firmware
 *
 * WiFi credentials and the GitHub repository are configured at compile
 * time via cmake defines:
 *   -DWIFI_SSID="MyNetwork" -DWIFI_PASSWORD="secret"
 *   -DGITHUB_OTA_OWNER="owner" -DGITHUB_OTA_REPO="repo"
 */

/** Result of an OTA update check. */
typedef enum {
    OTA_RESULT_NO_UPDATE,
    OTA_RESULT_UPDATE_APPLIED,
    OTA_RESULT_ERROR_NO_WIFI_CONFIG,
    OTA_RESULT_ERROR_WIFI,
    OTA_RESULT_ERROR_HTTP,
    OTA_RESULT_ERROR_VERSION,
    OTA_RESULT_ERROR_FLASH,
} ota_update_result_t;

/**
 * Check for a firmware update on GitHub and apply it if available.
 *
 * This is a blocking call that may take 10-60 seconds depending on
 * network conditions and firmware size. Intended for use during
 * startup before entering the main loop.
 *
 * If an update is applied, this function reboots the device and does
 * not return.
 *
 * @return Result code indicating outcome.
 */
ota_update_result_t ota_update_check_and_apply(void);

/**
 * Get a human-readable name for a result code.
 */
const char *ota_update_result_name(ota_update_result_t result);

#endif /* OTA_UPDATE_H */
