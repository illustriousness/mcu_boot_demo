#ifndef BOOT_PARAM_H_
#define BOOT_PARAM_H_

#include <stdint.h>

/* Shared flash layout fallback values (kept aligned with boot_port.h). */
#ifndef BOOT_FLASH_BASE
#define BOOT_FLASH_BASE            0x08000000u
#endif
#ifndef BOOT_PRIMARY_SLOT_OFFSET
#define BOOT_PRIMARY_SLOT_OFFSET   0x7800u
#endif
#ifndef BOOT_PRIMARY_SLOT_SIZE
#define BOOT_PRIMARY_SLOT_SIZE     0x1B800u
#endif
#ifndef BOOT_SECONDARY_SLOT_OFFSET
#define BOOT_SECONDARY_SLOT_OFFSET 0x23000u
#endif
#ifndef BOOT_SECONDARY_SLOT_SIZE
#define BOOT_SECONDARY_SLOT_SIZE   0x1B800u
#endif
#ifndef BOOT_PARAM_OFFSET
#define BOOT_PARAM_OFFSET          0x3F000u
#endif
#ifndef BOOT_PARAM_SIZE
#define BOOT_PARAM_SIZE            0x1000u
#endif

#define BOOT_SLOT_0                0u
#define BOOT_SLOT_1                1u
#define BOOT_SLOT_INVALID          0xFFFFFFFFu

#define BOOT_PARAM_MAGIC           0x4250524Du /* "BPRM" */
#define BOOT_PARAM_VERSION         1u

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t active_slot;
    uint32_t trial_slot;
    uint32_t confirmed;
    uint32_t trial_boot_count;
    uint32_t last_boot_slot;
    uint32_t reserved0;
    uint32_t crc32;
} boot_param_t;

void boot_param_get_default(boot_param_t *param);
int boot_param_load(boot_param_t *param);
int boot_param_save(const boot_param_t *param);

int boot_slot_is_valid(uint32_t slot);
uint32_t boot_slot_addr(uint32_t slot);

#endif /* BOOT_PARAM_H_ */
