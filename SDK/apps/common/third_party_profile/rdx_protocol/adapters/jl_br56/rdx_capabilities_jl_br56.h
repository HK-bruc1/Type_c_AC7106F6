#ifndef RDX_CAPABILITIES_JL_BR56_H
#define RDX_CAPABILITIES_JL_BR56_H

#include "rdx/rdx_capability.h"

/* Potential capabilities of the audited JL BR56 SDK Adapter family. */
#define RDX_JL_BR56_AVAILABLE_CAPS              \
    (RDX_CAP_TRANSPORT                         | \
     RDX_CAP_DEVICE_STATE_REPOSITORY           | \
     RDX_CAP_DEVICE_IDENTITY                   | \
     RDX_CAP_RTC                               | \
     RDX_CAP_MEETING_CAPTURE                   | \
     RDX_CAP_CALL_CAPTURE)

#endif /* RDX_CAPABILITIES_JL_BR56_H */
