#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ai_player.data.bss")
#pragma data_seg(".ai_player.data")
#pragma const_seg(".ai_player.text.const")
#pragma code_seg(".ai_player.text")
#endif

#include "system/includes.h"
#include "app_config.h"
#include "app_msg.h"
#include "ai_rx_player.h"
#include "sniff.h"
#include "classic/tws_api.h"
#include "volume_node.h"
#include "reference_time.h"
#include "avctp_user.h"
#include "asm/charge.h"
#include "system/task.h"
#include "ai_audio_common.h"
#include "ai_player.h"

#define LOG_TAG             "[AI_PLAY]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_AI_PLAYER_ENABLE

#define TWS_FUNC_ID_AI_PLAYER               TWS_FUNC_ID('A', 'I', 'P', 'Y')

struct ai_player_ch {
    u8 ch;
    u8 state;
    u8 host_state;  //tws主机状态
    u8 media_type;
    u8 tws_play;
    struct list_head head;
    u32 recv_size;
    u32 max_size;
    void *cb_priv;
    void (*recv_decode_resume)(void *cb_priv, u8 ch);
    u8 ready;
    u8 full;
    u8 sync_play;
    u8 underrun;
    u32 frame_priv;
    OS_MUTEX mutex;
    u32 start_time;
    u32 timestamp; //us
    u8 ready_ts;
    u16 timer_id;
    struct ai_audio_format format;
};

struct ai_player_handle {
    struct ai_player_ch player_ch[AI_AUDIO_CH_MAX];
    struct ai_player_ops ops;
    u8 init;
    u16 volume;
    u16 volume_timer_id;
};
static struct ai_player_handle player_hdl;

enum {
    TWS_TRANS_CMD_INIT,
    TWS_TRANS_CMD_DEINIT,
    TWS_TRANS_CMD_SET_OPS,
    TWS_TRANS_CMD_SYNC_PLAY_INFO,
    TWS_TRANS_CMD_RECV_AUDIO_SYNC,
    TWS_TRANS_CMD_SET_VOLUME,
    TWS_TRANS_CMD_SYNC_PLAY,
    TWS_TRANS_CMD_MEDIA_SUSPEND,
    TWS_TRANS_CMD_MEDIA_RESUME
};


static int ai_player_tws_send_cmd(u8 cmd, u8 auxiliary, void *param, u32 len);
static void ai_player_tws_sync_play(u8 ch);
static void ai_player_tws_suspend(u8 ch);
static void ai_player_tws_resume(u8 ch);
extern u32 bt_audio_conn_clock_time(void *addr);
extern u32 bt_audio_reference_clock_time(u8 network);
extern void set_esco_link_timing2_interval(u8 esco_timing2_interval);


