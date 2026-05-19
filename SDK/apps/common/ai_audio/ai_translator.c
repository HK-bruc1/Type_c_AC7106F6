#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ai_translator.data.bss")
#pragma data_seg(".ai_translator.data")
#pragma const_seg(".ai_translator.text.const")
#pragma code_seg(".ai_translator.text")
#endif
#include "system/includes.h"
#include "app_config.h"
#include "app_msg.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "esco_recoder.h"
#include "sniff.h"
#include "classic/tws_api.h"
#include "avctp_user.h"
#include "system/task.h"
#include "ai_audio_common.h"
#include "ai_player.h"
#include "ai_recorder.h"
#include "ai_translator.h"

#define LOG_TAG             "[AI_TRANS]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_AI_TRANSLATOR_ENABLE

#define TWS_FUNC_ID_AI_TRANSLATOR           TWS_FUNC_ID('A', 'I', 'T', 'R')


//翻译功能总信息
struct ai_translator_handle {
    u8 init;
    u8 state;
    u8 call_translate_step;
    u8 stop_rec_for_call;
    struct ai_trans_mode mode_info;
    u16 detect_timer_id;
    struct ai_translator_ops ops;
};
static struct ai_translator_handle tlr_hdl;


enum {
    TWS_TRANS_CMD_SYNC_TLR_INFO,
    TWS_TRANS_CMD_SET_OPS,
};

static int ai_translator_tws_send_cmd(u8 cmd, u8 auxiliary, void *param, u32 len);
extern void set_esco_link_timing2_interval(u8 esco_timing2_interval);


