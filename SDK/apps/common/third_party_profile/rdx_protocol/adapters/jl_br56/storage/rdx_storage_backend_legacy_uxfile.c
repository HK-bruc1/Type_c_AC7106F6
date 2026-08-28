#include "app_config.h"

#if RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && RDX_STORAGE_BACKEND_LEGACY_UXFILE

/* Do not guess the UXFILE ABI.  The static library member, public headers,
 * callback lifetime and required legacy dependencies are still unverified on
 * BR56.  A legacy build must opt into the audited adapter explicitly. */
#if !RDX_LEGACY_UXFILE_ABI_READY
#error "Legacy UXFILE backend requires the audited rdx_uxfile ABI adapter"
#endif
#if !RDX_LEGACY_UXFILE_ADAPTER_IMPLEMENTED
#error "Legacy UXFILE backend adapter is not implemented for this target"
#endif

#include "rdx_storage_backend_legacy_uxfile.h"

#endif /* RDX_CFG_LOCAL_STORAGE_BUILD_ENABLE && RDX_STORAGE_BACKEND_LEGACY_UXFILE */
