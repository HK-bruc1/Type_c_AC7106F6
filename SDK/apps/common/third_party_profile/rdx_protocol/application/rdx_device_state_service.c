#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE
#include "rdx/rdx_device_state_service.h"
#include "rdx/ports/rdx_device_state_repository_port.h"

static rdx_device_state_t rdx_device_state;
static uint8_t rdx_device_state_initialized;
static uint8_t rdx_device_state_persisted_current;

static void rdx_device_state_fill_defaults(rdx_device_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

static uint8_t rdx_device_state_name_bytes_valid(const uint8_t *name,
                                                  uint16_t len)
{
    uint8_t has_non_space = 0;
    uint16_t i;

    if (!name || !len || len > RDX_DEVICE_STATE_BLE_NAME_MAX_LEN) {
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

static uint8_t rdx_device_state_name_override_valid(
    const char *name, uint8_t len)
{
    uint16_t i;

    if (!len) {
        for (i = 0; i < RDX_DEVICE_STATE_BLE_NAME_MAX_LEN + 1; i++) {
            if (name[i]) {
                return 0;
            }
        }
        return 1;
    }
    if (!rdx_device_state_name_bytes_valid(
            (const uint8_t *)name, len)
        || name[len] != '\0') {
        return 0;
    }
    for (i = len + 1; i < RDX_DEVICE_STATE_BLE_NAME_MAX_LEN + 1; i++) {
        if (name[i]) {
            return 0;
        }
    }
    return 1;
}

static uint8_t rdx_device_state_data_valid(
    const rdx_device_state_t *state)
{
    return state
           && state->bound <= RDX_BOUND_STATE_BOUND
           && rdx_device_state_name_override_valid(
                  state->ble_name, state->ble_name_len)
           && state->mic_gain_override_valid <= 1
           && (!state->mic_gain_override_valid
               || state->mic_gain <= RDX_DEVICE_STATE_MIC_GAIN_MAX);
}

int rdx_device_state_init(void)
{
    rdx_device_state_t loaded_state;
    int ret;

    rdx_device_state_initialized = 0;
    rdx_device_state_fill_defaults(&rdx_device_state);
    rdx_device_state_persisted_current = 0;
    memset(&loaded_state, 0, sizeof(loaded_state));
    ret = rdx_device_state_repository_load(&loaded_state);
    if (ret < 0 || !rdx_device_state_data_valid(&loaded_state)) {
        printf("[RDX][STATE] init=failed reason=repository ret=%d valid=%u\n",
               ret, rdx_device_state_data_valid(&loaded_state));
        return -1;
    }

    rdx_device_state = loaded_state;
    rdx_device_state_persisted_current =
        ret == RDX_DEVICE_STATE_REPOSITORY_LOAD_CURRENT;
    rdx_device_state_initialized = 1;
    printf("[RDX][STATE] init=ok source=%s bound=%u name_len=%u gain_override=%u gain=%u\n",
           rdx_device_state_persisted_current ? "current" : "recovered",
           rdx_device_state.bound, rdx_device_state.ble_name_len,
           rdx_device_state.mic_gain_override_valid,
           rdx_device_state.mic_gain);
    return 0;
}

void rdx_device_state_exit(void)
{
    rdx_device_state_initialized = 0;
    rdx_device_state_persisted_current = 0;
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
    uint8_t target_bound;
    uint8_t old_bound;
    int ret;

    if (!rdx_device_state_initialized
        || (bound != RDX_BOUND_STATE_UNBOUND
            && bound != RDX_BOUND_STATE_BOUND)) {
        printf("[RDX][DROP] cmd=state reason=invalid initialized=%u target_bound=%u\n",
               rdx_device_state_initialized, (unsigned int)bound);
        return -1;
    }

    target_bound = (uint8_t)bound;
    old_bound = rdx_device_state.bound;
    if (old_bound == target_bound) {
        printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u store=skip\n",
               old_bound, target_bound);
        return 0;
    }

    next_state = rdx_device_state;
    next_state.bound = target_bound;
    ret = rdx_device_state_repository_save(&next_state);
    if (ret) {
        printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u store_ret=%d result=failed\n",
               old_bound, target_bound, ret);
        return -2;
    }

    rdx_device_state = next_state;
    rdx_device_state_persisted_current = 1;
    printf("[RDX][SESSION] state_transition old_bound=%u target_bound=%u result=ok\n",
           old_bound, target_bound);
    return 0;
}

uint8_t rdx_device_state_get_ble_name_override(const char **name)
{
    if (name) {
        *name = rdx_device_state.ble_name_len
                ? rdx_device_state.ble_name : NULL;
    }
    return rdx_device_state.ble_name_len;
}

int rdx_device_state_set_ble_name_override(const uint8_t *name,
                                           uint16_t len)
{
    rdx_device_state_t next_state;
    uint8_t old_len;
    int ret;

    if (!rdx_device_state_initialized
        || (len && !rdx_device_state_name_bytes_valid(name, len))
        || len > RDX_DEVICE_STATE_BLE_NAME_MAX_LEN) {
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
        printf("[RDX][STATE] name_transition old_len=%u target_len=%u store=skip changed=0\n",
               old_len, (unsigned int)len);
        return 0;
    }

    ret = rdx_device_state_repository_save(&next_state);
    if (ret) {
        printf("[RDX][STATE] name_transition old_len=%u target_len=%u store_ret=%d changed=0 result=failed\n",
               old_len, (unsigned int)len, ret);
        return -2;
    }

    rdx_device_state = next_state;
    rdx_device_state_persisted_current = 1;
    printf("[RDX][STATE] name_transition old_len=%u target_len=%u changed=1 result=ok\n",
           old_len, (unsigned int)len);
    return 0;
}

uint8_t rdx_device_state_get_mic_gain_override(uint8_t *gain)
{
    if (gain) {
        *gain = rdx_device_state.mic_gain_override_valid
                ? rdx_device_state.mic_gain : 0;
    }
    return rdx_device_state.mic_gain_override_valid;
}

int rdx_device_state_set_mic_gain_override(uint8_t gain)
{
    rdx_device_state_t next_state;
    uint8_t old_valid;
    uint8_t old_gain;
    int ret;

    if (!rdx_device_state_initialized
        || gain > RDX_DEVICE_STATE_MIC_GAIN_MAX) {
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
        printf("[RDX][STATE] mic_gain_transition old_valid=%u old_gain=%u target_gain=%u store=skip changed=0\n",
               old_valid, old_gain, gain);
        return 0;
    }

    ret = rdx_device_state_repository_save(&next_state);
    if (ret) {
        printf("[RDX][STATE] mic_gain_transition old_valid=%u old_gain=%u target_gain=%u store_ret=%d changed=0 result=failed\n",
               old_valid, old_gain, gain, ret);
        return -2;
    }

    rdx_device_state = next_state;
    rdx_device_state_persisted_current = 1;
    printf("[RDX][STATE] mic_gain_transition old_valid=%u old_gain=%u target_gain=%u changed=1 result=ok\n",
           old_valid, old_gain, gain);
    return 0;
}

int rdx_device_state_restore_defaults(void)
{
    rdx_device_state_t default_state;
    uint8_t old_bound;
    int ret;

    if (!rdx_device_state_initialized) {
        printf("[RDX][DROP] cmd=state_restore reason=not_initialized\n");
        return -1;
    }

    rdx_device_state_fill_defaults(&default_state);
    old_bound = rdx_device_state.bound;
    if (rdx_device_state_persisted_current
        && !memcmp(&rdx_device_state, &default_state,
                   sizeof(default_state))) {
        printf("[RDX][SESSION] state_restore old_bound=%u target_bound=%u store=skip\n",
               old_bound, default_state.bound);
        return 0;
    }

    ret = rdx_device_state_repository_save(&default_state);
    if (ret) {
        printf("[RDX][SESSION] state_restore old_bound=%u target_bound=%u store_ret=%d result=failed\n",
               old_bound, default_state.bound, ret);
        return -2;
    }

    rdx_device_state = default_state;
    rdx_device_state_persisted_current = 1;
    printf("[RDX][SESSION] state_restore old_bound=%u target_bound=%u result=ok\n",
           old_bound, default_state.bound);
    return 0;
}

#endif
