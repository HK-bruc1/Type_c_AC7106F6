#ifndef RDX_MVP0_COMPAT_CONFIG_H
#define RDX_MVP0_COMPAT_CONFIG_H

#include "rdx_protocol/rdx_protocol_defs.h"

/*
 * MVP0 temporarily mirrors the selected 701 reference product so the BLE
 * transport can be validated with the same APP. Replace these values only
 * after a Golden Trace and the target product identity have been frozen.
 */
#define RDX_COMPAT_LOCAL_NAME                  "Zenchord Case 0000"
#define RDX_COMPAT_PRODUCT_CODE                "603"
#define RDX_COMPAT_FACTORY_CODE                "ZENCHD"
#define RDX_COMPAT_PRODUCT_TYPE                "E1"
/*
 * Fixed 2-byte identity suffix carried at the end of Manufacturer Data.
 * The device does not interpret it, but the APP may use it for discovery or
 * compatibility matching, so it is not an unused placeholder and must not be
 * removed or changed without a confirmed APP/protocol contract.
 * It is neither a color code, a Device Ability switch, nor Record Mark.
 */
#define RDX_COMPAT_SELF_MARK                   "NV"

/*
 * Selects the RDX Identity characteristic wire layout, not a hardware or
 * feature switch. 1 uses the charge-case-compatible 95-byte "C,..." layout;
 * 0 uses the non-case layout that currently returns only the 24-byte AuthKey.
 * It does not enable charging, storage, recording, or any Device Ability.
 */
#define RDX_COMPAT_PRODUCT_IS_CHARGE_CASE      1

/*
 * Product feature selection has one source of truth: Device Ability.
 * rdx_integration_config.h derives all internal module gates from this mask;
 * do not add parallel product switches for individual RDX abilities.
 */
#define RDX_COMPAT_DEVICE_ABILITY              \
    (RDX_ABILITY_RTC                          | \
     RDX_ABILITY_CONFERENCE_RECORDING         | \
     RDX_ABILITY_RECORD_PAUSE_RESUME           | \
     RDX_ABILITY_RECORD_MARK)
#define RDX_COMPAT_INCLUDE_PROTOCOL_VERSION    1
#define RDX_COMPAT_PROTOCOL_VERSION            0x17
#define RDX_MVP0_FIXED_BATTERY_LEVEL           100
#define RDX_BLE_ADV_INTERVAL                   160

#define RDX_COMPAT_FIRMWARE_VERSION            "1.0.0"
#define RDX_COMPAT_HARDWARE_VERSION            "0.0.1"

/* Official Zenchord compatibility set; the MAC-limited test key is excluded. */
#define RDX_COMPAT_APP_KEY_PRIMARY             "14F6F7A1508E4155"
#define RDX_COMPAT_APP_KEY_1                   "385FA36EC106DE3D"
#define RDX_COMPAT_APP_KEY_2                   "7C0BAF778B727175"
#define RDX_COMPAT_APP_KEY_LIST                \
    RDX_COMPAT_APP_KEY_PRIMARY,                \
    RDX_COMPAT_APP_KEY_1,                      \
    RDX_COMPAT_APP_KEY_2

/* Development-only identity placeholders. Never put production keys here. */
#define RDX_COMPAT_AUTH_KEY                    "000000000000000000000000"
#define RDX_COMPAT_WIFI_MAC                    "000000000000"
#define RDX_COMPAT_LABEL_SN                    "0000000000000000"

#endif
