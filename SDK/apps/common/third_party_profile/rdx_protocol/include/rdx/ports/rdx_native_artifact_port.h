#ifndef RDX_NATIVE_ARTIFACT_PORT_H
#define RDX_NATIVE_ARTIFACT_PORT_H

#include <stdint.h>

#include "rdx/rdx_storage_types.h"

/* Native mode only: the Storage Service owns the Artifact transaction. */
struct rdx_native_artifact_ops {
    enum rdx_storage_result (*open_temp)(
        const struct rdx_storage_artifact_spec *spec, void **handle);
    enum rdx_storage_result (*append)(void *handle, const uint8_t *data,
                                      uint32_t len, uint32_t *written);
    enum rdx_storage_result (*close)(void *handle, uint64_t *file_size,
                                     uint32_t *audio_crc);
    enum rdx_storage_result (*truncate)(void *handle, uint64_t size);
    enum rdx_storage_result (*publish)(const char *temp_name,
                                       const char *final_name);
    enum rdx_storage_result (*store_marker)(
        const struct rdx_storage_marker *marker);
    enum rdx_storage_result (*load_latest_marker)(
        const struct rdx_artifact_id *artifact_id,
        struct rdx_storage_marker *marker);
    enum rdx_storage_result (*load_index)(
        struct rdx_storage_index_snapshot *snapshot);
    enum rdx_storage_result (*commit_index)(
        const struct rdx_storage_index_snapshot *snapshot);
    enum rdx_storage_result (*quarantine)(
        const struct rdx_artifact_id *artifact_id, const char *reason);
};

#endif /* RDX_NATIVE_ARTIFACT_PORT_H */
