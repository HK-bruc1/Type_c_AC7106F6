#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "user_cfg_id.h"
#include "rdx_device_state.h"
#include "rdx_mvp0_compat_config.h"

typedef char rdx_device_state_size_check[
    (sizeof(rdx_device_state_t) == 4) ? 1 : -1];
typedef char rdx_default_bound_check[
    (RDX_COMPAT_BOUND_STATE <= 1) ? 1 : -1];

static rdx_device_state_t rdx_device_state;
static u8 rdx_device_state_initialized;

static void rdx_device_state_set_defaults(void)
{
    memset(&rdx_device_state, 0, sizeof(rdx_device_state));
    rdx_device_state.version = RDX_DEVICE_STATE_VERSION;
    rdx_device_state.bound = RDX_COMPAT_BOUND_STATE;
}

int rdx_device_state_init(void)
{
    rdx_device_state_t stored_state;
    int ret;

    rdx_device_state_set_defaults();
    memset(&stored_state, 0, sizeof(stored_state));
    ret = syscfg_read(CFG_RDX_DEVICE_STATE, &stored_state,
                      sizeof(stored_state));
    if (ret != sizeof(stored_state)) {
        printf("[RDX][SESSION] state_load=default reason=read_failed vm_id=%u vm_ret=%d bound=%u\n",
               CFG_RDX_DEVICE_STATE, ret, rdx_device_state.bound);
    } else if (stored_state.version != RDX_DEVICE_STATE_VERSION) {
        printf("[RDX][SESSION] state_load=default reason=version vm_id=%u stored=%u expected=%u bound=%u\n",
               CFG_RDX_DEVICE_STATE, stored_state.version,
               RDX_DEVICE_STATE_VERSION, rdx_device_state.bound);
    } else if (stored_state.bound > 1) {
        printf("[RDX][SESSION] state_load=default reason=bound vm_id=%u stored=%u bound=%u\n",
               CFG_RDX_DEVICE_STATE, stored_state.bound,
               rdx_device_state.bound);
    } else {
        rdx_device_state = stored_state;
        printf("[RDX][SESSION] state_load=vm vm_id=%u version=%u bound=%u\n",
               CFG_RDX_DEVICE_STATE, rdx_device_state.version,
               rdx_device_state.bound);
    }

    rdx_device_state_initialized = 1;
    return 0;
}

void rdx_device_state_exit(void)
{
    rdx_device_state_initialized = 0;
    rdx_device_state_set_defaults();
}

void rdx_device_state_get_snapshot(rdx_device_state_t *snapshot)
{
    if (snapshot) {
        *snapshot = rdx_device_state;
    }
}

u8 rdx_device_state_get_bound(void)
{
    return rdx_device_state.bound;
}

int rdx_device_state_set_bound(u8 bound)
{
    rdx_device_state_t next_state;
    u8 old_bound;
    int ret;

    if (!rdx_device_state_initialized || bound > 1) {
        printf("[RDX][DROP] cmd=state reason=invalid initialized=%u target_bound=%u\n",
               rdx_device_state_initialized, bound);
        return -1;
    }

    old_bound = rdx_device_state.bound;
    if (old_bound == bound) {
        printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u vm=skip\n",
               old_bound, bound);
        return 0;
    }

    memset(&next_state, 0, sizeof(next_state));
    next_state.version = RDX_DEVICE_STATE_VERSION;
    next_state.bound = bound;
    ret = syscfg_write(CFG_RDX_DEVICE_STATE, &next_state,
                       sizeof(next_state));
    if (ret != sizeof(next_state)) {
        printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=failed\n",
               old_bound, bound, CFG_RDX_DEVICE_STATE, ret);
        return -2;
    }

    rdx_device_state = next_state;
    printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=ok\n",
           old_bound, bound, CFG_RDX_DEVICE_STATE, ret);
    return 0;
}

#endif
