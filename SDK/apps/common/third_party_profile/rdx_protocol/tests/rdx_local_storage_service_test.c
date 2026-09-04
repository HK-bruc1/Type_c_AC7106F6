#include <assert.h>
#include <string.h>

#include "rdx/rdx_local_storage_service.h"
#include "rdx/rdx_record_coordinator.h"
#include "rdx/rdx_record_local_destination.h"

struct fake_media {
    int ready_calls;
    enum rdx_storage_result result;
};

struct fake_native {
    int append_calls;
    uint32_t append_lengths[64];
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
    uint32_t index_generation;
    uint32_t serial_high_water;
};

struct fake_completion {
    int calls;
    struct rdx_local_storage_completion last;
};

struct fake_operation_media {
    struct rdx_storage_file_info files[2];
    uint8_t data[8];
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint32_t count;
    int capacity_calls;
    int enumerate_calls;
    int read_calls;
    int delete_calls;
    int format_calls;
    enum rdx_storage_result capacity_result;
    enum rdx_storage_result enumerate_result;
    enum rdx_storage_result read_result;
    enum rdx_storage_result delete_result;
    enum rdx_storage_result format_result;
};

struct fake_operation_completion {
    int calls;
    struct rdx_local_storage_operation_completion last;
};

static struct fake_media *g_media;
static struct fake_native *g_native;
static struct fake_operation_media *g_operation_media;

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

