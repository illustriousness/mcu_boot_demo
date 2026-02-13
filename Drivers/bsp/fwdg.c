/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2022-01-25     iysheng           first version
 */
#define RT_USING_WDT

#include <main.h>
#include "fwdg.h"
#define DBG_TAG "drv.wdt"
#define DBG_LVL DBG_ERROR
#include <rtdbg.h>

#ifdef RT_USING_WDT

typedef struct
{
    uint32_t min_threshold_s;
    uint32_t max_threshold_s;
    uint32_t current_threshold_s;
} gd32_wdt_device_t;

static gd32_wdt_device_t g_wdt_dev;
// ErrStatus fwdgt_config(uint16_t reload_value, uint8_t prescaler_div)

#define RT_DEVICE_CTRL_WDT_START     1
#define RT_DEVICE_CTRL_WDT_KEEPALIVE -1

#define RT_DEVICE_CTRL_WDT_SET_TIMEOUT 6
#define RT_DEVICE_CTRL_WDT_GET_TIMEOUT 0
int bsp_wdt_control(int cmd, void *arg)
{
    uint32_t param;

    switch (cmd)
    {
    case RT_DEVICE_CTRL_WDT_KEEPALIVE:
        fwdgt_counter_reload();
        break;
    case RT_DEVICE_CTRL_WDT_SET_TIMEOUT:
        param = *(uint32_t *)arg;
        if ((param > g_wdt_dev.max_threshold_s) ||
            (param < g_wdt_dev.min_threshold_s))
        {
            LOG_E("invalid param@%u", param);
            return -RT_EINVAL;
        }
        else
        {
            g_wdt_dev.current_threshold_s = param;
            LOG_I("set timeout %d s", param);
        }
        fwdgt_write_enable();
        fwdgt_config(param * 40000 >> 8, FWDGT_PSC_DIV256);
        fwdgt_write_disable();
        break;
    case RT_DEVICE_CTRL_WDT_GET_TIMEOUT:
        *(uint32_t *)arg = g_wdt_dev.current_threshold_s;
        LOG_D("timeout %d", g_wdt_dev.current_threshold_s);
        break;
    case RT_DEVICE_CTRL_WDT_START:
        fwdgt_enable();
        break;
    }

    return RT_EOK;
}

void bsp_wdt_init()
{
    rcu_osci_on(RCU_IRC40K);
    if (ERROR == rcu_osci_stab_wait(RCU_IRC40K))
    {
        LOG_E("failed init IRC40K clock for free watchdog.");
        return;
    }

    g_wdt_dev.min_threshold_s = 1;
    g_wdt_dev.max_threshold_s = (0xfff << 8) / 40000;
    LOG_I("threshold section [%u, %d]",
          g_wdt_dev.min_threshold_s, g_wdt_dev.max_threshold_s);
}

//app 喂狗 bl不喂狗 如果是上电复位则启动看门狗 如果发现是看门狗复位 则不初始化看门狗
static int8_t app_is_invalid_flag;

void clear_app_invalid_flag()
{
    app_is_invalid_flag = 0;
}

int8_t app_is_invalid()
{
    return app_is_invalid_flag;
}

static void wdt_init()
{
    if (!rcu_flag_get(RCU_FLAG_FWDGTRST))
    {
        bsp_wdt_init();

        uint32_t timeout_s = 5;
        bsp_wdt_control(RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout_s);
        bsp_wdt_control(RT_DEVICE_CTRL_WDT_START, &timeout_s);
    }
    else
    {
        app_is_invalid_flag = 1;
        LOG_E("APP is invalid");
    }
    rcu_all_reset_flag_clear();
}
INIT_PREV_EXPORT(wdt_init);

// #include "finsh.h"
// int wdt_demo(int argc, char *argv[])
// {
//     int cmd;
//     int arg;
//     if (argc < 2)
//     {
//         return 0;
//     }
//     cmd = rt_atoi(argv[1]);
//     if (argv[2])
//     {
//         arg = rt_atoi(argv[2]);
//     }
//     bsp_wdt_control(cmd, &arg);
//     return 0;
// }
// MSH_CMD_EXPORT(wdt_demo, desc);
#endif
