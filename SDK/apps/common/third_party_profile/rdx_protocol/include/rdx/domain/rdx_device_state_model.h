#ifndef RDX_DEVICE_STATE_MODEL_H
#define RDX_DEVICE_STATE_MODEL_H

#include <stdint.h>

#define RDX_DEVICE_STATE_BLE_NAME_MAX_LEN       24
#define RDX_DEVICE_STATE_MIC_GAIN_MAX           19

struct rdx_device_state_data {
    uint8_t bound;
    uint8_t ble_name_len;
    char ble_name[RDX_DEVICE_STATE_BLE_NAME_MAX_LEN + 1];
    uint8_t mic_gain_override_valid;
    uint8_t mic_gain;
};

#endif /* RDX_DEVICE_STATE_MODEL_H */
