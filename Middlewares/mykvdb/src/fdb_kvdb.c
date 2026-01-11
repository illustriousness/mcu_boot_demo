/**
 * @file kvdb.c
 * @author Yucai Liu (1486344514@qq.com)
 * @brief 
 * @version 1.1
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * TODO: 
 feat:
    提供init失败 reset函数  
    规范检查扇区有效性
    使用区域变化需要reset
    可用范围使用变量
    不应该变化的变量应该声明为const 或者加入magic占位符 调用时检查
 */
#include "fdb_cfg.h"
#include "fdb_def.h"
#include "fdb_low_lvl.h"
#include "flashdb.h"
#include <stdint.h>

#define KV_STATUS_TABLE_SIZE FDB_STATUS_TABLE_SIZE(FDB_KV_STATUS_NUM)

/* magic word(`F`, `D`, `B`, `0`) */
#define SECTOR_MAGIC_WORD 0x30424446
/* magic word(`K`, `V`, `0`, `0`) */
#define KV_MAGIC_WORD        0x3030564B
#define SECTOR_HDR_DATA_SIZE (FDB_WG_ALIGN(sizeof(struct sector_hdr_data)))
#define KV_HDR_DATA_SIZE     (FDB_WG_ALIGN(sizeof(struct kv_hdr_data)))

#define MEMBER_OFFSET(type, member) ((uint32_t) & ((type *)0)->member)
#define KV_STATUS_MEMBER_OFFSET     MEMBER_OFFSET(struct kv_hdr_data, status)
#define SECTOR_STORE_MEMBER_OFFSET  MEMBER_OFFSET(struct sector_hdr_data, store)
#define SECTOR_DIRTY_MEMBER_OFFSET  MEMBER_OFFSET(struct sector_hdr_data, dirty)

#define KV_MAGIC_OFFSET ((unsigned long)(&((struct kv_hdr_data *)0)->magic))

#define FDB_EOK    0
#define FDB_ERR    1
#define FDB_ENOSPC 2
#define FDB_EINVAL 3
#define FDB_EEMPTY 4
#define FDB_EIO    5
#define FDB_ENOMEM 6

#define FLAG_NEED_FIND  0x01
#define FLAG_NEED_ALLOC 0x02

#define db_sec_size(db)    (((fdb_db_t)db)->sec_size)
#define db_max_size(db)    (((fdb_db_t)db)->max_size)
#define db_oldest_addr(db) (((fdb_db_t)db)->oldest_addr)

#define FAILED_ADDR 0xFFFFFFFF
#if (FDB_BYTE_ERASED == 0xFF)
#define SECTOR_NOT_COMBINED 0xFFFFFFFF
#define SECTOR_COMBINED     0x00000000
#else
#define SECTOR_NOT_COMBINED 0x00000000
#define SECTOR_COMBINED     0xFFFFFFFF
#endif

/* the total remain empty sector threshold before GC */
#ifndef FDB_GC_EMPTY_SEC_THRESHOLD
#define FDB_GC_EMPTY_SEC_THRESHOLD 1
#endif

struct sector_hdr_data
{
    uint32_t magic; /**< magic word (`F`, `D`, `B`, `0`)*/
#if (FDB_WRITE_GRAN == 1)
    uint8_t store; /**< sector store status @see fdb_sector_store_status_t */
    uint8_t dirty; /**< sector dirty status @see fdb_sector_dirty_status_t */
    uint16_t pad;
    uint8_t reserved[8];
// #elif (FDB_WRITE_GRAN == 16) // 4+6*2 ->16
#else
    uint8_t store[FDB_STORE_STATUS_TABLE_SIZE];
    uint8_t dirty[FDB_DIRTY_STATUS_TABLE_SIZE];
#endif
};
typedef struct sector_hdr_data *sector_hdr_data_t;

struct sector_info
{
    // int8_t check_ok;                    /**< sector header check is OK */
    enum fdb_sector_store_status store; /**< enum fdb_sector_store_status */
    enum fdb_sector_dirty_status dirty; /**< */
    // uint32_t addr;                      /**< sector start address */
    // uint32_t remain;                    /**< remain size */
    // uint32_t empty_kv;                  /**< the next empty KV node start address */
};

typedef struct kv_hdr_data
{
    uint32_t magic;                       /**< magic word(`K`, `V`, `0`, `0`) */
    uint8_t status[KV_STATUS_TABLE_SIZE]; /**< KV node status, @see fdb_kv_status_t */
    uint32_t len;       /**< KV node total length (header + name + value), must align by FDB_WRITE_GRAN */
    uint32_t crc32;     /**< crc32(len+ name_crc + data_len + name + value) */
    uint8_t name_len;   /**< name length */
    uint16_t name_crc;  /**< name hash */
    uint32_t value_len; /**< value length */
} *kv_hdr_data_t;

typedef struct
{
    const char *name;
    uint8_t name_len;
    uint16_t name_crc;

    uint32_t old_kv_addr;      /**< old kv start address */
    uint32_t new_kv_addr;      /**< new kv start address */
    struct kv_hdr_data kv_hdr; //可以使用buf代替哦
} kv_ctx_t;

#define FDB_LOG_test(...) _fdb_print(db, __VA_ARGS__)

// #include <stdarg.h>
// static void _fdb_print(fdb_db_t db, const char *fmt, ...)
// {
//     // typedef int (*priv_printf_t)(const char *fmt, ...);
//     // priv_printf_t priv_printf = (priv_printf_t)db->user_data;
//     // priv_printf(fmt);
//     if (!db->printf) {
//         return;
//     }
//     va_list args;
//     va_start(args, fmt);
//     vprintf(fmt, args);
//     va_end(args);
// }

__attribute__((weak)) void print_kv_cb(char *name, uint32_t data_addr, void *value, uint32_t len)
{
    // _fdb_print("kv name:%s\n", name);
    // extern void LOG_HEX(uint32_t offset, uint8_t * buf, uint32_t size);
    // LOG_HEX(data_addr, value, len);
}


void print_status(fdb_db_t db, uint8_t status, uint32_t addr, uint8_t is_kv, uint8_t is_store, uint8_t is_dirty)
{
    // FDB_LOG_test( "kv");
    // if (is_kv)
    // {
    //     switch (status)
    //     {
    //     case FDB_KV_UNUSED:
    //         FDB_PRINT(db, "kv@0x%08x: unused\n", addr);
    //         break;
    //     case FDB_KV_PRE_WRITE:
    //         FDB_PRINT(db, "kv@0x%08x: pre_write\n", addr);
    //         break;
    //     case FDB_KV_WRITE:
    //         FDB_PRINT(db, "kv@0x%08x: write\n", addr);
    //         break;
    //     case FDB_KV_PRE_DELETE:
    //         FDB_PRINT(db, "kv@0x%08x: pre_delete\n", addr);
    //         break;
    //     case FDB_KV_DELETED:
    //         FDB_PRINT(db, "kv@0x%08x: deleted\n", addr);
    //         break;
    //     case FDB_KV_ERR_HDR:
    //         FDB_PRINT(db, "kv@0x%08x: err_hdr\n", addr);
    //     default:
    //         FDB_PRINT(db, "kv@0x%08x: unknown\n", addr);
    //         break;
    //     }
    // }
    // else if (is_store)
    // {
    //     switch (status)
    //     {
    //     case FDB_SECTOR_STORE_UNUSED:
    //         FDB_PRINT(db, "store@0x%08x: unused\n", addr);
    //         break;
    //     case FDB_SECTOR_STORE_EMPTY:
    //         FDB_PRINT(db, "store@0x%08x: empty\n", addr);
    //         break;
    //     case FDB_SECTOR_STORE_USING:
    //         FDB_PRINT(db, "store@0x%08x: using\n", addr);
    //         break;
    //     case FDB_SECTOR_STORE_FULL:
    //         FDB_PRINT(db, "store@0x%08x: full\n", addr);
    //         break;
    //     default:
    //         FDB_PRINT(db, "store@0x%08x: unknown\n", addr);
    //         break;
    //     }
    // }
    // else if (is_dirty)
    // {
    //     switch (status)
    //     {
    //     case FDB_SECTOR_DIRTY_UNUSED:
    //         FDB_PRINT(db, "dirty@0x%08x: unused\n", addr);
    //         break;
    //     case FDB_SECTOR_DIRTY_FALSE:
    //         FDB_PRINT(db, "dirty@0x%08x: false\n", addr);
    //         break;
    //     case FDB_SECTOR_DIRTY_GC:
    //         FDB_PRINT(db, "dirty@0x%08x: gc\n", addr);
    //         break;
    //     case FDB_SECTOR_DIRTY_TRUE:
    //         FDB_PRINT(db, "dirty@0x%08x: true\n", addr);
    //         break;
    //     default:
    //         FDB_PRINT(db, "dirty@0x%08x: unknown\n", addr);
    //         break;
    //     }
    // }
}
#ifdef FDB_KV_USING_CACHE

