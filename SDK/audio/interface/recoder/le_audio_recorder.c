/*************************************************************************************************/
/*!
*  \file      le_audio_recorder.c
*
*  \brief
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#include "jlstream.h"
#include "le_audio_recorder.h"
#include "sdk_config.h"
#include "le_audio_stream.h"
#include "audio_config_def.h"
#include "audio_config.h"
#include "spdif_file.h"
#include "fm_file.h"
#include "media/sync/audio_syncts.h"
#include "audio_dac.h"
#include "audio_cvp.h"
#include "app_config.h"
#include "audio_anc_includes.h"
#include "esco_recoder.h"
#include "cvp_node.h"

#include "app_main.h"
#include "volume_node.h"
#if TCFG_APP_SPDIF_EN
#include "spdif.h"
#endif
#include "btstack/a2dp_media_codec.h"
#include "btstack/a2dp_media_codec.h"
#include "classic/tws_api.h"

struct le_audio_a2dp_recorder {
    void *stream;
    void *file;
    u8 btaddr[6];
    u16 retry_timer;
    u16 dump_packet_timer;
};

static struct le_audio_a2dp_recorder *g_a2dp_recorder = NULL;
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_AUDIO_LINEIN_ENABLE
struct le_audio_aux_recorder {
    void *stream;
};
static struct le_audio_aux_recorder *g_aux_recorder = NULL;
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_SPDIF_ENABLE
struct le_audio_spdif_recorder {
    void *stream;
};
static struct le_audio_spdif_recorder *g_spdif_recorder = NULL;
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_AUDIO_FM_ENABLE
struct le_audio_fm_recorder {
    void *stream;
};
static struct le_audio_fm_recorder *g_fm_recorder = NULL;
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && (TCFG_APP_IIS_EN || TCFG_IIS_RX_NODE_ENABLE || TCFG_MULTI_CH_IIS_RX_NODE_ENABLE)
struct le_audio_iis_recorder {
    void *stream;
};
static struct le_audio_iis_recorder *g_iis_recorder = NULL;
static struct le_audio_iis_recorder *g_muti_ch_iis_recorder = NULL;
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_PC_ENABLE
struct le_audio_pc_recorder {
    void *stream;
};
static struct le_audio_pc_recorder *g_pc_recorder = NULL;
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN)))
struct le_audio_mic_recorder {
    void *stream;
};
static struct le_audio_mic_recorder *g_mic_recorder = NULL;
#endif

struct le_audio_tx_vol {
    //cppcheck-suppress  unusedStructMember
    bool mute;
    s16 dvol_step;
    s16 dvol;
    s16 max_dvol;
    u8 save_vol_cnt;
    u16 save_vol_timer;
};
static struct le_audio_tx_vol g_le_audio_tx_vol = {0};
static struct le_audio_tx_vol g_le_audio_tx_monitor_vol = {0};

//a2dp流创建但未start的情况下，数据流被打断suspend，a2dp_file.c suspend流程是不走的，需启动HH丢数据处理，避免蓝牙buf满打印ffffff问题
//丢数据策略选配, 0:使用a2dp_file.c内自带流程 1:播放器内使用timer丢数.因测试覆盖场景有限，默认使用播放器自带timer丢数据
#define LE_AUDIO_DUMP_PACKET_LOCGIC_SEL  1
#if LE_AUDIO_DUMP_PACKET_LOCGIC_SEL
static void abandon_a2dp_data(void *p)
{
    if (!p) {
        return ;
    }
    struct le_audio_a2dp_recorder *hdl = (struct le_audio_a2dp_recorder *)p;
    struct a2dp_media_frame _frame;
    hdl->file = a2dp_open_media_file(hdl->btaddr);
    if (!hdl->file) {
        return;
    }

    while ((a2dp_media_get_packet_num(hdl->file) > 1) && (a2dp_media_try_get_packet(hdl->file, &_frame) > 0)) {
        a2dp_media_free_packet(hdl->file, _frame.packet);
    }
}

static void a2dp_file_start_abandon_data(void *p)
{
    if (!p) {
        return ;
    }

    struct le_audio_a2dp_recorder *hdl = (struct le_audio_a2dp_recorder *)p;

    int role = TWS_ROLE_MASTER;

    /*if (CONFIG_BTCTLER_TWS_ENABLE) {
        role = tws_api_get_role();
    }*/
    if (role == TWS_ROLE_MASTER) {
        if (hdl->dump_packet_timer == 0) {
            abandon_a2dp_data(hdl); //启动丢包时清一下缓存;
            hdl->dump_packet_timer = sys_timer_add(hdl, abandon_a2dp_data, 50);
            /* printf("------------a2dp le_audio recorder start_abandon_a2dp_data------------%d\n", __LINE__); */
        }
    }
}

static void a2dp_file_stop_abandon_data(struct le_audio_a2dp_recorder *hdl)
{
    if (hdl && hdl->dump_packet_timer) {
        abandon_a2dp_data(hdl); //停止丢数据清一下缓存
        sys_timer_del(hdl->dump_packet_timer);
        hdl->dump_packet_timer = 0;
        /* printf("--------------a2dp le_audio recorder stop_abandon_a2dp_data-------------%d\n", __LINE__); */
    }
}
#endif

static void a2dp_recorder_callback(void *private_data, int event)
{
    printf("le audio a2dp recorder callback : %d\n", event);

    struct le_audio_a2dp_recorder *hdl = g_a2dp_recorder;

    switch (event) {
    case STREAM_EVENT_PREEMPTED:
        /* puts("---------------------le_audio_recordr suspend event preempted case--------------\n"); */
#if LE_AUDIO_DUMP_PACKET_LOCGIC_SEL
        a2dp_file_start_abandon_data(hdl);
#else
        jlstream_node_ioctl(hdl->stream, NODE_UUID_SOURCE, NODE_IOC_SUSPEND, 0);
#endif
        break;
    case STREAM_EVENT_START:
#if LE_AUDIO_DUMP_PACKET_LOCGIC_SEL
        a2dp_file_stop_abandon_data(hdl);
#endif
        break;
    default:
        break;
    }
}

static void retry_start_a2dp_player(void *p)
{
    if (g_a2dp_recorder && g_a2dp_recorder->stream) {
        int err = jlstream_start(g_a2dp_recorder->stream);
        if (err == 0) {
            sys_timer_del(g_a2dp_recorder->retry_timer);
            g_a2dp_recorder->retry_timer = 0;
        }
    }
}

