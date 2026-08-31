#ifndef RDX_STORAGE_TYPES_H
#define RDX_STORAGE_TYPES_H

#include <stdint.h>

#include "rdx/rdx_record_types.h"

/*
 * Storage is an RDX business contract.  Keep platform objects (FILE *,
 * __dev, mount handles, etc.) out of these types so the domain and
 * application layers remain host-testable.
 */
#define RDX_STORAGE_NAME_MAX              64

enum rdx_storage_result {
    RDX_STORAGE_RESULT_READY = 0,
    RDX_STORAGE_RESULT_NOT_READY,
    RDX_STORAGE_RESULT_BUSY,
    RDX_STORAGE_RESULT_QUEUE_FULL,
    RDX_STORAGE_RESULT_BAD_STATE,
    RDX_STORAGE_RESULT_BAD_ARGUMENT,
    RDX_STORAGE_RESULT_NO_SPACE,
    RDX_STORAGE_RESULT_IO_ERROR,
    RDX_STORAGE_RESULT_CORRUPT,
    RDX_STORAGE_RESULT_UNSUPPORTED,
    RDX_STORAGE_RESULT_TIMEOUT,
    RDX_STORAGE_RESULT_CHECKSUM,
};

enum rdx_storage_readiness {
    RDX_STORAGE_READINESS_UNINITIALIZED = 0,
    RDX_STORAGE_READINESS_PROBING,
    RDX_STORAGE_READINESS_RECOVERING,
    RDX_STORAGE_READINESS_READY,
    RDX_STORAGE_READINESS_UNAVAILABLE,
    RDX_STORAGE_READINESS_RECOVERY_REQUIRED,
    RDX_STORAGE_READINESS_QUIESCING,
    RDX_STORAGE_READINESS_HOST_OWNED,
};

enum rdx_artifact_state {
    RDX_ARTIFACT_STATE_NONE = 0,
    RDX_ARTIFACT_STATE_CREATING,
    RDX_ARTIFACT_STATE_WRITING,
    RDX_ARTIFACT_STATE_SEAL_QUEUED,
    RDX_ARTIFACT_STATE_COMMITTING,
    RDX_ARTIFACT_STATE_COMMITTED,
    RDX_ARTIFACT_STATE_FAILED,
    RDX_ARTIFACT_STATE_QUARANTINED,
};

/* Stable on-media values used by the Native marker serializer.  They are
 * intentionally separate from enum rdx_artifact_state: changing an in-memory
 * domain enum must never renumber an existing recovery record. */
enum rdx_storage_marker_state {
    RDX_STORAGE_MARKER_STATE_ALLOCATED = 0,
    RDX_STORAGE_MARKER_STATE_WRITING = 1,
    RDX_STORAGE_MARKER_STATE_SEALED = 2,
    RDX_STORAGE_MARKER_STATE_PUBLISHED = 3,
    RDX_STORAGE_MARKER_STATE_INDEXED = 4,
    RDX_STORAGE_MARKER_STATE_FAILED = 5,
};

struct rdx_artifact_id {
    uint64_t value;
};

struct rdx_storage_file_id {
    struct rdx_artifact_id artifact_id;
    uint32_t serial_number;
    char name[RDX_STORAGE_NAME_MAX];
};

struct rdx_storage_artifact_spec {
    struct rdx_artifact_id artifact_id;
    uint32_t session_id;
    uint32_t serial_number;
    enum rdx_record_format_id format_id;
    uint32_t sample_rate;
    uint8_t channels;
    uint16_t frame_ms;
    uint16_t payload_size;
    char temp_name[RDX_STORAGE_NAME_MAX];
    char final_name[RDX_STORAGE_NAME_MAX];
};

/*
 * Marker fields are deliberately scalar and fixed-width.  The on-media
 * serializer owns byte order, CRC coverage and padding rules; callers must
 * not write this C representation directly to FAT.
 */
struct rdx_storage_marker {
    struct rdx_artifact_id artifact_id;
    uint32_t generation;
    uint32_t state;
    uint32_t session_id;
    uint32_t serial_number;
    uint32_t produced_frame_count;
    uint32_t destination_accepted_count;
    uint32_t local_accepted_count;
    uint32_t persisted_frame_count;
    uint32_t rejected_frame_count;
    uint32_t dropped_frame_count;
    uint32_t first_error_cause;
    uint64_t audio_bytes;
    uint32_t audio_crc;
    uint32_t marks_count;
    uint32_t marks_crc;
    char temp_name[RDX_STORAGE_NAME_MAX];
    char final_name[RDX_STORAGE_NAME_MAX];
};

struct rdx_storage_file_info {
    struct rdx_storage_file_id file_id;
    uint32_t state;
    uint64_t file_size;
    uint32_t frame_count;
    uint32_t audio_crc;
};

struct rdx_storage_index_entry {
    struct rdx_storage_file_info file;
};

/* The service owns the entry buffer; the port never allocates it. */
struct rdx_storage_index_snapshot {
    uint32_t schema_version;
    uint32_t generation;
    uint32_t serial_high_water;
    struct rdx_storage_index_entry *entries;
    uint32_t count;
    uint32_t capacity;
};

#endif /* RDX_STORAGE_TYPES_H */