static void update_kv_cache(fdb_kvdb_t db, uint16_t name_crc, uint32_t addr)
{
    size_t i, empty_index = FDB_KV_CACHE_TABLE_SIZE, min_activity_index = FDB_KV_CACHE_TABLE_SIZE;

    uint16_t min_activity = 0xFFFF;

    for (i = 0; i < FDB_KV_CACHE_TABLE_SIZE; i++)
    {
        if (addr != FDB_DATA_UNUSED)
        {
            /* update the KV address in cache */
            if (db->kv_cache_table[i].name_crc == name_crc)
            {
                db->kv_cache_table[i].addr = addr;
                return;
            }
            else if ((db->kv_cache_table[i].addr == FDB_DATA_UNUSED) && (empty_index == FDB_KV_CACHE_TABLE_SIZE))
            {
                empty_index = i;
            }
            else if (db->kv_cache_table[i].addr != FDB_DATA_UNUSED)
            {
                if (db->kv_cache_table[i].active > 0)
                {
                    db->kv_cache_table[i].active--;
                }
                if (db->kv_cache_table[i].active < min_activity)
                {
                    min_activity_index = i;
                    min_activity = db->kv_cache_table[i].active;
                }
            }
        }
        else if (db->kv_cache_table[i].name_crc == name_crc)
        {
            /* delete the KV */
            db->kv_cache_table[i].addr = FDB_DATA_UNUSED;
            db->kv_cache_table[i].active = 0;
            return;
        }
    }
    /* add the KV to cache, using LRU (Least Recently Used) like algorithm */
    if (empty_index < FDB_KV_CACHE_TABLE_SIZE)
    {
        db->kv_cache_table[empty_index].addr = addr;
        db->kv_cache_table[empty_index].name_crc = name_crc;
        db->kv_cache_table[empty_index].active = FDB_KV_CACHE_TABLE_SIZE;
    }
    else if (min_activity_index < FDB_KV_CACHE_TABLE_SIZE)
    {
        db->kv_cache_table[min_activity_index].addr = addr;
        db->kv_cache_table[min_activity_index].name_crc = name_crc;
        db->kv_cache_table[min_activity_index].active = FDB_KV_CACHE_TABLE_SIZE;
    }
}

/*
 * Get KV info from cache. It's return true when cache is hit.
 */
static bool get_kv_from_cache(fdb_kvdb_t db, const char *name, size_t name_len, uint32_t *addr)
{
    size_t i;
    uint16_t name_crc = (uint16_t)(fdb_calc_crc32(0, name, name_len) >> 16);

    for (i = 0; i < FDB_KV_CACHE_TABLE_SIZE; i++)
    {
        if ((db->kv_cache_table[i].addr != FDB_DATA_UNUSED) && (db->kv_cache_table[i].name_crc == name_crc))
        {
            char saved_name[FDB_KV_NAME_MAX] = { 0 };
            /* read the KV name in flash */
            _fdb_flash_read((fdb_db_t)db, db->kv_cache_table[i].addr + KV_HDR_DATA_SIZE, (uint32_t *)saved_name, FDB_KV_NAME_MAX);
            if (!rt_strncmp(name, saved_name, name_len))
            {
                *addr = db->kv_cache_table[i].addr;
                if (db->kv_cache_table[i].active >= 0xFFFF - FDB_KV_CACHE_TABLE_SIZE)
                {
                    db->kv_cache_table[i].active = 0xFFFF;
                }
                else
                {
                    db->kv_cache_table[i].active += FDB_KV_CACHE_TABLE_SIZE;
                }
                return true;
            }
        }
    }

    return false;
}
#endif /* FDB_KV_USING_CACHE */

int fdb_write_status(fdb_db_t db, uint32_t addr, uint8_t status, int8_t is_kv)
{
    uint8_t status_table[FDB_STATUS_TABLE_SIZE(FDB_KV_STATUS_NUM)];
    uint8_t status_num = FDB_SECTOR_STORE_STATUS_NUM;
    if (is_kv)
    {
        status_num = FDB_KV_STATUS_NUM;
    }
    int res = _fdb_write_status(db, addr, status_table, status_num, status, 1);
    return res;
}

static fdb_err_t write_empty_sector_header(fdb_db_t db, uint32_t sec_addr)
{
    uint8_t buf[SECTOR_HDR_DATA_SIZE];
    sector_hdr_data_t sec_hdr_ptr = (sector_hdr_data_t)buf;
    memset(buf, 0xff, sizeof(buf));
    sec_hdr_ptr->magic = SECTOR_MAGIC_WORD;
    _fdb_set_status((uint8_t *)&sec_hdr_ptr->store, FDB_SECTOR_STORE_STATUS_NUM, FDB_SECTOR_STORE_EMPTY);
    _fdb_set_status((uint8_t *)&sec_hdr_ptr->dirty, FDB_SECTOR_DIRTY_STATUS_NUM, FDB_SECTOR_DIRTY_FALSE);
    return _fdb_flash_write(db, sec_addr, buf, SECTOR_HDR_DATA_SIZE, 1);
}

static int whether_all_FF(uint8_t *buf, uint32_t len)
{
    for (int i = 0; i < len; i++)
    {
        if (buf[i] != 0xff)
        {
            return 0;
        }
    }
    return 1;
}

static uint32_t get_next_sec_addr(fdb_db_t db, uint32_t cur_sec_addr)
{
    uint32_t end_addr = db->max_size;
    uint32_t next_sec_addr;
    if (cur_sec_addr + db->sec_size >= end_addr)
    {
        next_sec_addr = 0;
    }
    else
    {
        next_sec_addr = cur_sec_addr + db->sec_size;
    }
    return next_sec_addr;
}
/**
 * @brief Get the sec info object
 *
 * @param db
 * @param sec_addr
 * @param sec_info if sec_info is null,means only check the sector header
 * @return int
 */
int get_sec_info(fdb_db_t db, uint32_t sec_addr, struct sector_info *sec_info)
{
    struct sector_hdr_data sec_hdr;
    _fdb_flash_read(db, sec_addr, &sec_hdr, sizeof(sec_hdr));
    if (sec_hdr.magic != SECTOR_MAGIC_WORD)
    {
        return -1;
    }
    sec_info->store = (enum fdb_sector_store_status)_fdb_get_status((uint8_t *)&sec_hdr.store, FDB_SECTOR_STORE_STATUS_NUM);
    // print_status(db, sec_info->store, sec_addr, 0, 1, 0);
    sec_info->dirty = (enum fdb_sector_dirty_status)_fdb_get_status((uint8_t *)&sec_hdr.dirty, FDB_SECTOR_DIRTY_STATUS_NUM);
    // print_status(db, sec_info->dirty, sec_addr, 0, 0, 1);
    // sec_info->addr = sec_addr;
    // sec_info->check_ok = 1;
    return 0;
}

uint32_t calc_kv_crc32(kv_hdr_data_t kv_hdr, const char *name, const void *value)
{
    /* crc32(len+ name_crc + data_len + name + value) */
    uint32_t crc32 = 0;
    crc32 = fdb_calc_crc32(crc32, &kv_hdr->len, sizeof(kv_hdr->len));
    crc32 = fdb_calc_crc32(crc32, &kv_hdr->name_crc, sizeof(kv_hdr->name_crc));
    crc32 = fdb_calc_crc32(crc32, &kv_hdr->value_len, sizeof(kv_hdr->value_len));

    crc32 = fdb_calc_crc32(crc32, name, kv_hdr->name_len);
    crc32 = fdb_calc_crc32(crc32, value, kv_hdr->value_len);
    return crc32;
}
/**
 * @brief
 *
 * @param db
 * @param kv_hdr
 * @param kv_hdr_addr 为0表示不检查crc
 * @return int
 */
int check_kv_hdr(fdb_db_t db, kv_hdr_data_t kv_hdr, uint32_t kv_hdr_addr)
{
    int res = -FDB_ERR;
    uint8_t buf[32];

    if (kv_hdr->magic == KV_MAGIC_WORD && kv_hdr->len >= KV_HDR_DATA_SIZE && kv_hdr->len <= db->sec_size)
    {
        kv_hdr_data_t kv_hdr_tmp = (kv_hdr_data_t)buf;
        if (kv_hdr_addr + kv_hdr->len < FDB_ALIGN(kv_hdr_addr, db->sec_size))
        {
        }
        _fdb_flash_read(db, kv_hdr_addr + kv_hdr->len, kv_hdr_tmp, sizeof(*kv_hdr_tmp));
        if (whether_all_FF(buf, KV_HDR_DATA_SIZE) ||
            (kv_hdr_tmp->magic == KV_MAGIC_WORD && kv_hdr_tmp->len < KV_HDR_DATA_SIZE && kv_hdr_tmp->len > db->sec_size))
        {
            uint32_t crc32 = 0;

            crc32 = fdb_calc_crc32(crc32, &kv_hdr->len, sizeof(kv_hdr->len));
            crc32 = fdb_calc_crc32(crc32, &kv_hdr->name_crc, sizeof(kv_hdr->name_crc));
            crc32 = fdb_calc_crc32(crc32, &kv_hdr->value_len, sizeof(kv_hdr->value_len));

            uint16_t size = 0;
            uint8_t cur_size;
            while (size < kv_hdr->name_len)
            {
                cur_size = (kv_hdr->name_len - size) > sizeof(buf) ? sizeof(buf) : (kv_hdr->name_len - size);
                _fdb_flash_read(db, kv_hdr_addr + KV_HDR_DATA_SIZE + size, buf, cur_size);
                crc32 = fdb_calc_crc32(crc32, buf, cur_size);
                size += cur_size;
            }

            size = 0;
            while (size < kv_hdr->value_len)
            {
                cur_size = (kv_hdr->value_len - size) > sizeof(buf) ? sizeof(buf) : (kv_hdr->value_len - size);
                _fdb_flash_read(db, kv_hdr_addr + KV_HDR_DATA_SIZE + FDB_WG_ALIGN(kv_hdr->name_len) + size, buf, cur_size);
                crc32 = fdb_calc_crc32(crc32, buf, cur_size);
                size += cur_size;
            }

            if (crc32 == kv_hdr->crc32)
            {
                res = 0;
            }
        }
    }
    return res;
}

