/*
 *          自定义低延时DSP流程
 * 可视化界面选择：音频流程->自定义->低延时DSP
 * 数据流：IIS_RX->第三方音效算法->播放同步->IIS_TX
 */

#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".dsp_low_latency_player.data.bss")
#pragma data_seg(".dsp_low_latency_player.data")
#pragma const_seg(".dsp_low_latency_player.text.const")
#pragma code_seg(".dsp_low_latency_player.text")
#endif

#include "jlstream.h"
#include "audio_dsp_low_latency_player.h"
#include "app_config.h"
#include "audio_config_def.h"
#include "effects/effects_adj.h"

struct dsp_low_latency_player {
    struct jlstream *stream;
};

static struct dsp_low_latency_player *g_dsp_low_latency_player = NULL;

static void dsp_low_latency_player_callback(void *private_data, int event)
{
    struct dsp_low_latency_player *player = g_dsp_low_latency_player;
    struct jlstream *stream = (struct jlstream *)private_data;
    switch (event) {
    case STREAM_EVENT_START:
        break;
    }
}


int dsp_low_latency_player_open()
{
    int err;
    struct dsp_low_latency_player *player;
    if (g_dsp_low_latency_player) {
        return 0;
    }
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"user_defined");
    if (uuid == 0) {
        return -EFAULT;
    }

    player = malloc(sizeof(*player));
    if (!player) {
        return -ENOMEM;
    }

    player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_IIS0_RX);

    if (!player->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

    //设置中断点数
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_IIS_IRQ_POINTS);
    jlstream_set_callback(player->stream, player->stream, dsp_low_latency_player_callback);
    jlstream_set_scene(player->stream, STREAM_SCENE_USER_DEFINED);
    err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }
    g_dsp_low_latency_player = player;
    return 0;
__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);
    return err;
}

bool dsp_low_latency_player_runing()
{
    return g_dsp_low_latency_player != NULL;
}


void dsp_low_latency_player_close()
{
    struct dsp_low_latency_player *player = g_dsp_low_latency_player;

    if (!player) {
        return;
    }
    jlstream_stop(player->stream, 50);
    jlstream_release(player->stream);

    free(player);
    g_dsp_low_latency_player = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"user_defined");
}


