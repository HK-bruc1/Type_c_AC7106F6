#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE
#include "rdx/ports/rdx_product_profile_port.h"
#include "rdx/domain/rdx_device_state_model.h"
#include "rdx/rdx_ble_name_service.h"
#include "rdx/rdx_device_state_service.h"
#include "rdx/rdx_identity_service.h"

static uint8_t rdx_ble_name_initialized;
static char rdx_ble_default_name[RDX_DEVICE_STATE_BLE_NAME_MAX_LEN + 1];

static int rdx_ble_name_build_default(void)
{
    const char *auth_key = rdx_identity_get_auth_key();

    if (!auth_key) {
        return -1;
    }
    return rdx_product_profile_build_default_ble_name(
        auth_key, rdx_ble_default_name, sizeof(rdx_ble_default_name));
}

static uint8_t rdx_ble_name_valid(const uint8_t *name, uint16_t len)
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

int rdx_ble_name_init(void)
{
    const char *default_name;
    const char *override_name;
    uint8_t override_len;
    uint16_t default_len;

    rdx_ble_name_initialized = 0;
    memset(rdx_ble_default_name, 0, sizeof(rdx_ble_default_name));
    if (rdx_ble_name_build_default()) {
        printf("[RDX][DROP] cmd=ble_name_init reason=default_build_failed\n");
        return -1;
    }
    default_name = rdx_ble_default_name;
    default_len = strlen(default_name);
    if (!rdx_ble_name_valid((const uint8_t *)default_name, default_len)) {
        printf("[RDX][DROP] cmd=ble_name_init reason=invalid_default len=%u\n",
               default_len);
        return -1;
    }

    override_len = rdx_device_state_get_ble_name_override(&override_name);
    if (override_len
        && !rdx_ble_name_valid((const uint8_t *)override_name, override_len)) {
        printf("[RDX][DROP] cmd=ble_name_init reason=invalid_state len=%u\n",
               override_len);
        return -2;
    }

    rdx_ble_name_initialized = 1;
    printf("[RDX][STATE] ble_name_init source=%s len=%u\n",
           override_len ? "override" : "default",
           override_len ? override_len : default_len);
    return 0;
}

const char *rdx_ble_name_get(void)
{
    const char *override_name;

    if (rdx_device_state_get_ble_name_override(&override_name)) {
        return override_name;
    }
    return rdx_ble_default_name;
}

int rdx_ble_name_set(const uint8_t *name, uint16_t len, uint8_t *changed)
{
    const char *default_name = rdx_ble_default_name;
    char old_name[RDX_DEVICE_STATE_BLE_NAME_MAX_LEN + 1];
    const uint16_t default_len = strlen(default_name);
    uint16_t old_len;
    uint16_t target_len;
    int ret;

    if (changed) {
        *changed = 0;
    }
    if (!rdx_ble_name_initialized || !changed
        || !rdx_ble_name_valid(name, len)) {
        printf("[RDX][DROP] cmd=blename reason=invalid_name initialized=%u len=%u\n",
               rdx_ble_name_initialized, len);
        return -1;
    }

    old_len = strlen(rdx_ble_name_get());
    memcpy(old_name, rdx_ble_name_get(), old_len + 1);

    target_len = len;
    if (len == default_len
        && !memcmp(name, default_name, default_len)) {
        target_len = 0;
    }

    ret = rdx_device_state_set_ble_name_override(name, target_len);
    if (ret) {
        return ret;
    }

    *changed = strcmp(old_name, rdx_ble_name_get()) != 0;
    return 0;
}

#endif
