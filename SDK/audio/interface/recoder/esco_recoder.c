#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".esco_recoder.data.bss")
#pragma data_seg(".esco_recoder.data")
#pragma const_seg(".esco_recoder.text.const")
#pragma code_seg(".esco_recoder.text")
#endif
#include "jlstream.h"
#include "esco_recoder.h"
#include "encoder_node.h"
#include "sdk_config.h"
#include "media/includes.h"
#include "app_config.h"
#include "audio_dac.h"
#include "audio_cvp.h"
#include "esco_player.h"
#include "cvp_node.h"

#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif

extern struct audio_dac_hdl dac_hdl;
struct esco_recoder {
    struct jlstream *stream;
};

static struct esco_recoder *g_esco_recoder = NULL;


static void esco_recoder_callback(void *private_data, int event)
{
    struct esco_recoder *recoder = g_esco_recoder;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:
        break;
    }
}

int esco_recoder_open(u8 link_type, void *bt_addr)
{
    int ext_type = ESCO_RECODER_EXT_TYPE_NONE;
    if (link_type == JL_DOGLE_ACL) {
        ext_type = ESCO_RECODER_EXT_TYPE_JL_DONGLE_ACL;
    }
    return esco_recoder_open_extended(bt_addr, ext_type, NULL);
}


struct jlstream *voice_call_stream_search(u16 *source_uuid, VOICE_CALL_TYPE type)
{
    struct jlstream *stream = NULL;
    u16 uuid_temp;

    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"esco");
    if (uuid == 0) {
        return NULL;
    }

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    if (type == VOICE_CALL_TYPE_ESCO) {
        stream = jlstream_pipeline_parse_by_node_name(uuid, "esco_adc");
    }
    if (type == VOICE_CALL_TYPE_LE_AUDIO) {
        stream = jlstream_pipeline_parse_by_node_name(uuid, "le_audio_adc");
    }
    if (!stream) {
        stream = jlstream_pipeline_parse(uuid, NODE_UUID_ADC);
    }
#else
    stream = jlstream_pipeline_parse(uuid, NODE_UUID_ADC);
#endif
    uuid_temp = NODE_UUID_ADC;
    if (!stream) {
        stream = jlstream_pipeline_parse(uuid, NODE_UUID_PDM_MIC);
        uuid_temp = NODE_UUID_PDM_MIC;
    }
    if (!stream) {
        stream = jlstream_pipeline_parse(uuid, NODE_UUID_IIS0_RX);
        uuid_temp = NODE_UUID_IIS0_RX;
    }
    if (source_uuid) {
        *source_uuid = uuid_temp;
    }
    return stream;
}