static enum rdx_storage_result operation_ready(void)
{
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result operation_capacity(
    uint64_t *total_bytes, uint64_t *free_bytes)
{
    g_operation_media->capacity_calls++;
    if (g_operation_media->capacity_result != RDX_STORAGE_RESULT_READY) {
        return g_operation_media->capacity_result;
    }
    *total_bytes = g_operation_media->total_bytes;
    *free_bytes = g_operation_media->free_bytes;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result operation_enumerate(
    struct rdx_storage_file_info *items, uint32_t capacity, uint32_t *count)
{
    g_operation_media->enumerate_calls++;
    *count = g_operation_media->count;
    if (g_operation_media->enumerate_result != RDX_STORAGE_RESULT_READY) {
        return g_operation_media->enumerate_result;
    }
    if (capacity < g_operation_media->count) {
        return RDX_STORAGE_RESULT_NO_SPACE;
    }
    if (g_operation_media->count) {
        memcpy(items, g_operation_media->files,
               g_operation_media->count * sizeof(items[0]));
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result operation_read(
    const struct rdx_storage_file_id *file_id, uint64_t offset,
    uint8_t *buffer, uint32_t len, uint32_t *read_len)
{
    uint32_t available;

    g_operation_media->read_calls++;
    if (g_operation_media->read_result != RDX_STORAGE_RESULT_READY) {
        *read_len = 0;
        return g_operation_media->read_result;
    }
    assert(file_id->artifact_id.value
           == g_operation_media->files[0].file_id.artifact_id.value);
    assert(offset < g_operation_media->files[0].file_size);
    available = (uint32_t)(
        g_operation_media->files[0].file_size - offset);
    if (len > available) {
        len = available;
    }
    memcpy(buffer, g_operation_media->data + (uint32_t)offset, len);
    *read_len = len;
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result operation_delete(
    const struct rdx_storage_file_id *file_id)
{
    g_operation_media->delete_calls++;
    assert(file_id->artifact_id.value
           == g_operation_media->files[0].file_id.artifact_id.value);
    if (g_operation_media->delete_result == RDX_STORAGE_RESULT_READY) {
        g_operation_media->count = 0;
    }
    return g_operation_media->delete_result;
}

static enum rdx_storage_result operation_format(void)
{
    g_operation_media->format_calls++;
    if (g_operation_media->format_result == RDX_STORAGE_RESULT_READY) {
        g_operation_media->count = 0;
    }
    return g_operation_media->format_result;
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
    assert(native->append_calls
           < (int)(sizeof(native->append_lengths)
                   / sizeof(native->append_lengths[0])));
    native->append_lengths[native->append_calls] = len;
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
    snapshot->schema_version = 2;
    snapshot->generation = g_native->index_generation;
    snapshot->serial_high_water = g_native->serial_high_water
                                      ? g_native->serial_high_water : 2;
    snapshot->count = g_native->index_count;
    if (snapshot->count) {
        assert(snapshot->capacity >= 1);
        memset(&snapshot->entries[0], 0, sizeof(snapshot->entries[0]));
        snapshot->entries[0].file.file_id.artifact_id.value = 0x321;
        snapshot->entries[0].file.file_id.serial_number = 2;
        strcpy(snapshot->entries[0].file.file_id.name, "R0002.OPU");
    }
    return RDX_STORAGE_RESULT_READY;
}

static enum rdx_storage_result fake_commit_index(
    const struct rdx_storage_index_snapshot *snapshot)
{
    assert(snapshot->count <= 1);
    g_native->index_commit_calls++;
    g_native->index_generation++;
    g_native->index_count = snapshot->count;
    g_native->serial_high_water = snapshot->serial_high_water;
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

static void operation_completion(
    void *priv,
    const struct rdx_local_storage_operation_completion *event)
{
    struct fake_operation_completion *fake = priv;

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
    strcpy(spec.final_name, "R0002.OPU");
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
    assert(events.last.produced_frame_count == 1);
    assert(events.last.destination_accepted_count == 1);
    assert(events.last.local_accepted_count == 1);
    assert(events.last.persisted_frame_count == 1);
    assert(events.last.file_size == 4);
    assert(events.last.audio_crc == 0xb63cfbcdUL);
    assert(events.last.audio_crc == native.audio_crc);
    assert(service.artifact.audio_crc == native.audio_crc);
    assert(media.ready_calls == 1);
    assert(native.append_calls == 1 && native.publish_calls == 1);
    assert(native.index_commit_calls == 1 && native.marker_calls == 5);
    assert(native.marker_states[0] == 0);
    assert(native.marker_states[1] == 1);
    assert(native.marker_states[2] == 2);
    assert(native.marker_states[3] == 3);
    assert(native.marker_states[4] == 4);
}

static void test_native_checkpoint_cadence(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_local_destination destination;
    struct rdx_storage_artifact_spec spec = test_spec();
    const uint8_t payload[4] = { 1, 2, 3, 4 };
    uint32_t checkpoint_frames =
        RDX_LOCAL_STORAGE_CHECKPOINT_INTERVAL_MS / spec.frame_ms;
    uint32_t i;

    assert(checkpoint_frames > 1);
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
    for (i = 0; i < checkpoint_frames - 1; i++) {
        struct rdx_record_frame frame = test_frame(i, payload);

        assert(rdx_record_local_destination_ops()->consume(
                   &destination, &frame) == 0);
        assert(rdx_local_storage_service_pump(&service, 0)
               == RDX_STORAGE_RESULT_READY);
    }
    assert(native.marker_calls == 2);
    {
        struct rdx_record_frame frame = test_frame(i, payload);

        assert(rdx_record_local_destination_ops()->consume(
                   &destination, &frame) == 0);
        assert(rdx_local_storage_service_pump(&service, 0)
               == RDX_STORAGE_RESULT_READY);
    }
#if RDX_CFG_LOCAL_STORAGE_PERIODIC_CHECKPOINT_ENABLE
    assert(native.marker_calls == 3);
    assert(native.marker_states[2] == RDX_STORAGE_MARKER_STATE_WRITING);
#else
    assert(native.marker_calls == 2);
#endif
    assert(rdx_record_local_destination_ops()->abort(
               &destination, 8, 4, RDX_RECORD_CAUSE_USER_REQUEST) != 0);
}

static void test_native_write_batch_and_seal_tail(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_local_destination destination;
    struct rdx_storage_artifact_spec spec = test_spec();
    uint8_t payload[4] = { 1, 2, 3, 4 };
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

    for (i = 0; i < 7; i++) {
        struct rdx_record_frame frame = test_frame(i, payload);

        payload[0] = (uint8_t)i;
        assert(rdx_record_local_destination_ops()->consume(
                   &destination, &frame) == 0);
    }
    assert(rdx_local_storage_service_pump(
               &service, RDX_CFG_LOCAL_STORAGE_WRITE_BATCH_FRAMES)
           == RDX_STORAGE_RESULT_READY);
    assert(native.append_calls == 1);
    assert(native.append_lengths[0] == 24);
    assert(service.queue_count == 1);
    assert(service.artifact.persisted_frame_count == 6);

    assert(rdx_record_local_destination_ops()->seal(&destination, 8, 4) == 0);
    assert(native.append_calls == 2);
    assert(native.append_lengths[1] == 4);
    assert(events.calls == 1);
    assert(events.last.persisted_frame_count == 7);
    assert(events.last.file_size == 28);
}

static void test_pause_resume_rebind_preserves_artifact(void)
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
    const struct rdx_record_destination_ops *ops;

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
    ops = rdx_record_local_destination_ops();
    assert(ops->attach(&destination, 8, 4) == 0);
    assert(ops->rebind_capture != NULL);
    assert(ops->rebind_capture(&destination, 8, 3, 5) != 0);
    assert(service.capture_generation == 4);
    assert(ops->rebind_capture(&destination, 8, 4, 5) == 0);
    assert(service.capture_generation == 5);

    frame = test_frame(0, payload);
    assert(ops->consume(&destination, &frame) != 0);
    frame.capture_generation = 5;
    assert(ops->consume(&destination, &frame) == 0);
    assert(rdx_local_storage_service_pump(&service, 0)
           == RDX_STORAGE_RESULT_READY);
    assert(ops->seal(&destination, 8, 5) == 0);
    assert(events.calls == 1);
    assert(events.last.type == RDX_LOCAL_STORAGE_COMPLETION_ARTIFACT_COMMITTED);
    assert(events.last.frame_count == 1);
    assert(events.last.persisted_frame_count == 1);
    assert(!service.active);
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

static void test_coordinator_reserves_persistent_identity(void)
{
    struct fake_media media = { 0, RDX_STORAGE_RESULT_READY };
    struct fake_native native = { 0 };
    struct fake_completion events = { 0 };
    struct rdx_storage_media_ops media_ops = { 0 };
    struct rdx_native_artifact_ops native_ops = { 0 };
    struct rdx_local_storage_service service;
    struct rdx_record_coordinator coordinator;
    struct rdx_record_request request;
    uint32_t wire_serial_number;
    char wire_filename[RDX_STORAGE_NAME_MAX];
    const char *final_basename;

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
    memset(&request, 0, sizeof(request));
    request.request_id = 7;
    request.type = RDX_RECORD_REQUEST_START;
    request.format_id = RDX_RECORD_FORMAT_MEETING_V1;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, completion, &events) == 0);
    assert(rdx_record_coordinator_init(&coordinator, &service, NULL, NULL)
           == 0);
    assert(rdx_record_coordinator_prepare_engine_start(
               &coordinator, &request) == RDX_RECORD_RESULT_OK);
    assert(service.artifact.spec.artifact_id.value == 3);
    assert(service.artifact.spec.serial_number == 3);
    assert(service.artifact.spec.session_id == 0);
    assert(service.artifact.spec.payload_size
           == RDX_RECORD_MEETING_V1_PAYLOAD_SIZE);
    assert(!strcmp(service.artifact.spec.temp_name,
                   "RDXTMP/T00000003.OPU"));
    final_basename = service.artifact.spec.final_name;
    assert(strlen(final_basename) >= sizeof("000.raw") - 1);
    assert(!strcmp(final_basename + strlen(final_basename) - 4, ".raw"));
    assert(rdx_record_coordinator_get_record_identity(
               &coordinator, &wire_serial_number, wire_filename,
               sizeof(wire_filename)) == 0);
    assert(wire_serial_number == 3);
    assert(!strcmp(wire_filename, final_basename));
    assert(native.serial_high_water == 3);
    assert(native.index_commit_calls == 1);
    assert(request.destination_ops
           == rdx_record_composite_destination_ops());
    assert(rdx_record_coordinator_cancel_start(
               &coordinator, RDX_RECORD_CAUSE_START_FAILED)
           != RDX_STORAGE_RESULT_READY);

    memset(&request, 0, sizeof(request));
    request.request_id = 8;
    request.type = RDX_RECORD_REQUEST_START;
    request.format_id = RDX_RECORD_FORMAT_CALL_V1;
    assert(rdx_record_coordinator_prepare_recording(&coordinator, &request)
           == RDX_STORAGE_RESULT_READY);
    assert(service.artifact.spec.artifact_id.value == 4);
    assert(service.artifact.spec.serial_number == 4);
    assert(service.artifact.spec.format_id == RDX_RECORD_FORMAT_CALL_V1);
    assert(native.serial_high_water == 4);
    assert(native.index_commit_calls == 2);
    assert(rdx_record_coordinator_cancel_start(
               &coordinator, RDX_RECORD_CAUSE_START_FAILED)
           != RDX_STORAGE_RESULT_READY);
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

static void test_app_operations_are_serialized_and_identity_safe(void)
{
    struct fake_operation_media media;
    struct fake_native native;
    struct fake_operation_completion events;
    struct rdx_storage_media_ops media_ops;
    struct rdx_native_artifact_ops native_ops;
    struct rdx_local_storage_service service;
    struct rdx_local_storage_operation_request request;
    struct rdx_storage_file_info items[2];
    uint8_t read_buffer[4];
    int calls_before;

    memset(&media, 0, sizeof(media));
    memset(&native, 0, sizeof(native));
    memset(&events, 0, sizeof(events));
    memset(&media_ops, 0, sizeof(media_ops));
    memset(&native_ops, 0, sizeof(native_ops));
    media.total_bytes = 8ULL * 1024 * 1024;
    media.free_bytes = 3ULL * 1024 * 1024;
    media.count = 1;
    media.files[0].file_id.artifact_id.value = 0x700;
    media.files[0].file_id.serial_number = 7;
    strcpy(media.files[0].file_id.name, "abc.raw");
    media.files[0].file_size = 4;
    media.files[0].audio_crc = 0x12345678UL;
    media.data[0] = 0x10;
    media.data[1] = 0x20;
    media.data[2] = 0x30;
    media.data[3] = 0x40;
    media_ops.ensure_ready = operation_ready;
    media_ops.get_capacity = operation_capacity;
    media_ops.read = operation_read;
    media_ops.enumerate = operation_enumerate;
    media_ops.delete = operation_delete;
    media_ops.format = operation_format;
    native_ops.recover = fake_recover;
    native_ops.open_temp = fake_open_temp;
    native_ops.append = fake_append;
    native_ops.close = fake_close;
    native_ops.publish = fake_publish;
    native_ops.store_marker = fake_marker;
    native_ops.load_index = fake_load_index;
    native_ops.commit_index = fake_commit_index;
    native_ops.quarantine = fake_quarantine;
    g_operation_media = &media;
    g_native = &native;
    assert(rdx_local_storage_service_init(
               &service, RDX_LOCAL_STORAGE_BACKEND_NATIVE, &media_ops,
               &native_ops, NULL, NULL, NULL) == 0);

    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_CAPACITY;
    request.token = 1;
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == 1);
    assert(events.last.type == RDX_LOCAL_STORAGE_OPERATION_CAPACITY);
    assert(events.last.token == 1);
    assert(events.last.total_bytes == media.total_bytes);
    assert(events.last.free_bytes == media.free_bytes);

    memset(items, 0, sizeof(items));
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_ENUMERATE;
    request.token = 2;
    request.data.enumerate.items = items;
    request.data.enumerate.capacity = 2;
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == 2 && events.last.count == 1);
    assert(items[0].file_id.serial_number == 7);

    memset(read_buffer, 0, sizeof(read_buffer));
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_READ_BY_SERIAL;
    request.token = 3;
    request.data.read.serial_number = 7;
    request.data.read.offset = 1;
    request.data.read.buffer = read_buffer;
    request.data.read.length = 2;
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == 3);
    assert(events.last.result == RDX_STORAGE_RESULT_READY);
    assert(events.last.read_length == 2);
    assert(events.last.file.file_id.serial_number == 7);
    assert(read_buffer[0] == 0x20 && read_buffer[1] == 0x30);

    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY;
    request.token = 4;
    request.data.delete_file.serial_number = 7;
    strcpy(request.data.delete_file.filename, "wrong.raw");
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == 4);
    assert(events.last.result == RDX_STORAGE_RESULT_BAD_ARGUMENT);
    assert(media.delete_calls == 0 && media.count == 1);

    request.token = 5;
    strcpy(request.data.delete_file.filename, "abc.raw");
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == 5);
    assert(events.last.result == RDX_STORAGE_RESULT_READY);
    assert(media.delete_calls == 1 && media.count == 0);

