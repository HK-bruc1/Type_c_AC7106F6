#ifndef RDX_IDENTITY_H
#define RDX_IDENTITY_H

#include "system/includes.h"

int rdx_identity_init(void);
const char *rdx_identity_get_local_name(void);
const u8 *rdx_identity_get_ble_mac(void);
u16 rdx_identity_get_read_value(const u8 **data);
u8 rdx_identity_fill_manufacturer_data(u8 *buffer, u8 buffer_size);
u8 rdx_identity_get_battery_level(void);

#endif
