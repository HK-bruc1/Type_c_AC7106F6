#ifndef RDX_RECORD_COORDINATOR_H
#define RDX_RECORD_COORDINATOR_H

#include <stdint.h>

#include "rdx/rdx_record_composite_destination.h"
#include "rdx/rdx_record_local_destination.h"
#include "rdx/rdx_storage_types.h"

struct rdx_local_storage_service;

/*
 * Composition root for a Local REQUIRED + Online OPTIONAL recording.  The
 * coordinator owns no platform handles and does not choose file names or
 * serial-number policy; the product supplies an artifact spec template.
 *
 * Record Engine allocates session_id/capture_generation during START.  A
 * zero spec.session_id is therefore a supported wildcard and is bound by
 * Local Storage at destination attach().
 */
struct rdx_record_coordinator {
    struct rdx_local_storage_service *local_service;
    struct rdx_record_local_destination local_destination;
    struct rdx_record_composite_destination composite_destination;
    const struct rdx_record_destination_ops *online_ops;
    void *online_priv;
    uint8_t prepared;
};

int rdx_record_coordinator_init(
    struct rdx_record_coordinator *coordinator,
    struct rdx_local_storage_service *local_service,
    const struct rdx_record_destination_ops *online_ops,
    void *online_priv);

/* Prepare the Local artifact and fill request->destination_* for Engine. */
enum rdx_storage_result rdx_record_coordinator_prepare_start(
    struct rdx_record_coordinator *coordinator,
    const struct rdx_storage_artifact_spec *spec,
    struct rdx_record_request *request);

/* Application-facing START preparation: Storage reserves the persistent
 * identity and naming policy; Protocol supplies only the Engine request. */
enum rdx_storage_result rdx_record_coordinator_prepare_recording(
    struct rdx_record_coordinator *coordinator,
    struct rdx_record_request *request);

/* Engine-facing adapter for deferred START preparation.  This function is
 * invoked on the Record Engine worker, never from the BLE protocol callback
 * stack, and maps Storage results into the Engine result contract. */
enum rdx_record_result rdx_record_coordinator_prepare_engine_start(
    void *priv, struct rdx_record_request *request);

/* Roll back a prepared artifact when Engine START could not be submitted. */
enum rdx_storage_result rdx_record_coordinator_cancel_start(
    struct rdx_record_coordinator *coordinator,
    enum rdx_record_cause cause);

/* Return the current persistent identity using the reference Wire filename
 * shape (basename only).  It is available after Local START preparation and
 * remains valid through the synchronous normal STOP indication. */
int rdx_record_coordinator_get_record_identity(
    const struct rdx_record_coordinator *coordinator,
    uint32_t *serial_number, char *filename, uint32_t filename_capacity);

#endif /* RDX_RECORD_COORDINATOR_H */
