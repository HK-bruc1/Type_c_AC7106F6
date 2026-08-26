#ifndef RDX_DEVICE_MANAGEMENT_SERVICE_H
#define RDX_DEVICE_MANAGEMENT_SERVICE_H

#include <stdint.h>

int rdx_device_management_bind(void);
int rdx_device_management_unbind(void);
int rdx_device_management_restore_defaults(void);
uint8_t rdx_device_management_get_bound(void);

#endif
