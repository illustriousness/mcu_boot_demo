

// #include "core_cm4.h"
// #include "core_cmFunc.h"
// #include "gd32f30x.h"

#include "main.h"
#include "timer.h"


static volatile uint32_t tick;
list_t _timer_list[TIMER_SKIP_LIST_LEVEL];

void tick_init(void)
{
    // SysTick_Config(SystemCoreClock / 1000U);
    tick = 0;
    SysTick_Config(120000000U / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 0x00U);
}

uint32_t tick_get(void)
{
    return tick;
}

void SysTick_Handler(void)
{
    ++tick;
}


static void
_timer_init(rt_timer_t timer, void (*timeout)(void *parameter), void *parameter, uint32_t time, uint8_t flag)
{
    int i;

    /* 设置标志 */
    timer->flag = flag;

    /* 设置为停用 */
    timer->flag &= ~RT_TIMER_FLAG_ACTIVATED;

    timer->timeout_func = timeout;
    timer->parameter = parameter;

    timer->timeout_tick = 0;
    timer->init_tick = time;

    /* 初始化定时器列表 */
    for (i = 0; i < TIMER_SKIP_LIST_LEVEL; i++)
    {
        rt_list_init(&(timer->row[i]));
    }
}

/**
 * @brief 查找下一个空计时器滴答声
 *
 * @paramtimer_list是时间列表数组
 *
 * @param timeout_tick 是下一个计时器的滴答声
 *
 * @return 返回操作状态。如果返回值为RT_EOK，则函数执行成功。
 *如果返回值为其他值，则表示本次操作失败。
 */
// static int8_t _timer_list_next_timeout(list_t timer_list[], uint32_t *timeout_tick)
// {
//     struct rt_timer *timer;
//     // rt_base_t        level;

//     /* 禁用中断 */
//     // rt_hw_interrupt_disable();

//     if (!rt_list_isempty(&timer_list[TIMER_SKIP_LIST_LEVEL - 1]))
//     {
//         timer         = rt_list_entry(timer_list[TIMER_SKIP_LIST_LEVEL - 1].next,
//                                       struct rt_timer, row[TIMER_SKIP_LIST_LEVEL - 1]);
//         *timeout_tick = timer->timeout_tick;

//         /* 使能中断 */
//         // rt_hw_interrupt_enable(level);

//         return 0;
//     }

//     /* 使能中断 */
//     // rt_hw_interrupt_enable(level);

//     return -1;
// }

/**
 * @brief 删除计时器
 *
 * @paramtimer 定时器的点
 */
rt_inline void _timer_remove(rt_timer_t timer)
{
    int i;

    for (i = 0; i < TIMER_SKIP_LIST_LEVEL; i++)
    {
        rt_list_remove(&timer->row[i]);
    }
}

void rt_timer_init(rt_timer_t timer, void (*timeout)(void *parameter), void *parameter, uint32_t time, uint8_t flag)
{
    _timer_init(timer, timeout, parameter, time, flag);
}

/**
 * @brief 此函数会将计时器从计时器管理中分离出来。
 *
 * @paramtimer是要分离的定时器
 *
 * @return 分离状态
 */
int8_t rt_timer_detach(rt_timer_t timer)
{
    // rt_base_t level;

    /* 参数检查 */
    // RT_ASSERT(timer != RT_NULL);
    // RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);
    // RT_ASSERT(rt_object_is_systemobject(&timer->parent));

    /* 禁用中断 */
    // level = rt_hw_interrupt_disable();

    _timer_remove(timer);
    /* 停止计时器 */
    timer->flag &= ~RT_TIMER_FLAG_ACTIVATED;

    /* 使能中断 */
    // rt_hw_interrupt_enable(level);

    // rt_object_detach(&(timer->parent));

    return 0;
}

/**
 * @brief 该函数将启动计时器
 *
 * @paramtimer要启动的计时器
 *
 * @返回操作状态，RT_EOK表示OK，-RT_ERROR表示错误
 */
