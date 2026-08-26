#ifndef RDX_DEVICE_STATE_REPOSITORY_FAKE_H
#define RDX_DEVICE_STATE_REPOSITORY_FAKE_H

#include <stdint.h>

#include "rdx_device_state_repository.h"

struct rdx_device_state_repository_fake_state {
    int load_result;
    int save_result;
    uint32_t load_count;
    uint32_t save_attempt_count;
    uint32_t save_success_count;
    struct rdx_device_state_data load_data;
    struct rdx_device_state_data last_save_attempt;
};

void rdx_device_state_repository_fake_reset(void);
void rdx_device_state_repository_fake_set_load(
    int result, const struct rdx_device_state_data *state);
void rdx_device_state_repository_fake_set_save_result(int result);
const struct rdx_device_state_repository_fake_state *
rdx_device_state_repository_fake_get_state(void);

#endif
