/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2012-09-30     Bernard      first version.
 * 2013-05-08     Grissiom     reimplement
 * 2016-08-18     heyuanjie    add interface
 * 2021-07-20     arminker     fix write_index bug in function rt_ringbuffer_put_force
 * 2021-08-14     Jackistang   add comments for function interface.
 */

// #include <rtthread.h>
// #include <rtdevice.h>
#include "ringbuffer.h"

// #include <string.h>
// #define rt_memcpy memcpy
#include "rtthread.h"

#ifndef NULL
#define NULL 0U
#endif

#define RT_ALIGN_SIZE 4
#define ALIGN_DOWN(size, align) ((size) & ~((align) - 1))

rt_inline enum rt_ringbuffer_state rt_ringbuffer_status(struct rt_ringbuffer* rb)
{
    if (rb->read_index == rb->write_index) {
        if (rb->read_mirror == rb->write_mirror)
            return RT_RINGBUFFER_EMPTY;
        else
            return RT_RINGBUFFER_FULL;
    }
    return RT_RINGBUFFER_HALFFULL;
}

/**
 * @brief Initialize the ring buffer object.
 *
 * @param rb        A pointer to the ring buffer object.
 * @param pool      A pointer to the buffer.
 * @param size      The size of the buffer in bytes.
 */
void rt_ringbuffer_init(struct rt_ringbuffer* rb, uint8_t* pool, int16_t size)
{
    RT_ASSERT(rb != NULL);
    RT_ASSERT(size > 0);

    /* initialize read and write index */
    rb->read_mirror = rb->read_index = 0;
    rb->write_mirror = rb->write_index = 0;

    /* set buffer pool and size */
    rb->buffer_ptr  = pool;
    rb->buffer_size = ALIGN_DOWN(size, RT_ALIGN_SIZE);
}
RTM_EXPORT(rt_ringbuffer_init);

/**
 * @brief Put a block of data into the ring buffer. If the capacity of ring buffer is insufficient, it will discard
 * out-of-range data.
 *
 * @param rb            A pointer to the ring buffer object.
 * @param ptr           A pointer to the data buffer.
 * @param length        The size of data in bytes.
 *
 * @return Return the data size we put into the ring buffer.
 */
uint32_t rt_ringbuffer_put(struct rt_ringbuffer* rb, const uint8_t* ptr, uint16_t length)
{
    uint16_t size;

    RT_ASSERT(rb != NULL);

    /* whether has enough space */
    size = rt_ringbuffer_space_len(rb);

    /* no space */
    if (size == 0)
        return 0;

    /* drop some data */
    if (size < length)
        length = size;

    if (rb->buffer_size - rb->write_index > length) {
        /* read_index - write_index = empty space */
        rt_memcpy(&rb->buffer_ptr[rb->write_index], ptr, length);
        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->write_index += length;
        return length;
    }

    rt_memcpy(&rb->buffer_ptr[rb->write_index], &ptr[0], rb->buffer_size - rb->write_index);
    rt_memcpy(
        &rb->buffer_ptr[0], &ptr[rb->buffer_size - rb->write_index], length - (rb->buffer_size - rb->write_index));

    /* we are going into the other side of the mirror */
    rb->write_mirror = ~rb->write_mirror;
    rb->write_index  = length - (rb->buffer_size - rb->write_index);

    return length;
}
RTM_EXPORT(rt_ringbuffer_put);

/**
 * @brief Put a block of data into the ring buffer. If the capacity of ring buffer is insufficient, it will overwrite
 * the existing data in the ring buffer.
 *
 * @param rb            A pointer to the ring buffer object.
 * @param ptr           A pointer to the data buffer.
 * @param length        The size of data in bytes.
 *
 * @return Return the data size we put into the ring buffer.
 */
uint32_t rt_ringbuffer_put_force(struct rt_ringbuffer* rb, const uint8_t* ptr, uint16_t length)
{
    uint16_t space_length;

    RT_ASSERT(rb != NULL);

    space_length = rt_ringbuffer_space_len(rb);

    if (length > rb->buffer_size) {
        ptr    = &ptr[length - rb->buffer_size];
        length = rb->buffer_size;
    }

    if (rb->buffer_size - rb->write_index > length) {
        /* read_index - write_index = empty space */
        rt_memcpy(&rb->buffer_ptr[rb->write_index], ptr, length);
        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->write_index += length;

        if (length > space_length)
            rb->read_index = rb->write_index;

        return length;
    }

    rt_memcpy(&rb->buffer_ptr[rb->write_index], &ptr[0], rb->buffer_size - rb->write_index);
    rt_memcpy(
        &rb->buffer_ptr[0], &ptr[rb->buffer_size - rb->write_index], length - (rb->buffer_size - rb->write_index));

    /* we are going into the other side of the mirror */
    rb->write_mirror = ~rb->write_mirror;
    rb->write_index  = length - (rb->buffer_size - rb->write_index);

    if (length > space_length) {
        if (rb->write_index <= rb->read_index)
            rb->read_mirror = ~rb->read_mirror;
        rb->read_index = rb->write_index;
    }

    return length;
}
RTM_EXPORT(rt_ringbuffer_put_force);

