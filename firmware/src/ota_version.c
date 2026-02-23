#include "ota_version.h"

#include <stdio.h>

/* ── Compile-time version ────────────────────────────────────────────── */

#ifndef PADPROXY_VERSION_MAJOR
#define PADPROXY_VERSION_MAJOR 0
#endif
#ifndef PADPROXY_VERSION_MINOR
#define PADPROXY_VERSION_MINOR 0
#endif
#ifndef PADPROXY_VERSION_PATCH
#define PADPROXY_VERSION_PATCH 0
#endif

const ota_version_t OTA_CURRENT_VERSION = {
    .major = PADPROXY_VERSION_MAJOR,
    .minor = PADPROXY_VERSION_MINOR,
    .patch = PADPROXY_VERSION_PATCH,
};

/* ── Parsing ─────────────────────────────────────────────────────────── */

/**
 * Parse an unsigned decimal integer, advancing *p past consumed digits.
 */
static bool parse_u16(const char **p, uint16_t *out)
{
    if (**p < '0' || **p > '9') return false;

    uint32_t val = 0;
    while (**p >= '0' && **p <= '9') {
        val = val * 10 + (uint32_t)(**p - '0');
        if (val > UINT16_MAX) return false;
        (*p)++;
    }
    *out = (uint16_t)val;
    return true;
}

bool ota_version_parse(const char *str, ota_version_t *out)
{
    if (!str || !out) return false;

    const char *p = str;

    /* Skip optional 'v'/'V' prefix */
    if (*p == 'v' || *p == 'V') p++;

    ota_version_t v = {0};

    if (!parse_u16(&p, &v.major)) return false;
    if (*p != '.') return false;
    p++;

    if (!parse_u16(&p, &v.minor)) return false;
    if (*p != '.') return false;
    p++;

    if (!parse_u16(&p, &v.patch)) return false;

    *out = v;
    return true;
}

/* ── Comparison ──────────────────────────────────────────────────────── */

int ota_version_compare(const ota_version_t *a, const ota_version_t *b)
{
    if (a->major != b->major) return (a->major > b->major) ? 1 : -1;
    if (a->minor != b->minor) return (a->minor > b->minor) ? 1 : -1;
    if (a->patch != b->patch) return (a->patch > b->patch) ? 1 : -1;
    return 0;
}

/* ── Formatting ──────────────────────────────────────────────────────── */

int ota_version_format(const ota_version_t *v, char *buf, int len)
{
    if (!v || !buf || len < 1) return -1;
    return snprintf(buf, (size_t)len, "%u.%u.%u", v->major, v->minor, v->patch);
}
