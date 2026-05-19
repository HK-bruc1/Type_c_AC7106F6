#include "jlstream.h"
#include "loudspeaker_iis_player.h"
#include "app_config.h"
#include "effects/audio_pitchspeed.h"
#include "audio_config_def.h"
#include "scene_switch.h"
#include "asm/dac.h"
#include "audio_cvp.h"
#include "cvp/aec_ref_dac_ch_data.h"
#include "effects/audio_howling_ahs.h"

#if TCFG_APP_LOUDSPEAKER_EN

struct loudspeaker_iis_file_player {
    struct jlstream *stream;
};

static struct loudspeaker_iis_file_player *g_loudspeaker_iis_player = NULL;

static void loudspeaker_iis_player_callback(void *private_data, int event)
{
    struct loudspeaker_iis_file_player *player = g_loudspeaker_iis_player;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:

        break;
    }
}

int loudspeaker_iis_player_open(void)
{
    int err = 0;
    struct loudspeaker_iis_file_player *player;
    printf("loudspeaker_iis_player_open\n");
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"loudspkiis");
    if (uuid == 0) {
        return -EFAULT;
    }

    player = malloc(sizeof(*player));
    if (!player) {
        return -ENOMEM;
    }

    player->stream = jlstream_pipeline_parse_by_node_name(uuid, "SPK_IIS_RX");

    if (!player->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

    //设置 IIS 中断点数
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_IIS_IRQ_POINTS_LOUDSPEAKER_MODE);
    jlstream_node_ioctl(player->stream, NODE_UUID_VOCAL_TRACK_SYNTHESIS, NODE_IOC_SET_PRIV_FMT, AUDIO_IIS_IRQ_POINTS_LOUDSPEAKER_MODE);//四声道时，指定声道合并单个声道的点数
    jlstream_set_callback(player->stream, player->stream, loudspeaker_iis_player_callback);
    jlstream_set_scene(player->stream, STREAM_SCENE_LOUDSPEAKER_IIS);

#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
    if (config_audio_dac_mix_enable && !const_audio_howling_ahs_adc_hw_ref) {
        //软件回采
        set_aec_ref_dac_ch_name("DacSPK");
        aec_ref_dac_ch_data_read_init();
        //iis输入dac输出 回采数据需要添加固定采样率变化
        RS_PARA_STRUCT rs_para_obj;
        rs_para_obj.nch = AUDIO_CH_NUM(TCFG_AUDIO_DAC_CONNECT_MODE);
        rs_para_obj.new_insample = 624 * 20;
        rs_para_obj.new_outsample = 625 * 20;
        rs_para_obj.dataTypeobj.IndataBit = 0;
        rs_para_obj.dataTypeobj.OutdataBit = 0;
        rs_para_obj.dataTypeobj.IndataInc = AUDIO_CH_NUM(TCFG_AUDIO_DAC_CONNECT_MODE);
        rs_para_obj.dataTypeobj.OutdataInc = AUDIO_CH_NUM(TCFG_AUDIO_DAC_CONNECT_MODE);
        rs_para_obj.dataTypeobj.Qval = 15;
        aec_ref_dac_ch_data_src_init(&rs_para_obj);
        //设置回采数据采样率
        extern struct audio_dac_hdl dac_hdl;
        u32 ref_sr = audio_dac_get_sample_rate(&dac_hdl);
        jlstream_node_ioctl(player->stream, NODE_UUID_HOWLING_AHS, NODE_IOC_SET_FMT, (int)ref_sr);
    }
    u8 iis_in_dac_out = 1; //loudspeaker_iis模式自动使能，其他模式需要手动使能const_audio_howling_ahs_iis_in_dac_out变量
    //高16bit传递iis_in_dac_out使能位，低16bit传递帧长
    jlstream_node_ioctl(player->stream, NODE_UUID_HOWLING_AHS, NODE_IOC_SET_PRIV_FMT, (iis_in_dac_out << 16) | AHS_NN_FRAME_POINTS);
    jlstream_add_thread(player->stream, "mic_effect1");
    jlstream_add_thread(player->stream, "mic_effect2");
#endif
    err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }

    g_loudspeaker_iis_player = player;

    return 0;

__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);
    return err;
}

bool loudspeaker_iis_player_runing(void)
{
    return g_loudspeaker_iis_player != NULL;
}

void loudspeaker_iis_player_close()
{
    struct loudspeaker_iis_file_player *player = g_loudspeaker_iis_player;

    if (!player) {
        return;
    }

    printf("loudspeaker_iis_player_close\n");
#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
    audio_ahs_sem_post();
#endif
    jlstream_stop(player->stream, 50);
    jlstream_release(player->stream);

    free(player);
    g_loudspeaker_iis_player = NULL;
#if (defined(TCFG_HOWLING_AHS_NODE_ENABLE) && TCFG_HOWLING_AHS_NODE_ENABLE)
    if (config_audio_dac_mix_enable && !const_audio_howling_ahs_adc_hw_ref) {
        //软件回采
        aec_ref_dac_ch_data_read_exit();
    }
#endif
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"iis");
}
#endif





