#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".recp_translator.data.bss")
#pragma data_seg(".recp_translator.data")
#pragma const_seg(".recp_translator.text.const")
#pragma code_seg(".recp_translator.text")
#endif
#include "system/includes.h"
#include "app_config.h"
#include "app_msg.h"
#include "btstack_rcsp_user.h"
#include "rcsp.h"
#include "rcsp_config.h"
#include "rcsp/JL_rcsp_protocol.h"
#include "app_protocol_common.h"
#include "rcsp_translator.h"
#include "sniff.h"
#include "classic/tws_api.h"
#include "system/task.h"
#include "ai_recorder.h"
#include "ai_player.h"
#include "ai_translator.h"

#define RCSP_DEBUG_EN
#ifdef  RCSP_DEBUG_EN
#define rcsp_putchar(x)                	putchar(x)
#define rcsp_printf                    	printf
#define rcsp_put_buf(x,len)				put_buf(x,len)
#else
#define rcsp_putchar(...)
#define rcsp_printf(...)
#define rcsp_put_buf(...)
#endif

#if RCSP_MODE && RCSP_ADV_TRANSLATOR


//翻译功能总信息
struct translator_meta_t {
    u16 ble_con_handle;
    u8 spp_remote_addr[6];
    //struct translator_mode_info mode_info;
};
static struct translator_meta_t tlr_hdl;

//note: 状态用于回复APP，如有更新需要跟APP一同更新
enum {
    TRANS_SET_MODE_STATUS_SUCC = 0,
    TRANS_SET_MODE_STATUS_IN_MODE,
    TRANS_SET_MODE_STATUS_INVALID_PARAM,
    TRANS_SET_MODE_STATUS_IN_CALL,
    TRANS_SET_MODE_STATUS_IN_A2DP,
    TRANS_SET_MODE_STATUS_BUSY,
    TRANS_SET_MODE_STATUS_FAIL
};


static u8 source_to_ch_remap(u8 source)
{
    u8 ch;
    switch (source) {
    case RCSP_TRANSLATOR_AUDIO_SOURCE_DEV_MIC:
    case RCSP_TRANSLATOR_AUDIO_SOURCE_PHONE_MIC:
    case RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_UPSTREAM:
    case RCSP_TRANSLATOR_AUDIO_SOURCE_MSBC:
    case RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_MIX:
        ch = 0;
        break;
    case RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_DOWNSTREAM:
        ch = 1;
        break;
    default:
        ch = 0;
    }
    return ch;
}

static const int mode_remap_table[][2] = {
    RCSP_TRANSLATOR_MODE_IDLE,                      AI_TRANSLATOR_MODE_IDLE,
    RCSP_TRANSLATOR_MODE_RECORD,                    AI_TRANSLATOR_MODE_RECORD,
    RCSP_TRANSLATOR_MODE_RECORD_TRANSLATION,        AI_TRANSLATOR_MODE_RECORD_TRANSLATION,
    RCSP_TRANSLATOR_MODE_CALL_TRANSLATION,          AI_TRANSLATOR_MODE_CALL_TRANSLATION,
    RCSP_TRANSLATOR_MODE_A2DP_TRANSLATION,          AI_TRANSLATOR_MODE_A2DP_TRANSLATION,
    RCSP_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION,  AI_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION,
    RCSP_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC, AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC,
    RCSP_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC,    AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC,
};

static int __mode_a2r_remap(int ai_mode)
{
    int rcsp_mode = RCSP_TRANSLATOR_MODE_IDLE;
    for (int i = 0; i < sizeof(mode_remap_table) / 4 / 2; i++) {
        if (ai_mode == mode_remap_table[i][1]) {
            rcsp_mode = mode_remap_table[i][0];
            break;
        }
    }
    return rcsp_mode;
}

