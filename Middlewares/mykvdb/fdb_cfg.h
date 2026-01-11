#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_
#define FDB_USING_KVDB

// #define FDB_KV_CACHE_TABLE_SIZE      0  /* KV 缓存表大小，用于加速查找 */
// #define FDB_SECTOR_CACHE_TABLE_SIZE  0   /* 扇区缓存表大小，需与 KV 缓存同时使能 */

// #define FDB_USING_TSDB

#define FDB_USING_OTHER_MODE /* 修改 _fdb_flash_erase read write 直接调用你自己的函数 */
// #define FDB_USING_FAL_MODE
// #define FDB_USING_FILE_MODE
// #define FDB_USING_FILE_POSIX_MODE


#define FDB_WRITE_GRAN           16  /* 写入粒度 1 16 32 64 128 units:bit */

// #define FDB_PRINT(db, ...)                 rt_kprintf(__VA_ARGS__)

#define FDB_PRINT(db, ...) (db)->printf(__VA_ARGS__)

// #define FDB_DEBUG_ENABLE

#endif /* _FDB_CFG_H_ */
