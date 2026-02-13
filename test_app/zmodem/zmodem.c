// #include "zmodem.h"
// #include "main.h"
// #include "usart.h"
// #include "upgrade.h"
// #include <string.h>
// #define RT_ALIGN(size, align) (((size) + (align) - 1) & ~((align) - 1))
// extern int rt_kprintf(const char *fmt, ...);
// // #define LOG_D(...) rt_kprintf(__VA_ARGS__)
// #define LOG_D(...)
// /* Timeout in milliseconds */
// #define ZMODEM_TIMEOUT_MS 5000
// extern void LOG_HEX(uint32_t offset, uint8_t *buf, uint32_t size);
// extern uint32_t tick_get(void);
// /* CRC16 table for ZMODEM */
// static const uint16_t crc16_table[256] = {
//     0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
//     0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
//     0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
//     0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
//     0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
//     0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
//     0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
//     0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
//     0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
//     0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
//     0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
//     0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
//     0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
//     0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
//     0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
//     0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
//     0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
//     0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
//     0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
//     0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
//     0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
//     0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
//     0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
//     0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
//     0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
//     0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
//     0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
//     0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
//     0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
//     0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
//     0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
//     0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
// };

// /**
//  * @brief Calculate CRC16 for ZMODEM
//  */
// uint16_t zmodem_calc_crc16(const uint8_t *data, uint16_t len)
// {
//     uint16_t crc = 0;
//     while (len--)
//     {
//         crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xFF];
//     }
//     return crc;
// }

// /**
//  * @brief Calculate CRC32 for ZMODEM (using FlashDB CRC32)
//  */
// uint32_t zmodem_calc_crc32(const uint8_t *data, uint16_t len)
// {
//     extern uint32_t fdb_calc_crc32(uint32_t crc, const void *buf, size_t size);
//     return fdb_calc_crc32(0, data, len);
// }

// /**
//  * @brief Convert value to hex ASCII
//  */
// static void int_to_hex(uint32_t value, uint8_t *hex, uint8_t len)
// {
//     for (int i = len - 1; i >= 0; i--)
//     {
//         uint8_t nibble = (value >> (i * 4)) & 0xF;
//         hex[len - 1 - i] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
//     }
// }

// static int zmodem_read_byte_timeout (uint8_t *ch, uint32_t deadline)
// {
//     while (tick_get() < deadline)
//     {
//         if (zmodem_read(ch, 1) == 1)
//         {
//             return ZM_OK;
//         }
//     }
//     return ZM_TIMEOUT;
// }

// static int zmodem_read_zdle_escaped_byte_timeout (uint8_t *out, uint32_t deadline)
// {
//     uint8_t ch;
//     int ret = zmodem_read_byte_timeout(&ch, deadline);
//     if (ret != ZM_OK)
//     {
//         return ret;
//     }

//     if (ch != ZDLE)
//     {
//         *out = ch;
//         return ZM_OK;
//     }

//     ret = zmodem_read_byte_timeout(&ch, deadline);
//     if (ret != ZM_OK)
//     {
//         return ret;
//     }

//     if (ch == ZRUB0 || ch == ZRUB1)
//     {
//         *out = 0x7F;
//         return ZM_OK;
//     }

//     *out = ch ^ 0x40;
//     return ZM_OK;
// }

// static int zmodem_read_subpacket (uint8_t *out, uint16_t out_size, uint16_t *out_len, uint32_t deadline)
// {
//     uint16_t idx = 0;
//     uint8_t ch;

//     while (tick_get() < deadline)
//     {
//         int ret = zmodem_read_byte_timeout(&ch, deadline);
//         if (ret != ZM_OK)
//         {
//             return ret;
//         }

//         if (ch != ZDLE)
//         {
//             if (idx < out_size)
//             {
//                 out[idx++] = ch;
//             }
//             continue;
//         }

//         ret = zmodem_read_byte_timeout(&ch, deadline);
//         if (ret != ZM_OK)
//         {
//             return ret;
//         }