int esco_recoder_open_extended(void *bt_addr, int ext_type, void *ext_param)
{
    int err;
    struct encoder_fmt enc_fmt;
    struct esco_recoder *recoder;
    struct stream_fmt fmt;
    struct cvp_param_fmt cvp_fmt;
    u16 source_uuid = 0;

    if (g_esco_recoder) {
        return -EBUSY;
    }
    recoder = malloc(sizeof(*recoder));
    if (!recoder) {
        return -ENOMEM;
    }

    recoder->stream = voice_call_stream_search(&source_uuid, VOICE_CALL_TYPE_ESCO);

    if (!recoder->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

#if TCFG_AUDIO_SIDETONE_ENABLE && TCFG_SWITCH_NODE_ENABLE
    /*设置switch开头丢掉多少帧数据*/
    jlstream_node_ioctl(recoder->stream, NODE_UUID_SWITCH, NODE_IOC_SET_PRIV_FMT, 0);
#endif

    //设置ADC的中断点数
    err = jlstream_node_ioctl(recoder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, 256);
    if (err) {
        goto __exit1;
    }

    jlstream_node_ioctl(recoder->stream, NODE_UUID_ESCO_TX, NODE_IOC_SET_BTADDR, (int)bt_addr);
    jlstream_node_ioctl(recoder->stream, NODE_UUID_SOURCE, NODE_IOC_GET_FMT, (int)&fmt);
    cvp_fmt.mic_num = fmt.channel_mode >> 4;
    //设置源节点是哪个
    u16 node_uuid = get_cvp_node_uuid();
    u32 ref_sr = audio_dac_get_sample_rate(&dac_hdl);
    if (node_uuid) {
#if !(TCFG_AUDIO_CVP_OUTPUT_WAY_IIS_ENABLE && (defined TCFG_IIS_NODE_ENABLE))
#if (TCFG_ESCO_DL_CVSD_SR_USE_16K > 1)
        jlstream_node_ioctl(recoder->stream, node_uuid, NODE_IOC_SET_FMT, (int)ref_sr);
#endif
#endif
        cvp_fmt.source_uuid = source_uuid;
        err = jlstream_node_ioctl(recoder->stream, node_uuid, NODE_IOC_SET_PRIV_FMT, (int)&cvp_fmt);
        if (err && (err != -ENOENT)) {	//兼容没有cvp节点的情况
            goto __exit1;
        }
    }

#if TCFG_AI_TX_NODE_ENABLE
    if (ext_type == ESCO_RECODER_EXT_TYPE_AI) {
        struct stream_enc_fmt *s_enc_fmt = ext_param;
        struct stream_enc_fmt ai_tx_s_enc_fmt = {0};
        jlstream_node_ioctl(recoder->stream, NODE_UUID_ENCODER, NODE_IOC_GET_ENC_FMT, (int)&ai_tx_s_enc_fmt);
        if (s_enc_fmt && s_enc_fmt->coding_type == AUDIO_CODING_OPUS) {
#if TCFG_ENC_OPUS_ENABLE
            ai_tx_s_enc_fmt.coding_type = s_enc_fmt->coding_type;
            ai_tx_s_enc_fmt.bit_rate = s_enc_fmt->bit_rate;
            ai_tx_s_enc_fmt.sample_rate = s_enc_fmt->sample_rate;
            ai_tx_s_enc_fmt.frame_dms = s_enc_fmt->frame_dms;
            ai_tx_s_enc_fmt.channel = s_enc_fmt->channel;
            //jlstream_node_ioctl(recoder->stream, NODE_UUID_ENCODER, NODE_IOC_SET_ENC_FMT, (int)&ai_tx_s_enc_fmt);
            // opus单声道编码，通话上下行名字区分两个编码器
            err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL2", NODE_IOC_SET_ENC_FMT, &ai_tx_s_enc_fmt, sizeof(ai_tx_s_enc_fmt));
            if (err) {
                // opus立体声编码，通话上下行共用编码器，一个名字
                err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL", NODE_IOC_SET_ENC_FMT, &ai_tx_s_enc_fmt, sizeof(ai_tx_s_enc_fmt));
            }
            if (err) {
                goto __exit1;
            }
            struct encoder_fmt ai_tx_enc_fmt = {0};
            ai_tx_enc_fmt.complexity = 0;
            ai_tx_enc_fmt.format = 0;
            ai_tx_enc_fmt.frame_dms = s_enc_fmt->frame_dms;
            ai_tx_enc_fmt.ch_num = s_enc_fmt->channel;
            //jlstream_node_ioctl(recoder->stream, NODE_UUID_ENCODER, NODE_IOC_SET_PRIV_FMT, (int)&ai_tx_enc_fmt);
            err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL2", NODE_IOC_SET_PRIV_FMT, &ai_tx_enc_fmt, sizeof(ai_tx_enc_fmt));
            if (err) {
                err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL", NODE_IOC_SET_PRIV_FMT, &ai_tx_enc_fmt, sizeof(ai_tx_enc_fmt));
            }
            if (err) {
                goto __exit1;
            }
#endif
        } else if (s_enc_fmt && s_enc_fmt->coding_type == AUDIO_CODING_JLA_V2) {
#if TCFG_ENC_JLA_V2_ENABLE
            ai_tx_s_enc_fmt.coding_type = s_enc_fmt->coding_type;
            ai_tx_s_enc_fmt.sample_rate = s_enc_fmt->sample_rate;
            ai_tx_s_enc_fmt.frame_dms = s_enc_fmt->frame_dms;
            ai_tx_s_enc_fmt.bit_rate = s_enc_fmt->bit_rate;
            ai_tx_s_enc_fmt.channel = s_enc_fmt->channel;
            //jlstream_node_ioctl(recoder->stream, NODE_UUID_ENCODER, NODE_IOC_SET_ENC_FMT, (int)&ai_tx_s_enc_fmt);
            err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL2", NODE_IOC_SET_ENC_FMT, &ai_tx_s_enc_fmt, sizeof(ai_tx_s_enc_fmt));
            if (err) {
                err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL", NODE_IOC_SET_ENC_FMT, &ai_tx_s_enc_fmt, sizeof(ai_tx_s_enc_fmt));
            }
            if (err) {
                goto __exit1;
            }
#endif
        }
    }
#endif

    if (ext_type == ESCO_RECODER_EXT_TYPE_JL_DONGLE_ACL) { //连接方式为ACL  msbc 编码需要用软件; aec 参考采样率为48000;
        enc_fmt.sw_hw_option = 1; //连接方式时ACL msbc 编码需要用软件
        //设置编码参数
        err = jlstream_node_ioctl(recoder->stream, NODE_UUID_ENCODER, NODE_IOC_SET_PRIV_FMT, (int)(&enc_fmt));
        //根据回音消除的类型，将配置传递到对应的节点
        if (node_uuid) {
            err = jlstream_node_ioctl(recoder->stream, node_uuid, NODE_IOC_SET_FMT, (int)ref_sr);
        }
        if (err && (err != -ENOENT)) {	//兼容没有节点的情况
            goto __exit1;
        }
    }


    jlstream_set_callback(recoder->stream, recoder->stream, esco_recoder_callback);
    jlstream_set_scene(recoder->stream, STREAM_SCENE_ESCO);

#if 0 //通话与ANC互斥demo
    if (anc_mode_get() != ANC_OFF) {
        anc_mode_switch_pend(ANC_OFF, 0);
    }
#endif

    err = jlstream_start(recoder->stream);
    if (err) {
        goto __exit1;
    }

    g_esco_recoder = recoder;

    return 0;

__exit1:
    jlstream_release(recoder->stream);
__exit0:
    free(recoder);
    return err;
}

int esco_recoder_running()
{
    return g_esco_recoder != NULL;
}

void esco_recoder_set_ai_tx_node_func(int (*func)(u8 *, u32))
{
    struct esco_recoder *recoder = g_esco_recoder;

    if (recoder && recoder->stream) {
        jlstream_node_ioctl(recoder->stream, NODE_UUID_AI_TX, NODE_IOC_SET_PRIV_FMT, (int)func);
    }
}

void esco_recoder_close()
{
    struct esco_recoder *recoder = g_esco_recoder;

    if (!recoder) {
        return;
    }
    jlstream_stop(recoder->stream, 0);
    jlstream_release(recoder->stream);

    free(recoder);
    g_esco_recoder = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_RECODER, (int)"esco");
#if TCFG_AUDIO_DUT_ENABLE
    //退出通话默认设置为算法模式
    cvp_dut_mode_set(CVP_DUT_MODE_ALGORITHM);
#endif
}


