#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".esco_player.data.bss")
#pragma data_seg(".esco_player.data")
#pragma const_seg(".esco_player.text.const")
#pragma code_seg(".esco_player.text")
#endif
#include "jlstream.h"
#include "esco_player.h"
#include "sdk_config.h"
#include "app_config.h"
#include "aec_ref_dac_ch_data.h"
#include "audio_cvp.h"
#include "encoder_node.h"
#include "audio_anc_includes.h"

#if TCFG_BT_SUPPORT_HFP

struct esco_player {
    u8 bt_addr[6];
    struct jlstream *stream;
};

static struct esco_player *g_esco_player = NULL;
extern const int config_audio_dac_mix_enable;

static void esco_player_callback(void *private_data, int event)
{
    struct esco_player *player = g_esco_player;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:
        break;
    }
}

int esco_player_open(u8 *bt_addr)
{
    return esco_player_open_extended(bt_addr, ESCO_PLAYER_EXT_TYPE_NONE, NULL);
}

int esco_player_open_extended(u8 *bt_addr, int ext_type, void *ext_param)
{
    int err;
    struct esco_player *player;

    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"esco");
    if (uuid == 0) {
        return -EFAULT;
    }

    player = zalloc(sizeof(*player));
    if (!player) {
        return -ENOMEM;
    }
    player->stream = jlstream_pipeline_parse_by_node_name(uuid, "esco_rx");
    if (!player->stream) {
        player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_ESCO_RX);
    }
    if (!player->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

#if TCFG_AUDIO_SIDETONE_ENABLE
    if (config_audio_dac_mix_enable) {
        set_aec_ref_dac_ch_name(SIDETONE_ESCO_DAC_NAME);
        aec_ref_dac_ch_data_read_init();
    }
#endif

#if TCFG_AUDIO_ANC_ENABLE
    audio_anc_mic_gain_check(1);

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_ESCO, 1);
    audio_icsd_adt_reset(ADT_SCENE_ESCO);
#endif
#endif

#if TCFG_ESCO_DL_CVSD_SR_USE_16K
    jlstream_node_ioctl(player->stream, NODE_UUID_BT_AUDIO_SYNC, NODE_IOC_SET_PRIV_FMT, TCFG_ESCO_DL_CVSD_SR_USE_16K);
#endif /*TCFG_ESCO_DL_CVSD_SR_USE_16K*/

#if (((defined TCFG_MULTI_CH_IIS_NODE_ENABLE) && (TCFG_MULTI_CH_IIS_NODE_ENABLE == 1)) || ((defined TCFG_IIS_NODE_ENABLE) && (TCFG_IIS_NODE_ENABLE == 1))) && (TCFG_DAC_NODE_ENABLE == 0)
#if (TCFG_AUDIO_GLOBAL_SAMPLE_RATE == 48000)
    //如果涉及iis输出, 将通话的同步节点采样率改为48k, 前提是全局采样率已经设置为48000
    jlstream_node_ioctl(player->stream, NODE_UUID_BT_AUDIO_SYNC, NODE_IOC_SET_PRIV_FMT, 3);
#else
    //报错，需要客户自己检查把关
    r_printf("If esco failed, Please check if the call stream is IIS output? and global sample rate is 48000? \n");
#endif
#endif

