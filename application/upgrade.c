#include "core_cm4.h"
#include "core_cmFunc.h"
#include "main.h"
#include "rtm.h"
#include "shell.h"
#include "upgrade.h"
#include "boot_port.h"
#include "mcuboot_start.h"
#include "image.h"
#include <stdlib.h>

// #define LOG_D(...) rt_kprintf(__VA_ARGS__)
#define LOG_D(...)
#define APP_DIAG_MARK_ADDR 0x2000BF80u

static int parse_addr_arg(const char *arg, uint32_t *app_addr)
{
    if ((arg == RT_NULL) || (app_addr == RT_NULL))
    {
        return -RT_EINVAL;
    }

    if ((arg[0] == '0') && ((arg[1] == 'x') || (arg[1] == 'X')))
    {
        *app_addr = (uint32_t)strtoul(arg + 2, RT_NULL, 16);
    }
    else
    {
        *app_addr = (uint32_t)strtoul(arg, RT_NULL, 10);
    }

    return RT_EOK;
}

static int adjust_addr_if_signed_image(uint32_t *app_addr)
{
    uint32_t flash_end = BOOT_FLASH_BASE + BOOT_FLASH_SIZE;
    const struct image_header *hdr;
    uint32_t addr;

    if (app_addr == RT_NULL)
    {
        return -RT_EINVAL;
    }

    addr = *app_addr;
    if ((addr < BOOT_FLASH_BASE) || ((addr + IMAGE_HEADER_SIZE) > flash_end) || ((addr & 0x3u) != 0u))
    {
        return -RT_EINVAL;
    }

    hdr = (const struct image_header *)(uintptr_t)addr;
    if ((hdr->ih_magic != IMAGE_MAGIC) && (hdr->ih_magic != IMAGE_MAGIC_V1))
    {
        return RT_EOK;
    }

    if ((hdr->ih_hdr_size < IMAGE_HEADER_SIZE) ||
        (hdr->ih_hdr_size > 0x400u) ||
        ((hdr->ih_hdr_size & 0x3u) != 0u) ||
        ((addr + hdr->ih_hdr_size + 8u) > flash_end))
    {
        return -RT_EINVAL;
    }

    rt_kprintf("[run_app] signed image header detected, hdr_size=0x%04x\n",
               (unsigned int)hdr->ih_hdr_size);
    *app_addr = addr + hdr->ih_hdr_size;
    return RT_EOK;
}

void run_app(int argc, char **argv)
{
    const char *arg;
    uint32_t app_addr = BOOT_FLASH_BASE + BOOT_PRIMARY_SLOT_OFFSET;
    uint32_t flash_end = BOOT_FLASH_BASE + BOOT_FLASH_SIZE;

    rt_kprintf("[run_app] enter argc=%d\n", argc);

    if (argc == 2)
    {
        arg = argv[1];
        if (parse_addr_arg(arg, &app_addr) != RT_EOK)
        {
            rt_kprintf("[run_app] invalid addr arg\n");
            return;
        }
    }

    if (adjust_addr_if_signed_image(&app_addr) != RT_EOK)
    {
        rt_kprintf("[run_app] invalid signed image header at addr=0x%08lx\n", (unsigned long)app_addr);
        return;
    }

    if ((app_addr < BOOT_FLASH_BASE) || (app_addr >= flash_end) || ((app_addr & 0x3u) != 0u))
    {
        rt_kprintf("[run_app] invalid addr=0x%08lx\n", (unsigned long)app_addr);
        return;
    }

    LOG_D("run app at 0x%08x\n", app_addr);
    rt_kprintf("[run_app] addr=0x%08lx vec0=0x%08lx vec1=0x%08lx\n",
               (unsigned long)app_addr,
               (unsigned long)(*(volatile uint32_t *)app_addr),
               (unsigned long)(*(volatile uint32_t *)(app_addr + 4u)));
    JumpToApplication(app_addr);
}
MSH_CMD_EXPORT(run_app, desc);

void boot(void)
{
    mcuboot_start();
}
MSH_CMD_EXPORT(boot, start app via mcuboot policy);

void app_diag(void)
{
    uint32_t mark = *(volatile uint32_t *)(uintptr_t)APP_DIAG_MARK_ADDR;
    rt_kprintf("[app_diag] mark=0x%08lx\n", (unsigned long)mark);
}
MSH_CMD_EXPORT(app_diag, app jump diag marker);

void app_diag_clr(void)
{
    *(volatile uint32_t *)(uintptr_t)APP_DIAG_MARK_ADDR = 0u;
    rt_kprintf("[app_diag] cleared\n");
}
MSH_CMD_EXPORT(app_diag_clr, clear app jump diag marker);

void reset_flags(void)
{
    rt_kprintf("[reset_flags] EPRST=%d PORRST=%d SWRST=%d FWDGTRST=%d WWDGTRST=%d LPRST=%d\n",
               rcu_flag_get(RCU_FLAG_EPRST),
               rcu_flag_get(RCU_FLAG_PORRST),
               rcu_flag_get(RCU_FLAG_SWRST),
               rcu_flag_get(RCU_FLAG_FWDGTRST),
               rcu_flag_get(RCU_FLAG_WWDGTRST),
               rcu_flag_get(RCU_FLAG_LPRST));
}
MSH_CMD_EXPORT(reset_flags, show reset source flags);

void reset_flags_clr(void)
{
    rcu_all_reset_flag_clear();
    rt_kprintf("[reset_flags] cleared\n");
}
MSH_CMD_EXPORT(reset_flags_clr, clear reset source flags);
