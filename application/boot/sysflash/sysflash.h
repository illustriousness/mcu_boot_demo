#ifndef SYSFLASH_H_
#define SYSFLASH_H_

#include <mcuboot_config/mcuboot_config.h>
#include <flash_map/flash_map.h>

#if (MCUBOOT_IMAGE_NUMBER == 1)
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ? FLASH_AREA_IMAGE_0 : FLASH_AREA_IMAGE_0)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ? FLASH_AREA_IMAGE_1 : FLASH_AREA_IMAGE_1)
#elif (MCUBOOT_IMAGE_NUMBER == 2)
#define FLASH_AREA_IMAGE_PRIMARY(x)    (((x) == 0) ? FLASH_AREA_IMAGE_0 : \
                                        ((x) == 1) ? FLASH_AREA_IMAGE_2 : 255)
#define FLASH_AREA_IMAGE_SECONDARY(x)  (((x) == 0) ? FLASH_AREA_IMAGE_1 : \
                                        ((x) == 1) ? FLASH_AREA_IMAGE_3 : 255)
#else
#error "Unsupported MCUBOOT_IMAGE_NUMBER"
#endif

#endif /* SYSFLASH_H_ */
