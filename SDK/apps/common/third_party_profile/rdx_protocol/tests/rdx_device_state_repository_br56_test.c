#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rdx_device_state_repository.h"

#define FAKE_VM_CAPACITY 64

static uint8_t fake_vm[FAKE_VM_CAPACITY];
static int fake_vm_len;
static int fake_write_result;
static uint8_t fake_corrupt_read_back;

int syscfg_read(unsigned int id, void *buffer, unsigned int len)
{
    unsigned int copy_len;

    assert(id == 15);
    if (fake_vm_len < 0) {
        return fake_vm_len;
    }
    copy_len = (unsigned int)fake_vm_len < len
               ? (unsigned int)fake_vm_len : len;
    memcpy(buffer, fake_vm, copy_len);
    if (fake_corrupt_read_back && copy_len) {
        ((uint8_t *)buffer)[copy_len - 1] ^= 0x5a;
        fake_corrupt_read_back = 0;
    }
    return fake_vm_len;
}

int syscfg_write(unsigned int id, const void *buffer, unsigned int len)
{
    assert(id == 15);
    assert(len <= FAKE_VM_CAPACITY);
    if (fake_write_result >= 0) {
        if ((unsigned int)fake_write_result == len) {
            memcpy(fake_vm, buffer, len);
            fake_vm_len = (int)len;
        }
        return fake_write_result;
    }
    memcpy(fake_vm, buffer, len);
    fake_vm_len = (int)len;
    return (int)len;
}

static void fake_vm_reset(void)
{
    memset(fake_vm, 0, sizeof(fake_vm));
    fake_vm_len = -1;
    fake_write_result = -1;
    fake_corrupt_read_back = 0;
}

static void test_empty_and_v1_load_recover(void)
{
    static const uint8_t stored_v1[] = {1, 1, 0, 0};
    struct rdx_device_state_data state;

    fake_vm_reset();
    assert(rdx_device_state_repository_load(&state)
           == RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED);
    assert(state.bound == 0);
    assert(state.ble_name_len == 0);

    memcpy(fake_vm, stored_v1, sizeof(stored_v1));
    fake_vm_len = sizeof(stored_v1);
    assert(rdx_device_state_repository_load(&state)
           == RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED);
    assert(state.bound == 1);
    assert(state.ble_name_len == 0);
}

static void test_save_verify_and_current_load(void)
{
    struct rdx_device_state_data state;
    struct rdx_device_state_data loaded;

    fake_vm_reset();
    memset(&state, 0, sizeof(state));
    state.bound = 1;
    state.ble_name_len = 4;
    memcpy(state.ble_name, "IKKO", 5);
    state.mic_gain_override_valid = 1;
    state.mic_gain = 9;

    assert(rdx_device_state_repository_save(&state) == 0);
    assert(fake_vm_len == 32);
    assert(rdx_device_state_repository_load(&loaded)
           == RDX_DEVICE_STATE_REPOSITORY_LOAD_CURRENT);
    assert(!memcmp(&loaded, &state, sizeof(state)));

    fake_write_result = 0;
    state.bound = 0;
    assert(rdx_device_state_repository_save(&state) == -1);

    fake_write_result = -1;
    fake_corrupt_read_back = 1;
    assert(rdx_device_state_repository_save(&state) == -1);
}

static void test_invalid_storage_is_sanitized(void)
{
    struct rdx_device_state_data state;

    fake_vm_reset();
    memset(fake_vm, 0, 32);
    fake_vm[0] = 3;
    fake_vm[1] = 1;
    fake_vm[2] = 4;
    memcpy(&fake_vm[4], "bad#", 4);
    fake_vm_len = 32;

    assert(rdx_device_state_repository_load(&state)
           == RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED);
    assert(state.bound == 1);
    assert(state.ble_name_len == 0);
}

int main(void)
{
    test_empty_and_v1_load_recover();
    test_save_verify_and_current_load();
    test_invalid_storage_is_sanitized();
    puts("RDX BR56 device state repository tests passed");
    return 0;
}