void ai_translator_get_mode_info(struct ai_trans_mode *minfo)
{
    memcpy(minfo, &tlr_hdl.mode_info, sizeof(*minfo));
    if (minfo->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION) {
#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
        //通话翻译模式，参数未设置完成时，先不通知应用流程已进入通话翻译
        if (tlr_hdl.call_translate_step) {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
        //tws从机不通知应用流程已进入通话翻译
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
#endif
#if TCFG_AI_PLAYER_ENABLE && TCFG_AI_RECORDER_ENABLE
        if (ai_player_get_status(0) && ai_player_get_status(1) &&
            ai_recorder_get_status(0) && ai_recorder_get_status(1)) {
        } else {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
#endif
    } else if (minfo->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
        //通话翻译模式，参数未设置完成时，先不通知应用流程已进入通话翻译
        if (tlr_hdl.call_translate_step) {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
        //tws从机不通知应用流程已进入通话翻译
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
#endif
#if TCFG_AI_PLAYER_ENABLE && TCFG_AI_RECORDER_ENABLE
        if (ai_player_get_status(0) && ai_player_get_status(1)
#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
            && ai_recorder_get_status(0)
#endif
           ) {
        } else {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
        //printf("st: %d %d %d", ai_player_get_status(0), ai_player_get_status(1), ai_recorder_get_status(0));
#endif
    } else if (minfo->mode == AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
        //tws从机不通知应用流程已进入通话录音
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
#if TCFG_AI_RECORDER_ENABLE
        if (0 == ai_recorder_get_status(0)) {
            minfo->mode = AI_TRANSLATOR_MODE_IDLE;
        }
#endif
    }
}

static void ai_translator_stop_a2dp_confirm(void *arg)
{
    if (0 == a2dp_player_runing()) {
        //TODO  mute住a2dp
        sys_timer_del(tlr_hdl.detect_timer_id);
        tlr_hdl.detect_timer_id = 0;
    }
}

static void ai_translator_stop_a2dp_player()
{
    bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
    if (!tlr_hdl.detect_timer_id) {
        tlr_hdl.detect_timer_id = sys_timer_add(NULL, ai_translator_stop_a2dp_confirm, 50);
    }
}

static int __ai_translator_start_call_translation(u8 mode, struct ai_audio_format *format, u8 *play_ch, u8 *rec_ch)
{
    int ret = 0;
#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
    u8 tws_play = 0;
#else
    u8 tws_play = 1;
#endif

    if (mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION) {
#if TCFG_AI_RECORDER_ENABLE
        ret = ai_recorder_start(0, format, AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM, 0);
        if (ret) {
            ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
            goto __err_exit;
        }
        rec_ch[0] = 0;
#endif
#if TCFG_AI_PLAYER_ENABLE
        ret = ai_player_start(0, format, AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM, tws_play);
        if (ret) {
            ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
            goto __err_exit;
        }
        play_ch[0] = 0;
#endif
#if TCFG_AI_RECORDER_ENABLE
        ret = ai_recorder_start(1, format, AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM, 0);
        if (ret) {
            ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
            goto __err_exit;
        }
        rec_ch[1] = 1;
#endif
#if TCFG_AI_PLAYER_ENABLE
        ret = ai_player_start(1, format, AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM, tws_play);
        if (ret) {
            ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
            goto __err_exit;
        }
        play_ch[1] = 1;
#endif

    } else if (mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
        struct ai_audio_format rec_fmt = {0};
        memcpy(&rec_fmt, format, sizeof(struct ai_audio_format));
        rec_fmt.channel = 2;  //opus立体声编码
        rec_fmt.bit_rate *= 2;  //立体声码率翻倍
#if TCFG_AI_RECORDER_ENABLE
        ret = ai_recorder_start(0, &rec_fmt, AI_AUDIO_MEDIA_TYPE_ESCO_MIX, 0);
        if (ret) {
            ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
            goto __err_exit;
        }
        rec_ch[0] = 0;
#endif
        struct ai_audio_format play_fmt = {0};
        memcpy(&play_fmt, format, sizeof(struct ai_audio_format));
        play_fmt.channel = 1;  //opus单声道解码
#if TCFG_AI_PLAYER_ENABLE
        ret = ai_player_start(0, &play_fmt, AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM, tws_play);
        if (ret) {
            ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
            goto __err_exit;
        }
        play_ch[0] = 0;
#endif
#if TCFG_AI_PLAYER_ENABLE
        ret = ai_player_start(1, &play_fmt, AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM, tws_play);
        if (ret) {
            ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
            goto __err_exit;
        }
        play_ch[1] = 1;
#endif
    }
    //cppcheck-suppress unusedLabel
__err_exit:
    return ret;
}

static int ai_translator_start_call_translation()
{
    int ret = 0;
    struct ai_trans_mode *minfo_param = &tlr_hdl.mode_info;
    u8 rec_ch[2] = {0xff, 0xff};
    u8 play_ch[2] = {0xff, 0xff};
    struct ai_audio_format format = {0};

    if (minfo_param->mode != AI_TRANSLATOR_MODE_CALL_TRANSLATION &&
        minfo_param->mode != AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
        log_error("%s() %d, cur mode: %d\n", __func__, __LINE__, minfo_param->mode);
        return -1;
    }
    //设置编解码格式
    format.coding_type = minfo_param->coding_type;
    format.sample_rate = minfo_param->sr;
    format.bit_rate = 16000;
    format.channel = minfo_param->ch;
    format.frame_dms = 200;

    ret = __ai_translator_start_call_translation(minfo_param->mode, &format, play_ch, rec_ch);
    if (ret) {
        log_error("%s() %d, ret: %d\n", __func__, __LINE__, ret);
        goto __err_exit;
    }

    return ret;

__err_exit:
#if TCFG_AI_RECORDER_ENABLE
    if (rec_ch[1] != 0xff) {
        ai_recorder_stop(rec_ch[1]);
    }
    if (rec_ch[0] != 0xff) {
        ai_recorder_stop(rec_ch[0]);
    }
#endif
#if TCFG_AI_PLAYER_ENABLE
    if (play_ch[1] != 0xff) {
        ai_player_stop(play_ch[1]);
    }
    if (play_ch[0] != 0xff) {
        ai_player_stop(play_ch[0]);
    }
#endif
    return ret;
}

int ai_translator_set_mode(struct ai_trans_mode *m_param)
{
    struct ai_trans_mode *minfo = &tlr_hdl.mode_info;
    int ret = 0;
    u8 rec_ch[2] = {0xff, 0xff};
    u8 play_ch[2] = {0xff, 0xff};
    struct ai_audio_format format = {0};

    if (m_param == NULL) {
        return -AI_TRANS_SET_MODE_STATUS_INVALID_PARAM;
    }
    if (minfo->mode == m_param->mode) {
        return -AI_TRANS_SET_MODE_STATUS_IN_MODE;
    }
    //当前模式未关闭，不允许直接设到其他模式
    if (minfo->mode != AI_TRANSLATOR_MODE_IDLE &&
        m_param->mode != AI_TRANSLATOR_MODE_IDLE) {
        return -AI_TRANS_SET_MODE_STATUS_IN_MODE;
    }
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return -AI_TRANS_SET_MODE_STATUS_FAIL;
    }
    log_info("set translator mode:\n"
             "mode: %d, encode: 0x%x, ch: %d, sr: %d\n",
             m_param->mode,
             m_param->coding_type,
             m_param->ch,
             m_param->sr);

    //设置编解码格式
    format.coding_type = m_param->coding_type;
    format.sample_rate = m_param->sr;
    format.bit_rate = 16000;
    format.channel = m_param->ch;
    format.frame_dms = 200;

    if (m_param->mode == AI_TRANSLATOR_MODE_RECORD) {
        if (a2dp_player_runing()) {
            log_info("a2dp is running\n");
            ai_translator_stop_a2dp_player();
        } else if (esco_player_runing()) {
            log_info("esco is running\n");
#if AI_AUDIO_TRANSLATION_CALL_CONTROL_BY_DEVICE
            ret = -AI_TRANS_SET_MODE_STATUS_IN_CALL;
            goto __err_exit;
#endif
        }
        tlr_hdl.state = 1;

    } else if (m_param->mode == AI_TRANSLATOR_MODE_RECORD_TRANSLATION) {
        if (a2dp_player_runing()) {
            log_info("a2dp is running\n");
            ai_translator_stop_a2dp_player();
        } else if (esco_player_runing()) {
            log_info("esco is running\n");
#if AI_AUDIO_TRANSLATION_CALL_CONTROL_BY_DEVICE
            ret = -AI_TRANS_SET_MODE_STATUS_IN_CALL;
            goto __err_exit;
#endif
        }
        if (tlr_hdl.state == 0) {
#if AI_AUDIO_TRANSLATION_RECV_CHANNEL_ENABLE
#if TCFG_AI_PLAYER_ENABLE
            ret = ai_player_start(0, &format, AI_AUDIO_MEDIA_TYPE_VOICE, 1);
            if (ret) {
                ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
                goto __err_exit;
            }
            play_ch[0] = 0;
#endif
#endif
            tlr_hdl.state = 1;
        }

    } else if (m_param->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION ||
               m_param->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
        if (a2dp_player_runing()) {
            log_info("a2dp is running\n");
            ai_translator_stop_a2dp_player();
        }
        if (tlr_hdl.state == 0) {
            u8 start = 1;
#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                //固定通话翻译左耳播原声，右耳播译文
                if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
                    if (tws_api_get_local_channel() == 'L') {
                        start = 0;
                        tlr_hdl.call_translate_step = 1;
                    } else if (tws_api_get_local_channel() == 'R') {
                        start = 1;
                        tlr_hdl.call_translate_step = 0;
                    }
                } else {
                    start = 1;
                }
            } else {
                start = 0;
            }
#endif
            //cppcheck-suppress knownConditionTrueFalse
            if (start) {
                ret = __ai_translator_start_call_translation(m_param->mode, &format, play_ch, rec_ch);
                if (ret) {
                    goto __err_exit;
                }
            }
            tlr_hdl.state = 1;
        }

    } else if (m_param->mode == AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
        if (a2dp_player_runing()) {
            log_info("a2dp is running\n");
            ai_translator_stop_a2dp_player();
        }
        if (tlr_hdl.state == 0) {
            struct ai_audio_format rec_fmt = {0};
            memcpy(&rec_fmt, &format, sizeof(struct ai_audio_format));
            rec_fmt.channel = 2;  //opus立体声编码
            rec_fmt.bit_rate *= 2;  //立体声码率翻倍
#if TCFG_AI_RECORDER_ENABLE
            ret = ai_recorder_start(0, &rec_fmt, AI_AUDIO_MEDIA_TYPE_ESCO_MIX, 0);
            if (ret) {
                ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
                goto __err_exit;
            }
            rec_ch[0] = 0;
#endif
            tlr_hdl.state = 1;
        }

    } else if (m_param->mode == AI_TRANSLATOR_MODE_A2DP_TRANSLATION) {
        if (esco_player_runing()) {
            log_info("esco is running\n");
#if AI_AUDIO_TRANSLATION_CALL_CONTROL_BY_DEVICE
            ret = -AI_TRANS_SET_MODE_STATUS_IN_CALL;
            goto __err_exit;
#endif
        }
        if (tlr_hdl.state == 0) {
#if TCFG_AI_RECORDER_ENABLE
            ret = ai_recorder_start(0, &format, AI_AUDIO_MEDIA_TYPE_A2DP, 0);
            if (ret) {
                ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
                goto __err_exit;
            }
            rec_ch[0] = 0;
#endif
#if AI_AUDIO_TRANSLATION_RECV_CHANNEL_ENABLE
#if AI_AUDIO_A2DP_TRANSLATION_RECV_ENABLE
#if TCFG_AI_PLAYER_ENABLE
            ret = ai_player_start(0, &format, AI_AUDIO_MEDIA_TYPE_A2DP, 1);
            if (ret) {
                ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
                goto __err_exit;
            }
            play_ch[0] = 0;
#endif
#endif
#endif
            tlr_hdl.state = 1;
        }

    } else if (m_param->mode == AI_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION) {
        if (a2dp_player_runing()) {
            log_info("a2dp is running\n");
            ai_translator_stop_a2dp_player();
        } else if (esco_player_runing()) {
            log_info("esco is running\n");
#if AI_AUDIO_TRANSLATION_CALL_CONTROL_BY_DEVICE
            ret = -AI_TRANS_SET_MODE_STATUS_IN_CALL;
            goto __err_exit;
#endif
        }
        if (tlr_hdl.state == 0) {
#if AI_AUDIO_TRANSLATION_RECV_CHANNEL_ENABLE
#if TCFG_AI_PLAYER_ENABLE
            ret = ai_player_start(0, &format, AI_AUDIO_MEDIA_TYPE_VOICE, 1);
            if (ret) {
                ret = -AI_TRANS_SET_MODE_STATUS_FAIL;
                goto __err_exit;
            }
            play_ch[0] = 0;
#endif
#endif
            tlr_hdl.state = 1;
        }

    } else if (m_param->mode == AI_TRANSLATOR_MODE_IDLE) {
        ret = 0;
        if (minfo->mode == AI_TRANSLATOR_MODE_RECORD) {
            goto __err_exit;
        } else if (minfo->mode == AI_TRANSLATOR_MODE_RECORD_TRANSLATION) {
            play_ch[0] = 0;
            goto __err_exit;
        } else if (minfo->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION ||
                   minfo->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
            rec_ch[0] = 0;
            play_ch[0] = 0;
            rec_ch[1] = 1;
            play_ch[1] = 1;
#if TCFG_TWS_AUTO_ROLE_SWITCH_ENABLE
            tws_api_auto_role_switch_enable();
#endif
            goto __err_exit;
        } else if (minfo->mode == AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
            rec_ch[0] = 0;
            goto __err_exit;
        } else if (minfo->mode == AI_TRANSLATOR_MODE_A2DP_TRANSLATION) {
            rec_ch[0] = 0;
#if AI_AUDIO_A2DP_TRANSLATION_RECV_ENABLE
            play_ch[0] = 0;
#endif
            goto __err_exit;
        } else if (minfo->mode == AI_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION) {
            play_ch[0] = 0;
            goto __err_exit;
        }
    }

    //设模式成功，传入的信息更新到句柄
    memcpy(minfo, m_param, sizeof(struct ai_trans_mode));

    bt_sniff_disable();

    if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            ai_translator_tws_send_cmd(TWS_TRANS_CMD_SYNC_TLR_INFO, 1, &tlr_hdl, sizeof(struct ai_translator_handle));

#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
            if (minfo->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION ||
                minfo->mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
                if (tws_api_get_local_channel() == 'L') {
                    tws_api_role_switch();
                } else if (tws_api_get_local_channel() == 'R') {
                    tws_api_auto_role_switch_disable();
                }
            }
#endif
        }
    }
    return 0;