/**
 * @brief Get data from the ring buffer.
 *
 * @param rb            A pointer to the ring buffer.
 * @param ptr           A pointer to the data buffer.
 * @param length        The size of the data we want to read from the ring buffer.
 *
 * @return Return the data size we read from the ring buffer.
 */
uint32_t rt_ringbuffer_get(struct rt_ringbuffer* rb, uint8_t* ptr, uint16_t length)
{
    uint32_t size;

    RT_ASSERT(rb != NULL);

    /* whether has enough data  */
    size = rt_ringbuffer_data_len(rb);

    /* no data */
    if (size == 0)
        return 0;

    /* less data */
    if (size < length)
        length = size;

    if (rb->buffer_size - rb->read_index > length) {
        /* copy all of data */
        rt_memcpy(ptr, &rb->buffer_ptr[rb->read_index], length);
        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->read_index += length;
        return length;
    }

    rt_memcpy(&ptr[0], &rb->buffer_ptr[rb->read_index], rb->buffer_size - rb->read_index);
    rt_memcpy(&ptr[rb->buffer_size - rb->read_index], &rb->buffer_ptr[0], length - (rb->buffer_size - rb->read_index));

    /* we are going into the other side of the mirror */
    rb->read_mirror = ~rb->read_mirror;
    rb->read_index  = length - (rb->buffer_size - rb->read_index);

    return length;
}
RTM_EXPORT(rt_ringbuffer_get);

/**
 * @brief Get the first readable byte of the ring buffer.
 *
 * @param rb        A pointer to the ringbuffer.
 * @param ptr       When this function return, *ptr is a pointer to the first readable byte of the ring buffer.
 *
 * @note It is recommended to read only one byte, otherwise it may cause buffer overflow.
 *
 * @return Return the size of the ring buffer.
 */
uint32_t rt_ringbuffer_peek(struct rt_ringbuffer* rb, uint8_t** ptr)
{
    uint32_t size;

    RT_ASSERT(rb != NULL);

    *ptr = NULL;

    /* whether has enough data  */
    size = rt_ringbuffer_data_len(rb);

    /* no data */
    if (size == 0)
        return 0;

    *ptr = &rb->buffer_ptr[rb->read_index];

    if (( uint32_t )(rb->buffer_size - rb->read_index) > size) {
        rb->read_index += size;
        return size;
    }

    size = rb->buffer_size - rb->read_index;

    /* we are going into the other side of the mirror */
    rb->read_mirror = ~rb->read_mirror;
    rb->read_index  = 0;

    return size;
}
RTM_EXPORT(rt_ringbuffer_peek);

/**
 * @brief Put a byte into the ring buffer. If ring buffer is full, this operation will fail.
 *
 * @param rb        A pointer to the ring buffer object.
 * @param ch        A byte put into the ring buffer.
 *
 * @return Return the data size we put into the ring buffer. The ring buffer is full if returns 0. Otherwise, it will
 * return 1.
 */
uint32_t rt_ringbuffer_putchar(struct rt_ringbuffer* rb, const uint8_t ch)
{
    RT_ASSERT(rb != NULL);

    /* whether has enough space */
    if (!rt_ringbuffer_space_len(rb))
        return 0;

    rb->buffer_ptr[rb->write_index] = ch;

    /* flip mirror */
    if (rb->write_index == rb->buffer_size - 1) {
        rb->write_mirror = ~rb->write_mirror;
        rb->write_index  = 0;
    }
    else {
        rb->write_index++;
    }

    return 1;
}
RTM_EXPORT(rt_ringbuffer_putchar);

/**
 * @brief Put a byte into the ring buffer. If ring buffer is full, it will discard an old data and put into a new data.
 *
 * @param rb        A pointer to the ring buffer object.
 * @param ch        A byte put into the ring buffer.
 *
 * @return Return the data size we put into the ring buffer. Always return 1.
 */
