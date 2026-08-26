#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rdx/rdx_device_state_service.h"
#include "rdx_device_state_repository_fake.h"

static void test_first_start_and_default_repair(void)
{
    rdx_device_state_t snapshot;
    const struct rdx_device_state_repository_fake_state *fake;

    rdx_device_state_repository_fake_reset();
    assert(rdx_device_state_init() == 0);
    rdx_device_state_get_snapshot(&snapshot);
    assert(snapshot.bound == RDX_BOUND_STATE_UNBOUND);
    assert(snapshot.ble_name_len == 0);
    assert(snapshot.mic_gain_override_valid == 0);

    fake = rdx_device_state_repository_fake_get_state();
    assert(fake->save_attempt_count == 0);
    assert(rdx_device_state_restore_defaults() == 0);
    assert(fake->save_attempt_count == 1);
    assert(fake->save_success_count == 1);
    assert(rdx_device_state_restore_defaults() == 0);
    assert(fake->save_attempt_count == 1);
    rdx_device_state_exit();
}

static void test_transactional_updates_and_write_failure(void)
{
    static const uint8_t new_name[] = "RDX-TypeC";
    struct rdx_device_state_data stored;
    rdx_device_state_t snapshot;
    const char *name;
    uint8_t gain;
    const struct rdx_device_state_repository_fake_state *fake;

    memset(&stored, 0, sizeof(stored));
    stored.bound = RDX_BOUND_STATE_BOUND;
    stored.ble_name_len = 4;
    memcpy(stored.ble_name, "IKKO", 5);
    stored.mic_gain_override_valid = 1;
    stored.mic_gain = 8;

    rdx_device_state_repository_fake_reset();
    rdx_device_state_repository_fake_set_load(
        RDX_DEVICE_STATE_REPOSITORY_LOAD_CURRENT, &stored);
    assert(rdx_device_state_init() == 0);
    assert(rdx_device_state_get_bound() == RDX_BOUND_STATE_BOUND);
    assert(rdx_device_state_get_ble_name_override(&name) == 4);
    assert(!strcmp(name, "IKKO"));
    assert(rdx_device_state_get_mic_gain_override(&gain) == 1);
    assert(gain == 8);

    rdx_device_state_repository_fake_set_save_result(-1);
    assert(rdx_device_state_set_bound(RDX_BOUND_STATE_UNBOUND) == -2);
    assert(rdx_device_state_get_bound() == RDX_BOUND_STATE_BOUND);
    assert(rdx_device_state_set_ble_name_override(
               new_name, sizeof(new_name) - 1) == -2);
    assert(rdx_device_state_get_ble_name_override(&name) == 4);
    assert(!strcmp(name, "IKKO"));

    fake = rdx_device_state_repository_fake_get_state();
    assert(fake->save_attempt_count == 2);
    rdx_device_state_repository_fake_set_save_result(0);
    assert(rdx_device_state_set_bound(RDX_BOUND_STATE_UNBOUND) == 0);
    assert(rdx_device_state_set_ble_name_override(
               new_name, sizeof(new_name) - 1) == 0);
    assert(rdx_device_state_set_mic_gain_override(19) == 0);
    rdx_device_state_get_snapshot(&snapshot);
    assert(snapshot.bound == RDX_BOUND_STATE_UNBOUND);
    assert(snapshot.ble_name_len == sizeof(new_name) - 1);
    assert(!strcmp(snapshot.ble_name, (const char *)new_name));
    assert(snapshot.mic_gain_override_valid == 1);
    assert(snapshot.mic_gain == 19);
    assert(fake->save_success_count == 3);

    assert(rdx_device_state_set_mic_gain_override(20) == -1);
    assert(rdx_device_state_set_ble_name_override(
               (const uint8_t *)"bad#name", 8) == -1);
    assert(fake->save_attempt_count == 5);
    rdx_device_state_exit();
}

static void test_repository_contract_failure_is_fail_closed(void)
{
    struct rdx_device_state_data invalid_state;

    rdx_device_state_repository_fake_reset();
    rdx_device_state_repository_fake_set_load(-1, NULL);
    assert(rdx_device_state_init() == -1);
    assert(rdx_device_state_set_bound(RDX_BOUND_STATE_BOUND) == -1);

    memset(&invalid_state, 0, sizeof(invalid_state));
    invalid_state.ble_name[1] = 'x';
    rdx_device_state_repository_fake_set_load(
        RDX_DEVICE_STATE_REPOSITORY_LOAD_CURRENT, &invalid_state);
    assert(rdx_device_state_init() == -1);
    rdx_device_state_exit();
}

int main(void)
{
    test_first_start_and_default_repair();
    test_transactional_updates_and_write_failure();
    test_repository_contract_failure_is_fail_closed();
    puts("RDX device state service tests passed");
    return 0;
}
