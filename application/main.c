#include "main.h"
#include "mcuboot_start.h"
#include "timer.h"
#include "app_reloc.h"
#include "boot_port.h"
#include "image.h"
#include "boot_param.h"
#include "fwdg.h"
#include <stdint.h>
#include <stdlib.h>

#define APP_JUMP_DEBUG     0
#define APP_DIAG_MARK_ADDR 0x2000BF80u
#define BOOT_AUTO_START_DELAY_MS 3000u
#define BOOT_TRIAL_MAX_BOOT_COUNT 2u

static void app_diag_mark(uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)APP_DIAG_MARK_ADDR = value;
    __DSB();
}

#if APP_JUMP_DEBUG
#define APP_JUMP_LOG(...) rt_kprintf("[app_jump] " __VA_ARGS__)
#else
#define APP_JUMP_LOG(...) ((void)0)
#endif

static void print_boot_param(const boot_param_t *param, const char *tag)
{
    if ((param == RT_NULL) || (tag == RT_NULL))
    {
        return;
    }

    rt_kprintf("[%s] active=%lu trial=%lu confirmed=%lu boot_count=%lu last=%lu\n",
               tag,
               (unsigned long)param->active_slot,
               (unsigned long)param->trial_slot,
               (unsigned long)param->confirmed,
               (unsigned long)param->trial_boot_count,
               (unsigned long)param->last_boot_slot);
}

static int parse_slot_arg(const char *arg, uint32_t *slot)
{
    uint32_t value;

    if ((arg == RT_NULL) || (slot == RT_NULL))
    {
        return -RT_EINVAL;
    }

    if ((arg[0] == '0') && ((arg[1] == 'x') || (arg[1] == 'X')))
    {
        value = (uint32_t)strtoul(arg + 2, RT_NULL, 16);
    }
    else
    {
        value = (uint32_t)strtoul(arg, RT_NULL, 10);
    }

    if (!boot_slot_is_valid(value))
    {
        return -RT_EINVAL;
    }

    *slot = value;
    return RT_EOK;
}

static int app_vector_seems_valid(uint32_t app_addr)
{
    uint32_t flash_end = BOOT_FLASH_BASE + BOOT_FLASH_SIZE;
    uint32_t sram_end = BOOT_SRAM_BASE + BOOT_SRAM_SIZE;
    uint32_t msp;
    uint32_t reset;

    if ((app_addr < BOOT_FLASH_BASE) || ((app_addr + 8u) > flash_end) || ((app_addr & 0x3u) != 0u))
    {
        return 0;
    }

    msp = *(volatile uint32_t *)(uintptr_t)(app_addr + 0u);
    reset = *(volatile uint32_t *)(uintptr_t)(app_addr + 4u);
    reset &= ~0x1u;

    if ((msp < BOOT_SRAM_BASE) || (msp > sram_end))
    {
        return 0;
    }
    if ((reset < BOOT_FLASH_BASE) || (reset >= flash_end))
    {
        return 0;
    }

    return 1;
}

static int resolve_slot_app_addr(uint32_t slot, uint32_t *app_addr, uint32_t *hdr_size)
{
    uint32_t slot_base;
    const struct image_header *hdr;
    uint32_t candidate;

    if ((app_addr == RT_NULL) || !boot_slot_is_valid(slot))
    {
        return -RT_EINVAL;
    }

    slot_base = boot_slot_addr(slot);
    if (slot_base == 0u)
    {
        return -RT_EINVAL;
    }

    /* Raw-vector image (no MCUboot header in this slot). */
    if (app_vector_seems_valid(slot_base))
    {
        *app_addr = slot_base;
        if (hdr_size != RT_NULL)
        {
            *hdr_size = 0u;
        }
        return RT_EOK;
    }

    /* Signed image: vector table starts at slot_base + ih_hdr_size. */
    hdr = (const struct image_header *)(uintptr_t)slot_base;
    if ((hdr->ih_magic != IMAGE_MAGIC) && (hdr->ih_magic != IMAGE_MAGIC_V1))
    {
        return -RT_EINVAL;
    }
    if ((hdr->ih_hdr_size < IMAGE_HEADER_SIZE) ||
        (hdr->ih_hdr_size > 0x400u) ||
        ((hdr->ih_hdr_size & 0x3u) != 0u))
    {
        return -RT_EINVAL;
    }

    candidate = slot_base + hdr->ih_hdr_size;
    if (!app_vector_seems_valid(candidate))
    {
        return -RT_EINVAL;
    }

    *app_addr = candidate;
    if (hdr_size != RT_NULL)
    {
        *hdr_size = hdr->ih_hdr_size;
    }
    return RT_EOK;
}