extern bool esco_player_runing();
int esco_recoder_switch(u8 en)
{
    if (!esco_player_runing()) {
        printf("esco player close now, non-operational!");
        return 1;
    }
    if (en) {
        //固定LINK_SCO类型，dongle不跑switch
        u8 bt_addr[6];
        esco_player_get_btaddr(bt_addr);
        esco_recoder_open(COMMON_SCO, bt_addr);
    } else {
        esco_recoder_close();
    }
    return 0;
}

//esco_recoder复位流程，目前提供产测使用
int esco_recoder_reset(void)
{
    if (!esco_player_runing()) {
        printf("esco player close now, non-operational!");
        return 1;
    }
    u8 bt_addr[6];
    //产测流程，固定LINK_SCO类型
    esco_recoder_close();
    esco_player_get_btaddr(bt_addr);
    esco_recoder_open(COMMON_SCO, bt_addr);
    return 0;
}
bool esco_recoder_runing()
{
    return g_esco_recoder != NULL;
}

#if TCFG_AUDIO_SIDETONE_ENABLE
static u8 sidetone_state = 1;
/*
*********************************************************************
*                  Audio Sidetone Open
* Description: 打开通话监听
* Arguments  : None.
* Return	 : 0成功 其他失败
* Note(s)    : None.
*********************************************************************
*/
int audio_sidetone_open(void)
{
    sidetone_state = 1;
    return 0;
}

/*
*********************************************************************
*                  Audio Sidetone Close
* Description: 关闭通话监听
* Arguments  : None.
* Return	 : 0成功 其他失败
* Note(s)    : None.
*********************************************************************
*/
int audio_sidetone_close(void)
{
    sidetone_state = 0;
    return 0;
}

int get_audio_sidetone_state()
{
    return sidetone_state;
}
#endif


