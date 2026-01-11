#include <flash_map/flash_map.h>
#include <flash_map_backend/flash_map_backend.h>
#include "boot_port.h"
#include "main.h"

static const struct flash_area flash_areas[] = {
    { FLASH_AREA_BOOTLOADER, FLASH_DEVICE_ID, 0, BOOTLOADER_OFFSET, BOOTLOADER_SIZE },
    { FLASH_AREA_IMAGE_0,    FLASH_DEVICE_ID, 0, BOOT_PRIMARY_SLOT_OFFSET, BOOT_PRIMARY_SLOT_SIZE },
    { FLASH_AREA_IMAGE_1,    FLASH_DEVICE_ID, 0, BOOT_SECONDARY_SLOT_OFFSET, BOOT_SECONDARY_SLOT_SIZE },
    { FLASH_AREA_IMAGE_SCRATCH, FLASH_DEVICE_ID, 0, BOOT_SCRATCH_OFFSET, BOOT_SCRATCH_SIZE },
    { FLASH_AREA_PARAM,      FLASH_DEVICE_ID, 0, BOOT_PARAM_OFFSET, BOOT_PARAM_SIZE },
};

static uint8_t flash_area_refcnt[sizeof(flash_areas) / sizeof(flash_areas[0])];

static const struct flash_area *flash_area_find(uint8_t id)
{
    for (size_t i = 0; i < sizeof(flash_areas) / sizeof(flash_areas[0]); i++) {
        if (flash_areas[i].fa_id == id) {
            return &flash_areas[i];
        }
    }
    return 0;
}

int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    if (fd_id != FLASH_DEVICE_ID || ret == 0) {
        return -1;
    }
    *ret = BOOT_FLASH_BASE;
    return 0;
}

int flash_area_open(uint8_t id, const struct flash_area **fa)
{
    const struct flash_area *area = flash_area_find(id);
    if (!area || !fa) {
        return -1;
    }
    *fa = area;
    for (size_t i = 0; i < sizeof(flash_areas) / sizeof(flash_areas[0]); i++) {
        if (flash_areas[i].fa_id == id) {
            flash_area_refcnt[i]++;
            break;
        }
    }
    return 0;
}

void flash_area_close(const struct flash_area *fa)
{
    if (!fa) {
        return;
    }
    for (size_t i = 0; i < sizeof(flash_areas) / sizeof(flash_areas[0]); i++) {
        if (flash_areas[i].fa_id == fa->fa_id) {
            if (flash_area_refcnt[i] > 0) {
                flash_area_refcnt[i]--;
            }
            break;
        }
    }
}

int flash_area_read(const struct flash_area *fa, uint32_t off, void *dst, uint32_t len)
{
    if (!fa || !dst || (off + len) > fa->fa_size) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (onchip_flash_read(fa->fa_off + off, (uint8_t *)dst, len) < 0) {
        return -1;
    }
    return 0;
}

int flash_area_write(const struct flash_area *fa, uint32_t off, const void *src, uint32_t len)
{
    uint32_t addr;
    const uint8_t *p;
    uint32_t remaining;

    if (!fa || !src || (off + len) > fa->fa_size) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    addr = fa->fa_off + off;
    p = (const uint8_t *)src;
    remaining = len;

    if (addr & 1U) {
        uint8_t temp[2];
        uint32_t half_off = addr - 1U;
        if (onchip_flash_read(half_off, temp, sizeof(temp)) < 0) {
            return -1;
        }
        temp[1] = *p;
        if (onchip_flash_write(half_off, temp, sizeof(temp)) < 0) {
            return -1;
        }
        addr += 1U;
        p += 1U;
        remaining -= 1U;
    }

    if (remaining >= 2U) {
        uint32_t even_len = remaining & ~1U;
        if (onchip_flash_write(addr, p, even_len) < 0) {
            return -1;
        }
        addr += even_len;
        p += even_len;
        remaining -= even_len;
    }

    if (remaining != 0U) {
        uint8_t temp[2];
        if (onchip_flash_read(addr, temp, sizeof(temp)) < 0) {
            return -1;
        }
        temp[0] = *p;
        if (onchip_flash_write(addr, temp, sizeof(temp)) < 0) {
            return -1;
        }
    }

    return 0;
}

int flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    if (!fa || (off + len) > fa->fa_size) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    if (onchip_flash_erase(fa->fa_off + off, len) < 0) {
        return -1;
    }
    return 0;
}

uint32_t flash_area_align(const struct flash_area *fa)
{
    (void)fa;
    return 2;
}

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
    (void)fa;
    return 0xFF;
}

int flash_area_sector_from_off(uint32_t off, struct flash_sector *sector)
{
    if (!sector) {
        return -1;
    }
    sector->fs_off = (off / BOOT_FLASH_SECTOR_SIZE) * BOOT_FLASH_SECTOR_SIZE;
    sector->fs_size = BOOT_FLASH_SECTOR_SIZE;
    return 0;
}

int flash_area_get_sector(const struct flash_area *fa, uint32_t off, struct flash_sector *sector)
{
    if (!fa || !sector || off >= fa->fa_size) {
        return -1;
    }
    return flash_area_sector_from_off(off, sector);
}

int flash_area_get_sectors(int fa_id, uint32_t *count, struct flash_sector *sectors)
{
    const struct flash_area *fa = flash_area_find((uint8_t)fa_id);
    uint32_t max_count;
    uint32_t needed;

    if (!fa || !count || !sectors) {
        return -1;
    }

    max_count = *count;
    needed = fa->fa_size / BOOT_FLASH_SECTOR_SIZE;
    if (needed > max_count) {
        return -1;
    }

    for (uint32_t i = 0; i < needed; i++) {
        sectors[i].fs_off = i * BOOT_FLASH_SECTOR_SIZE;
        sectors[i].fs_size = BOOT_FLASH_SECTOR_SIZE;
    }
    *count = needed;
    return 0;
}

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    if (image_index != 0) {
        return -1;
    }
    if (slot == 0) {
        return FLASH_AREA_IMAGE_0;
    }
    if (slot == 1) {
        return FLASH_AREA_IMAGE_1;
    }
    return -1;
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    if (image_index != 0) {
        return -1;
    }
    if (area_id == FLASH_AREA_IMAGE_0) {
        return 0;
    }
    if (area_id == FLASH_AREA_IMAGE_1) {
        return 1;
    }
    return -1;
}

int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}
