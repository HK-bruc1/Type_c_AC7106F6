#ifndef RDX_LOCAL_STORAGE_SERVICE_H
#define RDX_LOCAL_STORAGE_SERVICE_H

#include <stdint.h>

#if CONFIG_CPU_BR56
#include "system/includes.h"
#endif

#include "rdx/domain/rdx_artifact.h"
#include "rdx/ports/rdx_legacy_uxfile_port.h"
#include "rdx/ports/rdx_native_artifact_port.h"
#include "rdx/ports/rdx_storage_media_port.h"

/* The queue is deliberately fixed-size: a Local REQUIRED destination must
 * fail deterministically instead of allocating until the capture task dies. */
#ifndef RDX_LOCAL_STORAGE_QUEUE_DEPTH
#define RDX_LOCAL_STORAGE_QUEUE_DEPTH 16
#endif

/* Persist WRITING progress at a bounded cadence.  A marker transaction opens,
 * closes and reads the marker back on BR56 FAT, so doing it for every audio
 * frame would block the REQUIRED destination long enough to overflow Engine's
 * candidate queue.  Normal SEAL still persists the exact final boundary. */
#ifndef RDX_LOCAL_STORAGE_CHECKPOINT_INTERVAL_MS
#define RDX_LOCAL_STORAGE_CHECKPOINT_INTERVAL_MS 1000
#endif

/* Product builds override this from rdx_integration_config.h.  Keep the safe
 * O0 policy as the standalone/Host default as well: terminal state changes
 * are still persisted, but no periodic WRITING transaction is generated. */
#ifndef RDX_CFG_LOCAL_STORAGE_PERIODIC_CHECKPOINT_ENABLE
#define RDX_CFG_LOCAL_STORAGE_PERIODIC_CHECKPOINT_ENABLE 0
#endif

/* Native audio is persisted in small contiguous batches.  480 B holds six
 * 80 B Opus frames while staying below the next 512 B FAT allocation step;
 * the frame cap also bounds STOP latency and per-run lock ownership. */
#ifndef RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_BYTES
#define RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_BYTES 480
#endif
#ifndef RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_FRAMES
#define RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_FRAMES 6
#endif

enum rdx_local_storage_backend {
    RDX_LOCAL_STORAGE_BACKEND_NATIVE = 1,
    RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE = 2,
};

enum rdx_local_storage_completion_type {
    RDX_LOCAL_STORAGE_COMPLETION_NONE = 0,
    RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_COMMITTED,
    RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_FAILED,
    RDX_LOCAL_STORAGE_COMPLETION_LEGACY_FILE_COMMITTED,
    RDX_LOCAL_STORAGE_COMPLETION_LEGACY_FILE_FAILED,
};

/* APP file commands arrive on btstack and must never execute FAT there.
 * These operations are copied into the existing rdx_storage worker.  Caller
 * owned output buffers must remain valid until the completion callback. */
enum rdx_local_storage_operation_type {
    RDX_LOCAL_STORAGE_OPERATION_NONE = 0,
    RDX_LOCAL_STORAGE_OPERATION_CAPACITY,
    RDX_LOCAL_STORAGE_OPERATION_ENUMERATE,
    RDX_LOCAL_STORAGE_OPERATION_READ_BY_SERIAL,
    RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY,
    RDX_LOCAL_STORAGE_OPERATION_FORMAT,
};

struct rdx_local_storage_operation_request {
    enum rdx_local_storage_operation_type type;
    uint32_t token;
    union {
        struct {
            struct rdx_storage_file_info *items;
            uint32_t capacity;
        } enumerate;
        struct {
            uint32_t serial_number;
            uint64_t offset;
            uint8_t *buffer;
            uint32_t length;
        } read;
        struct {
            uint32_t serial_number;
            char filename[RDX_STORAGE_NAME_MAX];
        } delete_file;
    } data;
};

struct rdx_local_storage_operation_completion {
    enum rdx_local_storage_operation_type type;
    enum rdx_storage_result result;
    uint32_t token;
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint32_t count;
    uint32_t read_length;
    struct rdx_storage_file_info file;
};

typedef void (*rdx_local_storage_operation_callback_t)(
    void *priv,
    const struct rdx_local_storage_operation_completion *completion);

struct rdx_local_storage_completion {
    enum rdx_local_storage_completion_type type;
    enum rdx_storage_result result;
    struct rdx_artifact_id artifact_id;
    uint32_t generation;
    uint32_t serial_number;
    uint32_t frame_count;
    uint32_t produced_frame_count;
    uint32_t destination_accepted_count;
    uint32_t local_accepted_count;
    uint32_t persisted_frame_count;
    uint32_t rejected_frame_count;
    uint32_t dropped_frame_count;
    uint64_t file_size;
    uint32_t audio_crc;
    enum rdx_record_cause cause;
};

typedef void (*rdx_local_storage_completion_callback_t)(
    void *priv, const struct rdx_local_storage_completion *completion);

struct rdx_local_storage_frame_slot {
    struct rdx_record_frame frame;
    uint8_t payload[RDX_RECORD_CANDIDATE_MAX_SIZE];
};

