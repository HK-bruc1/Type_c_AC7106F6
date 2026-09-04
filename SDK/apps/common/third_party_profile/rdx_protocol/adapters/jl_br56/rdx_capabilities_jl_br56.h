#ifndef RDX_CAPABILITIES_JL_BR56_H
#define RDX_CAPABILITIES_JL_BR56_H

#include "rdx/rdx_capability.h"

/* Potential capabilities of the audited JL BR56 SDK Adapter family. */
#define RDX_JL_BR56_AVAILABLE_CAPS              \
    (RDX_CAP_TRANSPORT                         | \
     RDX_CAP_DEVICE_STATE_REPOSITORY           | \
     RDX_CAP_DEVICE_IDENTITY                   | \
     RDX_CAP_FACTORY_IDENTITY                  | \
     RDX_CAP_RTC                               | \
     RDX_CAP_MEETING_CAPTURE                   | \
     RDX_CAP_CALL_CAPTURE                       | \
     RDX_CAP_LOCAL_STORAGE                      | \
     RDX_CAP_STORAGE_BACKEND_NATIVE             | \
     RDX_CAP_STORAGE_SERVICE                    | \
     RDX_CAP_OTA_UPDATE)

#endif /* RDX_CAPABILITIES_JL_BR56_H */
