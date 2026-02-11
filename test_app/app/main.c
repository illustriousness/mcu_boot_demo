#include "SEGGER_RTT.h"
#include <stdint.h>
#include <stdlib.h>
#include "finsh.h"
#include "main.h"

#define APP_DIAG_MARK_ADDR         0x2000BF80u
#define SEGGER_RTT_printf(ch, ...) rt_kprintf(__VA_ARGS__)
static const uint32_t g_const_magic = 0xC0DE2026u;
static const char g_const_str[] = "CONST_OK";

static uint32_t g_data_magic = 0x1234ABCDu;
static char g_data_str[] = "DATA_OK";

static uint32_t g_bss_magic;
static char g_bss_buf[8];

static void app_diag_mark(unsigned int value)
{
    *(volatile unsigned int *)(APP_DIAG_MARK_ADDR) = value;
}

static void app_dump_startup_checks(void)
{
    char *end = NULL;
    unsigned long v_dec;
    unsigned long v_hex;
    unsigned long v_tail;

    SEGGER_RTT_printf(0, "[app] const magic=0x%08lx addr=0x%08lx str=%s\r\n",
                      (unsigned long)g_const_magic,
                      (unsigned long)(uintptr_t)&g_const_magic,
                      g_const_str);
    SEGGER_RTT_printf(0, "[app] data  magic=0x%08lx addr=0x%08lx str=%10s\r\n",
                      (unsigned long)g_data_magic,
                      (unsigned long)(uintptr_t)&g_data_magic,
                      g_data_str);
    SEGGER_RTT_printf(0, "[app] bss   before magic=0x%08lx addr=0x%08lx buf0=0x%02x buf1=0x%02x\r\n",
                      (unsigned long)g_bss_magic,
                      (unsigned long)(uintptr_t)&g_bss_magic,
                      (unsigned int)(unsigned char)g_bss_buf[0],
                      (unsigned int)(unsigned char)g_bss_buf[1]);

    g_bss_magic = 0x55AA33CCu;
    g_bss_buf[0] = 'O';
    g_bss_buf[1] = 'K';
    g_bss_buf[2] = '\0';
    g_data_magic ^= 0x11110000u;

    SEGGER_RTT_printf(0, "[app] bss   after  magic=0x%08lx buf=%s\r\n",
                      (unsigned long)g_bss_magic,
                      g_bss_buf);
    SEGGER_RTT_printf(0, "[app] data  after  magic=0x%08lx\r\n",
                      (unsigned long)g_data_magic);

    // v_dec = strtoul("12345", &end, 10);
    // SEGGER_RTT_printf(0, "[app] strtoul dec='12345' -> %lu end=0x%02x\r\n",
    //                   v_dec,
    //                   (unsigned int)((end != NULL && *end != '\0') ? (unsigned char)*end : 0u));

    // v_hex = strtoul("0x1A2B", &end, 0);
    // SEGGER_RTT_printf(0, "[app] strtoul hex='0x1A2B' -> 0x%08lx end=0x%02x\r\n",
    //                   v_hex,
    //                   (unsigned int)((end != NULL && *end != '\0') ? (unsigned char)*end : 0u));

    // v_tail = strtoul("77xyz", &end, 10);
    // SEGGER_RTT_printf(0, "[app] strtoul tail='77xyz' -> %lu end='%c'(0x%02x)\r\n",
    //                   v_tail,
    //                   (end != NULL && *end != '\0') ? *end : '.',
    //                   (unsigned int)((end != NULL && *end != '\0') ? (unsigned char)*end : 0u));
}
MSH_CMD_EXPORT_ALIAS(app_dump_startup_checks, dump_startup, desc);
//8.3ns 1000 8.3us 1000 8.3ms
void delay(int ms)
{
    for (int i = 0; i < ms; i++)
    {
        for (int j = 0; j < 1000; j++)
        {
            __asm("nop");
        }
    }
}

int main()
{
    uint32_t last_tick = 0u;
    __asm__ volatile("cpsie i");


    // app_diag_mark(0xA5A5B001u);
    /* Keep boot/app RTT session continuous; avoid force re-init on handoff. */
    // SEGGER_RTT_SetFlagsUpBuffer(0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    // rt_kprintf("[app] entry begin SystemCoreClock %d \r\n", SystemCoreClock);
    app_dump_startup_checks();
    // app_dump_startup_checks();
    // app_diag_mark(0xA5A5B002u);
    while (1)
    {
        // app_diag_mark(0xA5A5B003u);
        // SEGGER_RTT_printf(0, "Hello, world! loop=%lu %d\r\n", (unsigned long)last_tick++, tick_get());
        // delay(10000);
        rt_timer_check();
        if (tick_get() - last_tick >= 1000)
        {
            last_tick = tick_get();
            rt_kprintf("[app] tick=%lu\r\n", (unsigned long)last_tick);
        }
    }
    return 0;
}
void say_hello(void *parameter)
{
    rt_kprintf("hello! this app build time is %s %s\n", __DATE__, __TIME__);
}
MSH_CMD_EXPORT(say_hello, desc);

void reboot()
{
    NVIC_SystemReset();
}
MSH_CMD_EXPORT(reboot, reboot command);
