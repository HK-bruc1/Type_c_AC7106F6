#ifndef RDX_DEVICE_STATE_H
#define RDX_DEVICE_STATE_H

#include "system/includes.h"

#define RDX_DEVICE_STATE_VERSION              1

typedef enum {
    RDX_BOUND_STATE_UNBOUND = 0,
    RDX_BOUND_STATE_BOUND = 1,
} rdx_bound_state_t;

typedef struct {
    u8 version;
    u8 bound;
    u8 reserved[2];
} rdx_device_state_t;

int rdx_device_state_init(void);
void rdx_device_state_exit(void);
void rdx_device_state_get_snapshot(rdx_device_state_t *snapshot);
rdx_bound_state_t rdx_device_state_get_bound(void);
int rdx_device_state_set_bound(rdx_bound_state_t bound);
int rdx_device_state_restore_defaults(void);

#endif
