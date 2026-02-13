// #include "main.h"
// #include "shell.h"
// #include "upgrade.h"
// #include "usart.h"
// #include "zmodem.h"
// #include <string.h>
// extern int rt_kprintf(const char *fmt, ...);
// // #define LOG_D(...) rt_kprintf(__VA_ARGS__)
// #define LOG_D(...)
// /* ZMODEM upgrade context */
// static zmodem_ctx_t zmodem_ctx;
// static uint8_t zmodem_buffer[1024];
// static uint8_t data_buffer[1024];

// extern uint32_t swap_4bytes(uint32_t value);
// extern uint32_t tick_get(void);
// /* ZMODEM file receiving state */
// static uint32_t flash_write_pos = 0;
// static uint32_t total_bytes_written = 0;

// #include "SEGGER_RTT.h"
// extern uint8_t super_mode;

// static int flash_write_flush(uint8_t *buf, uint16_t *len)
// {
//     uint16_t write_len;

//     if (*len < 2)
//     {
//         return ZM_OK;
//     }

//     /* onchip_flash_write() requires even size */
//     write_len = (uint16_t)(*len & (uint16_t)~1U);
//     rt_kprintf("zmodem write flush: %d bytes\n", write_len);
//     // if (onchip_flash_write(APP_OFFSET + flash_write_pos, buf, write_len) != (int)write_len)
//     // {
//     //     return ZM_ERROR;
//     // }

//     flash_write_pos += write_len;
//     total_bytes_written += write_len;

//     /* Keep odd tail byte for next flush or finalization */
//     if (*len != write_len)
//     {
//         buf[0] = buf[*len - 1];
//         *len = 1;
//     }
//     else
//     {
//         *len = 0;
//     }

//     return ZM_OK;
// }

// static int flash_write_finalize_odd(uint8_t *buf, uint16_t *len)
// {
//     if (*len != 1)
//     {
//         return ZM_OK;
//     }

//     /* If the sender's file size is odd, flush the last byte padded with 0xFF. */
//     if (total_bytes_written + 1 == zmodem_ctx.file_size)
//     {
//         uint8_t last_halfword[2] = { buf[0], 0xFF };
//         rt_kprintf("zmodem write odd\n");
//         // if (onchip_flash_write(APP_OFFSET + flash_write_pos, last_halfword, 2) != 2)
//         // {
//         //     return ZM_ERROR;
//         // }

//         flash_write_pos += 2;
//         total_bytes_written += 1;
//         *len = 0;
//     }

//     return ZM_OK;
// }

// uint32_t zmodem_read(uint8_t *buf, uint16_t len)
// {
//     return usart_read(buf, len);
// }

// void zmodem_write(uint8_t *buf, uint16_t len)
// {
//     usart_write((uint8_t *)buf, len);
// }

// /**
//  * @brief MSH command: Start ZMODEM receive
//  * Usage: rz
//  */
// void rz(int argc, char **argv)
// {
//     int ret;
//     uint8_t ch;
//     uint16_t data_idx = 0;
//     uint8_t subpkt_type = 0;
//     uint32_t start_tick = 0;
//     uint8_t receiving_data = 0;

//     super_mode = 2;
//     LOG_D("\nZMODEM receive ready. Please start sending file...\n");

//     zmodem_init(&zmodem_ctx, zmodem_buffer, sizeof(zmodem_buffer));

//     /* Erase application flash area */
//     LOG_D("Erasing flash...\n");
//     // onchip_flash_erase(FLASH_APP_ADDR, 128 * 1024);  /* Erase 128KB */
//     LOG_D("Flash erased.\n");

//     flash_write_pos = 0;
//     total_bytes_written = 0;

//     zmodem_ctx_t *p_ctx = &zmodem_ctx;

//     ret = zmodem_start_receive(p_ctx);

//     start_tick = tick_get();

//     uint8_t data_state_entered = 0;

//     /* Main receive loop */
//     while (1)
//     {
//         /* Timeout check - allow user time to pick a file in Tabby before ZFILE arrives */
//         if (tick_get() - start_tick > 10000)
//         {
//             LOG_D("\nTimeout waiting for data\n");
//             break;
//         }

