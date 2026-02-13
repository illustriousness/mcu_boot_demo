
// #include "main.h"
// #include "ringbuffer.h"
// // #include "bsp_can.h"
// static struct rt_ringbuffer rb;
// static can_receive_message_struct rx_buffer[16];
// // static void (*rx_indicate)(uint16_t len);

// static void can_networking_init(void)
// {
//     can_parameter_struct can_parameter     = { 0 };
//     can_filter_parameter_struct can_filter = { 0 };
//     /* initialize CAN */
//     can_parameter.working_mode          = CAN_NORMAL_MODE;
//     can_parameter.resync_jump_width     = CAN_BT_SJW_1TQ;
//     can_parameter.time_segment_1        = CAN_BT_BS1_7TQ;
//     can_parameter.time_segment_2        = CAN_BT_BS2_2TQ;
//     can_parameter.time_triggered        = DISABLE;
//     can_parameter.auto_bus_off_recovery = ENABLE;
//     can_parameter.auto_wake_up          = DISABLE;
//     can_parameter.no_auto_retrans       = ENABLE;
//     can_parameter.rec_fifo_overwrite    = DISABLE;
//     can_parameter.trans_fifo_order      = DISABLE;
//     can_parameter.prescaler             = 6; /*1M*/
//     can_init(CAN0, &can_parameter);

//     can_filter.filter_number      = 0;
//     can_filter.filter_mode        = CAN_FILTERMODE_MASK;
//     can_filter.filter_bits        = CAN_FILTERBITS_32BIT;
//     can_filter.filter_list_high   = 0x0000;
//     can_filter.filter_list_low    = 0x0000;
//     can_filter.filter_mask_high   = 0x0000;
//     can_filter.filter_mask_low    = 0x0000;
//     can_filter.filter_fifo_number = CAN_FIFO0;
//     can_filter.filter_enable      = ENABLE;
//     can_filter_init(&can_filter);
// }

// static void can_gpio_config(void)
// {
//     /* enable CAN clock */
//     rcu_periph_clock_enable(RCU_CAN0);

//     rcu_periph_clock_enable(RCU_GPIOA);
//     rcu_periph_clock_enable(RCU_AF);

//     /* configure CAN0 GPIO */
//     gpio_init(GPIOA, GPIO_MODE_IPU, GPIO_OSPEED_50MHZ, GPIO_PIN_11);
//     gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12);
//     //    gpio_pin_remap_config(GPIO_CAN_FULL_REMAP,ENABLE);
// }

// void USBD_LP_CAN0_RX0_IRQHandler(void)
// {
//     can_receive_message_struct receive_message;
//     can_message_receive(CAN0, CAN_FIFO0, &receive_message);
//     rt_ringbuffer_put(&rb, ( const uint8_t* )&receive_message, sizeof(receive_message));
// }


// uint8_t can_write(uint32_t id, void* msg, uint8_t len)
// {
//     can_trasnmit_message_struct TxMessage = { 0 };
//     if (id > 0x7FF) {
//         TxMessage.tx_efid = id;
//         TxMessage.tx_ff   = CAN_FF_EXTENDED;
//     }
//     else {
//         TxMessage.tx_sfid = id;
//         TxMessage.tx_ff   = CAN_FF_STANDARD;
//     }
//     TxMessage.tx_ft   = CAN_FT_DATA;
//     TxMessage.tx_dlen = len;
//     rt_memcpy(TxMessage.tx_data, msg, len);
//     // can_message_transmit(CAN0, &TxMessage);

//     uint8_t mbox = can_message_transmit(CAN0, &TxMessage);
//     uint32_t i   = 0;
//     can_transmit_state_enum state;
//     while (1) {
//         state = can_transmit_states(CAN0, mbox);
//         if (state == CAN_TRANSMIT_OK) {
//             break;
//         }
//         if (i++ > 0XFFF || state == CAN_TRANSMIT_FAILED) {
//             return 1;
//         }
//     }
//     // while ((can_transmit_states(CAN0, mbox) == CAN_TRANSMIT_FAILED) && (i < 0XFFF)) i++;  // 等待发送结束
//     // if (i >= 0XFFF)
//     //     return 1;
//     return 0;
// }

// uint8_t can_read(can_receive_message_struct* msg)
// {
//     return rt_ringbuffer_get(&rb, ( uint8_t* )msg, sizeof(can_receive_message_struct));
// }

// void bsp_can_init(void)
// {
//     rt_ringbuffer_init(&rb, ( uint8_t* )rx_buffer, sizeof(rx_buffer));

//     can_gpio_config();
//     can_networking_init();
//     can_interrupt_enable(CAN0, CAN_INT_RFNE0);
//     nvic_irq_enable(USBD_LP_CAN0_RX0_IRQn, 0, 0);
//     // can_interrupt_enable(CAN0, CAN_INT_TME);
//     // nvic_irq_enable(USBD_HP_CAN0_TX_IRQn, 0, 0);
// }
// #include "rtthread.h"
// INIT_BOARD_EXPORT(bsp_can_init);