static int boot_select_app_addr(uint32_t *app_addr)
{
    boot_param_t param;
    uint32_t selected_slot;
    uint32_t selected_addr = 0u;
    uint32_t selected_hdr_size = 0u;
    uint32_t alt_slot;
    uint32_t alt_addr = 0u;
    uint32_t alt_hdr_size = 0u;
    int wdg_reset;
    int load_rc;
    uint8_t dirty = 0u;

    if (app_addr == RT_NULL)
    {
        return -RT_EINVAL;
    }

    load_rc = boot_param_load(&param);
    if (load_rc != RT_EOK)
    {
        boot_param_get_default(&param);
        dirty = 1u;
        rt_kprintf("[boot_state] invalid or empty param, fallback to default\n");
    }

    if (!boot_slot_is_valid(param.active_slot))
    {
        param.active_slot = BOOT_SLOT_0;
        dirty = 1u;
    }
    if ((param.trial_slot != BOOT_SLOT_INVALID) && !boot_slot_is_valid(param.trial_slot))
    {
        param.trial_slot = BOOT_SLOT_INVALID;
        param.confirmed = 1u;
        param.trial_boot_count = 0u;
        dirty = 1u;
    }
    if ((param.confirmed != 0u) && (param.confirmed != 1u))
    {
        param.confirmed = 1u;
        dirty = 1u;
    }

    selected_slot = param.active_slot;
    wdg_reset = (app_is_invalid() != 0);

    if ((param.trial_slot != BOOT_SLOT_INVALID) && (param.confirmed == 0u))
    {
        if (wdg_reset && (param.last_boot_slot == param.trial_slot))
        {
            rt_kprintf("[boot_state] watchdog reset on trial slot %lu, rollback to slot %lu\n",
                       (unsigned long)param.trial_slot,
                       (unsigned long)param.active_slot);
            selected_slot = param.active_slot;
            param.trial_slot = BOOT_SLOT_INVALID;
            param.confirmed = 1u;
            param.trial_boot_count = 0u;
            param.last_boot_slot = selected_slot;
            dirty = 1u;
        }
        else if (param.trial_boot_count >= BOOT_TRIAL_MAX_BOOT_COUNT)
        {
            rt_kprintf("[boot_state] trial boot count overflow, rollback to slot %lu\n",
                       (unsigned long)param.active_slot);
            selected_slot = param.active_slot;
            param.trial_slot = BOOT_SLOT_INVALID;
            param.confirmed = 1u;
            param.trial_boot_count = 0u;
            param.last_boot_slot = selected_slot;
            dirty = 1u;
        }
        else
        {
            selected_slot = param.trial_slot;
            param.last_boot_slot = selected_slot;
            param.trial_boot_count++;
            dirty = 1u;
            rt_kprintf("[boot_state] trial boot slot=%lu try=%lu/%u\n",
                       (unsigned long)selected_slot,
                       (unsigned long)param.trial_boot_count,
                       (unsigned int)BOOT_TRIAL_MAX_BOOT_COUNT);
        }
    }
    else if (wdg_reset)
    {
        rt_kprintf("[boot_state] watchdog reset but no pending trial, keep active slot %lu\n",
                   (unsigned long)selected_slot);
    }

    if (dirty)
    {
        int save_rc = boot_param_save(&param);
        if (save_rc != RT_EOK)
        {
            rt_kprintf("[boot_state] warn: save boot param rc=%d\n", save_rc);
        }
    }

    clear_app_invalid_flag();

    if (resolve_slot_app_addr(selected_slot, &selected_addr, &selected_hdr_size) != RT_EOK)
    {
        alt_slot = (selected_slot == BOOT_SLOT_0) ? BOOT_SLOT_1 : BOOT_SLOT_0;
        if (resolve_slot_app_addr(alt_slot, &alt_addr, &alt_hdr_size) == RT_EOK)
        {
            rt_kprintf("[boot_state] slot %lu invalid, fallback to slot %lu\n",
                       (unsigned long)selected_slot,
                       (unsigned long)alt_slot);
            selected_slot = alt_slot;
            selected_addr = alt_addr;
            selected_hdr_size = alt_hdr_size;
            param.active_slot = alt_slot;
            param.trial_slot = BOOT_SLOT_INVALID;
            param.confirmed = 1u;
            param.trial_boot_count = 0u;
            param.last_boot_slot = alt_slot;
            (void)boot_param_save(&param);
        }
        else
        {
            rt_kprintf("[boot_state] both slots invalid: slot0=0x%08lx slot1=0x%08lx\n",
                       (unsigned long)boot_slot_addr(BOOT_SLOT_0),
                       (unsigned long)boot_slot_addr(BOOT_SLOT_1));
            return -RT_EINVAL;
        }
    }

    print_boot_param(&param, "boot_state");

    *app_addr = selected_addr;
    rt_kprintf("[boot_state] slot=%lu app_addr=0x%08lx hdr_size=0x%08lx\n",
               (unsigned long)selected_slot,
               (unsigned long)selected_addr,
               (unsigned long)selected_hdr_size);
    if ((*app_addr == 0u) || ((*app_addr & 0x3u) != 0u))
    {
        return -RT_EINVAL;
    }

    return RT_EOK;
}