int le_audio_a2dp_recorder_open(u8 *btaddr, void *arg, void *le_audio)
{
    if (g_a2dp_recorder) {
        return 0;
    }
    int err = 0;
    struct le_audio_stream_format *le_audio_fmt = (struct le_audio_stream_format *)arg;
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"a2dp_le_audio");
    struct stream_enc_fmt fmt = {
        .coding_type = le_audio_fmt->coding_type,
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
    };

    if (!g_a2dp_recorder) {
        g_a2dp_recorder = zalloc(sizeof(struct le_audio_a2dp_recorder));
        if (!g_a2dp_recorder) {
            return -ENOMEM;
        }
    }
    g_a2dp_recorder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_A2DP_RX);

    jlstream_set_callback(g_a2dp_recorder->stream, NULL, a2dp_recorder_callback);
    jlstream_set_scene(g_a2dp_recorder->stream, STREAM_SCENE_A2DP);

    jlstream_node_ioctl(g_a2dp_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_BTADDR, (int)btaddr);

    jlstream_node_ioctl(g_a2dp_recorder->stream, NODE_UUID_IIS0_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
    jlstream_node_ioctl(g_a2dp_recorder->stream, NODE_UUID_IIS1_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);

    jlstream_node_ioctl(g_a2dp_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);

    /*memcpy(&fmt, arg, sizeof(struct stream_enc_fmt));*/

    memcpy(g_a2dp_recorder->btaddr, btaddr, 6);

    err = jlstream_ioctl(g_a2dp_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(g_a2dp_recorder->stream);
        if (err) {
            g_a2dp_recorder->retry_timer = sys_timer_add(NULL, retry_start_a2dp_player, 200);
            return 0;
        }
    }

    if (err) {
        jlstream_release(g_a2dp_recorder->stream);
        free(g_a2dp_recorder);
        g_a2dp_recorder = NULL;
        return err;
    }
    return 0;
}

void le_audio_a2dp_recorder_close(u8 *btaddr)
{
    struct le_audio_a2dp_recorder *a2dp_recorder = g_a2dp_recorder;

    if (!a2dp_recorder) {
        return;
    }

    if (memcmp(a2dp_recorder->btaddr, btaddr, 6)) {
        return;
    }

    if (a2dp_recorder->stream) {
#if LE_AUDIO_LOCAL_MIC_EN
        printf(">> %s jlstream stop!\n", __func__);
        jlstream_node_ioctl(g_a2dp_recorder->stream, NODE_UUID_MIXER, NODE_IOC_STOP, 1);
#endif
        jlstream_stop(a2dp_recorder->stream, 0);
        jlstream_release(a2dp_recorder->stream);
    }

    if (g_a2dp_recorder->retry_timer) {
        sys_timer_del(g_a2dp_recorder->retry_timer);
        g_a2dp_recorder->retry_timer = 0;
    }
#if LE_AUDIO_DUMP_PACKET_LOCGIC_SEL
    a2dp_file_stop_abandon_data(g_a2dp_recorder);
#endif

    free(a2dp_recorder);
    g_a2dp_recorder = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"a2dp_le_audio");
}

u8 *le_audio_a2dp_recorder_get_btaddr(void)
{
    if (!g_a2dp_recorder) {
        return NULL;
    } else {
        return g_a2dp_recorder->btaddr;
    }

}

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_AUDIO_LINEIN_ENABLE
static void aux_recorder_callback(void *private_data, int event)
{
    printf("le audio aux recorder callback : %d\n", event);
}

void *get_le_audio_linein_recorder_stream(void)
{
    if (g_aux_recorder) {
        return g_aux_recorder->stream;
    }
    return NULL;
}

int le_audio_linein_recorder_open(void *params, void *le_audio, int latency)
{
    if (g_aux_recorder) {
        return 0;
    }
    int err = 0;

    //确认中断点数的配置
    u16 linein_irq_point_unit = AUDIO_ADC_IRQ_POINTS;
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SOURCE_EN)
    u16 frame_duration = get_auracast_frame_duration();	//100 or 75
    if (frame_duration == 100) {
        //如果是10ms帧长，那么中断时长需要为5ms，44100的采样率，中断点数为 5 * 44.1 = 220
        linein_irq_point_unit = 220;
    } else if (frame_duration == 75) {
        //如果是7.5ms帧长，那么中断时长需要为2.5ms，44100的采样率，中断点数为 2.5 * 44.1 = 110
        linein_irq_point_unit = 110;
    }
