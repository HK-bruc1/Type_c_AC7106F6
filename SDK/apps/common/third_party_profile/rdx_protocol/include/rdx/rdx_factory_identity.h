#ifndef RDX_FACTORY_IDENTITY_H
#define RDX_FACTORY_IDENTITY_H

#include <stdint.h>

#define RDX_FACTORY_IDENTITY_AUTH_KEY_LEN  24
#define RDX_FACTORY_IDENTITY_MAC_TEXT_LEN  12
#define RDX_FACTORY_IDENTITY_LABEL_SN_LEN  16
#define RDX_FACTORY_IDENTITY_BLE_MAC_LEN   6

#define RDX_FACTORY_IDENTITY_RECORD_LEN      \
    (RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1 \
     + RDX_FACTORY_IDENTITY_MAC_TEXT_LEN + 1 \
     + RDX_FACTORY_IDENTITY_LABEL_SN_LEN)

/* Payload-format errors are distinct from provider/flash errors. */
#define RDX_FACTORY_RECORD_ERR_LENGTH        (-201)
#define RDX_FACTORY_RECORD_ERR_DELIMITER     (-202)
#define RDX_FACTORY_RECORD_ERR_AUTH_KEY      (-203)
#define RDX_FACTORY_RECORD_ERR_BLE_MAC      (-204)
#define RDX_FACTORY_RECORD_ERR_MAC_VALUE     (-205)
#define RDX_FACTORY_RECORD_ERR_LABEL_SN      (-206)

struct rdx_factory_identity {
    char auth_key[RDX_FACTORY_IDENTITY_AUTH_KEY_LEN + 1];
    uint8_t ble_mac[RDX_FACTORY_IDENTITY_BLE_MAC_LEN];
    char ble_mac_string[RDX_FACTORY_IDENTITY_MAC_TEXT_LEN + 1];
    char label_sn[RDX_FACTORY_IDENTITY_LABEL_SN_LEN + 1];
};

int rdx_factory_identity_parse(const uint8_t *payload,
                               uint16_t payload_len,
                               struct rdx_factory_identity *identity);

#endif
