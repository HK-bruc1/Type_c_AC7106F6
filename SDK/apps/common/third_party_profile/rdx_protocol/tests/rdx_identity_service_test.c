#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rdx/ports/rdx_factory_identity_port.h"
#include "rdx/ports/rdx_product_profile_port.h"
#include "rdx/rdx_factory_identity.h"
#include "rdx/rdx_identity_service.h"
#include "rdx/rdx_device_state_service.h"
#include "rdx/rdx_wire_defs.h"

static int fake_provider_result;

int rdx_platform_read_auth_code(uint8_t *buffer,
                                uint16_t buffer_size,
                                uint16_t *data_len)
{
    (void)buffer;
    (void)buffer_size;
    if (data_len) {
        *data_len = 0;
    }
    return fake_provider_result;
}

rdx_bound_state_t rdx_device_state_get_bound(void)
{
    return RDX_BOUND_STATE_UNBOUND;
}

static void test_unprogrammed_identity_uses_zero_values(void)
{
    static const char expected_read_value[] =
        "C,000000000000,000000000000,000000000000,"
        "000000000000000000000000,0000000000000000,000000000000";
    const uint8_t *read_value;
    const uint8_t *ble_mac;
    char default_name[RDX_BLE_NAME_MAX_LEN + 1];
    uint8_t i;

    fake_provider_result = RDX_AUTH_CODE_ERR_NOT_PRESENT;
    assert(rdx_identity_init() == 0);
    assert(!strcmp(rdx_identity_get_auth_key(),
                   "000000000000000000000000"));
    assert(!strcmp(rdx_identity_get_ble_mac_string(), "000000000000"));
    assert(!strcmp(rdx_identity_get_label_sn(), "0000000000000000"));
    ble_mac = rdx_identity_get_ble_mac();
    for (i = 0; i < RDX_FACTORY_IDENTITY_BLE_MAC_LEN; i++) {
        assert(ble_mac[i] == 0);
    }
    assert(rdx_identity_get_read_value(&read_value)
           == sizeof(expected_read_value) - 1);
    assert(!memcmp(read_value, expected_read_value,
                   sizeof(expected_read_value) - 1));
    assert(rdx_product_profile_build_default_ble_name(
               rdx_identity_get_auth_key(), default_name,
               sizeof(default_name)) == 0);
    assert(!strcmp(default_name, "IKKO 0000"));
}

static void test_corrupt_or_unavailable_provider_errors_are_not_hidden(void)
{
    fake_provider_result = RDX_AUTH_CODE_ERR_CRC;
    assert(rdx_identity_init() == RDX_AUTH_CODE_ERR_CRC);
}

int main(void)
{
    test_unprogrammed_identity_uses_zero_values();
    test_corrupt_or_unavailable_provider_errors_are_not_hidden();
    puts("RDX identity service tests passed");
    return 0;
}