    media.count = 1;
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_FORMAT;
    request.token = 6;
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == 6);
    assert(events.last.result == RDX_STORAGE_RESULT_READY);
    assert(events.last.total_bytes == media.total_bytes);
    assert(events.last.free_bytes == media.free_bytes);
    assert(media.format_calls == 1 && native.recover_calls == 1);
    assert(rdx_local_storage_service_readiness(&service)
           == RDX_STORAGE_READINESS_READY);

    media.format_result = RDX_STORAGE_RESULT_IO_ERROR;
    request.token = 7;
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == 7);
    assert(events.last.result == RDX_STORAGE_RESULT_IO_ERROR);
    assert(rdx_local_storage_service_readiness(&service)
           == RDX_STORAGE_READINESS_RECOVERY_REQUIRED);

    calls_before = events.calls;
    service.active = 1;
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_CAPACITY;
    request.token = 8;
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_BUSY);
    assert(events.calls == calls_before);
    service.active = 0;

    media.count = 2;
    media.files[1] = media.files[0];
    media.files[1].file_id.artifact_id.value = 0x701;
    strcpy(media.files[1].file_id.name, "duplicate.raw");
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_READ_BY_SERIAL;
    request.token = 9;
    request.data.read.serial_number = 7;
    request.data.read.buffer = read_buffer;
    request.data.read.length = sizeof(read_buffer);
    assert(rdx_local_storage_service_submit_operation(
               &service, &request, operation_completion, &events)
           == RDX_STORAGE_RESULT_READY);
    assert(events.calls == calls_before + 1);
    assert(events.last.result == RDX_STORAGE_RESULT_CORRUPT);
    assert(rdx_local_storage_service_readiness(&service)
           == RDX_STORAGE_READINESS_RECOVERY_REQUIRED);
}

int main(void)
{
    test_native_commit();
    test_native_checkpoint_cadence();
    test_native_write_batch_and_seal_tail();
    test_pause_resume_rebind_preserves_artifact();
    test_required_queue_failure();
    test_short_write_is_failed();
    test_close_size_mismatch_is_not_published();
    test_indexed_marker_failure_is_not_committed();
    test_coordinator_binds_runtime_session();
    test_coordinator_reserves_persistent_identity();
    test_recovery_failure_gates_start();
    test_media_outage_invalidates_recovery();
    test_app_operations_are_serialized_and_identity_safe();
    return 0;
}
