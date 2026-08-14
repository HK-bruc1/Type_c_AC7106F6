#ifndef RDX_MVP0_COMPAT_CONFIG_H
#define RDX_MVP0_COMPAT_CONFIG_H

#include "rdx_protocol/rdx_protocol_defs.h"

/*
 * MVP0 阶段暂时沿用选定的 701 参考产品身份，以便使用同一个 APP
 * 验证 BLE 通信链路。只有在 Golden Trace 和目标产品身份均已冻结后，
 * 才允许替换以下配置值。
 */
/* Sole factory/default BLE name; VM stores only an optional user override. */
#define RDX_COMPAT_LOCAL_NAME                  "Zenchord Case 0000"
#define RDX_COMPAT_PRODUCT_CODE                "603"
#define RDX_COMPAT_FACTORY_CODE                "ZENCHD"
#define RDX_COMPAT_PRODUCT_TYPE                "E1"
/*
 * Manufacturer Data 末尾携带的固定 2 字节身份尾标。
 * 设备端不解析该字段，但 APP 可能使用它进行设备发现或兼容性匹配，
 * 因此它不是无效的占位字段。未确认 APP/协议约定前，不得删除或修改。
 * 该字段不是颜色码、Device Ability 功能开关，也不是录音标记。
 */
#define RDX_COMPAT_SELF_MARK                   "NV"

/*
 * RDX Identity 特征值的线上数据格式。
 * 这些配置仅选择 APP 读取的认证/身份数据编码方式，不用于声明产品类型，
 * 也不会启用充电、存储、录音或任何 Device Ability 功能。
 */
#define RDX_IDENTITY_FORMAT_AUTH_KEY_ONLY      1
#define RDX_IDENTITY_FORMAT_CASE_COMPOSITE     2

/* 当前产品使用 95 字节的 "C,..." 仓聚合身份格式。 */
#define RDX_COMPAT_IDENTITY_FORMAT             RDX_IDENTITY_FORMAT_CASE_COMPOSITE

/*
 * 产品功能只允许通过 Device Ability 统一配置。
 * rdx_integration_config.h 会根据该掩码派生所有内部模块的编译门控；
 * 不要再为单项 RDX 能力增加平行的产品功能开关。
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

/* Zenchord 官方兼容密钥集合，不包含限制 MAC 地址的测试密钥。 */
#define RDX_COMPAT_APP_KEY_PRIMARY             "14F6F7A1508E4155"
#define RDX_COMPAT_APP_KEY_1                   "385FA36EC106DE3D"
#define RDX_COMPAT_APP_KEY_2                   "7C0BAF778B727175"
#define RDX_COMPAT_APP_KEY_LIST                \
    RDX_COMPAT_APP_KEY_PRIMARY,                \
    RDX_COMPAT_APP_KEY_1,                      \
    RDX_COMPAT_APP_KEY_2

/* 仅供开发阶段使用的身份占位值，严禁在此填写量产密钥。 */
#define RDX_COMPAT_AUTH_KEY                    "000000000000000000000000"
#define RDX_COMPAT_WIFI_MAC                    "000000000000"
#define RDX_COMPAT_LABEL_SN                    "0000000000000000"

#endif
