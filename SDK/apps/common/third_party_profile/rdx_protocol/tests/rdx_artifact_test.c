#include <assert.h>
#include <string.h>

#include "rdx/domain/rdx_artifact.h"

static struct rdx_storage_artifact_spec test_spec(void)
{
    struct rdx_storage_artifact_spec spec;

    memset(&spec, 0, sizeof(spec));
    spec.artifact_id.value = 0x1234;
    spec.session_id = 7;
    spec.serial_number = 1;
    spec.format_id = RDX_RECORD_FORMAT_MEETING_V1;
    spec.sample_rate = RDX_RECORD_MEETING_V1_SAMPLE_RATE;
    spec.channels = RDX_RECORD_MEETING_V1_CHANNELS;
    spec.frame_ms = RDX_RECORD_MEETING_V1_FRAME_MS;
    spec.payload_size = RDX_RECORD_MEETING_V1_PAYLOAD_SIZE;
    return spec;
}

static void test_commit_requires_persisted_frames(void)
{
    struct rdx_artifact artifact;
    struct rdx_storage_artifact_spec spec = test_spec();

    rdx_artifact_init(&artifact);
    assert(rdx_artifact_begin(&artifact, &spec) == 0);
    assert(artifact.state == RDX_ARTIFACT_STATE_CREATING);
    assert(rdx_artifact_mark_writing(&artifact) == 0);
    assert(rdx_artifact_note_produced(&artifact) == 0);
    assert(rdx_artifact_note_destination_accepted(&artifact) == 0);
    assert(rdx_artifact_accept_frame(&artifact, spec.payload_size) == 0);
    assert(rdx_artifact_queue_seal(&artifact) != 0);
    assert(rdx_artifact_persist_frame(&artifact, spec.payload_size, 0x55aa) == 0);
    assert(rdx_artifact_queue_seal(&artifact) == 0);
    assert(rdx_artifact_begin_commit(&artifact) == 0);
    assert(rdx_artifact_commit(&artifact) == 0);
    assert(artifact.state == RDX_ARTIFACT_STATE_COMMITTED);
    assert(artifact.audio_bytes == spec.payload_size);
}

static void test_failure_is_first_cause_and_quarantine(void)
{
    struct rdx_artifact artifact;
    struct rdx_storage_artifact_spec spec = test_spec();

    rdx_artifact_init(&artifact);
    assert(rdx_artifact_begin(&artifact, &spec) == 0);
    assert(rdx_artifact_mark_writing(&artifact) == 0);
    assert(rdx_artifact_fail(&artifact, RDX_RECORD_CAUSE_QUEUE_FULL) == 0);
    assert(artifact.first_error_cause == RDX_RECORD_CAUSE_QUEUE_FULL);
    assert(rdx_artifact_fail(&artifact, RDX_RECORD_CAUSE_SOURCE_LOST) != 0);
    assert(rdx_artifact_quarantine(&artifact) == 0);
    assert(artifact.state == RDX_ARTIFACT_STATE_QUARANTINED);
}

int main(void)
{
    test_commit_requires_persisted_frames();
    test_failure_is_first_cause_and_quarantine();
    return 0;
}
