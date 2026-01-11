#ifndef MCUBOOT_LOGGING_H_
#define MCUBOOT_LOGGING_H_
#include "main.h"

#define DBG_TAG "boot"
#define DBG_LVL DBG_LOG

#define MCUBOOT_LOG_ERR(...) do {LOG_E(__VA_ARGS__)} while (0)
#define MCUBOOT_LOG_WRN(...) do {LOG_W(__VA_ARGS__)} while (0)
#define MCUBOOT_LOG_INF(...) do {LOG_I(__VA_ARGS__)} while (0)
#define MCUBOOT_LOG_DBG(...) do {LOG_D(__VA_ARGS__)} while (0)
#define MCUBOOT_LOG_SIM(...) do {LOG_I(__VA_ARGS__)} while (0)

#define MCUBOOT_LOG_MODULE_DECLARE(module)
#define MCUBOOT_LOG_MODULE_REGISTER(module)

#endif /* MCUBOOT_LOGGING_H_ */