__err_exit:
#if TCFG_AI_RECORDER_ENABLE
    if (rec_ch[1] != 0xff) {
        ai_recorder_stop(rec_ch[1]);
    }
    if (rec_ch[0] != 0xff) {
        ai_recorder_stop(rec_ch[0]);
    }
#endif
#if TCFG_AI_PLAYER_ENABLE
    if (play_ch[1] != 0xff) {
        ai_player_stop(play_ch[1]);
    }
    if (play_ch[0] != 0xff) {
        ai_player_stop(play_ch[0]);
    }
#endif
    tlr_hdl.state = 0;
    if (ret != 0) {
        log_info("set translator mode fail, ret %d\n", ret);
        minfo->mode = AI_TRANSLATOR_MODE_IDLE;
    } else {
        memcpy(minfo, m_param, sizeof(struct ai_trans_mode));
        //退出模式通知tws从机
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                ai_translator_tws_send_cmd(TWS_TRANS_CMD_SYNC_TLR_INFO, 1, &tlr_hdl, sizeof(struct ai_translator_handle));
            }
        }
    }
    bt_sniff_enable();
    return ret;
}

static void ai_translator_bt_event_close_call_translation(void *arg)
{
    struct ai_trans_mode minfo;
    memcpy(&minfo, &tlr_hdl.mode_info, sizeof(minfo));
    if (minfo.mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION ||
        minfo.mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC ||
        minfo.mode == AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
        minfo.mode = AI_TRANSLATOR_MODE_IDLE;
        if (tlr_hdl.ops.translator_inform_mode_info) {
            tlr_hdl.ops.translator_inform_mode_info(&minfo);
        }
        ai_translator_set_mode(&minfo);
    }
}

