#ifndef OS_MALLOC_H_
#define OS_MALLOC_H_

#include <stddef.h>
#include <stdlib.h>

static inline void *os_malloc(size_t size)
{
    return malloc(size);
}

static inline void os_free(void *ptr)
{
    free(ptr);
}

#endif /* OS_MALLOC_H_ */
