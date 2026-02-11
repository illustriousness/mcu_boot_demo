#include "core_cm4.h"
#include "core_cmFunc.h"
#include "main.h"
#include "rtm.h"
#include "shell.h"
#include "upgrade.h"
#include "boot_port.h"
#include <stdlib.h>
// #define LOG_D(...) rt_kprintf(__VA_ARGS__)
#define LOG_D(...)
#define APP_DIAG_MARK_ADDR 0x2000BF80u

void run_app(int argc, char **argv)
{
    const char *arg;
    uint32_t app_addr = 0x08000000 + 30 * 1024; // 应用程序的起始地址
    uint32_t flash_end = BOOT_FLASH_BASE + BOOT_FLASH_SIZE;

    rt_kprintf("[run_app] enter argc=%d\n", argc);

    if (argc == 2)
    {
        arg = argv[1];
        if ((arg[0] == '0') && ((arg[1] == 'x') || (arg[1] == 'X')))
        {
            app_addr = (uint32_t)strtoul(arg + 2, RT_NULL, 16);
        }
        else
        {
            app_addr = (uint32_t)strtoul(arg, RT_NULL, 10);
        }
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