int ai_player_start(u8 ch, struct ai_audio_format *fmt, u8 media_type, u8 tws_play)
{
    int ret = 0;
    struct ai_player_ch *play_ch = NULL;
    u32 fsize = 40;
    u32 size;
    u8 *remote_bt_addr = NULL;
    struct ai_rx_player_param param = {0};

    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    play_ch = &player_hdl.player_ch[ch];
    os_mutex_pend(&play_ch->mutex, 0);
    if (play_ch->state) {
        os_mutex_post(&play_ch->mutex);
        return 0;
    }
    log_info("ai player start in: ch %d, media_type %d\n", ch, media_type);
    if (fmt->coding_type == AUDIO_CODING_OPUS) {
        param.coding_type = AUDIO_CODING_OPUS;
        if (fmt->channel == 1) {
            param.channel_mode = AUDIO_CH_MIX;
        } else if (fmt->channel == 2) {
            param.channel_mode = AUDIO_CH_LR;
        } else {
            ASSERT(0, "%s() channel not support\n", __func__);
        }
        param.sample_rate = fmt->sample_rate;//16000;
        param.frame_dms = fmt->frame_dms;
        param.bit_rate = fmt->bit_rate;
        int frame_size = (fmt->bit_rate / 8) * fmt->frame_dms / 10000;
        play_ch->format.frame_size = frame_size;//40; //一帧编码输出40个byte;
        play_ch->format.frame_dms = fmt->frame_dms;//200; //20ms一帧
        play_ch->format.bit_rate = fmt->bit_rate;
        fsize = frame_size * 5;//40 * 5;
    } else if (fmt->coding_type == AUDIO_CODING_JLA_V2) {
        param.coding_type = AUDIO_CODING_JLA_V2;
        param.sample_rate = fmt->sample_rate;//16000;
        param.frame_dms = fmt->frame_dms;//200;  //20ms一帧
        if (fmt->channel == 1) {
            param.channel_mode = AUDIO_CH_MIX;
        } else if (fmt->channel == 2) {
            param.channel_mode = AUDIO_CH_LR;
        } else {
            ASSERT(0, "%s() channel not support\n", __func__);
        }
        param.bit_rate = fmt->bit_rate;//16000;
        int frame_size = (fmt->bit_rate / 8) * fmt->frame_dms / 10000;
        frame_size += (JLA_V2_CODEC_WITH_FRAME_HEADER ? 2 : 0);
        play_ch->format.frame_size = frame_size; //一帧编码输出的大小;
        play_ch->format.frame_dms = param.frame_dms;
        play_ch->format.bit_rate =  param.bit_rate;
        fsize = frame_size * 5;
#if 0
    } else if (fmt->coding_type == AUDIO_CODING_PCM) {
        param.coding_type = AUDIO_CODING_PCM;
        fsize = 320 * 2;  //320采样点 16bit 1ch
    } else if (fmt->coding_type == AUDIO_CODING_SPEEX) {
        param.coding_type = AUDIO_CODING_SPEEX;
        fsize = 200;
    } else if (fmt->coding_type == AUDIO_CODING_MSBC) {
        param.coding_type = AUDIO_CODING_MSBC;
        fsize = 200;
#endif
    } else {
        ret = -1;
        goto __err_exit;
    }

    play_ch->format.coding_type =  param.coding_type;
    play_ch->format.sample_rate =  param.sample_rate;
    play_ch->format.channel =  fmt->channel;

    size = AI_AUDIO_CACHE_SIZE(fsize);
    INIT_LIST_HEAD(&play_ch->head);
    play_ch->recv_size = 0;
    play_ch->max_size = size;
    play_ch->ready = 0;
    play_ch->full = 0;
    play_ch->sync_play = 0;
    play_ch->underrun = 0;
    play_ch->media_type = media_type;
    play_ch->tws_play = tws_play;
#if TCFG_AI_RX_NODE_ENABLE
    remote_bt_addr = ai_audio_common_get_remote_bt_addr();
    if (remote_bt_addr == NULL) {
        ret = -1;
        goto __err_exit;
    }
    switch (media_type) {
    case AI_AUDIO_MEDIA_TYPE_A2DP:
        param.type = AI_SERVICE_MEDIA;
        break;
    case AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM:
        param.type = AI_SERVICE_CALL_UPSTREAM;
        break;
    case AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM:
        param.type = AI_SERVICE_CALL_DOWNSTREAM;
        break;
    default:
        param.type = AI_SERVICE_VOICE;
        break;
    }

    ret = ai_rx_player_open(remote_bt_addr, play_ch->ch, &param);
    if (ret < 0) {
        goto __err_exit;
    }
#endif
    play_ch->state = 1;
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        play_ch->host_state = 1;
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_audio_common_sync_remote_bt_addr();
            ai_player_tws_send_cmd(TWS_TRANS_CMD_SYNC_PLAY_INFO, play_ch->ch, play_ch, sizeof(struct ai_player_ch));
        }
    }
    log_info("ai player start out: ch %d, media_type %d\n", ch, media_type);
    os_mutex_post(&play_ch->mutex);

    return 0;

__err_exit:
    log_error("ai player start fail: ch %d, media_type %d\n", ch, media_type);
    os_mutex_post(&play_ch->mutex);
    return ret;
}

int ai_player_stop(u8 ch)
{
    struct ai_player_ch *play_ch;
    struct list_head *p, *n;
    struct ai_player_audio_frame *frame;

    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    play_ch = &player_hdl.player_ch[ch];
    os_mutex_pend(&play_ch->mutex, 0);
    if (!play_ch->state) {
        os_mutex_post(&play_ch->mutex);
        return 0;
    }
    log_info("ai player stop: ch %d, media_type %d\n", ch, play_ch->media_type);
#if TCFG_AI_RX_NODE_ENABLE
    ai_rx_player_close(play_ch->ch);
#endif
    list_for_each_safe(p, n, &play_ch->head) {
        frame = list_entry(p, struct ai_player_audio_frame, entry);
        list_del(&frame->entry);
        frame->size = 0;
        frame->timestamp = 0;
        free(frame);
    }
    play_ch->recv_size = 0;
    play_ch->state = 0;
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        play_ch->host_state = 0;
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_player_tws_send_cmd(TWS_TRANS_CMD_SYNC_PLAY_INFO, play_ch->ch, play_ch, sizeof(struct ai_player_ch));
        }
    }
    os_mutex_post(&play_ch->mutex);
    return 0;
}

