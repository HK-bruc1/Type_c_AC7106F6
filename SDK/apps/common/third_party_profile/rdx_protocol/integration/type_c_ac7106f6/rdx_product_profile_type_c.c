#include "app_config.h"

#if TCFG_RDX_ENABLE

#include <string.h>

#include "rdx/ports/rdx_product_profile_port.h"
#include "rdx_protocol/integration/type_c_ac7106f6/rdx_product_compat_profile.h"
#include "rdx/rdx_wire_defs.h"

static const char *const rdx_type_c_app_keys[] = {
    RDX_COMPAT_APP_KEY_LIST
};

typedef char rdx_local_name_size_check[
    (sizeof(RDX_COMPAT_LOCAL_NAME) > 1
     && sizeof(RDX_COMPAT_LOCAL_NAME) - 1 <= RDX_BLE_NAME_MAX_LEN) ? 1 : -1];
typedef char rdx_local_name_suffix_size_check[
    (sizeof(RDX_COMPAT_LOCAL_NAME) > 1
     && sizeof(RDX_COMPAT_LOCAL_NAME) - 1 + 1 + 4
            <= RDX_BLE_NAME_MAX_LEN) ? 1 : -1];
typedef char rdx_wifi_mac_size_check[
    (sizeof(RDX_COMPAT_WIFI_MAC) - 1 == RDX_MAC_STRING_SIZE) ? 1 : -1];
typedef char rdx_app_key_count_check[
    (sizeof(rdx_type_c_app_keys) / sizeof(rdx_type_c_app_keys[0]) <= 0xff)
    ? 1 : -1];

static const struct rdx_product_profile rdx_type_c_product_profile = {
    .local_name = RDX_COMPAT_LOCAL_NAME,
    .product_code = RDX_COMPAT_PRODUCT_CODE,
    .factory_code = RDX_COMPAT_FACTORY_CODE,
    .product_type = RDX_COMPAT_PRODUCT_TYPE,
    .self_mark = RDX_COMPAT_SELF_MARK,
    .identity_format = RDX_COMPAT_IDENTITY_FORMAT,
    .device_ability = RDX_COMPAT_DEVICE_ABILITY,
    .include_protocol_version = RDX_COMPAT_INCLUDE_PROTOCOL_VERSION,
    .protocol_version = RDX_COMPAT_PROTOCOL_VERSION,
    .fixed_battery_level = RDX_COMPAT_FIXED_BATTERY_LEVEL,
    .ble_adv_interval = RDX_BLE_ADV_INTERVAL,
    .firmware_version = RDX_COMPAT_FIRMWARE_VERSION,
    .hardware_version = RDX_COMPAT_HARDWARE_VERSION,
    .wifi_mac = RDX_COMPAT_WIFI_MAC,
    .app_keys = rdx_type_c_app_keys,
    .app_key_count = sizeof(rdx_type_c_app_keys)
                     / sizeof(rdx_type_c_app_keys[0]),
};

const struct rdx_product_profile *rdx_product_profile_get(void)
{
    return &rdx_type_c_product_profile;
}

int rdx_product_profile_build_default_ble_name(const char *auth_key,
                                               char *buffer,
                                               uint16_t buffer_size)
{
    const char *base_name = rdx_type_c_product_profile.local_name;
    uint16_t base_len;
    uint16_t total_len;

    if (!auth_key || !buffer || !buffer_size
        || strlen(auth_key) < RDX_AUTH_KEY_STRING_SIZE) {
        return -1;
    }

    /* Preserve the legacy Type-C convention: "<name> <AuthKey-last-4>". */
    base_len = strlen(base_name);
    total_len = base_len + 1 + 4;
    if (!base_len || total_len > RDX_BLE_NAME_MAX_LEN
        || total_len >= buffer_size) {
        memset(buffer, 0, buffer_size);
        return -1;
    }
    memcpy(buffer, base_name, base_len);
    buffer[base_len] = ' ';
    memcpy(buffer + base_len + 1,
           auth_key + RDX_AUTH_KEY_STRING_SIZE - 4, 4);
    buffer[total_len] = '\0';
    return 0;
}

#endif