static int __mode_r2a_remap(int rcsp_mode)
{
    int ai_mode = AI_TRANSLATOR_MODE_IDLE;
    for (int i = 0; i < sizeof(mode_remap_table) / 4 / 2; i++) {
        if (rcsp_mode == mode_remap_table[i][0]) {
            ai_mode = mode_remap_table[i][1];
            break;
        }
    }
    return ai_mode;
}

static const int encode_remap_table[][2] = {
    RCSP_TRANSLATOR_ENCODE_TYPE_PCM,        AUDIO_CODING_PCM,
    RCSP_TRANSLATOR_ENCODE_TYPE_SPEEX,      AUDIO_CODING_SPEEX,
    RCSP_TRANSLATOR_ENCODE_TYPE_OPUS,       AUDIO_CODING_OPUS,
    RCSP_TRANSLATOR_ENCODE_TYPE_MSBC,       AUDIO_CODING_MSBC,
    RCSP_TRANSLATOR_ENCODE_TYPE_JLA_V2,     AUDIO_CODING_JLA_V2,
};

static int __encode_a2r_remap(int ai_encode)
{
    int rcsp_encode = RCSP_TRANSLATOR_ENCODE_TYPE_OPUS;
    for (int i = 0; i < sizeof(encode_remap_table) / 4 / 2; i++) {
        if (ai_encode == encode_remap_table[i][1]) {
            rcsp_encode = encode_remap_table[i][0];
            break;
        }
    }
    return rcsp_encode;
}

static int __encode_r2a_remap(int rcsp_encode)
{
    int ai_encode = AUDIO_CODING_OPUS;
    for (int i = 0; i < sizeof(encode_remap_table) / 4 / 2; i++) {
        if (rcsp_encode == encode_remap_table[i][0]) {
            ai_encode = encode_remap_table[i][1];
            break;
        }
    }
    return ai_encode;
}

static const int set_status_remap_table[][2] = {
    TRANS_SET_MODE_STATUS_SUCC,             AI_TRANS_SET_MODE_STATUS_SUCC,
    -TRANS_SET_MODE_STATUS_IN_MODE,         -AI_TRANS_SET_MODE_STATUS_IN_MODE,
    -TRANS_SET_MODE_STATUS_INVALID_PARAM,   -AI_TRANS_SET_MODE_STATUS_INVALID_PARAM,
    -TRANS_SET_MODE_STATUS_IN_CALL,         -AI_TRANS_SET_MODE_STATUS_IN_CALL,
    -TRANS_SET_MODE_STATUS_IN_A2DP,         -AI_TRANS_SET_MODE_STATUS_IN_A2DP,
    -TRANS_SET_MODE_STATUS_BUSY,            -AI_TRANS_SET_MODE_STATUS_BUSY,
    -TRANS_SET_MODE_STATUS_FAIL,            -AI_TRANS_SET_MODE_STATUS_FAIL,
};

static int __set_status_a2r_remap(int ai_set_status)
{
    int rcsp_set_status = TRANS_SET_MODE_STATUS_SUCC;
    for (int i = 0; i < sizeof(set_status_remap_table) / 4 / 2; i++) {
        if (ai_set_status == set_status_remap_table[i][1]) {
            rcsp_set_status = set_status_remap_table[i][0];
            break;
        }
    }
    return rcsp_set_status;
}

static int __set_status_r2a_remap(int rcsp_set_status)
{
    int ai_set_status = AI_TRANS_SET_MODE_STATUS_SUCC;
    for (int i = 0; i < sizeof(set_status_remap_table) / 4 / 2; i++) {
        if (rcsp_set_status == set_status_remap_table[i][0]) {
            ai_set_status = set_status_remap_table[i][1];
            break;
        }
    }
    return ai_set_status;
}

static int translator_send_ch_for_dev_mic(u8 *buf, u32 len)
{
    int ret = 0;
#if TCFG_AI_RECORDER_ENABLE
    u8 ch = source_to_ch_remap(RCSP_TRANSLATOR_AUDIO_SOURCE_DEV_MIC);
    ret = ai_recorder_data_send(ch, buf, len, 0, RCSP_TRANSLATOR_AUDIO_SOURCE_DEV_MIC);
#endif
    return ret;
}