u32 ai_player_get_timestamp(u8 ch)
{
    struct ai_player_ch *play_ch;
    u8 *remote_bt_addr = NULL;
    u32 timestamp = 0;

    if (ch >= AI_AUDIO_CH_MAX) {
        return 0;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (play_ch->state) {
        remote_bt_addr = ai_audio_common_get_remote_bt_addr();
        if (remote_bt_addr == NULL) {
            return 0;
        }
        if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
            return 0;
        }
        if (!play_ch->ready) {
            if (!play_ch->ready_ts) {
                play_ch->start_time = bt_audio_conn_clock_time(remote_bt_addr);//获取当前时间,
                play_ch->timestamp = play_ch->start_time * 625; //转换成us
                play_ch->ready_ts = 1;
                /* y_printf("-ready : start_ts: %u,\n",play_ch->timestamp); */
            }

        } else {
            play_ch->ready_ts = 0;
        }
        if (!play_ch->start_time) {
            play_ch->start_time = bt_audio_conn_clock_time(remote_bt_addr);//获取当前时间,
            play_ch->timestamp = play_ch->start_time * 625;
            /* y_printf("---start_ts: %u,\n",play_ch->timestamp); */
        }
        /* printf("--- %u,\n",play_ch->timestamp); */
        timestamp = play_ch->timestamp & 0xffffffff;
        os_mutex_post(&play_ch->mutex);
    }
    return timestamp;
}

void ai_player_update_timestamp(u8 ch, u32 input_size)
{
    struct ai_player_ch *play_ch;
    u8 frame_num = 0;
    u32 frame_time = 0;
    /*解析帧长时间*/
    if (ch >= AI_AUDIO_CH_MAX) {
        return;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (play_ch->state) {
        if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
            return;
        }
        if (play_ch->format.coding_type == AUDIO_CODING_OPUS) {
            //opus一帧数据40个byte,20ms,如有改动，这里需要同步修改
            frame_num = input_size / play_ch->format.frame_size;
            frame_time = frame_num * (play_ch->format.frame_dms / 10) * 1000 ;  //us
        } else if (play_ch->format.coding_type == AUDIO_CODING_JLA_V2) {
            //JLA_V2一帧数据40个byte + 2个byte 的头,20ms,如有改动，这里需要同步修改
            /* frame_num = input_size / (40 + 2); */
            frame_num = input_size / play_ch->format.frame_size;
            frame_time = frame_num * (play_ch->format.frame_dms / 10) * 1000 ;  //us
        } else {
            /*添加其他格式 */
        }
        play_ch->timestamp += frame_time;
        os_mutex_post(&play_ch->mutex);
    }
}

int ai_player_put_frame(u8 ch, u8 *buf, u32 len, u32 timestamp, u32 priv)
{
    struct ai_player_ch *play_ch;
    int ret = 0;
    struct ai_player_audio_frame *frame = NULL;

    //ch == 0 ? putchar('A') : putchar('B');
    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
        return 0;
    }
    if (!play_ch->state) {
        ret = -EFAULT;
        goto __err_exit;
    }
    if (play_ch->underrun) {
        if ((play_ch->underrun & 0x02) == 0) {
            if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
                if (tws_api_get_role() == TWS_ROLE_MASTER) {
                    ai_player_tws_send_cmd(TWS_TRANS_CMD_MEDIA_RESUME, play_ch->ch, NULL, 0);
                }
            } else {
                int msg[3];
                msg[0] = (int)ai_player_tws_resume;
                msg[1] = 1;
                msg[2] = play_ch->ch;
                os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
            }
            play_ch->underrun |= 0x02;
        }
    }
    //临界溢出情况处理，允许稍微超一点
    if (play_ch->recv_size + len > play_ch->max_size * 3 / 2) {
        r_printf("drop frame");
        ret = -ENOMEM;
        goto __err_exit;
    } else if (play_ch->recv_size + len > play_ch->max_size) {
        r_printf("translate cache full");
        put_buf(buf, 40);
    }
    frame = zalloc(sizeof(*frame) + len);
    if (frame == NULL) {
        ret = -ENOMEM;
        goto __err_exit;
    }
    frame->buf = (u8 *)(frame + 1);
    frame->size = len;
    frame->timestamp = timestamp;
    memcpy(frame->buf, buf, frame->size);
    list_add_tail(&frame->entry, &play_ch->head);
    play_ch->recv_size += len;
    play_ch->frame_priv = priv;

    if (play_ch->tws_play) {
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                switch (play_ch->media_type) {
                case AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM:
                case AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM:
#if AI_AUDIO_CALL_TRANSLATION_LEFT_RIGHT_SPLIT
                    break;
#endif
                default:
                    ai_player_tws_send_cmd(TWS_TRANS_CMD_RECV_AUDIO_SYNC,
                                           play_ch->ch,
                                           (u8 *)frame,
                                           sizeof(*frame) + len);
                    break;
                }
            }
        }
    }

    if (!play_ch->ready) {
        //用ai_rx_file.c里的play_latency做起播缓存控制，这里就
        //去掉起播缓存控制
        /* if (play_ch->recv_size >= play_ch->format.frame_size * 5 * 3) */
        {
            play_ch->ready = 1;
            if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
                //if (tws_api_get_role() == TWS_ROLE_MASTER) {
                //    play_ch->sync_play = 0;
                ai_player_tws_send_cmd(TWS_TRANS_CMD_SYNC_PLAY, play_ch->ch, NULL, 0);
                //}
            } else {
                int msg[3];
                msg[0] = (int)ai_player_tws_sync_play;
                msg[1] = 1;
                msg[2] = play_ch->ch;
                os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
            }
        }
    }