#if TCFG_AI_TX_NODE_ENABLE
    if (ext_type == ESCO_PLAYER_EXT_TYPE_AI) {
        struct stream_enc_fmt *s_enc_fmt = ext_param;
        struct stream_enc_fmt ai_tx_s_enc_fmt = {0};
        jlstream_node_ioctl(player->stream, NODE_UUID_ENCODER, NODE_IOC_GET_ENC_FMT, (int)&ai_tx_s_enc_fmt);
        if (s_enc_fmt && s_enc_fmt->coding_type == AUDIO_CODING_OPUS) {
#if TCFG_ENC_OPUS_ENABLE
            ai_tx_s_enc_fmt.coding_type = s_enc_fmt->coding_type;
            ai_tx_s_enc_fmt.bit_rate = s_enc_fmt->bit_rate;
            ai_tx_s_enc_fmt.sample_rate = s_enc_fmt->sample_rate;
            ai_tx_s_enc_fmt.frame_dms = s_enc_fmt->frame_dms;
            ai_tx_s_enc_fmt.channel = s_enc_fmt->channel;
            //jlstream_node_ioctl(player->stream, NODE_UUID_ENCODER, NODE_IOC_SET_ENC_FMT, (int)&ai_tx_s_enc_fmt);
            // opus单声道编码，通话上下行名字区分两个编码器
            err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL1", NODE_IOC_SET_ENC_FMT, &ai_tx_s_enc_fmt, sizeof(ai_tx_s_enc_fmt));
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
            //jlstream_node_ioctl(player->stream, NODE_UUID_ENCODER, NODE_IOC_SET_PRIV_FMT, (int)&ai_tx_enc_fmt);
            err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL1", NODE_IOC_SET_PRIV_FMT, &ai_tx_enc_fmt, sizeof(ai_tx_enc_fmt));
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
            //jlstream_ioctl(player->stream, NODE_IOC_SET_ENC_FMT, (int)&ai_tx_s_enc_fmt);
            err = jlstream_set_node_specify_param(NODE_UUID_ENCODER, "ENCODE_AI_CALL1", NODE_IOC_SET_ENC_FMT, &ai_tx_s_enc_fmt, sizeof(ai_tx_s_enc_fmt));
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

    jlstream_set_callback(player->stream, player->stream, esco_player_callback);
    jlstream_set_scene(player->stream, STREAM_SCENE_ESCO);
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_BTADDR, (int)bt_addr);

#if TCFG_NS_NODE_ENABLE
    jlstream_node_ioctl(player->stream, NODE_UUID_NOISE_SUPPRESSOR, NODE_IOC_SET_BTADDR, (int)bt_addr);
#endif

#if TCFG_DNS_NODE_ENABLE
    jlstream_node_ioctl(player->stream, NODE_UUID_DNS_NOISE_SUPPRESSOR, NODE_IOC_SET_BTADDR, (int)bt_addr);
#endif

    err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }

    memcpy(player->bt_addr, bt_addr, 6);
    g_esco_player = player;

#if (TCFG_AUDIO_CVP_OUTPUT_WAY_IIS_ENABLE && (TCFG_IIS_NODE_ENABLE || TCFG_MULTI_CH_IIS_NODE_ENABLE))
    //获取用的哪个iis模块， 哪个通道
    audio_cvp_ref_alink_cfg_get(uuid);
#endif

    return 0;

__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_ESCO, 0);
    audio_icsd_adt_reset(ADT_SCENE_ESCO);
#endif

    return err;
}

bool esco_player_runing()
{
    return g_esco_player != NULL;
}

int esco_player_get_btaddr(u8 *btaddr)
{
    if (g_esco_player) {
        memcpy(btaddr, g_esco_player->bt_addr, 6);
        return 1;
    }
    return 0;
}

/*
	ESCO状态检测，
	param:btaddr 目标蓝牙地址, 传NULL则只检查耳机状态
 */
int esco_player_is_playing(u8 *btaddr)
{
    int cur_addr = 1;
    if (btaddr && g_esco_player) {	//当前蓝牙地址检测
        cur_addr = (!memcmp(btaddr, g_esco_player->bt_addr, 6));
    }
    if (g_esco_player && cur_addr) {
        return true;
    }
    return false;
}

void esco_player_set_ai_tx_node_func(int (*func)(u8 *, u32))
{
    struct esco_player *player = g_esco_player;

    if (player && player->stream) {
        jlstream_node_ioctl(player->stream, NODE_UUID_AI_TX, NODE_IOC_SET_PRIV_FMT, (int)func);
    }
}

void esco_player_close()
{
    struct esco_player *player = g_esco_player;

    if (!player) {
        return;
    }

    jlstream_stop(player->stream, 0);
    jlstream_release(player->stream);

    free(player);
    g_esco_player = NULL;

#if TCFG_AUDIO_SIDETONE_ENABLE
    if (config_audio_dac_mix_enable) {
        aec_ref_dac_ch_data_read_exit();
    }
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_ESCO, 0);
    audio_icsd_adt_reset(ADT_SCENE_ESCO);
#endif

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"esco");
}
#else
bool esco_player_runing()
{
    return 0;
}
int esco_player_get_btaddr(u8 *btaddr)
{
    return 0;
}
int esco_player_is_playing(u8 *btaddr)
{
    return false;
}
int esco_player_open(u8 *bt_addr)
{
    return -EFAULT;
}
void esco_player_close()
{
}

#endif