//         if (ch == ZCRCE || ch == ZCRCG || ch == ZCRCQ || ch == ZCRCW)
//         {
//             /* Consume CRC16 (2 bytes), handling ZDLE escaping */
//             for (int i = 0; i < 2; i++)
//             {
//                 ret = zmodem_read_zdle_escaped_byte_timeout(&ch, deadline);
//                 if (ret != ZM_OK)
//                 {
//                     return ret;
//                 }
//             }

//             *out_len = idx;
//             return ZM_OK;
//         }

//         if (ch == ZRUB0 || ch == ZRUB1)
//         {
//             ch = 0x7F;
//         }
//         else
//         {
//             ch ^= 0x40;
//         }

//         if (idx < out_size)
//         {
//             out[idx++] = ch;
//         }
//     }

//     return ZM_TIMEOUT;
// }

// /**
//  * @brief Send ZMODEM hex header
//  * Format: **<TYPE><4 bytes position in hex><2 bytes CRC16 in hex><CR><LF><XON>
//  */
// int zmodem_send_hex_header(uint8_t type, uint32_t position)
// {
//     uint8_t header[32];  /* FIXED: Was 20, but needs 21 bytes (increased to 32 for safety) */
//     uint8_t crc_data[5];
//     uint16_t crc;
//     uint8_t idx = 0;

//     /* ZPAD ZPAD */
//     header[idx++] = ZPAD;
//     header[idx++] = ZPAD;

//     /* ZDLE */
//     header[idx++] = ZDLE;

//     /* Hex header indicator */
//     header[idx++] = 'B';

//     /* Frame type */
//     int_to_hex(type, &header[idx], 2);
//     crc_data[0] = type;
//     idx += 2;

//     /* Position (4 bytes, big endian)
//      * Tabby/zmodem.js expects parameters in MSB->LSB order in hex headers. */
//     for (int i = 3; i >= 0; i--)
//     {
//         uint8_t byte = (position >> (i * 8)) & 0xFF;
//         int_to_hex(byte, &header[idx], 2);
//         crc_data[1 + (3 - i)] = byte;
//         idx += 2;
//     }

//     /* Calculate CRC16 */
//     crc = zmodem_calc_crc16(crc_data, 5);

//     /* Add CRC16 (big endian) */
//     int_to_hex((crc >> 8) & 0xFF, &header[idx], 2);
//     idx += 2;
//     int_to_hex(crc & 0xFF, &header[idx], 2);
//     idx += 2;

//     /* CR LF */
//     header[idx++] = '\r';
//     header[idx++] = '\n';

//     /* XON (optional, helps with flow control) */
//     header[idx++] = 0x11;
//     // LOG_HEX(0, header, idx);
//     /* Send header */
//     zmodem_write(header, idx);

//     LOG_D("TX HEX HDR: type=%02x pos=%08x\n", type, position);
//     return ZM_OK;
// }

// /**
//  * @brief Send ZMODEM binary header (16-bit CRC)
//  * Format: ZPAD ZDLE <TYPE> <4 bytes position> <2 bytes CRC16>
//  */
// int zmodem_send_bin_header(uint8_t type, uint32_t position)
// {
//     uint8_t header[20];
//     uint8_t crc_data[5];
//     uint16_t crc;
//     uint8_t idx = 0;

//     /* ZPAD */
//     header[idx++] = ZPAD;

//     /* ZDLE */
//     header[idx++] = ZDLE;

//     /* Binary header indicator */
//     header[idx++] = 'A';

//     /* Frame type */
//     header[idx++] = type;
//     crc_data[0] = type;

//     /* Position (4 bytes, little endian) */
//     for (int i = 0; i < 4; i++)
//     {
//         header[idx++] = (position >> (i * 8)) & 0xFF;
//         crc_data[1 + i] = header[idx - 1];
//     }

//     /* Calculate CRC16 */
//     crc = zmodem_calc_crc16(crc_data, 5);

//     /* Add CRC16 (big endian) */
//     header[idx++] = (crc >> 8) & 0xFF;
//     header[idx++] = crc & 0xFF;

//     // LOG_HEX(0, header, idx);
//     zmodem_write(header, idx);
//     LOG_D("TX BIN HDR: type=%02x pos=%08x\n", type, position);
//     return ZM_OK;
// }

