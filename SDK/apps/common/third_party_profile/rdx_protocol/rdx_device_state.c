#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "user_cfg_id.h"
#include "rdx_device_state.h"

typedef char rdx_device_state_size_check[
    (sizeof(rdx_device_state_t) == 4) ? 1 : -1];

static rdx_device_state_t rdx_device_state;
static u8 rdx_device_state_initialized;

static void rdx_device_state_fill_defaults(rdx_device_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->version = RDX_DEVICE_STATE_VERSION;
    state->bound = RDX_BOUND_STATE_UNBOUND;
}

int rdx_device_state_init(void)
{
    rdx_device_state_t stored_state;
    int ret;

    rdx_device_state_fill_defaults(&rdx_device_state);
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
    } else if (stored_state.bound > RDX_BOUND_STATE_BOUND) {
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
    rdx_device_state_fill_defaults(&rdx_device_state);
}

void rdx_device_state_get_snapshot(rdx_device_state_t *snapshot)
{
    if (snapshot) {
        *snapshot = rdx_device_state;
    }
}

rdx_bound_state_t rdx_device_state_get_bound(void)
{
    return (rdx_bound_state_t)rdx_device_state.bound;
}

int rdx_device_state_set_bound(rdx_bound_state_t bound)
{
    rdx_device_state_t next_state;
    u8 target_bound;
    u8 old_bound;
    int ret;

    if (!rdx_device_state_initialized
        || (bound != RDX_BOUND_STATE_UNBOUND
            && bound != RDX_BOUND_STATE_BOUND)) {
        printf("[RDX][DROP] cmd=state reason=invalid initialized=%u target_bound=%u\n",
               rdx_device_state_initialized, (unsigned int)bound);
        return -1;
    }

    target_bound = (u8)bound;
    old_bound = rdx_device_state.bound;
    if (old_bound == target_bound) {
        printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u vm=skip\n",
               old_bound, target_bound);
        return 0;
    }

    next_state = rdx_device_state;
    next_state.bound = target_bound;
    ret = syscfg_write(CFG_RDX_DEVICE_STATE, &next_state,
                       sizeof(next_state));
    if (ret != sizeof(next_state)) {
        printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=failed\n",
               old_bound, target_bound, CFG_RDX_DEVICE_STATE, ret);
        return -2;
    }

    rdx_device_state = next_state;
    printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=ok\n",
           old_bound, target_bound, CFG_RDX_DEVICE_STATE, ret);
    return 0;
}

int rdx_device_state_restore_defaults(void)
{
    rdx_device_state_t default_state;
    u8 old_bound;
    int ret;

    if (!rdx_device_state_initialized) {
        printf("[RDX][DROP] cmd=state_restore reason=not_initialized\n");
        return -1;
    }

    rdx_device_state_fill_defaults(&default_state);
    old_bound = rdx_device_state.bound;
    if (!memcmp(&rdx_device_state, &default_state, sizeof(default_state))) {
        printf("[RDX][SESSION] state_restore old_bound=%u target_bound=%u vm=skip\n",
               old_bound, default_state.bound);
        return 0;
    }

    ret = syscfg_write(CFG_RDX_DEVICE_STATE, &default_state,
                       sizeof(default_state));
    if (ret != sizeof(default_state)) {
        printf("[RDX][SESSION] state_restore old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=failed\n",
               old_bound, default_state.bound, CFG_RDX_DEVICE_STATE, ret);
        return -2;
    }

    rdx_device_state = default_state;
    printf("[RDX][SESSION] state_restore old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=ok\n",
           old_bound, default_state.bound, CFG_RDX_DEVICE_STATE, ret);
    return 0;
}

#endif
