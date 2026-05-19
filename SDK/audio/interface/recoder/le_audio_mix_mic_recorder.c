/*************************************************************************************************/
/*!
*  \file      le_audio_mix_mic_recorder.c
*
*  \brief	  在其它非mic模式下的广播下叠加mic一起广播出去
*
*  Copyright (c) 2011-2025 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/

#include "jlstream.h"
#include "le_audio_recorder.h"
#include "sdk_config.h"
#include "le_audio_stream.h"
#include "audio_config_def.h"
#include "audio_config.h"
#include "media/sync/audio_syncts.h"
#include "asm/dac.h"
#include "audio_cvp.h"
#include "wireless_trans.h"
#include "le_audio_mix_mic_recorder.h"
#include "le_broadcast.h"
#include "le_connected.h"
#include "mic_effect.h"

extern void mic_effect_ram_code_unload();
extern void mic_effect_ram_code_load();

struct le_audio_mix_mic_recorder {
    void *stream;
};
static struct le_audio_mix_mic_recorder *g_mix_mic_recorder = NULL;



#if LE_AUDIO_LOCAL_MIC_EN	//该宏需要和LE_AUDIO_MIX_MIC_EN 互斥
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN))


#include "app_le_broadcast.h"
#include "le_audio_stream.h"
#include "le_audio_player.h"


/* ******************************************************
 * 功能：全程混合mic广播                                *
 * 要求：使能bis 并固定为发送端，需要广播打开后才能调用 *
 *       宏	LE_AUDIO_MIX_MIC_EN 需要为 0                *
 * *****************************************************/

static void *g_le_audio = NULL;
static u8 local_mic_le_audio_en = 0; //该值若为1，则广播始终为发送端

static u8 local_mic_le_audio_status = LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE;

static const u8 LE_AUDIO_LOCAL_MUSIC_BIT = BIT(0);	//music 播放的标志位
static const u8 LE_AUDIO_LOCAL_MIC_BIT = BIT(1);	//广播mic 打开的标志位

static void mic_recorder_callback(void *private_data, int event)
{
    printf("le audio mic recorder callback : %d\n", event);
}

static int local_mic_le_audio_recorder_open(void *params, void *le_audio, int latency)
{
    u16 irq_points = 256;
    if (g_mix_mic_recorder) {
        return 0;
    }
    int err = 0;
    struct le_audio_stream_params *lea_params = params;
    struct le_audio_stream_format *le_audio_fmt = &lea_params->fmt;
#if (LE_AUDIO_MIX_MIC_EFFECT_EN)
    //如果本地混响打开的情况下，关闭混响
    if (mic_effect_player_runing()) {
        mic_effect_player_close();
    }
    mic_effect_ram_code_load();

    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
#else
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_le_audio");
#endif
    struct stream_enc_fmt fmt = {
        .coding_type = le_audio_fmt->coding_type,
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
    };
    if (!g_mix_mic_recorder) {
        g_mix_mic_recorder = zalloc(sizeof(struct le_audio_mix_mic_recorder));
        if (!g_mix_mic_recorder) {
            return -ENOMEM;
        }
    }
#if (LE_AUDIO_MIX_MIC_EFFECT_EN)
    g_mix_mic_recorder->stream = jlstream_pipeline_parse_by_node_name(uuid, "MicEff");
#else
    g_mix_mic_recorder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_ADC);
