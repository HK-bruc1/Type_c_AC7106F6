#include <stddef.h>
#if !CONFIG_CPU_BR56
#include <stdio.h>
#endif
#include <string.h>

#include "rdx/rdx_local_storage_service.h"

#if CONFIG_CPU_BR56
#include "task.h"
#include "user_cfg.h"
#define RDX_LOCAL_STORAGE_TASK_NAME       "rdx_storage"
#define RDX_LOCAL_STORAGE_TASK_WAKE       0x52445802
#define RDX_LOCAL_STORAGE_PUMP_BUDGET     4
#define RDX_LOCAL_STORAGE_SEAL_WAIT_TICKS 100
#endif

#define RDX_COMPAT_FILENAME_ATTEMPTS      64

/* CRC32/IEEE is part of the Native Artifact checkpoint contract.  Keep the
 * serialized value in the Artifact (rather than a backend-private running
 * state) so a checkpoint can be compared with the value returned by close().
 * The first frame uses the standard init/xorout pair; subsequent frames feed
 * the previous finalized value back into the running state. */
static uint32_t service_crc32_update(uint32_t previous,
                                     uint32_t frame_count,
                                     const uint8_t *data, uint32_t len)
{
    uint32_t crc = frame_count ? (previous ^ 0xffffffffUL) : 0xffffffffUL;
    uint32_t i;

    for (i = 0; i < len; i++) {
        uint32_t bit;

        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1)
                  ^ (0xedb88320UL & (uint32_t)-(int)(crc & 1));
        }
    }
    return crc ^ 0xffffffffUL;
}

#if CONFIG_CPU_BR56
static void service_lock(struct rdx_local_storage_service *service)
{
    if (service && service->lock_initialized) {
        os_mutex_pend(&service->lock, 0);
    }
}

static void service_unlock(struct rdx_local_storage_service *service)
{
    if (service && service->lock_initialized) {
        os_mutex_post(&service->lock);
    }
}

static void service_wake(struct rdx_local_storage_service *service)
{
    if (service && service->worker_created) {
        os_taskq_post_msg(RDX_LOCAL_STORAGE_TASK_NAME, 1,
                          RDX_LOCAL_STORAGE_TASK_WAKE);
    }
}
#else
static void service_lock(struct rdx_local_storage_service *service)
{
    (void)service;
}

static void service_unlock(struct rdx_local_storage_service *service)
{
    (void)service;
}

static void service_wake(struct rdx_local_storage_service *service)
{
    (void)service;
}
#endif

static uint8_t service_operation_busy(
    const struct rdx_local_storage_service *service)
{
    return service
           && (service->operation_pending || service->operation_running);
}

static int service_ids_match(const struct rdx_local_storage_service *service,
                             uint32_t session_id,
                             uint32_t capture_generation)
{
    return service && service->active && service->session_id == session_id
           && service->capture_generation == capture_generation;
}

static void service_fill_marker(
    const struct rdx_local_storage_service *service,
    uint32_t marker_state, struct rdx_storage_marker *marker)
{
    const struct rdx_artifact *artifact = &service->artifact;

    memset(marker, 0, sizeof(*marker));
    marker->artifact_id = artifact->spec.artifact_id;
    marker->generation = artifact->generation;
    marker->state = marker_state;
    marker->session_id = artifact->spec.session_id;
    marker->serial_number = artifact->spec.serial_number;
    marker->produced_frame_count = artifact->produced_frame_count;
    marker->destination_accepted_count =
        artifact->destination_accepted_count;
    marker->local_accepted_count = artifact->local_accepted_count;
    marker->persisted_frame_count = artifact->persisted_frame_count;
    marker->rejected_frame_count = artifact->rejected_frame_count;
    marker->dropped_frame_count = artifact->dropped_frame_count;
    marker->first_error_cause = artifact->first_error_cause;
    marker->audio_bytes = artifact->audio_bytes;
    marker->audio_crc = artifact->audio_crc;
    memcpy(marker->temp_name, artifact->spec.temp_name,
           sizeof(marker->temp_name));
    memcpy(marker->final_name, artifact->spec.final_name,
           sizeof(marker->final_name));
}

static void service_emit(struct rdx_local_storage_service *service,
                         enum rdx_local_storage_completion_type type,
                         enum rdx_storage_result result,
                         enum rdx_record_cause cause)
{
    struct rdx_local_storage_completion completion;

    if (!service || service->completion_sent) {
        return;
    }
    memset(&completion, 0, sizeof(completion));
    completion.type = type;
    completion.result = result;
    completion.artifact_id = service->artifact.spec.artifact_id;
    completion.generation = service->artifact.generation;
    completion.serial_number = service->artifact.spec.serial_number;
    completion.frame_count = service->artifact.persisted_frame_count;
    completion.produced_frame_count =
        service->artifact.produced_frame_count;
    completion.destination_accepted_count =
        service->artifact.destination_accepted_count;
    completion.local_accepted_count =
        service->artifact.local_accepted_count;
    completion.persisted_frame_count =
        service->artifact.persisted_frame_count;
    completion.rejected_frame_count =
        service->artifact.rejected_frame_count;
    completion.dropped_frame_count =
        service->artifact.dropped_frame_count;
    completion.file_size = service->artifact.audio_bytes;
    completion.audio_crc = service->artifact.audio_crc;
    completion.cause = cause;
    service->completion_sent = 1;
    if (service->completion_callback) {
        service->completion_callback(service->completion_priv, &completion);
    }
}

/* A failure can happen after the backend has appended the new entry but
 * before the INDEXED marker is durable.  Best-effort removal keeps a stale
 * index entry from being advertised as a committed file.  If the media is
 * already unavailable, recovery will retry from the marker/index pair. */
static void service_remove_native_index_entry(
    struct rdx_local_storage_service *service)
{
    struct rdx_storage_index_snapshot snapshot;
    uint32_t i;

