// #include <fal.h>
// #include "fal_def.h"
#include "main.h"


#define DBG_TAG "rom"
#define DBG_LVL DBG_NONE

#include "rtdbg.h"
/*
STM32F1会因容量不同而不同
    小容量和中容量产品主存储块128KB以下，  每页1KB。
    大容量和互联型产品主存储块256KB以上，  每页2KB。

GD32   会因容量不同而不同
    1. Low-density Products     Flash容量从 16KB到  32KB的产品
    2. Medium-density Products  Flash容量从 64KB到 128KB的产品
          全是1K
    3. High-density Products    Flash容量从256KB到 512KB的产品
          全是2K
    4. XL-density Products      Flash容量从768KB到3072KB的产品
          <512K 是2K
          >512K 是4K

雅特力
    全是2K

STM32F4
    STM32F4的flash页尺寸不一样，低地址16KB，高地址32KB或128KB.
*/

#define FLASH_SECTOR_SIZE (2048)
#define FLASH_START_ADDR  FLASH_BASE
#define FLASH_END_ADDR    (FLASH_BASE + 256 * 1024)

int onchip_flash_read(uint32_t offset, uint8_t *buf, uint32_t size)
{
    uint32_t addr = FLASH_START_ADDR + offset;
    // FAL_ASSERT(addr < FLASH_END_ADDR);
    // FAL_ASSERT(addr >= FLASH_START_ADDR);
    rt_memcpy(buf, (const void *)addr, size);
    return size;
}

// int onchip_flash_write(uint32_t offset, const uint8_t *buf, uint32_t size)
// {
//     uint32_t addr = FLASH_START_ADDR + offset;
//     int result = 0;

//     if (size == 0)
//     {
//         return 0;
//     }

//     fmc_unlock();

//     /* 处理非2字节对齐的起始地址 */
//     if (offset % 2 != 0)
//     {
//         /* 分离出第一个字节 */
//         uint32_t tmp = addr - 1;
//         uint16_t original = *(volatile uint16_t *)tmp;
//         uint16_t write_data = (original & 0xFF) | (buf[0] << 8);
//         if (fmc_halfword_program(tmp, write_data) != FMC_READY)
//         {
//             LOG_E("Program failed @ 0x%08lX", tmp);
//             result = -2;
//             goto finish;
//         }

//         /* 验证写入 */
//         if (*(volatile uint16_t *)tmp != write_data)
//         {
//             LOG_E("Verify failed @ 0x%08lX", tmp);
//             result = -3;
//             goto finish;
//         }

//         /* 更新地址和缓冲区 */
//         addr += 1;
//         buf += 1;
//         size -= 1;
//     }

//     /* 处理剩余的半字对齐数据 */
//     for (uint32_t i = 0; i < size; i += 2)
//     {
//         uint16_t write_data;

//         if (i == size - 1)
//         {
//             /* 处理最后一个字节 */
//             uint8_t last_byte = buf[i];
//             uint16_t original = *(volatile uint16_t *)(addr + i);
//             write_data = (original & 0xFF00) | last_byte;
//         }
//         else
//         {
//             /* 按半字处理 */
//             write_data = *(uint16_t *)(buf + i);
//         }

//         /* 编程操作 */
//         if (fmc_halfword_program(addr + i, write_data) != FMC_READY)
//         {
//             LOG_E("Program failed @ 0x%08lX", addr + i);
//             result = -2;
//             goto finish;
//         }

//         /* 验证写入 */
//         if (*(volatile uint16_t *)(addr + i) != write_data)
//         {
//             LOG_E("Verify failed @ 0x%08lX", addr + i);
//             result = -3;
//             goto finish;
//         }
//     }

// finish:
//     /* 加锁 Flash */
//     fmc_lock();
//     if (result == 0)
//     {
//         result = size;
//     }
//     return result;
// }

int onchip_flash_write(uint32_t offset, const uint8_t *buf, uint32_t size)
{
    uint32_t addr = FLASH_START_ADDR + offset;

    /* 上层保证 offset 2 字节对齐，size 也保证为偶数 */

    if (size == 0)
        return 0;

    fmc_unlock();

    for (uint32_t i = 0; i < size; i += 2)
    {
        uint16_t halfword = *(const uint16_t *)(buf + i);

        /* Flash 半字编程 */
        if (fmc_halfword_program(addr + i, halfword) != FMC_READY)
        {
            LOG_E("Program failed @ 0x%08lX", addr + i);
            fmc_lock();
            return -2;
        }

        /* 验证 */
        if (*(volatile uint16_t *)(addr + i) != halfword)
        {
            LOG_E("Verify failed @ 0x%08lX", addr + i);
            fmc_lock();
            return -3;
        }
    }

    fmc_lock();
    return (int)size;
}

int onchip_flash_erase(uint32_t offset, uint32_t size)
{
    uint32_t addr_start = FLASH_START_ADDR + offset;
    uint32_t addr_end = addr_start + size;
    int ret = 0;
    LOG_E("erase 0x%x size %d", addr_start, size);
    /* 计算需要擦除的页数 */
    uint32_t sector_num = (addr_end - addr_start) / FLASH_SECTOR_SIZE;

    /* 获取硬件锁 */
    fmc_unlock();

    /* 执行批量擦除 */
    for (uint32_t i = 0; i < sector_num; i++)
    {
        uint32_t cur_addr = addr_start + i * FLASH_SECTOR_SIZE;

        /* 发送页擦除命令 */
        if (fmc_page_erase(cur_addr) != 0)
        {
            LOG_E("Sector erase failed @ 0x%08lX", cur_addr);
            ret = -2;
            break;
        }

        // /* 验证擦除结果（全FF） */
        // for (uint32_t check = 0; check < FLASH_SECTOR_SIZE; check += 4)
        // {
        //     if (*(volatile uint32_t *)(cur_addr + check) != 0xFFFFFFFF)
        //     {
        //         LOG_E("Verify failed @ 0x%08lX+0x%lX", cur_addr, check);
        //         ret = -3;
        //         goto exit;
        //     }
        // }
    }

// exit:
    fmc_lock();
    if (ret == 0)
    {
        ret = size;
    }
    return ret;
}

void onchip_flash_init(void)
{
    fmc_unlock();
    FMC_WSEN |= FMC_WSEN_BPEN;
    fmc_lock();
}
INIT_BOARD_EXPORT(onchip_flash_init);