#endif

    if (!g_mix_mic_recorder->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

    jlstream_set_callback(g_mix_mic_recorder->stream, NULL, mic_recorder_callback);
    jlstream_set_scene(g_mix_mic_recorder->stream, STREAM_SCENE_MIC);
    jlstream_node_ioctl(g_mix_mic_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, irq_points);
    jlstream_node_ioctl(g_mix_mic_recorder->stream, NODE_UUID_VOCAL_TRACK_SYNTHESIS, NODE_IOC_SET_PRIV_FMT, irq_points);

#if (LE_AUDIO_MIX_MIC_EFFECT_EN)
    jlstream_add_thread(g_mix_mic_recorder->stream, "mic_effect1");
    jlstream_add_thread(g_mix_mic_recorder->stream, "mic_effect2");
    jlstream_add_thread(g_mix_mic_recorder->stream, "mic_effect3");
#endif

    if (latency == 0) {
        latency = 85000;
    }

    jlstream_node_ioctl(g_mix_mic_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
    err = jlstream_node_ioctl(g_mix_mic_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
    err = jlstream_ioctl(g_mix_mic_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    //设置中断点数
    /* jlstream_node_ioctl(g_mix_mic_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS); */

    if (err == 0) {
        err = jlstream_start(g_mix_mic_recorder->stream);
    }
    if (err) {
        goto __exit1;
    }

    printf("le_audio mic recorder open success  \n");
    return 0;

__exit1:
    jlstream_release(g_mix_mic_recorder->stream);
__exit0:
    free(g_mix_mic_recorder);
    g_mix_mic_recorder = NULL;
    return err;
}

static void local_mic_le_audio_recorder_close(void)
{
    struct le_audio_mix_mic_recorder *mic_recorder = g_mix_mic_recorder;

    printf("-- local_mic_le_audio_recorder_close\n");

    if (!mic_recorder) {
        return;
    }
    if (mic_recorder->stream) {
        jlstream_stop(mic_recorder->stream, 0);
        jlstream_release(mic_recorder->stream);
    }
#if LE_AUDIO_MIX_MIC_EFFECT_EN
    mic_effect_ram_code_unload();
#endif
    free(mic_recorder);
    g_mix_mic_recorder = NULL;

    set_local_mic_le_audio_en(0);

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic_le_audio");
}


/*
 * 打开 混合mic广播
 * 要求：使能bis 并固定为发送端，需要广播打开后才能调用
 * 返回值：le_audio 句柄
 */
void *local_mix_mic_le_audio_open(void *args)
{
    if (is_local_mix_mic_le_audio_runing()) {
        r_printf("__________ err, local_mix_mic_le_audio_open, local_mic_le_audio_status is LOCAL_MIX_MIC_OPEN!");
        return g_le_audio;
    }
    int err = 0;
    printf("____________ enter : local_mix_mic_le_audio_open\n");

    if (g_le_audio) {
        //music 广播已经打开
        if (args == NULL) {
            struct le_audio_stream_params params = {0};
            //普通广播
            params.fmt.nch = get_big_audio_coding_nch();
            params.fmt.bit_rate = get_big_audio_coding_bit_rate();
            params.fmt.coding_type = LE_AUDIO_CODEC_TYPE;
            params.fmt.frame_dms = get_big_audio_coding_frame_duration();
            params.fmt.sdu_period = get_big_sdu_period_us();
            params.fmt.isoIntervalUs = get_big_iso_period_us();
            params.fmt.sample_rate = LE_AUDIO_CODEC_SAMPLERATE;
            params.fmt.dec_ch_mode = LEA_TX_DEC_OUTPUT_CHANNEL;
            params.latency = get_big_tx_latency();
            err = local_mic_le_audio_recorder_open((void *)&params.fmt, g_le_audio, params.latency);
        } else {
            struct le_audio_stream_params *params = (struct le_audio_stream_params *)args;
            err = local_mic_le_audio_recorder_open((void *)&params->fmt, g_le_audio, params->latency);
        }
        if (err != 0) {
            ASSERT(0, "%s %d, recorder open fail", __func__, __LINE__);
        }
    } else {
        // music 没有广播
        void *le_audio = NULL;
        update_app_broadcast_deal_scene(LE_AUDIO_MUSIC_START);
        struct le_audio_stream_params *params = (struct le_audio_stream_params *)args;
        le_audio = le_audio_stream_create(params->conn, &params->fmt);


        err = local_mic_le_audio_recorder_open((void *)&params->fmt, le_audio, params->latency);
        if (err != 0) {
            ASSERT(0, "%s %d, recorder open fail", __func__, __LINE__);
        }
#if LEA_LOCAL_SYNC_PLAY_EN
        err = le_audio_player_open(le_audio, params);
        if (err != 0) {
            ASSERT(0, "player open fail");
        }
#endif
        g_le_audio = le_audio;
    }

    local_mic_le_audio_status = local_mic_le_audio_status | LE_AUDIO_LOCAL_MIC_BIT;

    y_printf(">>>>>>>>>>>>>>>>>>>>>> tx mic Update LE_AUDIO_MUSIC_START~");
    update_app_broadcast_deal_scene(LE_AUDIO_MUSIC_START);

    return g_le_audio;
}


//这个函数的关闭适用于music广播打开的情况！！
//这个函数的关闭适用于music广播打开的情况！！
//这个函数的关闭适用于music广播打开的情况！！
int local_mic_tx_le_audio_close(void)
{
    if (!is_local_mix_mic_le_audio_runing()) {
        r_printf("__________ err, local_mic_tx_le_audio_close, local_mic_le_audio_status is 0x%x", local_mic_le_audio_status);
        return 0;
    }
    printf("____________ enter : local_mic_tx_le_audio_close\n");

    local_mic_le_audio_status &= (~LE_AUDIO_LOCAL_MIC_BIT);

    //关闭广播音频播放
#if LEA_LOCAL_SYNC_PLAY_EN
    if (get_local_le_audio_status() == LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE) {
        le_audio_player_close(g_le_audio);
    }
#endif
    local_mic_le_audio_recorder_close();

    if (get_local_le_audio_status() == LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE) {
        le_audio_stream_free(g_le_audio);
        g_le_audio = NULL;
        printf("Func:%s, tx mic Update LE_AUDIO_MUSIC_STOP~", __func__);
        update_app_broadcast_deal_scene(LE_AUDIO_MUSIC_STOP);
    }

    return 0;
}

//当音乐停止的时候，local le_audio 需要做的处理
void local_le_audio_music_stop_deal(void)
{
    printf("< local_le_audio_music_stop_deal - 1>\n");
    local_mic_le_audio_status &= (~LE_AUDIO_LOCAL_MUSIC_BIT);
    if (get_local_le_audio_status() == LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE) {
        printf("< local_le_audio_music_stop_deal - 2>\n");
        le_audio_player_close(g_le_audio);
        le_audio_stream_free(g_le_audio);
        update_app_broadcast_deal_scene(LE_AUDIO_MUSIC_STOP);
        g_le_audio = NULL;
    }
}

//当音乐停止的时候，local le_audio 需要做的处理
void local_le_audio_music_start_deal()
{
    printf("< local_le_audio_music_start_deal >\n");
    local_mic_le_audio_status = local_mic_le_audio_status | LE_AUDIO_LOCAL_MUSIC_BIT;
}

//获取当前local 广播状态
int get_local_le_audio_status()
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));

    int ret = 0;
    switch (local_mic_le_audio_status) {
    case LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE:
        r_printf("______ status: LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE! addr:%x\n", rets);
        ret = LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE;
        break;
    case LOCAL_MIX_MIC_CLOSE_MUSIC_OPEN:
        r_printf("______ status: LOCAL_MIX_MIC_CLOSE_MUSIC_OPEN! addr:%x\n", rets);
        ret = LOCAL_MIX_MIC_CLOSE_MUSIC_OPEN;
        break;
    case LOCAL_MIX_MIC_OPEN_MUSIC_CLOSE:
        r_printf("______ status: LOCAL_MIX_MIC_OPEN_MUSIC_CLOSE! addr:%x\n", rets);
        ret = LOCAL_MIX_MIC_OPEN_MUSIC_CLOSE;
        break;
    case LOCAL_MIX_MIC_OPEN_MUSIC_OPEN:
        r_printf("______ status: LOCAL_MIX_MIC_OPEN_MUSIC_OPEN! addr:%x\n", rets);
        ret = LOCAL_MIX_MIC_OPEN_MUSIC_OPEN;
        break;
    default:
        ret = -1;
        ASSERT(0, "err, %s, %d, status:%d\n", __func__, __LINE__, local_mic_le_audio_status);
        break;
    }

    return ret;
}


