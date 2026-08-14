#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "user_cfg_id.h"
#include "rdx_device_state.h"

typedef char rdx_device_state_size_check[
    (sizeof(rdx_device_state_t) == 32) ? 1 : -1];

typedef struct {
    u8 version;
    u8 bound;
    u8 reserved[2];
} rdx_device_state_v1_t;

typedef char rdx_device_state_v1_size_check[
    (sizeof(rdx_device_state_v1_t) == 4) ? 1 : -1];

typedef struct {
    u8 version;
    u8 bound;
    u8 ble_name_len;
    u8 reserved0;
    char ble_name[RDX_BLE_NAME_MAX_LEN + 1];
    u8 reserved1[3];
} rdx_device_state_v2_t;

typedef char rdx_device_state_v2_size_check[
    (sizeof(rdx_device_state_v2_t) == 32) ? 1 : -1];

static rdx_device_state_t rdx_device_state;
static u8 rdx_device_state_initialized;
static u8 rdx_device_state_persisted_v3;

static void rdx_device_state_fill_defaults(rdx_device_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->version = RDX_DEVICE_STATE_VERSION;
    state->bound = RDX_BOUND_STATE_UNBOUND;
}

static u8 rdx_device_state_name_bytes_valid(const u8 *name, u16 len)
{
    u8 has_non_space = 0;
    u16 i;

    if (!name || !len || len > RDX_BLE_NAME_MAX_LEN) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (name[i] < 0x20 || name[i] > 0x7e || name[i] == '#') {
            return 0;
        }
        if (name[i] != ' ') {
            has_non_space = 1;
        }
    }
    return has_non_space;
}

static u8 rdx_device_state_name_override_valid(const char *name,
                                                u8 name_len)
{
    u16 i;

    if (!name_len) {
        for (i = 0; i < RDX_BLE_NAME_MAX_LEN + 1; i++) {
            if (name[i]) {
                return 0;
            }
        }
        return 1;
    }
    if (!rdx_device_state_name_bytes_valid(
            (const u8 *)name, name_len)
        || name[name_len] != '\0') {
        return 0;
    }
    for (i = name_len + 1; i < RDX_BLE_NAME_MAX_LEN + 1; i++) {
        if (name[i]) {
            return 0;
        }
    }
    return 1;
}

static u8 rdx_device_state_gain_override_valid(
    const rdx_device_state_t *state)
{
    if (state->mic_gain_override_valid > 1) {
        return 0;
    }
    return !state->mic_gain_override_valid
           || state->mic_gain <= RDX_MIC_GAIN_LEVEL_MAX;
}

