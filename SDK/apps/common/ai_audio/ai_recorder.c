#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ai_recorder.data.bss")
#pragma data_seg(".ai_recorder.data")
#pragma const_seg(".ai_recorder.text.const")
#pragma code_seg(".ai_recorder.text")
#endif
#include "system/includes.h"
#include "app_config.h"
#include "app_msg.h"
#include "circular_buf.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "esco_recoder.h"
#include "sniff.h"
#include "classic/tws_api.h"
#include "avctp_user.h"
#include "asm/charge.h"
#include "ai_voice_recoder.h"
#include "system/task.h"
#include "ai_audio_common.h"
#include "ai_recorder.h"

#define LOG_TAG             "[AI_REC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_AI_RECORDER_ENABLE

#define TWS_FUNC_ID_AI_RECORDER             TWS_FUNC_ID('A', 'I', 'R', 'E')

struct ai_recorder_ch {
    u8 ch;
    u8 state;
    u8 host_state;  //tws主机状态
    u8 media_type;
    u8 tws_rec;
    u8 *cbuf_buf;
    u32 buf_size;
    u32 offset;
    u16 timer_id;
    OS_MUTEX mutex;
    struct ai_audio_format format;
};

struct ai_recorder_handle {
    struct ai_recorder_ch recorder_ch[AI_AUDIO_CH_MAX];
    struct ai_recorder_ops ops;
    u8 init;
    u8 charge_state;
    u8 esco_param_is_set;
};
static struct ai_recorder_handle recorder_hdl;

enum {
    TWS_TRANS_CMD_INIT,
    TWS_TRANS_CMD_DEINIT,
    TWS_TRANS_CMD_SET_OPS,
    TWS_TRANS_CMD_SYNC_REC_INFO
};


static int ai_recorder_tws_send_cmd(u8 cmd, u8 auxiliary, void *param, u32 len);


static int ai_recorder_set_esco_stream_param(struct ai_recorder_ch *rec_ch)
{
    int ret = 0;
    u8 *remote_bt_addr;
    struct stream_enc_fmt s_enc_fmt = {0};

    if (esco_recoder_running() == 0) {
        return 0;
    }
    switch (rec_ch->format.coding_type) {
    case AUDIO_CODING_OPUS:
        s_enc_fmt.coding_type = AUDIO_CODING_OPUS;
        s_enc_fmt.sample_rate = rec_ch->format.sample_rate;//16000;
        s_enc_fmt.frame_dms = rec_ch->format.frame_dms;//200;
        s_enc_fmt.bit_rate = rec_ch->format.bit_rate;//16000;
        s_enc_fmt.channel = rec_ch->format.channel;//1;
        break;
    case AUDIO_CODING_JLA_V2:
        s_enc_fmt.coding_type = AUDIO_CODING_JLA_V2;
        s_enc_fmt.sample_rate = rec_ch->format.sample_rate;//16000;
        s_enc_fmt.frame_dms = rec_ch->format.frame_dms;//200;
        s_enc_fmt.bit_rate = rec_ch->format.bit_rate;//16000;
        s_enc_fmt.channel = rec_ch->format.channel;//1;
        break;
    }
    remote_bt_addr = bt_get_current_remote_addr();
    if (remote_bt_addr == NULL) {
        ret = -EFAULT;
        goto __err_exit;
    }
    if (rec_ch->media_type == AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM ||
        rec_ch->media_type == AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM) {
        esco_recoder_close();
        esco_player_close();
        ret = esco_player_open_extended(remote_bt_addr, ESCO_PLAYER_EXT_TYPE_AI, &s_enc_fmt);
        if (ret < 0) {
            goto __err_exit;
        }
        esco_player_set_ai_tx_node_func(recorder_hdl.ops.recorder_send_for_esco_downstream);
        ret = esco_recoder_open_extended(remote_bt_addr, ESCO_RECODER_EXT_TYPE_AI, &s_enc_fmt);
        if (ret < 0) {
            goto __err_exit;
        }
        esco_recoder_set_ai_tx_node_func(recorder_hdl.ops.recorder_send_for_esco_upstream);

    } else if (rec_ch->media_type == AI_AUDIO_MEDIA_TYPE_ESCO_MIX) {
        esco_recoder_close();
        esco_player_close();
        ret = esco_player_open_extended(remote_bt_addr, ESCO_PLAYER_EXT_TYPE_AI, &s_enc_fmt);
        if (ret < 0) {
            goto __err_exit;
        }
        ret = esco_recoder_open_extended(remote_bt_addr, ESCO_RECODER_EXT_TYPE_AI, &s_enc_fmt);
        if (ret < 0) {
            goto __err_exit;
        }
        //合并声道编码，只设esco player或者esco recorder其中一个的AI_TX回调
        //函数即可，不用两个都设
        esco_player_set_ai_tx_node_func(recorder_hdl.ops.recorder_send_for_esco_mix);
    }

    return 1;

__err_exit:
    esco_recoder_close();
    esco_player_close();
    //恢复默认配置
    esco_player_open_extended(remote_bt_addr, ESCO_PLAYER_EXT_TYPE_NONE, NULL);
    esco_recoder_open_extended(remote_bt_addr, ESCO_RECODER_EXT_TYPE_NONE, NULL);
    return ret;
}

