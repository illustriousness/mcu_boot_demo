#ifndef MAIN_H_
#define MAIN_H_
#include "gd32f30x.h"
#include "rtthread.h"
// #include "rtdbg.h"
#if 0
#define LOG_E(...)           \
    rt_kprintf("\033[31m");  \
    rt_kprintf(__VA_ARGS__); \
    rt_kprintf("\033[0m\n")

#define LOG_W(...)           \
    rt_kprintf("\033[33m");  \
    rt_kprintf(__VA_ARGS__); \
    rt_kprintf("\033[0m\n")

#define LOG_I(...)           \
    rt_kprintf("\033[32m");  \
    rt_kprintf(__VA_ARGS__); \
    rt_kprintf("\033[0m\n")

#define LOG_D(...)           \
    rt_kprintf("\033[34m");  \
    rt_kprintf(__VA_ARGS__); \
    rt_kprintf("\033[0m\n")
#endif
void JumpToApplication(uint32_t app_addr);

#define FLASH_APP_ADDR (30 * 1024)
int onchip_flash_read(uint32_t offset, uint8_t *buf, uint32_t size);
int onchip_flash_write(uint32_t offset, const uint8_t *buf, uint32_t size);
int onchip_flash_erase(uint32_t offset, uint32_t size);

int flash_get_value(char *name, void *value, uint8_t len);
int flash_set_value(char *name, void *value, uint8_t len);

uint8_t can_write(uint32_t id, uint8_t *msg, uint8_t len);
uint8_t can_read(can_receive_message_struct *msg);

uint32_t tick_get(void);

uint8_t app_can_upgrade(void);
#endif /* MAIN_H_ */