#if AI_AUDIO_RECV_SPEED_DEBUG
    g_printf("put[%d]: %d %d t %u %u\n", ch, frame->size, play_ch->recv_size, bt_audio_reference_clock_time(1) * 625, timestamp);
#endif
    os_mutex_post(&play_ch->mutex);
    return ret;

__err_exit:
    if (frame) {
        free(frame);
    }
    os_mutex_post(&play_ch->mutex);
    return ret;
}

static void ai_player_audio_underrun_signal(void *arg)
{
    struct ai_player_ch *play_ch = (struct ai_player_ch *)arg;

    local_irq_disable();
    if (play_ch->recv_decode_resume) {
        play_ch->recv_decode_resume(play_ch->cb_priv, play_ch->ch);
    }
    play_ch->timer_id = 0;
    local_irq_enable();
}

static int ai_player_audio_frame_underrun_detect(u8 ch)
{
    u8 *remote_bt_addr = ai_audio_common_get_remote_bt_addr();
    if (remote_bt_addr == NULL) {
        return -1;
    }
    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    struct ai_player_ch *play_ch = &player_hdl.player_ch[ch];
    int underrun_time = 30;
    u32 reference_time = bt_audio_conn_clock_time(remote_bt_addr) * 625;
    u32 next_timestamp =  ai_player_get_timestamp(ch);
    int distance_time = next_timestamp - reference_time;
    if (distance_time > 67108863L || distance_time < -67108863L) {
        if (next_timestamp > reference_time) {
            distance_time = next_timestamp - 0xffffffff - reference_time;
        } else {
            distance_time = 0xffffffff - reference_time + next_timestamp;
        }
    }
    if (distance_time <= underrun_time) {
        return true;
    }
    //local_irq_disable();
    if (play_ch->timer_id) {
        sys_hi_timeout_del(play_ch->timer_id);
        play_ch->timer_id = 0;
    }
    play_ch->timer_id = sys_hi_timeout_add(play_ch, ai_player_audio_underrun_signal, (distance_time - underrun_time));
    //local_irq_enable();

    return 0;
}

struct ai_player_audio_frame *ai_player_get_frame(u8 ch)
{
    struct ai_player_ch *play_ch;
    struct ai_player_audio_frame *frame = NULL;

    //ch == 0 ? putchar('0') : putchar('1');
    if (ch >= AI_AUDIO_CH_MAX) {
        return NULL;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
        return NULL;
    }
    if (!play_ch->state) {
        goto __exit;
    }
    if (play_ch->ready && play_ch->sync_play) {
        if (!list_empty(&play_ch->head)) {
            frame = list_first_entry(&play_ch->head, struct ai_player_audio_frame, entry);
        } else {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                if (ai_player_audio_frame_underrun_detect(ch)) {
                    r_printf("%s() no frame", __func__);
                    play_ch->ready = 0;
                    play_ch->sync_play = 0;
                    if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
                        ai_player_tws_send_cmd(TWS_TRANS_CMD_MEDIA_SUSPEND, play_ch->ch, NULL, 0);
                    } else {
                        int msg[3];
                        msg[0] = (int)ai_player_tws_suspend;
                        msg[1] = 1;
                        msg[2] = play_ch->ch;
                        os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
                    }
                }
            } else {
                play_ch->ready = 0;
                play_ch->sync_play = 0;
            }
        }
    }
