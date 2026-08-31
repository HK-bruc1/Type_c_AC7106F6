#include <assert.h>
#include <string.h>

#include "rdx/rdx_record_coordinator.h"
#include "rdx/rdx_record_local_destination.h"

struct fake_media {
    int ready_calls;
    enum rdx_storage_result result;
};

struct fake_native {
    int append_calls;
    int publish_calls;
    int marker_calls;
    int index_commit_calls;
    int quarantine_calls;
    uint32_t bytes;
    uint32_t audio_crc;
    uint8_t audio_initialized;
    uint32_t close_size_delta;
    uint32_t marker_states[8];
    int fail_marker_state;
    int fail_append;
    int recover_calls;
    enum rdx_storage_result recover_result;
    uint32_t index_count;
};

struct fake_completion {
    int calls;
    struct rdx_local_storage_completion last;
};

static struct fake_media *g_media;
static struct fake_native *g_native;

static uint32_t fake_crc32_update(uint32_t previous, uint32_t frame_count,
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

static enum rdx_storage_result fake_ready(void)
{
    g_media->ready_calls++;
    return g_media->result;
}

static enum rdx_storage_result fake_recover(void)
{
    g_native->recover_calls++;
    return g_native->recover_result;
}

static enum rdx_storage_result fake_open_temp(
    const struct rdx_storage_artifact_spec *spec, void **handle)
{
    (void)spec;
    *handle = g_native;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_append(void *handle, const uint8_t *data,
                                           uint32_t len, uint32_t *written)
{
    struct fake_native *native = handle;
    (void)data;
    native->append_calls++;
    if (native->fail_append) {
        *written = 0;
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    native->bytes += len;
    native->audio_crc = fake_crc32_update(native->audio_crc,
                                          native->audio_initialized,
                                          data, len);
    native->audio_initialized = 1;
    *written = len;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_close(void *handle, uint64_t *size,
                                          uint32_t *crc)
{
    struct fake_native *native = handle;
    *size = native->bytes + native->close_size_delta;
    /* The fake models the Native backend's CRC32/IEEE close read-back. */
    *crc = native->audio_initialized ? native->audio_crc : 0;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_publish(const char *temp_name,
                                            const char *final_name)
{
    (void)temp_name;
    (void)final_name;
    g_native->publish_calls++;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_marker(
    const struct rdx_storage_marker *marker)
{
    assert(marker->artifact_id.value != 0);
    assert(g_native->marker_calls
           < (int)(sizeof(g_native->marker_states)
                   / sizeof(g_native->marker_states[0])));
    g_native->marker_states[g_native->marker_calls] = marker->state;
    g_native->marker_calls++;
    if (g_native->fail_marker_state
        && g_native->fail_marker_state == (int)marker->state) {
        return RDX_STORAGE_RESULT_IO_ERROR;
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_load_index(
    struct rdx_storage_index_snapshot *snapshot)
{
    snapshot->count = g_native->index_count;
    if (snapshot->count) {
        assert(snapshot->capacity >= 1);
        memset(&snapshot->entries[0], 0, sizeof(snapshot->entries[0]));
        snapshot->entries[0].file.file_id.artifact_id.value = 0x321;
        snapshot->entries[0].file.file_id.serial_number = 2;
        strcpy(snapshot->entries[0].file.file_id.name, "RDXREC/R0002.OPU");
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_commit_index(
    const struct rdx_storage_index_snapshot *snapshot)
{
    assert(snapshot->count <= 1);
    g_native->index_commit_calls++;
    g_native->index_count = snapshot->count;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_quarantine(
    const struct rdx_artifact_id *artifact_id, const char *reason)
{
    assert(artifact_id->value != 0);
    assert(reason != NULL);
    g_native->quarantine_calls++;
    return RDX_STORAGE_RESULT_READY;
}

static void completion(void *priv,
                       const struct rdx_local_storage_completion *event)
{
    struct fake_completion *fake = priv;
    fake->calls++;
    fake->last = *event;
}

static struct rdx_storage_artifact_spec test_spec(void)
{
    struct rdx_storage_artifact_spec spec;

    memset(&spec, 0, sizeof(spec));
    spec.artifact_id.value = 0x321;
    spec.session_id = 8;
    spec.serial_number = 2;
    spec.format_id = RDX_RECORD_FORMAT_MEETING_V1;
    spec.sample_rate = RDX_RECORD_MEETING_V1_SAMPLE_RATE;
    spec.channels = RDX_RECORD_MEETING_V1_CHANNELS;
    spec.frame_ms = RDX_RECORD_MEETING_V1_FRAME_MS;
    spec.payload_size = 4;
    strcpy(spec.temp_name, "RDXTMP/T0002.OPU");
    strcpy(spec.final_name, "RDXREC/R0002.OPU");
    return spec;
}

static struct rdx_record_frame test_frame(uint32_t sequence,
                                          const uint8_t *payload)
{
    struct rdx_record_frame frame;

    memset(&frame, 0, sizeof(frame));
    frame.session_id = 8;
    frame.capture_generation = 4;
    frame.session_frame_seq = sequence;
    frame.payload = payload;
    frame.payload_len = 4;
    return frame;
}

static void test_native_commit(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_local_destination destination;
    struct rdx_storage_artifact_spec spec = test_spec();
    struct rdx_record_frame frame;
    const uint8_t payload[4] = { 1, 2, 3, 4 };

    media_ops.ensure_ready = fake_ready;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, completion, &events) == 0);
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_READY);
    assert(rdx_record_local_destination_init(&destination, &service) == 0);
    assert(rdx_record_local_destination_ops()->attach(
               &destination, 8, 4) == 0);
    frame = test_frame(1, payload);
    assert(rdx_record_local_destination_ops()->consume(
               &destination, &frame) == 0);
    assert(rdx_local_storage_service_pump(&service, 0)
           == RDX_STORAGE_RESULT_READY);
    assert(rdx_record_local_destination_ops()->seal(&destination, 8, 4) == 0);
    assert(events.calls == 1);
    assert(events.last.type == RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_COMMITTED);
    assert(events.last.frame_count == 1);
    assert(events.last.file_size == 4);
    assert(events.last.audio_crc == 0xb63cfbcdUL);
    assert(events.last.audio_crc == native.audio_crc);
    assert(service.artifact.audio_crc == native.audio_crc);
    assert(media.ready_calls == 1);
    assert(native.append_calls == 1 && native.publish_calls == 1);
    assert(native.index_commit_calls == 1 && native.marker_calls == 6);
    assert(native.marker_states[0] == 0);
    assert(native.marker_states[1] == 1);
    assert(native.marker_states[2] == 1);
    assert(native.marker_states[3] == 2);
    assert(native.marker_states[4] == 3);
    assert(native.marker_states[5] == 4);
}

static void test_required_queue_failure(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_local_destination destination;
    struct rdx_storage_artifact_spec spec = test_spec();
    uint8_t payload[4] = { 0 };
    uint32_t i;

    media_ops.ensure_ready = fake_ready;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, completion, &events) == 0);
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_READY);
    assert(rdx_record_local_destination_init(&destination, &service) == 0);
    assert(rdx_record_local_destination_ops()->attach(
               &destination, 8, 4) == 0);
    for (i = 0; i < RDX_LOCAL_STORAGE_QUEUE_DEPTH; i++) {
        struct rdx_record_frame frame = test_frame(i + 1, payload);
        assert(rdx_record_local_destination_ops()->consume(
                   &destination, &frame) == 0);
    }
    {
        struct rdx_record_frame frame = test_frame(99, payload);
        assert(rdx_record_local_destination_ops()->consume(
                   &destination, &frame) != 0);
    }
    assert(service.artifact.first_error_cause == RDX_RECORD_CAUSE_QUEUE_FULL);
    assert(events.calls == 0);
    assert(rdx_record_local_destination_ops()->abort(
               &destination, 8, 4, RDX_RECORD_CAUSE_QUEUE_FULL) != 0);
    assert(events.calls == 1);
    assert(events.last.type == RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_FAILED);
    assert(service.artifact.state == RDX_ARTIFACT_STATE_QUARANTINED);
    assert(native.quarantine_calls == 1);
}

static void test_short_write_is_failed(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_local_destination destination;
    struct rdx_storage_artifact_spec spec = test_spec();
    struct rdx_record_frame frame;
    const uint8_t payload[4] = { 9, 8, 7, 6 };

    media_ops.ensure_ready = fake_ready;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    native.fail_append = 1;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, completion, &events) == 0);
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_READY);
    assert(rdx_record_local_destination_init(&destination, &service) == 0);
    assert(rdx_record_local_destination_ops()->attach(
               &destination, 8, 4) == 0);
    frame = test_frame(1, payload);
    assert(rdx_record_local_destination_ops()->consume(
               &destination, &frame) == 0);
    assert(rdx_local_storage_service_pump(&service, 0)
           == RDX_STORAGE_RESULT_IO_ERROR);
    assert(events.calls == 1);
    assert(events.last.type == RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_FAILED);
    assert(service.artifact.state == RDX_ARTIFACT_STATE_QUARANTINED);
}

static void test_close_size_mismatch_is_not_published(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_local_destination destination;
    struct rdx_storage_artifact_spec spec = test_spec();
    struct rdx_record_frame frame;
    const uint8_t payload[4] = { 4, 3, 2, 1 };

    media_ops.ensure_ready = fake_ready;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    native.close_size_delta = 1;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, completion, &events) == 0);
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_READY);
    assert(rdx_record_local_destination_init(&destination, &service) == 0);
    assert(rdx_record_local_destination_ops()->attach(
               &destination, 8, 4) == 0);
    frame = test_frame(1, payload);
    assert(rdx_record_local_destination_ops()->consume(
               &destination, &frame) == 0);
    assert(rdx_record_local_destination_ops()->seal(&destination, 8, 4) != 0);
    assert(events.calls == 1);
    assert(events.last.result == RDX_STORAGE_RESULT_CORRUPT);
    assert(native.publish_calls == 0);
    assert(native.index_commit_calls == 0);
    assert(service.artifact.state == RDX_ARTIFACT_STATE_QUARANTINED);
}