static int translator_send_ch_for_esco_upstream(u8 *buf, u32 len)
{
    int ret = 0;
#if TCFG_AI_RECORDER_ENABLE
    u8 ch = source_to_ch_remap(RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_UPSTREAM);
    ret = ai_recorder_data_send(ch, buf, len, 0, RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_UPSTREAM);
#endif
    return ret;
}

static int translator_send_ch_for_esco_downstream(u8 *buf, u32 len)
{
    int ret = 0;
#if TCFG_AI_RECORDER_ENABLE
    u8 ch = source_to_ch_remap(RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_DOWNSTREAM);
    ret = ai_recorder_data_send(ch, buf, len, 0, RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_DOWNSTREAM);
#endif
    return ret;
}

static int translator_send_ch_for_esco_mix(u8 *buf, u32 len)
{
    int ret = 0;
#if TCFG_AI_RECORDER_ENABLE
    u8 ch = source_to_ch_remap(RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_MIX);
    ret = ai_recorder_data_send(ch, buf, len, 0, RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_MIX);
#endif
    return ret;
}

static int translator_send_ch_for_a2dp(u8 *buf, u32 len)
{
    int ret = 0;
#if TCFG_AI_RECORDER_ENABLE
    u8 ch = source_to_ch_remap(RCSP_TRANSLATOR_AUDIO_SOURCE_MSBC);
    ret = ai_recorder_data_send(ch, buf, len, 0, RCSP_TRANSLATOR_AUDIO_SOURCE_MSBC);
#endif
    return ret;
}

int JL_rcsp_translator_whether_play_by_ai_rx()
{
#if AI_AUDIO_TRANSLATION_RECV_CHANNEL_ENABLE
    return 1;
#else
    return 0;
#endif
}

int JL_rcsp_translator_whether_a2dp_translate_play()
{
#if AI_AUDIO_A2DP_TRANSLATION_RECV_ENABLE
    return 1;
#else
    return 0;
#endif
}

static int _translator_op_get_info(u8 *buf, u32 len)
{
    struct translator_mode_info minfo;
    struct ai_trans_mode ai_minfo;

    ai_translator_get_mode_info(&ai_minfo);
    minfo.mode = __mode_a2r_remap(ai_minfo.mode);
    minfo.encode = __encode_a2r_remap(ai_minfo.coding_type);
    minfo.ch = ai_minfo.ch;
    minfo.sr = ai_minfo.sr;

    buf[0] = minfo.mode;
    buf[1] = minfo.encode;
    buf[2] = minfo.ch;
    buf[3] = (minfo.sr >> 24) & 0xff;
    buf[4] = (minfo.sr >> 16) & 0xff;
    buf[5] = (minfo.sr >> 8) & 0xff;
    buf[6] = (minfo.sr >> 0) & 0xff;
    rcsp_printf("rcsp get translator info:\n"
                "mode: %d, encode: %d, ch: %d, sr: %d\n",
                minfo.mode,
                minfo.encode,
                minfo.ch,
                minfo.sr);
    return 0;
}

static int _translator_op_set_mode(struct translator_mode_info *minfo_src)
{
    int ret = 0;
    struct ai_trans_mode ai_minfo = {0};

    rcsp_printf("rcsp set translator mode:\n"
                "mode: %d, encode: %d, ch: %d, sr: %d\n",
                minfo_src->mode,
                minfo_src->encode,
                minfo_src->ch,
                minfo_src->sr);
    ai_minfo.mode = __mode_r2a_remap(minfo_src->mode);
    ai_minfo.coding_type = __encode_r2a_remap(minfo_src->encode);
    ai_minfo.ch = minfo_src->ch;
    ai_minfo.sr = minfo_src->sr;
    ret = ai_translator_set_mode(&ai_minfo);
    ret = __set_status_a2r_remap(ret);
    if (ret < 0) {
        goto __exit;
    }
__exit:
    return ret;
}