#endif

    struct le_audio_stream_format *le_audio_fmt = (struct le_audio_stream_format *)params;
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"linein_le_audio");
    struct stream_enc_fmt fmt = {
        .coding_type = le_audio_fmt->coding_type,
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
    };

    if (!g_aux_recorder) {
        g_aux_recorder = zalloc(sizeof(struct le_audio_aux_recorder));
        if (!g_aux_recorder) {
            return -ENOMEM;
        }
    }
    g_aux_recorder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_LINEIN);
    jlstream_set_callback(g_aux_recorder->stream, NULL, aux_recorder_callback);
    jlstream_set_scene(g_aux_recorder->stream, STREAM_SCENE_LINEIN);

    jlstream_node_ioctl(g_aux_recorder->stream, NODE_UUID_IIS0_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
    jlstream_node_ioctl(g_aux_recorder->stream, NODE_UUID_IIS1_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);


    err = jlstream_node_ioctl(g_aux_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
    if (latency == 0) {
        latency = 100000;
    }
    jlstream_node_ioctl(g_aux_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
    //设置中断点数
    /* jlstream_node_ioctl(g_aux_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS); */
    jlstream_node_ioctl(g_aux_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, linein_irq_point_unit);


    err = jlstream_ioctl(g_aux_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(g_aux_recorder->stream);
    }

    if (err) {
        jlstream_release(g_aux_recorder->stream);
        free(g_aux_recorder);
        g_aux_recorder = NULL;
        return err;
    }

    return 0;
}

void le_audio_linein_recorder_close(void)
{
    struct le_audio_aux_recorder *aux_recorder = g_aux_recorder;
    if (!aux_recorder) {
        return;
    }
    if (aux_recorder->stream) {
        jlstream_stop(aux_recorder->stream, 0);
        jlstream_release(aux_recorder->stream);
    }

    free(aux_recorder);
    g_aux_recorder = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"linein_le_audio");
}
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && (TCFG_APP_IIS_EN || TCFG_IIS_RX_NODE_ENABLE || TCFG_MULTI_CH_IIS_RX_NODE_ENABLE)

static void muti_ch_iis_recorder_callback(void *private_data, int event)
{
    printf("le audio muti_ch_iis recorder callback : %d\n", event);
}

int le_audio_muti_ch_iis_recorder_open(void *params_ch0, void *params_ch1, void *le_audio, int latency)
{
    int err = 0;
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    struct le_audio_stream_params *lea_params_0 = params_ch0;
    struct le_audio_stream_params *lea_params_1 = params_ch1;

    struct le_audio_stream_format *le_audio_fmt_ch0 = &lea_params_0->fmt;
    struct le_audio_stream_format *le_audio_fmt_ch1 = &lea_params_1->fmt;
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
#else
    struct le_audio_stream_format *le_audio_fmt_ch0 = (struct le_audio_stream_format *)params_ch0;
    struct le_audio_stream_format *le_audio_fmt_ch1 = (struct le_audio_stream_format *)params_ch1;
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"iis_le_audio");
#endif
    struct stream_enc_fmt fmt_ch0 = {
        .coding_type = le_audio_fmt_ch0->coding_type,
        .channel = le_audio_fmt_ch0->nch,
        .bit_rate = le_audio_fmt_ch0->bit_rate,
        .sample_rate = le_audio_fmt_ch0->sample_rate,
        .frame_dms = le_audio_fmt_ch0->frame_dms,
    };
    struct stream_enc_fmt fmt_ch1 = {
        .coding_type = le_audio_fmt_ch1->coding_type,
        .channel = le_audio_fmt_ch1->nch,
        .bit_rate = le_audio_fmt_ch1->bit_rate,
        .sample_rate = le_audio_fmt_ch1->sample_rate,
        .frame_dms = le_audio_fmt_ch1->frame_dms,
    };
    if (!g_muti_ch_iis_recorder) {
        g_muti_ch_iis_recorder = zalloc(sizeof(struct le_audio_iis_recorder));
        if (!g_muti_ch_iis_recorder) {
            return -ENOMEM;
        }
    }
    g_muti_ch_iis_recorder->stream = jlstream_pipeline_parse_by_node_name(uuid, "MULTI_CH_IIS_RX");
    if (!g_muti_ch_iis_recorder->stream) {
        free(g_muti_ch_iis_recorder);
        g_muti_ch_iis_recorder = NULL;
        return -ENOMEM;
    }
    jlstream_set_callback(g_muti_ch_iis_recorder->stream, NULL, muti_ch_iis_recorder_callback);

    if (latency == 0) {
        latency = 100000;
    }
    //延时设置

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    if (lea_params_0->service_type == LEA_SERVICE_WL_MIC) {
        //printf("LEA Recoder:WL_MIC_MULT_IIS\n");
        jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
        jlstream_set_scene(g_muti_ch_iis_recorder->stream, STREAM_SCENE_WIRELESS_MIC);
        //设置中断点数
        u16 irq_point = 0;
#if (defined(TCFG_ADC_IRQ_INTERVAL) &&  TCFG_ADC_IRQ_INTERVAL) //根据配置设置中断点数
#if (defined(TCFG_AUDIO_ADC_SAMPLE_RATE) &&  TCFG_AUDIO_ADC_SAMPLE_RATE)
        irq_point =  TCFG_AUDIO_ADC_SAMPLE_RATE * TCFG_ADC_IRQ_INTERVAL / 1000000;  //sr * interval(us) / 1000000
#else
        irq_point =  fmt.sample_rate * TCFG_ADC_IRQ_INTERVAL / 1000000; //sr * interval(us) / 1000000
#endif
#else
        irq_point =  AUDIO_IIS_IRQ_POINTS;//fmt.sample_rate * 1000 / 1000000; //默认1ms 的中断,//sr * interval(us) / 1000000
#endif

        jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, irq_point);
        jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_IIS0_RX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
        jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_IIS1_RX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
        err = jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
        err = jlstream_ioctl(g_muti_ch_iis_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt_ch1);
    } else

#endif
    {

        jlstream_set_scene(g_muti_ch_iis_recorder->stream, STREAM_SCENE_MUTI_CH_IIS);

        jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_IIS0_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
        jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_IIS1_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);

        /* err = jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio); */
        err = jlstream_ioctl(g_muti_ch_iis_recorder->stream, NODE_IOC_SET_BTADDR, (int)le_audio);

        //设置中断点数
        jlstream_node_ioctl(g_muti_ch_iis_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_IIS_IRQ_POINTS);
        /*节点名字与数据流节点对应*/
        char *lea_enc_node_name = "LEA_ENC_CH0";
        char *lea_sync_node_name = "LEA_Sync_CH0";
        char *lea_source_node_name = "LEA_Source_CH0";
        jlstream_set_node_specify_param(NODE_UUID_ENCODER, lea_enc_node_name, NODE_IOC_SET_ENC_FMT, &fmt_ch0, sizeof(fmt_ch0));
        jlstream_set_node_specify_param(NODE_UUID_LE_AUDIO_SOURCE, lea_source_node_name, NODE_IOC_SET_ENC_FMT, &fmt_ch0, sizeof(fmt_ch0));
        jlstream_set_node_param(NODE_UUID_CAPTURE_SYNC, lea_sync_node_name, &latency, sizeof(int));

        /*节点名字与数据流节点对应*/
        lea_enc_node_name = "LEA_ENC_CH1";
        lea_sync_node_name = "LEA_Sync_CH1";
        lea_source_node_name = "LEA_Source_CH1";
        jlstream_set_node_specify_param(NODE_UUID_ENCODER, lea_enc_node_name, NODE_IOC_SET_ENC_FMT, &fmt_ch1, sizeof(fmt_ch1));
        jlstream_set_node_specify_param(NODE_UUID_LE_AUDIO_SOURCE, lea_source_node_name, NODE_IOC_SET_ENC_FMT, &fmt_ch1, sizeof(fmt_ch1));
        jlstream_set_node_param(NODE_UUID_CAPTURE_SYNC, lea_sync_node_name, &latency, sizeof(int));
    }

    /*节点名字与数据流节点对应*/
    if (err == 0) {
        err = jlstream_start(g_muti_ch_iis_recorder->stream);
    }
    if (err) {
        jlstream_release(g_muti_ch_iis_recorder->stream);
        free(g_muti_ch_iis_recorder);
        g_muti_ch_iis_recorder = NULL;
        return err;
    }
    return 0;
}

