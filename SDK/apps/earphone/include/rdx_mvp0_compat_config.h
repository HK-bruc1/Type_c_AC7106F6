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
#define RDX_COMPAT_SELF_MARK                   "NV"

#define RDX_COMPAT_PRODUCT_IS_CHARGE_CASE      1
#define RDX_COMPAT_BOUND_STATE                 0
#define RDX_COMPAT_DEVICE_ABILITY              \
    (RDX_ABILITY_RTC                          | \
     RDX_ABILITY_CONFERENCE_RECORDING         | \
     RDX_ABILITY_LOCAL_STORAGE                | \
     RDX_ABILITY_RECORD_PAUSE_RESUME          | \
     RDX_ABILITY_RECORD_MARK                  | \
     RDX_ABILITY_FLASHNOTE)
#define RDX_COMPAT_INCLUDE_PROTOCOL_VERSION    1
#define RDX_COMPAT_PROTOCOL_VERSION            0x18
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
