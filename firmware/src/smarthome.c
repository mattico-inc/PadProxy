#include "smarthome.h"

/**
 * Smart home platform dispatch.
 *
 * Returns the compile-time selected platform, or NULL for HTTP-only mode.
 */

#if defined(SMARTHOME_HASS)
extern const smarthome_platform_t smarthome_hass;
#endif

const smarthome_platform_t *smarthome_get_platform(void)
{
#if defined(SMARTHOME_HASS)
    return &smarthome_hass;
#else
    return (void *)0;
#endif
}
