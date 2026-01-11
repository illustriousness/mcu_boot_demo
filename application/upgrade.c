#include "core_cm4.h"
#include "core_cmFunc.h"
#include "main.h"
#include "rtm.h"
#include "shell.h"
#include "upgrade.h"
#include "flashdb.h"
#include "fdb_low_lvl.h"
// #define LOG_D(...) rt_kprintf(__VA_ARGS__)
// #define LOG_D(...)

/* FlashDB keys for firmware info */
#define FW_INFO_KEY_LEN   "fw_len"
#define FW_INFO_KEY_CRC   "fw_crc32"
#define FW_INFO_KEY_VALID "fw_valid"
// #define APP_ADDRESS        0x0800C800             // 应用程序的起始地址  0x0802 5800
// #define NVIC_VECTTAB_FLASH ((uint32_t)0x08000000) /*!< Flash base address */


enum can_upgrade_state
{
    CAN_UPGRADE_IDLE = 0,
    // CAN_UPGRADE_WAIT_START,
    CAN_UPGRADE_START,
    CAN_UPGRADE_DONE = 10,
    CAN_UPGRADE_ETIMEOUT = 0Xff,
    // CAN_UPGRADE_START,
    // CAN_UPGRADE_WAIT_PKG_DONE,
};
#pragma pack(1)
typedef struct can_upgrade_pkg
{
    uint8_t head;
    uint8_t cmd;
    uint16_t pkg_len;
    uint8_t data[1024];
    uint8_t sum;
    uint8_t end;
    uint16_t pkg_num;
} *can_upgrade_pkg_t;
#pragma pack()
static uint8_t can_pkg[sizeof(struct can_upgrade_pkg)];

/* can upgrade

*/

/**
 * @brief
wait 500ms
soh frame
recv: head(0xA5) cmd(0X01) total_len(u32) sum(u8) end(0xED)
if chk ok
    ack "btl"
    flash erase app
    ack "erase"

data frame
recv: head(0xA5) cmd(0X11) pkg_len(msb u16) data(1024) sum(u8) end(0xED) pkg_num(u16) // 1+1+2+1024+1+1+2 = 1032
    if chk ok
        flash write
        ack "ok"
    else
        ack "dataerr"
    if recv_pkg_len = len
        ack "crc+CRC" //3+4=7
 * @return uint8_t @arg enum can_upgrade_state
 */
