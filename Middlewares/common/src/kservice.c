// #include <rtthread.h>

// #include <rthw.h>
#include "rtthread.h"
#include <stdint.h>
#include "rtm.h"
/* 使用精度 */

/* RT-Thread 中的全局错误号 */
// static volatile int __rt_errno;

#if defined(RT_USING_DEVICE) && defined(RT_USING_CONSOLE)
static rt_device_t _console_device = RT_NULL;
#endif

// RT_WEAK void rt_hw_us_delay(rt_uint32_t us)
// {
//     (void)us;
//     // RT_DEBUG_LOG(RT_DEBUG_DEVICE, ("rt_hw_us_delay() doesn't support for this board."
//     //     "Please consider implementing rt_hw_us_delay() in another file.\n"));
// }

#ifndef RT_KSERVICE_USING_STDLIB_MEMORY
/**
 *该函数将内存的内容设置为指定值。
 *
 * @param s 是源内存的地址，指向要填充的内存块。
 *
 * @param c 是要设置的值。值以 int 形式传递，但函数
 *填充内存块时使用无符号字符形式的值。
 *
 * @param count 要设置的字节数。
 *
 * @return 源内存的地址。
 */
RT_WEAK void *rt_memset(void *s, int c, uint32_t count)
{
#ifdef RT_KSERVICE_USING_TINY_SIZE
    char *xs = (char *)s;

    while (count--)
        *xs++ = c;

    return s;
#else
#define LBLOCKSIZE     (sizeof(long))
#define UNALIGNED(X)   ((long)X & (LBLOCKSIZE - 1))
#define TOO_SMALL(LEN) ((LEN) < LBLOCKSIZE)

    unsigned int i;
    char *m = (char *)s;
    unsigned long buffer;
    unsigned long *aligned_addr;
    unsigned int d = c & 0xff; /* 为了避免符号扩展，请将 C 复制到
                                un有符号变量。  */

    if (!TOO_SMALL(count) && !UNALIGNED(s))
    {
        /* 如果我们到目前为止，我们知道 count 很大并且 s 是字对齐的。 */
        aligned_addr = (unsigned long *)s;

        /* 将 d 存储到缓冲区中每个 char 大小的位置，以便
         *我们可以快速设置大块。
         */
        if (LBLOCKSIZE == 4)
        {
            buffer = (d << 8) | d;
            buffer |= (buffer << 16);
        }
        else
        {
            buffer = 0;
            for (i = 0; i < LBLOCKSIZE; i++)
                buffer = (buffer << 8) | d;
        }

        while (count >= LBLOCKSIZE * 4)
        {
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            count -= 4 * LBLOCKSIZE;
        }

        while (count >= LBLOCKSIZE)
        {
            *aligned_addr++ = buffer;
            count -= LBLOCKSIZE;
        }

        /* 通过字节循环获取余数。 */
        m = (char *)aligned_addr;
    }

    while (count--)
    {
        *m++ = (char)d;
    }

    return s;

#undef LBLOCKSIZE
#undef UNALIGNED
#undef TOO_SMALL
#endif /* 使用微小尺寸的 RT kservice */
}

/**
 *该函数将内存内容从源地址复制到目标地址。
 *
 * @param dst 是目标内存的地址，指向复制的内容。
 *
 * @param src 为源内存地址，指向要复制的数据源。
 *
 * @param count 是复制的长度。
 *
 * @return 目标内存地址
 */
RT_WEAK void *rt_memcpy(void *dst, const void *src, uint32_t count)
{
#ifdef RT_KSERVICE_USING_TINY_SIZE
    char *tmp = (char *)dst, *s = (char *)src;
    uint32_t len;

    if (tmp <= s || tmp > (s + count))
    {
        while (count--)
            *tmp++ = *s++;
    }
    else
    {
        for (len = count; len > 0; len--)
            tmp[len - 1] = s[len - 1];
    }

    return dst;
#else

#define UNALIGNED(X, Y) \
    (((long)X & (sizeof(long) - 1)) | ((long)Y & (sizeof(long) - 1)))
#define BIGBLOCKSIZE    (sizeof(long) << 2)
#define LITTLEBLOCKSIZE (sizeof(long))
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)

    char *dst_ptr = (char *)dst;
    char *src_ptr = (char *)src;
    long *aligned_dst;
    long *aligned_src;
    uint32_t len = count;

    /* 如果尺寸较小，或者SRC或DST未对齐，
    then 进入字节复制循环。  这种情况应该很少见。 */
    if (!TOO_SMALL(len) && !UNALIGNED(src_ptr, dst_ptr))
    {
        aligned_dst = (long *)dst_ptr;
        aligned_src = (long *)src_ptr;

        /* 如果可能的话，一次复制 4X 长的单词。 */
        while (len >= BIGBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            len -= BIGBLOCKSIZE;
        }

        /* 如果可能的话，一次复制一个长单词。 */
        while (len >= LITTLEBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            len -= LITTLEBLOCKSIZE;
        }

        /* 用字节复制器拾取任何残留物。 */
        dst_ptr = (char *)aligned_dst;
        src_ptr = (char *)aligned_src;
    }

    while (len--)
        *dst_ptr++ = *src_ptr++;

    return dst;
#undef UNALIGNED
#undef BIGBLOCKSIZE
#undef LITTLEBLOCKSIZE
#undef TOO_SMALL
#endif /* 使用微小尺寸的 RT kservice */
}

/**
 *该函数将内存内容从源地址移动到目标地址
 *地址。如果目标内存与源内存不重叠，
 *功能与memcpy()相同。
 *
 * @param dest 是目标内存的地址，指向复制的内容。
 *
 * @param src 为源内存地址，指向要复制的数据源。
 *
 * @param n 是复制的长度。
 *
 * @return 目标内存的地址。
 */