// /**
//  * @brief Receive and parse ZMODEM header
//  */
// int zmodem_receive_header(zmodem_header_t *header)
// {
//     uint8_t buf[20];
//     uint8_t ch;
//     uint32_t timeout = tick_get() + ZMODEM_TIMEOUT_MS;
//     int idx = 0;

//     LOG_D("[RX_HDR] Waiting for ZPAD...\n");

//     /* Wait for ZPAD (there should be 1 or 2) */
//     uint32_t wait_count = 0;
//     while (tick_get() < timeout)
//     {
//         if (zmodem_read(&ch, 1) == 1)
//         {
//             // LOG_D("[RX_HDR] Got byte: 0x%02x\n", ch);
//             if (ch == ZPAD)
//             {
//                 LOG_D("[RX_HDR] Found ZPAD!\n");
//                 break;
//             }
//         }
//         else
//         {
//             wait_count++;
//             if (wait_count > 1000000)
//             {
//                 LOG_D("[RX_HDR] Still waiting... (timeout in %d ms)\n", timeout - tick_get());
//                 wait_count = 0;
//             }
//         }
//     }

//     if (tick_get() >= timeout)
//     {
//         return ZM_TIMEOUT;
//     }

//     /* Try to read next byte - could be another ZPAD or ZDLE */
//     wait_count = 0;
//     while (tick_get() < timeout)
//     {
//         if (zmodem_read(&ch, 1) == 1)
//         {
//             LOG_D("[RX_HDR] After ZPAD, got: 0x%02x\n", ch);
//             if (ch == ZPAD)
//             {
//                 /* Got second ZPAD, now wait for ZDLE */
//                 LOG_D("[RX_HDR] Got second ZPAD\n");
//                 break;
//             }
//             else if (ch == ZDLE)
//             {
//                 /* Only one ZPAD, this is ZDLE already, continue */
//                 LOG_D("[RX_HDR] Got ZDLE directly\n");
//                 goto read_header_type;
//             }
//         }
//         else
//         {
//             wait_count++;
//             if (wait_count > 100000)
//             {
//                 wait_count = 0;
//             }
//         }
//     }

//     if (tick_get() >= timeout)
//     {
//         LOG_D("[RX_HDR] Timeout waiting for second ZPAD or ZDLE\n");
//         return ZM_TIMEOUT;
//     }

//     /* Read ZDLE */
//     wait_count = 0;
//     while (tick_get() < timeout)
//     {
//         if (zmodem_read(&ch, 1) == 1)
//         {
//             LOG_D("[RX_HDR] After second ZPAD, got: 0x%02x\n", ch);
//             if (ch == ZDLE)
//             {
//                 LOG_D("[RX_HDR] Got ZDLE\n");
//                 break;
//             }
//         }
//         else
//         {
//             wait_count++;
//             if (wait_count > 100000)
//             {
//                 wait_count = 0;
//             }
//         }
//     }

//     if (tick_get() >= timeout)
//     {
//         LOG_D("[RX_HDR] Timeout waiting for ZDLE\n");
//         return ZM_TIMEOUT;
//     }

// read_header_type:

//     /* Read header type indicator */
//     wait_count = 0;
//     while (tick_get() < timeout)
//     {
//         if (zmodem_read(&ch, 1) == 1)
//         {
//             LOG_D("[RX_HDR] Header type indicator: 0x%02x ('%c')\n", ch, ch);
//             break;
//         }
//         else
//         {
//             wait_count++;
//             if (wait_count > 100000)
//             {
//                 wait_count = 0;
//             }
//         }
//     }

//     if (tick_get() >= timeout)
//     {
//         LOG_D("[RX_HDR] Timeout waiting for header type indicator\n");
//         return ZM_TIMEOUT;
//     }

//     if (ch == 'B')
//     {
//         /* Hex header: type(2) + pos(8) + crc(4) = 14 hex chars */
//         uint8_t hex_data[14];

//         LOG_D("[RX_HDR] Reading 14 hex chars for hex header...\n");