static int ai_recorder_try_to_set_func(struct ai_recorder_ch *rec_ch)
{
    int done = 0;
    int ret = 0;

    switch (rec_ch->media_type) {
    case AI_AUDIO_MEDIA_TYPE_VOICE:
        ret = ai_voice_recoder_open(rec_ch->format.coding_type, 0);
        if (ret) {
            done = -1;
            break;
        }
        ai_voice_recoder_set_ai_tx_node_func(recorder_hdl.ops.recorder_send_for_dev_mic);
        done = 1;
        break;
    case AI_AUDIO_MEDIA_TYPE_A2DP:
        if (a2dp_player_runing()) {
            a2dp_player_set_ai_tx_node_func(recorder_hdl.ops.recorder_send_for_a2dp);
            done = 1;
        }
        break;
    case AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM:
    case AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM:
    case AI_AUDIO_MEDIA_TYPE_ESCO_MIX:
        if (recorder_hdl.esco_param_is_set) {
            done = 1;
        } else {
            ret = ai_recorder_set_esco_stream_param(rec_ch);
            if (ret > 0) {
                recorder_hdl.esco_param_is_set = 1;
                done = 1;
            } else if (ret < 0) {
                recorder_hdl.esco_param_is_set = 1;
                done = -1;
            }
        }
        break;
    default:
        done = -1;
    }
    return done;
}

static void ai_recorder_delay_set_func(void *priv)
{
    struct ai_recorder_ch *rec_ch = priv;
    int done = 0;
    done = ai_recorder_try_to_set_func(rec_ch);
    if (done && rec_ch->timer_id) {
        sys_timer_del(rec_ch->timer_id);
        rec_ch->timer_id = 0;
    }
}

int ai_recorder_data_send(u8 ch, u8 *buf, u32 len, u32 offset, u32 priv)
{
    int ret = 0;
    struct ai_recorder_ch *rec_ch = NULL;
    u32 left_len = 0;
    u8 role;

    if (recorder_hdl.charge_state) {
        return 0;
    }
    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    rec_ch = &recorder_hdl.recorder_ch[ch];
    if (OS_NO_ERR != os_mutex_pend(&rec_ch->mutex, 10)) {
        return -1;
    }
    if (!rec_ch->state) {
        ret = 0;
        goto __exit;
    }
    if (len > rec_ch->buf_size - rec_ch->offset) {
        left_len = len - (rec_ch->buf_size - rec_ch->offset);
        len -= left_len;
        if (left_len >= rec_ch->buf_size) {
            r_printf("trans send overflow!!! ch %d\n", ch);
            ret = -1;
            goto __exit;
        }
    }
    memcpy(rec_ch->cbuf_buf + rec_ch->offset, buf, len);
    rec_ch->offset += len;
    if (rec_ch->offset == rec_ch->buf_size) {
        rec_ch->offset = 0;
        role = tws_api_get_role();
        //g_printf("send: len %d ch %d role %d\n", rec_ch->buf_size, ch, role);
        if (role == TWS_ROLE_MASTER) {
            if (recorder_hdl.ops.recorder_send_by_protocol_layer_host) {
                ret = recorder_hdl.ops.recorder_send_by_protocol_layer_host(rec_ch->cbuf_buf, rec_ch->buf_size, offset, priv);
                if (ret) {
                    ret = -1;
                }
            }
        } else if (role == TWS_ROLE_SLAVE) {
            if (recorder_hdl.ops.recorder_send_by_protocol_layer_slave) {
                ret = recorder_hdl.ops.recorder_send_by_protocol_layer_slave(rec_ch->cbuf_buf, rec_ch->buf_size, offset, priv);
                if (ret) {
                    ret = -1;
                }
            }
        }
    }
    if (left_len) {
        memcpy(rec_ch->cbuf_buf, buf + len, left_len);
        rec_ch->offset = left_len;
    }
__exit:
    os_mutex_post(&rec_ch->mutex);
    return ret;
}