void le_audio_muti_ch_iis_recorder_close(void)
{
    struct le_audio_iis_recorder *iis_recorder = g_muti_ch_iis_recorder;
    if (!iis_recorder) {
        return;
    }
    if (iis_recorder->stream) {
        jlstream_stop(iis_recorder->stream, 0);
        jlstream_release(iis_recorder->stream);
    }

    free(iis_recorder);
    g_muti_ch_iis_recorder = NULL;
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic_effect");
#else
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"iis_le_audio");
#endif
}


static void iis_recorder_callback(void *private_data, int event)
{
    printf("le audio iis recorder callback : %d\n", event);
}
int le_audio_iis_recorder_open(void *params, void *le_audio, int latency)
{
    if (g_iis_recorder) {
        return 0;
    }
    int err = 0;
    struct le_audio_stream_params *lea_params = params;
    struct le_audio_stream_format *le_audio_fmt = &lea_params->fmt;

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
#else
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"iis_le_audio");
#endif
    struct stream_enc_fmt fmt = {
        .coding_type = le_audio_fmt->coding_type,
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
    };

    if (!g_iis_recorder) {
        g_iis_recorder = zalloc(sizeof(struct le_audio_iis_recorder));
        if (!g_iis_recorder) {
            return -ENOMEM;
        }
    }
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    g_iis_recorder->stream = jlstream_pipeline_parse_by_node_name(uuid, "WL_IIS0_RX");
#else
    g_iis_recorder->stream = jlstream_pipeline_parse_by_node_name(uuid, "IIS0_RX2");
#endif
    jlstream_set_callback(g_iis_recorder->stream, NULL, iis_recorder_callback);
    jlstream_set_scene(g_iis_recorder->stream, STREAM_SCENE_IIS);

    if (lea_params->service_type == LEA_SERVICE_WL_MIC) {
        //printf("LEA Recoder:WL_MIC_IIS\n");
        jlstream_set_scene(g_iis_recorder->stream, STREAM_SCENE_WIRELESS_MIC);
        u16 irq_point = 0;
#if (defined(TCFG_ADC_IRQ_INTERVAL) &&  TCFG_ADC_IRQ_INTERVAL) //根据配置设置中断点数
#if (defined(TCFG_AUDIO_ADC_SAMPLE_RATE) &&  TCFG_AUDIO_ADC_SAMPLE_RATE)
        irq_point =  TCFG_AUDIO_ADC_SAMPLE_RATE * TCFG_ADC_IRQ_INTERVAL / 1000000;  //sr * interval(us) / 1000000
#else
        irq_point =  fmt.sample_rate * TCFG_ADC_IRQ_INTERVAL / 1000000; //sr * interval(us) / 1000000
#endif
#else
        irq_point =  AUDIO_IIS_IRQ_POINTS;//fmt.sample_rate * 1000 / 1000000; //默认1ms 的中断,//sr * interval(us) / 1000000
#endif

        jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, irq_point);
        jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_IIS0_RX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
        jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_IIS0_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
        err = jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
    } else {
        jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_IIS0_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
        jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_IIS1_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
        err = jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);

        jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_IIS_IRQ_POINTS);

    }
    if (latency == 0) {
        latency = 100000;
    }
    jlstream_node_ioctl(g_iis_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
    //设置中断点数

    err = jlstream_ioctl(g_iis_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(g_iis_recorder->stream);
    }
    if (err) {
        jlstream_release(g_iis_recorder->stream);
        free(g_iis_recorder);
        g_iis_recorder = NULL;
        return err;
    }

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    char *vol_name = "Vol_WMicTX";
    struct volume_cfg cfg = {0};
    int ret = jlstream_get_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg));
    if (ret > 0) {
        g_le_audio_tx_vol.max_dvol = (cfg.cfg_level_max >= cfg.cur_vol) ? cfg.cfg_level_max : cfg.cur_vol;
        g_le_audio_tx_vol.dvol_step =  g_le_audio_tx_vol.max_dvol / cfg.cfg_level_max;
        ret = syscfg_read(CFG_WIRELESS_MIC0_VOLUME, &g_le_audio_tx_vol.dvol, 2);
        if (ret > 0) {
            /* g_le_audio_tx_vol.dvol = app_var.mic_eff_volume; //先使用mic_effect的音量配置 */
            if (g_le_audio_tx_vol.dvol != cfg.cur_vol) {
                le_audio_wireless_mic_tx_set_dvol(g_le_audio_tx_vol.dvol, 0xff); //有记录的音量先用记录的音量
            }
        } else {
            g_le_audio_tx_vol.dvol = cfg.cur_vol;
            le_audio_wireless_mic_tx_set_dvol(g_le_audio_tx_vol.dvol, 0xff);
        }
        printf(" %s dvol:%d\n", __FUNCTION__, g_le_audio_tx_vol.dvol);
    }

    vol_name = "Vol_WMonitor";
    memset(&cfg, 0, sizeof(struct volume_cfg));
    ret = jlstream_get_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg));
    if (ret > 0) {
        g_le_audio_tx_monitor_vol.max_dvol = (cfg.cfg_level_max >= cfg.cur_vol) ? cfg.cfg_level_max : cfg.cur_vol;
        g_le_audio_tx_monitor_vol.dvol_step =  g_le_audio_tx_monitor_vol.max_dvol / cfg.cfg_level_max;
        ret = syscfg_read(CFG_WIRELESS_MIC1_VOLUME, &g_le_audio_tx_monitor_vol.dvol, 2);
        if (ret > 0) {
            /* g_le_audio_tx_monitor_vol.dvol = app_var.mic_eff_volume; //先使用mic_effect的音量配置 */
            if (g_le_audio_tx_monitor_vol.dvol != cfg.cur_vol) {
                le_audio_wireless_mic_tx_monitor_set_dvol(g_le_audio_tx_monitor_vol.dvol, 0xff); //有记录的音量先用记录的音量
            }
        } else {
            g_le_audio_tx_monitor_vol.dvol = cfg.cur_vol;
            le_audio_wireless_mic_tx_monitor_set_dvol(g_le_audio_tx_monitor_vol.dvol, 0xff);
        }
        printf(" %s dvol:%d\n", __FUNCTION__, g_le_audio_tx_monitor_vol.dvol);
    }
