#ifndef RDX_DEVICE_MANAGEMENT_H
#define RDX_DEVICE_MANAGEMENT_H

#include "system/includes.h"

int rdx_device_management_bind(void);
int rdx_device_management_unbind(void);
int rdx_device_management_restore_defaults(void);
u8 rdx_device_management_get_bound(void);

#endif