static int ai_translator_bt_event_handler(int *event)
{
    struct bt_event *bt = (struct bt_event *)event;
    struct ai_trans_mode minfo;
    //cppcheck-suppress unusedVariable
    struct ai_audio_format format;

    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        //修改成每5个esco通讯周期留出1个空隙给acl通讯用
        set_esco_link_timing2_interval(5 * 12);
        break;
    case BT_STATUS_SCO_CONNECTION_REQ:
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            //来电暂停录音，有可能在模式中，也有可能不在模式中，是按键拉起的录音
            //除了通话翻译，其他情况先暂停录音
            if (tlr_hdl.mode_info.mode != AI_TRANSLATOR_MODE_CALL_TRANSLATION &&
                tlr_hdl.mode_info.mode != AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC &&
                tlr_hdl.mode_info.mode != AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
#if TCFG_AI_RECORDER_ENABLE
                if (ai_recorder_get_status(0)) {
                    ai_recorder_stop(0);
                    tlr_hdl.stop_rec_for_call = 1;
                }
#endif
            }
#if AI_AUDIO_TRANSLATION_CALL_CONTROL_BY_DEVICE
            switch (tlr_hdl.mode_info.mode) {
            case AI_TRANSLATOR_MODE_RECORD:
            case AI_TRANSLATOR_MODE_RECORD_TRANSLATION:
            case AI_TRANSLATOR_MODE_A2DP_TRANSLATION:
            case AI_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION:
                //来电关闭录音（听译、音视频，面对面）翻译模式
                memcpy(&minfo, &tlr_hdl.mode_info, sizeof(minfo));
                minfo.mode = AI_TRANSLATOR_MODE_IDLE;
                //通知app关闭模式
                if (tlr_hdl.ops.translator_inform_mode_info) {
                    tlr_hdl.ops.translator_inform_mode_info(&minfo);
                }
                ai_translator_set_mode(&minfo);
                break;
            }
#endif
        }
        break;
    case BT_STATUS_SCO_DISCON:
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            memcpy(&minfo, &tlr_hdl.mode_info, sizeof(minfo));
            //结束通话恢复录音
            if (minfo.mode != AI_TRANSLATOR_MODE_CALL_TRANSLATION &&
                minfo.mode != AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC &&
                minfo.mode != AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
#if TCFG_AI_RECORDER_ENABLE
                if (tlr_hdl.stop_rec_for_call) {
                    tlr_hdl.stop_rec_for_call = 0;
                    format.coding_type = minfo.coding_type;
                    format.sample_rate = minfo.sr;
                    format.bit_rate = 16000;
                    format.channel = minfo.ch;
                    format.frame_dms = 200;
                    ai_recorder_start(0, &format, AI_AUDIO_MEDIA_TYPE_VOICE, 0);
                }
#endif
            } else {
#if 1/* AI_AUDIO_TRANSLATION_CALL_CONTROL_BY_DEVICE */
                //挂断电话关闭通话翻译模式
                sys_timeout_add(NULL, ai_translator_bt_event_close_call_translation, 600);
#endif
            }
        }
        break;
    }
    return 0;
}

