/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal stub: RAM load is not enabled for this port.
 */

#include "mcuboot_config/mcuboot_config.h"

#if defined(MCUBOOT_RAM_LOAD)
#error "MCUBOOT_RAM_LOAD not supported in this port."
#endif