static int _translator_op_record_start()
{
    int ret = 0;
#if TCFG_AI_RECORDER_ENABLE
    struct ai_trans_mode ai_minfo;
    ai_translator_get_mode_info(&ai_minfo);
    struct ai_audio_format format = {0};
    format.coding_type = ai_minfo.coding_type;
    format.sample_rate = ai_minfo.sr;
    format.bit_rate = 16000;
    format.channel = ai_minfo.ch;
    format.frame_dms = 200;
    ret = ai_recorder_start(0, &format, AI_AUDIO_MEDIA_TYPE_VOICE, 0);
#endif
    return ret;
}

static int _translator_op_record_stop()
{
    int ret = 0;
#if TCFG_AI_RECORDER_ENABLE
    ret = ai_recorder_stop(0);
#endif
    return ret;
}

static int _translator_op_inform_mode_info(struct ai_trans_mode *ai_minfo)
{
    int ret;
    u16 len;
    u8 buf[16] = {0};
    struct translator_mode_info minfo;

    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }

    minfo.mode = __mode_a2r_remap(ai_minfo->mode);
    minfo.encode = __encode_a2r_remap(ai_minfo->coding_type);
    minfo.ch = ai_minfo->ch;
    minfo.sr = ai_minfo->sr;
    rcsp_printf("rcsp inform info:\nmode: %d, encode: %d, ch: %d, sr: %d\n",
                minfo.mode,
                minfo.encode,
                minfo.ch,
                minfo.sr);
    len = sizeof(struct translator_mode_info);
    buf[0] = RCSP_TRANSLATOR_OP_INFORM_MODE_INFO;
    buf[1] = len;
    buf[2] = minfo.mode;
    buf[3] = minfo.encode;
    buf[4] = minfo.ch;
    buf[5] = (minfo.sr >> 24) & 0xff;
    buf[6] = (minfo.sr >> 16) & 0xff;
    buf[7] = (minfo.sr >> 8) & 0xff;
    buf[8] = (minfo.sr >> 0) & 0xff;
    len += 2;
    ret = JL_CMD_send(JL_OPCODE_TRANSLATOR, buf, len, JL_NOT_NEED_RESPOND, 0, NULL);
    return ret;
}

static int _translator_op_get_cache_free_size(u8 source, u32 *free_size)
{
    int ret = 0;
#if TCFG_AI_PLAYER_ENABLE
    u8 ch = source_to_ch_remap(source);
    ret = ai_player_get_cache_free_size(ch, free_size);
#endif
    return ret;
}

static int _translator_op_inform_cache_free_size(u32 free_size, u32 priv)
{
    int ret = 0;
    u8 ch;
    u8 source = priv;
    u8 buf[16] = {0};

    ch = source_to_ch_remap(source);
#if AI_AUDIO_RECV_SPEED_DEBUG
    g_printf("wakeup[%d]: %d\n", ch, free_size);
#endif
    buf[0] = RCSP_TRANSLATOR_OP_INFORM_CACHE_FREE_SIZE;
    buf[1] = 5;
    buf[2] = source;
    free_size = READ_BIG_U32(&free_size);
    memcpy(buf + 3, &free_size, 4);
    ret = JL_CMD_send(JL_OPCODE_TRANSLATOR, buf, 7, JL_NOT_NEED_RESPOND, 0, NULL);
    return ret;
}

