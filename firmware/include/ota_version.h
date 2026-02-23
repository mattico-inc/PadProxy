#ifndef OTA_VERSION_H
#define OTA_VERSION_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Semantic version (major.minor.patch).
 *
 * Used to compare the running firmware version against the latest
 * GitHub release tag to decide whether an OTA update is available.
 */
typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} ota_version_t;

/**
 * The firmware version baked into this build.
 *
 * Set at compile time via -DPADPROXY_VERSION_MAJOR/MINOR/PATCH.
 * Defaults to 0.0.0 (development build, always eligible for update).
 */
extern const ota_version_t OTA_CURRENT_VERSION;

/**
 * Parse a version string "vMAJOR.MINOR.PATCH" or "MAJOR.MINOR.PATCH".
 *
 * Leading 'v'/'V' is optional and stripped. Trailing characters after
 * the patch number (e.g., "-rc1") are ignored.
 *
 * @param str  Null-terminated version string.
 * @param out  Parsed version on success.
 * @return     true if parsing succeeded.
 */
bool ota_version_parse(const char *str, ota_version_t *out);

/**
 * Compare two versions.
 *
 * @return  <0 if a < b, 0 if equal, >0 if a > b.
 */
int ota_version_compare(const ota_version_t *a, const ota_version_t *b);

/**
 * Format a version as "MAJOR.MINOR.PATCH" (no 'v' prefix).
 *
 * @param v    Version to format.
 * @param buf  Output buffer (at least 16 bytes).
 * @param len  Buffer size.
 * @return     Characters written (excluding NUL), or -1 on error.
 */
int ota_version_format(const ota_version_t *v, char *buf, int len);

#endif /* OTA_VERSION_H */
