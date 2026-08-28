#include <string.h>

#include "rdx/rdx_record_local_destination.h"

static int local_attach(void *priv, uint32_t session_id,
                        uint32_t capture_generation)
{
    struct rdx_record_local_destination *destination = priv;

    return rdx_local_storage_service_attach(destination->service, session_id,
                                             capture_generation)
           == RDX_STORAGE_RESULT_READY ? 0 : -1;
}

static int local_consume(void *priv, const struct rdx_record_frame *frame)
{
    struct rdx_record_local_destination *destination = priv;
    enum rdx_storage_result result;

    result = rdx_local_storage_service_consume(destination->service, frame);
    if (result == RDX_STORAGE_RESULT_READY) {
        return 0;
    }
    return result == RDX_STORAGE_RESULT_QUEUE_FULL
               ? RDX_RECORD_DESTINATION_ERROR_QUEUE_FULL
               : RDX_RECORD_DESTINATION_ERROR_STORAGE;
}

static int local_seal(void *priv, uint32_t session_id,
                      uint32_t capture_generation)
{
    struct rdx_record_local_destination *destination = priv;
    enum rdx_storage_result result;

    result = rdx_local_storage_service_seal(destination->service, session_id,
                                             capture_generation);
    return result == RDX_STORAGE_RESULT_READY
               ? 0 : RDX_RECORD_DESTINATION_ERROR_STORAGE;
}

static int local_abort(void *priv, uint32_t session_id,
                       uint32_t capture_generation,
                       enum rdx_record_cause cause)
{
    struct rdx_record_local_destination *destination = priv;

    /* Coordinator may have prepared the artifact before Engine assigned its
     * runtime tokens.  Engine uses the same REQUIRED abort path for failures
     * that happen before destination attach, so close/quarantine that
     * unbound artifact explicitly instead of returning a token mismatch. */
    if (destination && destination->service
        && destination->service->active
        && !destination->service->capture_generation) {
        return rdx_local_storage_service_cancel_start(destination->service,
                                                       cause)
               == RDX_STORAGE_RESULT_READY ? 0 : -1;
    }
    return rdx_local_storage_service_abort(destination->service, session_id,
                                            capture_generation, cause)
           == RDX_STORAGE_RESULT_READY ? 0 : -1;
}

static void local_detach_capture(void *priv, uint32_t session_id,
                                 uint32_t capture_generation)
{
    struct rdx_record_local_destination *destination = priv;

    rdx_local_storage_service_detach_capture(destination->service, session_id,
                                              capture_generation);
}

static const struct rdx_record_destination_ops local_ops = {
    .attach = local_attach,
    .consume = local_consume,
    .seal = local_seal,
    .abort = local_abort,
    .detach_capture = local_detach_capture,
    .requirement = RDX_RECORD_DESTINATION_REQUIRED,
};

int rdx_record_local_destination_init(
    struct rdx_record_local_destination *destination,
    struct rdx_local_storage_service *service)
{
    if (!destination || !service) {
        return -1;
    }
    memset(destination, 0, sizeof(*destination));
    destination->service = service;
    return 0;
}

const struct rdx_record_destination_ops *
rdx_record_local_destination_ops(void)
{
    return &local_ops;
}