static int _translator_op_receive_audio_data(u8 *buf, u32 len)
{
    u16 ret = 0;
    u32 audio_len;
    u8 *audio_buf;
    struct translator_op_03_audio_format op03_fmt = {0};
    u16 crc;
    u32 timestamp;

    memcpy(&op03_fmt, buf, sizeof(struct translator_op_03_audio_format));
    //TODO 注意有没有非对齐问题，crc的offset是3。目前校验pass
    op03_fmt.crc = READ_BIG_U16(&op03_fmt.crc);
    op03_fmt.len = READ_BIG_U16(&op03_fmt.len);
    if (len != sizeof(struct translator_op_03_audio_format) + op03_fmt.len) {
        rcsp_printf("trans recv len not match\n");
        return -1;
    }
    crc = CRC16(buf + sizeof(struct translator_op_03_audio_format), op03_fmt.len);
    if (op03_fmt.crc == crc) {
        //rcsp_printf("opCode 0x34, op 0x03 rx crc ok\n");
        audio_len = op03_fmt.len;
        audio_buf = buf + sizeof(struct translator_op_03_audio_format);
        //rcsp_put_buf(audio_buf, audio_len);
#if TCFG_AI_PLAYER_ENABLE
        u8 ch = source_to_ch_remap(op03_fmt.source);
        timestamp = ai_player_get_timestamp(ch);
        ai_player_put_frame(ch, audio_buf, audio_len, timestamp, op03_fmt.source);
        ai_player_update_timestamp(ch, audio_len);
#endif
    } else {
        rcsp_printf("opCode 0x34, op 0x03 rx crc error, %x - %x\n", op03_fmt.crc, crc);
        ret = -1;
    }
    return ret;
}

static int _translator_op_send_audio_data(u8 *buf, u32 len, u32 offset, u32 priv)
{
    struct translator_mode_info minfo;
    struct ai_trans_mode ai_minfo;
    int ret = -1;
    u32 param_len;
    u8 *param;
    u32 ch;
    struct translator_op_03_audio_format op03_fmt = {0};
    u8 count = offset;
    u8 source = priv;

    ai_translator_get_mode_info(&ai_minfo);
    minfo.mode = __mode_a2r_remap(ai_minfo.mode);
    minfo.encode = __encode_a2r_remap(ai_minfo.coding_type);
    minfo.ch = ai_minfo.ch;
    minfo.sr = ai_minfo.sr;
    switch (minfo.mode) {
    case RCSP_TRANSLATOR_MODE_RECORD:
    case RCSP_TRANSLATOR_MODE_RECORD_TRANSLATION:
        ret = JL_DATA_send(JL_OPCODE_DATA, 0x04, buf, len, JL_NOT_NEED_RESPOND, 0, NULL);
        break;
    case RCSP_TRANSLATOR_MODE_CALL_TRANSLATION:
    case RCSP_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC:
    case RCSP_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC:
    case RCSP_TRANSLATOR_MODE_A2DP_TRANSLATION:
    case RCSP_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION:
        if (minfo.mode == RCSP_TRANSLATOR_MODE_CALL_TRANSLATION) {
            op03_fmt.source = RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_UPSTREAM;
        } else if (minfo.mode == RCSP_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
            op03_fmt.source = RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_MIX;
        } else if (minfo.mode == RCSP_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
            op03_fmt.source = RCSP_TRANSLATOR_AUDIO_SOURCE_ESCO_MIX;
        } else if (minfo.mode == RCSP_TRANSLATOR_MODE_A2DP_TRANSLATION) {
            op03_fmt.source = RCSP_TRANSLATOR_AUDIO_SOURCE_MSBC;
        } else if (minfo.mode == RCSP_TRANSLATOR_MODE_FACE_TO_FACE_TRANSLATION) {
            op03_fmt.source = RCSP_TRANSLATOR_AUDIO_SOURCE_DEV_MIC;
        }
        if (source != 0xff) {
            op03_fmt.source = source;
        }
        //TODO 确认后期会不会换编码格式
        op03_fmt.encode = minfo.encode;
        op03_fmt.count = count;
        op03_fmt.crc = CRC16(buf, len);
        op03_fmt.crc = READ_BIG_U16(&op03_fmt.crc);
        op03_fmt.len = READ_BIG_U16(&len);
        param_len = 2 + sizeof(struct translator_op_03_audio_format) + len;
        param = zalloc(param_len);
        if (!param) {
            return -1;
        }
        param[0] = RCSP_TRANSLATOR_OP_SEND_AUDIO_DATA;
        //TODO 有超长度的风险
        param[1] = sizeof(struct translator_op_03_audio_format) + len;
        memcpy(param + 2, &op03_fmt, sizeof(struct translator_op_03_audio_format));
        memcpy(param + 2 + sizeof(struct translator_op_03_audio_format), buf, len);
        ret = JL_CMD_send(JL_OPCODE_TRANSLATOR, param, param_len, JL_NOT_NEED_RESPOND, 0, NULL);
        free(param);
        break;
    case RCSP_TRANSLATOR_MODE_IDLE:
        break;
    }
    return ret;
}

