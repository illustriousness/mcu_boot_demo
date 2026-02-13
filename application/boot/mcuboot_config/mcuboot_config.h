#ifndef MCUBOOT_CONFIG_H_
#define MCUBOOT_CONFIG_H_

#include "boot_port.h"

/* Minimal footprint: ECDSA P-256 + SHA-256, no encryption. */
#define MCUBOOT_USE_TINYCRYPT     1
#define MCUBOOT_SIGN_EC256        1

/* Image management: direct-XIP across slots (app is relocatable). */
#define MCUBOOT_DIRECT_XIP        1
/* Keep revert logic in custom boot watchdog state machine. */
/* #define MCUBOOT_DIRECT_XIP_REVERT 1 */
#define MCUBOOT_VALIDATE_PRIMARY_SLOT 1

/* Single-image setup. */
#define MCUBOOT_IMAGE_NUMBER      1

/* Flash and sector sizing. */
#define MCUBOOT_BOOT_MAX_ALIGN    8
#define MCUBOOT_MAX_IMG_SECTORS   64
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS 1

#define MCUBOOT_HAVE_LOGGING
#define MCUBOOT_WATCHDOG_FEED() do {} while (0)

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#ifndef _Static_assert
#define _Static_assert(cond, msg) typedef char static_assertion_##__LINE__[(cond) ? 1 : -1]
#endif
#endif

#endif /* MCUBOOT_CONFIG_H_ */
