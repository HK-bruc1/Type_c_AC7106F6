#ifndef RDX_LEGACY_UXFILE_PORT_H
#define RDX_LEGACY_UXFILE_PORT_H

#include <stdint.h>

#include "rdx/rdx_storage_types.h"

/*
 * Compatibility mode only.  These calls are the narrow seam around the
 * rdx_uxfile.c.o worker; UXFILE private structures and callbacks stay inside
 * the adapter and are never passed to Engine or Protocol.
 */
struct rdx_legacy_uxfile_ops {
    enum rdx_storage_result (*init)(void);
    enum rdx_storage_result (*begin_file)(
        const struct rdx_storage_artifact_spec *spec,
        struct rdx_storage_file_id *file_id);
    enum rdx_storage_result (*raw_write)(const struct rdx_storage_file_id *file_id,
                                         const uint8_t *data, uint32_t len,
                                         uint32_t *written);
    enum rdx_storage_result (*flush_commit)(
        const struct rdx_storage_file_id *file_id);
    enum rdx_storage_result (*abort_file)(
        const struct rdx_storage_file_id *file_id);
};

#endif /* RDX_LEGACY_UXFILE_PORT_H */