int JL_rcsp_translator_functions(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    struct RcspModel *rcspModel = (struct RcspModel *)priv;
    u8 op;
    u32 param_len;
    u8 *param;
    u32 resp_len;
    u8 *resp_buf;
    u32 free_size;
    int err;
    int ret = 0;
    u8 ch;
    int mutex_ret = 0;
    struct translator_mode_info minfo = {0};

#define check_resp_alloc(resp_buf) \
    if (resp_buf == NULL) { \
        /*ASSERT(resp_buf, "malloc fail\n");*/ \
        ret = JL_PRO_STATUS_FAIL; \
        goto __err_exit; \
    }

    if (rcspModel == NULL) {
        return JL_PRO_STATUS_FAIL;
    }

    tlr_hdl.ble_con_handle = ble_con_handle;
    if (spp_remote_addr) {
        memcpy(tlr_hdl.spp_remote_addr, spp_remote_addr, 6);
    }

    switch (OpCode) {
    case JL_OPCODE_TRANSLATOR:
        //printf("%s() %d\n", __func__, __LINE__);
        op = data[0];
        param_len = data[1];
        param = data + 2;
        switch (op) {
        case RCSP_TRANSLATOR_OP_GET_MODE_INFO:
            //printf("%s() %d\n", __func__, __LINE__);
            resp_len = 2 + sizeof(struct translator_mode_info);
            resp_buf = zalloc(resp_len);
            check_resp_alloc(resp_buf);
            resp_buf[0] = op;
            resp_buf[1] = resp_len - 2;
            _translator_op_get_info(resp_buf + 2, resp_len - 2);
            ret = JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, resp_buf, resp_len, ble_con_handle, spp_remote_addr);
            free(resp_buf);
            break;
        case RCSP_TRANSLATOR_OP_SET_MODE:
            //printf("%s() %d\n", __func__, __LINE__);
            resp_len = 3;
            resp_buf = zalloc(resp_len);
            check_resp_alloc(resp_buf);
            resp_buf[0] = op;
            resp_buf[1] = 1;
            minfo.mode = param[0];
            minfo.encode = param[1];
            minfo.ch = param[2];
            minfo.sr = READ_BIG_U32(param + 3);
            err = _translator_op_set_mode(&minfo);
            if (!err) {
                resp_buf[2] = 0;
            } else {
                resp_buf[2] = -err;
            }
            ret = JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, resp_buf, resp_len, ble_con_handle, spp_remote_addr);
            free(resp_buf);
            break;
        case RCSP_TRANSLATOR_OP_SEND_AUDIO_DATA:
            //no response
            //printf("%s() %d\n", __func__, __LINE__);
            err = _translator_op_receive_audio_data(param, param_len);
            if (err == 0) {
                ret = JL_PRO_STATUS_SUCCESS;
            } else {
                ret = JL_PRO_STATUS_FAIL;
                goto __exit;
            }
            break;
        case RCSP_TRANSLATOR_OP_GET_CACHE_FREE_SIZE:
#if AI_AUDIO_RECV_SPEED_DEBUG
            int usage[2];
            os_cpu_usage(NULL, usage);
            rcsp_printf("usage: %d - %d\n", usage[0], usage[1]);
#endif
            resp_len = 7;
            resp_buf = zalloc(resp_len);
            check_resp_alloc(resp_buf);
            err = _translator_op_get_cache_free_size(param[0], &free_size);
            if (err < 0) {
                free_size = 0;
            }
            resp_buf[0] = RCSP_TRANSLATOR_OP_GET_CACHE_FREE_SIZE;
            resp_buf[1] = 5;
            resp_buf[2] = param[0];
            free_size = READ_BIG_U32(&free_size);
            memcpy(resp_buf + 3, &free_size, 4);
            ret = JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, resp_buf, resp_len, ble_con_handle, spp_remote_addr);
            free(resp_buf);
            break;
        default:
            ret = JL_PRO_STATUS_UNKOWN_CMD;
            goto __exit;
        }
        break;
    case 0x04:  //start record
        param_len = len;
        param = data;
        rcsp_printf("cmd 0x04, encode %x, sr type %x, judge way %x\n", param[0], param[1], param[2]);
        err = _translator_op_record_start();
        if (err < 0) {
            ret = JL_PRO_STATUS_FAIL;
            goto __err_exit;
        }
        ret = JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, NULL, 0, ble_con_handle, spp_remote_addr);
        break;
    case 0x05:  //stop record
        param_len = len;
        param = data;
        rcsp_printf("cmd 0x05, stop reason %x\n", param[0]);
        err = _translator_op_record_stop();
        if (err < 0) {
            ret = JL_PRO_STATUS_FAIL;
            goto __err_exit;
        }
        ret = JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, NULL, 0, ble_con_handle, spp_remote_addr);
        break;
    default:
        ret = JL_PRO_STATUS_UNKOWN_CMD;
        goto __exit;
    }

