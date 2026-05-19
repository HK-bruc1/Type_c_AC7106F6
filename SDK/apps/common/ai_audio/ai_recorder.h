#ifndef __AI_RECORDER__
#define __AI_RECORDER__

#include "typedef.h"
#include "ai_audio_common.h"

struct ai_recorder_ops {
    //协议层发送接口，tws主机
    int (*recorder_send_by_protocol_layer_host)(u8 *buf, u32 len, u32 offset, u32 priv);
    //协议层发送接口，tws从机
    int (*recorder_send_by_protocol_layer_slave)(u8 *buf, u32 len, u32 offset, u32 priv);
    //注册给<智能语音>页面的AI_TX节点的发送接口
    int (*recorder_send_for_dev_mic)(u8 *buf, u32 len);
    //注册给<蓝牙通话>页面esco recorder的AI_TX节点的发送接口
    int (*recorder_send_for_esco_upstream)(u8 *buf, u32 len);
    //注册给<蓝牙通话>页面esco player的AI_TX节点的发送接口
    int (*recorder_send_for_esco_downstream)(u8 *buf, u32 len);
    //注册给<蓝牙通话>页面esco player + recorder混合的AI_TX节点的发送接口
    int (*recorder_send_for_esco_mix)(u8 *buf, u32 len);
    //注册给<蓝牙音乐>页面的AI_TX节点的发送接口
    int (*recorder_send_for_a2dp)(u8 *buf, u32 len);
};

/** @brief      初始化AI录音
  * @param[in]  ops: 对接协议层的回调函数集
  * @return     none
  * @note
  */
void ai_recorder_init(const struct ai_recorder_ops *ops);
/** @brief      注销AI录音
  * @return     none
  * @note
  */
void ai_recorder_deinit();
/** @brief      获取AI录音当前状态
  * @param[in]  ch: 通道
  * @return     = 1 已打开; = 0 未打开
  * @note
  */
u8 ai_recorder_get_status(u8 ch);
/** @brief      启动AI录音
  * @param[in]  ch: 通道
  * @param[in]  fmt: 编码格式
  * @param[in]  media_type: 媒体类型 参考 AI_AUDIO_MEDIA_TYPE_VOICE
  * @param[in]  tws_rec: 是否tws双端一起控制
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_recorder_start(u8 ch, struct ai_audio_format *fmt, u8 media_type, u8 tws_rec);
/** @brief      停止AI录音
  * @param[in]  ch: 通道
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_recorder_stop(u8 ch);
/** @brief      AI录音数据发送
  * @param[in]  ch: 通道
  * @param[in]  buf: 发数的数据
  * @param[in]  len: 数据长度
  * @param[in]  offset: 偏移量，不使用可以置0
  * @param[in]  priv: 私有数据，比如用作协议层封装包头的通路标志，给APP区分是哪一路数据
  * @return     = 0 成功; < 0 失败
  * @note       这个函数是调用协议层回调函数
  *             int (*recorder_send_by_protocol_layer_host)(...)
  *             发送数据的，所以需要初始化时先注册协议层发送函数
  */
int ai_recorder_data_send(u8 ch, u8 *buf, u32 len, u32 offset, u32 priv);

#endif
