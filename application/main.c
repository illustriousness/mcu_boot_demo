#include "main.h"
#include "mcuboot_start.h"
#include "timer.h"

void JumpToApplication(uint32_t app_addr)
{
    // void (*AppEntry)(void);
    void (*AppEntry)(uint32_t RTMSymTab_Base, uint32_t RTMSymTab_Limit);
    uint32_t JumpAddress;

    // 关闭所有外设
    // ... 在这里可以添加更多的清理代码
    usart_deinit(USART0);
    can_deinit(CAN0);
    gpio_deinit(GPIOA);
    nvic_irq_disable(USART0_IRQn);
    nvic_irq_disable(USBD_LP_CAN0_RX0_IRQn);

    // 禁用中断
    __set_PRIMASK(1); // 关闭全局中断
    // rt_hw_interrupt_disable();
    // 获取应用程序的起始地址
    JumpAddress = *(volatile uint32_t *)(app_addr + 4);      // 读取应用程序的堆栈指针
    AppEntry = (void (*)(uint32_t, uint32_t))JumpAddress; // 获取应用程序的入口地址

    // 设置主栈指针并切换向量表
    __set_MSP(*(volatile uint32_t *)app_addr); // 设置堆栈指针
    SCB->VTOR = app_addr;
    // uint32_t r0 __asm("r0") = (uint32_t)&RTMSymTab$$Base;
    // uint32_t r1 __asm("r1") = (uint32_t)&RTMSymTab$$Limit;
    // AppEntry(r0, r1);
    // __set_PRIMASK(0); // 关闭全局中断
    // uint32_t reg_r0 = (uint32_t)&RTMSymTab$$Base;
    // uint32_t reg_r1 = (uint32_t)&RTMSymTab$$Limit;
    // AppEntry(reg_r0, reg_r1);
    AppEntry(0, 0);
}

int main(void)
{
    // mcuboot_start();
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