static fdb_err_t align_write(fdb_db_t db, uint32_t addr, const void *buf, size_t size)
{
    fdb_err_t result = FDB_NO_ERR;
    size_t align_remain;

#if (FDB_WRITE_GRAN / 8 > 0)
    uint8_t align_data[FDB_WRITE_GRAN / 8];
    size_t align_data_size = sizeof(align_data);
#else
    /* For compatibility with C89 */
    uint8_t align_data_u8, *align_data = &align_data_u8;
    size_t align_data_size = 1;
#endif

    rt_memset(align_data, FDB_BYTE_ERASED, align_data_size);
    if (FDB_WG_ALIGN_DOWN(size) > 0)
    {
        result = _fdb_flash_write(db, addr, buf, FDB_WG_ALIGN_DOWN(size), 0);
    }

    align_remain = size - FDB_WG_ALIGN_DOWN(size);
    if (result == FDB_NO_ERR && align_remain)
    {
        rt_memcpy(align_data, (uint8_t *)buf + FDB_WG_ALIGN_DOWN(size), align_remain);
        result = _fdb_flash_write(db, addr + FDB_WG_ALIGN_DOWN(size), align_data, align_data_size, 0);
    }

    return result;
}

int8_t find_kv(fdb_db_t db, uint32_t cur_sec_addr, kv_ctx_t *ctx, uint8_t flag)
{
#ifdef FDB_KV_USING_CACHE
    if (flag == FLAG_NEED_FIND && get_kv_from_cache((fdb_kvdb_t)db, ctx->name, ctx->name_len, &ctx->old_kv_addr))
    {
        _fdb_flash_read(db, ctx->old_kv_addr, &ctx->kv_hdr, sizeof(struct kv_hdr_data));
        return 0;
    }
#endif /* FDB_KV_USING_CACHE */

    int8_t res = -1;
    uint32_t offset = SECTOR_HDR_DATA_SIZE;
    uint8_t need_find = flag & FLAG_NEED_FIND;
    uint8_t need_alloc = flag & FLAG_NEED_ALLOC;
    char name_buf[FDB_KV_NAME_MAX] = { 0 };

    do
    {
        _fdb_flash_read(db, cur_sec_addr + offset, &ctx->kv_hdr, sizeof(struct kv_hdr_data));
        if (whether_all_FF((uint8_t *)&ctx->kv_hdr, sizeof(struct kv_hdr_data)))
        {
            if (need_alloc)
            {
                ctx->new_kv_addr = cur_sec_addr + offset;
                res = 0;
            }
            break;
        }

        uint8_t cur_kv_status = _fdb_get_status((uint8_t *)&ctx->kv_hdr.status, FDB_KV_STATUS_NUM);
        // print_status(db, cur_kv_status, cur_sec_addr + offset, 1, 0, 0);
        if (FDB_KV_ERR_HDR == cur_kv_status)
        {
            break;
        }

        if (need_find && (FDB_KV_WRITE == cur_kv_status || FDB_KV_PRE_DELETE == cur_kv_status))
        {
            if (ctx->kv_hdr.name_crc == ctx->name_crc)
            {
                uint32_t name_addr = cur_sec_addr + KV_HDR_DATA_SIZE + offset;

                _fdb_flash_read(db, name_addr, name_buf, ctx->name_len);

                if (!rt_strcmp(ctx->name, name_buf))
                {
                    ctx->old_kv_addr = cur_sec_addr + offset;
                    FDB_LOG_I(db, "find kv:%s @0x%x(%d)\n", ctx->name, ctx->old_kv_addr, cur_kv_status);
                    need_find = 0;
                    res = 0;
#ifdef FDB_KV_USING_CACHE
                    update_kv_cache((fdb_kvdb_t)db, ctx->name_crc, ctx->old_kv_addr);
#endif /* FDB_KV_USING_CACHE */
                }
            }
        }

        offset += ctx->kv_hdr.len;
        if (offset >= db->sec_size - KV_HDR_DATA_SIZE)
        {
            break;
        }
    } while (need_alloc || need_find);

    return res;
}

int move_data(fdb_db_t db, uint32_t src_addr, uint32_t size, uint32_t dist_addr)
{
    uint32_t wr_size = 0;
    uint8_t buf[32];
    while (wr_size < size)
    {
        uint8_t cur_size = (size - wr_size) > sizeof(buf) ? sizeof(buf) : (size - wr_size);
        _fdb_flash_read(db, src_addr + wr_size, buf, cur_size);
        align_write(db, dist_addr + wr_size, buf, cur_size);
        wr_size += cur_size;
    }
    return 0;
}

// 从首扇区开始 如果为满 把满中的非删除的kv 全搬走
//最坏的情况 首扇区全是在使用的kv
int do_gc(fdb_db_t db)
{
    uint32_t from_addr = db->oldest_addr;
    uint32_t from_offset = SECTOR_HDR_DATA_SIZE;
    uint32_t next_alloc_addr = db->empty_kv;
    uint32_t to_addr_down = FDB_ALIGN_DOWN(next_alloc_addr, db->sec_size);
    // uint32_t cross_addr;

    uint8_t buf[KV_HDR_DATA_SIZE];
    kv_hdr_data_t kv_hdr = (kv_hdr_data_t)buf;
    struct sector_info sec_info;

    /* optimize */
    get_sec_info(db, from_addr, &sec_info);
    if (sec_info.dirty == FDB_SECTOR_DIRTY_TRUE)
    {
        fdb_write_status(db, from_addr + SECTOR_DIRTY_MEMBER_OFFSET, FDB_SECTOR_DIRTY_GC, 0);
    }

    // move kv: old pre del -> new pre write -> new write -> old del
    while (from_offset < db->sec_size - KV_HDR_DATA_SIZE)
    {
        _fdb_flash_read(db, from_addr + from_offset, kv_hdr, sizeof(*kv_hdr));
        if (whether_all_FF((uint8_t *)kv_hdr, sizeof(*kv_hdr)))
        {
            break;
        }
        uint8_t cur_kv_status = _fdb_get_status(kv_hdr->status, FDB_KV_STATUS_NUM);
        if (FDB_KV_WRITE == cur_kv_status || FDB_KV_PRE_DELETE == cur_kv_status)
        {
            /* double sector size > free size > single sector size! just do it */
            if (next_alloc_addr + kv_hdr->len + KV_HDR_DATA_SIZE >= to_addr_down + db->sec_size)
            {
                get_sec_info(db, to_addr_down, &sec_info);
                if (sec_info.store == FDB_SECTOR_STORE_USING)
                {
                    fdb_write_status(db, to_addr_down + SECTOR_STORE_MEMBER_OFFSET, FDB_SECTOR_STORE_FULL, 0);
                }
                /*处理下一个扇区*/
                to_addr_down = get_next_sec_addr(db, to_addr_down);
                get_sec_info(db, to_addr_down, &sec_info);
                if (sec_info.store == FDB_SECTOR_STORE_EMPTY)
                {
                    fdb_write_status(db, to_addr_down + SECTOR_STORE_MEMBER_OFFSET, FDB_SECTOR_STORE_USING, 0);
                }

                next_alloc_addr = to_addr_down + SECTOR_HDR_DATA_SIZE;
            }

            if (FDB_KV_WRITE == cur_kv_status)
            {
                /* old pre del */
                fdb_write_status(db, from_addr + KV_STATUS_MEMBER_OFFSET + from_offset, FDB_KV_PRE_DELETE, 1);
            }
            /* new pre write */
            _fdb_set_status((uint8_t *)kv_hdr->status, FDB_KV_STATUS_NUM, FDB_KV_PRE_WRITE);
            _fdb_flash_write(db, next_alloc_addr, kv_hdr, KV_HDR_DATA_SIZE, 1);
            FDB_LOG_W(db, "move kv @0x%x -> @0x%x size %d\n", from_addr + from_offset, next_alloc_addr, kv_hdr->len);
            /* move data */
            move_data(db, from_addr + KV_HDR_DATA_SIZE + from_offset, kv_hdr->len - KV_HDR_DATA_SIZE, next_alloc_addr + KV_HDR_DATA_SIZE);
            /* new write */
            fdb_write_status(db, next_alloc_addr + KV_STATUS_MEMBER_OFFSET, FDB_KV_WRITE, 1);
            /* old del */
            fdb_write_status(db, from_addr + KV_STATUS_MEMBER_OFFSET + from_offset, FDB_KV_DELETED, 1);
#ifdef FDB_KV_USING_CACHE
            update_kv_cache((fdb_kvdb_t)db, kv_hdr->name_crc, next_alloc_addr);
#endif /* FDB_KV_USING_CACHE */
            if (next_alloc_addr % db->sec_size == SECTOR_HDR_DATA_SIZE && sec_info.dirty == FDB_SECTOR_DIRTY_FALSE)
            {
                fdb_write_status(db, next_alloc_addr - SECTOR_HDR_DATA_SIZE + SECTOR_DIRTY_MEMBER_OFFSET, FDB_SECTOR_DIRTY_TRUE, 0);
            }
            next_alloc_addr += kv_hdr->len;
        }
        else if (FDB_KV_ERR_HDR == cur_kv_status)
        {
            FDB_LOG_W(db, "E/gc bad head\n");
            break;
        }
        else
        {
            // FDB_PRINT(db, "garbage kv @0x%x status %x\n", from_addr + from_offset, cur_kv_status);
            if (cur_kv_status != FDB_KV_DELETED)
            {
                FDB_LOG_E(db, "E/gc status\n");
                // print_status(db, cur_kv_status, from_addr + from_offset, 1, 0, 0);
            }
        }
        from_offset += kv_hdr->len;
    }

    _fdb_flash_erase(db, from_addr, db->sec_size);
    write_empty_sector_header(db, from_addr);

    db->empty_kv = next_alloc_addr;
    db->oldest_addr = get_next_sec_addr(db, db->oldest_addr);
    return 0;
}
RTM_EXPORT(do_gc);