#endif

    return 0;
}

void le_audio_iis_recorder_close(void)
{
    struct le_audio_iis_recorder *iis_recorder = g_iis_recorder;
    if (!iis_recorder) {
        return;
    }
    if (iis_recorder->stream) {
        jlstream_stop(iis_recorder->stream, 0);
        jlstream_release(iis_recorder->stream);
    }

    free(iis_recorder);
    g_iis_recorder = NULL;
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic_effect");
#else
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"iis_le_audio");
#endif
}
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_PC_ENABLE
static void pc_recorder_callback(void *private_data, int event)
{
    printf("le audio pc recorder callback : %d\n", event);
}
int le_audio_pc_recorder_open(void *params, void *le_audio, int latency)
{
    if (g_pc_recorder) {
        return 0;
    }
    int err = 0;
    struct le_audio_stream_format *le_audio_fmt = (struct le_audio_stream_format *)params;
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
#else
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"pc_le_audio");
#endif
    struct stream_enc_fmt fmt = {
        .coding_type = le_audio_fmt->coding_type,
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
    };
    if (!g_pc_recorder) {
        g_pc_recorder = zalloc(sizeof(struct le_audio_pc_recorder));
        if (!g_pc_recorder) {
            return -ENOMEM;
        }
    }
    g_pc_recorder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_PC_SPK);
    jlstream_set_callback(g_pc_recorder->stream, NULL, pc_recorder_callback);
    jlstream_set_scene(g_pc_recorder->stream, STREAM_SCENE_PC_SPK);
    if (latency == 0) {
        latency = 100000;
    }
    jlstream_node_ioctl(g_pc_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
    err = jlstream_node_ioctl(g_pc_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
    //设置中断点数
    jlstream_node_ioctl(g_pc_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, 192);
    err = jlstream_ioctl(g_pc_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(g_pc_recorder->stream);
    }
    if (err) {
        jlstream_release(g_pc_recorder->stream);
        free(g_pc_recorder);
        g_pc_recorder = NULL;
        return err;
    }
    return 0;
}

void le_audio_pc_recorder_close(void)
{
    struct le_audio_pc_recorder *pc_recorder = g_pc_recorder;
    if (!pc_recorder) {
        return;
    }
    if (pc_recorder->stream) {
        jlstream_stop(pc_recorder->stream, 0);
        jlstream_release(pc_recorder->stream);
    }
    free(pc_recorder);
    g_pc_recorder = NULL;
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"pc_le_audio");
}
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_AUDIO_SPDIF_ENABLE

u8 is_le_audio_spdif_open(void)
{
    return (g_spdif_recorder == NULL) ? 0 : 1;
}

static void spdif_recorder_callback(void *private_data, int event)
{
    printf("le audio spdif recorder callback : %d\n", event);
}

int le_audio_spdif_recorder_open(void *params, void *le_audio, int latency)
{
    if (g_spdif_recorder) {
        return 0;
    }
    int err = 0;
    struct le_audio_stream_format *le_audio_fmt = (struct le_audio_stream_format *)params;
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"spdif_le_audio");
    struct stream_enc_fmt fmt = {
        .coding_type = le_audio_fmt->coding_type,
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
    };

    if (!g_spdif_recorder) {
        g_spdif_recorder = zalloc(sizeof(struct le_audio_spdif_recorder));
        if (!g_spdif_recorder) {
            return -ENOMEM;
        }
    }
    g_spdif_recorder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_SPDIF);
    jlstream_set_callback(g_spdif_recorder->stream, NULL, spdif_recorder_callback);
    jlstream_set_scene(g_spdif_recorder->stream, STREAM_SCENE_SPDIF);

    jlstream_node_ioctl(g_spdif_recorder->stream, NODE_UUID_IIS0_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
    jlstream_node_ioctl(g_spdif_recorder->stream, NODE_UUID_IIS1_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);

    err = jlstream_node_ioctl(g_spdif_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
    if (latency == 0) {
        latency = 100000;
    }
    jlstream_node_ioctl(g_spdif_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
    jlstream_node_ioctl(g_spdif_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, SPDIF_DATA_DMA_LEN);

    err = jlstream_ioctl(g_spdif_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(g_spdif_recorder->stream);
    }
    if (err) {
        jlstream_release(g_spdif_recorder->stream);
        free(g_spdif_recorder);
        g_spdif_recorder = NULL;
        return err;
    }
    return 0;
}

void le_audio_spdif_recorder_close(void)
{
    struct le_audio_spdif_recorder *spdif_recorder = g_spdif_recorder;
    if (!spdif_recorder) {
        return;
    }
    if (spdif_recorder->stream) {
        jlstream_stop(spdif_recorder->stream, 0);
        jlstream_release(spdif_recorder->stream);
    }

    free(spdif_recorder);
    g_spdif_recorder = NULL;
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"spdif_le_audio");
}
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))) && TCFG_AUDIO_FM_ENABLE
static void fm_recorder_callback(void *private_data, int event)
{
    printf("le audio fm recorder callback : %d\n", event);
}
int le_audio_fm_recorder_open(void *params, void *le_audio, int latency)
{
    if (g_fm_recorder) {
        return 0;
    }
    int err = 0;
    struct le_audio_stream_format *le_audio_fmt = (struct le_audio_stream_format *)params;
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"fm_le_audio");
    struct stream_enc_fmt fmt = {
        .coding_type = le_audio_fmt->coding_type,
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
    };

    if (!g_fm_recorder) {
        g_fm_recorder = zalloc(sizeof(struct le_audio_fm_recorder));
        if (!g_fm_recorder) {
            return -ENOMEM;
        }
    }
    g_fm_recorder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_FM);
    jlstream_set_callback(g_fm_recorder->stream, NULL, fm_recorder_callback);
    jlstream_set_scene(g_fm_recorder->stream, STREAM_SCENE_FM);

    jlstream_node_ioctl(g_fm_recorder->stream, NODE_UUID_IIS0_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);
    jlstream_node_ioctl(g_fm_recorder->stream, NODE_UUID_IIS1_TX, NODE_IOC_SET_PARAM, AUDIO_NETWORK_LOCAL);

    err = jlstream_node_ioctl(g_fm_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
    if (latency == 0) {
        latency = 100000;
    }
    jlstream_node_ioctl(g_fm_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
    jlstream_node_ioctl(g_fm_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, FM_ADC_IRQ_POINTS);

    err = jlstream_ioctl(g_fm_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(g_fm_recorder->stream);
    }
    if (err) {
        jlstream_release(g_fm_recorder->stream);
        free(g_fm_recorder);
        g_fm_recorder = NULL;
        return err;
    }
    return 0;
}

void le_audio_fm_recorder_close(void)
{
    struct le_audio_fm_recorder *fm_recorder = g_fm_recorder;
    if (!fm_recorder) {
        return;
    }
    if (fm_recorder->stream) {
        jlstream_stop(fm_recorder->stream, 0);
        jlstream_release(fm_recorder->stream);
    }

    free(fm_recorder);
    g_fm_recorder = NULL;
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"fm_le_audio");
}
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_BIS_TX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN)))

