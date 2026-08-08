#ifndef RDX_DEVICE_STATE_H
#define RDX_DEVICE_STATE_H

#include "system/includes.h"

#define RDX_DEVICE_STATE_VERSION              1

typedef struct {
    u8 version;
    u8 bound;
    u8 reserved[2];
} rdx_device_state_t;

int rdx_device_state_init(void);
void rdx_device_state_exit(void);
void rdx_device_state_get_snapshot(rdx_device_state_t *snapshot);
u8 rdx_device_state_get_bound(void);
int rdx_device_state_set_bound(u8 bound);

#endif
