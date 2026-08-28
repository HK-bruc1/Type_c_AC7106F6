#ifndef RDX_DOMAIN_ARTIFACT_H
#define RDX_DOMAIN_ARTIFACT_H

#include <stdint.h>

#include "rdx/rdx_storage_types.h"

/* In-memory Artifact aggregate.  It contains no FAT or SDK handles. */
struct rdx_artifact {
    struct rdx_storage_artifact_spec spec;
    enum rdx_artifact_state state;
    uint32_t generation;
    uint32_t produced_frame_count;
    uint32_t destination_accepted_count;
    uint32_t local_accepted_count;
    uint32_t persisted_frame_count;
    uint32_t rejected_frame_count;
    uint32_t dropped_frame_count;
    enum rdx_record_cause first_error_cause;
    uint64_t audio_bytes;
    uint32_t audio_crc;
};

void rdx_artifact_init(struct rdx_artifact *artifact);
int rdx_artifact_begin(struct rdx_artifact *artifact,
                       const struct rdx_storage_artifact_spec *spec);
int rdx_artifact_mark_writing(struct rdx_artifact *artifact);
int rdx_artifact_note_produced(struct rdx_artifact *artifact);
int rdx_artifact_note_destination_accepted(struct rdx_artifact *artifact);
int rdx_artifact_accept_frame(struct rdx_artifact *artifact,
                              uint32_t payload_len);
int rdx_artifact_persist_frame(struct rdx_artifact *artifact,
                               uint32_t payload_len, uint32_t audio_crc);
int rdx_artifact_queue_seal(struct rdx_artifact *artifact);
int rdx_artifact_begin_commit(struct rdx_artifact *artifact);
int rdx_artifact_commit(struct rdx_artifact *artifact);
int rdx_artifact_fail(struct rdx_artifact *artifact,
                      enum rdx_record_cause cause);
int rdx_artifact_quarantine(struct rdx_artifact *artifact);

#endif /* RDX_DOMAIN_ARTIFACT_H */