int ai_recorder_start(u8 ch, struct ai_audio_format *fmt, u8 media_type, u8 tws_rec)
{
    int ret = 0;
    struct ai_recorder_ch *rec_ch = NULL;
    u32 fsize = 40;
    u32 size;

    //if (tws_api_get_role() == TWS_ROLE_SLAVE) {
    //   return 0;
    //}
    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    rec_ch = &recorder_hdl.recorder_ch[ch];
    if (fmt == NULL) {
        return -1;
    }
    os_mutex_pend(&rec_ch->mutex, 0);
    if (rec_ch->state) {
        os_mutex_post(&rec_ch->mutex);
        return 0;
    }
    log_info("ai recorder start in: ch %d, media_type %d\n", ch, media_type);
    if (fmt->coding_type == AUDIO_CODING_OPUS) {
        fsize = (fmt->bit_rate / 8) * fmt->frame_dms / 10000;
#if 0
    } else if (fmt->coding_type == AUDIO_CODING_PCM) {
        fsize = 320 * 2;  //320采样点 16bit 1ch
    } else if (fmt->coding_type == RAUDIO_CODING_SPEEX) {
        //TODO
        fsize = 200;
    } else if (fmt->coding_type == AUDIO_CODING_MSBC) {
        //TODO
        fsize = 200;
#endif
    } else if (fmt->coding_type == AUDIO_CODING_JLA_V2) {
        fsize = (fmt->bit_rate / 8) * fmt->frame_dms / 10000;
        fsize += (JLA_V2_CODEC_WITH_FRAME_HEADER ? 2 : 0);
    } else {
        ret = -1;
        goto __err_exit;
    }
    memcpy(&rec_ch->format, fmt, sizeof(struct ai_audio_format));
    rec_ch->format.frame_size = fsize;
    rec_ch->media_type = media_type;
    rec_ch->tws_rec = tws_rec;
    //size = fsize * 5;
    switch (media_type) {
    case AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM:
    case AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM:
        size = (100 + fsize / 2) / fsize * fsize;
        break;
    case AI_AUDIO_MEDIA_TYPE_ESCO_MIX:
        size = fsize;
        break;
    default:
        //尽量保持在200左右，round取整
        size = (200 + fsize / 2) / fsize * fsize;
        break;
    }
    log_info("ai_recorder: frame size %d, send size %d\n", fsize, size);
    rec_ch->cbuf_buf = malloc(size);
    if (rec_ch->cbuf_buf == NULL) {
        ret = -1;
        goto __err_exit;
    }
    rec_ch->buf_size = size;
    rec_ch->offset = 0;
    //设置AI_TX节点发送回调函数，如果a2dp / esco未打开不能设进去回调函数，
    //就设定时器定时查询直到设入成功
    int err = ai_recorder_try_to_set_func(rec_ch);
    if (err == 0) {
        rec_ch->timer_id = sys_timer_add(rec_ch, ai_recorder_delay_set_func, 20);
    } else if (err < 0) {
        ret = err;
        goto __err_exit;
    }
    log_info("ai recorder start out: ch %d, media_type %d\n", ch, media_type);
    rec_ch->state = 1;
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        rec_ch->host_state = 1;
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_recorder_tws_send_cmd(TWS_TRANS_CMD_SYNC_REC_INFO, rec_ch->ch, rec_ch, sizeof(struct ai_recorder_ch));
        }
    }
    os_mutex_post(&rec_ch->mutex);

    return 0;

__err_exit:
    log_error("ai recorder start fail: ch %d, media_type %d\n", ch, media_type);
    if (rec_ch) {
        if (rec_ch->cbuf_buf) {
            free(rec_ch->cbuf_buf);
            rec_ch->cbuf_buf = NULL;
        }
        os_mutex_post(&rec_ch->mutex);
    }
    return ret;
}

