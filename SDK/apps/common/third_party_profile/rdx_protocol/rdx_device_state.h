#ifndef RDX_DEVICE_STATE_H
#define RDX_DEVICE_STATE_H

#include "system/includes.h"
#include "rdx_protocol_defs.h"

#define RDX_DEVICE_STATE_VERSION              3

typedef enum {
    RDX_BOUND_STATE_UNBOUND = 0,
    RDX_BOUND_STATE_BOUND = 1,
} rdx_bound_state_t;

typedef struct {
    u8 version;
    u8 bound;
    u8 ble_name_len;
    u8 reserved0;
    char ble_name[RDX_BLE_NAME_MAX_LEN + 1];
    u8 mic_gain_override_valid;
    u8 mic_gain;
    u8 reserved1;
} rdx_device_state_t;

int rdx_device_state_init(void);
void rdx_device_state_exit(void);
void rdx_device_state_get_snapshot(rdx_device_state_t *snapshot);
rdx_bound_state_t rdx_device_state_get_bound(void);
int rdx_device_state_set_bound(rdx_bound_state_t bound);
u8 rdx_device_state_get_ble_name_override(const char **name);
int rdx_device_state_set_ble_name_override(const u8 *name, u16 len);
u8 rdx_device_state_get_mic_gain_override(u8 *gain);
int rdx_device_state_set_mic_gain_override(u8 gain);
int rdx_device_state_restore_defaults(void);

#endif