uint32_t calc_free_size(fdb_db_t db)
{
    uint32_t free_size;

    if (db->empty_kv > db->oldest_addr)
    {
        free_size = db->max_size - (db->empty_kv - db->oldest_addr);
    }
    else
    {
        free_size = db->max_size - (db->empty_kv - 0 + db->max_size - db->oldest_addr);
    }
    return free_size;
}
RTM_EXPORT(calc_free_size);

uint32_t alloc_kv(fdb_db_t db, uint32_t size)
{
    uint8_t retey_cnt = 2;
    uint32_t free;
    struct sector_info sec_info;

    while (1)
    {
        free = calc_free_size(db);
        free -= size;
        if (free > FDB_GC_EMPTY_SEC_THRESHOLD * db->sec_size)
        {
            break;
        }

        if (retey_cnt == 0)
        {
            return FAILED_ADDR;
        }

        do_gc(db);
        retey_cnt--;
    }

    uint32_t cur_sec_addr = FDB_ALIGN_DOWN(db->empty_kv, db->sec_size);
    get_sec_info(db, cur_sec_addr, &sec_info);
    if (db->empty_kv + size + KV_HDR_DATA_SIZE > cur_sec_addr + db->sec_size)//做人留一线 日后好相见
    {
        if (sec_info.store == FDB_SECTOR_STORE_USING)
        {
            fdb_write_status(db, cur_sec_addr + SECTOR_STORE_MEMBER_OFFSET, FDB_SECTOR_STORE_FULL, 0);
            /*处理下一个扇区*/
            cur_sec_addr = get_next_sec_addr(db, cur_sec_addr);
            get_sec_info(db, cur_sec_addr, &sec_info);
            if (sec_info.store != FDB_SECTOR_STORE_USING)
            {
                fdb_write_status(db, cur_sec_addr + SECTOR_STORE_MEMBER_OFFSET, FDB_SECTOR_STORE_USING, 0);
            }
        }
        free = cur_sec_addr + SECTOR_HDR_DATA_SIZE;
    }
    else
    {
        /* 防止重复写 */
        if (sec_info.store == FDB_SECTOR_STORE_EMPTY)
        {
            fdb_write_status(db, cur_sec_addr + SECTOR_STORE_MEMBER_OFFSET, FDB_SECTOR_STORE_USING, 0);
        }
        free = db->empty_kv;
    }
    // FDB_LOG_I(db, "alloc new kv @0x%x len %d\n", free, size);

    return free;
}

void make_kv_ctx(kv_ctx_t *ctx, const char *name)
{
    ctx->name = name;
    ctx->name_len = rt_strlen(name);
    ctx->name_crc = (uint16_t)(fdb_calc_crc32(0, name, ctx->name_len) >> 16);
    ctx->old_kv_addr = FAILED_ADDR;
    ctx->new_kv_addr = FAILED_ADDR;
}
/**
 * @brief
应用层必须检查是否写入成功
 */
int kv_set(fdb_kvdb_t kvdb, const char *name, const void *value, uint32_t value_len)
{
    fdb_db_t db = (fdb_db_t)kvdb;
    int res;
    kv_ctx_t ctx;
    struct sector_info sec_info;

    uint32_t sec_addr;

    make_kv_ctx(&ctx, name);

    if (value && (ctx.new_kv_addr = alloc_kv(db, KV_HDR_DATA_SIZE + FDB_WG_ALIGN(ctx.name_len) + FDB_WG_ALIGN(value_len))) == FAILED_ADDR)
    {
        return -1;
    }
    /* */
    sec_addr = db->oldest_addr;
    for (int i = 0; i < db->max_size / db->sec_size; i++)
    {
        get_sec_info(db, sec_addr, &sec_info);
        if (sec_info.store == FDB_SECTOR_STORE_USING || sec_info.store == FDB_SECTOR_STORE_FULL)
        {
            res = find_kv(db, sec_addr, &ctx, FLAG_NEED_FIND);
            if (res == 0)
            {
                break;
            }
        }
        else if (sec_info.store == FDB_SECTOR_STORE_EMPTY)
        {
            break;
        }
        sec_addr = get_next_sec_addr(db, sec_addr);
    }

    /* 需要删除 */
    if (value == NULL)
    {
        if (ctx.old_kv_addr != FAILED_ADDR)
        {
            fdb_write_status(db, ctx.old_kv_addr + KV_STATUS_MEMBER_OFFSET, FDB_KV_DELETED, 1);
#ifdef FDB_KV_USING_CACHE
            update_kv_cache((fdb_kvdb_t)db, ctx.name_crc, FDB_DATA_UNUSED);
#endif /* FDB_KV_USING_CACHE */
            return 0;
        }
        else
        {
            return -FDB_EEMPTY;
        }
    }

    /* 有旧值 标记预删除 */
    if (ctx.old_kv_addr != FAILED_ADDR && FDB_KV_WRITE == _fdb_get_status((uint8_t *)&ctx.kv_hdr.status, FDB_KV_STATUS_NUM))
    {
        fdb_write_status(db, ctx.old_kv_addr + KV_STATUS_MEMBER_OFFSET, FDB_KV_PRE_DELETE, 1);
    }

    do
    {
        uint8_t buf[KV_HDR_DATA_SIZE];
        kv_hdr_data_t kv_hdr = (kv_hdr_data_t)buf;

        memset(buf, 0xff, sizeof(buf));
        kv_hdr->magic = KV_MAGIC_WORD;
        _fdb_set_status((uint8_t *)&kv_hdr->status, FDB_KV_STATUS_NUM, FDB_KV_PRE_WRITE);
        kv_hdr->len = KV_HDR_DATA_SIZE + FDB_WG_ALIGN(ctx.name_len) + FDB_WG_ALIGN(value_len);
        kv_hdr->name_crc = ctx.name_crc;
        kv_hdr->name_len = ctx.name_len;
        kv_hdr->value_len = value_len;
        kv_hdr->crc32 = calc_kv_crc32(kv_hdr, name, value);

        res = _fdb_flash_write(db, ctx.new_kv_addr, kv_hdr, KV_HDR_DATA_SIZE, 1);
        if (res < 0)
        {
            FDB_LOG_E(db, "E/set kv hdr(%d)\n", res);
            fdb_write_status(db, ctx.new_kv_addr + KV_STATUS_MEMBER_OFFSET, FDB_KV_ERR_HDR, 1);
            db->empty_kv = FDB_ALIGN(db->empty_kv, db->sec_size) - 1;
            alloc_kv(db, KV_HDR_DATA_SIZE);
            break;
        }

        if (ctx.new_kv_addr % db->sec_size == SECTOR_HDR_DATA_SIZE)
        {
            fdb_write_status(db, ctx.new_kv_addr - SECTOR_HDR_DATA_SIZE + SECTOR_DIRTY_MEMBER_OFFSET, FDB_SECTOR_DIRTY_TRUE, 0);
            // FDB_LOG_D(db, "mark dirty true@0x%x\n", ctx.new_kv_addr - SECTOR_HDR_DATA_SIZE + SECTOR_DIRTY_MEMBER_OFFSET);
        }
        db->empty_kv = ctx.new_kv_addr + kv_hdr->len;

        res = align_write(db, ctx.new_kv_addr + KV_HDR_DATA_SIZE, name, ctx.name_len);
        if (res < 0)
        {
            // FDB_LOG_E(db, "E/set kv name(%d)\n", res);
            break;
        }

        res = align_write(db, ctx.new_kv_addr + KV_HDR_DATA_SIZE + FDB_WG_ALIGN(ctx.name_len), value, value_len);
        if (res < 0)
        {
            // FDB_LOG_E(db, "E/set kv value(%d)\n", res);
            break;
        }
    } while (0);

    if (res == 0)
    {
        fdb_write_status(db, ctx.new_kv_addr + KV_STATUS_MEMBER_OFFSET, FDB_KV_WRITE, 1);
        if (ctx.old_kv_addr != FAILED_ADDR)
        {
            fdb_write_status(db, ctx.old_kv_addr + KV_STATUS_MEMBER_OFFSET, FDB_KV_DELETED, 1);
        }
#ifdef FDB_KV_USING_CACHE
        update_kv_cache((fdb_kvdb_t)db, ctx.name_crc, ctx.new_kv_addr);
#endif /* FDB_KV_USING_CACHE */
    }

    return res;
}
RTM_EXPORT(kv_set);

