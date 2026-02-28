#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * OTA Firmware Update via GitHub Releases
 *
 * Uses the RP2350 boot ROM's "Try Before You Buy" (TBYB) mechanism
 * with an A/B partition table for safe, rollback-capable updates.
 *
 * Update flow:
 *   1. Connect to WiFi
 *   2. Query GitHub Releases API for the latest version tag
 *   3. Compare against the running firmware version
 *   4. If newer: download the .bin asset to the inactive partition
 *   5. Issue a FLASH_UPDATE reboot into the new partition
 *   6. On next boot, rom_explicit_buy() accepts the new image
 *   7. If the new image crashes, boot ROM rolls back automatically
 *
 * WiFi credentials are provided by the caller (typically from the
 * device config, with compile-time defaults as fallback).
 * The GitHub repository is configured at compile time via cmake defines:
 *   -DGITHUB_OTA_OWNER="owner" -DGITHUB_OTA_REPO="repo"
 *
 * Requires a partition table with linked A/B partitions to be flashed
 * to the device (see partition_table.json + picotool).
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
 * Accept the current firmware image (TBYB buy).
 *
 * Must be called early in main() on every boot.  If this boot was a
 * TBYB flash-update boot, this makes the current partition permanent.
 * If not a TBYB boot, the call is a harmless no-op.
 *
 * Without this call a TBYB image is rolled back by the boot ROM
 * after ~16.7 seconds.
 */
void ota_accept_current_image(void);

/**
 * WiFi credentials for OTA update checks.
 *
 * The caller provides credentials — typically from the device config
 * (runtime/flash) with compile-time defaults as fallback.
 */
typedef struct {
    const char *ssid;
    const char *password;
} ota_wifi_creds_t;

/**
 * Check for a firmware update on GitHub and apply it if available.
 *
 * This is a blocking call that may take 10-60 seconds depending on
 * network conditions and firmware size. Intended for use during
 * startup before entering the main loop.
 *
 * If an update is applied, this function issues a FLASH_UPDATE reboot
 * and does not return.
 *
 * @param creds  WiFi credentials. Pass NULL to skip the update check.
 * @return Result code indicating outcome.
 */
ota_update_result_t ota_update_check_and_apply(const ota_wifi_creds_t *creds);

/**
 * Get a human-readable name for a result code.
 */
const char *ota_update_result_name(ota_update_result_t result);

#endif /* OTA_UPDATE_H */
