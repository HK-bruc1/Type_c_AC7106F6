#ifndef RDX_STORAGE_BACKEND_SD0_JL_BR56_H
#define RDX_STORAGE_BACKEND_SD0_JL_BR56_H

#include "rdx/ports/rdx_native_artifact_port.h"
#include "rdx/ports/rdx_storage_media_port.h"

/* The BR56 adapter only observes the mount created by dev_manager_init(). */
int rdx_storage_backend_sd0_jl_br56_init(void);
const struct rdx_storage_media_ops *
rdx_storage_backend_sd0_jl_br56_media_ops(void);
const struct rdx_native_artifact_ops *
rdx_storage_backend_sd0_jl_br56_native_ops(void);

#endif /* RDX_STORAGE_BACKEND_SD0_JL_BR56_H */