//         /* Process ZMODEM protocol (skip when receiving data) */
//         if (zmodem_ctx.state != ZM_STATE_RECEIVING_DATA)
//         {
//             ret = zmodem_process(&zmodem_ctx);
//         }
//         else
//         {
//             /* In data receiving state - print once */
//             if (!data_state_entered)
//             {
//                 LOG_D("[MAIN] Skipping zmodem_process() - in RECEIVING_DATA state\n");
//                 data_state_entered = 1;
//             }
//         }

//         if (zmodem_ctx.state == ZM_STATE_COMPLETE)
//         {
//             rt_kprintf("\n\nTransfer complete!\n");
//             rt_kprintf("Total bytes: %d\n", total_bytes_written);
//             if (check_fireware(APP_ADDRESS, total_bytes_written) == 0)
//             {
//                 rt_kprintf("CRC check passed.\n");
//                 flash_set_value("file_size", &total_bytes_written, 4);
//                 super_mode = 0;
//                 do_safe_boot(1000);
//                 return;
//             }
//             break;
//         }

//         if (zmodem_ctx.state == ZM_STATE_ERROR || ret == ZM_CANCELLED)
//         {
//             rt_kprintf("\nTransfer failed or cancelled\n");
//             break;
//         }

//         /* Receive data when in data receiving state */
//         if (zmodem_ctx.state == ZM_STATE_RECEIVING_DATA)
//         {
//             if (!receiving_data)
//             {
//                 LOG_D("\n=== START RECEIVING DATA ===\n");
//                 receiving_data = 1;
//                 data_idx = 0;
//                 data_state_entered = 0;
//             }

//             /* Read data from UART - continuously read all available data */
//             int bytes_in_subpacket = 0;
//             while (zmodem_read(&ch, 1) == 1)
//             {
//                 bytes_in_subpacket++;
//                 start_tick = tick_get(); /* Reset timeout */
//                 zmodem_ctx.last_activity = start_tick;

//                 /* Check for ZDLE escape */
//                 if (ch == ZDLE)
//                 {
//                     /* Wait for the escaped byte with timeout */
//                     uint32_t escape_timeout = tick_get() + 1000; /* 1 second timeout */
//                     while (zmodem_read(&ch, 1) != 1)
//                     {
//                         if (tick_get() >= escape_timeout)
//                         {
//                             LOG_D("ERROR: ZDLE escape timeout\n");
//                             zmodem_ctx.state = ZM_STATE_ERROR;
//                             goto exit_data_loop;
//                         }
//                         /* Small delay to avoid busy wait */
//                         for (volatile int i = 0; i < 100; i++)
//                         {
//                         }
//                     }
//                     start_tick = tick_get(); /* Reset timeout after successful read */
//                     zmodem_ctx.last_activity = start_tick;

//                     if (ch == ZRUB0 || ch == ZRUB1)
//                     {
//                         ch = 0x7F;
//                     }
//                     else if (ch == ZCRCE || ch == ZCRCG || ch == ZCRCQ || ch == ZCRCW)
//                     {
//                         /* Subpacket terminator found */
//                         subpkt_type = ch;

//                         /* Read CRC16 (2 bytes) with ZDLE escaping and timeout */
//                         uint8_t crc_bytes[2];
//                         uint32_t crc_timeout = tick_get() + 1000;

//                         /* Read CRC byte 0 */
//                         while (zmodem_read(&crc_bytes[0], 1) != 1)
//                         {
//                             if (tick_get() >= crc_timeout)
//                             {
//                                 LOG_D("ERROR: CRC byte 0 timeout\n");
//                                 goto exit_data_loop;
//                             }
//                         }
//                         if (crc_bytes[0] == ZDLE)
//                         {
//                             while (zmodem_read(&crc_bytes[0], 1) != 1)
//                             {
//                                 if (tick_get() >= crc_timeout)
//                                     goto exit_data_loop;
//                             }
//                             crc_bytes[0] ^= 0x40;
//                         }

