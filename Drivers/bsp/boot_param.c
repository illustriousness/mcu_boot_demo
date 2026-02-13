#include "boot_param.h"

#include "main.h"
#include "rtthread.h"

static uint32_t boot_param_crc32(const uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    uint32_t j;

    for (i = 0; i < len; i++)
    {
        crc ^= (uint32_t)buf[i];
        for (j = 0; j < 8u; j++)
        {
            if (crc & 1u)
            {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

static uint32_t boot_param_calc_crc(const boot_param_t *param)
{
    boot_param_t tmp;

    tmp = *param;
    tmp.crc32 = 0u;

    return boot_param_crc32((const uint8_t *)&tmp, sizeof(tmp));
}

static int boot_param_is_valid(const boot_param_t *param)
{
    if (param->magic != BOOT_PARAM_MAGIC)
    {
        return 0;
    }
    if (param->version != BOOT_PARAM_VERSION)
    {
        return 0;
    }
    if (!boot_slot_is_valid(param->active_slot))
    {
        return 0;
    }
    if ((param->trial_slot != BOOT_SLOT_INVALID) && !boot_slot_is_valid(param->trial_slot))
    {
        return 0;
    }
    if ((param->confirmed != 0u) && (param->confirmed != 1u))
    {
        return 0;
    }
    if (param->crc32 != boot_param_calc_crc(param))
    {
        return 0;
    }

    return 1;
}

void boot_param_get_default(boot_param_t *param)
{
    if (param == RT_NULL)
    {
        return;
    }

    param->magic = BOOT_PARAM_MAGIC;
    param->version = BOOT_PARAM_VERSION;
    param->active_slot = BOOT_SLOT_0;
    param->trial_slot = BOOT_SLOT_INVALID;
    param->confirmed = 1u;
    param->trial_boot_count = 0u;
    param->last_boot_slot = BOOT_SLOT_0;
    param->reserved0 = 0u;
    param->crc32 = boot_param_calc_crc(param);
}

int boot_param_load(boot_param_t *param)
{
    if (param == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (onchip_flash_read(BOOT_PARAM_OFFSET, (uint8_t *)param, sizeof(*param)) < 0)
    {
        boot_param_get_default(param);
        return -RT_ERROR;
    }

    if (!boot_param_is_valid(param))
    {
        boot_param_get_default(param);
        return -RT_ERROR;
    }

    return RT_EOK;
}

int boot_param_save(const boot_param_t *param)
{
    boot_param_t to_write;

    if (param == RT_NULL)
    {
        return -RT_EINVAL;
    }

    to_write = *param;
    to_write.magic = BOOT_PARAM_MAGIC;
    to_write.version = BOOT_PARAM_VERSION;
    to_write.crc32 = boot_param_calc_crc(&to_write);

    if (onchip_flash_erase(BOOT_PARAM_OFFSET, BOOT_PARAM_SIZE) < 0)
    {
        return -RT_ERROR;
    }

    if (onchip_flash_write(BOOT_PARAM_OFFSET, (const uint8_t *)&to_write, sizeof(to_write)) < 0)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

int boot_slot_is_valid(uint32_t slot)
{
    return (slot == BOOT_SLOT_0) || (slot == BOOT_SLOT_1);
}

uint32_t boot_slot_addr(uint32_t slot)
{
    if (slot == BOOT_SLOT_0)
    {
        return BOOT_FLASH_BASE + BOOT_PRIMARY_SLOT_OFFSET;
    }
    if (slot == BOOT_SLOT_1)
    {
        return BOOT_FLASH_BASE + BOOT_SECONDARY_SLOT_OFFSET;
    }
    return 0u;
}