uint32_t rt_ringbuffer_putchar_force(struct rt_ringbuffer* rb, const uint8_t ch)
{
    enum rt_ringbuffer_state old_state;

    RT_ASSERT(rb != NULL);

    old_state = rt_ringbuffer_status(rb);

    rb->buffer_ptr[rb->write_index] = ch;

    /* flip mirror */
    if (rb->write_index == rb->buffer_size - 1) {
        rb->write_mirror = ~rb->write_mirror;
        rb->write_index  = 0;
        if (old_state == RT_RINGBUFFER_FULL) {
            rb->read_mirror = ~rb->read_mirror;
            rb->read_index  = rb->write_index;
        }
    }
    else {
        rb->write_index++;
        if (old_state == RT_RINGBUFFER_FULL)
            rb->read_index = rb->write_index;
    }

    return 1;
}
RTM_EXPORT(rt_ringbuffer_putchar_force);

/**
 * @brief Get a byte from the ring buffer.
 *
 * @param rb        The pointer to the ring buffer object.
 * @param ch        A pointer to the buffer, used to store one byte.
 *
 * @return 0    The ring buffer is empty.
 * @return 1    Success
 */
uint32_t rt_ringbuffer_getchar(struct rt_ringbuffer* rb, uint8_t* ch)
{
    RT_ASSERT(rb != NULL);

    /* ringbuffer is empty */
    if (!rt_ringbuffer_data_len(rb))
        return 0;

    /* put byte */
    *ch = rb->buffer_ptr[rb->read_index];

    if (rb->read_index == rb->buffer_size - 1) {
        rb->read_mirror = ~rb->read_mirror;
        rb->read_index  = 0;
    }
    else {
        rb->read_index++;
    }

    return 1;
}
RTM_EXPORT(rt_ringbuffer_getchar);

/**
 * @brief Get the size of data in the ring buffer in bytes.
 *
 * @param rb        The pointer to the ring buffer object.
 *
 * @return Return the size of data in the ring buffer in bytes.
 */
uint32_t rt_ringbuffer_data_len(struct rt_ringbuffer* rb)
{
    switch (rt_ringbuffer_status(rb)) {
    case RT_RINGBUFFER_EMPTY:
        return 0;
    case RT_RINGBUFFER_FULL:
        return rb->buffer_size;
    case RT_RINGBUFFER_HALFFULL:
    default: {
        uint32_t wi = rb->write_index, ri = rb->read_index;

        if (wi > ri)
            return wi - ri;
        else
            return rb->buffer_size - (ri - wi);
    }
    }
}
RTM_EXPORT(rt_ringbuffer_data_len);

/**
 * @brief Reset the ring buffer object, and clear all contents in the buffer.
 *
 * @param rb        A pointer to the ring buffer object.
 */
void rt_ringbuffer_reset(struct rt_ringbuffer* rb)
{
    RT_ASSERT(rb != NULL);

    rb->read_mirror  = 0;
    rb->read_index   = 0;
    rb->write_mirror = 0;
    rb->write_index  = 0;
}
RTM_EXPORT(rt_ringbuffer_reset);

/**
 * @brief Get the buffer size of the ring buffer object.
 *
 * @param rb        A pointer to the ring buffer object.
 *
 * @return  Buffer size.
 */
// rt_inline uint16_t rt_ringbuffer_get_size(struct rt_ringbuffer *rb)
// {
//     RT_ASSERT(rb != RT_NULL);
//     return rb->buffer_size;
// }
#ifdef RT_USING_HEAP

/**
 * @brief Create a ring buffer object with a given size.
 *
 * @param size      The size of the buffer in bytes.
 *
 * @return Return a pointer to ring buffer object. When the return value is NULL, it means this creation failed.
 */
struct rt_ringbuffer* rt_ringbuffer_create(uint16_t size)
{
    struct rt_ringbuffer* rb;
    uint8_t* pool;

    RT_ASSERT(size > 0);

    size = ALIGN_DOWN(size, RT_ALIGN_SIZE);

    rb = ( struct rt_ringbuffer* )rt_malloc(sizeof(struct rt_ringbuffer));
    if (rb == NULL)
        goto exit;

    pool = ( uint8_t* )rt_malloc(size);
    if (pool == NULL) {
        rt_free(rb);
        rb = NULL;
        goto exit;
    }
    rt_ringbuffer_init(rb, pool, size);

exit:
    return rb;
}
RTM_EXPORT(rt_ringbuffer_create);

/**
 * @brief Destroy the ring buffer object, which is created by rt_ringbuffer_create() .
 *
 * @param rb        A pointer to the ring buffer object.
 */
void rt_ringbuffer_destroy(struct rt_ringbuffer* rb)
{
    RT_ASSERT(rb != NULL);

    rt_free(rb->buffer_ptr);
    rt_free(rb);
}
RTM_EXPORT(rt_ringbuffer_destroy);

#endif
