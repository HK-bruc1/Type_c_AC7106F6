#include "jlstream.h"
#include "loudspeaker_mic_player.h"
#include "app_config.h"
#include "effects/audio_pitchspeed.h"
#include "audio_config_def.h"
#include "scene_switch.h"
#include "app_main.h"
#include "audio_cvp.h"
#include "cvp/aec_ref_dac_ch_data.h"
#include "effects/audio_howling_ahs.h"

#if TCFG_APP_LOUDSPEAKER_EN

struct loudspeaker_mic_file_player {
    struct jlstream *stream;
};

static struct loudspeaker_mic_file_player *g_loudspeaker_mic_player = NULL;

static void loudspeaker_mic_player_callback(void *private_data, int event)
{
    struct loudspeaker_mic_file_player *player = g_loudspeaker_mic_player;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:

        break;
    }
}


int loudspeaker_mic_player_open(void)
{
    int err = 0;
    struct loudspeaker_mic_file_player *player;

#if TCFG_LOCAL_TWS_ENABLE
    struct local_tws_stream_params *local_tws_fmt = {0};

    struct stream_enc_fmt fmt = {
        .channel = LOCAL_TWS_CODEC_CHANNEL,
        .bit_width = LOCAL_TWS_CODEC_BIT_WIDTH,
        .frame_dms = LOCAL_TWS_CODEC_FRAME_LEN,
        .sample_rate = LOCAL_TWS_CODEC_SAMPLERATE,
        .bit_rate = LOCAL_TWS_CODEC_BIT_RATE,
        .coding_type = LOCAL_TWS_CODEC_TYPE,
    };
#endif


    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"loudspkmic");
    if (uuid == 0) {
        return -EFAULT;
    }

    player = malloc(sizeof(*player));
    if (!player) {
        return -ENOMEM;
    }

    char mic_effect_source_name[8] = "MicLine";
    player->stream = jlstream_pipeline_parse_by_node_name(uuid, mic_effect_source_name);//jlstream_pipeline_parse(uuid, NODE_UUID_ADC);

    if (!player->stream) {
        err = -ENOMEM;
        goto __exit0;
    }
    //设置 mic 中断点数
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS_LOUDSPEAKER_MODE);

    // jlstream_node_ioctl(player->stream, NODE_UUID_VOCAL_TRACK_SYNTHESIS, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS);//四声道时，指定声道合并单个声道的点数
    jlstream_set_callback(player->stream, player->stream, loudspeaker_mic_player_callback);
    jlstream_set_scene(player->stream, STREAM_SCENE_LOUDSPEAKER_MIC);
#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
    if (config_audio_dac_mix_enable && !const_audio_howling_ahs_adc_hw_ref) {
        //软件回采
        set_aec_ref_dac_ch_name("Dacline");
        aec_ref_dac_ch_data_read_init();
        //设置回采数据采样率
        extern struct audio_dac_hdl dac_hdl;
        u32 ref_sr = audio_dac_get_sample_rate(&dac_hdl);
        jlstream_node_ioctl(player->stream, NODE_UUID_HOWLING_AHS, NODE_IOC_SET_FMT, (int)ref_sr);
    }
    jlstream_node_ioctl(player->stream, NODE_UUID_HOWLING_AHS, NODE_IOC_SET_PRIV_FMT, AHS_NN_FRAME_POINTS);
    jlstream_add_thread(player->stream, "mic_effect1");
    jlstream_add_thread(player->stream, "mic_effect2");
#endif
    err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }

    g_loudspeaker_mic_player = player;

    return 0;

__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);
    return err;
}

bool loudspeaker_mic_player_runing(void)
{
    return g_loudspeaker_mic_player != NULL;
}

void loudspeaker_mic_player_close()
{
    struct loudspeaker_mic_file_player *player = g_loudspeaker_mic_player;

    if (!player) {
        return;
    }
#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
    audio_ahs_sem_post();
#endif
    jlstream_stop(player->stream, 50);
    jlstream_release(player->stream);

    free(player);
    g_loudspeaker_mic_player = NULL;
#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
    if (config_audio_dac_mix_enable && !const_audio_howling_ahs_adc_hw_ref) {
        //软件回采
        aec_ref_dac_ch_data_read_exit();
    }
#endif
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic");
}
#endif
