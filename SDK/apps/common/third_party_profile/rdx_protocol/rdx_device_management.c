#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_device_management.h"
#include "rdx_device_state.h"

int rdx_device_management_bind(void)
{
    return rdx_device_state_set_bound(1);
}

u8 rdx_device_management_get_bound(void)
{
    return rdx_device_state_get_bound();
}

#endif
