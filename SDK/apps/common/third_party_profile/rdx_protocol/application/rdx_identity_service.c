#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE
#include "rdx/ports/rdx_factory_identity_port.h"
#include "rdx/ports/rdx_product_profile_port.h"
#include "rdx/rdx_device_state_service.h"
#include "rdx/rdx_factory_identity.h"
#include "rdx/rdx_identity_service.h"

#define RDX_CASE_IDENTITY_SIZE              95

static uint8_t rdx_ble_mac[RDX_FACTORY_IDENTITY_BLE_MAC_LEN];
static char rdx_ble_mac_string[RDX_FACTORY_IDENTITY_MAC_TEXT_LEN + 1];
static char rdx_auth_key[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1];
static char rdx_label_sn[RDX_FACTORY_IDENTITY_LABEL_SN_LEN + 1];
static uint8_t rdx_read_value[RDX_CASE_IDENTITY_SIZE + 1];
static uint16_t rdx_read_value_len;

static void rdx_identity_build_read_value(void);

static void rdx_identity_set_zero_factory_identity(void)
{
    memset(rdx_ble_mac, 0, sizeof(rdx_ble_mac));
    memset(rdx_ble_mac_string, '0', sizeof(rdx_ble_mac_string) - 1);
    rdx_ble_mac_string[sizeof(rdx_ble_mac_string) - 1] = '\0';
    memset(rdx_auth_key, '0', sizeof(rdx_auth_key) - 1);
    rdx_auth_key[sizeof(rdx_auth_key) - 1] = '\0';
    memset(rdx_label_sn, '0', sizeof(rdx_label_sn) - 1);
    rdx_label_sn[sizeof(rdx_label_sn) - 1] = '\0';
    rdx_identity_build_read_value();
}

static void rdx_identity_build_read_value(void)
{
    const struct rdx_product_profile *profile = rdx_product_profile_get();
    int len;

    switch (profile->identity_format) {
    case RDX_PRODUCT_IDENTITY_FORMAT_CASE_COMPOSITE:
        len = snprintf((char *)rdx_read_value, sizeof(rdx_read_value),
                       "C,000000000000,000000000000,%02X%02X%02X%02X%02X%02X,%s,%s,000000000000",
                       rdx_ble_mac[5], rdx_ble_mac[4], rdx_ble_mac[3],
                       rdx_ble_mac[2], rdx_ble_mac[1], rdx_ble_mac[0],
                       rdx_auth_key, rdx_label_sn);
        if (len < 0) {
            rdx_read_value_len = 0;
        } else if (len > RDX_CASE_IDENTITY_SIZE) {
            rdx_read_value_len = RDX_CASE_IDENTITY_SIZE;
        } else {
            rdx_read_value_len = len;
        }
        break;
    case RDX_PRODUCT_IDENTITY_FORMAT_AUTH_KEY_ONLY:
        rdx_read_value_len = strlen(rdx_auth_key);
        memcpy(rdx_read_value, rdx_auth_key, rdx_read_value_len);
        break;
    default:
        rdx_read_value_len = 0;
        printf("[RDX] unsupported identity format=%u\n",
               profile->identity_format);
        break;
    }
}

