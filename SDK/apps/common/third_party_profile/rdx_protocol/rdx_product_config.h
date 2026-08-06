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
#define RDX_MVP0_PROTOCOL_VERSION              0x01
#define RDX_MVP0_BATTERY_LEVEL                 100
#define RDX_MVP0_ADV_INTERVAL                  160

/* Development-only identity placeholders. Never put production keys here. */
#define RDX_MVP0_TEST_AUTH_KEY                 "000000000000000000000000"
#define RDX_MVP0_TEST_LABEL_SN                 "0000000000000000"

#endif
