#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rdx/rdx_ble_name_service.h"
#include "rdx/rdx_identity_service.h"
#include "rdx/rdx_wire_defs.h"

static const char *const fake_auth_key = "ABCDEFGHIJKLMNOPQRST7890";
static char fake_override[RDX_DEVICE_STATE_BLE_NAME_MAX_LEN + 1];
static uint8_t fake_override_len;

const char *rdx_identity_get_auth_key(void)
{
    return fake_auth_key;
}

uint8_t rdx_device_state_get_ble_name_override(const char **name)
{
    if (name) {
        *name = fake_override_len ? fake_override : NULL;
    }
    return fake_override_len;
}

int rdx_device_state_set_ble_name_override(const uint8_t *name, uint16_t len)
{
    if (len > RDX_DEVICE_STATE_BLE_NAME_MAX_LEN
        || (len && !name)) {
        return -1;
    }
    if (!len) {
        memset(fake_override, 0, sizeof(fake_override));
        fake_override_len = 0;
        return 0;
    }
    memcpy(fake_override, name, len);
    fake_override[len] = '\0';
    fake_override_len = len;
    return 0;
}

static void test_default_name_and_override_priority(void)
{
    static const uint8_t custom_name[] = "CUSTOM";
    static const uint8_t default_name[] = "IKKO 7890";
    uint8_t changed;

    memset(fake_override, 0, sizeof(fake_override));
    fake_override_len = 0;
    assert(rdx_ble_name_init() == 0);
    assert(!strcmp(rdx_ble_name_get(), (const char *)default_name));

    assert(rdx_ble_name_set(custom_name, sizeof(custom_name) - 1,
                            &changed) == 0);
    assert(changed);
    assert(!strcmp(rdx_ble_name_get(), "CUSTOM"));

    assert(rdx_ble_name_set(custom_name, sizeof(custom_name) - 1,
                            &changed) == 0);
    assert(!changed);

    assert(rdx_ble_name_set(default_name, sizeof(default_name) - 1,
                            &changed) == 0);
    assert(changed);
    assert(!strcmp(rdx_ble_name_get(), "IKKO 7890"));
    assert(fake_override_len == 0);
}

static void test_invalid_names_are_rejected(void)
{
    static const uint8_t invalid_name[] = "BAD#NAME";
    uint8_t changed;

    assert(rdx_ble_name_set(invalid_name, sizeof(invalid_name) - 1,
                            &changed) != 0);

    memcpy(fake_override, invalid_name, sizeof(invalid_name));
    fake_override_len = sizeof(invalid_name) - 1;
    assert(rdx_ble_name_init() == -2);
    memset(fake_override, 0, sizeof(fake_override));
    fake_override_len = 0;
    assert(rdx_ble_name_init() == 0);
}

int main(void)
{
    test_default_name_and_override_priority();
    test_invalid_names_are_rejected();
    puts("RDX BLE name service tests passed");
    return 0;
}
