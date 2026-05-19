#include "jlstream.h"
#include "mic_player.h"
#include "app_config.h"
#include "effects/audio_pitchspeed.h"
#include "audio_config_def.h"
#include "scene_switch.h"

struct mic_file_player {
    struct jlstream *stream;
};

static struct mic_file_player *g_mic_player = NULL;

static void mic_player_callback(void *private_data, int event)
{
    struct mic_file_player *player = g_mic_player;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:
#ifdef TCFG_VOCAL_REMOVER_NODE_ENABLE
        music_vocal_remover_update_parm();
#endif

        break;
    }
}


int mic_player_open(void)
{
    int err;
    struct mic_file_player *player;

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

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
#else
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic");
#endif
    if (uuid == 0) {
        return -EFAULT;
    }

    player = malloc(sizeof(*player));
    if (!player) {
        return -ENOMEM;
    }

    player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_ADC);

    if (!player->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

    //设置 mic 中断点数
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS);

    jlstream_node_ioctl(player->stream, NODE_UUID_VOCAL_TRACK_SYNTHESIS, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS);//四声道时，指定声道合并单个声道的点数
    jlstream_set_callback(player->stream, player->stream, mic_player_callback);
#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    jlstream_set_scene(player->stream, STREAM_SCENE_WIRELESS_MIC);
#else
    jlstream_set_scene(player->stream, STREAM_SCENE_MIC);
#endif
#if TCFG_LOCAL_TWS_ENABLE
    err = jlstream_ioctl(player->stream, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    if (err == 0) {
        err = jlstream_start(player->stream);
    }
#else
    err = jlstream_start(player->stream);
#endif
    if (err) {
        goto __exit1;
    }

    g_mic_player = player;

    return 0;

__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);
    return err;
}

bool mic_player_runing(void)
{
    return g_mic_player != NULL;
}

void mic_player_close()
{
    struct mic_file_player *player = g_mic_player;

    if (!player) {
        return;
    }
    jlstream_stop(player->stream, 50);
    jlstream_release(player->stream);

    free(player);
    g_mic_player = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic");
}



