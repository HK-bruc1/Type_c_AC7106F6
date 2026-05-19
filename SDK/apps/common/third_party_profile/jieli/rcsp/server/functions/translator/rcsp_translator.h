#ifndef __RCSP_TRANSLATOR__
#define __RCSP_TRANSLATOR__

#include "typedef.h"
#include "generic/list.h"

//op
#define RCSP_TRANSLATOR_OP_GET_MODE_INFO                        0x00
#define RCSP_TRANSLATOR_OP_SET_MODE                             0x01
#define RCSP_TRANSLATOR_OP_INFORM_MODE_INFO                     0x02
#define RCSP_TRANSLATOR_OP_SEND_AUDIO_DATA                      0x03
#define RCSP_TRANSLATOR_OP_GET_CACHE_FREE_SIZE                  0x04
#define RCSP_TRANSLATOR_OP_INFORM_CACHE_FREE_SIZE               0x05

//mode info
#define RCSP_TRANSLATOR_MODE_IDLE                               0x00
#define RCSP_TRANSLATOR_MODE_RECORD                             0x01
#define RCSP_TRANSLATOR_MODE_RECORD_TRANSLATION                 0x02
#define RCSP_TRANSLATOR_MODE_CALL_TRANSLATION                   0x03
#define RCSP_TRANSLATOR_MODE_A2DP_TRANSLATION                   0x04
#define RCSP_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION           0x05
#define RCSP_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC        0x06
#define RCSP_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC             0x07

//encode type
#define RCSP_TRANSLATOR_ENCODE_TYPE_PCM                         0x00
#define RCSP_TRANSLATOR_ENCODE_TYPE_SPEEX                       0x01
#define RCSP_TRANSLATOR_ENCODE_TYPE_OPUS                        0x02
#define RCSP_TRANSLATOR_ENCODE_TYPE_MSBC                        0x03
#define RCSP_TRANSLATOR_ENCODE_TYPE_JLA_V2                      0x04

//audio source
#define RCSP_TRANSLATOR_AUDIO_SOURCE_FILE                       0x00
#define RCSP_TRANSLATOR_AUDIO_SOURCE_DEV_MIC                    0x01
#define RCSP_TRANSLATOR_AUDIO_SOURCE_PHONE_MIC                  0x02
#define RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_UPSTREAM              0x03
#define RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_DOWNSTREAM            0x04
#define RCSP_TRANSLATOR_AUDIO_SOURCE_MSBC                       0x05
#define RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_MIX                   0x06
#pragma pack(1)
//mode info
struct translator_mode_info {
    u8 mode;
    u8 encode;  //编码类型
    u8 ch;  //声道
    u32 sr;  //采样率
};

//opCode 0x34 op 03音频数据格式
struct translator_op_03_audio_format {
    u8 source;  //来源
    u8 encode;  //编码类型
    u8 count;  //计数，倒序，计数到0为结束包
    u16 crc;  //音频数据的crc
    u16 len;  //音频数据长度
    //u8 buf[0];
};
#pragma pack()


/** @brief      初始化RCSP翻译，rcsp协议层连上的时候调用
  * @return     none
  * @note
  */
void JL_rcsp_translator_init();
/** @brief      注销RCSP翻译，rcsp协议层断开的时候调用
  * @return     none
  * @note
  */
void JL_rcsp_translator_deinit();
/** @brief      RCSP翻译响应APP命令，执行相应操作
  * @param[in]  priv: rcsp句柄
  * @param[in]  OpCode: OpCode字段
  * @param[in]  OpCode_SN: OpCode_SN字段
  * @param[in]  data: 接收到的数据
  * @param[in]  len: 数据长度
  * @param[in]  ble_con_handle: ble句柄
  * @param[in]  spp_remote_addr: spp地址
  * @return     = 0 成功; < 0 失败
  * @note
  */
int JL_rcsp_translator_functions(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);
/** @brief      RCSP翻译播放是否走opus解码通道
  * @return     1 走opus解码通路，0 走a2dp通路
  * @note
  */
int JL_rcsp_translator_whether_play_by_ai_rx();
/** @brief      RCSP音视频翻译是否播报翻译后语音
  * @return     1 播报，0 不播报
  * @note
  */
int JL_rcsp_translator_whether_a2dp_translate_play();

#endif
