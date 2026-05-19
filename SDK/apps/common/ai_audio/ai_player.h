#ifndef __AI_PLAYER__
#define __AI_PLAYER__

#include "typedef.h"
#include "generic/list.h"
#include "ai_audio_common.h"

struct ai_player_ops {
    //协议层通知APP当前播放空闲缓存，给带流控的场景使用，不带流控可不注册
    int (*player_inform_cache_free_size)(u32 free_size, u32 priv);
};

struct ai_player_audio_frame {
    struct list_head entry;
    u8 *buf;
    u32 size;
    u32 timestamp;
};


/** @brief      初始化AI播放
  * @param[in]  ops: 对接协议层的回调函数集
  * @return     none
  * @note
  */
void ai_player_init(const struct ai_player_ops *ops);
/** @brief      注销AI播放
  * @return     none
  * @note
  */
void ai_player_deinit();
/** @brief      获取AI播放当前状态
  * @param[in]  ch: 通道
  * @return     = 1 已打开; = 0 未打开
  * @note
  */
u8 ai_player_get_status(u8 ch);
/** @brief      启动AI播放
  * @param[in]  ch: 通道
  * @param[in]  fmt: 编码格式
  * @param[in]  media_type: 媒体类型 参考 AI_AUDIO_MEDIA_TYPE_VOICE
  * @param[in]  tws_play: 是否tws双端一起控制，tws播放
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_player_start(u8 ch, struct ai_audio_format *fmt, u8 media_type, u8 tws_play);
/** @brief      停止AI播放
  * @param[in]  ch: 通道
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_player_stop(u8 ch);
/** @brief      获取AI播放剩余缓存空间，给带流控的场景使用
  * @param[in]  ch: 通道
  * @param[out] free_size: 返回剩余空间
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_player_get_cache_free_size(u8 ch, u32 *free_size);
/** @brief      注册AI播放解码恢复（给音频流程使用）回调函数
  * @param[in]  ch: 通道
  * @param[in]  priv: 回调函数私有数据
  * @param[in]  func: 回调函数
  * @return     none
  * @note
  */
void ai_player_set_decode_resume_handler(u8 ch, void *priv, void (*func)(void *priv, u8 source));
/** @brief      AI播放获取时间戳
  * @param[in]  ch: 通道
  * @return     时间戳，单位us
  * @note
  */
u32 ai_player_get_timestamp(u8 ch);
/** @brief      AI播放更新时间戳
  * @param[in]  ch: 通道
  * @param[in]  input_size: 传入opus数据长度，用于计算占用了多长时间，更新时间戳
  * @return     none
  * @note
  */
void ai_player_update_timestamp(u8 ch, u32 input_size);
/** @brief      AI播放填入opus数据，供协议层调用
  * @param[in]  ch: 通道
  * @param[in]  buf: 填入opus数据
  * @param[in]  len: 数据长度
  * @param[in]  timestamp: 时间戳，单位us
  * @param[in]  priv: 私有数据，比如用作协议层封装包头的通路标志，
  *             int (*player_inform_cache_free_size)(u32 free_size, u32 priv)
  *             通知协议层时会用到，如果不带流控，可置0
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_player_put_frame(u8 ch, u8 *buf, u32 len, u32 timestamp, u32 priv);
/** @brief      AI播放获取opus音频帧，供音频解码使用（AI_TX节点）
  * @param[in]  ch: 通道
  * @return     音频帧（多帧），带时间戳信息
  * @note
  */
struct ai_player_audio_frame *ai_player_get_frame(u8 ch);
/** @brief      AI播放释放音频帧
  * @param[in]  ch: 通道
  * @param[in]  frame: 要释放的音频帧
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_player_free_frame(u8 ch, struct ai_player_audio_frame *frame);
/** @brief      AI播放设置音量，供协议层调用
  * @param[in]  volume: 音量，范围 0~16
  * @return     = 0 成功; < 0 失败
  * @note
  */
int ai_player_set_play_volume(u16 volume);
/** @brief      AI播放更新音量，供音频流程调用
  * @param[in]  ch: 通道
  * @return     none
  * @note
  */
void ai_player_update_play_volume(u8 ch);

#endif
