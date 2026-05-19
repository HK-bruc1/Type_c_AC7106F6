#include "system/includes.h"
#include "sdk_config.h"
// #include "earphone.h"
#include "app_main.h"
#include "btstack/third_party/app_protocol_event.h"
#include "multi_protocol_main.h"
#include "app_ble_spp_api.h"
#include "realme_platform_api.h"
#include "realme_config.h"


#if (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)

#if (!CONFIG_DOUBLE_BANK_ENABLE) || (!CONFIG_ONLY_GRENERATE_ALIGN_4K_CODE)
#ERROR  "realme OTA 需要开启这些功能"
#endif

// Bit 0 Read Remote Version 读取设备版本信息功能
// Bit 1 Battery Level reporting 读取设备的电量信息
// Bit 2 OTA Upgrading 设备 OTA upgrading 功能
// Bit 3 Key Functions Map 配置设备按键功能
// Bit 4 Earbuds Status Reporting 设备佩戴状态
// Bit 5 Earbud Finding 耳机查找功能
// Bit 6 Free Music 随心听歌功能
// Bit 7 Switch Feature 开关相应功能
const u8 realme_capabilities_0 = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(5) | BIT(7);


// Bit 0 Set Noise Reduction 降噪相关功能
// Bit 1 Detect compactness 检测贴合度功能
// Bit 2 EQ Setting 音频 EQ 相关功能
// Bit 3 High Volume Gain 音量等级相关功能
// Bit 4 Is Support MultiConnect 耳机是否支持抢连，该位值为 1 时表示耳机支持抢连，在铁三角一期联动时（比如播放音乐和通话）抢连不需要相关设备先断开
// Bit 5 Set Related Device Information铁三角一期联动功能
// Bit 6 Sync host UTC time 将 host 的 UTC 时间同步给耳机
// Bit 7 Set Auto Power Off Time 设置自动关机时长
const u8 realme_capabilities_1 = BIT(2) | BIT(3);


// Bit 0 TWS mode 音箱的聚会模式（TWS 模式）
// Bit 1 Get Codec Type 获取耳机当前使用的编码方式
// Bit 2 Resotre Factory Setting 进入恢复出厂设置
// Bit 3 Hearing Enhancement 听力增强(个性化听感)功能（即根据用户听力损伤情况进行补偿）
// Bit 4 Take Photo 耳机拍照功能
// Bit 5 Zen Mode 耳机禅定功能
// Bit 6 Concurrent register Notification 耳机支持并行注册多个 notification event
// Bit 7 Debug Command 耳机 debug 相关功能
const u8 realme_capabilities_2 = BIT(7);


// Bit 0 Remote Firmware Information 耳机固件相关信息
// Bit 1 Device Restore Information 耳机存储相关信息
// Bit 2 Personalized Noise Reduction 个性化降噪相关信息
// Bit 3 Capabilites Or Status 设备之间同步支持能力和状态，用于三角联动
// Bit 4 某种原因该 bit 位弃用
// Bit 5 Multi Connection 一拖二相关功能
// Bit 6 Earphone Scan Test 耳机扫描相关功能
// Bit 7 Explore Earphone Controls 探索耳机操控功能
const u8 realme_capabilities_3 = BIT(5);


// Bit 0 Get Language Type 获取耳机语言类型，进而判断耳机内外销
// Bit 1 Earbuds Tone Information 耳机提示音功能
// Bit 2 Personalized EQ 自定义 EQ 功能
// Bit 3 Host Capabilites 获取 host 支持的 Capabilities
// Bit 4 Recovery Time For Free_To_Pick 免摘对话的恢复时间
// Bit 5 Switch Codec and hires 切换耳机编码方式和 hires 开关
// Bit 6 Bass Engine 低音引擎功能
// Bit 7 AccountKey Information 账号无缝切换相关功能
const u8 realme_capabilities_4 = BIT(0) | BIT(2) | BIT(6);


// Bit 0 Spine Related Information 颈椎健康相关功能
// Bit 1 LDAC support Multi-link 表示支持切换编码时(比如 LDAC)耳机不需要关闭一拖二
// Bit 2 Multi_Connect Switch and Reset 表示支持开关一拖二时耳机不需要重启
// Bit 3 Spatial Audio of Device 表示耳机端支持空间音频算法。
// Bit 4 Game EQ 表示支持游戏 EQ
// Bit 5 Smart Volume 表示支持智能音量(此处只有开关相关指令)
// Bit 6 Used Time 累计使用时长
// Bit 7 Spatial Audio Mode Switch 表示支持空间音频模式切换
const u8 realme_capabilities_5 = 0;


// Bit 0 某种原因该 bit 位弃用
// Bit 1 Game Spatial Sound 表示支持游戏音
// Bit 2 Need host to connect 表示在切换编码或者开关 hires 断连后需要手机主动发起回连
// Bit 3 Set Action For LEA 表示耳机支持 host 对其进行相关 LEA 模式下操作
// Bit 4 Need to re-bond for LEA 表示耳机支持 LEA 后需要手机重新配对连接
// Bit 5 Send tips 表示耳机支持给 host 发送 tips
const u8 realme_capabilities_6 = 0;


const u8 realme_notification_capabilities[] = { \
                                                OPPO_EVENT_CODE_BATTERY_CHANGE, \
                                                OPPO_EVENT_CODE_EARBUDS_CHANGE, \
                                                OPPO_EVENT_CODE_GAME_MODE_CHANGE, \
                                                OPPO_EVENT_CODE_CONN_DEV_INFO, \
                                                OPPO_EVENT_CODE_MULTI_CONN_STATE_CHANGE, \
                                                OPPO_EVENT_CODE_DEVICES_EXCEPTION_INFO, \
                                                OPPO_EVENT_CODE_DEVICE_BURIED_POINT, \
                                              };
const u8 realme_notification_capabilities_len = 7;


// 设备的厂商标识
const u16 realme_vendor_vid = 0x05D6;


// 产品颜色 ID
const u8 realme_color_id = 2;

// device id
const u8 realme_device_id[3] = {0x12, 0x0C, 0x05};


// 0:非 TWS 设备  1:TWS设备
#if TCFG_USER_TWS_ENABLE
const u16 realme_device_type = 1;
#else
const u16 realme_device_type = 0;
#endif

const char realme_product_id[] = "0000\0";
const char realme_protocol_version[] = "001\0";
const u8 realme_remote_version_num = 3;
const char realme_remote_version[] = "1,2,1.1.0.1,2,2,1.1.0.1,3,2,1.1.0.1\0";

#endif



