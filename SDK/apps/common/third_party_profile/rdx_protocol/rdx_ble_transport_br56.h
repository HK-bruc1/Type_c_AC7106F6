#ifndef RDX_BLE_TRANSPORT_BR56_H
#define RDX_BLE_TRANSPORT_BR56_H

#include "system/includes.h"

int rdx_ble_transport_init(void);
void rdx_ble_transport_exit(void);
int rdx_ble_transport_send(const u8 *data, u16 len);
int rdx_ble_transport_set_record_streaming(u8 enabled);
void rdx_ble_transport_disconnect(void);
u8 rdx_ble_transport_is_connected(void);

#endif
