#include "device_config_flash.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"

/*
 * partition_table.json allocates the A and B firmware partitions as
 * 1984 KB each (3968 KB total) out of the RP2350's 4 MB flash, leaving
 * the last 128 KB unpartitioned. Config lives in the very last sector
 * of flash, which is outside both A and B, so OTA updates (which only
 * ever write to the inactive partition) can never clobber it.
 */
#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

bool device_config_flash_load(device_config_t *cfg)
{
    const uint8_t *flash_data =
        (const uint8_t *)(XIP_BASE + CONFIG_FLASH_OFFSET);

    return device_config_deserialize(cfg, flash_data, FLASH_SECTOR_SIZE);
}

bool device_config_flash_save(const device_config_t *cfg)
{
    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0, sizeof(page));

    if (device_config_serialize(cfg, page, sizeof(page)) < 0)
        return false;

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CONFIG_FLASH_OFFSET, page, sizeof(page));
    restore_interrupts(ints);

    return true;
}
