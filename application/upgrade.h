#pragma  once
#include <stdint.h>

/* Firmware verification result */
typedef enum {
    FW_VERIFY_OK = 0,           /* Firmware is valid */
    FW_VERIFY_NO_INFO = -1,     /* No firmware info found in FlashDB */
    FW_VERIFY_CRC_FAIL = -2,    /* CRC32 mismatch */
    FW_VERIFY_INVALID_LEN = -3  /* Invalid firmware length */
} fw_verify_result_t;

/* Firmware information structure */
typedef struct {
    uint32_t length;    /* Firmware length in bytes */
    uint32_t crc32;     /* CRC32 checksum */
} fw_info_t;

void JumpToApplication(uint32_t app_addr);
int verify_app_firmware(void);
void save_firmware_info(uint32_t length, uint32_t crc32);