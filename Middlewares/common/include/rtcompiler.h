#ifndef __RT_COMPILER_H__
#define __RT_COMPILER_H__

/* 编译器相关定义 */
#if defined(__ARMCC_VERSION) /* ARM编译器 */
#define RT_SECTION(x) __attribute__((section(x)))
#define RT_USED       __attribute__((used))
#define ALIGN(n)      __attribute__((aligned(n)))
#define RT_WEAK       __attribute__((weak))
#define rt_inline     static __inline
/* 模块编译 */
#ifdef RT_USING_MODULE
#define RTT_API __declspec(dllimport)
#else
#define RTT_API __declspec(dllexport)
#endif                             /* RT 使用模块 */
#elif defined(__IAR_SYSTEMS_ICC__) /* 对于 IAR 编译器 */
#define RT_SECTION(x) @x
#define RT_USED       __root
#define PRAGMA(x)     _Pragma(#x)
#define ALIGN(n)      PRAGMA(data_alignment = n)
#define RT_WEAK       __weak
#define rt_inline     static inline
#define RTT_API
#elif defined(__GNUC__) /* GNU GCC 编译器 */
#ifndef RT_USING_LIBC
/* GNU GCC 的版本必须大于 4.x */
typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, l)   __builtin_va_arg(v, l)
#endif /* RT 使用 libc */
#define RT_SECTION(x) __attribute__((section(x)))
#define RT_USED       __attribute__((used))
#define ALIGN(n)      __attribute__((aligned(n)))
#define RT_WEAK       __attribute__((weak))
#define rt_inline     static __inline
#define RTT_API
#elif defined(__ADSPBLACKFIN__) /* VisualDSP++编译器 */
#define RT_SECTION(x) __attribute__((section(x)))
#define RT_USED       __attribute__((used))
#define ALIGN(n)      __attribute__((aligned(n)))
#define RT_WEAK       __attribute__((weak))
#define rt_inline     static inline
#define RTT_API
#elif defined(_MSC_VER)
#define RT_SECTION(x)
#define RT_USED
#define ALIGN(n) __declspec(align(n))
#define RT_WEAK
#define rt_inline static __inline
#define RTT_API
#elif defined(__TI_COMPILER_VERSION__)
/* TI编译器设置节的方式与其他编译器不同（至少
 *GCC 和 MDK）编译器。有关详细信息，请参阅 ARM 优化 C/C++ 编译器 5.9.3
 * 细节。 */
#define RT_SECTION(x)
#define RT_USED
#define PRAGMA(x) _Pragma(#x)
#define ALIGN(n)
#define RT_WEAK
#define rt_inline static inline
#define RTT_API
#elif defined(__TASKING__)
#define RT_SECTION(x) __attribute__((section(x)))
#define RT_USED       __attribute__((used, protect))
#define PRAGMA(x)     _Pragma(#x)
#define ALIGN(n)      __attribute__((__align(n)))
#define RT_WEAK       __attribute__((weak))
#define rt_inline     static inline
#define RTT_API
#else
#error not supported tool chain
#endif /* Armcc version */

/* initialization export */
typedef void (*init_fn_t)(void);
#define INIT_EXPORT(fn, level) RT_USED const init_fn_t __rt_init_##fn RT_SECTION(".rti_fn." level) = fn

#endif /* __RT_COMPILER_H__ */