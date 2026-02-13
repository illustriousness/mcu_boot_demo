#ifndef FWDG_H_
#define FWDG_H_

#include <stdint.h>

int bsp_wdt_control(int cmd, void *arg);
void bsp_wdt_init(void);

void clear_app_invalid_flag(void);
int8_t app_is_invalid(void);

#endif /* FWDG_H_ */
