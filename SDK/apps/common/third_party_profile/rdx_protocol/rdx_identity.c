#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "btstack/le/ble_api.h"
#include "rdx_identity.h"
#include "rdx_mvp0_compat_config.h"
#include "rdx_protocol_defs.h"

#define RDX_MAC_SIZE                         6
#define RDX_CASE_IDENTITY_SIZE              95

static u8 rdx_ble_mac[RDX_MAC_SIZE];
static char rdx_ble_mac_string[RDX_MAC_STRING_SIZE + 1];
static u8 rdx_read_value[RDX_CASE_IDENTITY_SIZE + 1];
static u16 rdx_read_value_len;

typedef char rdx_auth_key_size_check[
    (sizeof(RDX_COMPAT_AUTH_KEY) - 1 == RDX_AUTH_KEY_STRING_SIZE) ? 1 : -1];
typedef char rdx_wifi_mac_size_check[
    (sizeof(RDX_COMPAT_WIFI_MAC) - 1 == RDX_MAC_STRING_SIZE) ? 1 : -1];
typedef char rdx_label_sn_size_check[
    (sizeof(RDX_COMPAT_LABEL_SN) - 1 == RDX_LABEL_SN_STRING_SIZE) ? 1 : -1];

static void rdx_identity_build_read_value(void)
{
#if RDX_COMPAT_PRODUCT_IS_CHARGE_CASE
    int len;

    len = snprintf((char *)rdx_read_value, sizeof(rdx_read_value),
                   "C,000000000000,000000000000,%02X%02X%02X%02X%02X%02X,%s,%s,000000000000",
                   rdx_ble_mac[5], rdx_ble_mac[4], rdx_ble_mac[3],
                   rdx_ble_mac[2], rdx_ble_mac[1], rdx_ble_mac[0],
                   RDX_COMPAT_AUTH_KEY, RDX_COMPAT_LABEL_SN);
    if (len < 0) {
        rdx_read_value_len = 0;
    } else if (len > RDX_CASE_IDENTITY_SIZE) {
        rdx_read_value_len = RDX_CASE_IDENTITY_SIZE;
    } else {
        rdx_read_value_len = len;
    }
#else
    rdx_read_value_len = sizeof(RDX_COMPAT_AUTH_KEY) - 1;
    memcpy(rdx_read_value, RDX_COMPAT_AUTH_KEY, rdx_read_value_len);
#endif
}

int rdx_identity_init(void)
{
    int ret;

    memset(rdx_ble_mac, 0, sizeof(rdx_ble_mac));
    ret = le_controller_get_mac(rdx_ble_mac);
    if (ret) {
        printf("[RDX] read BLE MAC failed=%d\n", ret);
        return ret;
    }
    snprintf(rdx_ble_mac_string, sizeof(rdx_ble_mac_string),
             "%02X%02X%02X%02X%02X%02X",
             rdx_ble_mac[5], rdx_ble_mac[4], rdx_ble_mac[3],
             rdx_ble_mac[2], rdx_ble_mac[1], rdx_ble_mac[0]);
    memset(rdx_read_value, 0, sizeof(rdx_read_value));
    rdx_identity_build_read_value();
    return 0;
}

const char *rdx_identity_get_local_name(void)
{
    return RDX_COMPAT_LOCAL_NAME;
}

const u8 *rdx_identity_get_ble_mac(void)
{
    return rdx_ble_mac;
}

const char *rdx_identity_get_auth_key(void)
{
    return RDX_COMPAT_AUTH_KEY;
}

const char *rdx_identity_get_ble_mac_string(void)
{
    return rdx_ble_mac_string;
}

const char *rdx_identity_get_wifi_mac_string(void)
{
    return RDX_COMPAT_WIFI_MAC;
}

const char *rdx_identity_get_label_sn(void)
{
    return RDX_COMPAT_LABEL_SN;
}

u16 rdx_identity_get_read_value(const u8 **data)
{
    if (data) {
        *data = rdx_read_value;
    }
    return rdx_read_value_len;
}

u8 rdx_identity_fill_manufacturer_data(u8 *buffer, u8 buffer_size)
{
    u8 offset = 0;
    u8 i;
    u32 ability = RDX_COMPAT_DEVICE_ABILITY;
    const u8 required = (sizeof(RDX_COMPAT_PRODUCT_CODE) - 1)
                        + RDX_MAC_SIZE
                        + (sizeof(RDX_COMPAT_FACTORY_CODE) - 1)
                        + 1 + 4
#if RDX_COMPAT_INCLUDE_PROTOCOL_VERSION
                        + 1
#endif
                        + (sizeof(RDX_COMPAT_PRODUCT_TYPE) - 1)
                        + (sizeof(RDX_COMPAT_SELF_MARK) - 1);

    if (!buffer || buffer_size < required) {
        return 0;
    }

    memcpy(buffer + offset, RDX_COMPAT_PRODUCT_CODE, sizeof(RDX_COMPAT_PRODUCT_CODE) - 1);
    offset += sizeof(RDX_COMPAT_PRODUCT_CODE) - 1;

    for (i = 0; i < RDX_MAC_SIZE; i++) {
        buffer[offset++] = rdx_ble_mac[RDX_MAC_SIZE - 1 - i];
    }

    memcpy(buffer + offset, RDX_COMPAT_FACTORY_CODE, sizeof(RDX_COMPAT_FACTORY_CODE) - 1);
    offset += sizeof(RDX_COMPAT_FACTORY_CODE) - 1;

    buffer[offset++] = RDX_COMPAT_BOUND_STATE ? BIT(6) : 0;
    buffer[offset++] = ability & 0xff;
    buffer[offset++] = (ability >> 8) & 0xff;
    buffer[offset++] = (ability >> 16) & 0xff;
    buffer[offset++] = (ability >> 24) & 0xff;

#if RDX_COMPAT_INCLUDE_PROTOCOL_VERSION
    buffer[offset++] = RDX_COMPAT_PROTOCOL_VERSION;
#endif

    memcpy(buffer + offset, RDX_COMPAT_PRODUCT_TYPE, sizeof(RDX_COMPAT_PRODUCT_TYPE) - 1);
    offset += sizeof(RDX_COMPAT_PRODUCT_TYPE) - 1;
    memcpy(buffer + offset, RDX_COMPAT_SELF_MARK, sizeof(RDX_COMPAT_SELF_MARK) - 1);
    offset += sizeof(RDX_COMPAT_SELF_MARK) - 1;

    return offset;
}

#endif
