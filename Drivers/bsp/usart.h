#pragma once
#include <stdint.h>
void usart_init(void);
void usart_write(uint8_t *buf, uint16_t len);
uint32_t usart_read(uint8_t *buf, uint16_t len);
void usart_set_rx_indicate(void (*callback)(uint16_t len));