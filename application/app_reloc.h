#ifndef APP_RELOC_H_
#define APP_RELOC_H_

#include <stdint.h>

struct app_exec_context
{
    uint32_t msp;
    uint32_t vtor;
    uint32_t entry;
    uint32_t pic_base;
};

int app_prepare_exec(uint32_t app_addr, struct app_exec_context *ctx);

#endif /* APP_RELOC_H_ */
