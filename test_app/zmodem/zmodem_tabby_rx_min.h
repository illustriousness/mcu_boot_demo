#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint32_t (*zm_io_read_t)(uint8_t *buf, uint16_t len);
typedef void (*zm_io_write_t)(const uint8_t *buf, uint16_t len);
typedef uint32_t (*zm_tick_get_t)(void);

typedef int (*zm_on_begin_t)(const char *name, uint32_t size);
typedef int (*zm_on_data_t)(const uint8_t *data, uint16_t len);
typedef void (*zm_on_end_t)(int ok);

typedef struct {
    zm_io_read_t read;
    zm_io_write_t write;
    zm_tick_get_t tick_get;

    zm_on_begin_t on_begin;
    zm_on_data_t on_data;
    zm_on_end_t on_end;

    /* Scratch buffer provided by the caller.
     *
     * Usage:
     * - The receiver will use write_buf as a temporary chunk buffer when delivering payload
     *   to on_data(). It will NOT keep the data after on_data() returns.
     * - on_data() will be called with arbitrary chunk sizes up to write_buf_size (byte stream),
     *   depending on the ZMODEM subpacket boundaries.
     * - write_buf_size must be >= 2.
     * - For best performance and to avoid unaligned halfword access, provide a 2-byte aligned buffer.
     */
    uint8_t *write_buf;
    uint16_t write_buf_size;

    uint32_t user_pick_timeout_ms;
    uint32_t io_timeout_ms;

    char filename[64];
    uint32_t file_size;
    uint32_t bytes_received;
} zm_tabby_rx_t;

void rz_tabby_init(zm_tabby_rx_t *ctx);
/* only one file can be received at a time */
int rz_tabby(zm_tabby_rx_t *ctx);