void *rt_memmove(void *dest, const void *src, uint32_t n)
{
    char *tmp = (char *)dest, *s = (char *)src;

    if (s < tmp && tmp < s + n)
    {
        tmp += n;
        s += n;

        while (n--)
            *(--tmp) = *(--s);
    }
    else
    {
        while (n--)
            *tmp++ = *s++;
    }

    return dest;
}

/**
 *该函数将比较两个内存区域。
 *
 * @param cs 是一个内存块。
 *
 * @param ct 是另一个内存块。
 *
 * @param count 是区域的大小。
 *
 * @return 比较结果：
 *如果结果 < 0，则 cs 小于 ct。
 *如果结果 > 0，则 cs 大于 ct。
 *如果结果 = 0，则 cs 等于 ct。
 */
int32_t rt_memcmp(const void *cs, const void *ct, uint32_t count)
{
    const unsigned char *su1, *su2;
    int res = 0;

    for (su1 = (const unsigned char *)cs, su2 = (const unsigned char *)ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;

    return res;
}
#endif /* 使用 stdlib 内存的 Rt kservice*/

#ifndef RT_KSERVICE_USING_STDLIB
/**
 *该函数将返回第一次出现的字符串，不带
 *终止符'\0'。
 *
 * @param s1 是源字符串。
 *
 * @param s2 是查找字符串。
 *
 * @return s1 中第一次出现 s2，如果没有找到则返回 RT_NULL。
 */
char *rt_strstr(const char *s1, const char *s2)
{
    int l1, l2;

    l2 = rt_strlen(s2);
    if (!l2)
        return (char *)s1;
    l1 = rt_strlen(s1);
    while (l1 >= l2)
    {
        l1--;
        if (!rt_memcmp(s1, s2, l2))
            return (char *)s1;
        s1++;
    }

    return RT_NULL;
}

/**
 *该函数将比较两个字符串，同时忽略大小写差异
 *
 * @param a 是要比较的字符串。
 *
 * @param b 是要比较的字符串。
 *
 * @return 比较结果：
 *如果结果 < 0，则 a 小于 a。
 *如果结果 > 0，则 a 大于 a。
 *如果结果 = 0，则 a 等于 a。
 */
int32_t rt_strcasecmp(const char *a, const char *b)
{
    int ca, cb;

    do
    {
        ca = *a++ & 0xff;
        cb = *b++ & 0xff;
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
    } while (ca == cb && ca != '\0');

    return ca - cb;
}

/**
 *该函数将复制不超过n个字节的字符串。
 *
 * @param dst 指向用于存储复制内容的地址。
 *
 * @param src 是要复制的字符串。
 *
 * @param n 是最大复制长度。
 *
 * @return 复制内容存放的地址。
 */
char *rt_strncpy(char *dst, const char *src, uint32_t n)
{
    if (n != 0)
    {
        char *d = dst;
        const char *s = src;

        do
        {
            if ((*d++ = *s++) == 0)
            {
                /* NUL 填充剩余的 n-1 个字节 */
                while (--n != 0)
                    *d++ = 0;
                break;
            }
        } while (--n != 0);
    }

    return (dst);
}

/**
 *该函数将复制字符串。
 *
 * @param dst 指向用于存储复制内容的地址。
 *
 * @param src 是要复制的字符串。
 *
 * @return 复制内容存放的地址。
 */
char *rt_strcpy(char *dst, const char *src)
{
    char *dest = dst;

    while (*src != '\0')
    {
        *dst = *src;
        dst++;
        src++;
    }

    *dst = '\0';
    return dest;
}


/**
 *该函数将比较两个指定最大长度的字符串。
 *
 * @param cs 是要比较的字符串。
 *
 * @param ct 是要比较的字符串。
 *
 * @param count 是最大比较长度。
 *
 * @return 比较结果：
 *如果结果 < 0，则 cs 小于 ct。
 *如果结果 > 0，则 cs 大于 ct。
 *如果结果 = 0，则 cs 等于 ct。
 */
int32_t rt_strncmp(const char *cs, const char *ct, uint32_t count)
{
    signed char __res = 0;

    while (count)
    {
        if ((__res = *cs - *ct++) != 0 || !*cs++)
            break;
        count--;
    }

    return __res;
}

/**
 *该函数将比较两个没有指定长度的字符串。
 *
 * @param cs 是要比较的字符串。
 *
 * @param ct 是要比较的字符串。
 *
 * @return 比较结果：
 *如果结果 < 0，则 cs 小于 ct。
 *如果结果 > 0，则 cs 大于 ct。
 *如果结果 = 0，则 cs 等于 ct。
 */
int32_t rt_strcmp(const char *cs, const char *ct)
{
    while (*cs && *cs == *ct)
    {
        cs++;
        ct++;
    }

    return (*cs - *ct);
}

/**
 *该函数将返回一个字符串的长度，该字符串终止将
 *空字符。
 *
 * @param s 是字符串
 *
 * @return 字符串的长度。
 */
uint32_t rt_strlen(const char *s)
{
    const char *sc;

    for (sc = s; *sc != '\0'; ++sc) /* 没有什么 */
        ;

    return sc - s;
}

#endif /* 使用 stdlib 的 Rt kservice */

/**
 *strnlen()函数返回字符串中的字符数
 *s 指向的字符串，不包括终止空字节（'\0'），
 *但最多 maxlen。  在此过程中，strnlen() 仅查看
 *s 指向的字符串中的前 maxlen 个字符，但从不
 *超出 s+maxlen。
 *
 * @param s 是字符串。
 *
 * @param maxlen 是最大大小。
 *
 * @return 字符串的长度。
 */
uint32_t rt_strnlen(const char *s, uint32_t maxlen)
{
    const char *sc;

    for (sc = s; *sc != '\0' && (uint32_t)(sc - s) < maxlen; ++sc) /* 没有什么 */
        ;

    return sc - s;
}

int rt_atoi(const char *s)
{
    int n = 0, sign = 1;

    /* 处理符号 */
    if (*s == '-')
    {
        sign = -1;
        s++;
    }
    else if (*s == '+')
        s++;

    /* 转换数字 */
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');

    return sign * n;
}
#ifdef RT_USING_HEAP
/**
 *该函数将复制一个字符串。
 *
 * @param s 是要复制的字符串。
 *
 * @return 副本的字符串地址。
 */
char *rt_strdup(const char *s)
{
    uint32_t len = rt_strlen(s) + 1;
    char *tmp = (char *)rt_malloc(len);

    if (!tmp)
        return RT_NULL;

    rt_memcpy(tmp, s, len);

    return tmp;
}
RTM_EXPORT(rt_strdup);
#endif /* Rt 使用堆 */

/**
 *该函数将显示rt-thread rtos的版本
 */
void rt_show_version(void)
{
    rt_kprintf("\n \\ | /\n");
    rt_kprintf("- RT -     Thread Operating System\n");
    rt_kprintf(" / | \\    build %s %s\n", __DATE__, __TIME__);
    rt_kprintf(" 2006 - 2022 Copyright by RT-Thread team\n");
}

/* 私有函数 */
// #define _ISDIGIT(c) ((unsigned)((c) - '0') < 10)

/**
 *该函数将复制一个字符串。
 *
 * @param n 是要复制的字符串。
 *
 * @param base 是支持除法指令值。
 *
 * @return 重复的字符串指针。
 */
#ifdef RT_KPRINTF_USING_LONGLONG
rt_inline int divide(long long *n, int base)
#else
rt_inline int divide(long *n, int base)
#endif /* 使用longlong rt kprintf */
{
    int res;

    /* 针对不支持除法指令的处理器进行了优化。 */
    if (base == 10)
    {
#ifdef RT_KPRINTF_USING_LONGLONG
        res = (int)(((unsigned long long)*n) % 10U);
        *n = (long long)(((unsigned long long)*n) / 10U);
#else
        res = (int)(((unsigned long)*n) % 10U);
        *n = (long)(((unsigned long)*n) / 10U);
#endif
    }
    else
    {
#ifdef RT_KPRINTF_USING_LONGLONG
        res = (int)(((unsigned long long)*n) % 16U);
        *n = (long long)(((unsigned long long)*n) / 16U);
#else
        res = (int)(((unsigned long)*n) % 16U);
        *n = (long)(((unsigned long)*n) / 16U);
#endif
    }

    return res;
}

// rt_inline int skip_atoi(const char **s)
// {
//     int i = 0;
//     while (_ISDIGIT(**s))
//         i = i * 10 + *((*s)++) - '0';

//     return i;
// }

#define ZEROPAD (1 << 0) /* 用零填充 */
#define SIGN    (1 << 1) /* 无符号/有符号长 */
#define PLUS    (1 << 2) /* 显示加号 */
#define SPACE   (1 << 3) /* 如果加上空格 */
#define LEFT    (1 << 4) /* 左对齐 */
#define SPECIAL (1 << 5) /* 0x */
#define LARGE   (1 << 6) /* 使用“ABCDEF”代替“abcdef” */

static char *print_number(char *buf,
                          char *end,
#ifdef RT_KPRINTF_USING_LONGLONG
                          long long num,
#else
                          long num,
#endif /* 使用longlong rt kprintf */
                          int base,
                          int s,
#ifdef RT_PRINTF_PRECISION
                          int precision,
#endif /* rt printf 精度 */
                          int type)
{
    char c, sign;
#ifdef RT_KPRINTF_USING_LONGLONG
    char tmp[32];
#else
    char tmp[16];
#endif /* 使用longlong rt kprintf */
    int precision_bak = precision;
    const char *digits;
    static const char small_digits[] = "0123456789abcdef";
    static const char large_digits[] = "0123456789ABCDEF";
    int i, size;

    size = s;

    digits = (type & LARGE) ? large_digits : small_digits;
    if (type & LEFT)
        type &= ~ZEROPAD;

    c = (type & ZEROPAD) ? '0' : ' ';

    /* 得到标志 */
    sign = 0;
    if (type & SIGN)
    {
        if (num < 0)
        {
            sign = '-';
            num = -num;
        }
        else if (type & PLUS)
            sign = '+';
        else if (type & SPACE)
            sign = ' ';
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    }
#endif /* RT printf 特殊 */

    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
    {
        while (num != 0)
            tmp[i++] = digits[divide(&num, base)];
    }

#ifdef RT_PRINTF_PRECISION
    if (i > precision)
        precision = i;
    size -= precision;
#else
    size -= i;
#endif /* rt printf 精度 */

    if (!(type & (ZEROPAD | LEFT)))
    {
        if ((sign) && (size > 0))
            size--;

        while (size-- > 0)
        {
            if (buf < end)
                *buf = ' ';
            ++buf;
        }
    }

    if (sign)
    {
        if (buf < end)
        {
            *buf = sign;
        }
        --size;
        ++buf;
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 8)
        {
            if (buf < end)
                *buf = '0';
            ++buf;
        }
        else if (base == 16)
        {
            if (buf < end)
                *buf = '0';
            ++buf;
            if (buf < end)
            {
                *buf = type & LARGE ? 'X' : 'x';
            }
            ++buf;
        }
    }
#endif /* RT printf 特殊 */

    /* 不向左对齐 */
    if (!(type & LEFT))
    {
        while (size-- > 0)
        {
            if (buf < end)
                *buf = c;
            ++buf;
        }
    }

#ifdef RT_PRINTF_PRECISION
    while (i < precision--)
    {
        if (buf < end)
            *buf = '0';
        ++buf;
    }
#endif /* rt printf 精度 */

    /* 将数字放入临时缓冲区 */
    while (i-- > 0 && (precision_bak != 0))
    {
        if (buf < end)
            *buf = tmp[i];
        ++buf;
    }

    while (size-- > 0)
    {
        if (buf < end)
            *buf = ' ';
        ++buf;
    }

    return buf;
}

