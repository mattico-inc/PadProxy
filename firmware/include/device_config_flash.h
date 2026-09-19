#ifndef DEVICE_CONFIG_FLASH_H
#define DEVICE_CONFIG_FLASH_H

#include "device_config.h"

/**
 * Device Config Flash Storage (RP2350 HAL)
 *
 * Persists device_config_t to a reserved flash sector. This module is
 * the hardware-dependent counterpart to the pure-logic serialize/
 * deserialize functions in device_config.h.
 */

/**
 * Load device config from flash.
 *
 * On success, cfg is populated and true is returned. On failure (blank
 * flash, corrupt data) cfg is left unchanged and false is returned --
 * the caller should fall back to device_config_init() defaults.
 */
bool device_config_flash_load(device_config_t *cfg);

/**
 * Erase and rewrite the config flash sector with cfg.
 * Returns true on success, false if cfg fails to serialize.
 */
bool device_config_flash_save(const device_config_t *cfg);

#endif /* DEVICE_CONFIG_FLASH_H */