uint8_t app_can_upgrade(void)
{
    static uint32_t total_len;
    static uint32_t recv_len;
    static uint32_t recv_pkg_tick;
    static uint16_t recv_pkg_len;
    static uint16_t recv_pkg_num;

    static enum can_upgrade_state state;

    can_receive_message_struct msg;
    uint8_t pkg_sum;
    can_upgrade_pkg_t pkg = (can_upgrade_pkg_t)can_pkg;
    if (tick_get() > 500 * 1 && state == CAN_UPGRADE_IDLE)
    {
        LOG_D("bl timeout\n");
        state = CAN_UPGRADE_ETIMEOUT;
    }
    if (state == CAN_UPGRADE_START && tick_get() - recv_pkg_tick > 10000)
    {
        state = CAN_UPGRADE_ETIMEOUT;
        LOG_D("recv timeout at %d\n", recv_pkg_tick);
    }
    if (can_read(&msg) == sizeof(msg))
    {
        uint8_t send_version[8] = { 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xcb };
        uint8_t rx_version_cmd[8] = { 0XEE, 0X03, 0X00, 0X01, 0X00, 0X00, 0XFF, 0XFC };
        uint8_t rx_version_new_cmd[8] = { 0x01, 0x00, 0x0, 0x0, 0x0, 0x0, 0x00, 0xdf };
        if (msg.rx_sfid == 0x113)
        {
            if (!rt_memcmp(msg.rx_data, rx_version_cmd, 8) ||
                !rt_memcmp(msg.rx_data, rx_version_new_cmd, 8))
            {
                LOG_D("sned version\n");
                can_write(0x131, send_version, 8);
            }
        }

        if ((state == CAN_UPGRADE_IDLE || state == CAN_UPGRADE_ETIMEOUT) && msg.rx_sfid == 0x113)
        {
            recv_pkg_tick = tick_get();
            if (msg.rx_data[0] == 0xA5 && msg.rx_data[1] == 0x01 && msg.rx_data[7] == 0xED)
            {
                pkg_sum = 0;
                for (int i = 0; i < 1 + 1 + sizeof(total_len); i++)
                {
                    pkg_sum += msg.rx_data[i];
                }
                if (pkg_sum == msg.rx_data[sizeof(total_len) + 2])
                {
                    can_write(0x141, "btl", 3);

                    // Reset state variables for new upgrade session
                    recv_len = 0;
                    recv_pkg_num = 0;
                    recv_pkg_len = 0;

                    total_len = *((uint32_t *)&msg.rx_data[2]);
                    // total_len = swap_4bytes(total_len);
                    onchip_flash_erase(FLASH_APP_ADDR, total_len + (2048 - 1) & ~(2048 - 1));
                    LOG_D("flash erase\n");
                    LOG_D("total len %x\n", total_len);
                    can_write(0x141, "erase", 5);
                    state = CAN_UPGRADE_START;
                }
                else
                {
                    LOG_D("soh sum err.expect %2x\n", pkg_sum);
                }
            }
        }
        else if (state == CAN_UPGRADE_START && msg.rx_sfid == 0x114)
        {
            recv_pkg_tick = tick_get();
            rt_memcpy(&can_pkg[recv_pkg_len], msg.rx_data, msg.rx_dlen);
            recv_pkg_len += msg.rx_dlen;
            // LOG_D("recv len %x\n", recv_pkg_len);
            if (recv_pkg_len >= sizeof(struct can_upgrade_pkg))
            {
                recv_pkg_len = 0;
                int res = 0;
                do
                {
                    /*1. head check */
                    if (pkg->head == 0xA5 && pkg->cmd == 0x11 && pkg->end == 0xED)
                    {
                    }
                    else
                    {
                        res = -1;
                        LOG_D("pkg err head %2x cmd %2x end %2x\n", pkg->head, pkg->cmd, pkg->end);
                        break;
                    }
                    /*2. sum check */
                    pkg_sum = 0;
                    uint32_t member_offset = ((uint32_t) & ((can_upgrade_pkg_t)0)->sum);
                    for (int i = 0; i < member_offset; i++)
                    {
                        pkg_sum += can_pkg[i];
                    }
                    if (pkg_sum == pkg->sum)
                    {
                    }
                    else
                    {
                        res = -2;
                        LOG_D("data err.expect sum %2x \n", pkg_sum);
                        break;
                    }
                    /*3. pkg_num check */
                    recv_pkg_num++;
                    LOG_D("recv_pkg_num %x done\n", recv_pkg_num);
                    // pkg->pkg_num = swap_2bytes(pkg->pkg_num);
                    if (recv_pkg_num != pkg->pkg_num)
                    {
                        res = -3;
                        recv_pkg_num--;
                        LOG_D("data err.expect pkg_num %2x\n", recv_pkg_num);
                        break;
                    }

                } while (0);

                if (res == 0)
                {
                    // pkg->pkg_len = swap_2bytes(pkg->pkg_len);
                    pkg->pkg_len = pkg->pkg_len > sizeof(pkg->data) ? sizeof(pkg->data) : pkg->pkg_len;
                    LOG_D("pkg->pkg_len %x\n", pkg->pkg_len);
                    // flash write
                    onchip_flash_write(FLASH_APP_ADDR + recv_len, pkg->data, pkg->pkg_len);
                    recv_len += pkg->pkg_len;
                    can_write(0x141, "ok", 2);
                    LOG_D("recv_len %d\n", recv_len);

                    if (recv_len >= total_len)
                    {
                        uint8_t buf[8] = "crc";
                        // uint32_t crc32 = fdb_calc_crc32(0, (const void *)(FLASH_APP_ADDR + 0X08000000), total_len);
                        uint32_t crc32 = fdb_calc_crc32(0, (const void *)(FLASH_APP_ADDR + 0X08000000), total_len - 4);
                        uint32_t *file_crc32 = (uint32_t *)(FLASH_APP_ADDR + 0X08000000 + total_len - 4);
                        LOG_D("total len %d recv_len %d\n", total_len, recv_len);
                        LOG_D("recv all data.crc %08x file crc %08x\n", crc32, *file_crc32);
                        if (crc32 == *file_crc32)
                        {
                            /* Save firmware info to FlashDB for future verification */
                            save_firmware_info(total_len - 4, crc32);
                            state = CAN_UPGRADE_DONE;
                        }
                        else
                        {
                            LOG_E("CRC32 mismatch! Firmware corrupted!\n");
                            state = CAN_UPGRADE_ETIMEOUT;
                        }

                        // uint32_t crc32_swapped = swap_4bytes(crc32);
                        uint32_t crc32_swapped = crc32;
                        rt_memcpy(buf + 3, &crc32_swapped, sizeof(uint32_t));

                        can_write(0x141, buf, 7);
                        recv_len = 0;
                    }
                }
                else
                {
                    LOG_D("data err.res %2d \n", res);
                    can_write(0x141, "dataerr", 7);
                }
            }
        }
        else
        {
            LOG_D("unknown msg,id=%x\n", msg.rx_sfid);
        }
    }
    return state;
}

