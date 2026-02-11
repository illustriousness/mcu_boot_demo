#include "app_reloc.h"

#include "boot_port.h"
#include "gd32f30x.h"
#include "rtthread.h"

#define APP_RELOC_DEBUG 1

#if APP_RELOC_DEBUG
#define APP_RELOC_LOG(...) rt_kprintf("[app_reloc] " __VA_ARGS__)
#else
#define APP_RELOC_LOG(...) ((void)0)
#endif

#define APP_RELOC_MAGIC             0x524C4F43u /* "RLOC" */
#define APP_RELOC_VERSION           1u
#define APP_RELOC_HEADER_MIN_SIZE   (11u * 4u)
#define APP_RELOC_SEARCH_BYTES      0x400u
#define APP_RELOC_ARM_RELATIVE      23u
#define APP_RELOC_STARTUP_WINDOW    0x200u

#define APP_RELOC_FLASH_END         (BOOT_FLASH_BASE + BOOT_FLASH_SIZE)
#define APP_RELOC_SRAM_END          (BOOT_SRAM_BASE + BOOT_SRAM_SIZE)

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t struct_size;
    uint32_t link_base;
    uint32_t vector_vma;
    uint32_t vector_size;
    uint32_t rel_dyn_vma;
    uint32_t rel_dyn_size;
    uint32_t rel_ent_size;
    uint32_t entry_vma;
    uint32_t got_vma;
} app_reloc_info_t;

typedef struct
{
    uint32_t r_offset;
    uint32_t r_info;
} elf32_rel_t;

static int range_is_valid(uint32_t start, uint32_t size, uint32_t low, uint32_t high)
{
    uint32_t end;

    if (start < low)
    {
        return 0;
    }

    if (size == 0u)
    {
        return start <= high;
    }

    end = start + size - 1u;
    if (end < start)
    {
        return 0;
    }

    return end < high;
}

static int range_in_flash(uint32_t start, uint32_t size)
{
    return range_is_valid(start, size, BOOT_FLASH_BASE, APP_RELOC_FLASH_END);
}

static int range_in_sram(uint32_t start, uint32_t size)
{
    return range_is_valid(start, size, BOOT_SRAM_BASE, APP_RELOC_SRAM_END);
}

static const app_reloc_info_t *find_reloc_info(uint32_t app_addr, uint32_t scan_bytes)
{
    uint32_t offset;

    for (offset = 0; offset + sizeof(app_reloc_info_t) <= scan_bytes; offset += sizeof(uint32_t))
    {
        const app_reloc_info_t *info = (const app_reloc_info_t *)(uintptr_t)(app_addr + offset);

        if (info->magic != APP_RELOC_MAGIC)
        {
            continue;
        }
        if (info->version != APP_RELOC_VERSION)
        {
            continue;
        }
        if ((info->struct_size < APP_RELOC_HEADER_MIN_SIZE) || (info->struct_size > 0x100u))
        {
            continue;
        }
        if ((info->struct_size & 0x3u) != 0u)
        {
            continue;
        }
        return info;
    }

    return RT_NULL;
}

static int validate_reloc_info(uint32_t app_addr, const app_reloc_info_t *info)
{
    if (!range_in_flash(app_addr, sizeof(uint32_t)))
    {
        return -RT_EINVAL;
    }
    if (!range_in_flash(info->link_base, sizeof(uint32_t)))
    {
        return -RT_EINVAL;
    }
    if ((info->vector_size == 0u) || ((info->vector_size & 0x3u) != 0u))
    {
        return -RT_EINVAL;
    }
    if (info->vector_vma < info->link_base)
    {
        return -RT_EINVAL;
    }
    if ((info->rel_ent_size != 0u) && (info->rel_ent_size != sizeof(elf32_rel_t)))
    {
        return -RT_EINVAL;
    }
    if ((info->rel_dyn_size & (sizeof(elf32_rel_t) - 1u)) != 0u)
    {
        return -RT_EINVAL;
    }
    if ((info->got_vma != 0u) && !range_in_sram(info->got_vma, sizeof(uint32_t)))
    {
        return -RT_EINVAL;
    }

    return RT_EOK;
}

