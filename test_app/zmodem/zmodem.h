#pragma once
#include <stdint.h>

/* ZMODEM control characters */
#define ZPAD        '*'     /* Padding character */
#define ZDLE        0x18    /* Ctrl-X: ZMODEM escape */
#define ZDLEE       0x58    /* Escaped ZDLE (ZDLE + 0x40) */

/* ZMODEM frame types */
#define ZRQINIT     0       /* Request receive init */
#define ZRINIT      1       /* Receive init */
#define ZSINIT      2       /* Send init sequence (optional) */
#define ZACK        3       /* ACK to ZRQINIT ZRINIT or ZSINIT */
#define ZFILE       4       /* File name from sender */
#define ZSKIP       5       /* Skip this file */
#define ZNAK        6       /* Last packet was corrupted */
#define ZABORT      7       /* Abort batch transfers */
#define ZFIN        8       /* Finish session */
#define ZRPOS       9       /* Resume data trans at this position */
#define ZDATA       10      /* Data packet(s) follow */
#define ZEOF        11      /* End of file */
#define ZFERR       12      /* Fatal Read or Write error Detected */
#define ZCRC        13      /* Request for file CRC and response */
#define ZCHALLENGE  14      /* Receiver's Challenge */
#define ZCOMPL      15      /* Request is complete */
#define ZCAN        16      /* Other end canned session with CAN*5 */
#define ZFREECNT    17      /* Request for free bytes on filesystem */
#define ZCOMMAND    18      /* Command from sending program */

/* ZDATA subpacket types */
#define ZCRCE       'h'     /* CRC next, frame ends, header packet follows */
#define ZCRCG       'i'     /* CRC next, frame continues nonstop */
#define ZCRCQ       'j'     /* CRC next, frame continues, ZACK expected */
#define ZCRCW       'k'     /* CRC next, ZACK expected, end of frame */
#define ZRUB0       'l'     /* Translate to rubout 0177 */
#define ZRUB1       'm'     /* Translate to rubout 0377 */

/* ZRINIT flags */
#define CANFDX      0x01    /* Receiver can send and receive truly full duplex */
#define CANOVIO     0x02    /* Receiver can receive data during disk I/O */
#define CANBRK      0x04    /* Receiver can send a break signal */
#define CANCRY      0x08    /* Receiver can decrypt */
#define CANLZW      0x10    /* Receiver can uncompress */
#define CANFC32     0x20    /* Receiver can use 32 bit Frame Check */
#define ESCCTL      0x40    /* Receiver expects ctl chars to be escaped */
#define ESC8        0x80    /* Receiver expects 8th bit to be escaped */

/* ZFILE transfer flags */
#define ZCBIN       1       /* Binary transfer - inhibit conversion */
#define ZCNL        2       /* Convert NL to local end of line convention */
#define ZCRESUM     3       /* Resume interrupted file transfer */

/* Return codes */
#define ZM_OK           0
#define ZM_ERROR        -1
#define ZM_TIMEOUT      -2
#define ZM_CRC_ERROR    -3
#define ZM_CANCELLED    -4

/* State machine states */
typedef enum {
    ZM_STATE_IDLE = 0,
    ZM_STATE_WAIT_ZRQINIT,
    ZM_STATE_WAIT_ZFILE,
    ZM_STATE_WAIT_ZDATA,
    ZM_STATE_RECEIVING_DATA,
    ZM_STATE_WAIT_ZEOF,
    ZM_STATE_COMPLETE,
    ZM_STATE_ERROR
} zmodem_state_t;

/* ZMODEM header structure */
typedef struct {
    uint8_t type;       /* Frame type */
    uint32_t position;  /* File position or other info */
} zmodem_header_t;

/* ZMODEM session context */
typedef struct {
    volatile zmodem_state_t state;  /* IMPORTANT: volatile to prevent compiler optimization issues */
    uint32_t file_size;
    uint32_t bytes_received;
    uint32_t file_pos;
    uint8_t *rx_buffer;
    uint16_t rx_buffer_size;
    volatile uint32_t last_activity;  /* Also volatile for timeout checks */
    uint8_t can_count;      /* Count of consecutive CAN characters */
    char filename[256];     /* Received filename */
    uint32_t file_mtime;    /* File modification time */
} zmodem_ctx_t;

/* API Functions */
void zmodem_init(zmodem_ctx_t *ctx, uint8_t *buffer, uint16_t buffer_size);
int zmodem_start_receive(zmodem_ctx_t *ctx);
int zmodem_process(zmodem_ctx_t *ctx);
int zmodem_receive_data(zmodem_ctx_t *ctx, uint8_t *data, uint16_t len);

/* Internal functions */
int zmodem_send_hex_header(uint8_t type, uint32_t position);
int zmodem_send_bin_header(uint8_t type, uint32_t position);
int zmodem_receive_header(zmodem_header_t *header);
int zmodem_receive_file_info(zmodem_ctx_t *ctx);
uint16_t zmodem_calc_crc16(const uint8_t *data, uint16_t len);
uint32_t zmodem_calc_crc32(const uint8_t *data, uint16_t len);

uint32_t zmodem_read(uint8_t *buf, uint16_t len);
void zmodem_write(uint8_t *buf, uint16_t len);