struct rdx_local_storage_service {
    enum rdx_local_storage_backend backend;
    enum rdx_storage_readiness readiness;
    const struct rdx_storage_media_ops *media_ops;
    const struct rdx_native_artifact_ops *native_ops;
    const struct rdx_legacy_uxfile_ops *legacy_ops;
    void *artifact_handle;
    struct rdx_storage_file_id file_id;
    struct rdx_artifact artifact;
    uint32_t session_id;
    uint32_t capture_generation;
    uint8_t active;
    uint8_t capture_detached;
    uint8_t seal_requested;
    uint8_t completion_sent;
    uint8_t recovery_checked;
    uint8_t queue_head;
    uint8_t queue_tail;
    uint8_t queue_count;
    uint32_t checkpoint_persisted_frame_count;
    struct rdx_local_storage_frame_slot queue[RDX_LOCAL_STORAGE_QUEUE_DEPTH];
    union {
        struct rdx_storage_index_entry index_entries[32];
        struct rdx_storage_file_info file_items[32];
        /* Recording and media/index operations are mutually exclusive.  Use
         * the existing scratch storage for Native write coalescing so the
         * 1024-word storage task stack does not absorb another 480 bytes. */
        uint8_t write_batch[RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_BYTES];
    } scratch;
    rdx_local_storage_completion_callback_t completion_callback;
    void *completion_priv;
    struct rdx_local_storage_operation_request operation;
    rdx_local_storage_operation_callback_t operation_callback;
    void *operation_priv;
    uint8_t operation_pending;
    uint8_t operation_running;
#if CONFIG_CPU_BR56
    /* The target service owns one worker and serializes all backend calls.
     * Host tests intentionally compile without the SDK task primitives. */
    OS_MUTEX lock;
    uint8_t lock_initialized;
    uint8_t worker_created;
    uint8_t worker_stop;
    uint8_t worker_wake_pending;
#endif
};

int rdx_local_storage_service_init(
    struct rdx_local_storage_service *service,
    enum rdx_local_storage_backend backend,
    const struct rdx_storage_media_ops *media_ops,
    const struct rdx_native_artifact_ops *native_ops,
    const struct rdx_legacy_uxfile_ops *legacy_ops,
    rdx_local_storage_completion_callback_t completion_callback,
    void *completion_priv);

/* Validate the bounded Native marker/index recovery path after mount.  This
 * is intentionally separate from init so a temporary media outage can be
 * retried without rebuilding the Service or spawning another worker. */
enum rdx_storage_result rdx_local_storage_service_recover(
    struct rdx_local_storage_service *service);

/* Reserve a persistent Native serial number before Engine START and derive
 * the first-version raw Opus artifact identity/names from that reservation.
 * The serial high-water commit intentionally survives a later START rollback,
 * so power loss, failed capture open and file deletion cannot reuse an ID. */
enum rdx_storage_result rdx_local_storage_service_reserve_artifact(
    struct rdx_local_storage_service *service,
    enum rdx_record_format_id format_id,
    struct rdx_storage_artifact_spec *spec);

enum rdx_storage_result rdx_local_storage_service_start(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_artifact_spec *spec);
enum rdx_storage_result rdx_local_storage_service_attach(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation);
/* Preserve one Artifact across PAUSE/RESUME while rejecting stale frames
 * from the closed capture instance. */
enum rdx_storage_result rdx_local_storage_service_rebind_capture(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t old_capture_generation, uint32_t new_capture_generation);
/* Cancel an artifact that was prepared before Record Engine assigned the
 * runtime session/capture token.  This is only valid before attach(). */
enum rdx_storage_result rdx_local_storage_service_cancel_start(
    struct rdx_local_storage_service *service, enum rdx_record_cause cause);
enum rdx_storage_result rdx_local_storage_service_consume(
    struct rdx_local_storage_service *service,
    const struct rdx_record_frame *frame);
enum rdx_storage_result rdx_local_storage_service_pump(
    struct rdx_local_storage_service *service, uint8_t budget);
enum rdx_storage_result rdx_local_storage_service_seal(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation);
enum rdx_storage_result rdx_local_storage_service_abort(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation, enum rdx_record_cause cause);
void rdx_local_storage_service_detach_capture(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation);
enum rdx_storage_readiness rdx_local_storage_service_readiness(
    const struct rdx_local_storage_service *service);
const struct rdx_artifact *rdx_local_storage_service_artifact(
    const struct rdx_local_storage_service *service);
enum rdx_storage_result rdx_local_storage_service_get_capacity(
    struct rdx_local_storage_service *service, uint64_t *total_bytes,
    uint64_t *free_bytes);
enum rdx_storage_result rdx_local_storage_service_enumerate(
    struct rdx_local_storage_service *service,
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count);
enum rdx_storage_result rdx_local_storage_service_delete(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_file_id *file_id);
enum rdx_storage_result rdx_local_storage_service_format(
    struct rdx_local_storage_service *service);
/* Submit one bounded APP-facing operation.  Target builds complete it on the
 * rdx_storage worker; host builds complete synchronously for deterministic
 * Golden Trace tests while preserving the same callback contract. */
enum rdx_storage_result rdx_local_storage_service_submit_operation(
    struct rdx_local_storage_service *service,
    const struct rdx_local_storage_operation_request *request,
    rdx_local_storage_operation_callback_t callback, void *priv);
enum rdx_storage_result rdx_local_storage_service_quiesce(
    struct rdx_local_storage_service *service, uint16_t timeout_ms);
/* Stop the target worker and release any active artifact before the runtime
 * tears down the transport.  This is idempotent and also serves build-only
 * callers that do not have a target task implementation. */
enum rdx_storage_result rdx_local_storage_service_shutdown(
    struct rdx_local_storage_service *service, uint16_t timeout_ms);

#endif /* RDX_LOCAL_STORAGE_SERVICE_H */