int rdx_device_state_init(void)
{
    rdx_device_state_t stored_state;
    rdx_device_state_v1_t stored_v1;
    rdx_device_state_v2_t stored_v2;
    int ret;

    rdx_device_state_fill_defaults(&rdx_device_state);
    rdx_device_state_persisted_v3 = 0;
    memset(&stored_state, 0, sizeof(stored_state));
    ret = syscfg_read(CFG_RDX_DEVICE_STATE, &stored_state,
                      sizeof(stored_state));
    memcpy(&stored_v1, &stored_state, sizeof(stored_v1));
    if (ret == sizeof(stored_v1)
        && stored_v1.version == 1
        && stored_v1.bound <= RDX_BOUND_STATE_BOUND) {
        rdx_device_state.bound = stored_v1.bound;
        printf("[RDX][STATE] state_load=v1 vm_id=%u version=%u bound=%u name_len=0 migration_write=deferred\n",
               CFG_RDX_DEVICE_STATE, stored_v1.version,
               rdx_device_state.bound);
    } else if (ret != sizeof(stored_state)) {
        printf("[RDX][SESSION] state_load=default reason=read_failed vm_id=%u vm_ret=%d bound=%u\n",
               CFG_RDX_DEVICE_STATE, ret, rdx_device_state.bound);
    } else if (stored_state.version == 2) {
        memcpy(&stored_v2, &stored_state, sizeof(stored_v2));
        if (stored_v2.bound > RDX_BOUND_STATE_BOUND) {
            printf("[RDX][STATE] state_load=v2 reason=bound_invalid vm_id=%u stored=%u repair_write=deferred\n",
                   CFG_RDX_DEVICE_STATE, stored_v2.bound);
        } else if (!rdx_device_state_name_override_valid(
                       stored_v2.ble_name, stored_v2.ble_name_len)) {
            rdx_device_state.bound = stored_v2.bound;
            printf("[RDX][STATE] state_load=v2 reason=name_sanitized vm_id=%u bound=%u stored_name_len=%u effective_name=default repair_write=deferred\n",
                   CFG_RDX_DEVICE_STATE, rdx_device_state.bound,
                   stored_v2.ble_name_len);
        } else {
            rdx_device_state.bound = stored_v2.bound;
            rdx_device_state.ble_name_len = stored_v2.ble_name_len;
            memcpy(rdx_device_state.ble_name, stored_v2.ble_name,
                   sizeof(rdx_device_state.ble_name));
            printf("[RDX][STATE] state_load=v2 vm_id=%u bound=%u name_len=%u gain_override=0 migration_write=deferred\n",
                   CFG_RDX_DEVICE_STATE, rdx_device_state.bound,
                   rdx_device_state.ble_name_len);
        }
    } else if (stored_state.version != RDX_DEVICE_STATE_VERSION) {
        printf("[RDX][SESSION] state_load=default reason=version vm_id=%u stored=%u expected=%u bound=%u\n",
               CFG_RDX_DEVICE_STATE, stored_state.version,
               RDX_DEVICE_STATE_VERSION, rdx_device_state.bound);
    } else if (stored_state.bound > RDX_BOUND_STATE_BOUND) {
        printf("[RDX][SESSION] state_load=default reason=bound vm_id=%u stored=%u bound=%u\n",
               CFG_RDX_DEVICE_STATE, stored_state.bound,
               rdx_device_state.bound);
    } else if (!rdx_device_state_name_override_valid(
                   stored_state.ble_name, stored_state.ble_name_len)) {
        rdx_device_state.bound = stored_state.bound;
        printf("[RDX][STATE] state_load=v3 reason=name_sanitized vm_id=%u version=%u bound=%u stored_name_len=%u effective_name=default repair_write=deferred\n",
               CFG_RDX_DEVICE_STATE, stored_state.version,
               rdx_device_state.bound, stored_state.ble_name_len);
    } else if (!rdx_device_state_gain_override_valid(&stored_state)) {
        rdx_device_state.bound = stored_state.bound;
        rdx_device_state.ble_name_len = stored_state.ble_name_len;
        memcpy(rdx_device_state.ble_name, stored_state.ble_name,
               sizeof(rdx_device_state.ble_name));
        printf("[RDX][STATE] state_load=v3 reason=gain_sanitized vm_id=%u bound=%u name_len=%u stored_valid=%u stored_gain=%u effective_gain=factory repair_write=deferred\n",
               CFG_RDX_DEVICE_STATE, rdx_device_state.bound,
               rdx_device_state.ble_name_len,
               stored_state.mic_gain_override_valid,
               stored_state.mic_gain);
    } else {
        rdx_device_state = stored_state;
        if (!rdx_device_state.mic_gain_override_valid) {
            rdx_device_state.mic_gain = 0;
        }
        rdx_device_state_persisted_v3 =
            !memcmp(&rdx_device_state, &stored_state,
                    sizeof(stored_state));
        printf("[RDX][STATE] state_load=v3 vm_id=%u version=%u bound=%u name_len=%u gain_override=%u gain=%u repair_write=%s\n",
               CFG_RDX_DEVICE_STATE, rdx_device_state.version,
               rdx_device_state.bound, rdx_device_state.ble_name_len,
               rdx_device_state.mic_gain_override_valid,
               rdx_device_state.mic_gain,
               rdx_device_state_persisted_v3 ? "no" : "deferred");
    }

    rdx_device_state_initialized = 1;
    return 0;
}

void rdx_device_state_exit(void)
{
    rdx_device_state_initialized = 0;
    rdx_device_state_persisted_v3 = 0;
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
    rdx_device_state_persisted_v3 = 1;
    printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=ok\n",
           old_bound, target_bound, CFG_RDX_DEVICE_STATE, ret);
    return 0;
}

u8 rdx_device_state_get_ble_name_override(const char **name)
{
    if (name) {
        *name = rdx_device_state.ble_name_len
                ? rdx_device_state.ble_name : NULL;
    }
    return rdx_device_state.ble_name_len;
}

