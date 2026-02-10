#ifndef BOOT_PORT_H_
#define BOOT_PORT_H_

/* Flash layout for MCUboot on GD32F30x (base 0x08000000). */
#define BOOT_FLASH_BASE            0x08000000u
#define BOOT_FLASH_SIZE            (256u * 1024u)
#define BOOT_FLASH_SECTOR_SIZE     2048u

/* RAM layout (48KB SRAM: 0x20000000 ~ 0x2000BFFF). */
#define BOOT_SRAM_BASE             0x20000000u
#define BOOT_SRAM_SIZE             0x0000C000u

/* Shared RAM contract between boot and app. */
#define BOOT_APP_VTOR_RAM_BASE     0x2000BE00u
#define BOOT_APP_VTOR_RAM_SIZE     0x0200u

#define BOOTLOADER_OFFSET          0x0000u
#define BOOTLOADER_SIZE            0x7800u

#define BOOT_PRIMARY_SLOT_OFFSET   0x7800u
#define BOOT_PRIMARY_SLOT_SIZE     0x1B800u

#define BOOT_SECONDARY_SLOT_OFFSET 0x23000u
#define BOOT_SECONDARY_SLOT_SIZE   0x1B800u

#define BOOT_SCRATCH_OFFSET        0x3E800u
#define BOOT_SCRATCH_SIZE          0x0800u

#define BOOT_PARAM_OFFSET          0x3F000u
#define BOOT_PARAM_SIZE            0x1000u

#endif /* BOOT_PORT_H_ */
