#ifndef __AI_AUDIO_COMMON__
#define __AI_AUDIO_COMMON__

#include "typedef.h"

//audio type
enum ai_audio_media_type {
    //用于语音，适配<智能语音>页面
    AI_AUDIO_MEDIA_TYPE_VOICE,
    //用于esco recorder，适配<蓝牙通话>页面
    AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM,
    //用于esco player，适配<蓝牙通话>页面
    AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM,
    //用于esco player + recorder混合，适配<蓝牙通话>页面
    AI_AUDIO_MEDIA_TYPE_ESCO_MIX,
    //用于a2dp，适配<蓝牙音乐>页面
    AI_AUDIO_MEDIA_TYPE_A2DP,
};

struct ai_audio_format {
    u32 coding_type;    //编码格式，参考 AUDIO_CODING_OPUS
    u32 sample_rate;    //采样率
    u32 bit_rate;       //码率
    u8  channel;        //声道数，1 单声道
    u16 frame_size;     //帧大小，传参不用填，内部会计算出来
    u16 frame_dms;      //帧间隔，单位100us（eg. 20ms = 20000us，填入200）
};


/** @brief      设置remote地址，用于tws播放
  * @param[in]  spp_remote_addr: spp（经典蓝牙）地址
  * @param[in]  ble_con_hdl: bie句柄（暂不使用，传0）
  * @return     none
  * @note       目前仅支持edr做tws，所以只填入spp_remote_addr
  */
void ai_audio_common_set_remote_bt_addr(u8 *spp_remote_addr, u16 ble_con_hdl);
/** @brief      获取remote地址
  * @return     remote地址
  * @note
  */
u8 *ai_audio_common_get_remote_bt_addr();
/** @brief      同步remote地址到tws对端
  * @return     none
  * @note
  */
void ai_audio_common_sync_remote_bt_addr();
/** @brief      同步信息到tws对端
  * @param[in]  func_id: tws func id
  * @param[in]  cmd: 命令
  * @param[in]  auxiliary: 辅助信息，例如用于传递ch
  * @param[in]  param: 参数
  * @param[in]  len: 参数长度
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_audio_common_tws_send_cmd(u32 func_id, u8 cmd, u8 auxiliary, void *param, u32 len);

#endif