    if (!service || service->backend != RDX_LOCAL_STORAGE_BACKEND_NATIVE
        || !service->native_ops || !service->native_ops->load_index
        || !service->native_ops->commit_index) {
        return;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.entries = service->scratch.index_entries;
    snapshot.capacity = sizeof(service->scratch.index_entries)
                        / sizeof(service->scratch.index_entries[0]);
    if (service->native_ops->load_index(&snapshot)
        != RDX_STORAGE_RESULT_READY
        || snapshot.count > snapshot.capacity) {
        return;
    }
    for (i = 0; i < snapshot.count; i++) {
        if (snapshot.entries[i].file.file_id.artifact_id.value
            == service->artifact.spec.artifact_id.value) {
            if (i + 1 < snapshot.count) {
                memmove(&snapshot.entries[i], &snapshot.entries[i + 1],
                        (snapshot.count - i - 1)
                        * sizeof(snapshot.entries[0]));
            }
            snapshot.count--;
            (void)service->native_ops->commit_index(&snapshot);
            return;
        }
    }
}

static enum rdx_storage_result service_fail(
    struct rdx_local_storage_service *service,
    enum rdx_storage_result result, enum rdx_record_cause cause)
{
    struct rdx_storage_marker marker;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (service->artifact.state != RDX_ARTIFACT_STATE_NONE
        && service->artifact.state != RDX_ARTIFACT_STATE_FAILED
        && service->artifact.state != RDX_ARTIFACT_STATE_QUARANTINED) {
        rdx_artifact_fail(&service->artifact, cause);
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops) {
        if (service->artifact_handle && service->native_ops->close) {
            uint64_t file_size = 0;
            uint32_t audio_crc = 0;

            service->native_ops->close(service->artifact_handle,
                                       &file_size, &audio_crc);
            service->artifact_handle = NULL;
        }
        if (service->native_ops->store_marker) {
            service_fill_marker(service, RDX_STORAGE_MARKER_STATE_FAILED,
                                &marker);
            service->native_ops->store_marker(&marker);
        }
        service_remove_native_index_entry(service);
        if (service->native_ops->quarantine) {
            enum rdx_storage_result quarantine_result;

            quarantine_result = service->native_ops->quarantine(
                &service->artifact.spec.artifact_id, "rdx-storage-failed");
            if (quarantine_result == RDX_STORAGE_RESULT_READY
                && service->artifact.state == RDX_ARTIFACT_STATE_FAILED) {
                rdx_artifact_quarantine(&service->artifact);
            }
        }
    } else if (service->backend == RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE
               && service->legacy_ops && service->legacy_ops->abort_file) {
        service->legacy_ops->abort_file(&service->file_id);
    }
    service->queue_head = 0;
    service->queue_tail = 0;
    service->queue_count = 0;
    service->checkpoint_persisted_frame_count = 0;
    service->active = 0;
    service->capture_detached = 1;
    service->readiness =
        service->artifact.state == RDX_ARTIFACT_STATE_QUARANTINED
            ? RDX_STORAGE_READINESS_READY
            : RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
    service_emit(service,
                 service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
                     ? RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_FAILED
                     : RDX_LOCAL_STORAGE_COMPLETION_LEGACY_FILE_FAILED,
                 result, cause);
    return result;
}

static enum rdx_storage_result service_operation_enumerate_scratch(
    struct rdx_local_storage_service *service, uint32_t *count)
{
    uint32_t capacity = sizeof(service->scratch.file_items)
                        / sizeof(service->scratch.file_items[0]);

    memset(service->scratch.file_items, 0,
           sizeof(service->scratch.file_items));
    return service->media_ops->enumerate(
        service->scratch.file_items, capacity, count);
}

static struct rdx_storage_file_info *service_operation_find_file(
    struct rdx_local_storage_service *service, uint32_t serial_number,
    const char *filename, enum rdx_storage_result *result)
{
    struct rdx_storage_file_info *match = NULL;
    uint32_t count = 0;
    uint32_t i;

    *result = service_operation_enumerate_scratch(service, &count);
    if (*result != RDX_STORAGE_RESULT_READY) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        struct rdx_storage_file_info *item =
            &service->scratch.file_items[i];

        if (item->file_id.serial_number != serial_number) {
            continue;
        }
        if (filename
            && strncmp(item->file_id.name, filename,
                       sizeof(item->file_id.name))) {
            continue;
        }
        if (match) {
            *result = RDX_STORAGE_RESULT_CORRUPT;
            return NULL;
        }
        match = item;
    }
    if (!match) {
        *result = RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    return match;
}

static void service_operation_update_readiness(
    struct rdx_local_storage_service *service,
    enum rdx_storage_result result)
{
    if (result == RDX_STORAGE_RESULT_READY
        || result == RDX_STORAGE_RESULT_BAD_ARGUMENT
        || result == RDX_STORAGE_RESULT_NO_SPACE) {
        return;
    }
    if (result == RDX_STORAGE_RESULT_NOT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        service->recovery_checked = 0;
    } else if (result == RDX_STORAGE_RESULT_CORRUPT
               || result == RDX_STORAGE_RESULT_CHECKSUM) {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
    }
}

static enum rdx_storage_result service_format_and_recover(
    struct rdx_local_storage_service *service,
    uint64_t *total_bytes, uint64_t *free_bytes)
{
    enum rdx_storage_result result;

    if (!service || !service->media_ops || !service->media_ops->format
        || !service->media_ops->ensure_ready) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service->readiness = RDX_STORAGE_READINESS_RECOVERING;
    service->recovery_checked = 0;
    result = service->media_ops->format();
    if (result == RDX_STORAGE_RESULT_READY) {
        result = service->media_ops->ensure_ready();
    }
    if (result == RDX_STORAGE_RESULT_READY
        && service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops && service->native_ops->recover) {
        result = service->native_ops->recover();
    }
    if (result == RDX_STORAGE_RESULT_READY
        && total_bytes && free_bytes && service->media_ops->get_capacity) {
        result = service->media_ops->get_capacity(
            total_bytes, free_bytes);
    }
    if (result == RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_READY;
        service->recovery_checked = 1;
    } else if (result == RDX_STORAGE_RESULT_NOT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
    } else {
        /* A failed or only partially completed format cannot leave the
         * Service indefinitely in RECOVERING.  Gate recording until an
         * explicit bounded recovery proves the new media state. */
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
    }
    return result;
}

static void service_process_operation(
    struct rdx_local_storage_service *service)
{
    struct rdx_local_storage_operation_request request;
    struct rdx_local_storage_operation_completion completion;
    rdx_local_storage_operation_callback_t callback;
    void *callback_priv;