static void mic_recorder_callback(void *private_data, int event)
{
    printf("le audio mic recorder callback : %d\n", event);
}

int le_audio_mic_recorder_open(void *params, void *le_audio, int latency)
{
    if (g_mic_recorder) {
        return 0;
    }
    int err = 0;
    struct le_audio_stream_params *lea_params = params;
    struct le_audio_stream_format *le_audio_fmt = &lea_params->fmt;
    u16 source_uuid = 0;
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
#endif
    struct stream_enc_fmt fmt = {
        .channel = le_audio_fmt->nch,
        .bit_rate = le_audio_fmt->bit_rate,
        .sample_rate = le_audio_fmt->sample_rate,
        .frame_dms = le_audio_fmt->frame_dms,
        .coding_type = le_audio_fmt->coding_type,
    };
    if (!g_mic_recorder) {
        g_mic_recorder = zalloc(sizeof(struct le_audio_mic_recorder));
        if (!g_mic_recorder) {
            return -ENOMEM;
        }
    }
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    g_mic_recorder->stream = jlstream_pipeline_parse_by_node_name(uuid, "le_audio_adc");
    source_uuid = NODE_UUID_ADC;
#else
    g_mic_recorder->stream = voice_call_stream_search(&source_uuid, VOICE_CALL_TYPE_LE_AUDIO);
#endif

    if (!g_mic_recorder->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

#if TCFG_AUDIO_ANC_ENABLE
    audio_anc_mic_gain_check(1);

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_ESCO, 1);
    audio_icsd_adt_reset(ADT_SCENE_ESCO);
#endif
#endif

    jlstream_set_callback(g_mic_recorder->stream, NULL, mic_recorder_callback);
    if (lea_params->service_type == LEA_SERVICE_CALL) {
        //printf("LEA Recoder:LEA_CALL\n");
        jlstream_set_scene(g_mic_recorder->stream, STREAM_SCENE_LEA_CALL);
        jlstream_node_ioctl(g_mic_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, 256);
    } else if (lea_params->service_type == LEA_SERVICE_WL_MIC) {
        //printf("LEA Recoder:WL_MIC\n");
        jlstream_set_scene(g_mic_recorder->stream, STREAM_SCENE_WIRELESS_MIC);
        u16 irq_point = 0;
#if (defined(TCFG_ADC_IRQ_INTERVAL) &&  TCFG_ADC_IRQ_INTERVAL) //根据配置设置中断点数
#if (defined(TCFG_AUDIO_ADC_SAMPLE_RATE) &&  TCFG_AUDIO_ADC_SAMPLE_RATE)
        irq_point =  TCFG_AUDIO_ADC_SAMPLE_RATE * TCFG_ADC_IRQ_INTERVAL / 1000000;  //sr * interval(us) / 1000000
#else
        irq_point =  fmt.sample_rate * TCFG_ADC_IRQ_INTERVAL / 1000000; //sr * interval(us) / 1000000
#endif
#else
        irq_point =  fmt.sample_rate * 1000 / 1000000; //默认1ms 的中断,//sr * interval(us) / 1000000
#endif
        jlstream_node_ioctl(g_mic_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, irq_point);
    } else {
        //printf("LEA Recoder:MIC\n");
        jlstream_set_scene(g_mic_recorder->stream, STREAM_SCENE_MIC);
        jlstream_node_ioctl(g_mic_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, 160);
    }
    if (latency == 0) {
        latency = 100000;
    }

    jlstream_node_ioctl(g_mic_recorder->stream, NODE_UUID_CAPTURE_SYNC, NODE_IOC_SET_PARAM, latency);
    jlstream_node_ioctl(g_mic_recorder->stream, NODE_UUID_LE_AUDIO_SOURCE, NODE_IOC_SET_BTADDR, (int)le_audio);
    //设置中断点数
    /* jlstream_node_ioctl(g_mic_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS); */

#ifndef CONFIG_WIRELESS_MIC_CASE_ENABLE
    struct stream_fmt s_fmt;
    struct cvp_param_fmt cvp_fmt;
    jlstream_node_ioctl(g_mic_recorder->stream, NODE_UUID_SOURCE, NODE_IOC_GET_FMT, (int)&s_fmt);
    cvp_fmt.mic_num = s_fmt.channel_mode >> 4;
    u16 node_uuid = get_cvp_node_uuid();
    if (node_uuid) {
        u32 ref_sr = audio_dac_get_sample_rate(&dac_hdl);
        cvp_fmt.source_uuid = source_uuid;
        jlstream_node_ioctl(g_mic_recorder->stream, node_uuid, NODE_IOC_SET_FMT, (int)ref_sr);
        err = jlstream_node_ioctl(g_mic_recorder->stream, node_uuid, NODE_IOC_SET_PRIV_FMT, (int)&cvp_fmt);
        //根据回音消除的类型，将配置传递到对应的节点
        if (err && (err != -ENOENT)) {	//兼容没有cvp节点的情况
            goto __exit1;
        }
    }
#endif

    err = jlstream_ioctl(g_mic_recorder->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(g_mic_recorder->stream);
    }
    if (err) {
        goto __exit1;
    }

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    char *vol_name = "Vol_WMicTX";
    struct volume_cfg cfg = {0};
    int ret = jlstream_get_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg));
    if (ret > 0) {
        g_le_audio_tx_vol.max_dvol = (cfg.cfg_level_max >= cfg.cur_vol) ? cfg.cfg_level_max : cfg.cur_vol;
        g_le_audio_tx_vol.dvol_step = g_le_audio_tx_vol.max_dvol / cfg.cfg_level_max;
        ret = syscfg_read(CFG_WIRELESS_MIC0_VOLUME, &g_le_audio_tx_vol.dvol, 2);
        if (ret > 0) {
            /* g_le_audio_tx_vol.dvol = app_var.mic_eff_volume; //先使用mic_effect的音量配置 */
            if (g_le_audio_tx_vol.dvol != cfg.cur_vol) {
                le_audio_wireless_mic_tx_set_dvol(g_le_audio_tx_vol.dvol, 0xff); //有记录的音量先用记录的音量
            }
        } else {
            g_le_audio_tx_vol.dvol = cfg.cur_vol;
            le_audio_wireless_mic_tx_set_dvol(g_le_audio_tx_vol.dvol, 0xff);
        }
        printf(" %s dvol:%d\n", __FUNCTION__, g_le_audio_tx_vol.dvol);
    }

    vol_name = "Vol_WMonitor";
    memset(&cfg, 0, sizeof(struct volume_cfg));
    ret = jlstream_get_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg));
    if (ret > 0) {
        g_le_audio_tx_monitor_vol.max_dvol = (cfg.cfg_level_max >= cfg.cur_vol) ? cfg.cfg_level_max : cfg.cur_vol;
        g_le_audio_tx_monitor_vol.dvol_step =  g_le_audio_tx_monitor_vol.max_dvol / cfg.cfg_level_max;
        ret = syscfg_read(CFG_WIRELESS_MIC1_VOLUME, &g_le_audio_tx_monitor_vol.dvol, 2);
        if (ret > 0) {
            /* g_le_audio_tx_monitor_vol.dvol = app_var.mic_eff_volume; //先使用mic_effect的音量配置 */
            if (g_le_audio_tx_monitor_vol.dvol != cfg.cur_vol) {
                le_audio_wireless_mic_tx_monitor_set_dvol(g_le_audio_tx_monitor_vol.dvol, 0xff); //有记录的音量先用记录的音量
            }
        } else {
            g_le_audio_tx_monitor_vol.dvol = cfg.cur_vol;
            le_audio_wireless_mic_tx_monitor_set_dvol(g_le_audio_tx_monitor_vol.dvol, 0xff);
        }
        printf(" %s dvol:%d\n", __FUNCTION__, g_le_audio_tx_monitor_vol.dvol);
    }