int rdx_identity_init(void)
{
    uint8_t auth_code[RDX_AUTH_CODE_MAX_PAYLOAD_LEN];
    uint16_t auth_code_len = 0;
    struct rdx_factory_identity factory_identity;
    int ret;

    memset(rdx_ble_mac, 0, sizeof(rdx_ble_mac));
    memset(rdx_ble_mac_string, 0, sizeof(rdx_ble_mac_string));
    memset(rdx_auth_key, 0, sizeof(rdx_auth_key));
    memset(rdx_label_sn, 0, sizeof(rdx_label_sn));
    memset(rdx_read_value, 0, sizeof(rdx_read_value));
    rdx_read_value_len = 0;

    ret = rdx_platform_read_auth_code(auth_code, sizeof(auth_code),
                                      &auth_code_len);
    if (ret) {
        if (ret == RDX_AUTH_CODE_ERR_NOT_PRESENT) {
            rdx_identity_set_zero_factory_identity();
            printf("[RDX][WARN] factory identity not present; using zero identity\n");
#if defined(CONFIG_DEBUG_ENABLE) && CONFIG_DEBUG_ENABLE
            printf("[RDX] factory AuthKey=%s\n", rdx_auth_key);
            printf("[RDX] factory BLE_MAC=%s\n", rdx_ble_mac_string);
            printf("[RDX] factory Label_SN=%s\n", rdx_label_sn);
#endif
            return 0;
        }
        printf("[RDX] factory identity invalid stage=block code=%d\n", ret);
        return ret;
    }
    ret = rdx_factory_identity_parse(auth_code, auth_code_len,
                                     &factory_identity);
    if (ret) {
        printf("[RDX] factory identity invalid stage=record code=%d\n", ret);
        return ret;
    }

    memcpy(rdx_ble_mac, factory_identity.ble_mac, sizeof(rdx_ble_mac));
    memcpy(rdx_ble_mac_string, factory_identity.ble_mac_string,
           sizeof(rdx_ble_mac_string));
    memcpy(rdx_auth_key, factory_identity.auth_key, sizeof(rdx_auth_key));
    memcpy(rdx_label_sn, factory_identity.label_sn, sizeof(rdx_label_sn));
    rdx_identity_build_read_value();

#if defined(CONFIG_DEBUG_ENABLE) && CONFIG_DEBUG_ENABLE
    printf("[RDX] factory identity loaded\n");
    printf("[RDX] factory AuthKey=%s\n", rdx_auth_key);
    printf("[RDX] factory BLE_MAC=%s\n", rdx_ble_mac_string);
    printf("[RDX] factory Label_SN=%s\n", rdx_label_sn);
#endif
    return 0;
}

const uint8_t *rdx_identity_get_ble_mac(void)
{
    return rdx_ble_mac;
}

const char *rdx_identity_get_auth_key(void)
{
    return rdx_auth_key;
}

const char *rdx_identity_get_ble_mac_string(void)
{
    return rdx_ble_mac_string;
}

const char *rdx_identity_get_wifi_mac_string(void)
{
    return rdx_product_profile_get()->wifi_mac;
}

const char *rdx_identity_get_label_sn(void)
{
    return rdx_label_sn;
}

uint16_t rdx_identity_get_read_value(const uint8_t **data)
{
    if (data) {
        *data = rdx_read_value;
    }
    return rdx_read_value_len;
}

uint8_t rdx_identity_fill_manufacturer_data(uint8_t *buffer,
                                            uint8_t buffer_size)
{
    const struct rdx_product_profile *profile = rdx_product_profile_get();
    const uint16_t product_code_len = strlen(profile->product_code);
    const uint16_t factory_code_len = strlen(profile->factory_code);
    const uint16_t product_type_len = strlen(profile->product_type);
    const uint16_t self_mark_len = strlen(profile->self_mark);
    const uint16_t required = product_code_len
                              + RDX_FACTORY_IDENTITY_BLE_MAC_LEN
                              + factory_code_len + 1 + 4
                              + (profile->include_protocol_version ? 1 : 0)
                              + product_type_len + self_mark_len;
    uint8_t offset = 0;
    uint8_t i;
    uint8_t bound = rdx_device_state_get_bound();
    uint32_t ability = profile->device_ability;

    if (!buffer || buffer_size < required) {
        return 0;
    }

    memcpy(buffer + offset, profile->product_code, product_code_len);
    offset += product_code_len;

    for (i = 0; i < RDX_FACTORY_IDENTITY_BLE_MAC_LEN; i++) {
        buffer[offset++] =
            rdx_ble_mac[RDX_FACTORY_IDENTITY_BLE_MAC_LEN - 1 - i];
    }

    memcpy(buffer + offset, profile->factory_code, factory_code_len);
    offset += factory_code_len;

    buffer[offset++] = bound ? (1u << 6) : 0;
    buffer[offset++] = ability & 0xff;
    buffer[offset++] = (ability >> 8) & 0xff;
    buffer[offset++] = (ability >> 16) & 0xff;
    buffer[offset++] = (ability >> 24) & 0xff;

    if (profile->include_protocol_version) {
        buffer[offset++] = profile->protocol_version;
    }

    memcpy(buffer + offset, profile->product_type, product_type_len);
    offset += product_type_len;
    memcpy(buffer + offset, profile->self_mark, self_mark_len);
    offset += self_mark_len;

    printf("[RDX][SESSION] advertising bound=%u ability=%08X\n",
           bound, (unsigned int)ability);

    return offset;
}

#endif