    memset(&completion, 0, sizeof(completion));
    service_lock(service);
    if (!service || !service->operation_pending
        || service->operation_running) {
        service_unlock(service);
        return;
    }
    request = service->operation;
    callback = service->operation_callback;
    callback_priv = service->operation_priv;
    service->operation_pending = 0;
    service->operation_running = 1;
    service_unlock(service);

    completion.type = request.type;
    completion.token = request.token;
    completion.result = RDX_STORAGE_RESULT_UNSUPPORTED;
    switch (request.type) {
    case RDX_LOCAL_STORAGE_OPERATION_CAPACITY:
        if (service->media_ops->get_capacity) {
            completion.result = service->media_ops->get_capacity(
                &completion.total_bytes, &completion.free_bytes);
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_ENUMERATE:
        if (service->media_ops->enumerate) {
            completion.result = service->media_ops->enumerate(
                request.data.enumerate.items,
                request.data.enumerate.capacity, &completion.count);
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_READ_BY_SERIAL:
        if (service->media_ops->enumerate && service->media_ops->read) {
            enum rdx_storage_result find_result;
            struct rdx_storage_file_info *file;

            file = service_operation_find_file(
                service, request.data.read.serial_number, NULL,
                &find_result);
            completion.result = find_result;
            if (file) {
                completion.file = *file;
                if (request.data.read.offset >= file->file_size) {
                    completion.result = RDX_STORAGE_RESULT_BAD_ARGUMENT;
                } else {
                    completion.result = service->media_ops->read(
                        &file->file_id, request.data.read.offset,
                        request.data.read.buffer, request.data.read.length,
                        &completion.read_length);
                }
            }
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY:
        if (service->media_ops->enumerate && service->media_ops->delete) {
            enum rdx_storage_result find_result;
            struct rdx_storage_file_info *file;

            file = service_operation_find_file(
                service, request.data.delete_file.serial_number,
                request.data.delete_file.filename, &find_result);
            completion.result = find_result;
            if (file) {
                completion.file = *file;
                completion.result = service->media_ops->delete(
                    &file->file_id);
            }
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_FORMAT:
        if (service->media_ops->format) {
            completion.result = service_format_and_recover(
                service, &completion.total_bytes, &completion.free_bytes);
        }
        break;
    default:
        completion.result = RDX_STORAGE_RESULT_BAD_ARGUMENT;
        break;
    }

    service_lock(service);
    service_operation_update_readiness(service, completion.result);
    service->operation_running = 0;
    memset(&service->operation, 0, sizeof(service->operation));
    service->operation_callback = NULL;
    service->operation_priv = NULL;
    service_unlock(service);
    if (callback) {
        callback(callback_priv, &completion);
    }
}

#if CONFIG_CPU_BR56
static void rdx_local_storage_service_task(void *priv)
{
    struct rdx_local_storage_service *service = priv;
    int msg[4];

    if (!service) {
        return;
    }
    while (!service->worker_stop) {
        uint8_t has_operation;
        uint8_t has_work;

        service_lock(service);
        has_operation = service->operation_pending;
        has_work = service->active && service->queue_count;
        service_unlock(service);
        if (has_operation) {
            service_process_operation(service);
            continue;
        }
        if (has_work) {
            if (rdx_local_storage_service_pump(
                    service, RDX_LOCAL_STORAGE_PUMP_BUDGET)
                != RDX_STORAGE_RESULT_READY) {
                /* service_fail() has emitted the first error and detached the
                 * capture.  Leave the worker alive for a later START. */
                continue;
            }
            continue;
        }
        os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
    }
}
#endif

int rdx_local_storage_service_init(
    struct rdx_local_storage_service *service,
    enum rdx_local_storage_backend backend,
    const struct rdx_storage_media_ops *media_ops,
    const struct rdx_native_artifact_ops *native_ops,
    const struct rdx_legacy_uxfile_ops *legacy_ops,
    rdx_local_storage_completion_callback_t completion_callback,
    void *completion_priv)
{
    if (!service || !media_ops || !media_ops->ensure_ready
        || (backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
            && (!native_ops || !native_ops->open_temp || !native_ops->append
                || !native_ops->close || !native_ops->publish
                || !native_ops->store_marker || !native_ops->load_index
                || !native_ops->commit_index || !native_ops->quarantine))
        || (backend == RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE
            && (!legacy_ops || !legacy_ops->init || !legacy_ops->begin_file
                || !legacy_ops->raw_write || !legacy_ops->flush_commit
                || !legacy_ops->abort_file))) {
        return -1;
    }
    if (backend != RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && backend != RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE) {
        return -1;
    }
    memset(service, 0, sizeof(*service));
    service->backend = backend;
    service->readiness = RDX_STORAGE_READINESS_UNINITIALIZED;
    service->media_ops = media_ops;
    service->native_ops = native_ops;
    service->legacy_ops = legacy_ops;
    service->completion_callback = completion_callback;
    service->completion_priv = completion_priv;
#if CONFIG_CPU_BR56
    os_mutex_create(&service->lock);
    service->lock_initialized = 1;
    service->worker_stop = 0;
    if (task_create(rdx_local_storage_service_task, service,
                    RDX_LOCAL_STORAGE_TASK_NAME)) {
        service->lock_initialized = 0;
        return -1;
    }
    service->worker_created = 1;
#endif
    return 0;
}

enum rdx_storage_result rdx_local_storage_service_recover(
    struct rdx_local_storage_service *service)
{
    enum rdx_storage_result result;

    if (!service || !service->media_ops || !service->media_ops->ensure_ready) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
    service->readiness = RDX_STORAGE_READINESS_RECOVERING;
    result = service->media_ops->ensure_ready();
    if (result == RDX_STORAGE_RESULT_READY
        && service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops && service->native_ops->recover) {
        result = service->native_ops->recover();
    }
    if (result == RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_READY;
        service->recovery_checked = 1;
    } else if (result == RDX_STORAGE_RESULT_NOT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        service->recovery_checked = 0;
    } else {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
    }
    service_unlock(service);
    return result;
}

static const char *service_filename_basename(const char *name)
{
    const char *slash;

    if (!name) {
        return "";
    }
    slash = strrchr(name, '/');
    return slash ? slash + 1 : name;
}

static uint8_t service_filename_exists(
    const struct rdx_storage_index_snapshot *snapshot,
    const char *filename)
{
    uint32_t i;

    if (!snapshot || !filename || !filename[0]) {
        return 0;
    }
    for (i = 0; i < snapshot->count; i++) {
        const char *existing = service_filename_basename(
            snapshot->entries[i].file.file_id.name);

        if (!strcmp(existing, filename)) {
            return 1;
        }
    }
    return 0;
}

static void service_filename_random_bytes(uint32_t serial_number,
                                          uint32_t attempt,
                                          uint8_t random_bytes[3])
{
#if CONFIG_CPU_BR56
    (void)serial_number;
    (void)attempt;
    get_random_number(random_bytes, 3);
#else
    /* Host tests need repeatable names while the target follows the reference
     * implementation and draws three bytes from JL's random provider. */
    uint32_t value = serial_number ^ ((attempt + 1) * 0x9e3779b9UL);

    value ^= value >> 16;
    value *= 0x7feb352dUL;
    value ^= value >> 15;
    random_bytes[0] = (uint8_t)value;
    random_bytes[1] = (uint8_t)(value >> 8);
    random_bytes[2] = (uint8_t)(value >> 16);
#endif
}

static int service_generate_compat_filename(
    const struct rdx_storage_index_snapshot *snapshot,
    uint32_t serial_number, char *filename, uint32_t capacity)
{
    uint32_t attempt;

    if (!snapshot || !serial_number || !filename || !capacity) {
        return -1;
    }
    for (attempt = 0; attempt < RDX_COMPAT_FILENAME_ATTEMPTS; attempt++) {
        uint8_t random_bytes[3];
        int len;

        service_filename_random_bytes(serial_number, attempt, random_bytes);
        len = snprintf(filename, capacity, "%x%x%x.raw",
                       random_bytes[0], random_bytes[1], random_bytes[2]);
        if (len <= 0 || (uint32_t)len >= capacity) {
            return -1;
        }
        if (!service_filename_exists(snapshot, filename)) {
            return 0;
        }
    }
    filename[0] = 0;
    return -1;
}

static int service_fill_reserved_spec(
    enum rdx_record_format_id format_id, uint32_t serial_number,
    const struct rdx_storage_index_snapshot *snapshot,
    struct rdx_storage_artifact_spec *spec)
{
    char filename[RDX_STORAGE_NAME_MAX];
    int temp_len;
    int final_len;

    if (!spec || !serial_number || !snapshot) {
        return -1;
    }
    memset(spec, 0, sizeof(*spec));
    spec->artifact_id.value = serial_number;
    spec->serial_number = serial_number;
    spec->format_id = format_id;
    switch (format_id) {
    case RDX_RECORD_FORMAT_MEETING_V1:
        spec->sample_rate = RDX_RECORD_MEETING_V1_SAMPLE_RATE;
        spec->channels = RDX_RECORD_MEETING_V1_CHANNELS;
        spec->frame_ms = RDX_RECORD_MEETING_V1_FRAME_MS;
        spec->payload_size = RDX_RECORD_MEETING_V1_PAYLOAD_SIZE;
        break;
    case RDX_RECORD_FORMAT_CALL_V1:
        spec->sample_rate = RDX_RECORD_CALL_V1_SAMPLE_RATE;
        spec->channels = RDX_RECORD_CALL_V1_CHANNELS;
        spec->frame_ms = RDX_RECORD_CALL_V1_FRAME_MS;
        spec->payload_size = RDX_RECORD_CALL_V1_PAYLOAD_SIZE;
        break;
    default:
        return -1;
    }
    if (service_generate_compat_filename(snapshot, serial_number,
                                         filename, sizeof(filename))) {
        memset(spec, 0, sizeof(*spec));
        return -1;
    }
    temp_len = snprintf(spec->temp_name, sizeof(spec->temp_name),
                        "RDXTMP/T%08X.OPU", (unsigned int)serial_number);
    /* Legacy UXFILE and the APP file commands address recordings by a bare
     * filename below storage/sd0/C/.  Keep the published Artifact in that
     * root so list/read/delete remain wire-compatible with the reference
     * implementation; only transactional files live in internal folders. */
    final_len = snprintf(spec->final_name, sizeof(spec->final_name),
                         "%s", filename);
    if (temp_len <= 0 || temp_len >= sizeof(spec->temp_name)
        || final_len <= 0 || final_len >= sizeof(spec->final_name)) {
        memset(spec, 0, sizeof(*spec));
        return -1;
    }
    return 0;
}

enum rdx_storage_result rdx_local_storage_service_reserve_artifact(
    struct rdx_local_storage_service *service,
    enum rdx_record_format_id format_id,
    struct rdx_storage_artifact_spec *spec)
{
    struct rdx_storage_index_snapshot snapshot;
    enum rdx_storage_result result;
    uint32_t serial_number;

    if (!service || !spec) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (service->backend != RDX_LOCAL_STORAGE_BACKEND_NATIVE
        || !service->native_ops || !service->native_ops->load_index
        || !service->native_ops->commit_index) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
    if ((service->native_ops->recover && !service->recovery_checked)
        || service->readiness == RDX_STORAGE_READINESS_RECOVERY_REQUIRED
        || service->readiness == RDX_STORAGE_READINESS_HOST_OWNED
        || service->readiness == RDX_STORAGE_READINESS_QUIESCING) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    result = service->media_ops->ensure_ready();
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        service->recovery_checked = 0;
        service_unlock(service);
        return result;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.entries = service->scratch.index_entries;
    snapshot.capacity = sizeof(service->scratch.index_entries)
                        / sizeof(service->scratch.index_entries[0]);
    result = service->native_ops->load_index(&snapshot);
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
        service_unlock(service);
        return result;
    }
    if (snapshot.count >= snapshot.capacity) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_NO_SPACE;
    }
    if (snapshot.serial_high_water == UINT32_MAX) {
        service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
        service->recovery_checked = 0;
        service_unlock(service);
        return RDX_STORAGE_RESULT_CORRUPT;
    }
    serial_number = snapshot.serial_high_water + 1;
    if (service_fill_reserved_spec(format_id, serial_number, &snapshot,
                                   spec)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    snapshot.serial_high_water = serial_number;
    result = service->native_ops->commit_index(&snapshot);
    if (result != RDX_STORAGE_RESULT_READY) {
        memset(spec, 0, sizeof(*spec));
        if (result == RDX_STORAGE_RESULT_CORRUPT
            || result == RDX_STORAGE_RESULT_CHECKSUM) {
            service->readiness = RDX_STORAGE_READINESS_RECOVERY_REQUIRED;
            service->recovery_checked = 0;
        }
        service_unlock(service);
        return result;
    }
    service_unlock(service);
    printf("[RDX][LOCAL] reserved artifact=%08X sn=%u format=%u"
           " temp=%s final=%s\n",
           (unsigned int)spec->artifact_id.value,
           (unsigned int)spec->serial_number, spec->format_id,
           spec->temp_name, spec->final_name);
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_start(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_artifact_spec *spec)
{
    enum rdx_storage_result result;

    if (!service || !spec || !spec->artifact_id.value
        || !spec->serial_number || !spec->frame_ms || !spec->payload_size
        || spec->payload_size > RDX_RECORD_CANDIDATE_MAX_SIZE
        || !spec->temp_name[0] || !spec->final_name[0]) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (service->active || service_operation_busy(service)) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops && service->native_ops->recover
        && !service->recovery_checked) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (service->readiness == RDX_STORAGE_READINESS_RECOVERY_REQUIRED
        || service->readiness == RDX_STORAGE_READINESS_HOST_OWNED
        || service->readiness == RDX_STORAGE_READINESS_QUIESCING) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service->readiness = RDX_STORAGE_READINESS_PROBING;
    result = service->media_ops->ensure_ready();
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
            && service->native_ops && service->native_ops->recover) {
            /* The mount may represent a replaced or power-cycled medium when
             * it becomes available again.  Do not reuse recovery evidence
             * obtained before the readiness failure. */
            service->recovery_checked = 0;
        }
        return result;
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_LEGACY_UXFILE) {
        result = service->legacy_ops->init();
        if (result != RDX_STORAGE_RESULT_READY) {
            service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
            return result;
        }
    }
    service->artifact_handle = NULL;
    service->session_id = spec->session_id;
    service->capture_generation = 0;
    service->capture_detached = 0;
    service->seal_requested = 0;
    service->completion_sent = 0;
    service->queue_head = 0;
    service->queue_tail = 0;
    service->queue_count = 0;
    service->checkpoint_persisted_frame_count = 0;
    rdx_artifact_init(&service->artifact);
    if (rdx_artifact_begin(&service->artifact, spec)) {
        service->readiness = RDX_STORAGE_READINESS_READY;
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    memset(&service->file_id, 0, sizeof(service->file_id));
    service->file_id.artifact_id = spec->artifact_id;
    service->file_id.serial_number = spec->serial_number;
    memcpy(service->file_id.name, spec->final_name,
           sizeof(service->file_id.name));
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
        struct rdx_storage_marker marker;

        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_ALLOCATED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
        result = service->native_ops->open_temp(
            spec, &service->artifact_handle);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
    } else {
        result = service->legacy_ops->begin_file(spec, &service->file_id);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
    }
    if (rdx_artifact_mark_writing(&service->artifact)) {
        return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                            RDX_RECORD_CAUSE_START_FAILED);
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
        struct rdx_storage_marker marker;

        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_WRITING,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_START_FAILED);
        }
    }
    service->active = 1;
    service->readiness = RDX_STORAGE_READINESS_READY;
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_attach(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    if (!service || !service->active || !capture_generation
        || (service->session_id && service->session_id != session_id)
        || service->capture_generation) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    /* A coordinator may have to prepare the artifact before Record Engine
     * allocates its session token.  Bind that wildcard exactly once at the
     * attach cut, then persist the real session id in the WRITING marker. */
    if (!service->session_id) {
        service->session_id = session_id;
        service->artifact.spec.session_id = session_id;
        if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
            && service->native_ops && service->native_ops->store_marker) {
            struct rdx_storage_marker marker;

            service_fill_marker(service, RDX_STORAGE_MARKER_STATE_WRITING,
                                &marker);
            if (service->native_ops->store_marker(&marker)
                != RDX_STORAGE_RESULT_READY) {
                return service_fail(service, RDX_STORAGE_RESULT_IO_ERROR,
                                    RDX_RECORD_CAUSE_START_FAILED);
            }
        }
    }
    service->capture_generation = capture_generation;
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_rebind_capture(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t old_capture_generation, uint32_t new_capture_generation)
{
    enum rdx_storage_result result = RDX_STORAGE_RESULT_BAD_STATE;

    if (!service || !session_id || !old_capture_generation
        || !new_capture_generation
        || old_capture_generation == new_capture_generation) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    service_lock(service);
    if (service_ids_match(service, session_id, old_capture_generation)
        && !service->seal_requested && !service->capture_detached) {
        /* Queued slots already own copied payload bytes and do not use the
         * capture token while the storage worker drains them.  Updating the
         * admission token therefore preserves pre-pause frames and rejects
         * only stale callbacks from the closed capture instance. */
        service->capture_generation = new_capture_generation;
        result = RDX_STORAGE_RESULT_READY;
    }
    service_unlock(service);
    return result;
}