#endif
    printf("le_audio mic recorder open success  \n");
    return 0;

__exit1:
    jlstream_release(g_mic_recorder->stream);
__exit0:
    free(g_mic_recorder);
    g_mic_recorder = NULL;

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_ESCO, 0);
    audio_icsd_adt_reset(ADT_SCENE_ESCO);
#endif
    return err;
}

void le_audio_mic_recorder_close(void)
{
    struct le_audio_mic_recorder *mic_recorder = g_mic_recorder;

    printf("le_audio_mic_recorder_close\n");

    if (!mic_recorder) {
        return;
    }
    if (mic_recorder->stream) {
        jlstream_stop(mic_recorder->stream, 0);
        jlstream_release(mic_recorder->stream);
    }
    free(mic_recorder);
    g_mic_recorder = NULL;

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_ESCO, 0);
    audio_icsd_adt_reset(ADT_SCENE_ESCO);
#endif

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic_effect");
#else
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic_le_audio_call");
#endif
}

bool le_audio_mic_recorder_is_running(void)
{
    if (g_mic_recorder) {
        return 1;
    }
    return 0;
}
#endif

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE

static void le_audio_tx_volume_save_do(void *priv)
{
    if (++g_le_audio_tx_vol.save_vol_cnt >= 5) {
        sys_timer_del(g_le_audio_tx_vol.save_vol_timer);
        g_le_audio_tx_vol.save_vol_timer = 0;
        g_le_audio_tx_vol.save_vol_cnt = 0;
        printf("save le audio vol:%d\n", g_le_audio_tx_vol.dvol);
        syscfg_write(CFG_WIRELESS_MIC0_VOLUME, &g_le_audio_tx_vol.dvol, 2);
        return;
    }
}

static void le_audio_tx_volume_change(void)
{
    g_le_audio_tx_vol.save_vol_cnt = 0;
    if (g_le_audio_tx_vol.save_vol_timer == 0) {
        g_le_audio_tx_vol.save_vol_timer = sys_timer_add(NULL, le_audio_tx_volume_save_do, 1000);//中断里不能操作vm 关中断不能操作vm
    }
}

static void le_audio_tx_monitor_volume_save_do(void *priv)
{
    if (++g_le_audio_tx_monitor_vol.save_vol_cnt >= 5) {
        sys_timer_del(g_le_audio_tx_monitor_vol.save_vol_timer);
        g_le_audio_tx_monitor_vol.save_vol_timer = 0;
        g_le_audio_tx_monitor_vol.save_vol_cnt = 0;
        printf("save le audio tx monitor vol:%d\n", g_le_audio_tx_monitor_vol.dvol);
        syscfg_write(CFG_WIRELESS_MIC1_VOLUME, &g_le_audio_tx_monitor_vol.dvol, 2);
        return;
    }
}

static void le_audio_tx_monitor_volume_change(void)
{
    g_le_audio_tx_monitor_vol.save_vol_cnt = 0;
    if (g_le_audio_tx_monitor_vol.save_vol_timer == 0) {
        g_le_audio_tx_monitor_vol.save_vol_timer = sys_timer_add(NULL, le_audio_tx_monitor_volume_save_do, 1000);//中断里不能操作vm 关中断不能操作vm
    }
}