//                         /* Read CRC byte 1 */
//                         while (zmodem_read(&crc_bytes[1], 1) != 1)
//                         {
//                             if (tick_get() >= crc_timeout)
//                             {
//                                 LOG_D("ERROR: CRC byte 1 timeout\n");
//                                 goto exit_data_loop;
//                             }
//                         }
//                         if (crc_bytes[1] == ZDLE)
//                         {
//                             while (zmodem_read(&crc_bytes[1], 1) != 1)
//                             {
//                                 if (tick_get() >= crc_timeout)
//                                     goto exit_data_loop;
//                             }
//                             crc_bytes[1] ^= 0x40;
//                         }

//                         /* Write remaining data to flash */
//                         if (data_idx > 0)
//                         {
//                             if (flash_write_flush(data_buffer, &data_idx) != ZM_OK ||
//                                 flash_write_finalize_odd(data_buffer, &data_idx) != ZM_OK)
//                             {
//                                 LOG_D("ERROR: flash write failed\n");
//                                 zmodem_ctx.state = ZM_STATE_ERROR;
//                                 goto exit_data_loop;
//                             }
//                             LOG_D("\rReceived: %d bytes (subpkt=%d bytes)", total_bytes_written, bytes_in_subpacket);
//                         }

//                         /* Send ACK if required by subpacket type */
//                         if (subpkt_type == ZCRCW)
//                         {
//                             /* Wait-for-ACK: sender waits for ZACK before continuing */
//                             LOG_D(" [ACK sent]\n");
//                             zmodem_send_hex_header(ZACK, total_bytes_written);
//                         }
//                         else if (subpkt_type == ZCRCQ)
//                         {
//                             /* Quick ACK: sender continues but expects ACK */
//                             LOG_D(" [Quick ACK]\n");
//                             zmodem_send_hex_header(ZACK, total_bytes_written);
//                         }
//                         else if (subpkt_type == ZCRCG)
//                         {
//                             /* Go-on: sender continues immediately, no ACK needed */
//                             LOG_D(" [Go-on]\n");
//                         }
//                         else if (subpkt_type == ZCRCE)
//                         {
//                             /* End of frame */
//                             LOG_D(" [End of frame]\n");
//                             zmodem_ctx.state = ZM_STATE_WAIT_ZEOF;
//                             zmodem_ctx.last_activity = tick_get();
//                             receiving_data = 0;
//                             data_state_entered = 0; /* Reset for next file */
//                         }

//                         /* Reset counter for next subpacket */
//                         bytes_in_subpacket = 0;

//                         /* Continue reading - there may be more subpackets */
//                         if (subpkt_type == ZCRCE)
//                         {
//                             break;
//                         }
//                         continue;
//                     }
//                     else
//                     {
//                         /* Regular ZDLE escape: XOR with 0x40 */
//                         ch ^= 0x40;
//                     }
//                 }

//                 /* Store data in buffer */
//                 if (data_idx < sizeof(data_buffer))
//                 {
//                     data_buffer[data_idx++] = ch;
//                 }
//                 else
//                 {
//                     LOG_D("ERROR: data buffer overflow\n");
//                 }

//                 /* Flush buffer if full */
//                 if (data_idx >= sizeof(data_buffer))
//                 {
//                     if (flash_write_flush(data_buffer, &data_idx) != ZM_OK)
//                     {
//                         LOG_D("ERROR: flash write failed\n");
//                         zmodem_ctx.state = ZM_STATE_ERROR;
//                         goto exit_data_loop;
//                     }
//                     LOG_D("\rReceived: %d bytes (buffer full)", total_bytes_written);
//                 }
//             }

//         exit_data_loop:
//             /* Exit point for error handling in data reception */
//             if (zmodem_ctx.state == ZM_STATE_ERROR)
//             {
//                 LOG_D("\nData reception error\n");
//             }
//         }

//         /* Small delay to prevent busy loop */
//         for (volatile int i = 0; i < 1000; i++)
//         {
//         }
//     }

//     super_mode = 1;
//     rt_kprintf("\nZMODEM receive finished.\n");
// }
// MSH_CMD_EXPORT(rz, ZMODEM receive file);