__exit:
    os_mutex_post(&play_ch->mutex);
    return frame;
}

int ai_player_free_frame(u8 ch, struct ai_player_audio_frame *frame)
{
    struct ai_player_ch *play_ch;
    int ret = 0;

    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (frame == NULL) {
        return -EFAULT;
    }
    if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
        return 0;
    }
    if (!play_ch->state) {
        ret = 0;
        goto __exit;
    }
    play_ch->recv_size -= frame->size;
#if AI_AUDIO_RECV_SPEED_DEBUG
    g_printf("get[%d]: %d %d, t %u %u\n", ch, frame->size, play_ch->recv_size, bt_audio_reference_clock_time(1) * 625, frame->timestamp);
#endif
    if (play_ch->full) {
        if (play_ch->recv_size <= play_ch->max_size * 6 / 10) {
            //消耗一定量缓存之后通知APP恢复发数
            u32 free_size = play_ch->max_size - play_ch->recv_size;
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                if (player_hdl.ops.player_inform_cache_free_size) {
                    player_hdl.ops.player_inform_cache_free_size(free_size, play_ch->frame_priv);
                }
            }
            play_ch->full = 0;
        }
    }
    list_del(&frame->entry);
    free(frame);
__exit:
    os_mutex_post(&play_ch->mutex);
    return ret;
}

int ai_player_set_play_volume(u16 volume)
{
    struct volume_cfg vcfg;
    int ret;
    struct ai_player_ch *play_ch;
    const char *node_name = NULL;

    log_info("ai_player set volume to %d\n", volume);
    player_hdl.volume = volume;
    vcfg.bypass = VOLUME_NODE_CMD_SET_VOL;
    vcfg.cur_vol = volume;
    for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
        play_ch = &player_hdl.player_ch[i];
        if (play_ch->state) {
            switch (play_ch->media_type) {
            case AI_AUDIO_MEDIA_TYPE_VOICE:
                node_name = "Vol_AIVoice";
                break;
            case AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM:
                node_name = "Vol_BtcCallAI";
                break;
            }
            if (node_name) {
                break;
            }
        }
    }
    if (node_name == NULL) {
        return -1;
    }
    ret = jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, node_name, &vcfg, sizeof(vcfg));

    if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            ai_player_tws_send_cmd(TWS_TRANS_CMD_SET_VOLUME, 0, (u8 *)&volume, 2);
        }
    }
    return ret;
}

void ai_player_update_play_volume(u8 ch)
{
    struct ai_player_ch *play_ch;
    if (ch >= AI_AUDIO_CH_MAX) {
        return;
    }
    play_ch = &player_hdl.player_ch[ch];
    switch (play_ch->media_type) {
    case AI_AUDIO_MEDIA_TYPE_VOICE:
    case AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM:
        ai_player_set_play_volume(player_hdl.volume);
        break;
    }
}

void ai_player_set_decode_resume_handler(u8 ch, void *priv, void (*func)(void *priv, u8 source))
{
    struct ai_player_ch *play_ch;
    if (ch >= AI_AUDIO_CH_MAX) {
        return;
    }
    play_ch = &player_hdl.player_ch[ch];

    play_ch->cb_priv = priv;
    play_ch->recv_decode_resume = func;
}

int ai_player_get_cache_free_size(u8 ch, u32 *free_size)
{
    int ret = 0;
    struct ai_player_ch *play_ch = NULL;

    if (ch >= AI_AUDIO_CH_MAX) {
        return -1;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
        return 0;
    }
    if (!play_ch->state) {
        *free_size = 0;
        ret = -ENOMEM;
        goto __exit;
    }
    if (play_ch->recv_size > play_ch->max_size) {
        *free_size = 0;
    } else {
        *free_size = play_ch->max_size - play_ch->recv_size;
    }
#if AI_AUDIO_RECV_SPEED_DEBUG
    g_printf("free_size[%d]: %d\n", ch, *free_size);
#endif
    if (*free_size == 0) {
        //缓存已满，APP挂起发数，等待设备通知APP解除挂起
        play_ch->full = 1;
    }
__exit:
    os_mutex_post(&play_ch->mutex);
    return ret;
}

