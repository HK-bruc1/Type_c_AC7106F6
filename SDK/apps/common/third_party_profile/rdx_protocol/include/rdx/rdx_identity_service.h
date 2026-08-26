#ifndef RDX_IDENTITY_SERVICE_H
#define RDX_IDENTITY_SERVICE_H

#include <stdint.h>

int rdx_identity_init(void);
const uint8_t *rdx_identity_get_ble_mac(void);
const char *rdx_identity_get_auth_key(void);
const char *rdx_identity_get_ble_mac_string(void);
const char *rdx_identity_get_wifi_mac_string(void);
const char *rdx_identity_get_label_sn(void);
uint16_t rdx_identity_get_read_value(const uint8_t **data);
uint8_t rdx_identity_fill_manufacturer_data(uint8_t *buffer,
                                            uint8_t buffer_size);

#endif
