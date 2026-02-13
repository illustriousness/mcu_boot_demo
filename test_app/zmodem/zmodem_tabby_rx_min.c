#include "zmodem_tabby_rx_min.h"
// #include <string.h>
#define memset rt_memset
#define memcpy rt_memcpy
#include "rtthread.h"
#include "rtm.h"
// extern void LOG_HEX(uint32_t offset, uint8_t *buf, uint32_t size);
/* Control chars */
#define ZPAD '*'
#define ZDLE 0x18

/* Header types */
#define ZRINIT 1
#define ZSINIT 2
#define ZACK   3
#define ZFILE  4
#define ZFIN   8
#define ZRPOS  9
#define ZDATA  10
#define ZEOF   11

/* ZDLE escapes and data subpacket end markers */
#define ZCRCE 'h'
#define ZCRCG 'i'
#define ZCRCQ 'j'
#define ZCRCW 'k'
#define ZRUB0 'l'
#define ZRUB1 'm'

/* ZRINIT flags (minimal, CRC16 only) */
#define CANFDX  0x01
#define CANOVIO 0x02

/* Return codes */
#define ZM_OK      0
#define ZM_ERROR   -1
#define ZM_TIMEOUT -2

typedef struct
{
    uint8_t type;
    uint32_t position;
} zm_header_t;

static uint32_t now_ms(zm_tabby_rx_t *ctx)
{
    return ctx->tick_get ? ctx->tick_get() : 0;
}

static int read_byte_deadline(zm_tabby_rx_t *ctx, uint8_t *out, uint32_t deadline)
{
    while (now_ms(ctx) < deadline)
    {
        if (ctx->read(out, 1) == 1)
        {
            return ZM_OK;
        }
    }
    return ZM_TIMEOUT;
}

static int read_hex_nibble(uint8_t c, uint8_t *out)
{
    if (c >= '0' && c <= '9')
    {
        *out = (uint8_t)(c - '0');
        return ZM_OK;
    }
    if (c >= 'a' && c <= 'f')
    {
        *out = (uint8_t)(c - 'a' + 10);
        return ZM_OK;
    }
    if (c >= 'A' && c <= 'F')
    {
        *out = (uint8_t)(c - 'A' + 10);
        return ZM_OK;
    }
    *out = 0;
    return ZM_ERROR;
}

