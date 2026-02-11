#include "main.h"
#include "mcuboot_start.h"
#include "timer.h"
#include "app_reloc.h"
#include <stdint.h>

#define APP_JUMP_DEBUG 1
#define APP_DIAG_MARK_ADDR 0x2000BF80u

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
    can_deinit(CAN0);
    // usart_deinit(USART0);
    /* Keep SWD pins alive for RTT/J-Link after handoff. */
    // nvic_irq_disable(USART0_IRQn);
    nvic_irq_disable(USBD_LP_CAN0_RX0_IRQn);

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
        APP_JUMP_LOG("stage: app_prepare_exec rc=%d\n", rc);
        APP_JUMP_LOG("stage: abort handoff due to relocation validation failure\n");
        return;
    }
}

int main(void)
{
    // mcuboot_start();
    SCB->VTOR = 0X08000000u;
    while (1)
    {
        rt_timer_check();
    }
}
#include "finsh.h"
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