int kv_get(fdb_kvdb_t kvdb, const char *name, const void *value, uint32_t value_len)
{
    fdb_db_t db = (fdb_db_t)kvdb;
    int res = 0;
    uint32_t sec_addr;

    kv_ctx_t ctx;
    struct sector_info sec_info;

    make_kv_ctx(&ctx, name);

    sec_addr = db->oldest_addr;
    for (int i = 0; i < db->max_size / db->sec_size; i++)
    {
        get_sec_info(db, sec_addr, &sec_info);
        if (sec_info.store == FDB_SECTOR_STORE_USING || sec_info.store == FDB_SECTOR_STORE_FULL)
        {
            if (find_kv(db, sec_addr, &ctx, FLAG_NEED_FIND) == 0)
            {
                break;
            }
        }
        else if (sec_info.store == FDB_SECTOR_STORE_EMPTY)
        {
            break;
        }
        sec_addr = get_next_sec_addr(db, sec_addr);
    }

    if (ctx.old_kv_addr == FAILED_ADDR)
    {
        res = -FDB_EEMPTY;
    }
    else
    {
        if (value_len < ctx.kv_hdr.value_len)
        {
            res = -FDB_ENOMEM;
        }
        else
        {
            if (value_len > ctx.kv_hdr.value_len)
            {
                value_len = ctx.kv_hdr.value_len;
            }
            _fdb_flash_read(db, ctx.old_kv_addr + KV_HDR_DATA_SIZE + FDB_WG_ALIGN(ctx.kv_hdr.name_len), (void *)value, value_len);
            if (ctx.kv_hdr.crc32 == calc_kv_crc32(&ctx.kv_hdr, ctx.name, value))
            {
                res = value_len;
            }
            else
            {
                FDB_LOG_E(db, "get kv @0x%lx crc32 error! drop it\n", ctx.old_kv_addr);
                fdb_write_status(db, ctx.old_kv_addr + KV_STATUS_MEMBER_OFFSET, FDB_KV_DELETED, 1);
#ifdef FDB_KV_USING_CACHE
                update_kv_cache((fdb_kvdb_t)db, ctx.name_crc, FDB_DATA_UNUSED);
#endif /* FDB_KV_USING_CACHE */
                res = -FDB_EINVAL;
            }
        }
    }
    return res;
}
RTM_EXPORT(kv_get);

int fdb_print(fdb_kvdb_t kvdb, void *read_value_buf, uint32_t buf_size)
{
    fdb_db_t db = (fdb_db_t)kvdb;
    char name[FDB_KV_NAME_MAX];
    uint32_t sec_addr = db->oldest_addr;
    uint32_t offset;
    struct sector_info sec_info;
    struct kv_hdr_data kv_hdr;

    // if (!read_value_buf)
    // {
    //     return -FDB_ERR;
    // }

    for (int i = 0; i < db->max_size / db->sec_size; i++)
    {
        offset = SECTOR_HDR_DATA_SIZE;
        get_sec_info(db, sec_addr, &sec_info);
        if (sec_info.store == FDB_SECTOR_STORE_EMPTY)
        {
            break;
        }

        do
        {
            _fdb_flash_read(db, sec_addr + offset, &kv_hdr, sizeof(kv_hdr));
            if (whether_all_FF((uint8_t *)&kv_hdr, KV_HDR_DATA_SIZE))
            {
                break;
            }
            uint8_t cur_kv_status = _fdb_get_status((uint8_t *)&kv_hdr.status, FDB_KV_STATUS_NUM);
            if (FDB_KV_WRITE == cur_kv_status || FDB_KV_PRE_DELETE == cur_kv_status)
            {
                uint32_t data_addr = sec_addr + KV_HDR_DATA_SIZE + offset;
                _fdb_flash_read(db, data_addr, name, kv_hdr.name_len);
                name[kv_hdr.name_len] = '\0';
                data_addr += FDB_WG_ALIGN(kv_hdr.name_len);
                db->printf("name %s len %d at 0x%x\n", name, kv_hdr.value_len, data_addr);
                // if (buf_size < kv_hdr.value_len)
                // {
                //     return -FDB_ENOSPC;
                // }
                kv_hdr.value_len = kv_hdr.value_len > buf_size ? buf_size : kv_hdr.value_len;
                _fdb_flash_read(db, data_addr, read_value_buf, kv_hdr.value_len);
                print_kv_cb(name, data_addr, read_value_buf, kv_hdr.value_len);
            }
            offset += kv_hdr.len;
        } while (offset < db->sec_size - KV_HDR_DATA_SIZE);

        sec_addr = get_next_sec_addr(db, sec_addr);
    }
    return 0;
}
RTM_EXPORT(fdb_print);

int check_all_kv(fdb_db_t db)
{
    uint32_t sec_addr = db->oldest_addr;
    uint8_t cur_status;
    for (int s = 0; s < db->max_size / db->sec_size; s++)
    {
        struct sector_info sec_info;
        get_sec_info(db, sec_addr, &sec_info);

        if (sec_info.store == FDB_SECTOR_STORE_EMPTY)
            break; // 到尾了

        uint32_t offset = SECTOR_HDR_DATA_SIZE;

        while (offset + KV_HDR_DATA_SIZE < db->sec_size)
        {
            struct kv_hdr_data hdr;
            _fdb_flash_read(db, sec_addr + offset, &hdr, sizeof(hdr));
            if (whether_all_FF((uint8_t *)&hdr, sizeof(hdr)))
            {
                break; // 此 sector 结束
            }
            /* 1. 检查 header */
            if (hdr.magic != KV_MAGIC_WORD ||
                hdr.len < KV_HDR_DATA_SIZE ||
                hdr.len > db->sec_size)
            {
                fdb_write_status(db, sec_addr + offset + KV_STATUS_MEMBER_OFFSET, FDB_KV_ERR_HDR, 1);
                FDB_LOG_E(db, "E/invalid hdr @0x%x\n", sec_addr + offset);
                break;
            }

            /* 2. 获取状态 */
            cur_status = _fdb_get_status((uint8_t *)hdr.status, FDB_KV_STATUS_NUM);
            // print_status(db, cur_status, sec_addr + offset + KV_STATUS_MEMBER_OFFSET, 1, 0, 0);

            /* 3. 校验 CRC 的情况：WRITE(新KV) 或 PRE_DELETE */
            if (cur_status == FDB_KV_WRITE || cur_status == FDB_KV_PRE_DELETE)
            {
                uint32_t crc = 0;

                crc = fdb_calc_crc32(crc, &hdr.len, sizeof(hdr.len));
                crc = fdb_calc_crc32(crc, &hdr.name_crc, sizeof(hdr.name_crc));
                crc = fdb_calc_crc32(crc, &hdr.value_len, sizeof(hdr.value_len));

                /* 读取 name */
                uint32_t pos = sec_addr + offset + KV_HDR_DATA_SIZE;
                uint8_t buf[32];
                // char name_buf[FDB_KV_NAME_MAX];
                uint32_t size = hdr.name_len;

                while (size > 0)
                {
                    uint32_t chunk = size > sizeof(buf) ? sizeof(buf) : size;
                    _fdb_flash_read(db, pos, buf, chunk);
                    crc = fdb_calc_crc32(crc, buf, chunk);
                    pos += chunk;
                    size -= chunk;
                }
                // rt_strcpy(name_buf, (const char *)buf);
                /* 读取 value */
                size = hdr.value_len;
                pos = sec_addr + offset + KV_HDR_DATA_SIZE + FDB_WG_ALIGN(hdr.name_len);

                while (size > 0)
                {
                    uint32_t chunk = size > sizeof(buf) ? sizeof(buf) : size;
                    _fdb_flash_read(db, pos, buf, chunk);
                    crc = fdb_calc_crc32(crc, buf, chunk);
                    pos += chunk;
                    size -= chunk;
                }

                /* CRC mismatch -> 删除 */
                if (crc != hdr.crc32)
                {
                    FDB_LOG_W(db, "drop bad crc kv @0x%x\n", sec_addr + offset);
                    fdb_write_status(db,
                                     sec_addr + offset + KV_STATUS_MEMBER_OFFSET,
                                     FDB_KV_DELETED, 1);
                }
                // else
                // {
                //     db->printf("name %s at 0x%x\n", name_buf, sec_addr + offset);
                // }
            }
            else if (cur_status == FDB_KV_PRE_WRITE)
            {
                FDB_LOG_W(db, "drop pre write kv @0x%x\n", sec_addr + offset);
                fdb_write_status(db, sec_addr + offset + KV_STATUS_MEMBER_OFFSET, FDB_KV_DELETED, 1);
            }
            else if (cur_status >= FDB_KV_ERR_HDR)
            {
                FDB_LOG_E(db, "find bad hdr @0x%x(%d)\n", sec_addr + offset, cur_status);
                break;
            }
            /* 4. 跳到下一个 KV */
            offset += hdr.len;
        }

        sec_addr = get_next_sec_addr(db, sec_addr);
    }

    return 0;
}

