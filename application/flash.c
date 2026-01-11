
#include "flashdb.h"
#include "rtthread.h"
#include "main.h"
// static struct fdb_db def_kvdb = {.sec_size = 1024, .max_size = 4 * 1024};
static struct fdb_kvdb def_kvdb;

int flash_get_value(char *name, void *value, uint8_t len)
{
    return 0;
    return kv_get(&def_kvdb, name, value, len);
}

int flash_set_value(char *name, void *value, uint8_t len)
{
    return 0;
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
    fmc_unlock();
    FMC_WSEN |= FMC_WSEN_BPEN;
    fmc_lock();
    // def_kvdb.parent.sec_size = 2048;
    // def_kvdb.parent.max_size = 3 * 2048;
    // def_kvdb.parent.printf = rt_kprintf;//如果没有printf
    // flashdb_init(&def_kvdb);
}
INIT_COMPONENT_EXPORT(onchip_flash_init);
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
#include "finsh.h"
MSH_CMD_EXPORT_ALIAS(cmd_fdb, fdb, fdb ops);