//         /* Read 14 hex characters (type + position + CRC16) */
//         for (idx = 0; idx < 14 && tick_get() < timeout;)
//         {
//             if (zmodem_read(&hex_data[idx], 1) == 1)
//             {
//                 LOG_D("[RX_HDR] hex_data[%d] = 0x%02x ('%c')\n", idx, hex_data[idx],
//                       (hex_data[idx] >= 0x20 && hex_data[idx] < 0x7F) ? hex_data[idx] : '.');
//                 idx++;
//             }
//         }

//         LOG_D("[RX_HDR] Read %d/14 hex chars\n", idx);

//         if (idx < 14)
//         {
//             LOG_D("[RX_HDR] Timeout reading hex header (got %d/14 bytes)\n", idx);
//             return ZM_TIMEOUT;
//         }

//         /* Consume CR, LF, and optional XON */
//         uint8_t trailer_count = 0;
//         uint32_t trailer_timeout = tick_get() + 100;  /* Short timeout for trailer */
//         while (tick_get() < trailer_timeout && trailer_count < 3)
//         {
//             if (zmodem_read(&ch, 1) == 1)
//             {
//                 LOG_D("[RX_HDR] Trailer[%d] = 0x%02x\n", trailer_count, ch);
//                 trailer_count++;
//                 if (ch == 0x11)
//                     break;  /* XON marks end */
//             }
//         }

//         /* Parse hex values - helper function to convert hex char to nibble */
//         uint8_t hex_byte;
//         int i;

//         /* Parse type */
//         hex_byte = 0;
//         for (i = 0; i < 2; i++)
//         {
//             uint8_t c = hex_data[i];
//             uint8_t nibble;
//             if (c >= '0' && c <= '9')
//                 nibble = c - '0';
//             else if (c >= 'a' && c <= 'f')
//                 nibble = c - 'a' + 10;
//             else if (c >= 'A' && c <= 'F')
//                 nibble = c - 'A' + 10;
//             else
//                 nibble = 0;
//             hex_byte = (hex_byte << 4) | nibble;
//         }
//         header->type = hex_byte;

//         /* Parse position (4 bytes, big endian) */
//         header->position = 0;
//         for (i = 0; i < 4; i++)
//         {
//             hex_byte = 0;
//             for (int j = 0; j < 2; j++)
//             {
//                 uint8_t c = hex_data[2 + i * 2 + j];
//                 uint8_t nibble;
//                 if (c >= '0' && c <= '9')
//                     nibble = c - '0';
//                 else if (c >= 'a' && c <= 'f')
//                     nibble = c - 'a' + 10;
//                 else if (c >= 'A' && c <= 'F')
//                     nibble = c - 'A' + 10;
//                 else
//                     nibble = 0;
//                 hex_byte = (hex_byte << 4) | nibble;
//             }
//             header->position = (header->position << 8) | hex_byte;
//         }

//         LOG_D("RX HEX HDR: type=%02x pos=%08x\n", header->type, header->position);
//         return ZM_OK;
//     }
//     else if (ch == 'A')
//     {
//         /* Binary header - read 7 bytes (type + 4 pos + 2 crc), with ZDLE escaping */
//         LOG_D("[RX_HDR] Reading 7 bytes for binary header...\n");
//         for (idx = 0; idx < 7; idx++)
//         {
//             if (zmodem_read_zdle_escaped_byte_timeout(&buf[idx], timeout) != ZM_OK)
//             {
//                 break;
//             }
//             LOG_D("[RX_HDR] buf[%d] = 0x%02x\n", idx, buf[idx]);
//         }

//         LOG_D("[RX_HDR] Read %d/7 bytes\n", idx);

//         if (idx < 7)
//         {
//             LOG_D("[RX_HDR] Timeout reading binary header (got %d/7 bytes)\n", idx);
//             return ZM_TIMEOUT;
//         }

//         header->type = buf[0];
//         header->position = ((uint32_t)buf[1] << 24) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 8) | (uint32_t)buf[4];

//         LOG_D("RX BIN HDR: type=%02x pos=%08x\n", header->type, header->position);
//         return ZM_OK;
//     }

//     return ZM_ERROR;
// }

// /**
//  * @brief Receive and parse ZFILE data (filename and file info)
//  * Must be called after receiving ZFILE header
//  */
// int zmodem_receive_file_info(zmodem_ctx_t *ctx)
// {
//     uint8_t subpacket[384];
//     uint16_t subpacket_len = 0;
//     const uint32_t deadline = tick_get() + 15000;