APP_MSG_HANDLER(ai_translator_bt_event) = {
    .owner = 0xff,
    .from = MSG_FROM_BT_STACK,
    .handler = ai_translator_bt_event_handler,
};

static int ai_translator_tws_send_cmd(u8 cmd, u8 auxiliary, void *param, u32 len)
{
    return ai_audio_common_tws_send_cmd(TWS_FUNC_ID_AI_TRANSLATOR, cmd, auxiliary, param, len);
}

static void ai_translator_tws_msg_handler(u8 *_data, u32 len)
{
    int err = 0;
    u8 cmd;
    //u32 param_len;
    u8 *param;
    struct ai_translator_handle tlr_hdl_src;

    cmd = _data[0];
    switch (cmd) {
    case TWS_TRANS_CMD_SET_OPS:
        param = _data + 2;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(&tlr_hdl.ops, param, sizeof(struct ai_translator_ops));
        }
        break;
    case TWS_TRANS_CMD_SYNC_TLR_INFO:
        param = _data + 2;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(&tlr_hdl_src, param, sizeof(struct ai_translator_handle));
            tlr_hdl.state = tlr_hdl_src.state;
            memcpy(&tlr_hdl.mode_info, &tlr_hdl_src.mode_info, sizeof(struct ai_trans_mode));
            tlr_hdl.call_translate_step = tlr_hdl_src.call_translate_step;
            tlr_hdl.stop_rec_for_call = tlr_hdl_src.stop_rec_for_call;

            if (_data[1]) {
                log_info("sync translator mode:\n"
                         "mode: %d, encode: 0x%x, ch: %d, sr: %d\n",
                         tlr_hdl.mode_info.mode,
                         tlr_hdl.mode_info.coding_type,
                         tlr_hdl.mode_info.ch,
                         tlr_hdl.mode_info.sr);
            }
        }
        break;
    }

    free(_data);
}

