#ifndef RDX_PRODUCT_PROFILE_PORT_H
#define RDX_PRODUCT_PROFILE_PORT_H

#include <stdint.h>

enum rdx_identity_format {
    RDX_PRODUCT_IDENTITY_FORMAT_AUTH_KEY_ONLY = 1,
    RDX_PRODUCT_IDENTITY_FORMAT_CASE_COMPOSITE = 2,
};

struct rdx_product_profile {
    const char *local_name;
    const char *product_code;
    const char *factory_code;
    const char *product_type;
    const char *self_mark;
    enum rdx_identity_format identity_format;
    uint32_t device_ability;
    uint8_t include_protocol_version;
    uint8_t protocol_version;
    uint8_t fixed_battery_level;
    uint16_t ble_adv_interval;
    const char *firmware_version;
    const char *hardware_version;
    const char *wifi_mac;
    const char *const *app_keys;
    uint8_t app_key_count;
};

const struct rdx_product_profile *rdx_product_profile_get(void);

/*
 * Build the product's factory-derived default BLE name.  The application
 * service supplies the validated AuthKey; the product integration owns the
 * naming policy (including any serial suffix and separators).
 */
int rdx_product_profile_build_default_ble_name(const char *auth_key,
                                               char *buffer,
                                               uint16_t buffer_size);

#endif