static int reject_flash_relocation_targets(uint32_t app_addr, const app_reloc_info_t *info)
{
    uint32_t rel_addr;
    uint32_t rel_count;
    uint32_t vector_start;
    uint32_t vector_end;
    uint32_t startup_start;
    uint32_t startup_end;
    uint32_t i;
    const elf32_rel_t *rels;

    if (info->rel_dyn_size == 0u)
    {
        return RT_EOK;
    }
    if (info->rel_dyn_vma < info->link_base)
    {
        return -RT_EINVAL;
    }

    rel_addr = app_addr + (info->rel_dyn_vma - info->link_base);
    if (!range_in_flash(rel_addr, info->rel_dyn_size))
    {
        return -RT_EINVAL;
    }

    rels = (const elf32_rel_t *)(uintptr_t)rel_addr;
    rel_count = info->rel_dyn_size / sizeof(elf32_rel_t);
    vector_start = info->vector_vma;
    vector_end = info->vector_vma + info->vector_size;
    startup_start = info->entry_vma & ~0x1u;
    /* Include the upper boundary word to avoid edge false positives. */
    startup_end = startup_start + APP_RELOC_STARTUP_WINDOW + sizeof(uint32_t);
    if (vector_end < vector_start)
    {
        return -RT_EINVAL;
    }
    if (startup_end < startup_start)
    {
        return -RT_EINVAL;
    }

    for (i = 0; i < rel_count; i++)
    {
        uint32_t type = rels[i].r_info & 0xFFu;
        uint32_t target_addr = rels[i].r_offset;

        if (type != APP_RELOC_ARM_RELATIVE)
        {
            continue;
        }
        /* Vector table is copied to RAM shadow and adjusted separately. */
        if ((target_addr >= vector_start) && (target_addr < vector_end))
        {
            continue;
        }
        /* Reset_Handler literal pool is consumed before app RAM reloc and
         * may legally live in FLASH. Keep the window tight. */
        if ((target_addr >= startup_start) && (target_addr < startup_end))
        {
            continue;
        }
        if (range_in_flash(target_addr, sizeof(uint32_t)))
        {
            APP_RELOC_LOG("forbidden flash reloc target idx=%lu addr=0x%08lx\n",
                          (unsigned long)i, (unsigned long)target_addr);
            return -RT_EINVAL;
        }
    }

    return RT_EOK;
}

int app_prepare_exec(uint32_t app_addr, struct app_exec_context *ctx)
{
    const app_reloc_info_t *info;
    uint32_t vector_src;
    uint32_t entry_raw;
    uint32_t scan_bytes;
    int32_t delta;
    int rc;

    if (ctx == RT_NULL)
    {
        return -RT_EINVAL;
    }

    APP_RELOC_LOG("begin app_addr=0x%08lx\n", (unsigned long)app_addr);

    if ((app_addr < BOOT_FLASH_BASE) || (app_addr >= APP_RELOC_FLASH_END))
    {
        APP_RELOC_LOG("addr out of range\n");
        return -RT_EINVAL;
    }

    scan_bytes = APP_RELOC_FLASH_END - app_addr;
    if (scan_bytes > APP_RELOC_SEARCH_BYTES)
    {
        scan_bytes = APP_RELOC_SEARCH_BYTES;
    }

    info = find_reloc_info(app_addr, scan_bytes);
    if (info == RT_NULL)
    {
        APP_RELOC_LOG("reloc header not found\n");
        return -RT_ENOSYS;
    }

    APP_RELOC_LOG("header link_base=0x%08lx vec_vma=0x%08lx vec_size=0x%08lx rel_vma=0x%08lx rel_size=0x%08lx\n",
                  (unsigned long)info->link_base, (unsigned long)info->vector_vma,
                  (unsigned long)info->vector_size, (unsigned long)info->rel_dyn_vma,
                  (unsigned long)info->rel_dyn_size);
    APP_RELOC_LOG("header entry_vma=0x%08lx got_vma=0x%08lx\n",
                  (unsigned long)info->entry_vma, (unsigned long)info->got_vma);

    rc = validate_reloc_info(app_addr, info);
    if (rc != RT_EOK)
    {
        APP_RELOC_LOG("validate failed rc=%d\n", rc);
        return rc;
    }
    rc = reject_flash_relocation_targets(app_addr, info);
    if (rc != RT_EOK)
    {
        APP_RELOC_LOG("rel.dyn has flash targets rc=%d\n", rc);
        return rc;
    }

    delta = (int32_t)app_addr - (int32_t)info->link_base;
    vector_src = app_addr + (info->vector_vma - info->link_base);

    if (!range_in_flash(vector_src, info->vector_size))
    {
        APP_RELOC_LOG("vector src out of flash range: 0x%08lx\n", (unsigned long)vector_src);
        return -RT_EINVAL;
    }

    ctx->msp = *(volatile uint32_t *)(uintptr_t)(vector_src + 0u);
    if ((ctx->msp <= BOOT_SRAM_BASE) || (ctx->msp > APP_RELOC_SRAM_END))
    {
        APP_RELOC_LOG("bad msp=0x%08lx\n", (unsigned long)ctx->msp);
        return -RT_EINVAL;
    }

    entry_raw = *(volatile uint32_t *)(uintptr_t)(vector_src + 4u);
    if ((entry_raw >= info->link_base) && (entry_raw < APP_RELOC_FLASH_END))
    {
        entry_raw = (uint32_t)((int32_t)entry_raw + delta);
    }
    /* Boot only sets MSP and jumps into Reset_Handler.
     * VTOR/r9/.data/.bss/ram-reloc are now done in app startup. */
    ctx->entry = entry_raw;
    ctx->entry |= 0x1u;
    ctx->vtor = 0u;
    ctx->pic_base = 0u;
    APP_RELOC_LOG("handoff reset=0x%08lx (app handles VTOR/pic_base/data/bss+ram-reloc)\n",
                  (unsigned long)ctx->entry);

    return RT_EOK;
}