/**
 * @brief 找最老的扇区 检查和处理kv
 *
 * @param db
 * @return int
 */
int flashdb_init(fdb_kvdb_t kvdb)
{
    fdb_db_t db = (fdb_db_t)kvdb;

    // memset(db, 0, sizeof(*db));
    // db->max_size = FLASH_SECTOR_SIZE * FLASH_SEC_NUM;
    // db->sec_size = FLASH_SECTOR_SIZE;

    struct sector_info info;
    uint32_t sec_addr;
    int res;
    int8_t need_gc;
    uint8_t last_sec_status;
    uint8_t bad_sec_cnt, using_sec_cnt, full_sec_cnt;
    kv_ctx_t ctx;
#ifdef FDB_KV_USING_CACHE
    for (size_t i = 0; i < FDB_KV_CACHE_TABLE_SIZE; i++)
    {
        kvdb->kv_cache_table[i].addr = FDB_DATA_UNUSED;
        kvdb->kv_cache_table[i].name_crc = 0;
        kvdb->kv_cache_table[i].active = 0;
    }
#endif

init_chk:
    db->oldest_addr = FAILED_ADDR;
    db->empty_kv = FAILED_ADDR;
    db->init_ok = 0;
    bad_sec_cnt = using_sec_cnt = full_sec_cnt = 0;
    need_gc = 0;
    last_sec_status = FDB_SECTOR_STORE_EMPTY;

    for (sec_addr = 0; sec_addr < db->max_size; sec_addr += db->sec_size)
    {
        res = get_sec_info(db, sec_addr, &info);
        if (res == 0)
        {
            if (last_sec_status == FDB_SECTOR_STORE_EMPTY && (info.store == FDB_SECTOR_STORE_USING || info.store == FDB_SECTOR_STORE_FULL))
            {
                db->oldest_addr = sec_addr;
            }

            if (info.dirty == FDB_SECTOR_DIRTY_GC)
            {
                need_gc = 1;
                FDB_LOG_W(db, "sector @0x%x need gc\n", sec_addr);
                db->oldest_addr = sec_addr;
            }

            if (info.store == FDB_SECTOR_STORE_USING && info.dirty == FDB_SECTOR_DIRTY_TRUE)
            {
                FDB_PRINT(db, "using sector @0x%x\n", sec_addr);
                if (0 == find_kv(db, sec_addr, &ctx, FLAG_NEED_ALLOC))
                {
                    db->empty_kv = ctx.new_kv_addr;
                    using_sec_cnt++;
                }
            }
            else if (info.store == FDB_SECTOR_STORE_FULL)
            {
                FDB_LOG_I(db, "full sector @0x%x\n", sec_addr);
                full_sec_cnt++;
            }
            last_sec_status = info.store;
        }
        else
        {
            FDB_LOG_E(db, "sector @0x%x head is invalid,format it\n", sec_addr);
            _fdb_flash_erase(db, sec_addr, db->sec_size);
            write_empty_sector_header(db, sec_addr);
            bad_sec_cnt++;
        }
    }
    /* 如果是第一次使用 全部为bad 但是因为已经 foramt 过了 只需要重新init */
    if (bad_sec_cnt == db->max_size / db->sec_size)
    {
        goto init_chk;
    }
    /* 第一次使用 */
    if (!using_sec_cnt && !full_sec_cnt)
    {
        db->empty_kv = 0 + SECTOR_HDR_DATA_SIZE;
        db->oldest_addr = 0;
    }
    else
    {
        if (db->empty_kv == FAILED_ADDR || db->oldest_addr == FAILED_ADDR || using_sec_cnt != 1)
        {
            FDB_LOG_E(db, "E/format all\nempty_kv:0x%x oldest_addr:0x%x\n", db->empty_kv, db->oldest_addr);
            for (sec_addr = 0; sec_addr < db->max_size; sec_addr += db->sec_size)
            {
                _fdb_flash_erase(db, sec_addr, db->sec_size);
                write_empty_sector_header(db, sec_addr);
            }
            goto init_chk;
        }
    }
    FDB_LOG_I(db, "old_sec@0x%x\n", db->oldest_addr);
    FDB_LOG_I(db, "empty kv @0x%x\n", db->empty_kv);
    sec_addr = calc_free_size(db);
    FDB_LOG_I(db, "free: %d bytes(%d%%)\n", sec_addr, sec_addr * 100 / db->max_size);

    if (need_gc)
    {
        FDB_LOG_W(db, "now try gc\n");
        do_gc(db);
    }

    check_all_kv(db);
    db->init_ok = 1;

    return res;
}
RTM_EXPORT(flashdb_init);

//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// FOR UTEST////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// #ifdef FDB_USING_FAL_MODE
// #include "fal.h"
// #endif
// fdb_err_t
// fdb_kvdb_init(fdb_kvdb_t db, const char *name, const char *path, struct fdb_default_kv *default_kv, void *user_data)
// {
//     fdb_db_t mydb = &db->parent;
//     if (mydb->file_mode)
//     {
// #ifdef FDB_USING_FILE_MODE
//         memset(mydb->cur_file_sec, FDB_FAILED_ADDR, FDB_FILE_CACHE_TABLE_SIZE * sizeof(mydb->cur_file_sec[0]));
//         /* must set when using file mode */
//         FDB_ASSERT(mydb->sec_size != 0);
//         FDB_ASSERT(mydb->max_size != 0);
// #ifdef FDB_USING_FILE_POSIX_MODE
//         memset(mydb->cur_file, -1, FDB_FILE_CACHE_TABLE_SIZE * sizeof(mydb->cur_file[0]));
// #else
//         memset(mydb->cur_file, 0, FDB_FILE_CACHE_TABLE_SIZE * sizeof(mydb->cur_file[0]));
// #endif
//         mydb->storage.dir = path;
//         FDB_ASSERT(strlen(path) != 0)
// #endif /*FDB_USING_FILE_MODE */
//     }
//     else
//     {
// #ifdef FDB_USING_FAL_MODE
//         size_t block_size;

//         /* FAL (Flash Abstraction Layer) initialization */
//         fal_init();
//         /* check the flash partition */
//         if ((db->parent.storage.part = fal_partition_find(path)) == NULL)
//         {
//             FDB_INFO("Error: Partition (%s) not found.\n", path);
//             return FDB_PART_NOT_FOUND;
//         }

//         block_size = fal_flash_device_find(db->parent.storage.part->flash_name)->blk_size;
//         if (db->parent.sec_size == 0)
//         {
//             db->parent.sec_size = block_size;
//         }
//         else
//         {
//             /* must be aligned with block size */
//             if (db->parent.sec_size % block_size != 0)
//             {
//                 FDB_INFO("Error: db sector size (%" PRIu32 ") MUST align with block size (%zu).\n", db->sec_size, block_size);
//                 return FDB_INIT_FAILED;
//             }
//         }
//         if (!db->parent.max_size)
//         {
//             db->parent.max_size = db->parent.storage.part->len;
//         }
// #else

// #endif
//     }
//     return flashdb_init(db);
// }

// void fdb_kvdb_control(fdb_kvdb_t db, int cmd, void *arg)
// {
//     FDB_ASSERT(db);

//     switch (cmd)
//     {
//     case FDB_KVDB_CTRL_SET_SEC_SIZE:
//         /* this change MUST before database initialization */
//         FDB_ASSERT(db->parent.init_ok == false);
//         db->parent.sec_size = *(uint32_t *)arg;
//         break;
//     case FDB_KVDB_CTRL_GET_SEC_SIZE:
//         *(uint32_t *)arg = db->parent.sec_size;
//         break;
//     case FDB_KVDB_CTRL_SET_LOCK:
// #if !defined(__ARMCC_VERSION) && defined(__GNUC__)
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wpedantic"
// #endif
//         db->parent.lock = (void (*)(fdb_db_t db))arg;
// #if !defined(__ARMCC_VERSION) && defined(__GNUC__)
// #pragma GCC diagnostic pop
// #endif
//         break;
//     case FDB_KVDB_CTRL_SET_UNLOCK:
// #if !defined(__ARMCC_VERSION) && defined(__GNUC__)
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wpedantic"
// #endif
//         db->parent.unlock = (void (*)(fdb_db_t db))arg;
// #if !defined(__ARMCC_VERSION) && defined(__GNUC__)
// #pragma GCC diagnostic pop
// #endif
//         break;
//     case FDB_KVDB_CTRL_SET_FILE_MODE:
// #ifdef FDB_USING_FILE_MODE
//         /* this change MUST before database initialization */
//         FDB_ASSERT(db->parent.init_ok == false);
//         db->parent.file_mode = *(bool *)arg;
// #else
//         // FDB_INFO("Error: set file mode Failed. Please defined the
//         // FDB_USING_FILE_MODE macro.");
// #endif
//         break;
//     case FDB_KVDB_CTRL_SET_MAX_SIZE:

//         /* this change MUST before database initialization */
//         FDB_ASSERT(db->parent.init_ok == false);
//         db->parent.max_size = *(uint32_t *)arg;

