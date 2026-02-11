#include <stdint.h>

#define APP_RELOC_MAGIC           0x524C4F43u /* "RLOC" */
#define APP_RELOC_VERSION         1u
#define APP_RELOC_ARM_RELATIVE    23u
#define APP_RELOC_HEADER_MIN_SIZE (11u * 4u)

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

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t __bss_end;

void app_runtime_relocate_from_reset(const void *reloc_info_runtime, int32_t delta)
{
    const app_reloc_info_t *info = (const app_reloc_info_t *)reloc_info_runtime;
    const elf32_rel_t *rels;
    uint32_t rel_count;
    uintptr_t data_size;
    uintptr_t rw_start;
    uintptr_t rw_end;
    uintptr_t link_flash_end;
    uintptr_t link_flash_base;
    uint32_t i;

    if (info == (const app_reloc_info_t *)0)
    {
        return;
    }
    if ((info->magic != APP_RELOC_MAGIC) || (info->version != APP_RELOC_VERSION))
    {
        return;
    }
    if ((info->struct_size < APP_RELOC_HEADER_MIN_SIZE) || ((info->struct_size & 0x3u) != 0u))
    {
        return;
    }
    if ((info->rel_dyn_size == 0u) || ((info->rel_dyn_size & (sizeof(elf32_rel_t) - 1u)) != 0u))
    {
        return;
    }
    if ((info->rel_ent_size != 0u) && (info->rel_ent_size != sizeof(elf32_rel_t)))
    {
        return;
    }

    rels = (const elf32_rel_t *)(uintptr_t)((int32_t)info->rel_dyn_vma + delta);
    rel_count = info->rel_dyn_size / sizeof(elf32_rel_t);
    if ((uintptr_t)&_edata < (uintptr_t)&_sdata)
    {
        return;
    }
    data_size = (uintptr_t)&_edata - (uintptr_t)&_sdata;
    if (((uintptr_t)&_sidata > (UINTPTR_MAX - data_size)))
    {
        return;
    }
    link_flash_base = (uintptr_t)info->link_base;
    link_flash_end = (uintptr_t)&_sidata + data_size;
    if ((link_flash_end <= link_flash_base) || (link_flash_end < (uintptr_t)&_sidata))
    {
        return;
    }
    rw_start = (uintptr_t)&_sdata;
    rw_end = (uintptr_t)&__bss_end;
    if ((rw_end <= rw_start) || ((rw_end - rw_start) < sizeof(uint32_t)))
    {
        return;
    }

    for (i = 0; i < rel_count; i++)
    {
        uint32_t type = rels[i].r_info & 0xFFu;
        uintptr_t target_addr = rels[i].r_offset;
        uint32_t value;

        if (type != APP_RELOC_ARM_RELATIVE)
        {
            continue;
        }
        if ((target_addr < rw_start) || (target_addr > (rw_end - sizeof(uint32_t))))
        {
            continue;
        }

        value = *(volatile uint32_t *)(uintptr_t)target_addr;
        if (((uintptr_t)value < link_flash_base) || ((uintptr_t)value >= link_flash_end))
        {
            continue;
        }

        *(volatile uint32_t *)(uintptr_t)target_addr = (uint32_t)((int32_t)value + delta);
    }
}
