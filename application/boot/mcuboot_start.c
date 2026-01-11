#include "mcuboot_start.h"
#include "boot_port.h"
#include "upgrade.h"

#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "bootutil/fault_injection_hardening.h"

#include "main.h"
static void mcuboot_jump(const struct boot_rsp *rsp)
{
    uint32_t image_addr = BOOT_FLASH_BASE + rsp->br_image_off + rsp->br_hdr->ih_hdr_size;
    rt_kprintf("Jump to image at 0x%08lx\n", (unsigned long)image_addr);
    // while (1)
    // {
    // }
    // JumpToApplication(image_addr);
}
#include "finsh.h"
void mcuboot_start(void)
{
    struct boot_rsp rsp;
    fih_ret rc;

    rc = boot_go(&rsp);
    if (FIH_NOT_EQ(rc, FIH_SUCCESS))
    {
        goto exit;
        // while (1)
        // {
        // }
    }

    mcuboot_jump(&rsp);
    return;
exit:
    rt_kprintf("failed to boot (%d)\n", rc);
    // while (1)
    // {
    //     rt_timer_check();
    // }
}
MSH_CMD_EXPORT(mcuboot_start, desc);