static void ai_translator_tws_msg_from_sibling(void *_data, u16 len, bool rx)
{
    u8 *rx_data;
    int msg[4];
    if (!rx) {
        //是否需要限制只有rx才能收到消息
        //return;
    }
    rx_data = malloc(len);
    if (!rx_data) {
        return;
    }
    memcpy(rx_data, _data, len);
    msg[0] = (int)ai_translator_tws_msg_handler;
    msg[1] = 2;
    msg[2] = (int)rx_data;
    msg[3] = len;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
    if (err) {
        printf("tws rx post fail\n");
    }
    //cppcheck-suppress memleak
}

REGISTER_TWS_FUNC_STUB(ai_translator_tws_sibling_stub) = {
    .func_id = TWS_FUNC_ID_AI_TRANSLATOR,
    .func = ai_translator_tws_msg_from_sibling,
};


#if TCFG_USER_TWS_ENABLE
static int ai_translator_tws_event_handler(int *_msg)
{
    struct tws_event *evt = (struct tws_event *)_msg;
    int role = evt->args[0];
    int reason = evt->args[2];
    struct ai_trans_mode minfo = {0};
    //cppcheck-suppress unusedVariable
    u8 mode;

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        if (role == TWS_ROLE_MASTER) {
            if (tlr_hdl.init) {
                //连接上tws的时候将主机的状态同步给从机
                ai_translator_tws_send_cmd(TWS_TRANS_CMD_SET_OPS, 0, &tlr_hdl.ops, sizeof(struct ai_translator_ops));
                ai_translator_tws_send_cmd(TWS_TRANS_CMD_SYNC_TLR_INFO, 0, &tlr_hdl, sizeof(struct ai_translator_handle));
            }
        } else {
        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        if (role == TWS_ROLE_MASTER) {
        } else {
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_MASTER) {
#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
            if (tlr_hdl.call_translate_step) {
                tlr_hdl.call_translate_step++;
                if (tlr_hdl.call_translate_step == 2) {
                    tlr_hdl.call_translate_step = 0;
                    int ret = ai_translator_start_call_translation();
                    if (ret) {
                        tlr_hdl.mode_info.mode = AI_TRANSLATOR_MODE_IDLE;
                        tlr_hdl.state = 0;
                        //通知app关闭模式
                        if (tlr_hdl.ops.translator_inform_mode_info) {
                            tlr_hdl.ops.translator_inform_mode_info(&tlr_hdl.mode_info);
                        }
                        ai_translator_set_mode(&minfo);
                    }
                    //同步模式信息到对端
                    ai_translator_tws_send_cmd(TWS_TRANS_CMD_SYNC_TLR_INFO, 0, &tlr_hdl, sizeof(struct ai_translator_handle));
                }
            }
#endif
        } else {
        }
        break;
    }
    return 0;
}