/**
 *该函数将填充格式化字符串到缓冲区。
 *
 * @param buf 是保存格式化字符串的缓冲区。
 *
 * @param size 是缓冲区的大小。
 *
 * @param fmt 是格式参数。
 *
 * @param args 是可变参数的列表。
 *
 * @return 实际写入缓冲区的字符数。
 */
RT_WEAK int rt_vsnprintf(char *buf, uint32_t size, const char *fmt, va_list args)
{
#ifdef RT_KPRINTF_USING_LONGLONG
    unsigned long long num;
#else
    rt_uint32_t num;
#endif /* 使用longlong rt kprintf */
    int i, len;
    char *str, *end, c;
    const char *s;

    rt_uint8_t base;      /* 数的基数 */
    rt_uint8_t flags;     /* 打印数字的标志 */
    rt_uint8_t qualifier; /* 'h'、'l' 或 'L' 用于整数字段 */
    int32_t field_width;  /* 输出字段的宽度 */

#ifdef RT_PRINTF_PRECISION
    int precision; /* 分钟。整数的位数和字符串的最大位数 */
#endif             /* rt printf 精度 */

    str = buf;
    end = buf + size;

    /* 确保 end 始终 >= buf */
    if (end < buf)
    {
        end = ((char *)-1);
        size = end - buf;
    }

    for (; *fmt; ++fmt)
    {
        if (*fmt != '%')
        {
            if (str < end)
                *str = *fmt;
            ++str;
            continue;
        }

        /* 进程标志 */
        flags = 0;

        while (1)
        {
            /* 也跳过第一个“%” */
            ++fmt;
            if (*fmt == '-')
                flags |= LEFT;
            else if (*fmt == '+')
                flags |= PLUS;
            else if (*fmt == ' ')
                flags |= SPACE;
            else if (*fmt == '#')
                flags |= SPECIAL;
            else if (*fmt == '0')
                flags |= ZEROPAD;
            else
                break;
        }

        /* 获取字段宽度 */
        field_width = -1;
        if (_ISDIGIT(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*')
        {
            ++fmt;
            /* 这是下一个论点 */
            field_width = va_arg(args, int);
            if (field_width < 0)
            {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

#ifdef RT_PRINTF_PRECISION
        /* 得到精度 */
        precision = -1;
        if (*fmt == '.')
        {
            ++fmt;
            if (_ISDIGIT(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*')
            {
                ++fmt;
                /* 这是下一个论点 */
                precision = va_arg(args, int);
            }
            if (precision < 0) precision = 0;
        }
#endif /* RT_PRINTF_PRECISION */
        /* 获取转换限定符 */
        qualifier = 0;
#ifdef RT_KPRINTF_USING_LONGLONG
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
#else
        if (*fmt == 'h' || *fmt == 'l')
#endif /* 使用longlong rt kprintf */
        {
            qualifier = *fmt;
            ++fmt;
#ifdef RT_KPRINTF_USING_LONGLONG
            if (qualifier == 'l' && *fmt == 'l')
            {
                qualifier = 'L';
                ++fmt;
            }
#endif /* 使用longlong rt kprintf */
        }

        /* 默认基础 */
        base = 10;

        switch (*fmt)
        {
        case 'c':
            if (!(flags & LEFT))
            {
                while (--field_width > 0)
                {
                    if (str < end) *str = ' ';
                    ++str;
                }
            }

            /* 获取角色 */
            c = (rt_uint8_t)va_arg(args, int);
            if (str < end) *str = c;
            ++str;

            /* 放置宽度 */
            while (--field_width > 0)
            {
                if (str < end) *str = ' ';
                ++str;
            }
            continue;

        case 's':
            s = va_arg(args, char *);
            if (!s) s = "(RT_NULL)";

            for (len = 0; (len != field_width) && (s[len] != '\0'); len++);
#ifdef RT_PRINTF_PRECISION
            if (precision > 0 && len > precision) len = precision;
#endif /* rt printf 精度 */

            if (!(flags & LEFT))
            {
                while (len < field_width--)
                {
                    if (str < end) *str = ' ';
                    ++str;
                }
            }

            for (i = 0; i < len; ++i)
            {
                if (str < end) *str = *s;
                ++str;
                ++s;
            }

            while (len < field_width--)
            {
                if (str < end) *str = ' ';
                ++str;
            }
            continue;

        case 'p':
            if (field_width == -1)
            {
                field_width = sizeof(void *) << 1;
                flags |= ZEROPAD;
            }
#ifdef RT_PRINTF_PRECISION
            str = print_number(str, end,
                               (long)va_arg(args, void *),
                               16, field_width, precision, flags);
#else
            str = print_number(str, end,
                               (long)va_arg(args, void *),
                               16, field_width, flags);
#endif /* rt printf 精度 */
            continue;

        case '%':
            if (str < end) *str = '%';
            ++str;
            continue;

        /* 整数格式 -设置标志并“中断” */
        case 'o':
            base = 8;
            break;

        case 'X':
            flags |= LARGE;
        case 'x':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            break;

        default:
            if (str < end) *str = '%';
            ++str;

            if (*fmt)
            {
                if (str < end) *str = *fmt;
                ++str;
            }
            else
            {
                --fmt;
            }
            continue;
        }

#ifdef RT_KPRINTF_USING_LONGLONG
        if (qualifier == 'L')
            num = va_arg(args, long long);
        else if (qualifier == 'l')
#else
        if (qualifier == 'l')
#endif /* 使用longlong rt kprintf */
        {
            num = va_arg(args, rt_uint32_t);
            if (flags & SIGN) num = (int32_t)num;
        }
        else if (qualifier == 'h')
        {
            num = (rt_uint16_t)va_arg(args, int32_t);
            if (flags & SIGN) num = (rt_int16_t)num;
        }
        else
        {
            num = va_arg(args, rt_uint32_t);
            if (flags & SIGN) num = (int32_t)num;
        }
#ifdef RT_PRINTF_PRECISION
        str = print_number(str, end, num, base, field_width, precision, flags);
#else
        str = print_number(str, end, num, base, field_width, flags);
#endif /* rt printf 精度 */
    }

    if (size > 0)
    {
        if (str < end)
            *str = '\0';
        else
        {
            end[-1] = '\0';
        }
    }

    /* 尾随空字节不计入总数
    *++str;
    */
    return str - buf;
}
// RTM_EXPORT(rt_vsnprintf);

/**
 *该函数将填充格式化字符串到缓冲区。
 *
 * @param buf 是保存格式化字符串的缓冲区。
 *
 * @param size 是缓冲区的大小。
 *
 * @param fmt 是格式参数。
 *
 * @return 实际写入缓冲区的字符数。
 */
int rt_snprintf(char *buf, uint32_t size, const char *fmt, ...)
{
    int32_t n;
    va_list args;

    va_start(args, fmt);
    n = rt_vsnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}
RTM_EXPORT(rt_snprintf);

/**
 *该函数将填充格式化字符串到缓冲区。
 *
 * @param buf 是保存格式化字符串的缓冲区。
 *
 * @param format 是格式参数。
 *
 * @param arg_ptr 是变量参数列表。
 *
 * @return 实际写入缓冲区的字符数。
 */
int rt_vsprintf(char *buf, const char *format, va_list arg_ptr)
{
    return rt_vsnprintf(buf, (uint32_t)-1, format, arg_ptr);
}
// RTM_EXPORT(rt_vsprintf);

/**
 *该函数将格式化字符串填充到缓冲区
 *
 * @param buf 保存格式化字符串的缓冲区。
 *
 * @param format 是格式参数。
 *
 * @return 实际写入缓冲区的字符数。
 */
int rt_sprintf(char *buf, const char *format, ...)
{
    int32_t n;
    va_list arg_ptr;

    va_start(arg_ptr, format);
    n = rt_vsprintf(buf, format, arg_ptr);
    va_end(arg_ptr);

    return n;
}
RTM_EXPORT(rt_sprintf);

// #ifdef RT_USING_CONSOLE

#ifdef RT_USING_DEVICE
/**
 *该函数返回控制台中使用的设备。
 *
 * @return 返回控制台设备指针或 RT_NULL。
 */
rt_device_t rt_console_get_device(void)
{
    return _console_device;
}
RTM_EXPORT(rt_console_get_device);

/**
 *此函数将设备设置为控制台设备。
 *将设备设置为控制台后，rt_kprintf的所有输出都将是
 *重定向到这个新设备。
 *
 * @param name 是新控制台设备的名称。
 *
 * @成功则返回旧的控制台设备处理程序，失败则返回 RT_NULL。
 */
rt_device_t rt_console_set_device(const char *name)
{
    rt_device_t new_device, old_device;

    /* 保存旧设备 */
    old_device = _console_device;

    /* 找到新的控制台设备 */
    new_device = rt_device_find(name);

    /* 检查是否是同一设备 */
    if (new_device == old_device) return RT_NULL;

    if (new_device != RT_NULL)
    {
        if (_console_device != RT_NULL)
        {
            /* 关闭旧的控制台设备 */
            rt_device_close(_console_device);
        }

        /* 设置新的控制台设备 */
        rt_device_open(new_device, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_STREAM);
        _console_device = new_device;
    }

    return old_device;
}
RTM_EXPORT(rt_console_set_device);
#endif /* RT 使用设备 */

RT_WEAK void rt_hw_console_output(const char *str, uint16_t len)
{
    /* 空控制台输出 */
}
// RTM_EXPORT(rt_hw_console_output);

/**
 *此函数将在系统控制台上打印格式化字符串。
 *
 * @param fmt 是格式参数。
 *
 * @return 实际写入缓冲区的字符数。
 */
RT_WEAK int rt_kprintf(const char *fmt, ...)
{
    va_list args;
    uint32_t length;
    RT_SECTION(".shared_ram") static char rt_log_buf[RT_CONSOLEBUF_SIZE];

    va_start(args, fmt);
    /* vsnprintf 的返回值是字节数
     *如果缓冲区的大小足够，则写入缓冲区
     *大，不包括终止空字节。如果输出字符串
     *会大于rt_log_buf，我们必须调整输出
     * 长度。 */
    length = rt_vsnprintf(rt_log_buf, sizeof(rt_log_buf) - 1, fmt, args);
    if (length > RT_CONSOLEBUF_SIZE - 1)
        length = RT_CONSOLEBUF_SIZE - 1;
#ifdef RT_USING_DEVICE
    if (_console_device == RT_NULL)
    {
        rt_hw_console_output(rt_log_buf);
    }
    else
    {
        rt_device_write(_console_device, 0, rt_log_buf, length);
    }
#else
    rt_hw_console_output(rt_log_buf, length);
#endif /* RT 使用设备 */
    va_end(args);

    return length;
}
RTM_EXPORT(rt_kprintf);
// #endif /* 使用控制台进行 RT */

#if defined(RT_USING_HEAP) && !defined(RT_USING_USERHEAP)
#ifdef RT_USING_HOOK
static void (*rt_malloc_hook)(void *ptr, uint32_t size);
static void (*rt_free_hook)(void *ptr);

/**
 * @addtogroup 钩子
 */

/**@{*/

/**
 * @brief 该函数会设置一个钩子函数，当内存不足时会调用该函数
 *块是从堆内存中分配的。
 *
 * @param hook 钩子函数。
 */
void rt_malloc_sethook(void (*hook)(void *ptr, uint32_t size))
{
    rt_malloc_hook = hook;
}

/**
 * @brief 该函数会设置一个钩子函数，当内存不足时会调用该函数
 *块被释放到堆内存。
 *
 * @param hook 钩子函数
 */
void rt_free_sethook(void (*hook)(void *ptr))
{
    rt_free_hook = hook;
}

/**@}*/

#endif /* Rt 使用钩子 */

#if defined(RT_USING_HEAP_ISR)
#elif defined(RT_USING_MUTEX)
static struct rt_mutex _lock;
#endif

rt_inline void _heap_lock_init(void)
{
#if defined(RT_USING_HEAP_ISR)
#elif defined(RT_USING_MUTEX)
    rt_mutex_init(&_lock, "heap", RT_IPC_FLAG_PRIO);
#endif
}

rt_inline rt_base_t _heap_lock(void)
{
#if defined(RT_USING_HEAP_ISR)
    return rt_hw_interrupt_disable();
#elif defined(RT_USING_MUTEX)
    if (rt_thread_self())
        return rt_mutex_take(&_lock, RT_WAITING_FOREVER);
    else
        return RT_EOK;
#else
    rt_enter_critical();
    return RT_EOK;
#endif
}

rt_inline void _heap_unlock(rt_base_t level)
{
#if defined(RT_USING_HEAP_ISR)
    rt_hw_interrupt_enable(level);
#elif defined(RT_USING_MUTEX)
    RT_ASSERT(level == RT_EOK);
    if (rt_thread_self())
        rt_mutex_release(&_lock);
#else
    rt_exit_critical();
#endif
}

#if defined(RT_USING_SMALL_MEM_AS_HEAP)
static rt_smem_t system_heap;
rt_inline void _smem_info(uint32_t *total,
                          uint32_t *used, uint32_t *max_used)
{
    if (total)
        *total = system_heap->total;
    if (used)
        *used = system_heap->used;
    if (max_used)
        *max_used = system_heap->max;
}
#define _MEM_INIT(_name, _start, _size) \
    system_heap = rt_smem_init(_name, _start, _size)
#define _MEM_MALLOC(_size) \
    rt_smem_alloc(system_heap, _size)
#define _MEM_REALLOC(_ptr, _newsize) \
    rt_smem_realloc(system_heap, _ptr, _newsize)
#define _MEM_FREE(_ptr) \
    rt_smem_free(_ptr)
#define _MEM_INFO(_total, _used, _max) \
    _smem_info(_total, _used, _max)
#elif defined(RT_USING_MEMHEAP_AS_HEAP)
static struct rt_memheap system_heap;
void *_memheap_alloc(struct rt_memheap *heap, uint32_t size);
void _memheap_free(void *rmem);
void *_memheap_realloc(struct rt_memheap *heap, void *rmem, uint32_t newsize);
#define _MEM_INIT(_name, _start, _size) \
    rt_memheap_init(&system_heap, _name, _start, _size)
#define _MEM_MALLOC(_size) \
    _memheap_alloc(&system_heap, _size)
#define _MEM_REALLOC(_ptr, _newsize) \
    _memheap_realloc(&system_heap, _ptr, _newsize)
#define _MEM_FREE(_ptr) \
    _memheap_free(_ptr)
#define _MEM_INFO(_total, _used, _max) \
    rt_memheap_info(&system_heap, _total, _used, _max)
#elif defined(RT_USING_SLAB_AS_HEAP)
static rt_slab_t system_heap;
rt_inline void _slab_info(uint32_t *total,
                          uint32_t *used, uint32_t *max_used)
{
    if (total)
        *total = system_heap->total;
    if (used)
        *used = system_heap->used;
    if (max_used)
        *max_used = system_heap->max;
}
#define _MEM_INIT(_name, _start, _size) \
    system_heap = rt_slab_init(_name, _start, _size)
#define _MEM_MALLOC(_size) \
    rt_slab_alloc(system_heap, _size)
#define _MEM_REALLOC(_ptr, _newsize) \
    rt_slab_realloc(system_heap, _ptr, _newsize)
#define _MEM_FREE(_ptr) \
    rt_slab_free(system_heap, _ptr)
#define _MEM_INFO _slab_info
#else
#define _MEM_INIT(...)
#define _MEM_MALLOC(...)  RT_NULL
#define _MEM_REALLOC(...) RT_NULL
#define _MEM_FREE(...)
#define _MEM_INFO(...)
#endif

/**
 * @brief 该函数将初始化系统堆。
 *
 * @param begin_addr 系统页的起始地址。
 *
 * @param end_addr 系统页的结束地址。
 */
RT_WEAK void rt_system_heap_init(void *begin_addr, void *end_addr)
{
    uint32_t begin_align = RT_ALIGN((uint32_t)begin_addr, RT_ALIGN_SIZE);
    uint32_t end_align = RT_ALIGN_DOWN((uint32_t)end_addr, RT_ALIGN_SIZE);

    RT_ASSERT(end_align > begin_align);

    /* 初始化系统内存堆 */
    _MEM_INIT("heap", begin_addr, end_align - begin_align);
    /* 初始化多线程争用锁 */
    _heap_lock_init();
}

/**
 * @brief 分配最小“size”字节的内存块。
 *
 * @param size 是请求块的最小大小（以字节为单位）。
 *
 * @return 指向已分配内存的指针，如果未找到空闲内存，则返回 RT_NULL。
 */
RT_WEAK void *rt_malloc(uint32_t size)
{
    rt_base_t level;
    void *ptr;

    /* 进入临界区 */
    level = _heap_lock();
    /* 从系统堆中分配内存块 */
    ptr = _MEM_MALLOC(size);
    /* 退出临界区 */
    _heap_unlock(level);
    /* call 'rt_malloc' hook */
    RT_OBJECT_HOOK_CALL(rt_malloc_hook, (ptr, size));
    return ptr;
}
RTM_EXPORT(rt_malloc);

/**
 * @brief此函数将更改先前分配的内存块的大小。
 *
 * @param rmem 是指向 rt_malloc 分配的内存的指针。
 *
 * @param newsize 是所需的新大小。
 *
 * @return改变后的内存块地址。
 */
RT_WEAK void *rt_realloc(void *rmem, uint32_t newsize)
{
    rt_base_t level;
    void *nptr;

    /* 进入临界区 */
    level = _heap_lock();
    /* 更改先前分配的内存块的大小 */
    nptr = _MEM_REALLOC(rmem, newsize);
    /* 退出临界区 */
    _heap_unlock(level);
    return nptr;
}
RTM_EXPORT(rt_realloc);

/**
 * @brief 该函数将为 count 对象连续分配足够的空间
 *每个都是 size 字节的内存，并返回指向分配的内存的指针
 *        记忆。
 *
 * @note 分配的内存填充了值为零的字节。
 *
 * @param count 是要分配的对象数量。
 *
 * @param size 是要分配的一个对象的大小。
 *
 * @return 指向已分配内存的指针/如果出现错误则返回 RT_NULL 指针。
 */
RT_WEAK void *rt_calloc(uint32_t count, uint32_t size)
{
    void *p;

    /* 分配大小为“size”的“count”个对象 */
    p = rt_malloc(count * size);
    /* 将记忆归零 */
    if (p)
    {
        rt_memset(p, 0, count * size);
    }
    return p;
}
RTM_EXPORT(rt_calloc);

/**
 * @brief该函数将释放之前分配的内存块
 *rt_malloc。释放的内存块被带回系统堆。
 *
 * @param rmem 将被释放的内存地址。
 */
RT_WEAK void rt_free(void *rmem)
{
    rt_base_t level;

    /* 调用 'rt_free' 钩子 */
    RT_OBJECT_HOOK_CALL(rt_free_hook, (rmem));
    /* 空检查 */
    if (rmem == RT_NULL) return;
    /* 进入临界区 */
    level = _heap_lock();
    _MEM_FREE(rmem);
    /* 退出临界区 */
    _heap_unlock(level);
}
RTM_EXPORT(rt_free);

/**
* @brief 该函数将计算总内存、已用内存和
*最大使用内存。
*
* @param Total 是一个获取内存总大小的指针。
*
* @paramused是一个指针，用于获取所用内存的大小。
*
* @param max_used 是获取最大使用内存的指针。
*/
RT_WEAK void rt_memory_info(uint32_t *total,
                            uint32_t *used,
                            uint32_t *max_used)
{
    rt_base_t level;

    /* 进入临界区 */
    level = _heap_lock();
    _MEM_INFO(total, used, max_used);
    /* 退出临界区 */
    _heap_unlock(level);
}
RTM_EXPORT(rt_memory_info);

#if defined(RT_USING_SLAB) && defined(RT_USING_SLAB_AS_HEAP)
void *rt_page_alloc(uint32_t npages)
{
    rt_base_t level;
    void *ptr;

    /* 进入临界区 */
    level = _heap_lock();
    /* 分配页 */
    ptr = rt_slab_page_alloc(system_heap, npages);
    /* 退出临界区 */
    _heap_unlock(level);
    return ptr;
}

void rt_page_free(void *addr, uint32_t npages)
{
    rt_base_t level;

    /* 进入临界区 */
    level = _heap_lock();
    /* 免费页面 */
    rt_slab_page_free(system_heap, addr, npages);
    /* 退出临界区 */
    _heap_unlock(level);
}
#endif

/**
 *该函数释放内存块，该内存块由
 *rt_malloc_align函数与地址对齐。
 *
 * @param ptr 是内存块指针。
 */
RT_WEAK void rt_free_align(void *ptr)
{
    void *real_ptr;

    /* 空检查 */
    if (ptr == RT_NULL) return;
    real_ptr = (void *)*(uint32_t *)((uint32_t)ptr - sizeof(void *));
    rt_free(real_ptr);
}
RTM_EXPORT(rt_free_align);
#endif /* Rt 使用堆 */

#ifndef RT_USING_CPU_FFS
#ifdef RT_USING_TINY_FFS
const rt_uint8_t __lowest_bit_bitmap[] =
    {
        /*  0 -7  */ 0, 1, 2, 27, 3, 24, 28, 32,
        /*  8 -15 */ 4, 17, 25, 31, 29, 12, 32, 14,
        /* 16 -23 */ 5, 8, 18, 32, 26, 23, 32, 16,
        /* 24 -31 */ 30, 11, 13, 7, 32, 22, 15, 10,
        /* 32 -36 */ 6, 21, 9, 20, 19};

/**
 *该函数查找第一个位集（从最低有效位开始）
 *值并返回该位的索引。
 *
 *位从 1（最低有效位）开始编号。  返回值为
 *这些函数中的任何一个为零都意味着参数为零。
 *
 * @return 返回第一个位集的索引。如果值为 0，则此函数
 *应返回 0。
 */
int __rt_ffs(int value)
{
    return __lowest_bit_bitmap[(rt_uint32_t)(value & (value - 1) ^ value) % 37];
}
#else
// const rt_uint8_t __lowest_bit_bitmap[] =
//     {
//         /* 00 */ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 10 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 20 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 30 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 40 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 50 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 60 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 70 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 80 */ 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 90 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* A0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* B0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* 二氧化碳 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* D0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* E0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
//         /* F0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};

/**
 *该函数查找第一个位集（从最低有效位开始）
 *值并返回该位的索引。
 *
 *位从 1（最低有效位）开始编号。  返回值为
 *这些函数中的任何一个为零都意味着参数为零。
 *
 * @return 返回第一个位集的索引。如果值为 0，则此函数
 *应返回 0。
 */
// int __rt_ffs(int value)
// {
//     if (value == 0) return 0;

//     if (value & 0xff)
//         return __lowest_bit_bitmap[value & 0xff] + 1;

//     if (value & 0xff00)
//         return __lowest_bit_bitmap[(value & 0xff00) >> 8] + 9;

//     if (value & 0xff0000)
//         return __lowest_bit_bitmap[(value & 0xff0000) >> 16] + 17;

//     return __lowest_bit_bitmap[(value & 0xff000000) >> 24] + 25;
// }
#endif /* Rt using tiny ffs */
#endif /* RT 使用 cpu ffs */

#ifndef __on_rt_assert_hook
#define __on_rt_assert_hook(ex, func, line) __ON_HOOK_ARGS(rt_assert_hook, (ex, func, line))
#endif

#ifdef RT_DEBUG
/* RT_ASSERT(EX) 的钩子 */

void (*rt_assert_hook)(const char *ex, const char *func, uint32_t line);

/**
 *该函数将为 RT_ASSERT(EX) 设置一个钩子函数。当表达式为 false 时它将运行。
 *
 * @param hook 是钩子函数。
 */
void rt_assert_set_hook(void (*hook)(const char *ex, const char *func, uint32_t line))
{
    rt_assert_hook = hook;
}

/**
 *RT_ASSERT 函数。
 *
 * @param ex_string 是断言条件字符串。
 *
 * @param func 是断言时的函数名。
 *
 * @param line 是断言时的文件行号。
 */
// void rt_assert_handler(const char *ex_string, const char *func, uint32_t line)
// {
//     volatile char dummy = 0;

//     if (rt_assert_hook == RT_NULL)
//     {
// #ifdef RT_USING_MODULE
//         // if (dlmodule_self())
//         // {
//         //     /* 关闭断言模块 */
//         //     dlmodule_exit(-1);
//         // }
//         // else
// #endif /*RT 使用模块*/
//         {
//             rt_kprintf("(%s) assertion failed at function:%s, line number:%d \n", ex_string, func, line);
//             while (dummy == 0);
//         }
//     }
//     else
//     {
//         rt_assert_hook(ex_string, func, line);
//     }
// }
// RTM_EXPORT(rt_assert_handler);
#endif /* 实时调试 */

void rt_assert_handler(const char *ex_string, const char *func, uint32_t line)
{
    volatile char dummy = 0;
    {
        rt_kprintf("(%s) assertion failed at function:%s, line number:%d \n", ex_string, func, line);
        while (dummy == 0);
    }
}

void LOG_HEX(uint32_t offset, uint8_t* buf, uint32_t size)
{
    // rt_kprintf( "%*s", 12, "" );
    rt_kprintf("0x%08lx: ", offset);
    for (uint16_t i = 0; i < 16; i++) {
        rt_kprintf("%02d ", i);
    }
    rt_kprintf("\n");
    // 计算对齐后的起始地址
    long aligned_offset = offset & ~0x0F;
    // 计算数据结束地址
    long end_addr = offset + size;

    // 遍历每个对齐的块
    for (long addr = aligned_offset; addr < end_addr; addr += 16) {
        // 打印当前块的地址
        rt_kprintf("0x%08lx: ", addr);

        // 处理十六进制部分
        for (int i = 0; i < 16; ++i) {
            long current_addr = addr + i;
            if (current_addr >= offset && current_addr < end_addr) {
                // 在有效范围内，打印字节
                rt_kprintf("%02x ", buf[current_addr - offset]);
            }
            else {
                // 超出范围，用空格填充
                rt_kprintf("   ");
            }
        }
        // 分隔符
        rt_kprintf(" ");

        // 处理ASCII部分
        for (int i = 0; i < 16; ++i) {
            long current_addr = addr + i;
            if (current_addr >= offset && current_addr < end_addr) {
                uint8_t c = buf[current_addr - offset];
                // 可打印字符直接输出，否则用.代替
                // putchar(isprint(c) ? c : '.');
                rt_kprintf("%c", (c >= 32 && c <= 126) ? c : '.');
            }
            else {
                // 超出范围的部分用.填充
                rt_kprintf(".");
            }
        }
        rt_kprintf("\n");
    }
}
// RTM_EXPORT(LOG_HEX);
/**@}*/