enum rdx_storage_result rdx_local_storage_service_cancel_start(
    struct rdx_local_storage_service *service, enum rdx_record_cause cause)
{
    if (!service || !service->active || service->capture_generation) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (cause == RDX_RECORD_CAUSE_NONE) {
        cause = RDX_RECORD_CAUSE_START_FAILED;
    }
    service_lock(service);
    {
        enum rdx_storage_result result = service_fail(
            service, RDX_STORAGE_RESULT_IO_ERROR, cause);

        service_unlock(service);
        return result;
    }
}

enum rdx_storage_result rdx_local_storage_service_consume(
    struct rdx_local_storage_service *service,
    const struct rdx_record_frame *frame)
{
    struct rdx_local_storage_frame_slot *slot;

    if (!service || !frame || !service_ids_match(
                                service, frame->session_id,
                                frame->capture_generation)
        || service->seal_requested || service->capture_detached) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service_lock(service);
    /* The worker may have failed the artifact after the first check. */
    if (!service_ids_match(service, frame->session_id,
                           frame->capture_generation)
        || service->seal_requested || service->capture_detached) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (!frame->payload || !frame->payload_len
        || frame->payload_len > RDX_RECORD_CANDIDATE_MAX_SIZE
        || frame->payload_len != service->artifact.spec.payload_size) {
        service->artifact.rejected_frame_count++;
        rdx_artifact_fail(&service->artifact,
                          RDX_RECORD_CAUSE_FORMAT_MISMATCH);
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    if (rdx_artifact_note_produced(&service->artifact)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (service->queue_count >= RDX_LOCAL_STORAGE_QUEUE_DEPTH) {
        service->artifact.rejected_frame_count++;
        service->artifact.dropped_frame_count++;
        rdx_artifact_fail(&service->artifact, RDX_RECORD_CAUSE_QUEUE_FULL);
        service_unlock(service);
        return RDX_STORAGE_RESULT_QUEUE_FULL;
    }
    slot = &service->queue[service->queue_tail];
    slot->frame = *frame;
    memcpy(slot->payload, frame->payload, frame->payload_len);
    slot->frame.payload = slot->payload;
    if (rdx_artifact_note_destination_accepted(&service->artifact)
        || rdx_artifact_accept_frame(&service->artifact, frame->payload_len)) {
        service->artifact.rejected_frame_count++;
        rdx_artifact_fail(&service->artifact, RDX_RECORD_CAUSE_STORAGE_ERROR);
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service->queue_tail = (service->queue_tail + 1)
                          % RDX_LOCAL_STORAGE_QUEUE_DEPTH;
    service->queue_count++;
    service_unlock(service);
    service_wake(service);
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result service_pump_locked(
    struct rdx_local_storage_service *service, uint8_t budget)
{
    uint8_t pumped = 0;

    if (!service || !service->active) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    while (service->queue_count && (!budget || pumped < budget)) {
        struct rdx_local_storage_frame_slot *slot =
            &service->queue[service->queue_head];
        uint32_t written = 0;
        enum rdx_storage_result result;

        if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
            result = service->native_ops->append(
                service->artifact_handle, slot->payload,
                slot->frame.payload_len, &written);
        } else {
            result = service->legacy_ops->raw_write(
                &service->file_id, slot->payload,
                slot->frame.payload_len, &written);
        }
        if (result != RDX_STORAGE_RESULT_READY
            || written != slot->frame.payload_len
            || rdx_artifact_persist_frame(
                   &service->artifact, slot->frame.payload_len,
                   service_crc32_update(service->artifact.audio_crc,
                                        service->artifact.persisted_frame_count,
                                        slot->payload,
                                        slot->frame.payload_len))) {
            return service_fail(service,
                                result == RDX_STORAGE_RESULT_READY
                                    ? RDX_STORAGE_RESULT_IO_ERROR : result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->queue_head = (service->queue_head + 1)
                              % RDX_LOCAL_STORAGE_QUEUE_DEPTH;
        service->queue_count--;
        pumped++;
    }
    if (pumped && service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE
        && service->native_ops->store_marker
        && ((uint64_t)(service->artifact.persisted_frame_count
                       - service->checkpoint_persisted_frame_count)
            * service->artifact.spec.frame_ms
            >= RDX_LOCAL_STORAGE_CHECKPOINT_INTERVAL_MS)) {
        struct rdx_storage_marker marker;

        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_WRITING,
                            &marker);
        if (service->native_ops->store_marker(&marker)
            != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, RDX_STORAGE_RESULT_IO_ERROR,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->checkpoint_persisted_frame_count =
            service->artifact.persisted_frame_count;
    }
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_pump(
    struct rdx_local_storage_service *service, uint8_t budget)
{
    enum rdx_storage_result result;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    service_lock(service);
    result = service_pump_locked(service, budget);
    service_unlock(service);
    return result;
}

enum rdx_storage_result rdx_local_storage_service_seal(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    struct rdx_storage_marker marker;
    struct rdx_storage_index_snapshot snapshot;
    enum rdx_storage_result result;
    uint64_t file_size = 0;
    uint32_t audio_crc = 0;

    if (!service_ids_match(service, session_id, capture_generation)
        || service->seal_requested) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service_lock(service);
    if (!service_ids_match(service, session_id, capture_generation)
        || service->seal_requested) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    service->seal_requested = 1;
    service_unlock(service);
    service_wake(service);
#if CONFIG_CPU_BR56
    result = RDX_STORAGE_RESULT_READY;
    {
        uint16_t ticks = 0;
        uint8_t queue_count;

        for (;;) {
            service_lock(service);
            queue_count = service->queue_count;
            service_unlock(service);
            if (!queue_count || ticks++ >= RDX_LOCAL_STORAGE_SEAL_WAIT_TICKS) {
                break;
            }
            os_time_dly(1);
        }
        service_lock(service);
        queue_count = service->queue_count;
        service_unlock(service);
        if (queue_count) {
            result = RDX_STORAGE_RESULT_TIMEOUT;
        }
    }
#else
    result = rdx_local_storage_service_pump(service, 0);
#endif
    if (result != RDX_STORAGE_RESULT_READY) {
        return result;
    }
    if (rdx_artifact_queue_seal(&service->artifact)
        || rdx_artifact_begin_commit(&service->artifact)) {
        return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                            RDX_RECORD_CAUSE_STORAGE_ERROR);
    }
    if (service->backend == RDX_LOCAL_STORAGE_BACKEND_NATIVE) {
        result = service->native_ops->close(service->artifact_handle,
                                            &file_size, &audio_crc);
        printf("[RDX][LOCAL] seal stage=close result=%u"
               " bytes_hi=%u bytes_lo=%u crc=%08X\n",
               result, (unsigned int)(file_size >> 32),
               (unsigned int)file_size, (unsigned int)audio_crc);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->artifact_handle = NULL;
        if (file_size != service->artifact.audio_bytes
            || (service->artifact.persisted_frame_count
                && audio_crc != service->artifact.audio_crc)) {
            printf("[RDX][LOCAL] seal stage=verify result=%u"
                   " expected_bytes_hi=%u expected_bytes_lo=%u"
                   " expected_crc=%08X\n",
                   RDX_STORAGE_RESULT_CORRUPT,
                   (unsigned int)(service->artifact.audio_bytes >> 32),
                   (unsigned int)service->artifact.audio_bytes,
                   (unsigned int)service->artifact.audio_crc);
            return service_fail(service, RDX_STORAGE_RESULT_CORRUPT,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->artifact.audio_crc = audio_crc;
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_SEALED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        printf("[RDX][LOCAL] seal stage=sealed_marker result=%u\n",
               result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        result = service->native_ops->publish(
            service->artifact.spec.temp_name, service->artifact.spec.final_name);
        printf("[RDX][LOCAL] seal stage=publish result=%u\n", result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_PUBLISHED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        printf("[RDX][LOCAL] seal stage=published_marker result=%u\n",
               result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.entries = service->scratch.index_entries;
        snapshot.capacity = sizeof(service->scratch.index_entries)
                            / sizeof(service->scratch.index_entries[0]);
        result = service->native_ops->load_index(&snapshot);
        printf("[RDX][LOCAL] seal stage=load_index result=%u"
               " generation=%u count=%u high_water=%u\n",
               result, (unsigned int)snapshot.generation,
               (unsigned int)snapshot.count,
               (unsigned int)snapshot.serial_high_water);
        if (result != RDX_STORAGE_RESULT_READY
            || snapshot.count >= snapshot.capacity) {
            return service_fail(service,
                                result == RDX_STORAGE_RESULT_READY
                                    ? RDX_STORAGE_RESULT_NO_SPACE : result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        snapshot.entries[snapshot.count].file.file_id = service->file_id;
        snapshot.entries[snapshot.count].file.state =
            RDX_ARTIFACT_STATE_COMMITTED;
        snapshot.entries[snapshot.count].file.file_size = file_size;
        snapshot.entries[snapshot.count].file.frame_count =
            service->artifact.persisted_frame_count;
        snapshot.entries[snapshot.count].file.audio_crc = audio_crc;
        snapshot.count++;
        result = service->native_ops->commit_index(&snapshot);
        printf("[RDX][LOCAL] seal stage=commit_index result=%u"
               " generation=%u count=%u high_water=%u\n",
               result, (unsigned int)(snapshot.generation + 1),
               (unsigned int)snapshot.count,
               (unsigned int)snapshot.serial_high_water);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service_fill_marker(service, RDX_STORAGE_MARKER_STATE_INDEXED,
                            &marker);
        result = service->native_ops->store_marker(&marker);
        printf("[RDX][LOCAL] seal stage=indexed_marker result=%u\n",
               result);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        if (rdx_artifact_commit(&service->artifact)) {
            return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->active = 0;
        service->capture_detached = 1;
        service_emit(service, RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_COMMITTED,
                     RDX_STORAGE_RESULT_READY, RDX_RECORD_CAUSE_NONE);
    } else {
        result = service->legacy_ops->flush_commit(&service->file_id);
        if (result != RDX_STORAGE_RESULT_READY) {
            return service_fail(service, result,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        if (rdx_artifact_commit(&service->artifact)) {
            return service_fail(service, RDX_STORAGE_RESULT_BAD_STATE,
                                RDX_RECORD_CAUSE_STORAGE_ERROR);
        }
        service->active = 0;
        service->capture_detached = 1;
        service_emit(service,
                     RDX_LOCAL_STORAGE_COMPLETION_LEGACY_FILE_COMMITTED,
                     RDX_STORAGE_RESULT_READY, RDX_RECORD_CAUSE_NONE);
    }
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_abort(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation, enum rdx_record_cause cause)
{
    if (!service_ids_match(service, session_id, capture_generation)) {
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
    if (cause == RDX_RECORD_CAUSE_NONE) {
        cause = RDX_RECORD_CAUSE_STORAGE_ERROR;
    }
    service_lock(service);
    {
        enum rdx_storage_result result = service_fail(
            service, RDX_STORAGE_RESULT_IO_ERROR, cause);

        service_unlock(service);
        return result;
    }
}

void rdx_local_storage_service_detach_capture(
    struct rdx_local_storage_service *service, uint32_t session_id,
    uint32_t capture_generation)
{
    service_lock(service);
    if (service_ids_match(service, session_id, capture_generation)) {
        service->capture_detached = 1;
    }
    service_unlock(service);
}

enum rdx_storage_readiness rdx_local_storage_service_readiness(
    const struct rdx_local_storage_service *service)
{
    return service ? service->readiness : RDX_STORAGE_READINESS_UNINITIALIZED;
}

const struct rdx_artifact *rdx_local_storage_service_artifact(
    const struct rdx_local_storage_service *service)
{
    return service ? &service->artifact : NULL;
}

enum rdx_storage_result rdx_local_storage_service_get_capacity(
    struct rdx_local_storage_service *service, uint64_t *total_bytes,
    uint64_t *free_bytes)
{
    if (!service || !service->media_ops || !service->media_ops->get_capacity) {
        return RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
    {
        enum rdx_storage_result result = service->media_ops->get_capacity(
            total_bytes, free_bytes);

        service_unlock(service);
        return result;
    }
}

enum rdx_storage_result rdx_local_storage_service_enumerate(
    struct rdx_local_storage_service *service,
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count)
{
    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->enumerate) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
    {
        enum rdx_storage_result result = service->media_ops->enumerate(
            items, capacity, count);

        service_unlock(service);
        return result;
    }
}

enum rdx_storage_result rdx_local_storage_service_delete(
    struct rdx_local_storage_service *service,
    const struct rdx_storage_file_id *file_id)
{
    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->delete) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
    {
        enum rdx_storage_result result = service->media_ops->delete(file_id);

        service_unlock(service);
        return result;
    }
}

enum rdx_storage_result rdx_local_storage_service_format(
    struct rdx_local_storage_service *service)
{
    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->format) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
    {
        enum rdx_storage_result result = service_format_and_recover(
            service, NULL, NULL);

        service_unlock(service);
        return result;
    }
}

enum rdx_storage_result rdx_local_storage_service_submit_operation(
    struct rdx_local_storage_service *service,
    const struct rdx_local_storage_operation_request *request,
    rdx_local_storage_operation_callback_t callback, void *priv)
{
    if (!service || !request || !callback || !request->token
        || request->type <= RDX_LOCAL_STORAGE_OPERATION_NONE
        || request->type > RDX_LOCAL_STORAGE_OPERATION_FORMAT) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
    switch (request->type) {
    case RDX_LOCAL_STORAGE_OPERATION_CAPACITY:
    case RDX_LOCAL_STORAGE_OPERATION_FORMAT:
        break;
    case RDX_LOCAL_STORAGE_OPERATION_ENUMERATE:
        if (!request->data.enumerate.items
            || !request->data.enumerate.capacity) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_READ_BY_SERIAL:
        if (!request->data.read.serial_number
            || !request->data.read.buffer || !request->data.read.length) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY:
        if (!request->data.delete_file.serial_number
            || !request->data.delete_file.filename[0]
            || !memchr(request->data.delete_file.filename, '\0',
                       sizeof(request->data.delete_file.filename))) {
            return RDX_STORAGE_RESULT_BAD_ARGUMENT;
        }
        break;
    default:
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }

    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
#if CONFIG_CPU_BR56
    if (!service->worker_created || service->worker_stop) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BAD_STATE;
    }
#endif
    service->operation = *request;
    service->operation_callback = callback;
    service->operation_priv = priv;
    service->operation_pending = 1;
    service_unlock(service);
#if CONFIG_CPU_BR56
    service_wake(service);
#else
    service_process_operation(service);
#endif
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_quiesce(
    struct rdx_local_storage_service *service, uint16_t timeout_ms)
{
    enum rdx_storage_result result;

    if (!service || service->active || service_operation_busy(service)
        || !service->media_ops
        || !service->media_ops->quiesce) {
        return service && (service->active || service_operation_busy(service))
                   ? RDX_STORAGE_RESULT_BUSY
                                          : RDX_STORAGE_RESULT_UNSUPPORTED;
    }
    service_lock(service);
    if (service->active || service_operation_busy(service)) {
        service_unlock(service);
        return RDX_STORAGE_RESULT_BUSY;
    }
    service->readiness = RDX_STORAGE_READINESS_QUIESCING;
    result = service->media_ops->quiesce(timeout_ms);
    if (result != RDX_STORAGE_RESULT_READY) {
        service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        service_unlock(service);
        return result;
    }
    service->readiness = RDX_STORAGE_READINESS_HOST_OWNED;
    service_unlock(service);
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_shutdown(
    struct rdx_local_storage_service *service, uint16_t timeout_ms)
{
    enum rdx_storage_result result = RDX_STORAGE_RESULT_READY;

    if (!service) {
        return RDX_STORAGE_RESULT_BAD_ARGUMENT;
    }
#if CONFIG_CPU_BR56
    service->worker_stop = 1;
    service_wake(service);
    if (service->worker_created) {
        task_kill(RDX_LOCAL_STORAGE_TASK_NAME);
        service->worker_created = 0;
    }
#endif
    service_lock(service);
    if (service->active) {
        result = service_fail(service, RDX_STORAGE_RESULT_IO_ERROR,
                              RDX_RECORD_CAUSE_SHUTDOWN);
    }
    service_unlock(service);
    if (service->media_ops && service->media_ops->quiesce) {
        enum rdx_storage_result quiesce_result;

        quiesce_result = service->media_ops->quiesce(timeout_ms);
        if (result == RDX_STORAGE_RESULT_READY) {
            result = quiesce_result;
        }
        if (quiesce_result == RDX_STORAGE_RESULT_READY) {
            service->readiness = RDX_STORAGE_READINESS_HOST_OWNED;
        } else {
            service->readiness = RDX_STORAGE_READINESS_UNAVAILABLE;
        }
    }
    return result;
}
