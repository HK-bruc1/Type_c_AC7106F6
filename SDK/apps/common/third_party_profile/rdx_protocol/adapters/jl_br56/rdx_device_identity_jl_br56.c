#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "btstack/le/ble_api.h"
#include "rdx/ports/rdx_device_identity_port.h"

int rdx_device_identity_read_ble_mac(
    uint8_t ble_mac[RDX_DEVICE_IDENTITY_BLE_MAC_SIZE])
{
    if (!ble_mac) {
        return -1;
    }
    return le_controller_get_mac(ble_mac);
}

#endif