static void boot_handoff_by_state(void)
{
    uint32_t app_addr;
    int rc;

    rc = boot_select_app_addr(&app_addr);
    if (rc != RT_EOK)
    {
        if (resolve_slot_app_addr(BOOT_SLOT_0, &app_addr, RT_NULL) != RT_EOK)
        {
            app_addr = BOOT_FLASH_BASE + BOOT_PRIMARY_SLOT_OFFSET;
        }
        rt_kprintf("[boot_main] fallback app addr=0x%08lx (rc=%d)\n",
                   (unsigned long)app_addr, rc);
    }

    rt_kprintf("[boot_main] handoff app addr=0x%08lx\n", (unsigned long)app_addr);
    rt_kprintf("[boot_main] vec0=0x%08lx vec1=0x%08lx\n",
               (unsigned long)(*(volatile uint32_t *)(uintptr_t)(app_addr + 0u)),
               (unsigned long)(*(volatile uint32_t *)(uintptr_t)(app_addr + 4u)));
    JumpToApplication(app_addr);
    rt_kprintf("[boot_main] handoff returned unexpectedly\n");
}

static void jump_to_app(uint32_t msp, uint32_t entry)
{
    void (*AppEntry)(uint32_t RTMSymTab_Base, uint32_t RTMSymTab_Limit);

    app_diag_mark(0xA5A50020u);
    AppEntry = (void (*)(uint32_t, uint32_t))(entry | 0x1u);

    __set_MSP(msp);
    __set_PSP(msp);
    __set_CONTROL(0);
    __ISB();

    app_diag_mark(0xA5A50021u);

    app_diag_mark(0xA5A50022u);
    AppEntry(0, 0);

    while (1)
    {
    }
}

