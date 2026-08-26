#ifndef RDX_BLE_NAME_SERVICE_H
#define RDX_BLE_NAME_SERVICE_H

#include <stdint.h>

int rdx_ble_name_init(void);
const char *rdx_ble_name_get(void);
int rdx_ble_name_set(const uint8_t *name, uint16_t len, uint8_t *changed);

#endif