APP_MSG_HANDLER(translator_tws_event) = {
    .owner = 0xff,
    .from = MSG_FROM_TWS,
    .handler = ai_translator_tws_event_handler,
};
#endif

void ai_translator_init(const struct ai_translator_ops_hub *ops_hub)
{
    if (tlr_hdl.init) {
        return;
    }
    log_info("%s(), role: %d\n", __func__, tws_api_get_role());
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    tlr_hdl.state = 0;
    tlr_hdl.mode_info.mode = AI_TRANSLATOR_MODE_IDLE;
    tlr_hdl.mode_info.coding_type = AUDIO_CODING_OPUS;
    tlr_hdl.mode_info.ch = 1;
    tlr_hdl.mode_info.sr = 16000;
    tlr_hdl.call_translate_step = 0;
    if (ops_hub) {
        memcpy(&tlr_hdl.ops, &ops_hub->trans_ops, sizeof(struct ai_translator_ops));
    }
    if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        ai_translator_tws_send_cmd(TWS_TRANS_CMD_SET_OPS, 0, &tlr_hdl.ops, sizeof(struct ai_translator_ops));
        ai_translator_tws_send_cmd(TWS_TRANS_CMD_SYNC_TLR_INFO, 0, &tlr_hdl, sizeof(struct ai_translator_handle));
    }
    tlr_hdl.init = 1;
#if TCFG_AI_PLAYER_ENABLE
    ai_player_init(&ops_hub->player_ops);
#endif
#if TCFG_AI_RECORDER_ENABLE
    ai_recorder_init(&ops_hub->recorder_ops);
#endif
}

void ai_translator_deinit()
{
    struct ai_trans_mode minfo;
    if (!tlr_hdl.init) {
        return;
    }
    log_info("%s(), role: %d\n", __func__, tws_api_get_role());
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    memcpy(&minfo, &tlr_hdl.mode_info, sizeof(struct ai_trans_mode));
    minfo.mode = AI_TRANSLATOR_MODE_IDLE;
    ai_translator_set_mode(&minfo);
    tlr_hdl.init = 0;
#if TCFG_AI_PLAYER_ENABLE
    ai_player_deinit();
#endif
#if TCFG_AI_RECORDER_ENABLE
    ai_recorder_deinit();
#endif
}

#endif
