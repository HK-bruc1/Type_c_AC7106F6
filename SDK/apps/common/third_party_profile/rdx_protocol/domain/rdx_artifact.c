#include <string.h>

#include "rdx/domain/rdx_artifact.h"

static int rdx_artifact_is_terminal(enum rdx_artifact_state state)
{
    return state == RDX_ARTIFACT_STATE_COMMITTED
           || state == RDX_ARTIFACT_STATE_QUARANTINED;
}

void rdx_artifact_init(struct rdx_artifact *artifact)
{
    if (artifact) {
        memset(artifact, 0, sizeof(*artifact));
    }
}

int rdx_artifact_begin(struct rdx_artifact *artifact,
                       const struct rdx_storage_artifact_spec *spec)
{
    if (!artifact || !spec || artifact->state != RDX_ARTIFACT_STATE_NONE
        || !spec->artifact_id.value || !spec->serial_number
        || !spec->payload_size) {
        return -1;
    }
    memset(artifact, 0, sizeof(*artifact));
    artifact->spec = *spec;
    artifact->state = RDX_ARTIFACT_STATE_CREATING;
    artifact->generation = 1;
    return 0;
}

int rdx_artifact_mark_writing(struct rdx_artifact *artifact)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_CREATING) {
        return -1;
    }
    artifact->state = RDX_ARTIFACT_STATE_WRITING;
    return 0;
}

int rdx_artifact_note_produced(struct rdx_artifact *artifact)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_WRITING
        || artifact->produced_frame_count == UINT32_MAX) {
        return -1;
    }
    artifact->produced_frame_count++;
    return 0;
}

int rdx_artifact_note_destination_accepted(struct rdx_artifact *artifact)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_WRITING) {
        return -1;
    }
    if (artifact->destination_accepted_count
        >= artifact->produced_frame_count
        || artifact->destination_accepted_count == UINT32_MAX) {
        return -1;
    }
    artifact->destination_accepted_count++;
    return 0;
}

int rdx_artifact_accept_frame(struct rdx_artifact *artifact,
                              uint32_t payload_len)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_WRITING
        || payload_len != artifact->spec.payload_size
        || artifact->local_accepted_count
           >= artifact->produced_frame_count
        || artifact->local_accepted_count == UINT32_MAX) {
        return -1;
    }
    artifact->local_accepted_count++;
    return 0;
}

int rdx_artifact_persist_frame(struct rdx_artifact *artifact,
                               uint32_t payload_len, uint32_t audio_crc)
{
    if (!artifact || (artifact->state != RDX_ARTIFACT_STATE_WRITING
                      && artifact->state != RDX_ARTIFACT_STATE_SEAL_QUEUED)
        || payload_len != artifact->spec.payload_size
        || artifact->persisted_frame_count >= artifact->local_accepted_count
        || artifact->persisted_frame_count == UINT32_MAX
        || (uint64_t)payload_len > UINT64_MAX - artifact->audio_bytes) {
        return -1;
    }
    artifact->persisted_frame_count++;
    artifact->audio_bytes += payload_len;
    artifact->audio_crc = audio_crc;
    return 0;
}

int rdx_artifact_queue_seal(struct rdx_artifact *artifact)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_WRITING
        || artifact->local_accepted_count != artifact->persisted_frame_count
        || artifact->destination_accepted_count > artifact->produced_frame_count) {
        return -1;
    }
    artifact->state = RDX_ARTIFACT_STATE_SEAL_QUEUED;
    return 0;
}

int rdx_artifact_begin_commit(struct rdx_artifact *artifact)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_SEAL_QUEUED) {
        return -1;
    }
    artifact->state = RDX_ARTIFACT_STATE_COMMITTING;
    return 0;
}

int rdx_artifact_commit(struct rdx_artifact *artifact)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_COMMITTING
        || artifact->local_accepted_count != artifact->persisted_frame_count
        || artifact->audio_bytes
           != (uint64_t)artifact->persisted_frame_count
              * artifact->spec.payload_size) {
        return -1;
    }
    artifact->state = RDX_ARTIFACT_STATE_COMMITTED;
    return 0;
}

int rdx_artifact_fail(struct rdx_artifact *artifact,
                      enum rdx_record_cause cause)
{
    if (!artifact || rdx_artifact_is_terminal(artifact->state)
        || artifact->state == RDX_ARTIFACT_STATE_NONE
        || artifact->state == RDX_ARTIFACT_STATE_FAILED
        || cause == RDX_RECORD_CAUSE_NONE) {
        return -1;
    }
    artifact->state = RDX_ARTIFACT_STATE_FAILED;
    if (artifact->first_error_cause == RDX_RECORD_CAUSE_NONE) {
        artifact->first_error_cause = cause;
    }
    return 0;
}

int rdx_artifact_quarantine(struct rdx_artifact *artifact)
{
    if (!artifact || artifact->state != RDX_ARTIFACT_STATE_FAILED) {
        return -1;
    }
    artifact->state = RDX_ARTIFACT_STATE_QUARANTINED;
    return 0;
}
