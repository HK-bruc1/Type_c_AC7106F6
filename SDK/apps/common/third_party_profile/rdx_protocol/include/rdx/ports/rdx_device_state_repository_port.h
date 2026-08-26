#ifndef RDX_DEVICE_STATE_REPOSITORY_PORT_H
#define RDX_DEVICE_STATE_REPOSITORY_PORT_H

#include "rdx/domain/rdx_device_state_model.h"

enum rdx_device_state_repository_load_result {
    RDX_DEVICE_STATE_REPOSITORY_LOAD_CURRENT = 0,
    RDX_DEVICE_STATE_REPOSITORY_LOAD_RECOVERED = 1,
};

/*
 * load() always returns canonical domain data on success. RECOVERED means
 * the backend was empty, obsolete or invalid and should be repaired by a
 * later explicit save. Backend IDs and persisted layouts stay private to
 * the selected adapter.
 */
int rdx_device_state_repository_load(struct rdx_device_state_data *out);
int rdx_device_state_repository_save(
    const struct rdx_device_state_data *state);

#endif