__exit:
    return ret;

__err_exit:
    JL_CMD_response_send(OpCode, ret, OpCode_SN, NULL, 0, ble_con_handle, spp_remote_addr);
    return ret;
}

static const struct ai_translator_ops_hub trans_ops_hub = {
    .player_ops = {
        .player_inform_cache_free_size = _translator_op_inform_cache_free_size,
    },
    .recorder_ops = {
        // 1. 协议层发送接口
        .recorder_send_by_protocol_layer_host = _translator_op_send_audio_data,
        .recorder_send_by_protocol_layer_slave = NULL,
        // 2. 各个音频流页面AI_TX节点调用的回调函数
        // 2.1  <智能语音>页面AI_TX
        .recorder_send_for_dev_mic = translator_send_ch_for_dev_mic,
        // 2.2.1 <蓝牙通话>页面AI_TX通话上行独立编码单声道opus模式
        .recorder_send_for_esco_upstream = translator_send_ch_for_esco_upstream,
        // 2.2.2 <蓝牙通话>页面AI_TX通话下行独立编码单声道opus模式
        .recorder_send_for_esco_downstream = translator_send_ch_for_esco_downstream,
        // 2.3 <蓝牙通话>页面AI_TX通话上下行合并编码立体声opus模式
        .recorder_send_for_esco_mix = translator_send_ch_for_esco_mix,
        // 2.4 <蓝牙音乐>页面AI_TX
        .recorder_send_for_a2dp = translator_send_ch_for_a2dp,
    },
    .trans_ops = {
        .translator_inform_mode_info = _translator_op_inform_mode_info,
    },
};

void JL_rcsp_translator_init()
{
    ai_translator_init(&trans_ops_hub);
}

void JL_rcsp_translator_deinit()
{
    struct translator_mode_info minfo;
    minfo.mode = RCSP_TRANSLATOR_MODE_IDLE;
    _translator_op_set_mode(&minfo);
    ai_translator_deinit();
}

#endif
