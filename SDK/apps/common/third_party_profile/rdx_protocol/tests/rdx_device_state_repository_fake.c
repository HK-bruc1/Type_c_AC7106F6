#include <string.h>

#include "rdx_device_state_repository_fake.h"

static struct rdx_device_state_repository_fake_state fake_state;

void rdx_device_state_repository_fake_reset(void)
{
    memset(&fake_state, 0, sizeof(fake_state));
    fake_state.load_result =
        RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED;
}

void rdx_device_state_repository_fake_set_load(
    int result, const struct rdx_device_state_data *state)
{
    fake_state.load_result = result;
    memset(&fake_state.load_data, 0, sizeof(fake_state.load_data));
    if (state) {
        fake_state.load_data = *state;
    }
}

void rdx_device_state_repository_fake_set_save_result(int result)
{
    fake_state.save_result = result;
}

const struct rdx_device_state_repository_fake_state *
rdx_device_state_repository_fake_get_state(void)
{
    return &fake_state;
}

int rdx_device_state_repository_load(struct rdx_device_state_data *out)
{
    fake_state.load_count++;
    if (!out || fake_state.load_result < 0) {
        return -1;
    }
    *out = fake_state.load_data;
    return fake_state.load_result;
}

int rdx_device_state_repository_save(
    const struct rdx_device_state_data *state)
{
    fake_state.save_attempt_count++;
    if (!state) {
        return -1;
    }
    fake_state.last_save_attempt = *state;
    if (fake_state.save_result) {
        return fake_state.save_result;
    }
    fake_state.save_success_count++;
    fake_state.load_data = *state;
    fake_state.load_result = RDX_DEVICE_STATE_REPOSITORY_LOAD_CURRENT;
    return 0;
}