int rdx_device_state_set_ble_name_override(const u8 *name, u16 len)
{
    rdx_device_state_t next_state;
    u8 old_len;
    int ret;

    if (!rdx_device_state_initialized
        || (len && !rdx_device_state_name_bytes_valid(name, len))
        || len > RDX_BLE_NAME_MAX_LEN) {
        printf("[RDX][DROP] cmd=state_name reason=invalid initialized=%u target_len=%u\n",
               rdx_device_state_initialized, len);
        return -1;
    }

    next_state = rdx_device_state;
    old_len = next_state.ble_name_len;
    next_state.ble_name_len = 0;
    memset(next_state.ble_name, 0, sizeof(next_state.ble_name));
    if (len) {
        next_state.ble_name_len = len;
        memcpy(next_state.ble_name, name, len);
    }

    if (!memcmp(&rdx_device_state, &next_state, sizeof(next_state))) {
        printf("[RDX][STATE] name_transition old_len=%u target_len=%u vm=skip changed=0\n",
               old_len, (unsigned int)len);
        return 0;
    }

    ret = syscfg_write(CFG_RDX_DEVICE_STATE, &next_state,
                       sizeof(next_state));
    if (ret != sizeof(next_state)) {
        printf("[RDX][STATE] name_transition old_len=%u target_len=%u vm_id=%u vm_ret=%d changed=0 result=failed\n",
               old_len, (unsigned int)len, CFG_RDX_DEVICE_STATE, ret);
        return -2;
    }

    rdx_device_state = next_state;
    rdx_device_state_persisted_v3 = 1;
    printf("[RDX][STATE] name_transition old_len=%u target_len=%u vm_id=%u vm_ret=%d changed=1 result=ok\n",
           old_len, (unsigned int)len, CFG_RDX_DEVICE_STATE, ret);
    return 0;
}

u8 rdx_device_state_get_mic_gain_override(u8 *gain)
{
    if (gain) {
        *gain = rdx_device_state.mic_gain_override_valid
                ? rdx_device_state.mic_gain : 0;
    }
    return rdx_device_state.mic_gain_override_valid;
}

int rdx_device_state_set_mic_gain_override(u8 gain)
{
    rdx_device_state_t next_state;
    u8 old_valid;
    u8 old_gain;
    int ret;

    if (!rdx_device_state_initialized
        || gain > RDX_MIC_GAIN_LEVEL_MAX) {
        printf("[RDX][DROP] cmd=state_mic_gain reason=invalid initialized=%u target_gain=%u\n",
               rdx_device_state_initialized, gain);
        return -1;
    }

    old_valid = rdx_device_state.mic_gain_override_valid;
    old_gain = rdx_device_state.mic_gain;
    next_state = rdx_device_state;
    next_state.mic_gain_override_valid = 1;
    next_state.mic_gain = gain;

    if (!memcmp(&rdx_device_state, &next_state, sizeof(next_state))) {
        printf("[RDX][STATE] mic_gain_transition old_valid=%u old_gain=%u target_gain=%u vm=skip changed=0\n",
               old_valid, old_gain, gain);
        return 0;
    }

    ret = syscfg_write(CFG_RDX_DEVICE_STATE, &next_state,
                       sizeof(next_state));
    if (ret != sizeof(next_state)) {
        printf("[RDX][STATE] mic_gain_transition old_valid=%u old_gain=%u target_gain=%u vm_id=%u vm_ret=%d changed=0 result=failed\n",
               old_valid, old_gain, gain, CFG_RDX_DEVICE_STATE, ret);
        return -2;
    }

    rdx_device_state = next_state;
    rdx_device_state_persisted_v3 = 1;
    printf("[RDX][STATE] mic_gain_transition old_valid=%u old_gain=%u target_gain=%u vm_id=%u vm_ret=%d changed=1 result=ok\n",
           old_valid, old_gain, gain, CFG_RDX_DEVICE_STATE, ret);
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
    if (rdx_device_state_persisted_v3
        && !memcmp(&rdx_device_state, &default_state,
                   sizeof(default_state))) {
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
    rdx_device_state_persisted_v3 = 1;
    printf("[RDX][SESSION] state_restore old_bound=%u target_bound=%u vm_id=%u vm_ret=%d result=ok\n",
           old_bound, default_state.bound, CFG_RDX_DEVICE_STATE, ret);
    return 0;
}

#endif
