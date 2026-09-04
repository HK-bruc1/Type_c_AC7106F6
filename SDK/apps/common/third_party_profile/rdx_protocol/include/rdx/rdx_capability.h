#ifndef RDX_CAPABILITY_H
#define RDX_CAPABILITY_H

#include "rdx/rdx_ability.h"

/*
 * Technical capabilities are intentionally independent from the published
 * Device Ability bit layout.  They describe what the selected target can
 * compile and prove structurally, not what the product chooses to advertise.
 */
#define RDX_CAP_NONE                            0x00000000UL
#define RDX_CAP_TRANSPORT                       (1UL << 0)
#define RDX_CAP_DEVICE_STATE_REPOSITORY         (1UL << 1)
#define RDX_CAP_DEVICE_IDENTITY                 (1UL << 2)
#define RDX_CAP_FACTORY_IDENTITY                (1UL << 3)
#define RDX_CAP_RTC                             (1UL << 4)
#define RDX_CAP_MEETING_CAPTURE                 (1UL << 5)
#define RDX_CAP_CALL_CAPTURE                    (1UL << 6)
#define RDX_CAP_LOCAL_STORAGE                   (1UL << 7)
#define RDX_CAP_STORAGE_BACKEND_NATIVE         (1UL << 8)
#define RDX_CAP_STORAGE_BACKEND_LEGACY_UXFILE  (1UL << 9)
#define RDX_CAP_STORAGE_SERVICE                 (1UL << 10)
#define RDX_CAP_OTA_UPDATE                      (1UL << 11)

#define RDX_REQUIRED_BASE_CAPS                  \
    (RDX_CAP_TRANSPORT                         | \
     RDX_CAP_DEVICE_STATE_REPOSITORY           | \
     RDX_CAP_DEVICE_IDENTITY                    | \
     RDX_CAP_FACTORY_IDENTITY)

#define RDX_REQUIRED_CAPS_FROM_ABILITY(ability) \
    (RDX_REQUIRED_BASE_CAPS                    | \
     (((ability) & RDX_ABILITY_RTC)            \
          ? RDX_CAP_RTC : RDX_CAP_NONE)        | \
     (((ability) & RDX_ABILITY_CONFERENCE_RECORDING) \
          ? RDX_CAP_MEETING_CAPTURE : RDX_CAP_NONE)  | \
     (((ability) & RDX_ABILITY_CALL_RECORDING) \
          ? RDX_CAP_CALL_CAPTURE : RDX_CAP_NONE) | \
     (((ability) & RDX_ABILITY_LOCAL_STORAGE) \
          ? RDX_CAP_LOCAL_STORAGE : RDX_CAP_NONE))

#endif /* RDX_CAPABILITY_H */