int ai_recorder_stop(u8 ch)
{
    struct ai_recorder_ch *rec_ch;

    //if (tws_api_get_role() == TWS_ROLE_SLAVE) {
    //   return 0;
    //}
    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    rec_ch = &recorder_hdl.recorder_ch[ch];
    os_mutex_pend(&rec_ch->mutex, 0);
    if (!rec_ch->state) {
        os_mutex_post(&rec_ch->mutex);
        return 0;
    }
    log_info("ai recorder stop: ch %d, media_type %d\n", ch, rec_ch->media_type);
    switch (rec_ch->media_type) {
    case AI_AUDIO_MEDIA_TYPE_VOICE:
        ai_voice_recoder_close();
        break;
    }
    recorder_hdl.esco_param_is_set = 0;
    if (rec_ch->cbuf_buf) {
        free(rec_ch->cbuf_buf);
        rec_ch->cbuf_buf = NULL;
    }
    if (rec_ch->timer_id) {
        sys_timer_del(rec_ch->timer_id);
        rec_ch->timer_id = 0;
    }
    rec_ch->state = 0;
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        rec_ch->host_state = 0;
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_recorder_tws_send_cmd(TWS_TRANS_CMD_SYNC_REC_INFO, rec_ch->ch, rec_ch, sizeof(struct ai_recorder_ch));
        }
    }
    os_mutex_post(&rec_ch->mutex);
    return 0;
}

u8 ai_recorder_get_status(u8 ch)
{
    u8 state;
    struct ai_recorder_ch *rec_ch;

    if (ch >= AI_AUDIO_CH_MAX) {
        return 0;
    }
    rec_ch = &recorder_hdl.recorder_ch[ch];
    state = rec_ch->state;
    if (rec_ch->media_type == AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM ||
        rec_ch->media_type == AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM ||
        rec_ch->media_type == AI_AUDIO_MEDIA_TYPE_ESCO_MIX) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            if (recorder_hdl.esco_param_is_set == 0) {
                state = 0;
            }
        }
    }
    return state;
}

static int ai_recorder_charge_event_handler(int *msg)
{
    int ret = false;
    switch (msg[0]) {
    case CHARGE_EVENT_LDO5V_IN:
        //暂停发数，让tws切换可以执行
        recorder_hdl.charge_state = 1;
        break;
    case CHARGE_EVENT_LDO5V_OFF:
        recorder_hdl.charge_state = 0;
        break;
    }
    return ret;
}

APP_MSG_HANDLER(ai_recorder_charge_event) = {
    .owner = 0xff,
    .from = MSG_FROM_BATTERY,
    .handler = ai_recorder_charge_event_handler,
};

static int ai_recorder_bt_event_handler(int *event)
{
    struct bt_event *bt = (struct bt_event *)event;

    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(ai_recorder_bt_event) = {
    .owner = 0xff,
    .from = MSG_FROM_BT_STACK,
    .handler = ai_recorder_bt_event_handler,
};

static int ai_recorder_tws_send_cmd(u8 cmd, u8 auxiliary, void *param, u32 len)
{
    return ai_audio_common_tws_send_cmd(TWS_FUNC_ID_AI_RECORDER, cmd, auxiliary, param, len);
}

static void ai_recorder_tws_msg_handler(u8 *_data, u32 len)
{
    int err = 0;
    u8 cmd;
    //u32 param_len;
    u8 *param;
    struct ai_recorder_ch rec_ch_src;
    struct ai_recorder_ch *rec_ch;
    u8 ch;

    cmd = _data[0];
    switch (cmd) {
    case TWS_TRANS_CMD_INIT:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            ai_recorder_init(NULL);
        }
        break;
    case TWS_TRANS_CMD_DEINIT:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            ai_recorder_deinit();
        }
        break;
    case TWS_TRANS_CMD_SET_OPS:
        param = _data + 2;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(&recorder_hdl.ops, param, sizeof(struct ai_recorder_ops));
        }
        break;
    case TWS_TRANS_CMD_SYNC_REC_INFO:
        ch = _data[1];
        param = _data + 2;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(&rec_ch_src, param, sizeof(struct ai_recorder_ch));
            rec_ch = &recorder_hdl.recorder_ch[ch];
            rec_ch->host_state = rec_ch_src.host_state;
            rec_ch->media_type = rec_ch_src.media_type;
            rec_ch->tws_rec = rec_ch_src.tws_rec;
            memcpy(&rec_ch->format, &rec_ch_src.format, sizeof(struct ai_audio_format));

            //是否同步打开/关闭
            if (rec_ch->tws_rec) {
                if (rec_ch->host_state) {
                    ai_recorder_start(ch,
                                      &rec_ch->format,
                                      rec_ch->media_type,
                                      rec_ch->tws_rec);
                } else {
                    ai_recorder_stop(ch);
                }
            }
        }
        break;
    }

    free(_data);
}