int8_t rt_timer_start(rt_timer_t timer)
{
    unsigned int row_lvl;
    list_t *timer_list;
    // rt_base_t           level;
    // rt_bool_t           need_schedule;
    list_t *row_head[TIMER_SKIP_LIST_LEVEL];
    unsigned int tst_nr;
    static unsigned int random_nr;

    /* 参数检查 */
    // RT_ASSERT(timer != RT_NULL);
    // RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer);

    // need_schedule = RT_FALSE;

    /* 首先停止定时器 */
    // level = rt_hw_interrupt_disable();
    /* 从列表中删除计时器 */
    _timer_remove(timer);
    /* 改变定时器的状态 */
    timer->flag &= ~RT_TIMER_FLAG_ACTIVATED;

    timer->timeout_tick = tick_get() + timer->init_tick;

    timer_list = _timer_list;


    row_head[0] = &timer_list[0];
    for (row_lvl = 0; row_lvl < TIMER_SKIP_LIST_LEVEL; row_lvl++)
    {
        for (; row_head[row_lvl] != timer_list[row_lvl].prev; row_head[row_lvl] = row_head[row_lvl]->next)
        {
            struct rt_timer *t;
            list_t *p = row_head[row_lvl]->next;

            t = rt_list_entry(p, struct rt_timer, row[row_lvl]);

            if ((t->timeout_tick - timer->timeout_tick) == 0)
            {
                continue;
            }
            else if ((t->timeout_tick - timer->timeout_tick) < RT_TICK_MAX / 2)
            {
                break;
            }
        }
        if (row_lvl != TIMER_SKIP_LIST_LEVEL - 1)
            row_head[row_lvl + 1] = row_head[row_lvl] + 1;
    }

    random_nr++;
    tst_nr = random_nr;

    rt_list_insert_after(row_head[TIMER_SKIP_LIST_LEVEL - 1], &(timer->row[TIMER_SKIP_LIST_LEVEL - 1]));
    for (row_lvl = 2; row_lvl <= TIMER_SKIP_LIST_LEVEL; row_lvl++)
    {
        if (!(tst_nr & TIMER_SKIP_LIST_MASK))
            rt_list_insert_after(
                row_head[TIMER_SKIP_LIST_LEVEL - row_lvl], &(timer->row[TIMER_SKIP_LIST_LEVEL - row_lvl]));
        else
            break;

        tst_nr >>= (TIMER_SKIP_LIST_MASK + 1) >> 1;
    }

    timer->flag |= RT_TIMER_FLAG_ACTIVATED;
    /* 使能中断 */
    // rt_hw_interrupt_enable(level);


    return 0;
}


/**
 * @brief 该函数将停止计时器
 */
int8_t rt_timer_stop(rt_timer_t timer)
{
    if (!(timer->flag & RT_TIMER_FLAG_ACTIVATED))
        return -1;

    // RT_OBJECT_HOOK_CALL(rt_object_put_hook, (&(timer->parent)));

    // rt_hw_interrupt_disable();

    _timer_remove(timer);
    /* 改变状态 */
    timer->flag &= ~RT_TIMER_FLAG_ACTIVATED;

    /* 使能中断 */
    // rt_hw_interrupt_enable(level);

    return 0;
}

/**
 * @brief 这个函数将检查计时器列表，如果发生超时事件，相应的超时函数将被调用。
 * @note 该函数应在定时器中断中调用或者轮询。
 */
void rt_timer_check(void)
{
    struct rt_timer *t;
    uint32_t current_tick;
    // rt_base_t        level;
    list_t list;

    rt_list_init(&list);

    // RT_DEBUG_LOG(RT_DEBUG_TIMER, ("timer check enter\n"));

    current_tick = tick_get();

    /* 禁用中断 */
    // level = rt_hw_interrupt_disable();

    while (!rt_list_isempty(&_timer_list[TIMER_SKIP_LIST_LEVEL - 1]))
    {
        t = rt_list_entry(_timer_list[TIMER_SKIP_LIST_LEVEL - 1].next, struct rt_timer, row[TIMER_SKIP_LIST_LEVEL - 1]);

        /*
         *假设新的tick应小于当前tick持续时间的一半
         *勾选最大。
         */
        if ((current_tick - t->timeout_tick) < RT_TICK_MAX / 2)
        {
            // RT_OBJECT_HOOK_CALL(rt_timer_enter_hook, (t));

            /* 首先从计时器列表中删除计时器 */
            _timer_remove(t);
            if (!(t->flag & RT_TIMER_FLAG_PERIODIC))
            {
                t->flag &= ~RT_TIMER_FLAG_ACTIVATED;
            }
            /* 将计时器添加到临时列表  */
            rt_list_insert_after(&list, &(t->row[TIMER_SKIP_LIST_LEVEL - 1]));
            /* 调用超时功能 */
            t->timeout_func(t->parameter);

            /* 重新获得勾选 */
            current_tick = tick_get();

            if (rt_list_isempty(&list))
            {
                continue;
            }
            rt_list_remove(&(t->row[TIMER_SKIP_LIST_LEVEL - 1]));
            if ((t->flag & RT_TIMER_FLAG_PERIODIC) && (t->flag & RT_TIMER_FLAG_ACTIVATED))
            {
                /* 开始吧 */
                t->flag &= ~RT_TIMER_FLAG_ACTIVATED;
                rt_timer_start(t);
            }
        }
        else
            break;
    }

    /* 使能中断 */
    // rt_hw_interrupt_enable(level);

    // RT_DEBUG_LOG(RT_DEBUG_TIMER, ("timer check leave\n"));
}

/**
 * @brief 该函数将初始化系统计时器
 */
void rt_system_timer_init(void)
{
    uint32_t i;
    tick_init();

    for (i = 0; i < sizeof(_timer_list) / sizeof(_timer_list[0]); i++)
    {
        rt_list_init(_timer_list + i);
    }
}
