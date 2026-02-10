#ifndef __RT_SERVICE_H__
#define __RT_SERVICE_H__

#ifdef __cplusplus
extern "C" {
#endif
/******************config***********/
#define RT_CONSOLEBUF_SIZE 256
#define RT_PRINTF_PRECISION
#define PKG_USING_SEGGER_RTT
/************end of config***********/

#include "rtcompiler.h"

typedef signed char rt_int8_t;
typedef signed short rt_int16_t;
typedef signed int rt_int32_t;
typedef unsigned char rt_uint8_t;
typedef unsigned short rt_uint16_t;
typedef unsigned int rt_uint32_t;
typedef unsigned int rt_size_t;
typedef int rt_bool_t;

/* board init routines will be called in board_init() function */
#define INIT_BOARD_EXPORT(fn) INIT_EXPORT(fn, "1")

/* init cpu, memory, interrupt-controller, bus... */
#define INIT_CORE_EXPORT(fn) INIT_EXPORT(fn, "1.0")
/* init sys-timer, clk, pinctrl... */
#define INIT_SUBSYS_EXPORT(fn) INIT_EXPORT(fn, "1.1")
/* init platform, user code... */
#define INIT_PLATFORM_EXPORT(fn) INIT_EXPORT(fn, "1.2")

/* pre/device/component/env/app init routines will be called in init_thread */
/* components pre-initialization (pure software initialization) */
#define INIT_PREV_EXPORT(fn) INIT_EXPORT(fn, "2")
/* device initialization */
#define INIT_DEVICE_EXPORT(fn) INIT_EXPORT(fn, "3")
/* components initialization (dfs, lwip, ...) */
#define INIT_COMPONENT_EXPORT(fn) INIT_EXPORT(fn, "4")
/* environment initialization (mount disk, ...) */
#define INIT_ENV_EXPORT(fn) INIT_EXPORT(fn, "5")
/* application initialization (rtgui application etc ...) */
#define INIT_APP_EXPORT(fn) INIT_EXPORT(fn, "6")

/* init after mount fs */
#define INIT_FS_EXPORT(fn) INIT_EXPORT(fn, "6.0")
/* init in secondary_cpu_c_start */
#define INIT_SECONDARY_CPU_EXPORT(fn) INIT_EXPORT(fn, "7")


#define RT_NULL 0

#define RT_TRUE  1
#define RT_FALSE 0

#define RT_EOK      0
#define RT_ERROR    1
#define RT_ETIMEOUT 2
#define RT_EFULL    3
#define RT_EEMPTY   4
#define RT_ENOMEM   5
#define RT_ENOSYS   6
#define RT_EBUSY    7
#define RT_EIO      8
#define RT_EINTR    9
#define RT_EINVAL   10

#include <stdarg.h>
#include <stdint.h>

void* rt_memset(void* s, int c, uint32_t count);
void* rt_memcpy(void* dst, const void* src, uint32_t count);
void* rt_memmove(void* dest, const void* src, uint32_t n);
int32_t rt_memcmp(const void* cs, const void* ct, uint32_t count);
char* rt_strstr(const char* s1, const char* s2);
int32_t rt_strcasecmp(const char* a, const char* b);
char* rt_strncpy(char* dst, const char* src, uint32_t n);
char* rt_strcpy(char* dst, const char* src);
int32_t rt_strncmp(const char* cs, const char* ct, uint32_t count);
int32_t rt_strcmp(const char* cs, const char* ct);
uint32_t rt_strlen(const char* s);
uint32_t rt_strnlen(const char* s, uint32_t maxlen);
int rt_atoi(const char* s);
void rt_show_version(void);

#define _ISDIGIT(c) (( unsigned )((c) - '0') < 10)
// rt_inline int skip_atoi(const char **s);
rt_inline int skip_atoi(const char** s)
{
    int i = 0;
    while (_ISDIGIT(**s)) i = i * 10 + *((*s)++) - '0';

    return i;
}
int rt_snprintf(char* buf, uint32_t size, const char* fmt, ...);
int rt_sprintf(char* buf, const char* format, ...);
void rt_assert_handler(const char* ex_string, const char* func, uint32_t line);
int rt_kprintf(const char* fmt, ...);
void LOG_HEX(uint32_t offset, uint8_t* buf, uint32_t size);

rt_inline uint16_t swap_2bytes(uint16_t value)
{
    return (value << 8) | (value >> 8);
}

rt_inline uint32_t swap_4bytes(uint32_t value)
{
    return (value << 24) | ((value << 8) & 0x00FF0000) | ((value >> 8) & 0x0000FF00) | (value >> 24);
}
#ifdef __cplusplus
}
#endif

#endif  //__RT_SERVICE_H__
