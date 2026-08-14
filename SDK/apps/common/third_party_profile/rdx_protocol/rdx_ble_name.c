#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_ble_name.h"
#include "rdx_device_state.h"
#include "rdx_mvp0_compat_config.h"
#include "rdx_protocol_defs.h"

typedef char rdx_default_ble_name_size_check[
    (sizeof(RDX_COMPAT_LOCAL_NAME) > 1
     && sizeof(RDX_COMPAT_LOCAL_NAME) - 1 <= RDX_BLE_NAME_MAX_LEN) ? 1 : -1];

static u8 rdx_ble_name_initialized;

static u8 rdx_ble_name_valid(const u8 *name, u16 len)
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

int rdx_ble_name_init(void)
{
    const char *override_name;
    u8 override_len;
    const u16 default_len = sizeof(RDX_COMPAT_LOCAL_NAME) - 1;

    rdx_ble_name_initialized = 0;
    if (!rdx_ble_name_valid((const u8 *)RDX_COMPAT_LOCAL_NAME,
                            default_len)) {
        printf("[RDX][DROP] cmd=ble_name_init reason=invalid_default len=%u\n",
               default_len);
        return -1;
    }

    override_len = rdx_device_state_get_ble_name_override(&override_name);
    if (override_len
        && !rdx_ble_name_valid((const u8 *)override_name, override_len)) {
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
    return RDX_COMPAT_LOCAL_NAME;
}

int rdx_ble_name_set(const u8 *name, u16 len, u8 *changed)
{
    char old_name[RDX_BLE_NAME_MAX_LEN + 1];
    const u16 default_len = sizeof(RDX_COMPAT_LOCAL_NAME) - 1;
    u16 old_len;
    u16 target_len;
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
        && !memcmp(name, RDX_COMPAT_LOCAL_NAME, default_len)) {
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
