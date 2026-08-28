#ifndef RDX_STORAGE_BACKEND_LEGACY_UXFILE_H
#define RDX_STORAGE_BACKEND_LEGACY_UXFILE_H

#include "rdx/ports/rdx_legacy_uxfile_port.h"
#include "rdx/ports/rdx_storage_media_port.h"

/* This header is intentionally only a seam.  The concrete declarations must
 * be generated from the licensed rdx_uxfile.c.o ABI audit before selection. */
int rdx_storage_backend_legacy_uxfile_init(void);
const struct rdx_storage_media_ops *
rdx_storage_backend_legacy_uxfile_media_ops(void);
const struct rdx_legacy_uxfile_ops *
rdx_storage_backend_legacy_uxfile_ops(void);

#endif /* RDX_STORAGE_BACKEND_LEGACY_UXFILE_H */
