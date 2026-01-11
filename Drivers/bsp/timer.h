#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>
#include "rtthread.h"

#define TIMER_SKIP_LIST_LEVEL 1
#define TIMER_SKIP_LIST_MASK  3 /**< 1 or 3 */

#define RT_TIMER_FLAG_DEACTIVATED 0x0 /**< 计时器已停用 */
#define RT_TIMER_FLAG_ACTIVATED   0x1 /**< 计时器已激活 */
#define RT_TIMER_FLAG_ONE_SHOT    0x0 /**< 单次计时器 */
#define RT_TIMER_FLAG_PERIODIC    0x2 /**< 周期性定时器 */

#define RT_TICK_MAX 0xffffffff /**< 最大刻度数 */
#define rt_inline   static __inline

#define rt_container_of(ptr, type, member) (( type* )(( char* )(ptr) - ( unsigned long )(&(( type* )0)->member)))

#define rt_list_entry(node, type, member) rt_container_of(node, type, member)

struct list_node
{
    struct list_node* next;
    struct list_node* prev;
};
typedef struct list_node list_t;

struct object
{
    // char       name[8];
    uint8_t type;
    uint8_t flag;

    list_t list;
};
typedef struct object* object_t;


struct rt_timer
{
    // struct object parent;
    uint8_t flag;
    list_t row[TIMER_SKIP_LIST_LEVEL];

    void (*timeout_func)(void* parameter);
    void* parameter;

    uint32_t init_tick;
    uint32_t timeout_tick;
};
typedef struct rt_timer* rt_timer_t;

void rt_system_timer_init(void);
void rt_timer_init(rt_timer_t timer, void (*timeout)(void* parameter), void* parameter, uint32_t time, uint8_t flag);
int8_t rt_timer_start(rt_timer_t timer);
int8_t rt_timer_stop(rt_timer_t timer);
void rt_timer_check(void);
int8_t rt_timer_detach(rt_timer_t timer);

uint32_t tick_get(void);

/**
 * @brief initialize a list
 *
 * @param l list to be initialized
 */
rt_inline void rt_list_init(list_t* l)
{
    l->next = l->prev = l;
}

/**
 * @brief insert a node after a list
 *
 * @param l list to insert it
 * @param n new node to be inserted
 */
rt_inline void rt_list_insert_after(list_t* l, list_t* n)
{
    l->next->prev = n;
    n->next       = l->next;

    l->next = n;
    n->prev = l;
}

/**
 * @brief insert a node before a list
 *
 * @param n new node to be inserted
 * @param l list to insert it
 */
rt_inline void rt_list_insert_before(list_t* l, list_t* n)
{
    l->prev->next = n;
    n->prev       = l->prev;

    l->prev = n;
    n->next = l;
}

/**
 * @brief remove node from list.
 * @param n the node to remove from the list.
 */
rt_inline void rt_list_remove(list_t* n)
{
    n->next->prev = n->prev;
    n->prev->next = n->next;

    n->next = n->prev = n;
}

/**
 * @brief tests whether a list is empty
 * @param l the list to test.
 */
rt_inline int rt_list_isempty(const list_t* l)
{
    return l->next == l;
}

/**
 * @brief get the list length
 * @param l the list to get.
 */
rt_inline unsigned int rt_list_len(const list_t* l)
{
    unsigned int len = 0;
    const list_t* p  = l;
    while (p->next != l) {
        p = p->next;
        len++;
    }

    return len;
}
#endif //__TIMER_H__