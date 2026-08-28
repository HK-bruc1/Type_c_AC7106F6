#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE
#include "rdx/ports/rdx_device_identity_port.h"
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

/*
 * A development image may be programmed with a literal zero marker when the
 * production identity is not available yet.  The burner can store that
 * marker as either "0" or "0\0" (and some older flows use an all-zero text
 * field).  Keep this check deliberately narrow so explicit zero markers can
 * be diagnosed separately; other malformed records use the common fallback.
 */
static uint8_t rdx_identity_is_zero_placeholder(const uint8_t *payload,
                                                uint16_t payload_len)
{
    uint16_t i;

    if (!payload || !payload_len) {
        return 0;
    }

    if (payload_len <= RDX_FACTORY_IDENTITY_RECORD_LEN + 1) {
        uint8_t all_zero_text = 1;

        for (i = 0; i < payload_len; i++) {
            if (payload[i] != '0' && payload[i] != '\0') {
                all_zero_text = 0;
                break;
            }
        }
        if (all_zero_text) {
            return 1;
        }
    }

    /* Also accept the canonical 24,12,16 zero record with its delimiters. */
    if (payload_len == RDX_FACTORY_IDENTITY_RECORD_LEN
        || (payload_len == RDX_FACTORY_IDENTITY_RECORD_LEN + 1
            && payload[RDX_FACTORY_IDENTITY_RECORD_LEN] == '\0')) {
        if (payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN] != ','
            || payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1
                       + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN] != ',') {
            return 0;
        }
        for (i = 0; i < RDX_FACTORY_IDENTITY_AUTH_KEY_LEN; i++) {
            if (payload[i] != '0') {
                return 0;
            }
        }
        for (i = 0; i < RDX_FACTORY_IDENTITY_MAC_TEXT_LEN; i++) {
            if (payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1 + i] != '0') {
                return 0;
            }
        }
        for (i = 0; i < RDX_FACTORY_IDENTITY_LABEL_SN_LEN; i++) {
            if (payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1
                       + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN + 1 + i] != '0') {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

static void rdx_identity_set_fallback_identity(void)
{
    uint8_t default_ble_mac[RDX_DEVICE_IDENTITY_BLE_MAC_SIZE];
    int ret;

    /* Keep the SDK/controller address when only the factory identity is
     * unavailable.  It is already allocated by the BLE stack at this point,
     * and is a safer development fallback than advertising 00:00:00:00:00:00. */
    ret = rdx_device_identity_read_ble_mac(default_ble_mac);
    if (!ret) {
        uint8_t i;

        memcpy(rdx_ble_mac, default_ble_mac, sizeof(rdx_ble_mac));
        for (i = 0; i < RDX_FACTORY_IDENTITY_BLE_MAC_LEN; i++) {
            uint8_t byte = rdx_ble_mac[
                RDX_FACTORY_IDENTITY_BLE_MAC_LEN - 1 - i];
            static const char hex[] = "0123456789ABCDEF";
            rdx_ble_mac_string[i * 2] = hex[byte >> 4];
            rdx_ble_mac_string[i * 2 + 1] = hex[byte & 0x0f];
        }
        rdx_ble_mac_string[RDX_FACTORY_IDENTITY_MAC_TEXT_LEN] = '\0';
    } else {
        memset(rdx_ble_mac, 0, sizeof(rdx_ble_mac));
        memset(rdx_ble_mac_string, '0', sizeof(rdx_ble_mac_string) - 1);
        rdx_ble_mac_string[sizeof(rdx_ble_mac_string) - 1] = '\0';
        printf("[RDX][WARN] default BLE MAC read failed=%d; using zero MAC\n",
               ret);
    }
    memset(rdx_auth_key, '0', sizeof(rdx_auth_key) - 1);
    rdx_auth_key[sizeof(rdx_auth_key) - 1] = '\0';
    memset(rdx_label_sn, '0', sizeof(rdx_label_sn) - 1);
    rdx_label_sn[sizeof(rdx_label_sn) - 1] = '\0';
    rdx_identity_build_read_value();
}

static void rdx_identity_use_fallback_identity(const char *reason)
{
    rdx_identity_set_fallback_identity();
    printf("[RDX][WARN] factory identity %s; using default BLE MAC and zero identity\n",
           reason);
#if defined(CONFIG_DEBUG_ENABLE) && CONFIG_DEBUG_ENABLE
    printf("[RDX] factory AuthKey=%s\n", rdx_auth_key);
    printf("[RDX] factory BLE_MAC=%s\n", rdx_ble_mac_string);
    printf("[RDX] factory Label_SN=%s\n", rdx_label_sn);
#endif
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
    uint16_t identity_len;
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
            rdx_identity_use_fallback_identity("not present");
            return 0;
        }
        printf("[RDX] factory identity invalid stage=block code=%d\n", ret);
        rdx_identity_use_fallback_identity("unavailable");
        return 0;
    }
    if (rdx_identity_is_zero_placeholder(auth_code, auth_code_len)) {
        rdx_identity_use_fallback_identity("zero placeholder");
        return 0;
    }

    /* The legacy BR28 implementation always copied the first 54 bytes from
     * the auth-code area into its temporary buffer, regardless of the block
     * header length.  Some burner/tool versions therefore leave a trailing
     * NUL, line ending, or padded bytes in the block.  Keep the block CRC and
     * bounds checks strict in the Provider, but parse the fixed RDX record
     * prefix just like the reference implementation. */
    identity_len = auth_code_len;
    if (identity_len > RDX_FACTORY_IDENTITY_RECORD_LEN) {
        printf("[RDX][WARN] factory identity payload len=%u; parsing first %u bytes\n",
               identity_len, RDX_FACTORY_IDENTITY_RECORD_LEN);
        identity_len = RDX_FACTORY_IDENTITY_RECORD_LEN;
    }
    ret = rdx_factory_identity_parse(auth_code, identity_len,
                                     &factory_identity);
    if (ret) {
        printf("[RDX] factory identity invalid stage=record code=%d len=%u\n",
               ret, auth_code_len);
        rdx_identity_use_fallback_identity("unavailable");
        return 0;
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
