myfdb 代码结构与关键流程（摘要）
================================

目录与核心文件
--------------
- `fdb_cfg.h`：配置开关、打印宏、写粒度 `FDB_WRITE_GRAN`。
- `fdb_def.h`：公共宏、类型别名，重映射了 `mem*`/`str*` 到 RT-Thread，日志宏。
- `fdb_low_lvl.h`：底层对齐/状态表工具、Flash 读写接口声明（`_fdb_flash_read/_write/_erase`、状态读写等），以及 `calc_hash`、`fdb_calc_crc32`。
- `kvdb.c`：KV 引擎实现（扇区头/节点头格式、遍历、GC、初始化、读写）。

数据布局与状态机
----------------
- 扇区头 `sector_hdr_data`：`magic` + `store` 状态表 + `dirty` 状态表。`store` 取值 `EMPTY/USING/FULL`；`dirty` 取值 `FALSE/GC/TRUE`。
- KV 头 `kv_hdr_data`：
  - `magic` 固定 `KV_MAGIC_WORD`
  - `status` 状态表，取值 `UNUSED/PRE_WRITE/WRITE/PRE_DELETE/DELETED/ERR_HDR`
  - `len`：整条 KV 的长度（对齐到写粒度），用于推进 offset
  - `crc32`：覆盖 `len + name_hash + value_len + name + value`
  - `name_len`、`name_hash`、`value_len`
- 地址组织：扇区内从 `SECTOR_HDR_DATA_SIZE` 开始顺排 KV；`db->oldest_addr` 指向最老扇区基址；`db->empty_kv` 指向下一个可写 KV 起点。

关键函数与流程
--------------
- `calc_kv_crc32` / `check_kv_hdr`：计算并校验 KV 的 CRC32；`check_kv_hdr` 在需要时按块读取 name/value 验证。
- `align_write`：写入时按 `FDB_WRITE_GRAN` 对齐，末尾用擦除态填充补齐。
- `find_kv`：在单个扇区顺序扫描，支持查找已有 KV（匹配哈希+名字）或寻找第一个空洞以分配。
- `alloc_kv`：
  - 计算剩余空间 `calc_free_size`，低于 GC 阈值会触发 `do_gc`（最多重试 2 次）。
  - 依据 `db->empty_kv` 判断是否跨扇区，必要时把当前扇区标记 FULL，下一扇区标记 USING，并返回新 KV 地址。
- `kv_set`：
  - 查找旧值；若是删除操作 `value == NULL` 直接标记旧值 `DELETED`。
  - 为新值分配空间，写入头→对齐的 name→对齐的 value，成功后把新值标记 `WRITE`，再把旧值标记 `DELETED`（如果存在）。
  - 失败路径会保留旧值（除非前序逻辑手动标记了 PRE_DELETE）。
- `kv_get`：从 `oldest_addr` 开始遍历所有 USING/FULL 扇区，匹配哈希+名字，读出并校验 CRC，校验失败则标记该 KV 为 `DELETED`。
- `do_gc`（回收）：
  - 标记源扇区 `DIRTY_GC`，搬运其中 `WRITE` 的 KV 到目标空闲位置，搬运流程：旧 PRE_DELETE → 新 PRE_WRITE → 搬数据 → 新 WRITE → 旧 DELETED。
  - 扇区搬完后擦除并重置为 `EMPTY`，更新 `db->oldest_addr`。
- `check_all_kv`（启动自检）：
  - 全量遍历，校验 magic、状态与 CRC；遇到坏头记为 `ERR_HDR` 并可提前把扇区标记 FULL。
  - 处理未完成的 PRE_DELETE/PRE_WRITE，必要时将有效 KV 复制到新位置并删除旧的；确定新的 `info.new_kv_addr` 用作 `db->empty_kv` 候选。
  - 输出剩余空间占比。
- `flashdb_init`：扫描扇区头设定 `oldest_addr` / `empty_kv`，擦除坏扇区，必要时触发一次 GC，最后调用 `check_all_kv`，成功置 `db->init_ok=1`。

健壮性与注意事项
----------------
- 依赖底层 `_fdb_flash_*` 保证写擦正确性；状态表写入使用 `_fdb_write_status`，注意 KV 状态与扇区状态的表大小参数不同。
- `kv_hdr->len` 被信任用于推进 offset 和 GC 搬运，务必确保调用处对 `len` 做下限/上限/对齐检查（建议：`len >= KV_HDR_DATA_SIZE`、对齐写粒度、`offset+len <= sec_size-SECTOR_HDR_DATA_SIZE`），否则坏头可能导致越界或死循环。
- 启动顺序：如果系统掉电时存在 `PRE_DELETE` 或半写新值，建议先跑 `check_all_kv` 修复，再开放写入/GC，以避免 GC 在修复前丢掉唯一的旧值。

快速调用要点
------------
- 初始化：填好 `db->max_size`、`db->sec_size`、底层读写回调等后调用 `flashdb_init`。
- 写入：`kv_set(db, name, val, len)`，删除传 `value=NULL`。
- 读取：`kv_get(db, name, buf, buf_size)`，返回写入的长度或负错误码。
- 打印：`fdb_print` 遍历并打印所有 `WRITE` 状态的 KV。 
```c
#include "flashdb.h"
#include "rtthread.h"
#include "main.h"
// static struct fdb_db def_kvdb = {.sec_size = 1024, .max_size = 4 * 1024};
static struct fdb_kvdb def_kvdb;

int flash_get_value(char *name, void *value, uint8_t len)
{
    return kv_get(&def_kvdb, name, value, len);
}

int flash_set_value(char *name, void *value, uint8_t len)
{
    return kv_set(&def_kvdb, name, value, len);
}

void print_kv_cb(char *name, uint32_t data_addr, void *value, uint32_t len)
{
    rt_kprintf("kv name:%s\n", name);
    extern void LOG_HEX(uint32_t offset, uint8_t * buf, uint32_t size);
    LOG_HEX(data_addr, value, len);
}

void onchip_flash_init(void)
{
    def_kvdb.parent.sec_size = 2048;
    def_kvdb.parent.max_size = 3 * 2048;
    /* 如果没有printf  mykvdb\inc\fdb_def.h 改成下面的形式 
       #define FDB_LOG_E(db, ...)
       #define FDB_LOG_W(db, ...)
       #define FDB_LOG_I(db, ...)
       #define FDB_LOG_D(db, ...)
    */ 
    def_kvdb.parent.printf = rt_kprintf;//如果没有printf 
    flashdb_init(&def_kvdb);
}

static uint8_t fdb_value_buf[32];
static int cmd_fdb(int argc, char **argv)
{
    if (argc < 2)
    {
        return 0;
    }
    if (!rt_strcmp(argv[1], "print"))
    {
        fdb_print(&def_kvdb, fdb_value_buf, sizeof(fdb_value_buf));
    }
    else if (!rt_strcmp(argv[1], "set"))
    {
        kv_set(&def_kvdb, argv[2], argv[3], rt_strlen(argv[3]));
    }
    else if (!rt_strcmp(argv[1], "test"))
    {
        kv_set(&def_kvdb, "test", (void *)0x08000000, 512);
    }
    return 0;
}
```