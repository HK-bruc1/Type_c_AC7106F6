#ifndef RDX_DEVICE_STATE_DATA_H
#define RDX_DEVICE_STATE_DATA_H

#include <stdint.h>

#include "rdx_protocol_defs.h"

struct rdx_device_state_data {
    uint8_t bound;
    uint8_t ble_name_len;
    char ble_name[RDX_BLE_NAME_MAX_LEN + 1];
    uint8_t mic_gain_override_valid;
    uint8_t mic_gain;
};

#endif
