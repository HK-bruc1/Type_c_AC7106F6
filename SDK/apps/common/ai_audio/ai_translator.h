#ifndef __AI_TRANSLATOR__
#define __AI_TRANSLATOR__

#include "typedef.h"
#include "ai_audio_common.h"
#include "ai_recorder.h"
#include "ai_player.h"

//mode info
enum {
    AI_TRANSLATOR_MODE_IDLE,
    AI_TRANSLATOR_MODE_RECORD,
    AI_TRANSLATOR_MODE_RECORD_TRANSLATION,
    AI_TRANSLATOR_MODE_CALL_TRANSLATION,
    AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC,
    AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC,
    AI_TRANSLATOR_MODE_A2DP_TRANSLATION,
    AI_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION
};

enum {
    AI_TRANS_SET_MODE_STATUS_SUCC = 0,
    AI_TRANS_SET_MODE_STATUS_IN_MODE,
    AI_TRANS_SET_MODE_STATUS_INVALID_PARAM,
    AI_TRANS_SET_MODE_STATUS_IN_CALL,
    AI_TRANS_SET_MODE_STATUS_IN_A2DP,
    AI_TRANS_SET_MODE_STATUS_BUSY,
    AI_TRANS_SET_MODE_STATUS_FAIL
};

struct ai_trans_mode {
    u8 mode;            //模式，参考 AI_TRANSLATOR_MODE_RECORD
    u8 ch;              //声道，1 单声道
    u32 coding_type;    //编码类型，参考 AUDIO_CODING_OPUS
    u32 sr;             //采样率
} ;

struct ai_translator_ops {
    //协议层通知APP切换模式
    int (*translator_inform_mode_info)(struct ai_trans_mode *ai_minfo);
};

struct ai_translator_ops_hub {
    struct ai_player_ops player_ops;
    struct ai_recorder_ops recorder_ops;
    struct ai_translator_ops trans_ops;
};


/** @brief      初始化AI翻译
  * @param[in]  ops_hub: translator, player, recorder对接协议层的回调函数
  * @return     none
  * @note
  */
void ai_translator_init(const struct ai_translator_ops_hub *ops_hub);
/** @brief      注销AI翻译
  * @return     none
  * @note
  */
void ai_translator_deinit();
/** @brief      设置AI翻译模式
  * @param[in]  m_param: 模式信息，m_param->mode参考 AI_TRANSLATOR_MODE_RECORD
  * @return     = 0 成功; < 0 失败，参考 -AI_TRANS_SET_MODE_STATUS_FAIL
  * @note
  */
int ai_translator_set_mode(struct ai_trans_mode *m_param);
/** @brief      获取当前AI翻译模式
  * @param[out] minfo: 获取到的模式信息
  * @return     none
  * @note
  */
void ai_translator_get_mode_info(struct ai_trans_mode *minfo);

#endif
