#include <string.h>

#include "rdx/rdx_record_coordinator.h"

int rdx_record_coordinator_init(
    struct rdx_record_coordinator *coordinator,
    struct rdx_local_storage_service *local_service,
    const struct rdx_record_destination_ops *online_ops,
    void *online_priv)
{
    if (!coordinator || !local_service) {
        return -1;
    }
    memset(coordinator, 0, sizeof(*coordinator));
    coordinator->local_service = local_service;
    coordinator->online_ops = online_ops;
    coordinator->online_priv = online_priv;
    return 0;
}

enum rdx_storage_result rdx_record_coordinator_prepare_start(
    struct rdx_record_coordinator *coordinator,
    const struct rdx_storage_artifact_spec *spec,
    struct rdx_record_request *request)
{
    enum rdx_storage_result result;

    if (!coordinator || !coordinator->local_service || !spec || !request
        || request->type != RDX_RECORD_REQUEST_START) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (coordinator->prepared && coordinator->local_service->active) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    coordinator->prepared = 0;

    /* The artifact is opened before Engine START is queued.  This makes the
     * Local REQUIRED reservation deterministic; a queue-full submit is then
     * rolled back instead of leaving a temp file owned by nobody. */
    result = rdx_local_storage_service_start(coordinator->local_service,
                                             spec);
    if (result != RDX_STORAGE_RESULT_READY) {
        return result;
    }
    if (rdx_record_local_destination_init(
            &coordinator->local_destination, coordinator->local_service)
        || rdx_record_composite_destination_init(
               &coordinator->composite_destination,
               rdx_record_local_destination_ops(),
               &coordinator->local_destination,
               coordinator->online_ops, coordinator->online_priv)) {
        rdx_local_storage_service_cancel_start(
            coordinator->local_service, RDX_RECORD_CAUSE_START_FAILED);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }

    request->destination_ops = rdx_record_composite_destination_ops();
    request->destination_priv = &coordinator->composite_destination;
    coordinator->prepared = 1;
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_record_coordinator_cancel_start(
    struct rdx_record_coordinator *coordinator,
    enum rdx_record_cause cause)
{
    enum rdx_storage_result result;

    if (!coordinator || !coordinator->local_service
        || !coordinator->prepared) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    result = rdx_local_storage_service_cancel_start(
        coordinator->local_service, cause);
    if (result == RDX_STORAGE_RESULT_READY
        || !coordinator->local_service->active) {
        coordinator->prepared = 0;
    }
    return result;
}
