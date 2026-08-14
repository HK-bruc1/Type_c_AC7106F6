#ifndef RDX_BLE_NAME_H
#define RDX_BLE_NAME_H

#include "system/includes.h"

int rdx_ble_name_init(void);
const char *rdx_ble_name_get(void);
int rdx_ble_name_set(const u8 *name, u16 len, u8 *changed);

#endif