//     if (zmodem_read_subpacket(subpacket, sizeof(subpacket), &subpacket_len, deadline) != ZM_OK)
//     {
//         LOG_D("ERROR: Failed to read ZFILE subpacket\n");
//         return ZM_TIMEOUT;
//     }

//     uint16_t name_len = 0;
//     while (name_len < subpacket_len && subpacket[name_len] != 0)
//     {
//         name_len++;
//     }

//     if (name_len == 0 || name_len >= subpacket_len)
//     {
//         LOG_D("ERROR: ZFILE subpacket missing filename\n");
//         return ZM_ERROR;
//     }

//     if (name_len >= sizeof(ctx->filename))
//     {
//         name_len = sizeof(ctx->filename) - 1;
//     }
//     memcpy(ctx->filename, subpacket, name_len);
//     ctx->filename[name_len] = '\0';
//     LOG_D("Filename: %s\n", ctx->filename);

//     const char *info_buf = (const char *)&subpacket[name_len + 1];
//     LOG_D("File info: %s\n", info_buf);

//     /* Parse file size from info string */
//     ctx->file_size = 0;
//     ctx->file_mtime = 0;

//     if (*info_buf)
//     {
//         /* Format: "size mtime mode ..." - we only care about size */
//         const char *token = info_buf;

//         /* Parse size */
//         while (*token == ' ')
//             token++;
//         if (*token >= '0' && *token <= '9')
//         {
//             while (*token >= '0' && *token <= '9')
//             {
//                 ctx->file_size = ctx->file_size * 10 + (*token - '0');
//                 token++;
//             }
//         }

//         /* Skip to mtime (optional) */
//         while (*token == ' ')
//             token++;
//         if (*token >= '0' && *token <= '9')
//         {
//             while (*token >= '0' && *token <= '9')
//             {
//                 ctx->file_mtime = ctx->file_mtime * 10 + (*token - '0');
//                 token++;
//             }
//         }
//     }

//     rt_kprintf("Parsed file size: %u bytes\n", ctx->file_size);

//     return ZM_OK;
// }

// static int zmodem_discard_subpacket (uint32_t timeout_ms)
// {
//     uint8_t ch;
//     uint32_t timeout = tick_get() + timeout_ms;

//     while (tick_get() < timeout)
//     {
//         if (zmodem_read(&ch, 1) != 1)
//         {
//             continue;
//         }

//         if (ch != ZDLE)
//         {
//             continue;
//         }

//         if (zmodem_read(&ch, 1) != 1)
//         {
//             continue;
//         }

//         if (ch == ZCRCE || ch == ZCRCG || ch == ZCRCQ || ch == ZCRCW)
//         {
//             /* Consume CRC16 (2 bytes), handling ZDLE escaping */
//             uint8_t crc_bytes[2];
//             for (int i = 0; i < 2; i++)
//             {
//                 if (zmodem_read(&crc_bytes[i], 1) != 1)
//                 {
//                     return ZM_TIMEOUT;
//                 }
//                 if (crc_bytes[i] == ZDLE)
//                 {
//                     if (zmodem_read(&crc_bytes[i], 1) != 1)
//                     {
//                         return ZM_TIMEOUT;
//                     }
//                     crc_bytes[i] ^= 0x40;
//                 }
//             }
//             return ZM_OK;
//         }
//     }

//     return ZM_TIMEOUT;
// }

// /**
//  * @brief Initialize ZMODEM context
//  */
// void zmodem_init(zmodem_ctx_t *ctx, uint8_t *buffer, uint16_t buffer_size)
// {
//     memset(ctx, 0, sizeof(zmodem_ctx_t));
//     ctx->state = ZM_STATE_IDLE;
//     ctx->rx_buffer = buffer;
//     ctx->rx_buffer_size = buffer_size;
// }

// /**
//  * @brief Start ZMODEM receive session
//  */
// int zmodem_start_receive(zmodem_ctx_t *ctx)
// {
//     /* CRITICAL: Save pointer to volatile variable to prevent corruption */
//     zmodem_ctx_t * volatile ctx_safe = ctx;

