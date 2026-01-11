/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public APIs.
 */

#ifndef _FLASHDB_H_
#define _FLASHDB_H_

#include <fdb_cfg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// #include <stdio.h>
// #include <time.h>
#include <fdb_def.h>

#ifdef FDB_USING_FAL_MODE
// #include <fal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// /* FlashDB database API */
fdb_err_t fdb_kvdb_init(fdb_kvdb_t db, const char *name, const char *path,
                        struct fdb_default_kv *default_kv, void *user_data);
void fdb_kvdb_control(fdb_kvdb_t db, int cmd, void *arg);
// fdb_err_t fdb_kvdb_check(fdb_kvdb_t db);
fdb_err_t fdb_kvdb_deinit(fdb_kvdb_t db);
// fdb_err_t fdb_tsdb_init(fdb_tsdb_t db, const char *name, const char *path,
//                         fdb_get_time get_time, size_t max_len, void
//                         *user_data);
// void fdb_tsdb_control(fdb_tsdb_t db, int cmd, void *arg);
// fdb_err_t fdb_tsdb_deinit(fdb_tsdb_t db);

// /* blob API */
fdb_blob_t fdb_blob_make(fdb_blob_t blob, const void *value_buf,
                         size_t buf_len);
size_t fdb_blob_read(fdb_db_t db, fdb_blob_t blob);

// /* Key-Value API like a KV DB */
fdb_err_t fdb_kv_set(fdb_kvdb_t db, const char *key, const char *value);
char *fdb_kv_get(fdb_kvdb_t db, const char *key);
fdb_err_t fdb_kv_set_blob(fdb_kvdb_t db, const char *key, fdb_blob_t blob);
size_t fdb_kv_get_blob(fdb_kvdb_t db, const char *key, fdb_blob_t blob);
fdb_err_t fdb_kv_del(fdb_kvdb_t db, const char *key);
fdb_kv_t fdb_kv_get_obj(fdb_kvdb_t db, const char *key, fdb_kv_t kv);
fdb_blob_t fdb_kv_to_blob(fdb_kv_t kv, fdb_blob_t blob);
fdb_err_t fdb_kv_set_default(fdb_kvdb_t db);

fdb_kv_iterator_t fdb_kv_iterator_init(fdb_kvdb_t db, fdb_kv_iterator_t itr);
bool fdb_kv_iterate(fdb_kvdb_t db, fdb_kv_iterator_t itr);

int flashdb_init(fdb_kvdb_t kvdb);
int kv_set(fdb_kvdb_t kvdb, const char *name, const void *value,
           uint32_t value_len);
int kv_get(fdb_kvdb_t kvdb, const char *name, const void *value,
           uint32_t value_len);
int fdb_print(fdb_kvdb_t kvdb, void *read_value_buf, uint32_t buf_size);
#ifdef __cplusplus
}
#endif

#endif /* _FLASHDB_H_ */
