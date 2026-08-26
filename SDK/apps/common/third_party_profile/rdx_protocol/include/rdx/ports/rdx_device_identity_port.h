#ifndef RDX_DEVICE_IDENTITY_PORT_H
#define RDX_DEVICE_IDENTITY_PORT_H

#include <stdint.h>

#define RDX_DEVICE_IDENTITY_BLE_MAC_SIZE       6

int rdx_device_identity_read_ble_mac(
    uint8_t ble_mac[RDX_DEVICE_IDENTITY_BLE_MAC_SIZE]);

#endif
