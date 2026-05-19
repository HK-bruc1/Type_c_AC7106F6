#ifndef _MMA_CONFIG_H_
#define _MMA_CONFIG_H_

#include "system/includes.h"


/************** 一些产品固定信息配置 ***********/

#define XM_SERVICE_UUID      (0x0000)
#define XM_COMPANY_ID        (0x0000)
#define XM_VENDOR_ID         (0x0)
#define XM_PRODUCT_ID        (0x0)
#define XM_VERSION           (0x0)

#define XM_AT_VERSION               (0X00)    // AT指令协议的版本号
#define XM_AT_ADV_PORFILE_TYPE      (0X00)    // 小米快连广播类型 0:MIUI  1:MIOT  2:Fast Pair
#define XM_AT_ID_TYPE               (0X00)    // 当前Major ID和Minor ID所属的type

#define XM_FIREWARE_VERSION  (0x0000)
#define XM_MAJORID           (0x00)
#define XM_MINORID           (0x00)
#define XM_MAJORID_V2        (0x00)
#define XM_MINORID_V2        (0x0000)
#define XIAO_AI_PID          (0x0000)
#define XIAO_AI_VID          (0x0000)


/************** 支持的 freature 配置 ***********/

// 是否支持升级
extern const bool xm_feature_upgrade;
// 是否支持耳机防丢
extern const bool xm_feature_headset_anti_lost;
// 是否支持入耳检测
extern const bool xm_feature_ear_detection;
// 音频模式
extern const bool xm_feature_audio_mode;
// 单耳按键
extern const bool xm_feature_single_ear_mode;
// 自动接听来电
extern const bool xm_feature_auto_ack;
// 多点连接
extern const bool xm_feature_multi_connect;
// 贴合度
extern const bool xm_feature_compactness;
// 是否支持双击左耳
extern const bool xm_feature_double_click_left_headset;
// 是否支持双击右耳
extern const bool xm_feature_double_click_right_headset;
// 是否支持三击左耳
extern const bool xm_feature_triple_click_left_headset;
// 是否支持三击右耳
extern const bool xm_feature_triple_click_right_headset;
// 是否支持长按左耳
extern const bool xm_feature_long_press_left_headset;
// 是否支持长按右耳
extern const bool xm_feature_long_press_right_headset;
// 重命名
extern const bool xm_feature_rename_headset;
// 是否支持静默升级
extern const bool xm_feature_silent_upgrade;
// 是否支持降噪
extern const bool xm_feature_denoise;
// 是否支持声纹识别
extern const bool xm_feature_voiceprint_recognition;
// 是否支持游戏模式
extern const bool xm_feature_game_mode;
// 是否支持语音训练
extern const bool xm_feature_voice_command_training;
// 是否支持均衡器
extern const bool xm_feature_equalizer;
// 是否支持陀螺仪
extern const bool xm_feature_gyroscope;
// 是否支持噪音识别
extern const bool xm_feature_noise_identification;
// LC3与多点连接是否互斥
extern const bool xm_feature_lc3_multi_connect_mutually_exclusive;

extern const u16 mma_service_uuid;
extern const u16 mma_company_id;
extern const u16 mma_vendor_id;
extern const u16 mma_product_id;
extern const u16 mma_local_version;
extern const u8  xm_at_version;
extern const u8  xm_at_adv_porfile_type;
extern const u8  xm_at_id_type;
extern const u16 xm_fireware_version;
extern const u8  xm_majorid;
extern const u8  xm_minorid;
extern const u8  xm_majorid_v2;
extern const u16 xm_minorid_v2;
extern const u16 xm_xiaoai_pid;
extern const u16 xm_xiaoai_vid;
#endif // _MMA_CONFIG_H_
