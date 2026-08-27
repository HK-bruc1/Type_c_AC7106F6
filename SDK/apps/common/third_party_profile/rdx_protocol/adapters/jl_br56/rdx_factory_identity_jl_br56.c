#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "fs/sdfile.h"
#include "cpu/periph/crc.h"
#include "rdx/ports/rdx_factory_identity_port.h"

struct rdx_auth_code_block {
    uint16_t crc;
    uint16_t len;
    uint8_t data[0];
} __attribute__((packed));

int rdx_platform_read_auth_code(uint8_t *buffer,
                                uint16_t buffer_size,
                                uint16_t *data_len)
{
    uint32_t flash_capacity;
    uint32_t flash_addr;
    const struct rdx_auth_code_block *block;
    uint16_t len;

    if (!buffer || !data_len) {
        return RDX_AUTH_CODE_ERR_INVALID_ARGUMENT;
    }
    *data_len = 0;

    flash_capacity = sdfile_get_disk_capacity();
    if (flash_capacity < RDX_AUTH_CODE_BLOCK_SIZE) {
        return RDX_AUTH_CODE_ERR_INVALID_CAPACITY;
    }
    flash_addr = flash_capacity - RDX_AUTH_CODE_BLOCK_SIZE
                 + RDX_AUTH_CODE_BLOCK_OFFSET;
    block = (const struct rdx_auth_code_block *)
            sdfile_flash_addr2cpu_addr(flash_addr);
    if (!block) {
        return RDX_AUTH_CODE_ERR_ADDRESS_MAP;
    }

    len = block->len;
    if (len == 0xffffu || len == 0u) {
        return RDX_AUTH_CODE_ERR_NOT_PRESENT;
    }
    if (len > RDX_AUTH_CODE_MAX_PAYLOAD_LEN) {
        return RDX_AUTH_CODE_ERR_INVALID_LENGTH;
    }
    if (buffer_size < len) {
        return RDX_AUTH_CODE_ERR_BUFFER_TOO_SMALL;
    }
    if (CRC16((const uint8_t *)&block->len,
              (uint32_t)sizeof(block->len) + len) != block->crc) {
        return RDX_AUTH_CODE_ERR_CRC;
    }

    memcpy(buffer, block->data, len);
    *data_len = len;
    return 0;
}

#endif
