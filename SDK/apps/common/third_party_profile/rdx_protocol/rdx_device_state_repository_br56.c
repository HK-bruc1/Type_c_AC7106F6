#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "user_cfg_id.h"
#include "rdx_device_state_repository.h"

#define RDX_DEVICE_STATE_STORAGE_VERSION       3
#define RDX_BOUND_STATE_STORAGE_MAX            1

struct rdx_device_state_storage_v1 {
    uint8_t version;
    uint8_t bound;
    uint8_t reserved[2];
};

struct rdx_device_state_storage_v2 {
    uint8_t version;
    uint8_t bound;
    uint8_t ble_name_len;
    uint8_t reserved0;
    char ble_name[RDX_BLE_NAME_MAX_LEN + 1];
    uint8_t reserved1[3];
};

struct rdx_device_state_storage_v3 {
    uint8_t version;
    uint8_t bound;
    uint8_t ble_name_len;
    uint8_t reserved0;
    char ble_name[RDX_BLE_NAME_MAX_LEN + 1];
    uint8_t mic_gain_override_valid;
    uint8_t mic_gain;
    uint8_t reserved1;
};

typedef char rdx_device_state_storage_v1_size_check[
    (sizeof(struct rdx_device_state_storage_v1) == 4) ? 1 : -1];
typedef char rdx_device_state_storage_v2_size_check[
    (sizeof(struct rdx_device_state_storage_v2) == 32) ? 1 : -1];
typedef char rdx_device_state_storage_v3_size_check[
    (sizeof(struct rdx_device_state_storage_v3) == 32) ? 1 : -1];

static void rdx_device_state_repository_defaults(
    struct rdx_device_state_data *state)
{
    memset(state, 0, sizeof(*state));
}

static uint8_t rdx_device_state_repository_name_valid(
    const char *name, uint8_t len)
{
    uint8_t has_non_space = 0;
    uint8_t i;

    if (!len) {
        for (i = 0; i < RDX_BLE_NAME_MAX_LEN + 1; i++) {
            if (name[i]) {
                return 0;
            }
        }
        return 1;
    }
    if (len > RDX_BLE_NAME_MAX_LEN || name[len] != '\0') {
        return 0;
    }
    for (i = 0; i < len; i++) {
        if ((uint8_t)name[i] < 0x20 || (uint8_t)name[i] > 0x7e
            || name[i] == '#') {
            return 0;
        }
        if (name[i] != ' ') {
            has_non_space = 1;
        }
    }
    if (!has_non_space) {
        return 0;
    }
    for (i = len + 1; i < RDX_BLE_NAME_MAX_LEN + 1; i++) {
        if (name[i]) {
            return 0;
        }
    }
    return 1;
}

static uint8_t rdx_device_state_repository_data_valid(
    const struct rdx_device_state_data *state)
{
    return state
           && state->bound <= RDX_BOUND_STATE_STORAGE_MAX
           && rdx_device_state_repository_name_valid(
                  state->ble_name, state->ble_name_len)
           && state->mic_gain_override_valid <= 1
           && (!state->mic_gain_override_valid
               || state->mic_gain <= RDX_MIC_GAIN_LEVEL_MAX);
}

static void rdx_device_state_repository_encode(
    const struct rdx_device_state_data *state,
    struct rdx_device_state_storage_v3 *stored)
{
    memset(stored, 0, sizeof(*stored));
    stored->version = RDX_DEVICE_STATE_STORAGE_VERSION;
    stored->bound = state->bound;
    stored->ble_name_len = state->ble_name_len;
    memcpy(stored->ble_name, state->ble_name, sizeof(stored->ble_name));
    stored->mic_gain_override_valid = state->mic_gain_override_valid;
    stored->mic_gain = state->mic_gain_override_valid ? state->mic_gain : 0;
}

