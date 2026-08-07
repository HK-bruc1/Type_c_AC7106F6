#ifndef RDX_PRODUCT_CONFIG_H
#define RDX_PRODUCT_CONFIG_H

/*
 * MVP0 temporarily mirrors the selected 701 reference product so the BLE
 * transport can be validated with the same APP. Replace these values only
 * after a Golden Trace and the target product identity have been frozen.
 */
#define RDX_MVP0_LOCAL_NAME                    "Zenchord Case 0000"
#define RDX_MVP0_PRODUCT_CODE                  "603"
#define RDX_MVP0_FACTORY_CODE                  "ZENCHD"
#define RDX_MVP0_PRODUCT_TYPE                  "E1"
#define RDX_MVP0_SELF_MARK                     "NV"

#define RDX_MVP0_PRODUCT_IS_CHARGE_CASE        1
#define RDX_MVP0_BOUND_STATE                   0
#define RDX_MVP0_DEVICE_ABILITY                0x00000022u
#define RDX_MVP0_INCLUDE_PROTOCOL_VERSION      1
#define RDX_MVP0_PROTOCOL_VERSION              0x18
#define RDX_MVP0_BATTERY_LEVEL                 100
#define RDX_MVP0_ADV_INTERVAL                  160

#define RDX_MVP0_FIRMWARE_VERSION              "1.0.0"
#define RDX_MVP0_HARDWARE_VERSION              "0.0.1"

/* Official Zenchord compatibility set; the MAC-limited test key is excluded. */
#define RDX_MVP0_APP_KEY_PRIMARY               "14F6F7A1508E4155"
#define RDX_MVP0_APP_KEY_COMPAT_1              "385FA36EC106DE3D"
#define RDX_MVP0_APP_KEY_COMPAT_2              "7C0BAF778B727175"
#define RDX_MVP0_APP_KEY_LIST                  \
    RDX_MVP0_APP_KEY_PRIMARY,                  \
    RDX_MVP0_APP_KEY_COMPAT_1,                 \
    RDX_MVP0_APP_KEY_COMPAT_2

/* Development-only identity placeholders. Never put production keys here. */
#define RDX_MVP0_TEST_AUTH_KEY                 "000000000000000000000000"
#define RDX_MVP0_TEST_LABEL_SN                 "0000000000000000"

#endif
