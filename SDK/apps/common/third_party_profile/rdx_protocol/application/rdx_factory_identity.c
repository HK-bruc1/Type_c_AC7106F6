#include <string.h>

#include "rdx/rdx_factory_identity.h"

static uint8_t rdx_factory_is_alnum(uint8_t value)
{
    return ((value >= 'A' && value <= 'Z')
            || (value >= 'a' && value <= 'z')
            || (value >= '0' && value <= '9'));
}

static int rdx_factory_hex_value(uint8_t value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

int rdx_factory_identity_parse(const uint8_t *payload,
                               uint16_t payload_len,
                               struct rdx_factory_identity *identity)
{
    uint16_t effective_len = payload_len;
    uint8_t i;
    uint8_t all_zero = 1;
    uint8_t all_ff = 1;

    if (!payload || !identity) {
        return RDX_FACTORY_RECORD_ERR_LENGTH;
    }

    /* Some burner versions include a trailing NUL in len. */
    if (effective_len == RDX_FACTORY_IDENTITY_RECORD_LEN + 1
        && payload[RDX_FACTORY_IDENTITY_RECORD_LEN] == '\0') {
        effective_len = RDX_FACTORY_IDENTITY_RECORD_LEN;
    }
    if (effective_len != RDX_FACTORY_IDENTITY_RECORD_LEN) {
        return RDX_FACTORY_RECORD_ERR_LENGTH;
    }
    if (payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN] != ','
        || payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1
                   + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN] != ',') {
        return RDX_FACTORY_RECORD_ERR_DELIMITER;
    }

    for (i = 0; i < RDX_FACTORY_IDENTITY_AUTH_KEY_LEN; i++) {
        if (!rdx_factory_is_alnum(payload[i])) {
            return RDX_FACTORY_RECORD_ERR_AUTH_KEY;
        }
    }
    for (i = 0; i < RDX_FACTORY_IDENTITY_LABEL_SN_LEN; i++) {
        if (!rdx_factory_is_alnum(
                payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1
                        + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN + 1 + i])) {
            return RDX_FACTORY_RECORD_ERR_LABEL_SN;
        }
    }

    memset(identity, 0, sizeof(*identity));
    memcpy(identity->auth_key, payload, RDX_FACTORY_IDENTITY_AUTH_KEY_LEN);
    memcpy(identity->label_sn,
           payload + RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1
                   + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN + 1,
           RDX_FACTORY_IDENTITY_LABEL_SN_LEN);

    /* Text is MSB-first; the controller stores the address LSB-first. */
    for (i = 0; i < RDX_FACTORY_IDENTITY_BLE_MAC_LEN; i++) {
        int high = rdx_factory_hex_value(
            payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1 + i * 2]);
        int low = rdx_factory_hex_value(
            payload[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1 + i * 2 + 1]);
        if (high < 0 || low < 0) {
            return RDX_FACTORY_RECORD_ERR_BLE_MAC;
        }
        identity->ble_mac[RDX_FACTORY_IDENTITY_BLE_MAC_LEN - 1 - i] =
            (uint8_t)((high << 4) | low);
    }

    for (i = 0; i < RDX_FACTORY_IDENTITY_BLE_MAC_LEN; i++) {
        if (identity->ble_mac[i] != 0) {
            all_zero = 0;
        }
        if (identity->ble_mac[i] != 0xff) {
            all_ff = 0;
        }
    }
    if (all_zero || all_ff || (identity->ble_mac[5] & 0x01)) {
        return RDX_FACTORY_RECORD_ERR_MAC_VALUE;
    }

    for (i = 0; i < RDX_FACTORY_IDENTITY_BLE_MAC_LEN; i++) {
        uint8_t byte = identity->ble_mac[
            RDX_FACTORY_IDENTITY_BLE_MAC_LEN - 1 - i];
        static const char hex[] = "0123456789ABCDEF";
        identity->ble_mac_string[i * 2] = hex[byte >> 4];
        identity->ble_mac_string[i * 2 + 1] = hex[byte & 0x0f];
    }
    return 0;
}