void run_app(int argc, char **argv)
{
    uint32_t app_addr = 0x08000000 + 30 * 1024; // 应用程序的起始地址
    if (argc == 2)
    {
        // app_addr = skip_atoi((const char** )&argv[1]);
        app_addr = rt_atoi(argv[1]);
    }
    LOG_D("run app at 0x%08x\n", app_addr);
    JumpToApplication(app_addr);
}
MSH_CMD_EXPORT(run_app, desc);

/**
 * @brief Save firmware information to FlashDB after upgrade
 * @param length Firmware length in bytes
 * @param crc32 CRC32 checksum
 */
void save_firmware_info(uint32_t length, uint32_t crc32)
{
    extern struct fdb_kvdb kvdb;
    /* Save firmware length */
    flash_set_value(FW_INFO_KEY_LEN, &length, sizeof(length));
    /* Save firmware CRC32 */
    flash_set_value(FW_INFO_KEY_CRC, &crc32, sizeof(crc32));
    /* Set firmware valid flag */
    uint8_t valid = 1;
    flash_set_value(FW_INFO_KEY_VALID, &valid, sizeof(valid));
    LOG_D("Firmware info saved: len=%u, crc32=0x%08X\n", length, crc32);
}

/**
 * @brief Verify application firmware integrity before jumping
 * @return 0 if valid, negative error code otherwise
 */
int verify_app_firmware(void)
{
    extern struct fdb_kvdb kvdb;
    fw_info_t fw_info;
    uint8_t valid = 0;

    /* Check if firmware valid flag exists */
    flash_get_value(FW_INFO_KEY_VALID, &valid, sizeof(valid));
    if (valid != 1)
    {
        LOG_D("No valid firmware flag found\n");
        return FW_VERIFY_NO_INFO;
    }

    /* Read firmware length */
    flash_get_value(FW_INFO_KEY_LEN, &fw_info.length, sizeof(fw_info.length));
    if (fw_info.length == 0 || fw_info.length > (256 * 1024))
    {
        LOG_D("Invalid firmware length: %u\n", fw_info.length);
        return FW_VERIFY_INVALID_LEN;
    }

    /* Read stored CRC32 */
    flash_get_value(FW_INFO_KEY_CRC, &fw_info.crc32, sizeof(fw_info.crc32));
    /* Calculate actual CRC32 from flash */
    uint32_t calculated_crc = fdb_calc_crc32(0, (const void *)(FLASH_APP_ADDR + 0x08000000), fw_info.length);

    LOG_D("Firmware verification:\n");
    LOG_D("  Length: %u bytes\n", fw_info.length);
    LOG_D("  Stored CRC32:     0x%08X\n", fw_info.crc32);
    LOG_D("  Calculated CRC32: 0x%08X\n", calculated_crc);

    /* Compare CRC32 */
    if (calculated_crc != fw_info.crc32)
    {
        LOG_D("CRC32 mismatch! Firmware corrupted!\n");
        return FW_VERIFY_CRC_FAIL;
    }

    LOG_D("Firmware verification PASSED\n");
    return FW_VERIFY_OK;
}

/**
 * @brief Clear firmware valid flag (used when firmware is corrupted)
 */
void clear_firmware_valid(void)
{
    uint8_t valid = 0;
    flash_set_value(FW_INFO_KEY_VALID, &valid, sizeof(valid));
    LOG_D("Firmware valid flag cleared\n");
}

/**
 * @brief MSH command: Manually verify firmware integrity
 */
void fw_verify(int argc, char **argv)
{
    int result = verify_app_firmware();
    if (result)
    {
        rt_kprintf("FAIL - Error code: %d\n", result);
    }
    else
    {
        rt_kprintf("PASS - Firmware is valid and ready to use\n");
    }
}
MSH_CMD_EXPORT(fw_verify, Verify firmware integrity);

/**
 * @brief MSH command: Clear firmware valid flag
 */
void fw_clear(int argc, char **argv)
{
    clear_firmware_valid();
}
MSH_CMD_EXPORT(fw_clear, Clear firmware valid flag);
