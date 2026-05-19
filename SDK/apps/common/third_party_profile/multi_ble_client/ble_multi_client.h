// binary representation
// attribute size in bytes (16), flags(16), handle (16), uuid (16/128), value(...)

#ifndef _BLE_MULTI_CLIENT_H
#define _BLE_MULTI_CLIENT_H


void ble_multi_client_init();
void ble_multi_client_exit(void);
#endif // _BLE_MULTI_CLIENT_H