//     LOG_D("ZMODEM: Starting receive session (ctx=%p)\n", (void *)ctx_safe);

//     /* Send ZRINIT to indicate ready to receive
//      * Do not advertise CANFC32 unless ZBIN32/ZCRC32 is supported. */
//     uint32_t caps = CANFDX | CANOVIO;
//     zmodem_send_hex_header(ZRINIT, caps);

//     /* Use ctx_safe for all operations */
//     ctx_safe->state = ZM_STATE_WAIT_ZFILE;
//     ctx_safe->last_activity = tick_get();
//     ctx_safe->can_count = 0;

//     LOG_D("State set to %d at %p\n", ctx_safe->state, (void *)&ctx_safe->state);

//     return ZM_OK;
// }

// /**
//  * @brief Process received data
//  */
// int zmodem_receive_data(zmodem_ctx_t *ctx, uint8_t *data, uint16_t len)
// {
//     /* Check for CAN (cancel) sequence */
//     for (int i = 0; i < len; i++)
//     {
//         if (data[i] == 0x18)
//         { /* CAN character */
//             ctx->can_count++;
//             if (ctx->can_count >= 5)
//             {
//                 LOG_D("ZMODEM: Transfer cancelled by sender\n");
//                 ctx->state = ZM_STATE_ERROR;
//                 return ZM_CANCELLED;
//             }
//         }
//         else
//         {
//             ctx->can_count = 0;
//         }
//     }

//     return ZM_OK;
// }

// /**
//  * @brief Main ZMODEM processing loop
//  */
// int zmodem_process(zmodem_ctx_t *ctx)
// {
//     zmodem_header_t header;
//     int ret;
//     static uint32_t call_count = 0;
//     static uint8_t prev_state = 0xFF;

//     call_count++;

//     /* Always log first few calls and state changes */
//     if (call_count <= 5)
//     {
//         LOG_D("[PROCESS] Call #%d ENTER: state=%d, ctx=%p\n",
//               call_count, ctx->state, (void *)ctx);
//     }

//     /* Always log if state changed */
//     if (ctx->state != prev_state)
//     {
//         LOG_D("[PROCESS] State changed: %d -> %d\n", prev_state, ctx->state);
//         prev_state = ctx->state;
//     }

//     /* Check timeout */
//     if (tick_get() - ctx->last_activity > ZMODEM_TIMEOUT_MS)
//     {
//         LOG_D("ZMODEM: Timeout (elapsed=%d ms)\n", tick_get() - ctx->last_activity);
//         ctx->state = ZM_STATE_ERROR;
//         return ZM_TIMEOUT;
//     }

//     // LOG_D("[PROCESS] switch on state=%d (WAIT_ZFILE=%d, WAIT_ZDATA=%d)\n",
//     //       ctx->state, ZM_STATE_WAIT_ZFILE, ZM_STATE_WAIT_ZDATA);

//     switch (ctx->state)
//     {
//     case ZM_STATE_WAIT_ZFILE:
//         LOG_D("Waiting for ZFILE...\n");
//         ret = zmodem_receive_header(&header);
//         LOG_D("receive_header returned: %d\n", ret);
//         if (ret == ZM_OK)
//         {
//             if (header.type == ZSINIT)
//             {
//                 LOG_D("ZMODEM: ZSINIT received, discarding subpacket\n");
//                 (void)zmodem_discard_subpacket(5000);
//                 zmodem_send_hex_header(ZACK, 0);
//                 ctx->last_activity = tick_get();
//             }
//             else if (header.type == ZFILE)
//             {
//                 LOG_D("ZMODEM: ZFILE received, ctx=%p\n", (void *)ctx);

//                     /* CRITICAL: Read filename and file info after ZFILE header */
//                 LOG_D("Before receive_file_info: ctx=%p\n", (void *)ctx);
//                 ret = zmodem_receive_file_info(ctx);
//                 LOG_D("After receive_file_info: ctx=%p, ret=%d\n", (void *)ctx, ret);
//                 if (ret != ZM_OK)
//                 {
//                     LOG_D("Failed to receive file info\n");
//                     ctx->state = ZM_STATE_ERROR;
//                     return ZM_ERROR;
//                 }