u8 ai_player_get_status(u8 ch)
{
    struct ai_player_ch *play_ch = NULL;

    if (ch >= AI_AUDIO_CH_MAX) {
        return 0;
    }
    play_ch = &player_hdl.player_ch[ch];
    return play_ch->state;
}

static void ai_player_bt_event_set_volume(void *arg)
{
    ai_player_set_play_volume(player_hdl.volume);
    player_hdl.volume_timer_id = 0;
}

static int ai_player_bt_event_handler(int *event)
{
    struct bt_event *bt = (struct bt_event *)event;

    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        //设一个初始音量，因为有些手机不开音量同步，不发AVRCP调音量，智能语音是静音的
        player_hdl.volume = 8;
        break;
    case BT_STATUS_FIRST_CONNECTED:
        //设edr地址，后续用于tws同步播放
        ai_audio_common_set_remote_bt_addr(bt->args, 0);
        break;
    case BT_STATUS_AVRCP_VOL_CHANGE:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        player_hdl.volume = (bt->value + 1) * 16 / 127;
        if (player_hdl.volume > 16) {
            player_hdl.volume = 16;
        }
        if (player_hdl.volume_timer_id) {
            sys_timer_re_run(player_hdl.volume_timer_id);
        } else {
            player_hdl.volume_timer_id = sys_timeout_add(NULL, ai_player_bt_event_set_volume, 20);
        }
        break;
    }
    return 0;
}

APP_MSG_HANDLER(ai_player_bt_event) = {
    .owner = 0xff,
    .from = MSG_FROM_BT_STACK,
    .handler = ai_player_bt_event_handler,
};

static void ai_player_tws_sync_play(u8 ch)
{
    struct ai_player_ch *play_ch;

    if (ch >= AI_AUDIO_CH_MAX) {
        return;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
        return;
    }
    if (play_ch->state) {
        play_ch->sync_play = 1;
        if (play_ch->recv_decode_resume) {
            play_ch->recv_decode_resume(play_ch->cb_priv, ch);
        }
    }
    os_mutex_post(&play_ch->mutex);
}

static void ai_player_tws_suspend(u8 ch)
{
    struct ai_player_ch *play_ch;
    struct list_head *p, *n;
    struct ai_player_audio_frame *frame;

    if (ch >= AI_AUDIO_CH_MAX) {
        return;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
        return;
    }
    if (play_ch->state) {
#if TCFG_AI_RX_NODE_ENABLE
        ai_rx_player_close(play_ch->ch);
#endif
        list_for_each_safe(p, n, &play_ch->head) {
            frame = list_entry(p, struct ai_player_audio_frame, entry);
            list_del(&frame->entry);
            free(frame);
        }
        play_ch->recv_size = 0;
        play_ch->ready = 0;
        play_ch->sync_play = 0;
        play_ch->underrun = 1;

        //会有出现tws发送suspend到响应suspend期间，APP将缓存一次性填满情况，
        //然后APP挂起等待设备消耗数据，通知APP。这里清过缓存，如果是满的需要
        //通知APP发数
        if (play_ch->full) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                if (player_hdl.ops.player_inform_cache_free_size) {
                    player_hdl.ops.player_inform_cache_free_size(play_ch->max_size, play_ch->frame_priv);
                }
            }
            play_ch->full = 0;
        }
    }
    os_mutex_post(&play_ch->mutex);
}

