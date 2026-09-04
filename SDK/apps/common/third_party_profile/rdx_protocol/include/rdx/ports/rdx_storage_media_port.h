#ifndef RDX_STORAGE_MEDIA_PORT_H
#define RDX_STORAGE_MEDIA_PORT_H

#include <stdint.h>

#include "rdx/rdx_storage_types.h"

/*
 * Common media semantics shared by Native FAT and Legacy UXFILE modes.  An
 * implementation may know the JL mount and VFS, but must not expose those
 * platform handles through this vtable.
 */
struct rdx_storage_media_ops {
    enum rdx_storage_result (*ensure_ready)(void);
    enum rdx_storage_result (*quiesce)(uint16_t timeout_ms);
    enum rdx_storage_result (*get_capacity)(uint64_t *total_bytes,
                                             uint64_t *free_bytes);
    /* Upload readers are Backend-owned cursors.  The Service resolves a
     * trusted index identity once, opens one cursor for that upload session,
     * and then performs bounded seek/read calls without reopening the file.
     * Platform handles stay opaque above this port. */
    enum rdx_storage_result (*open_reader)(
        const struct rdx_storage_file_info *file, void **reader);
    enum rdx_storage_result (*read_reader)(
        void *reader, uint64_t offset, uint8_t *buf,
        uint32_t len, uint32_t *read_len);
    enum rdx_storage_result (*close_reader)(void *reader);
    enum rdx_storage_result (*stat)(const struct rdx_storage_file_id *file_id,
                                    uint64_t *file_size, uint32_t *audio_crc);
    enum rdx_storage_result (*enumerate)(struct rdx_storage_file_info *items,
                                         uint32_t capacity,
                                         uint32_t *count,
                                         uint32_t *generation);
    enum rdx_storage_result (*delete)(const struct rdx_storage_file_id *file_id);
    enum rdx_storage_result (*format)(void);
};

#endif /* RDX_STORAGE_MEDIA_PORT_H */