int rdx_device_state_repository_load(struct rdx_device_state_data *out)
{
    struct rdx_device_state_storage_v3 stored;
    struct rdx_device_state_storage_v2 stored_v2;
    struct rdx_device_state_storage_v1 stored_v1;
    struct rdx_device_state_storage_v3 canonical;
    int ret;

    if (!out) {
        return -1;
    }

    rdx_device_state_repository_defaults(out);
    memset(&stored, 0, sizeof(stored));
    ret = syscfg_read(CFG_RDX_DEVICE_STATE, &stored, sizeof(stored));
    memcpy(&stored_v1, &stored, sizeof(stored_v1));
    if (ret == sizeof(stored_v1)
        && stored_v1.version == 1
        && stored_v1.bound <= RDX_BOUND_STATE_STORAGE_MAX) {
        out->bound = stored_v1.bound;
        printf("[RDX][STATE_REPO] load=v1 vm_id=%u bound=%u migration=deferred\n",
               CFG_RDX_DEVICE_STATE, out->bound);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }
    if (ret != sizeof(stored)) {
        printf("[RDX][STATE_REPO] load=default reason=read_failed vm_id=%u vm_ret=%d\n",
               CFG_RDX_DEVICE_STATE, ret);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }

    if (stored.version == 2) {
        memcpy(&stored_v2, &stored, sizeof(stored_v2));
        if (stored_v2.bound > RDX_BOUND_STATE_STORAGE_MAX) {
            printf("[RDX][STATE_REPO] load=v2 reason=bound_invalid vm_id=%u stored=%u repair=deferred\n",
                   CFG_RDX_DEVICE_STATE, stored_v2.bound);
            return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
        }
        out->bound = stored_v2.bound;
        if (!rdx_device_state_repository_name_valid(
                stored_v2.ble_name, stored_v2.ble_name_len)) {
            printf("[RDX][STATE_REPO] load=v2 reason=name_sanitized vm_id=%u bound=%u stored_name_len=%u repair=deferred\n",
                   CFG_RDX_DEVICE_STATE, out->bound,
                   stored_v2.ble_name_len);
            return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
        }
        out->ble_name_len = stored_v2.ble_name_len;
        memcpy(out->ble_name, stored_v2.ble_name,
               sizeof(out->ble_name));
        printf("[RDX][STATE_REPO] load=v2 vm_id=%u bound=%u name_len=%u migration=deferred\n",
               CFG_RDX_DEVICE_STATE, out->bound, out->ble_name_len);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }

    if (stored.version != RDX_DEVICE_STATE_STORAGE_VERSION) {
        printf("[RDX][STATE_REPO] load=default reason=version vm_id=%u stored=%u expected=%u\n",
               CFG_RDX_DEVICE_STATE, stored.version,
               RDX_DEVICE_STATE_STORAGE_VERSION);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }
    if (stored.bound > RDX_BOUND_STATE_STORAGE_MAX) {
        printf("[RDX][STATE_REPO] load=default reason=bound vm_id=%u stored=%u\n",
               CFG_RDX_DEVICE_STATE, stored.bound);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }

    out->bound = stored.bound;
    if (!rdx_device_state_repository_name_valid(
            stored.ble_name, stored.ble_name_len)) {
        printf("[RDX][STATE_REPO] load=v3 reason=name_sanitized vm_id=%u bound=%u stored_name_len=%u repair=deferred\n",
               CFG_RDX_DEVICE_STATE, out->bound, stored.ble_name_len);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }
    out->ble_name_len = stored.ble_name_len;
    memcpy(out->ble_name, stored.ble_name, sizeof(out->ble_name));
    if (stored.mic_gain_override_valid > 1
        || (stored.mic_gain_override_valid
            && stored.mic_gain > RDX_MIC_GAIN_LEVEL_MAX)) {
        printf("[RDX][STATE_REPO] load=v3 reason=gain_sanitized vm_id=%u bound=%u name_len=%u stored_valid=%u stored_gain=%u repair=deferred\n",
               CFG_RDX_DEVICE_STATE, out->bound, out->ble_name_len,
               stored.mic_gain_override_valid, stored.mic_gain);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }
    out->mic_gain_override_valid = stored.mic_gain_override_valid;
    out->mic_gain = stored.mic_gain_override_valid ? stored.mic_gain : 0;

    rdx_device_state_repository_encode(out, &canonical);
    if (memcmp(&canonical, &stored, sizeof(stored))) {
        printf("[RDX][STATE_REPO] load=v3 vm_id=%u bound=%u name_len=%u gain_override=%u gain=%u repair=deferred\n",
               CFG_RDX_DEVICE_STATE, out->bound, out->ble_name_len,
               out->mic_gain_override_valid, out->mic_gain);
        return RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
    }

    printf("[RDX][STATE_REPO] load=v3 vm_id=%u bound=%u name_len=%u gain_override=%u gain=%u repair=no\n",
           CFG_RDX_DEVICE_STATE, out->bound, out->ble_name_len,
           out->mic_gain_override_valid, out->mic_gain);
    return RDX_DEVICE_STATE_REPOSITORY_LOAD_CURRENT;
}

int rdx_device_state_repository_save(
    const struct rdx_device_state_data *state)
{
    struct rdx_device_state_storage_v3 stored;
    struct rdx_device_state_storage_v3 read_back;
    int ret;

    if (!rdx_device_state_repository_data_valid(state)) {
        return -1;
    }

    rdx_device_state_repository_encode(state, &stored);
    ret = syscfg_write(CFG_RDX_DEVICE_STATE, &stored, sizeof(stored));
    if (ret != sizeof(stored)) {
        printf("[RDX][STATE_REPO] save=failed reason=write vm_id=%u vm_ret=%d\n",
               CFG_RDX_DEVICE_STATE, ret);
        return -1;
    }

    memset(&read_back, 0, sizeof(read_back));
    ret = syscfg_read(CFG_RDX_DEVICE_STATE, &read_back,
                      sizeof(read_back));
    if (ret != sizeof(read_back)
        || memcmp(&read_back, &stored, sizeof(stored))) {
        printf("[RDX][STATE_REPO] save=failed reason=verify vm_id=%u vm_ret=%d\n",
               CFG_RDX_DEVICE_STATE, ret);
        return -1;
    }

    printf("[RDX][STATE_REPO] save=ok vm_id=%u version=%u bound=%u name_len=%u gain_override=%u gain=%u\n",
           CFG_RDX_DEVICE_STATE, stored.version, stored.bound,
           stored.ble_name_len, stored.mic_gain_override_valid,
           stored.mic_gain);
    return 0;
}

#endif