static void ai_player_tws_resume(u8 ch)
{
    struct ai_player_ch *play_ch;
    u8 *remote_bt_addr;
    struct ai_rx_player_param param = {0};

    if (ch >= AI_AUDIO_CH_MAX) {
        return;
    }
    play_ch = &player_hdl.player_ch[ch];
    if (OS_NO_ERR != os_mutex_pend(&play_ch->mutex, 10)) {
        return;
    }
    if (play_ch->state) {
        if (!play_ch->underrun) {
            goto __exit;
        }
        remote_bt_addr = ai_audio_common_get_remote_bt_addr();
        if (remote_bt_addr == NULL) {
            goto __exit;
        }
#if TCFG_AI_RX_NODE_ENABLE
        switch (play_ch->media_type) {
        case AI_AUDIO_MEDIA_TYPE_A2DP:
            param.type = AI_SERVICE_MEDIA;
            break;
        case AI_AUDIO_MEDIA_TYPE_ESCO_UPSTREAM:
            param.type = AI_SERVICE_CALL_UPSTREAM;
            break;
        case AI_AUDIO_MEDIA_TYPE_ESCO_DOWNSTREAM:
            param.type = AI_SERVICE_CALL_DOWNSTREAM;
            break;
        default:
            param.type = AI_SERVICE_VOICE;
            break;
        }
        param.coding_type = play_ch->format.coding_type;
        param.sample_rate = play_ch->format.sample_rate;
        param.bit_rate = play_ch->format.bit_rate;
        if (play_ch->format.channel == 1) {
            param.channel_mode = AUDIO_CH_MIX;
        } else if (play_ch->format.channel == 2) {
            param.channel_mode = AUDIO_CH_LR;
        } else {
            ASSERT(0, "%s() channel not support\n", __func__);
        }
        param.frame_dms = play_ch->format.frame_dms;

        ai_rx_player_open(remote_bt_addr, play_ch->ch, &param);
#endif
        play_ch->underrun = 0;
    }
__exit:
    os_mutex_post(&play_ch->mutex);
}

static int ai_player_tws_send_cmd(u8 cmd, u8 auxiliary, void *param, u32 len)
{
    return ai_audio_common_tws_send_cmd(TWS_FUNC_ID_AI_PLAYER, cmd, auxiliary, param, len);
}

static void ai_player_tws_msg_handler(u8 *_data, u32 len)
{
    int err = 0;
    u8 cmd;
    u32 param_len;
    u8 *param;
    struct ai_player_audio_frame frame;
    u8 ch;
    struct ai_player_ch play_ch_src;
    struct ai_player_ch *play_ch;

    cmd = _data[0];
    switch (cmd) {
    case TWS_TRANS_CMD_INIT:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            ai_player_init(NULL);
        }
        break;
    case TWS_TRANS_CMD_DEINIT:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            ai_player_deinit();
        }
        break;
    case TWS_TRANS_CMD_SET_OPS:
        param = _data + 2;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(&player_hdl.ops, param, sizeof(struct ai_player_ops));
        }
        break;
    case TWS_TRANS_CMD_SYNC_PLAY_INFO:
        ch = _data[1];
        param = _data + 2;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(&play_ch_src, param, sizeof(struct ai_player_ch));
            play_ch = &player_hdl.player_ch[ch];
            play_ch->host_state = play_ch_src.host_state;
            play_ch->media_type = play_ch_src.media_type;
            play_ch->tws_play = play_ch_src.tws_play;
            memcpy(&play_ch->format, &play_ch_src.format, sizeof(struct ai_audio_format));
            //是否同步打开/关闭
            if (play_ch->tws_play) {
                if (play_ch->host_state) {
                    ai_player_start(ch,
                                    &play_ch->format,
                                    play_ch->media_type,
                                    play_ch->tws_play);
                } else {
                    ai_player_stop(ch);
                }
            }
        }
        break;
    case TWS_TRANS_CMD_RECV_AUDIO_SYNC:
        ch = _data[1];
        play_ch = &player_hdl.player_ch[ch];
        memcpy(&frame, _data + 2, sizeof(frame));
        param = _data + 2 + sizeof(frame);
        param_len = frame.size;
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            ai_player_put_frame(ch, param, param_len, frame.timestamp, play_ch->frame_priv);
        }
        break;
    case TWS_TRANS_CMD_SET_VOLUME:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            memcpy(&player_hdl.volume, _data + 2, 2);
            ai_player_set_play_volume(player_hdl.volume);
        }
        break;
    case TWS_TRANS_CMD_MEDIA_SUSPEND:
        log_info("TWS_TRANS_CMD_MEDIA_SUSPEND\n");
        ch = _data[1];
        ai_player_tws_suspend(ch);
        break;
    case TWS_TRANS_CMD_MEDIA_RESUME:
        log_info("TWS_TRANS_CMD_MEDIA_RESUME\n");
        ch = _data[1];
        ai_player_tws_resume(ch);
        break;
    case TWS_TRANS_CMD_SYNC_PLAY:
        log_info("TWS_TRANS_CMD_SYNC_PLAY\n");
        ch = _data[1];
        ai_player_tws_sync_play(ch);
        break;
    }

    free(_data);
}

/* static void translator_tws_msg_from_sibling(void *_data, u16 len, bool rx) */
static void ai_player_tws_msg_from_sibling(void *_data, u16 len, bool rx)
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
    msg[0] = (int)ai_player_tws_msg_handler;
    msg[1] = 2;
    msg[2] = (int)rx_data;
    msg[3] = len;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
    if (err) {
        log_error("tws rx post fail\n");
    }
    //cppcheck-suppress memleak
}

