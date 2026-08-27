#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rdx/ports/rdx_factory_identity_port.h"

static uint8_t fake_flash[RDX_AUTH_CODE_BLOCK_SIZE];
static uint16_t fake_crc;

uint32_t sdfile_get_disk_capacity(void)
{
    return RDX_AUTH_CODE_BLOCK_SIZE;
}

void *sdfile_flash_addr2cpu_addr(uint32_t addr)
{
    (void)addr;
    return fake_flash;
}

uint16_t CRC16(const uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    return fake_crc;
}

static void set_le16(uint8_t *field, uint16_t value)
{
    field[0] = value & 0xff;
    field[1] = value >> 8;
}

static void test_unprogrammed_blocks(void)
{
    uint8_t output[128];
    uint16_t output_len;

    memset(fake_flash, 0xff, sizeof(fake_flash));
    assert(rdx_platform_read_auth_code(output, sizeof(output), &output_len)
           == RDX_AUTH_CODE_ERR_NOT_PRESENT);

    memset(fake_flash, 0, sizeof(fake_flash));
    assert(rdx_platform_read_auth_code(output, sizeof(output), &output_len)
           == RDX_AUTH_CODE_ERR_NOT_PRESENT);
}

static void test_valid_and_invalid_blocks(void)
{
    static const uint8_t payload[] = "payload";
    uint8_t output[128];
    uint16_t output_len;

    memset(fake_flash, 0, sizeof(fake_flash));
    set_le16(fake_flash + 2, sizeof(payload) - 1);
    fake_crc = 0x1234;
    set_le16(fake_flash, fake_crc);
    memcpy(fake_flash + 4, payload, sizeof(payload) - 1);
    assert(rdx_platform_read_auth_code(output, sizeof(output), &output_len)
           == 0);
    assert(output_len == sizeof(payload) - 1);
    assert(!memcmp(output, payload, output_len));

    set_le16(fake_flash + 2, RDX_AUTH_CODE_MAX_PAYLOAD_LEN + 1);
    assert(rdx_platform_read_auth_code(output, sizeof(output), &output_len)
           == RDX_AUTH_CODE_ERR_INVALID_LENGTH);

    set_le16(fake_flash + 2, sizeof(payload) - 1);
    fake_crc = 0x4321;
    assert(rdx_platform_read_auth_code(output, sizeof(output), &output_len)
           == RDX_AUTH_CODE_ERR_CRC);
}

int main(void)
{
    test_unprogrammed_blocks();
    test_valid_and_invalid_blocks();
    puts("RDX factory identity provider tests passed");
    return 0;
}