//         break;
//     case FDB_KVDB_CTRL_SET_NOT_FORMAT:
//         /* this change MUST before database initialization */
//         FDB_ASSERT(db->parent.init_ok == false);
//         // db->parent.not_formatable = *( bool* )arg;
//         break;
//     }
// }

// fdb_err_t fdb_kv_set_blob(fdb_kvdb_t db, const char *key, fdb_blob_t blob)
// {
//     return kv_set(db, key, blob->buf, blob->size);
// }

// size_t fdb_kv_get_blob(fdb_kvdb_t db, const char *key, fdb_blob_t blob)
// {
//     int len;
//     len = kv_get(db, key, blob->buf, blob->size);
//     blob->saved.len = len;
//     if (len < 0)
//     {
//         len = 0;
//         blob->saved.len = 0;
//     }
//     return len;
// }

// fdb_err_t fdb_kv_del(fdb_kvdb_t db, const char *key)
// {
//     return kv_set(db, key, NULL, 0);
// }

// fdb_kv_t fdb_kv_get_obj(fdb_kvdb_t _db, const char *key, fdb_kv_t kv)
// {
//     fdb_db_t db = (fdb_db_t)_db;
//     uint32_t sec_addr;

//     kv_ctx_t ctx;
//     struct sector_info sec_info;

//     make_kv_ctx(&ctx, key);

//     sec_addr = db->oldest_addr;
//     for (int i = 0; i < db->max_size / db->sec_size; i++)
//     {
//         get_sec_info(db, sec_addr, &sec_info);
//         if (sec_info.store == FDB_SECTOR_STORE_USING || sec_info.store == FDB_SECTOR_STORE_FULL)
//         {
//             if (find_kv(db, sec_addr, &ctx, FLAG_NEED_FIND) == 0)
//             {
//                 break;
//             }
//         }
//         else if (sec_info.store == FDB_SECTOR_STORE_EMPTY)
//         {
//             break;
//         }
//         sec_addr = get_next_sec_addr(db, sec_addr);
//     }

//     if (ctx.old_kv_addr != FAILED_ADDR)
//     {
//         kv->addr.start = ctx.old_kv_addr;
//         kv->addr.value = ctx.old_kv_addr + KV_HDR_DATA_SIZE + FDB_WG_ALIGN(ctx.kv_hdr.name_len);
//         kv->value_len = ctx.kv_hdr.value_len;
//     }
//     else
//     {
//         kv = NULL;
//     }
//     return kv;
// }

// fdb_blob_t fdb_kv_to_blob(fdb_kv_t kv, fdb_blob_t blob)
// {
//     blob->saved.meta_addr = kv->addr.start;
//     blob->saved.addr = kv->addr.value;
//     blob->saved.len = kv->value_len;

//     return blob;
// }

// fdb_err_t fdb_kv_set(fdb_kvdb_t db, const char *key, const char *value)
// {
//     struct fdb_blob blob;

//     if (value)
//     {
//         return fdb_kv_set_blob(db, key, fdb_blob_make(&blob, value, strlen(value)));
//     }
//     else
//     {
//         return fdb_kv_del(db, key);
//     }
// }

// static bool fdb_is_str(uint8_t *value, size_t len)
// {
// #define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
//     size_t i;

//     for (i = 0; i < len; i++)
//     {
//         if (!__is_print(value[i]))
//         {
//             return false;
//         }
//     }
//     return true;
// }

// char *fdb_kv_get(fdb_kvdb_t db, const char *key)
// {
// // RT_SECTION(".shared_ram")
// #define FDB_STR_KV_VALUE_MAX_SIZE 128
//     static char value[FDB_STR_KV_VALUE_MAX_SIZE + 1];
//     size_t get_size;
//     struct fdb_blob blob;

//     if ((get_size = fdb_kv_get_blob(db, key, fdb_blob_make(&blob, value, FDB_STR_KV_VALUE_MAX_SIZE))) > 0)
//     {
//         /* the return value must be string */
//         if (fdb_is_str((uint8_t *)value, get_size))
//         {
//             value[get_size] = '\0';
//             return value;
//         }
//         else if (blob.saved.len > FDB_STR_KV_VALUE_MAX_SIZE)
//         {
//         }
//         else
//         {
//             printf("Warning: The KV value isn't string. Could not be returned\n");
//             return NULL;
//         }
//     }

//     return NULL;
// }

// void _fdb_deinit(fdb_db_t db)
// {
//     FDB_ASSERT(db);

//     if (db->init_ok)
//     {
// #ifdef FDB_USING_FILE_MODE
//         for (int i = 0; i < FDB_FILE_CACHE_TABLE_SIZE; i++)
//         {
// #ifdef FDB_USING_FILE_POSIX_MODE
//             if (db->cur_file[i] > 0)
//             {
//                 close(db->cur_file[i]);
//             }
// #else
//             if (db->cur_file[i] != 0)
//             {
//                 fclose(db->cur_file[i]);
//             }
// #endif /* FDB_USING_FILE_POSIX_MODE */
//         }
// #endif /* FDB_USING_FILE_MODE */
//     }
//     db->sec_size = 0;
//     db->max_size = 0;
//     db->init_ok = false;
// }

// fdb_err_t fdb_kvdb_deinit(fdb_kvdb_t db)
// {
//     _fdb_deinit((fdb_db_t)db);
//     rt_kprintf("fdb exit\n");
//     return FDB_NO_ERR;
// }

// /**
//  * recovery all KV to default.
//  *
//  * @param db database object
//  * @return result
//  */
// fdb_err_t fdb_kv_set_default(fdb_kvdb_t db)
// {
//     fdb_err_t result = FDB_NO_ERR;
//     uint32_t sec_addr, i, value_len;
//     struct kvdb_sec_info sector;

//     /* lock the KV cache */
//     // db_lock(db);

//     /* format all sectors */
//     for (sec_addr = 0; sec_addr < db->parent.max_size; sec_addr += db->parent.sec_size)
//     {
//         _fdb_flash_erase(&db->parent, sec_addr, db->parent.sec_size);
//         write_empty_sector_header(&db->parent, sec_addr);
//     }

//     flashdb_init(db);
//     /* create default KV */
//     for (i = 0; i < db->default_kvs.num; i++)
//     {
//         /* It seems to be a string when value length is 0.
//          * This mechanism is for compatibility with older versions (less then V4.0).
//          */
//         if (db->default_kvs.kvs[i].value_len == 0)
//         {
//             value_len = rt_strlen(db->default_kvs.kvs[i].value);
//         }
//         else
//         {
//             value_len = db->default_kvs.kvs[i].value_len;
//         }
//         kv_set(db, db->default_kvs.kvs[i].key, db->default_kvs.kvs[i].value, value_len);
//         if (result != FDB_NO_ERR)
//         {
//             goto __exit;
//         }
//     }

// __exit:
//     db_oldest_addr(db) = 0;
//     /* unlock the KV cache */
//     // db_unlock(db);

//     return result;
// }

// fdb_kv_iterator_t fdb_kv_iterator_init(fdb_kvdb_t db, fdb_kv_iterator_t itr)
// {
//     itr->curr_kv.addr.start = 0;

//     /* If iterator statistics is needed */
//     itr->iterated_cnt = 0;
//     itr->iterated_obj_bytes = 0;
//     itr->iterated_value_bytes = 0;
//     itr->traversed_len = 0;
//     /* Start from sector head */
//     itr->sector_addr = db_oldest_addr(db);
//     return itr;
// }

// static uint32_t get_next_sector_addr(fdb_kvdb_t db, uint32_t pre_sec, uint32_t traversed_len)
// {
//     uint32_t cur_block_size;

//     // if (pre_sec->combined == SECTOR_NOT_COMBINED) {
//     cur_block_size = db_sec_size(db);
//     // } else {
//     //     cur_block_size = pre_sec->combined * db_sec_size(db);
//     // }

//     if (traversed_len + cur_block_size <= db_max_size(db))
//     {
//         /* if reach to the end, roll back to the first sector */
//         if (pre_sec + cur_block_size < db_max_size(db))
//         {
//             return pre_sec + cur_block_size;
//         }
//         else
//         {
//             /* the next sector is on the top of the database */
//             return 0;
//         }
//     }
//     else
//     {
//         /* finished */
//         return FAILED_ADDR;
//     }
// }
// fdb_err_t read_sector_info(fdb_kvdb_t db, uint32_t addr, kv_sec_info_t sector, bool traversal)
// {
//     fdb_err_t res;
//     struct sector_info sec_info;
//     res = get_sec_info((fdb_db_t)db, addr, &sec_info);
//     sector->addr = addr;
//     sector->check_ok = (res == FDB_NO_ERR);
//     sector->status.store = sec_info.store;
//     sector->status.dirty = sec_info.dirty;
//     sector->combined = SECTOR_NOT_COMBINED;
//     return res;
// }

// static uint32_t find_next_kv_addr(fdb_kvdb_t db, uint32_t start, uint32_t end)
// {
//     uint8_t buf[32];
//     uint32_t start_bak = start, i;
//     uint32_t magic;

