#include <string.h>

#include "rdx/rdx_local_storage_service.h"
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

enum rdx_storage_result rdx_record_coordinator_prepare_recording(
    struct rdx_record_coordinator *coordinator,
    struct rdx_record_request *request)
{
    struct rdx_storage_artifact_spec spec;
    enum rdx_storage_result result;

    if (!coordinator || !request
        || request->type != RDX_RECORD_REQUEST_START) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    result = rdx_local_storage_service_reserve_artifact(
        coordinator->local_service, request->format_id, &spec);
    if (result != RDX_STORAGE_RESULT_READY) {
        return result;
    }
    return rdx_record_coordinator_prepare_start(coordinator, &spec, request);
}

enum rdx_record_result rdx_record_coordinator_prepare_engine_start(
    void *priv, struct rdx_record_request *request)
{
    struct rdx_record_coordinator *coordinator = priv;
    enum rdx_storage_result result;

    result = rdx_record_coordinator_prepare_recording(coordinator, request);
    if (result == RDX_STORAGE_RESULT_READY) {
        return RDX_RECORD_RESULT_OK;
    }
    if (result == RDX_STORAGE_RESULT_BUSY) {
        return RDX_RECORD_RESULT_BUSY;
    }
    return RDX_RECORD_RESULT_STORAGE_ERROR;
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

int rdx_record_coordinator_get_record_identity(
    const struct rdx_record_coordinator *coordinator,
    uint32_t *serial_number, char *filename, uint32_t filename_capacity)
{
    const struct rdx_artifact *artifact;
    const char *basename;
    const char *slash;
    size_t len;

    if (!coordinator || !coordinator->local_service || !serial_number
        || !filename || filename_capacity < 2) {
        return -1;
    }
    artifact = rdx_local_storage_service_artifact(
        coordinator->local_service);
    if (!artifact || !artifact->spec.serial_number
        || !artifact->spec.final_name[0]
        || artifact->state == RDX_ARTIFACT_STATE_NONE
        || artifact->state == RDX_ARTIFACT_STATE_FAILED
        || artifact->state == RDX_ARTIFACT_STATE_QUARANTINED) {
        return -1;
    }
    slash = strrchr(artifact->spec.final_name, '/');
    basename = slash ? slash + 1 : artifact->spec.final_name;
    len = strlen(basename);
    if (!len || len >= filename_capacity) {
        return -1;
    }
    memcpy(filename, basename, len + 1);
    *serial_number = artifact->spec.serial_number;
    return 0;
}
