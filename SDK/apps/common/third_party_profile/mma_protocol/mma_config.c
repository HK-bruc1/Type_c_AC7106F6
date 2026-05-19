#include "mma_config.h"


/************** 支持的 freature 配置 ***********/

// 是否支持升级
const bool xm_feature_upgrade = 1;
// 是否支持耳机防丢
const bool xm_feature_headset_anti_lost = 1;
// 是否支持入耳检测
const bool xm_feature_ear_detection = 1;
// 音频模式
const bool xm_feature_audio_mode = 1;
// 单耳按键
const bool xm_feature_single_ear_mode = 1;
// 自动接听来电
const bool xm_feature_auto_ack = 1;
// 多点连接
const bool xm_feature_multi_connect = 0;
// 贴合度
const bool xm_feature_compactness = 0;
// 是否支持双击左耳
const bool xm_feature_double_click_left_headset = 1;
// 是否支持双击右耳
const bool xm_feature_double_click_right_headset = 1;
// 是否支持三击左耳
const bool xm_feature_triple_click_left_headset = 1;
// 是否支持三击右耳
const bool xm_feature_triple_click_right_headset = 1;
// 是否支持长按左耳
const bool xm_feature_long_press_left_headset = 1;
// 是否支持长按右耳
const bool xm_feature_long_press_right_headset = 1;
// 重命名
const bool xm_feature_rename_headset = 1;
// 是否支持静默升级
const bool xm_feature_silent_upgrade = 1;
// 是否支持降噪
const bool xm_feature_denoise = 0;
// 是否支持声纹识别
const bool xm_feature_voiceprint_recognition = 0;
// 是否支持游戏模式
const bool xm_feature_game_mode = 1;
// 是否支持语音训练
const bool xm_feature_voice_command_training = 0;
// 是否支持均衡器
const bool xm_feature_equalizer = 1;
// 是否支持陀螺仪
const bool xm_feature_gyroscope = 0;
// 是否支持噪音识别
const bool xm_feature_noise_identification = 0;
// LC3与多点连接是否互斥
const bool xm_feature_lc3_multi_connect_mutually_exclusive = 0;


const u16 mma_service_uuid  = XM_SERVICE_UUID;
const u16 mma_company_id    = XM_COMPANY_ID;
const u16 mma_vendor_id     = XM_VENDOR_ID;
const u16 mma_product_id    = XM_PRODUCT_ID;
const u16 mma_local_version = XM_VERSION;

const u8  xm_at_version     = XM_AT_VERSION;
const u8  xm_at_adv_porfile_type = XM_AT_ADV_PORFILE_TYPE;
const u8  xm_at_id_type     = XM_AT_ID_TYPE;
const u16 xm_fireware_version = XM_FIREWARE_VERSION;
const u8  xm_majorid        = XM_MAJORID;
const u8  xm_minorid        = XM_MINORID;
const u8  xm_majorid_v2     = XM_MAJORID_V2;
const u16 xm_minorid_v2     = XM_MINORID_V2;

const u16 xm_xiaoai_pid     = XIAO_AI_PID;
const u16 xm_xiaoai_vid     = XIAO_AI_VID;

/************** MMA lib use ***********/
u8 user_at_cmd_send_support = 1;
u8 xm_dual_mode = 0;