static void ai_recorder_tws_msg_from_sibling(void *_data, u16 len, bool rx)
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
    msg[0] = (int)ai_recorder_tws_msg_handler;
    msg[1] = 2;
    msg[2] = (int)rx_data;
    msg[3] = len;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
    if (err) {
        printf("tws rx post fail\n");
    }
    //cppcheck-suppress memleak
}

REGISTER_TWS_FUNC_STUB(ai_recorder_tws_sibling_stub) = {
    .func_id = TWS_FUNC_ID_AI_RECORDER,
    .func = ai_recorder_tws_msg_from_sibling,
};

#if TCFG_USER_TWS_ENABLE
static int ai_recorder_tws_event_handler(int *_msg)
{
    struct tws_event *evt = (struct tws_event *)_msg;
    int role = evt->args[0];
    int reason = evt->args[2];
    struct ai_recorder_ch *rec_ch = NULL;

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        if (role == TWS_ROLE_MASTER) {
            if (recorder_hdl.init) {
                ai_recorder_tws_send_cmd(TWS_TRANS_CMD_INIT, 0, NULL, 0);
                ai_recorder_tws_send_cmd(TWS_TRANS_CMD_SET_OPS, 0, &recorder_hdl.ops, sizeof(struct ai_recorder_ops));
                for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
                    rec_ch = &recorder_hdl.recorder_ch[i];
                    ai_recorder_tws_send_cmd(TWS_TRANS_CMD_SYNC_REC_INFO, rec_ch->ch, rec_ch, sizeof(struct ai_recorder_ch));
                }
            }
        } else {

        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        if (role == TWS_ROLE_MASTER) {

        } else {
            ai_recorder_deinit();
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_MASTER) {
            for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
                rec_ch = &recorder_hdl.recorder_ch[i];
                if (rec_ch->tws_rec) {
                    ai_recorder_stop(rec_ch->ch);
                    ai_recorder_start(rec_ch->ch,
                                      &rec_ch->format,
                                      rec_ch->media_type,
                                      1);
                } else {
                    if (rec_ch->host_state) {
                        ai_recorder_start(rec_ch->ch,
                                          &rec_ch->format,
                                          rec_ch->media_type,
                                          0);
                    }
                }
            }
        } else {
            for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
                rec_ch = &recorder_hdl.recorder_ch[i];
                if (rec_ch->tws_rec) {
                } else {
                    if (rec_ch->host_state) {
                        ai_recorder_stop(rec_ch->ch);
                    }
                }
            }
        }
        break;
    }
    return 0;
}

APP_MSG_HANDLER(ai_recorder_tws_event) = {
    .owner = 0xff,
    .from = MSG_FROM_TWS,
    .handler = ai_recorder_tws_event_handler,
};
#endif

void ai_recorder_init(const struct ai_recorder_ops *ops)
{
    struct ai_recorder_ch *rec_ch = NULL;
    if (recorder_hdl.init) {
        return;
    }
    log_info("%s(), role: %d\n", __func__, tws_api_get_role());
    for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
        rec_ch = &recorder_hdl.recorder_ch[i];
        memset(rec_ch, 0, sizeof(struct ai_recorder_ch));
        rec_ch->ch = i;
        os_mutex_create(&rec_ch->mutex);
    }
    recorder_hdl.charge_state = get_charge_online_flag();
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        if (ops) {
            memcpy(&recorder_hdl.ops, ops, sizeof(struct ai_recorder_ops));
        }
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_recorder_tws_send_cmd(TWS_TRANS_CMD_INIT, 0, NULL, 0);
            ai_recorder_tws_send_cmd(TWS_TRANS_CMD_SET_OPS, 0, &recorder_hdl.ops, sizeof(struct ai_recorder_ops));
        }
    }
    recorder_hdl.init = 1;
}

void ai_recorder_deinit()
{
    if (!recorder_hdl.init) {
        return;
    }
    log_info("%s(), role: %d\n", __func__, tws_api_get_role());
    for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
        ai_recorder_stop(i);
    }
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_recorder_tws_send_cmd(TWS_TRANS_CMD_DEINIT, 0, NULL, 0);
        }
    }
    recorder_hdl.init = 0;
}

#endif