//                 ctx->bytes_received = 0;

//                     /* Send ZRPOS to request data from position 0 */
//                 LOG_D("Before send_hex_header: ctx=%p\n", (void *)ctx);
//                 zmodem_send_hex_header(ZRPOS, 0);
//                 LOG_D("After send_hex_header: ctx=%p\n", (void *)ctx);

//                 LOG_D("Setting state to ZM_STATE_WAIT_ZDATA (3), ctx=%p\n", (void *)ctx);
//                 ctx->state = ZM_STATE_WAIT_ZDATA;
//                 ctx->last_activity = tick_get();
//                 LOG_D("State after setting: %d, ctx=%p, &state=%p\n",
//                       ctx->state, (void *)ctx, (void *)&ctx->state);
//                 LOG_D("About to break from WAIT_ZFILE case\n");
//             }
//             else if (header.type == ZFIN)
//             {
//                 LOG_D("ZMODEM: Session finished\n");
//                 zmodem_send_hex_header(ZFIN, 0);
//                 ctx->state = ZM_STATE_COMPLETE;
//                 return ZM_OK;
//             }
//             else
//             {
//                 LOG_D("Unexpected header type: %02x\n", header.type);
//             }
//         }
//         else
//         {
//             LOG_D("Failed to receive header, ret=%d\n", ret);
//         }
//         break;

//     case ZM_STATE_WAIT_ZDATA:
//         LOG_D("Waiting for ZDATA...\n");
//         ret = zmodem_receive_header(&header);
//         LOG_D("receive_header returned: %d, type=%02x\n", ret, ret == ZM_OK ? header.type : 0xFF);
//         if (ret == ZM_OK && header.type == ZDATA)
//         {
//             LOG_D("ZMODEM: ZDATA received, pos=%d\n", header.position);
//             ctx->state = ZM_STATE_RECEIVING_DATA;
//             ctx->last_activity = tick_get();
//         }
//         break;

//     case ZM_STATE_RECEIVING_DATA:
//             /* This state is handled by data reception callback */
//         break;

//     case ZM_STATE_WAIT_ZEOF:
//         LOG_D("Waiting for ZEOF...\n");
//         ret = zmodem_receive_header(&header);
//         LOG_D("receive_header returned: %d, type=%02x\n", ret, ret == ZM_OK ? header.type : 0xFF);
//         if (ret == ZM_OK && header.type == ZEOF)
//         {
//             LOG_D("ZMODEM: ZEOF received (file position=%d)\n", header.position);
//             LOG_D("Total received: %d bytes\n", ctx->bytes_received);

//                 /* Send ZRINIT to acknowledge and wait for next file or ZFIN */
//             zmodem_send_hex_header(ZRINIT, CANFDX | CANOVIO);

//                 /* If this was the only file, sender will send ZFIN next */
//             ctx->state = ZM_STATE_WAIT_ZFILE;  /* Will receive ZFIN in WAIT_ZFILE state */
//             ctx->last_activity = tick_get();
//             LOG_D("Sent ZRINIT, waiting for ZFIN or next file...\n");
//         }
//         else if (ret == ZM_OK && header.type == ZFIN)
//         {
//                 /* Sender might send ZFIN directly */
//             LOG_D("ZMODEM: ZFIN received (session end)\n");
//             zmodem_send_hex_header(ZFIN, 0);
//             ctx->state = ZM_STATE_COMPLETE;
//         }
//         else if (ret != ZM_OK)
//         {
//             LOG_D("Failed to receive ZEOF, ret=%d\n", ret);
//         }
//         break;

//     case ZM_STATE_COMPLETE:
//         return ZM_OK;

//     case ZM_STATE_ERROR:
//         return ZM_ERROR;

//     default:
//         LOG_D("[PROCESS] default case, state=%d\n", ctx->state);
//         break;
//     }

//     LOG_D("[PROCESS] About to return, state=%d, &state=%p, *(&state)=%d\n",
//           ctx->state, (void *)&ctx->state, *(uint8_t *)&ctx->state);

//     /* Force memory barrier */
//     // __asm__ volatile("" ::: "memory");

//     return ZM_OK;
// }
