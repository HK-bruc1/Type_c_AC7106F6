#ifndef RDX_DEVICE_STATE_SERVICE_H
#define RDX_DEVICE_STATE_SERVICE_H

#include <stdint.h>

#include "rdx/domain/rdx_device_state_model.h"

typedef enum {
    RDX_BOUND_STATE_UNBOUND = 0,
    RDX_BOUND_STATE_BOUND = 1,
} rdx_bound_state_t;

typedef struct rdx_device_state_data rdx_device_state_t;

int rdx_device_state_init(void);
void rdx_device_state_exit(void);
void rdx_device_state_get_snapshot(rdx_device_state_t *snapshot);
rdx_bound_state_t rdx_device_state_get_bound(void);
int rdx_device_state_set_bound(rdx_bound_state_t bound);
uint8_t rdx_device_state_get_ble_name_override(const char **name);
int rdx_device_state_set_ble_name_override(const uint8_t *name,
                                           uint16_t len);
uint8_t rdx_device_state_get_mic_gain_override(uint8_t *gain);
int rdx_device_state_set_mic_gain_override(uint8_t gain);
int rdx_device_state_restore_defaults(void);

#endif