static uint16_t crc16_ccitt0(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0;
    while (len--)
    {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static const char hex[] = "0123456789abcdef";
static void to_hex2(uint8_t value, uint8_t out[2])
{
    out[0] = hex[(value >> 4) & 0xF];
    out[1] = hex[value & 0xF];
}
static void send_hex_header(zm_tabby_rx_t *ctx, uint8_t type, uint32_t position_be)
{
    uint8_t packet[32];
    uint8_t idx = 0;

    uint8_t crc_data[5];
    crc_data[0] = type;
    crc_data[1] = (uint8_t)(position_be >> 24);
    crc_data[2] = (uint8_t)(position_be >> 16);
    crc_data[3] = (uint8_t)(position_be >> 8);
    crc_data[4] = (uint8_t)(position_be);
    const uint16_t crc = crc16_ccitt0(crc_data, 5);

    packet[idx++] = ZPAD;
    packet[idx++] = ZPAD;
    packet[idx++] = ZDLE;
    packet[idx++] = 'B';

    to_hex2(type, &packet[idx]);
    idx += 2;

    to_hex2(crc_data[1], &packet[idx]);
    idx += 2;
    to_hex2(crc_data[2], &packet[idx]);
    idx += 2;
    to_hex2(crc_data[3], &packet[idx]);
    idx += 2;
    to_hex2(crc_data[4], &packet[idx]);
    idx += 2;

    to_hex2((uint8_t)(crc >> 8), &packet[idx]);
    idx += 2;
    to_hex2((uint8_t)(crc & 0xFF), &packet[idx]);
    idx += 2;

    packet[idx++] = '\r';
    packet[idx++] = '\n';
    packet[idx++] = 0x11;

    // LOG_HEX(0, packet, idx);
    ctx->write(packet, idx);
}

static int read_zhex_header(zm_tabby_rx_t *ctx, zm_header_t *hdr, uint32_t deadline)
{
    uint8_t hex_data[14];
    for (int i = 0; i < 14; i++)
    {
        if (read_byte_deadline(ctx, &hex_data[i], deadline) != ZM_OK)
        {
            return ZM_TIMEOUT;
        }
    }

    /* Consume trailer: CR LF [XON] (best-effort) */
    uint8_t ch;
    for (int i = 0; i < 3; i++)
    {
        if (ctx->read(&ch, 1) != 1)
        {
            break;
        }
        if (ch == 0x11)
        {
            break;
        }
    }

    uint8_t n;
    uint8_t type = 0;
    for (int i = 0; i < 2; i++)
    {
        if (read_hex_nibble(hex_data[i], &n) != ZM_OK)
            return ZM_ERROR;
        type = (uint8_t)((type << 4) | n);
    }

    uint8_t p[4];
    for (int i = 0; i < 4; i++)
    {
        uint8_t b = 0;
        for (int j = 0; j < 2; j++)
        {
            if (read_hex_nibble(hex_data[2 + i * 2 + j], &n) != ZM_OK)
                return ZM_ERROR;
            b = (uint8_t)((b << 4) | n);
        }
        p[i] = b;
    }

    uint8_t crc_bytes[2];
    for (int i = 0; i < 2; i++)
    {
        uint8_t b = 0;
        for (int j = 0; j < 2; j++)
        {
            if (read_hex_nibble(hex_data[10 + i * 2 + j], &n) != ZM_OK)
                return ZM_ERROR;
            b = (uint8_t)((b << 4) | n);
        }
        crc_bytes[i] = b;
    }

    const uint16_t got_crc = (uint16_t)((crc_bytes[0] << 8) | crc_bytes[1]);
    uint8_t crc_data[5] = { type, p[0], p[1], p[2], p[3] };
    const uint16_t exp_crc = crc16_ccitt0(crc_data, 5);
    if (got_crc != exp_crc)
    {
        return ZM_ERROR;
    }

    hdr->type = type;
    hdr->position = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    return ZM_OK;
}

static int read_zdle_escaped_byte(zm_tabby_rx_t *ctx, uint8_t *out, uint32_t deadline)
{
    uint8_t ch;
    int ret = read_byte_deadline(ctx, &ch, deadline);
    if (ret != ZM_OK)
    {
        return ret;
    }
    if (ch != ZDLE)
    {
        *out = ch;
        return ZM_OK;
    }

    ret = read_byte_deadline(ctx, &ch, deadline);
    if (ret != ZM_OK)
    {
        return ret;
    }

    if (ch == ZRUB0 || ch == ZRUB1)
    {
        *out = 0x7F;
        return ZM_OK;
    }

    *out = (uint8_t)(ch ^ 0x40);
    return ZM_OK;
}

static int read_zbin_header(zm_tabby_rx_t *ctx, zm_header_t *hdr, uint32_t deadline)
{
    uint8_t buf[7];
    for (int i = 0; i < 7; i++)
    {
        if (read_zdle_escaped_byte(ctx, &buf[i], deadline) != ZM_OK)
        {
            return ZM_TIMEOUT;
        }
    }

    uint8_t crc_data[5] = { buf[0], buf[1], buf[2], buf[3], buf[4] };
    const uint16_t exp_crc = crc16_ccitt0(crc_data, 5);
    const uint16_t got_crc = (uint16_t)((buf[5] << 8) | buf[6]);
    if (exp_crc != got_crc)
    {
        return ZM_ERROR;
    }

    hdr->type = buf[0];
    hdr->position = ((uint32_t)buf[1] << 24) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 8) | (uint32_t)buf[4];
    return ZM_OK;
}

static int recv_header(zm_tabby_rx_t *ctx, zm_header_t *hdr, uint32_t deadline)
{
    uint8_t ch;

    while (now_ms(ctx) < deadline)
    {
        if (read_byte_deadline(ctx, &ch, deadline) != ZM_OK)
        {
            return ZM_TIMEOUT;
        }

        if (ch != ZPAD)
        {
            continue;
        }

        /* optional second ZPAD */
        if (read_byte_deadline(ctx, &ch, deadline) != ZM_OK)
        {
            return ZM_TIMEOUT;
        }
        if (ch == ZPAD)
        {
            if (read_byte_deadline(ctx, &ch, deadline) != ZM_OK)
            {
                return ZM_TIMEOUT;
            }
        }

        if (ch != ZDLE)
        {
            continue;
        }

        if (read_byte_deadline(ctx, &ch, deadline) != ZM_OK)
        {
            return ZM_TIMEOUT;
        }

        if (ch == 'B')
        {
            return read_zhex_header(ctx, hdr, deadline);
        }
        if (ch == 'A')
        {
            return read_zbin_header(ctx, hdr, deadline);
        }
    }

    return ZM_TIMEOUT;
}

typedef int (*subpkt_chunk_cb_t)(void *user, const uint8_t *data, uint16_t len);

static int read_subpacket_stream(zm_tabby_rx_t *ctx, subpkt_chunk_cb_t cb, void *user, uint8_t *frame_end, uint32_t deadline)
{
    uint8_t ch;
    uint16_t used = 0;
    uint8_t *buf = ctx->write_buf;
    const uint16_t buf_size = ctx->write_buf_size;

    if (!buf || buf_size < 2)
    {
        return ZM_ERROR;
    }

    *frame_end = 0;

    while (now_ms(ctx) < deadline)
    {
        int ret = read_byte_deadline(ctx, &ch, deadline);
        if (ret != ZM_OK)
        {
            return ret;
        }

        if (ch != ZDLE)
        {
            buf[used++] = ch;
            if (used == buf_size)
            {
                ret = cb(user, buf, used);
                if (ret != ZM_OK)
                {
                    return ret;
                }
                used = 0;
            }
            continue;
        }

        ret = read_byte_deadline(ctx, &ch, deadline);
        if (ret != ZM_OK)
        {
            return ret;
        }

        if (ch == ZCRCE || ch == ZCRCG || ch == ZCRCQ || ch == ZCRCW)
        {
            *frame_end = ch;
            /* Consume CRC16 */
            (void)read_zdle_escaped_byte(ctx, &ch, deadline);
            (void)read_zdle_escaped_byte(ctx, &ch, deadline);

            if (used)
            {
                ret = cb(user, buf, used);
                if (ret != ZM_OK)
                {
                    return ret;
                }
            }
            return ZM_OK;
        }

        if (ch == ZRUB0 || ch == ZRUB1)
        {
            ch = 0x7F;
        }
        else
        {
            ch ^= 0x40;
        }

        buf[used++] = ch;
        if (used == buf_size)
        {
            ret = cb(user, buf, used);
            if (ret != ZM_OK)
            {
                return ret;
            }
            used = 0;
        }
    }

    return ZM_TIMEOUT;
}

static int discard_chunk_cb(void *user, const uint8_t *data, uint16_t len)
{
    (void)user;
    (void)data;
    (void)len;
    return ZM_OK;
}

static int discard_subpacket(zm_tabby_rx_t *ctx, uint32_t deadline)
{
    uint8_t frame_end = 0;
    return read_subpacket_stream(ctx, discard_chunk_cb, NULL, &frame_end, deadline);
}

typedef struct
{
    zm_tabby_rx_t *ctx;
    uint16_t name_len;
    uint8_t got_name;
    uint32_t size;
    uint8_t got_size;
} zfile_parse_t;

static int zfile_chunk_cb(void *user, const uint8_t *data, uint16_t len)
{
    zfile_parse_t *p = (zfile_parse_t *)user;
    for (uint16_t i = 0; i < len; i++)
    {
        const uint8_t ch = data[i];
        if (!p->got_name)
        {
            if (ch == 0)
            {
                p->ctx->filename[p->name_len] = 0;
                p->got_name = 1;
                continue;
            }
            if (p->name_len + 1 < sizeof(p->ctx->filename))
            {
                p->ctx->filename[p->name_len++] = (char)ch;
            }
            continue;
        }

        if (!p->got_size)
        {
            if (ch >= '0' && ch <= '9')
            {
                p->size = p->size * 10 + (uint32_t)(ch - '0');
            }
            else if (p->size)
            {
                p->got_size = 1;
            }
        }
    }
    return ZM_OK;
}

static int handle_zfile(zm_tabby_rx_t *ctx)
{
    zfile_parse_t parser;
    memset(&parser, 0, sizeof(parser));
    parser.ctx = ctx;

    uint8_t frame_end = 0;
    const uint32_t deadline = now_ms(ctx) + ctx->io_timeout_ms;

    int ret = read_subpacket_stream(ctx, zfile_chunk_cb, &parser, &frame_end, deadline);
    if (ret != ZM_OK)
    {
        return ret;
    }
    if (!parser.got_name)
    {
        return ZM_ERROR;
    }
    ctx->file_size = parser.size;

    if (ctx->on_begin)
    {
        if (ctx->on_begin(ctx->filename, ctx->file_size) != 0)
        {
            return ZM_ERROR;
        }
    }
    return ZM_OK;
}

static int data_chunk_cb(void *user, const uint8_t *data, uint16_t len)
{
    zm_tabby_rx_t *ctx = (zm_tabby_rx_t *)user;
    if (ctx->on_data && ctx->on_data(data, len) != 0)
    {
        return ZM_ERROR;
    }
    ctx->bytes_received += len;
    return ZM_OK;
}

void rz_tabby_init(zm_tabby_rx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->user_pick_timeout_ms = 120000;
    ctx->io_timeout_ms = 5000;
}

int rz_tabby(zm_tabby_rx_t *ctx)
{
    if (!ctx || !ctx->read || !ctx->write || !ctx->tick_get)
    {
        return ZM_ERROR;
    }
    if (!ctx->write_buf || ctx->write_buf_size < 2)
    {
        return ZM_ERROR;
    }

    ctx->bytes_received = 0;
    ctx->filename[0] = 0;
    ctx->file_size = 0;

    /* Ready to receive: ZRINIT with minimal flags */
    send_hex_header(ctx, ZRINIT, (uint32_t)(CANFDX | CANOVIO));

    zm_header_t hdr;

    /* Wait for ZFILE (user may be picking a file) */
    const uint32_t pick_deadline = now_ms(ctx) + ctx->user_pick_timeout_ms;
    while (now_ms(ctx) < pick_deadline)
    {
        int ret = recv_header(ctx, &hdr, pick_deadline);
        if (ret != ZM_OK)
        {
            continue;
        }

        if (hdr.type == ZSINIT)
        {
            (void)discard_subpacket(ctx, now_ms(ctx) + ctx->io_timeout_ms);
            send_hex_header(ctx, ZACK, 0);
            continue;
        }

        if (hdr.type == ZFILE)
        {
            ret = handle_zfile(ctx);
            if (ret != ZM_OK)
            {
                return ret;
            }
            send_hex_header(ctx, ZRPOS, 0);
            break;
        }
    }

    if (!ctx->filename[0])
    {
        return ZM_TIMEOUT;
    }

    /* Receive until ZEOF */
    uint8_t eof_received = 0;
    while (1)
    {
        const uint32_t deadline = now_ms(ctx) + ctx->io_timeout_ms;
        int ret = recv_header(ctx, &hdr, deadline);
        if (ret != ZM_OK)
        {
            continue;
        }

        if (hdr.type == ZDATA)
        {
            /* A ZDATA frame may contain multiple subpackets.
             * - ZCRCG: continue with another subpacket (no header)
             * - ZCRCQ: continue with another subpacket, ZACK expected
             * - ZCRCE: frame ends, header follows
             * - ZCRCW: frame ends, ZACK expected, header follows */
            while (1)
            {
                uint8_t frame_end = 0;
                ret = read_subpacket_stream(ctx, data_chunk_cb, ctx, &frame_end, now_ms(ctx) + ctx->io_timeout_ms);
                if (ret != ZM_OK)
                {
                    if (ctx->on_end)
                    {
                        ctx->on_end(0);
                    }
                    return ret;
                }

                if (frame_end == ZCRCQ || frame_end == ZCRCW)
                {
                    send_hex_header(ctx, ZACK, ctx->bytes_received);
                }

                if (frame_end == ZCRCG || frame_end == ZCRCQ)
                {
                    continue;
                }

                break;
            }
            continue;
        }

        if (hdr.type == ZEOF)
        {
            /* Sender has finished sending file data. Per ZMODEM flow,
             * acknowledge with ZRINIT and wait for sender's ZFIN. */
            eof_received = 1;
            send_hex_header(ctx, ZRINIT, (uint32_t)(CANFDX | CANOVIO));
            continue;
        }

        (void)eof_received;
        if (hdr.type == ZFIN)
        {
            send_hex_header(ctx, ZFIN, 0);
            if (ctx->on_end)
            {
                ctx->on_end(1);
            }
            return ZM_OK;
        }
    }
}
RTM_EXPORT(rz_tabby);