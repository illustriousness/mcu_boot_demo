#include "zmodem_tabby_rx_min.h"

#include "main.h"
#include "shell.h"
// #include "upgrade.h"
#include "usart.h"
#include "SEGGER_RTT.h"

#define DBG_LVL DBG_LOG
#define DBG_TAG "rz"

#include "rtdbg.h"

// #include <string.h>
#define memset rt_memset
#define memcpy rt_memcpy
#include "rtthread.h"
extern void LOG_HEX(uint32_t offset, uint8_t *buf, uint32_t size);
extern int rt_kprintf(const char *fmt, ...);
extern uint32_t tick_get(void);

extern uint8_t super_mode;

#define RT_ALIGN(size, align) (((size) + (align) - 1) & ~((align) - 1))

static void delay_ms(uint32_t ms)
{
    const uint32_t start = tick_get();
    while (tick_get() - start < ms)
    {
        // busy-wait
    }
}

static uint32_t zm_read(uint8_t *buf, uint16_t len)
{
    int res = usart_read(buf, len);
    // if (res)
    // {
    //     rt_kprintf("RX len %d\n", res);
    //     // LOG_HEX(0, buf, res);
    // }
    // else
    // {
    //     rt_kprintf("usart read no data\n");
    //     delay_ms(100);
    // }
    return res;
}

static void zm_write(const uint8_t *buf, uint16_t len)
{
    // rt_kprintf("TX len %d\n", len);
    // LOG_HEX(0, (uint8_t *)buf, len);
    usart_write((uint8_t *)buf, len);
}

static uint32_t zm_tick_get(void)
{
    return tick_get();
}

static uint32_t flash_write_pos = 0;
static uint32_t expected_size = 0;
static uint32_t received_size = 0;
static uint8_t rx_scratch[1024] __attribute__((aligned(2)));
static uint8_t flash_page[1024] __attribute__((aligned(2)));
static uint16_t flash_page_used = 0;

static int on_begin(const char *name, uint32_t size)
{
    (void)name;

    flash_write_pos = 0;
    expected_size = size;
    received_size = 0;
    flash_page_used = 0;

    onchip_flash_erase(APP_OFFSET, RT_ALIGN(size, 1024));
    // rt_kprintf("Receiving %s (%u bytes)\n", name, size);
    return 0;
}

static int flash_flush_page(void)
{
    if (!flash_page_used)
    {
        return 0;
    }

    if (flash_page_used & 1)
    {
        flash_page[flash_page_used++] = 0xFF;
    }

    if (onchip_flash_write(APP_OFFSET + flash_write_pos, flash_page, flash_page_used) != (int)flash_page_used)
    {
        return -1;
    }

    flash_write_pos += flash_page_used;
    flash_page_used = 0;
    return 0;
}

static int on_data(const uint8_t *data, uint16_t len)
{
    if (!len)
    {
        return 0;
    }

    received_size += len;

    while (len)
    {
        const uint16_t space = (uint16_t)(sizeof(flash_page) - flash_page_used);
        const uint16_t take = len < space ? len : space;
        memcpy(&flash_page[flash_page_used], data, take);
        flash_page_used = (uint16_t)(flash_page_used + take);
        data += take;
        len = (uint16_t)(len - take);

        if (flash_page_used == sizeof(flash_page))
        {
            if (flash_flush_page() != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

static void on_end(int ok)
{
    super_mode = 1;
    if (!ok)
    {
        rt_kprintf("ZMODEM receive failed (%d)\n", ok);
        return;
    }

    if (flash_flush_page() != 0)
    {
        rt_kprintf("Flash write failed\n");
        return;
    }

    rt_kprintf("Transfer complete, total bytes: %d\n", received_size);

    if (!received_size || received_size != expected_size)
    {
        LOG_E("Size mismatch: expected=%d got=%d\n", expected_size, received_size);
        return;
    }

    if (check_fireware(APP_ADDRESS, received_size) == 0)
    {
        LOG_I("CRC check passed.\n");
        flash_set_value("file_size", &received_size, 4);
        super_mode = 0;
        do_safe_boot(1000);
    }
    else
    {
        LOG_E("CRC check failed.\n");
    }
}

/**
 * @brief MSH command: Start Tabby-compatible minimal ZMODEM receive (single file)
 * Usage: rz
 */
void rz(int argc, char **argv)
{
    // (void)argc;
    // (void)argv;
    uint16_t timeout_ms = 5000;
    if (argv[1])
    {
        timeout_ms = (uint16_t)rt_atoi(argv[1]);
    }
    super_mode = 2;
    // SEGGER_RTT_SetFlagsUpBuffer(0, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
    zm_tabby_rx_t ctx;
    rz_tabby_init(&ctx);

    ctx.read = zm_read;
    ctx.write = zm_write;
    ctx.tick_get = zm_tick_get;

    ctx.on_begin = on_begin;
    ctx.on_data = on_data;
    ctx.on_end = on_end;

    ctx.write_buf = rx_scratch;
    ctx.write_buf_size = sizeof(rx_scratch);

    ctx.user_pick_timeout_ms = timeout_ms;
    ctx.io_timeout_ms = 1000;

    rt_kprintf("ZMODEM receive ready (Tabby minimal).\n");
    /* Tabby attaches ZMODEM middleware asynchronously after opening the session.
     * Delay a bit to avoid missing the initial ZRINIT. */

    const int ret = rz_tabby(&ctx);
    if (ret != 0)
    {
        on_end(ret);
    }

    // SEGGER_RTT_SetFlagsUpBuffer(0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}
MSH_CMD_EXPORT(rz, ZMODEM receive file);
