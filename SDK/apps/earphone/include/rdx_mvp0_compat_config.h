#ifndef RDX_MVP0_COMPAT_CONFIG_H
#define RDX_MVP0_COMPAT_CONFIG_H

#include "rdx_protocol/rdx_protocol_defs.h"

/*
 * MVP0 阶段暂时沿用选定的 701 参考产品身份，以便使用同一个 APP
 * 验证 BLE 通信链路。只有在 Golden Trace 和目标产品身份均已冻结后，
 * 才允许替换以下配置值。
 */
#define RDX_COMPAT_LOCAL_NAME                  "IKKO"
#define RDX_COMPAT_PRODUCT_CODE                "604"
#define RDX_COMPAT_FACTORY_CODE                "IKKOAI"
#define RDX_COMPAT_PRODUCT_TYPE                "K1"
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

/*
 * librdxApp.a 中按 app_select 选择的具名 AppKey 字典。
 * 字典项只用于记录兼容关系；只有 RDX_COMPAT_APP_KEY_LIST 中列出的项
 * 才会进入当前产品的运行时认证白名单。
 */
#define RDX_COMPAT_APP_KEY_NINGQU              "1664D1BBBD60B680"
#define RDX_COMPAT_APP_KEY_NOTTA               "385FA36EC106DE3D"
#define RDX_COMPAT_APP_KEY_TINGNAO             "BF73A44128EAF415"
#define RDX_COMPAT_APP_KEY_JMEASY              "2C1DF90FAA0EEC7F"
#define RDX_COMPAT_APP_KEY_SHENGLANG           "AABD6D05ACE276AA"
#define RDX_COMPAT_APP_KEY_AITIR               "31DD62DF76C3FFF5"
#define RDX_COMPAT_APP_KEY_YYS                 "8FF13D95400BD06D"
#define RDX_COMPAT_APP_KEY_LYNSE               "E414D702EE29ED28"
#define RDX_COMPAT_APP_KEY_TURING              "7202267367A07955"
#define RDX_COMPAT_APP_KEY_RAYCON              "945C69D689217345"
#define RDX_COMPAT_APP_KEY_CDJY                "F4C062DAEA418E89"
#define RDX_COMPAT_APP_KEY_BRANDWORKS          "DEAF5A76F7D60500"
#define RDX_COMPAT_APP_KEY_FINDAI              "25BE1A26D3FD78C8"
#define RDX_COMPAT_APP_KEY_BEANSTALK           "524A6C121EF8075C"
#define RDX_COMPAT_APP_KEY_DEEPMINER           "BC0C3E9E5C687B53"
#define RDX_COMPAT_APP_KEY_TTEASY              "B07BEF6D8A6A64CE"
#define RDX_COMPAT_APP_KEY_AISPEECH            "2F50F0235583C3C8"
#define RDX_COMPAT_APP_KEY_SHUGUO              "45CC3F511B91597F"
#define RDX_COMPAT_APP_KEY_VASCO               "CFF59857ADCF92C3"
#define RDX_COMPAT_APP_KEY_ABC                 "5D4E3FB857EACF2B"
#define RDX_COMPAT_APP_KEY_MLAMPWXB            "0E01F4429B589B74"
#define RDX_COMPAT_APP_KEY_CUSTOM_TEST         "93C0C31635436482"
#define RDX_COMPAT_APP_KEY_SAI                 "A8D901B704A93F1A"
#define RDX_COMPAT_APP_KEY_IKKO                "72231C5642E6E60B"

/* 旧测试 AppKey；另一个旧配置项对应的 APP 名称尚未确认。 */
#define RDX_COMPAT_APP_KEY_PRIMARY             "14F6F7A1508E4155"
#define RDX_COMPAT_APP_KEY_2                   "7C0BAF778B727175"

/* 运行时认证白名单；所有需要启用的 AppKey 均在此独立列出。 */
#define RDX_COMPAT_APP_KEY_LIST                \
    RDX_COMPAT_APP_KEY_NINGQU,                 \
    RDX_COMPAT_APP_KEY_NOTTA,                  \
    RDX_COMPAT_APP_KEY_TINGNAO,                \
    RDX_COMPAT_APP_KEY_JMEASY,                 \
    RDX_COMPAT_APP_KEY_SHENGLANG,              \
    RDX_COMPAT_APP_KEY_AITIR,                  \
    RDX_COMPAT_APP_KEY_YYS,                    \
    RDX_COMPAT_APP_KEY_LYNSE,                  \
    RDX_COMPAT_APP_KEY_TURING,                 \
    RDX_COMPAT_APP_KEY_RAYCON,                 \
    RDX_COMPAT_APP_KEY_CDJY,                   \
    RDX_COMPAT_APP_KEY_BRANDWORKS,             \
    RDX_COMPAT_APP_KEY_FINDAI,                 \
    RDX_COMPAT_APP_KEY_BEANSTALK,              \
    RDX_COMPAT_APP_KEY_DEEPMINER,              \
    RDX_COMPAT_APP_KEY_TTEASY,                 \
    RDX_COMPAT_APP_KEY_AISPEECH,               \
    RDX_COMPAT_APP_KEY_SHUGUO,                 \
    RDX_COMPAT_APP_KEY_VASCO,                  \
    RDX_COMPAT_APP_KEY_ABC,                    \
    RDX_COMPAT_APP_KEY_MLAMPWXB,               \
    RDX_COMPAT_APP_KEY_CUSTOM_TEST,            \
    RDX_COMPAT_APP_KEY_SAI,                    \
    RDX_COMPAT_APP_KEY_IKKO,                   \
    RDX_COMPAT_APP_KEY_PRIMARY,                \
    RDX_COMPAT_APP_KEY_2

/* 仅供开发阶段使用的身份占位值，严禁在此填写量产密钥。 */
#define RDX_COMPAT_AUTH_KEY                    "000000000000000000000000"
#define RDX_COMPAT_WIFI_MAC                    "000000000000"
#define RDX_COMPAT_LABEL_SN                    "0000000000000000"

#endif
