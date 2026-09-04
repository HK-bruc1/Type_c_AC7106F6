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
static uint8_t fake_provider_payload[128];
static uint16_t fake_provider_payload_len;

int rdx_device_identity_read_ble_mac(uint8_t ble_mac[6])
{
    static const uint8_t fake_ble_mac[] = { 0x01, 0x02, 0x03,
                                             0x04, 0x05, 0x06 };

    memcpy(ble_mac, fake_ble_mac, sizeof(fake_ble_mac));
    return 0;
}

int rdx_platform_read_auth_code(uint8_t *buffer,
                                uint16_t buffer_size,
                                uint16_t *data_len)
{
    if (data_len) {
        *data_len = fake_provider_payload_len;
    }
    if (fake_provider_result == 0) {
        assert(buffer_size >= fake_provider_payload_len);
        memcpy(buffer, fake_provider_payload, fake_provider_payload_len);
    }
    return fake_provider_result;
}

rdx_bound_state_t rdx_device_state_get_bound(void)
{
    return RDX_BOUND_STATE_UNBOUND;
}

static void test_unprogrammed_identity_uses_zero_identity_and_default_mac(void)
{
    static const char expected_read_value[] =
        "C,000000000000,000000000000,060504030201,"
        "000000000000000000000000,0000000000000000,000000000000";
    const uint8_t *read_value;
    const uint8_t *ble_mac;
    char default_name[RDX_BLE_NAME_MAX_LEN + 1];
    uint8_t i;

    fake_provider_result = RDX_AUTH_CODE_ERR_NOT_PRESENT;
    assert(rdx_identity_init() == 0);
    assert(!strcmp(rdx_identity_get_auth_key(),
                   "000000000000000000000000"));
    assert(!strcmp(rdx_identity_get_ble_mac_string(), "060504030201"));
    assert(!strcmp(rdx_identity_get_label_sn(), "0000000000000000"));
    ble_mac = rdx_identity_get_ble_mac();
    for (i = 0; i < RDX_FACTORY_IDENTITY_BLE_MAC_LEN; i++) {
        assert(ble_mac[i] == i + 1);
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

static void test_provider_errors_fall_back_without_stopping(void)
{
    fake_provider_result = RDX_AUTH_CODE_ERR_CRC;
    assert(rdx_identity_init() == 0);
    assert(!strcmp(rdx_identity_get_ble_mac_string(), "060504030201"));
    assert(!strcmp(rdx_identity_get_auth_key(),
                   "000000000000000000000000"));

    fake_provider_result = 0;
    fake_provider_payload_len = 1;
    fake_provider_payload[0] = 'x';
    assert(rdx_identity_init() == 0);
    assert(!strcmp(rdx_identity_get_ble_mac_string(), "060504030201"));
}

static void test_reference_prefix_read_accepts_trailing_payload(void)
{
    static const char record[] =
        "T2620NV20260624XHM000001,E8392C53D3C3,T2620NVXHM000001";

    fake_provider_result = 0;
    fake_provider_payload_len = RDX_FACTORY_IDENTITY_RECORD_LEN + 2;
    memcpy(fake_provider_payload, record, sizeof(record) - 1);
    fake_provider_payload[sizeof(record) - 1] = '\r';
    fake_provider_payload[sizeof(record)] = '\n';

    assert(rdx_identity_init() == 0);
    assert(!strcmp(rdx_identity_get_auth_key(),
                   "T2620NV20260624XHM000001"));
    assert(!strcmp(rdx_identity_get_ble_mac_string(), "E8392C53D3C3"));
    assert(!strcmp(rdx_identity_get_label_sn(), "T2620NVXHM000001"));
}

static void test_illegal_record_falls_back_without_stopping(void)
{
    fake_provider_result = 0;
    fake_provider_payload_len = RDX_FACTORY_IDENTITY_RECORD_LEN;
    memset(fake_provider_payload, 'A', fake_provider_payload_len);
    fake_provider_payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN] = ',';
    memcpy(fake_provider_payload + RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1,
           "E8392C53D3C3", RDX_FACTORY_IDENTITY_MAC_TEXT_LEN);
    fake_provider_payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1
                          + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN] = ',';
    memset(fake_provider_payload + RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1
           + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN + 1,
           'B', RDX_FACTORY_IDENTITY_LABEL_SN_LEN);
    fake_provider_payload[0] = '#';

    assert(rdx_identity_init() == 0);
    assert(!strcmp(rdx_identity_get_auth_key(),
                   "000000000000000000000000"));
    assert(!strcmp(rdx_identity_get_ble_mac_string(), "060504030201"));
    assert(!strcmp(rdx_identity_get_label_sn(), "0000000000000000"));
}

static void test_zero_placeholder_uses_zero_values(void)
{
    fake_provider_result = 0;
    fake_provider_payload_len = 1;
    fake_provider_payload[0] = '0';
    assert(rdx_identity_init() == 0);
    assert(!strcmp(rdx_identity_get_auth_key(),
                   "000000000000000000000000"));
    assert(!strcmp(rdx_identity_get_ble_mac_string(), "060504030201"));
    assert(!strcmp(rdx_identity_get_label_sn(), "0000000000000000"));

    fake_provider_payload_len = 2;
    fake_provider_payload[0] = '0';
    fake_provider_payload[1] = '\0';
    assert(rdx_identity_init() == 0);
}

int main(void)
{
    test_unprogrammed_identity_uses_zero_identity_and_default_mac();
    test_zero_placeholder_uses_zero_values();
    test_provider_errors_fall_back_without_stopping();
    test_reference_prefix_read_accepts_trailing_payload();
    test_illegal_record_falls_back_without_stopping();
    puts("RDX identity service tests passed");
    return 0;
}
