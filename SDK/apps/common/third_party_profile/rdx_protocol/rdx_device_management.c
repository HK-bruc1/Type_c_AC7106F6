#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_device_management.h"
#include "rdx_device_state.h"

int rdx_device_management_bind(void)
{
    return rdx_device_state_set_bound(RDX_BOUND_STATE_BOUND);
}

int rdx_device_management_unbind(void)
{
    return rdx_device_state_set_bound(RDX_BOUND_STATE_UNBOUND);
}

int rdx_device_management_restore_defaults(void)
{
    return rdx_device_state_restore_defaults();
}

u8 rdx_device_management_get_bound(void)
{
    return rdx_device_state_get_bound();
}

#endif