static void test_indexed_marker_failure_is_not_committed(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_local_destination destination;
    struct rdx_storage_artifact_spec spec = test_spec();
    struct rdx_record_frame frame;
    const uint8_t payload[4] = { 5, 6, 7, 8 };

    media_ops.ensure_ready = fake_ready;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    native.fail_marker_state = 4;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, completion, &events) == 0);
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_READY);
    assert(rdx_record_local_destination_init(&destination, &service) == 0);
    assert(rdx_record_local_destination_ops()->attach(
               &destination, 8, 4) == 0);
    frame = test_frame(1, payload);
    assert(rdx_record_local_destination_ops()->consume(
               &destination, &frame) == 0);
    assert(rdx_record_local_destination_ops()->seal(&destination, 8, 4) != 0);
    assert(events.calls == 1);
    assert(events.last.type == RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_FAILED);
    assert(service.artifact.state == RDX_ARTIFACT_STATE_FAILED
           || service.artifact.state == RDX_ARTIFACT_STATE_QUARANTINED);
    assert(native.publish_calls == 1);
    assert(native.index_commit_calls == 2);
    assert(native.index_count == 0);
}

static void test_coordinator_binds_runtime_session(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_coordinator coordinator;
    struct rdx_storage_artifact_spec spec = test_spec();
    struct rdx_record_request request;

    media_ops.ensure_ready = fake_ready;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    spec.session_id = 0;
    memset(&request, 0, sizeof(request));
    request.request_id = 1;
    request.type = RDX_RECORD_REQUEST_START;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, completion, &events) == 0);
    assert(rdx_record_coordinator_init(&coordinator, &service, NULL, NULL)
           == 0);
    assert(rdx_record_coordinator_prepare_start(
               &coordinator, &spec, &request) == RDX_STORAGE_RESULT_READY);
    assert(rdx_record_coordinator_cancel_start(
               &coordinator, RDX_RECORD_CAUSE_START_FAILED)
           != RDX_STORAGE_RESULT_READY);
    assert(!service.active && events.calls == 1);
    assert(rdx_record_coordinator_prepare_start(
               &coordinator, &spec, &request) == RDX_STORAGE_RESULT_READY);
    assert(request.destination_ops != NULL);
    assert(request.destination_ops->requirement
           == RDX_RECORD_DESTINATION_REQUIRED);
    assert(request.destination_ops->attach(
               request.destination_priv, 42, 9) == 0);
    assert(service.session_id == 42);
    assert(service.artifact.spec.session_id == 42);
    assert(native.marker_calls == 6);
    assert(request.destination_ops->abort(
               request.destination_priv, 42, 9,
               RDX_RECORD_CAUSE_START_FAILED) != 0);
    assert(events.calls == 2);
    assert(events.last.type == RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_FAILED);
}

