/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-09-20     Bernard      Change the name to components.c
 *                             And all components related header files.
 * 2012-12-23     Bernard      fix the pthread initialization issue.
 * 2013-06-23     Bernard      Add the init_call for components initialization.
 * 2013-07-05     Bernard      Remove initialization feature for MS VC++ compiler
 * 2015-02-06     Bernard      Remove the MS VC++ support and move to the kernel
 * 2015-05-04     Bernard      Rename it to components.c because compiling issue
 *                             in some IDEs.
 * 2015-07-29     Arda.Fu      Add support to use RT_USING_USER_MAIN with IAR
 * 2018-11-22     Jesven       Add secondary cpu boot up
 * 2023-09-15     xqyjlj       perf rt_hw_interrupt_disable/enable
 */

// #include <rthw.h>
#include "rtthread.h"

// cfg
#define RT_USING_COMPONENTS_INIT
#define RT_USING_USER_MAIN
// end cfg

#ifdef RT_USING_COMPONENTS_INIT
/*
 * Components Initialization will initialize some driver and components as following
 * order:
 * rti_start         --> 0
 * BOARD_EXPORT      --> 1
 * rti_board_end     --> 1.end
 *
 * DEVICE_EXPORT     --> 2
 * COMPONENT_EXPORT  --> 3
 * FS_EXPORT         --> 4
 * ENV_EXPORT        --> 5
 * APP_EXPORT        --> 6
 *
 * rti_end           --> 6.end
 *
 * These automatically initialization, the driver or component initial function must
 * be defined with:
 * INIT_BOARD_EXPORT(fn);
 * INIT_DEVICE_EXPORT(fn);
 * ...
 * INIT_APP_EXPORT(fn);
 * etc.
 */
static void rti_start(void)
{
    // return 0;
}
INIT_EXPORT(rti_start, "0");

static void rti_board_start(void)
{
    // return 0;
}
INIT_EXPORT(rti_board_start, "0.end");

static void rti_board_end(void)
{
    // return 0;
}
INIT_EXPORT(rti_board_end, "1.end");

static void rti_end(void)
{
    // return 0;
}
INIT_EXPORT(rti_end, "6.end");

/**
 * @brief  Onboard components initialization. In this function, the board-level
 *         initialization function will be called to complete the initialization
 *         of the on-board peripherals.
 */
void rt_components_board_init(void)
{
    volatile const init_fn_t* fn_ptr;

    for (fn_ptr = &__rt_init_rti_board_start; fn_ptr < &__rt_init_rti_board_end; fn_ptr++) {
        (*fn_ptr)();
    }
}

/**
 * @brief  RT-Thread Components Initialization.
 */
void rt_components_init(void)
{
#ifdef RT_DEBUGING_AUTO_INIT
    int result;
    const struct rt_init_desc* desc;

    rt_kprintf("do components initialization.\n");
    for (desc = &__rt_init_desc_rti_board_end; desc < &__rt_init_desc_rti_end; desc++) {
        rt_kprintf("initialize %s\n", desc->fn_name);
        result = desc->fn();
        rt_kprintf(":%d done\n", result);
    }
#else
    volatile const init_fn_t* fn_ptr;

    for (fn_ptr = &__rt_init_rti_board_end; fn_ptr < &__rt_init_rti_end; fn_ptr++) {
        (*fn_ptr)();
    }
#endif /* RT_DEBUGING_AUTO_INIT */
}
#endif /* RT_USING_COMPONENTS_INIT */

#ifdef RT_USING_USER_MAIN

void rt_application_init(void)
{
    rt_components_init();
#ifdef __ARMCC_VERSION
    {
        extern int $Super$$main(void);
        $Super$$main(); /* for ARMCC. */
    }
#elif defined(__ICCARM__) || defined(__GNUC__) || defined(__TASKING__) || defined(__TI_COMPILER_VERSION__)
    extern int main(void);
    main();
#endif /* __ARMCC_VERSION */
}

void rt_hw_board_init(void)
{
    rt_components_board_init();
}

int rtthread_startup(void);

#ifdef __ARMCC_VERSION
extern int $Super$$main(void);
/* re-define main function */
int $Sub$$main(void)
{
    rtthread_startup();
    return 0;
}
#elif defined(__ICCARM__)
/* __low_level_init will auto called by IAR cstartup */
extern void __iar_data_init3(void);
int __low_level_init(void)
{
    // call IAR table copy function.
    __iar_data_init3();
    rtthread_startup();
    return 0;
}
#elif defined(__GNUC__)
/* Add -eentry to arm-none-eabi-gcc argument */
int entry(void)
{
    rtthread_startup();
    return 0;
}
#endif

/**
 * @brief  This function will call all levels of initialization functions to complete
 *         the initialization of the system, and finally start the scheduler.
 *
 * @return Normally never returns. If 0 is returned, the scheduler failed.
 */
int rtthread_startup(void)
{
    // rt_hw_local_irq_disable();
    extern void rt_system_timer_init(void);
    rt_system_timer_init();
    /* board level initialization
     * NOTE: please initialize heap inside board initialization.
     */
    rt_hw_board_init();

    /* show RT-Thread version */
    // rt_show_version();

    /* timer system initialization */


    /* scheduler system initialization */
    // rt_system_scheduler_init();

#ifdef RT_USING_SIGNALS
    /* signal system initialization */
    rt_system_signal_init();
#endif /* RT_USING_SIGNALS */

    /* create init_thread */
    rt_application_init();

    /* timer thread initialization */
    // rt_system_timer_thread_init();

    /* idle thread initialization */
    // rt_thread_idle_init();

    /* defunct thread initialization */
    // rt_thread_defunct_init();

#ifdef RT_USING_SMP
    rt_hw_spin_lock(&_cpus_lock);
#endif /* RT_USING_SMP */

    /* start scheduler */
    // rt_system_scheduler_start();

    /* never reach here */
    return 0;
}
#endif /* RT_USING_USER_MAIN */