REGISTER_TWS_FUNC_STUB(ai_player_tws_sibling_stub) = {
    .func_id = TWS_FUNC_ID_AI_PLAYER,
    .func = ai_player_tws_msg_from_sibling,
};

#if TCFG_USER_TWS_ENABLE
static int ai_player_tws_event_handler(int *_msg)
{
    struct tws_event *evt = (struct tws_event *)_msg;
    int role = evt->args[0];
    int reason = evt->args[2];
    struct ai_player_ch *play_ch = NULL;

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        if (role == TWS_ROLE_MASTER) {
            if (player_hdl.init) {
                ai_player_tws_send_cmd(TWS_TRANS_CMD_INIT, 0, NULL, 0);
                ai_player_tws_send_cmd(TWS_TRANS_CMD_SET_OPS, 0, &player_hdl.ops, sizeof(struct ai_player_ops));
                //ai_audio_common_sync_remote_bt_addr();
                for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
                    play_ch = &player_hdl.player_ch[i];
                    //ai_player_tws_send_cmd(TWS_TRANS_CMD_SYNC_PLAY_INFO, play_ch->ch, play_ch, sizeof(struct ai_player_ch));
                    if (play_ch->host_state) {
                        ai_player_stop(play_ch->ch);
                        ai_player_start(play_ch->ch,
                                        &play_ch->format,
                                        play_ch->media_type,
                                        play_ch->tws_play);
                    }
                }
                ai_player_tws_send_cmd(TWS_TRANS_CMD_SET_VOLUME, 0, (u8 *)&player_hdl.volume, 2);
            }
        } else {

        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        if (role == TWS_ROLE_MASTER) {

        } else {
            ai_player_deinit();
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_MASTER) {
            for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
                play_ch = &player_hdl.player_ch[i];
                if (play_ch->tws_play) {
                    if (play_ch->host_state) {
                        ai_player_stop(play_ch->ch);
                        ai_player_start(play_ch->ch,
                                        &play_ch->format,
                                        play_ch->media_type,
                                        1);
                    }
                } else {
                    if (play_ch->host_state) {
                        ai_player_start(play_ch->ch,
                                        &play_ch->format,
                                        play_ch->media_type,
                                        0);
                    }
                }
            }
        } else {
            for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
                play_ch = &player_hdl.player_ch[i];
                if (play_ch->tws_play) {
                } else {
                    if (play_ch->host_state) {
                        ai_player_stop(play_ch->ch);
                    }
                }
            }
        }
        break;
    }
    return 0;
}

APP_MSG_HANDLER(ai_player_tws_event) = {
    .owner = 0xff,
    .from = MSG_FROM_TWS,
    .handler = ai_player_tws_event_handler,
};
#endif

void ai_player_init(const struct ai_player_ops *ops)
{
    struct ai_player_ch *play_ch = NULL;
    if (player_hdl.init) {
        return;
    }
    log_info("%s(), role: %d\n", __func__, tws_api_get_role());
    for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
        play_ch = &player_hdl.player_ch[i];
        memset(play_ch, 0, sizeof(struct ai_player_ch));
        play_ch->ch = i;
        os_mutex_create(&play_ch->mutex);
    }
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        if (ops) {
            memcpy(&player_hdl.ops, ops, sizeof(struct ai_player_ops));
        }
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_player_tws_send_cmd(TWS_TRANS_CMD_INIT, 0, NULL, 0);
            ai_player_tws_send_cmd(TWS_TRANS_CMD_SET_OPS, 0, &player_hdl.ops, sizeof(struct ai_player_ops));
        }
    }
    player_hdl.init = 1;
}

void ai_player_deinit()
{
    struct ai_player_ch *play_ch = NULL;
    if (!player_hdl.init) {
        return;
    }
    log_info("%s(), role: %d\n", __func__, tws_api_get_role());
    for (int i = 0; i < AI_AUDIO_CH_MAX; i++) {
        play_ch = &player_hdl.player_ch[i];
        ai_player_set_decode_resume_handler(play_ch->media_type, NULL, NULL);
        ai_player_stop(i);
    }
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        if ((tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
            ai_player_tws_send_cmd(TWS_TRANS_CMD_DEINIT, 0, NULL, 0);
        }
    }
    player_hdl.init = 0;
}

#endif