//     for (; start < end && start + sizeof(buf) < end; start += (sizeof(buf) - sizeof(uint32_t)))
//     {
//         if (_fdb_flash_read((fdb_db_t)db, start, (uint32_t *)buf, sizeof(buf)) != FDB_NO_ERR)
//             return FAILED_ADDR;
//         for (i = 0; i < sizeof(buf) - sizeof(uint32_t) && start + i < end; i++)
//         {
// #ifndef FDB_BIG_ENDIAN /* Little Endian Order */
//             magic = buf[i] + ((uint32_t)buf[i + 1] << 8) + ((uint32_t)buf[i + 2] << 16) + ((uint32_t)buf[i + 3] << 24);
// #else /* Big Endian Order */
//             magic = buf[i + 3] + ((uint32_t)buf[i + 2] << 8) + ((uint32_t)buf[i + 1] << 16) + ((uint32_t)buf[i] << 24);
// #endif
//             if (magic == KV_MAGIC_WORD && (start + i - KV_MAGIC_OFFSET) >= start_bak)
//             {
//                 return start + i - KV_MAGIC_OFFSET;
//             }
//         }
//     }

//     return FAILED_ADDR;
// }

// static uint32_t get_next_kv_addr(fdb_kvdb_t db, kv_sec_info_t sector, fdb_kv_t pre_kv)
// {
//     uint32_t addr = FAILED_ADDR;

//     if (sector->status.store == FDB_SECTOR_STORE_EMPTY)
//     {
//         return FAILED_ADDR;
//     }

//     if (pre_kv->addr.start == FAILED_ADDR)
//     {
//         /* the first KV address */
//         addr = sector->addr + SECTOR_HDR_DATA_SIZE;
//     }
//     else
//     {
//         if (pre_kv->addr.start <= sector->addr + db_sec_size(db))
//         {
//             if (pre_kv->crc_is_ok)
//             {
//                 addr = pre_kv->addr.start + pre_kv->len;
//             }
//             else
//             {
//                 /* when pre_kv CRC check failed, maybe the flash has error data
//                  * find_next_kv_addr after pre_kv address */
//                 // addr = pre_kv->addr.start + FDB_WG_ALIGN(1);
//             }
//             /* check and find next KV address */
//             addr = find_next_kv_addr(db, addr, sector->addr + db_sec_size(db) - SECTOR_HDR_DATA_SIZE);

//             if (addr == FAILED_ADDR || addr > sector->addr + db_sec_size(db) || pre_kv->len == 0)
//             {
//                 // TODO Sector continuous mode
//                 return FAILED_ADDR;
//             }
//         }
//         else
//         {
//             /* no KV */
//             return FAILED_ADDR;
//         }
//     }

//     return addr;
// }

// static fdb_err_t read_kv(fdb_kvdb_t db, fdb_kv_t kv)
// {
//     struct kv_hdr_data kv_hdr;

//     uint32_t calc_crc32 = 0, crc_data_len, kv_name_addr;
//     fdb_err_t result = FDB_NO_ERR;
//     size_t len, size;
//     /* read KV header raw data */
//     _fdb_flash_read((fdb_db_t)db, kv->addr.start, (uint32_t *)&kv_hdr, sizeof(struct kv_hdr_data));
//     kv->status = (fdb_kv_status_t)_fdb_get_status((uint8_t *)kv_hdr.status, FDB_KV_STATUS_NUM);
//     kv->len = kv_hdr.len;

//     uint8_t buf[32];
//     uint32_t crc32 = 0;

//     crc32 = fdb_calc_crc32(crc32, &kv_hdr.len, sizeof(kv_hdr.len));
//     crc32 = fdb_calc_crc32(crc32, &kv_hdr.name_crc, sizeof(kv_hdr.name_crc));
//     crc32 = fdb_calc_crc32(crc32, &kv_hdr.value_len, sizeof(kv_hdr.value_len));

//     size = 0;
//     uint8_t cur_size;
//     while (size < kv_hdr.name_len)
//     {
//         cur_size = (kv_hdr.name_len - size) > sizeof(buf) ? sizeof(buf) : (kv_hdr.name_len - size);
//         _fdb_flash_read((fdb_db_t)db, kv->addr.start + KV_HDR_DATA_SIZE + size, buf, cur_size);
//         crc32 = fdb_calc_crc32(crc32, buf, cur_size);
//         size += cur_size;
//     }

//     size = 0;
//     while (size < kv_hdr.value_len)
//     {
//         cur_size = (kv_hdr.value_len - size) > sizeof(buf) ? sizeof(buf) : (kv_hdr.value_len - size);
//         _fdb_flash_read((fdb_db_t)db, kv->addr.start + KV_HDR_DATA_SIZE + FDB_WG_ALIGN(kv_hdr.name_len) + size, buf, cur_size);
//         crc32 = fdb_calc_crc32(crc32, buf, cur_size);
//         size += cur_size;
//     }

//     if (crc32 == kv_hdr.crc32)
//     {
//         result = 0;
//     }

//     if (result)
//     {
//         kv->crc_is_ok = false;
//         result = FDB_READ_ERR;
//     }
//     else
//     {
//         kv->crc_is_ok = true;
//         /* 这个名字在对齐的KV标题后面 */
//         kv_name_addr = kv->addr.start + KV_HDR_DATA_SIZE;
//         _fdb_flash_read((fdb_db_t)db, kv_name_addr, (uint32_t *)kv->name, FDB_WG_ALIGN(kv_hdr.name_len));
//         /* the value is behind aligned name */
//         kv->addr.value = kv_name_addr + FDB_WG_ALIGN(kv_hdr.name_len);
//         kv->value_len = kv_hdr.value_len;
//         kv->name_len = kv_hdr.name_len;
//         if (kv_hdr.name_len >= sizeof(kv->name) / sizeof(kv->name[0]))
//         {
//             kv_hdr.name_len = sizeof(kv->name) / sizeof(kv->name[0]) - 1;
//         }
//         kv->name[kv_hdr.name_len] = '\0';
//     }
//     return result;
// }

// bool fdb_kv_iterate(fdb_kvdb_t db, fdb_kv_iterator_t itr)
// {
//     struct kvdb_sec_info sector;
//     fdb_kv_t kv = &(itr->curr_kv);

//     do
//     {
//         if (read_sector_info(db, itr->sector_addr, &sector, false) == FDB_NO_ERR)
//         {
//             if (sector.status.store == FDB_SECTOR_STORE_USING || sector.status.store == FDB_SECTOR_STORE_FULL)
//             {
//                 if (kv->addr.start == 0)
//                 {
//                     kv->addr.start = sector.addr + SECTOR_HDR_DATA_SIZE;
//                 }
//                 else if ((kv->addr.start = get_next_kv_addr(db, &sector, kv)) == FAILED_ADDR)
//                 {
//                     kv->addr.start = 0;
//                     itr->traversed_len += db_sec_size(db);
//                     continue;
//                 }
//                 do
//                 {
//                     read_kv(db, kv);
//                     if (kv->status == FDB_KV_WRITE && kv->crc_is_ok == true)
//                     {
//                         /* We got a valid kv here. */
//                         /* If iterator statistics is needed */
//                         itr->iterated_cnt++;
//                         itr->iterated_obj_bytes += kv->len;
//                         itr->iterated_value_bytes += kv->value_len;
//                         return true;
//                     }
//                 } while ((kv->addr.start = get_next_kv_addr(db, &sector, kv)) != FAILED_ADDR);
//             }
//         }
//         /** Set kv->addr.start to 0 when we get into a new sector so that if we
//          * successfully get the next sector info, the kv->addr.start is set to the
//          * new sector.addr + SECTOR_HDR_DATA_SIZE.
//          */
//         kv->addr.start = 0;
//         itr->traversed_len += db_sec_size(db);
//     } while ((itr->sector_addr = get_next_sector_addr(db, sector.addr, itr->traversed_len)) != FAILED_ADDR);
//     /* Finally we have iterated all the KVs. */
//     return false;
// }


// static void kv_iterator(fdb_kvdb_t db, fdb_kv_t kv, void *arg1, void *arg2, bool (*callback)(fdb_kv_t kv, void *arg1, void *arg2))
// {
//     struct kvdb_sec_info sector;
//     uint32_t sec_addr, traversed_len = 0;

//     sec_addr = db_oldest_addr(db);
//     /* search all sectors */
//     do
//     {
//         traversed_len += db_sec_size(db);
//         if (read_sector_info(db, sec_addr, &sector, false) != FDB_NO_ERR)
//         {
//             continue;
//         }
//         if (callback == NULL)
//         {
//             continue;
//         }
//         /* sector has KV */
//         if (sector.status.store == FDB_SECTOR_STORE_USING || sector.status.store == FDB_SECTOR_STORE_FULL)
//         {
//             kv->addr.start = sector.addr + SECTOR_HDR_DATA_SIZE;
//             /* search all KV */
//             do
//             {
//                 read_kv(db, kv);
//                 /* iterator is interrupted when callback return true */
//                 if (callback(kv, arg1, arg2))
//                 {
//                     return;
//                 }
//             } while ((kv->addr.start = get_next_kv_addr(db, &sector, kv)) != FAILED_ADDR);
//         }
//     } while ((sec_addr = get_next_sector_addr(db, sector.addr, traversed_len)) != FAILED_ADDR);
// }