bool is_local_mix_mic_le_audio_runing(void)
{
    int ret = (local_mic_le_audio_status & LE_AUDIO_LOCAL_MIC_BIT);
    /* printf("%s %d\n", __func__, ret); */
    return (ret != 0) ? 1 : 0;
}

bool is_local_le_audio_music_runing(void)
{
    int ret = (local_mic_le_audio_status & LE_AUDIO_LOCAL_MUSIC_BIT);
    /* printf("%s %d\n", __func__, ret); */
    return (ret != 0) ? 1 : 0;
}


void *get_local_mix_mic_le_audio(void)
{
    return g_le_audio;
}

void set_local_mix_mic_le_audio(void *le_audio)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    if (g_le_audio == NULL) {
        printf(">> set_local_mix_mic_le_audio: %x\n", (u32)le_audio);
        g_le_audio = le_audio;
    } else {
        ASSERT(0, "err, %s, %d, addr:0x%x", __func__, __LINE__, rets);
    }
}

// local_mic_le_audio_en 这个值决定打开广播的时候是否是发送端
void set_local_mic_le_audio_en(u8 en)
{
    local_mic_le_audio_en = en;
    printf("===> Set local_mic_le_audio_en:%d\n", en);
}

u8 get_local_mic_le_audio_en(void)
{
    printf("===> Get local_mic_le_audio_en:%d\n", local_mic_le_audio_en);
    return local_mic_le_audio_en;
}


int get_micEff2LeAudio_switch_status()
{
#if LE_AUDIO_MIX_MIC_EFFECT_EN
    if (is_local_mix_mic_le_audio_runing()) {
        putchar('1');
        return 1;
    }
    putchar('0');
    return 0;
#else
    return 0;
#endif
}

#endif
#endif