static void test_recovery_failure_gates_start(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_storage_artifact_spec spec = test_spec();

    media_ops.ensure_ready = fake_ready;
    native.recover_result = RDX_STORAGE_RESULT_CORRUPT;
    native_ops.recover = fake_recover;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, NULL, NULL) == 0);
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_BAD_STATE);
    assert(rdx_local_storage_service_recover(&service)
           == RDX_STORAGE_RESULT_CORRUPT);
    assert(native.recover_calls == 1);
    assert(rdx_local_storage_service_readiness(&service)
           == RDX_STORAGE_READINESS_RECOVERY_REQUIRED);
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_BAD_STATE);
}

static void test_media_outage_invalidates_recovery(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_storage_artifact_spec spec = test_spec();

    media_ops.ensure_ready = fake_ready;
    native.recover_result = RDX_STORAGE_RESULT_READY;
    native_ops.recover = fake_recover;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_media = &media;
    g_native = &native;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, NULL, NULL) == 0);
    assert(rdx_local_storage_service_recover(&service)
           == RDX_STORAGE_RESULT_READY);
    assert(native.recover_calls == 1);

    media.result = RDX_STORAGE_RESULT_NOT_READY;
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_NOT_READY);
    media.result = RDX_STORAGE_RESULT_READY;
    assert(rdx_local_storage_service_start(&service, &spec)
           == RDX_STORAGE_RESULT_BAD_STATE);
    assert(rdx_local_storage_service_recover(&service)
           == RDX_STORAGE_RESULT_READY);
    assert(native.recover_calls == 2);
}

int main(void)
{
    test_native_commit();
    test_required_queue_failure();
    test_short_write_is_failed();
    test_close_size_mismatch_is_not_published();
    test_indexed_marker_failure_is_not_committed();
    test_coordinator_binds_runtime_session();
    test_recovery_failure_gates_start();
    test_media_outage_invalidates_recovery();
    return 0;
}