void JumpToApplication(uint32_t app_addr)
{
    uint32_t msp;
    uint32_t entry;
    uint32_t i;
    struct app_exec_context ctx;
    int rc;

    APP_JUMP_LOG("stage: begin app_addr=0x%08lx vec0=0x%08lx vec1=0x%08lx\n",
                 (unsigned long)app_addr,
                 (unsigned long)(*(volatile uint32_t *)app_addr),
                 (unsigned long)(*(volatile uint32_t *)(app_addr + 4u)));
    app_diag_mark(0xA5A50010u);
    __set_PRIMASK(1); // 关闭全局中断
    // 关闭所有外设
    // ... 在这里可以添加更多的清理代码

    // usart_deinit(USART0);
    /* Keep SWD pins alive for RTT/J-Link after handoff. */
    // nvic_irq_disable(USART0_IRQn);
    // can_deinit(CAN0);
    // nvic_irq_disable(USBD_LP_CAN0_RX0_IRQn);

    APP_JUMP_LOG("stage: peripherals cleaned\n");

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    for (i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;


    APP_JUMP_LOG("stage: irq off, MSP=0x%08lx PSP=0x%08lx CONTROL=0x%08lx\n",
                 (unsigned long)__get_MSP(), (unsigned long)__get_PSP(), (unsigned long)__get_CONTROL());
    app_diag_mark(0xA5A50011u);

    rc = app_prepare_exec(app_addr, &ctx);
    if (rc == RT_EOK)
    {
        app_diag_mark(0xA5A50012u);
        msp = ctx.msp;
        entry = ctx.entry;
        jump_to_app(msp, entry);
    }
    else
    {
        app_diag_mark(0xA5A50013u);
        rt_kprintf("[app_jump] app_prepare_exec rc=%d addr=0x%08lx vec0=0x%08lx vec1=0x%08lx\n",
                   rc,
                   (unsigned long)app_addr,
                   (unsigned long)(*(volatile uint32_t *)(uintptr_t)(app_addr + 0u)),
                   (unsigned long)(*(volatile uint32_t *)(uintptr_t)(app_addr + 4u)));
        APP_JUMP_LOG("stage: app_prepare_exec rc=%d\n", rc);
        APP_JUMP_LOG("stage: abort handoff due to relocation validation failure\n");
        return;
    }
}

int main(void)
{
    uint8_t handoff_done = 0u;

    SCB->VTOR = 0X08000000u;
    while (1)
    {
        rt_timer_check();
        if ((handoff_done == 0u) && (tick_get() > BOOT_AUTO_START_DELAY_MS))
        {
            handoff_done = 1u;
            boot_handoff_by_state();
        }
    }
}
#include "finsh.h"

void boot_state(void)
{
    boot_param_t param;
    int rc = boot_param_load(&param);

    rt_kprintf("[boot_state] load rc=%d wdg_reset=%d\n", rc, (int)app_is_invalid());
    print_boot_param(&param, "boot_state");
}
MSH_CMD_EXPORT(boot_state, show rollback state);

int boot_set_trial(int argc, char **argv)
{
    boot_param_t param;
    uint32_t slot;
    int rc;

    if (argc != 2)
    {
        rt_kprintf("usage: boot_set_trial <0|1>\n");
        return -RT_EINVAL;
    }

    rc = parse_slot_arg(argv[1], &slot);
    if (rc != RT_EOK)
    {
        rt_kprintf("[boot_set_trial] invalid slot arg\n");
        return rc;
    }

    rc = boot_param_load(&param);
    if (rc != RT_EOK)
    {
        boot_param_get_default(&param);
    }

    param.trial_slot = slot;
    param.confirmed = 0u;
    param.trial_boot_count = 0u;
    param.last_boot_slot = BOOT_SLOT_INVALID;

    rc = boot_param_save(&param);
    rt_kprintf("[boot_set_trial] slot=%lu rc=%d\n", (unsigned long)slot, rc);
    return rc;
}
MSH_CMD_EXPORT(boot_set_trial, set trial slot and arm rollback);

int boot_set_active(int argc, char **argv)
{
    boot_param_t param;
    uint32_t slot;
    int rc;

    if (argc != 2)
    {
        rt_kprintf("usage: boot_set_active <0|1>\n");
        return -RT_EINVAL;
    }

    rc = parse_slot_arg(argv[1], &slot);
    if (rc != RT_EOK)
    {
        rt_kprintf("[boot_set_active] invalid slot arg\n");
        return rc;
    }

    rc = boot_param_load(&param);
    if (rc != RT_EOK)
    {
        boot_param_get_default(&param);
    }

    param.active_slot = slot;
    param.trial_slot = BOOT_SLOT_INVALID;
    param.confirmed = 1u;
    param.trial_boot_count = 0u;
    param.last_boot_slot = slot;

    rc = boot_param_save(&param);
    rt_kprintf("[boot_set_active] slot=%lu rc=%d\n", (unsigned long)slot, rc);
    return rc;
}
MSH_CMD_EXPORT(boot_set_active, force active slot);

void boot_param_reset(void)
{
    boot_param_t param;
    int rc;

    boot_param_get_default(&param);
    rc = boot_param_save(&param);
    rt_kprintf("[boot_param_reset] rc=%d\n", rc);
}
MSH_CMD_EXPORT(boot_param_reset, reset rollback state);

void say_hello(void *parameter)
{
    rt_kprintf("hello! this program build time is %s %s\n", __DATE__, __TIME__);
}
MSH_CMD_EXPORT(say_hello, desc);

void reboot()
{
    NVIC_SystemReset();
}
MSH_CMD_EXPORT(reboot, reboot command);
