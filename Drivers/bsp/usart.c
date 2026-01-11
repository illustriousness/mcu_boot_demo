#include "main.h"
#include "ringbuffer.h"

static struct rt_ringbuffer rb;
static uint8_t rx_buffer[256];  /* Increased to 256 bytes for ZMODEM */
// static void (*rx_indicate)(uint16_t len);

void bsp_usart_init(void)
{
    rt_ringbuffer_init(&rb, rx_buffer, sizeof(rx_buffer));
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

    usart_baudrate_set(USART0, 115200U);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);

    usart_interrupt_enable(USART0, USART_INT_RBNE);
    nvic_irq_enable(USART0_IRQn, 2, 0);
}
INIT_BOARD_EXPORT(bsp_usart_init);
#include "SEGGER_RTT.h"
void bsp_usart_deinit(void)
{
}
void usart_write(uint8_t *buf, uint16_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        usart_data_transmit(USART0, buf[i]);
        while (RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    }
    // SEGGER_RTT_Write(0, buf, len);
}

uint32_t usart_read(uint8_t *buf, uint16_t len)
{
    uint16_t cnt = 0;
    cnt          = rt_ringbuffer_get(&rb, buf, len);
    // if (cnt > 0) {
    //     rt_kprintf("[READ:%d bytes]", cnt);  /* Debug: show when data is read */
    // }
    return cnt;
}

// void usart_set_rx_indicate(void (*callback)(uint16_t len))
// {
//     rx_indicate = callback;
// }

void USART0_IRQHandler(void)
{
    if (RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE)) {
        usart_interrupt_flag_clear(USART0, USART_INT_FLAG_RBNE);
        uint16_t ch = usart_data_receive(USART0);
        // rt_kprintf("rx %x ", ch);  /* IMPORTANT: Comment out in ISR for performance! */
        rt_ringbuffer_putchar(&rb, ch);
        // if (rx_indicate)
        // {
        //     rx_indicate(1);
        // }
    }
}