int le_audio_wireless_mic_tx_set_dvol(u8 vol, s16 mute_en)
{
    if (cpu_in_irq() || cpu_irq_disabled()) {
        printf("[ERROR]CPU in irq or CPU irq disable, Operation not permitted");
        return -EPERM;
    }

    if ((0xff != mute_en) && (0 != mute_en) && (1 != mute_en)) {
        printf("[ERROR]mute_en, Invalid argument");
        return -EINVAL;
    }

    char *vol_name = "Vol_WMicTX";
    struct volume_cfg cfg = {0};
    if (0xff == mute_en) {
        cfg.bypass = VOLUME_NODE_CMD_SET_VOL;
        cfg.cur_vol = vol;
    } else {
        cfg.bypass = VOLUME_NODE_CMD_SET_MUTE;
        cfg.cur_vol = mute_en;
    }
    int err = jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg)) ;
    if (err < 0) {
        return -1;
    }
    printf("le audo tx dvol name: %s, le audo tx dvol:%d\n", vol_name, vol);
    if ((0xff == mute_en) && (vol != g_le_audio_tx_vol.dvol)) {
        g_le_audio_tx_vol.dvol = vol;
        le_audio_tx_volume_change();
    }
    return 0;
}

void le_audio_wireless_mic_tx_dvol_mute(bool mute)
{
    static u8 volume_before_muting = 0;

    if (mute) {
        //mute
        g_le_audio_tx_vol.mute = 1;
        volume_before_muting = g_le_audio_tx_vol.dvol;
        if (!volume_before_muting || volume_before_muting > g_le_audio_tx_vol.max_dvol) {
            printf("volume_before_muting %d record error", volume_before_muting);
            volume_before_muting = g_le_audio_tx_vol.max_dvol;
        }
        le_audio_wireless_mic_tx_set_dvol(0, mute);
    } else {
        //unmute
        g_le_audio_tx_vol.mute = 0;
        if (!volume_before_muting || volume_before_muting > g_le_audio_tx_vol.max_dvol) {
            printf("volume_before_muting %d record error", volume_before_muting);
            volume_before_muting = g_le_audio_tx_vol.max_dvol;
        }

        le_audio_wireless_mic_tx_set_dvol(volume_before_muting, mute);
    }
}

void le_audio_wireless_mic_tx_dvol_up(void)
{
    //mute时不允许调音量
    if (g_le_audio_tx_vol.mute) {
        return;
    }

    if (g_le_audio_tx_vol.dvol < g_le_audio_tx_vol.max_dvol) {
        g_le_audio_tx_vol.dvol += g_le_audio_tx_vol.dvol_step;
        le_audio_wireless_mic_tx_set_dvol(g_le_audio_tx_vol.dvol, 0xff);
    } else {
        printf("[WARING]le audio tx volum is max\n");
    }

}

void le_audio_wireless_mic_tx_dvol_down(void)
{
    //mute时不允许调音量
    if (g_le_audio_tx_vol.mute) {
        return;
    }

    if (g_le_audio_tx_vol.dvol) {
        g_le_audio_tx_vol.dvol -= g_le_audio_tx_vol.dvol_step;
        le_audio_wireless_mic_tx_set_dvol(g_le_audio_tx_vol.dvol, 0xff);
    } else {
        printf("[WARING]le audio tx volum is min\n");
    }
}

int le_audio_wireless_mic_tx_monitor_set_dvol(u8 vol, s16 mute_en)
{
    if (cpu_in_irq() || cpu_irq_disabled()) {
        printf("[ERROR]CPU in irq or CPU irq disable, Operation not permitted");
        return -EPERM;
    }

    if ((0xff != mute_en) && (0 != mute_en) && (1 != mute_en)) {
        printf("[ERROR]mute_en, Invalid argument");
        return -EINVAL;
    }

    char *vol_name = "Vol_WMonitor";
    struct volume_cfg cfg = {0};
    if (0xff == mute_en) {
        cfg.bypass = VOLUME_NODE_CMD_SET_VOL;
        cfg.cur_vol = vol;
    } else {
        cfg.bypass = VOLUME_NODE_CMD_SET_MUTE;
        cfg.cur_vol = mute_en;
    }
    int err = jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg)) ;
    if (err < 0) {
        return -1;
    }
    printf("le audo tx monitor dvol name: %s, le audo tx dvol:%d\n", vol_name, vol);
    if ((0xff == mute_en) && (vol != g_le_audio_tx_monitor_vol.dvol)) {
        g_le_audio_tx_monitor_vol.dvol = vol;
        le_audio_tx_volume_change();
    }
    return 0;
}

void le_audio_wireless_mic_tx_monitor_dvol_mute(bool mute)
{
    static u8 volume_before_muting = 0;

    if (mute) {
        //mute
        g_le_audio_tx_monitor_vol.mute = 1;
        volume_before_muting = g_le_audio_tx_monitor_vol.dvol;
        if (!volume_before_muting || volume_before_muting > g_le_audio_tx_monitor_vol.max_dvol) {
            printf("volume_before_muting %d record error", volume_before_muting);
            volume_before_muting = g_le_audio_tx_monitor_vol.max_dvol;
        }
        le_audio_wireless_mic_tx_monitor_set_dvol(0, mute);
    } else {
        //unmute
        g_le_audio_tx_monitor_vol.mute = 0;
        if (!volume_before_muting || volume_before_muting > g_le_audio_tx_monitor_vol.max_dvol) {
            printf("volume_before_muting %d record error", volume_before_muting);
            volume_before_muting = g_le_audio_tx_monitor_vol.max_dvol;
        }

        le_audio_wireless_mic_tx_monitor_set_dvol(volume_before_muting, mute);
    }
}

void le_audio_wireless_mic_tx_monitor_dvol_up(void)
{
    //mute时不允许调音量
    if (g_le_audio_tx_monitor_vol.mute) {
        return;
    }

    if (g_le_audio_tx_monitor_vol.dvol < g_le_audio_tx_monitor_vol.max_dvol) {
        g_le_audio_tx_monitor_vol.dvol += g_le_audio_tx_monitor_vol.dvol_step;
        le_audio_wireless_mic_tx_monitor_set_dvol(g_le_audio_tx_monitor_vol.dvol, 0xff);
    } else {
        printf("[WARING]le audio tx volum is max\n");
    }

}

void le_audio_wireless_mic_tx_monitor_dvol_down(void)
{
    //mute时不允许调音量
    if (g_le_audio_tx_monitor_vol.mute) {
        return;
    }

    if (g_le_audio_tx_monitor_vol.dvol) {
        g_le_audio_tx_monitor_vol.dvol -= g_le_audio_tx_monitor_vol.dvol_step;
        le_audio_wireless_mic_tx_monitor_set_dvol(g_le_audio_tx_monitor_vol.dvol, 0xff);
    } else {
        printf("[WARING]le audio tx volum is min\n");
    }
}
#endif

