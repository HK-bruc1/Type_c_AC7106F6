#include "jlstream.h"
#include "iis_player.h"
#include "app_config.h"
#include "effects/audio_pitchspeed.h"
#include "audio_config_def.h"
#include "scene_switch.h"
#include "effects/audio_llns_dns.h"
#include "node_param_update.h"


struct iis_file_player {
    struct jlstream *stream;
};

static struct iis_file_player *g_iis_player = NULL;
static u8 wait_iis_player_open_flag = 0;
static u8 wait_iis_player_close_flag = 0;

extern u8 get_dsp_llns1_bypass_status(void);
extern u8 get_dsp_llns2_bypass_status(void);

static void iis_player_callback(void *private_data, int event)
{
    struct iis_file_player *player = g_iis_player;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:
#ifdef TCFG_VOCAL_REMOVER_NODE_ENABLE
        music_vocal_remover_update_parm();
#endif
        break;
    }
}

void llns_dns_bypass_reset_latency(void *priv)
{

    struct iis_file_player *player = (struct iis_file_player *)priv;

#if (TCFG_AS_WIRELESS_MIC_DSP_ENABLE && TCFG_LLNS_DNS_NODE_ENABLE)
    int bypass_latency = 8000; //us
    int node_bypass1 = get_dsp_llns1_bypass_status();
    int node_bypass2 = get_dsp_llns2_bypass_status();

    if (node_bypass1 && node_bypass2) {
        g_printf("reset play latency:%d", bypass_latency);
        //llns_dns bypass后重新设置播放同步节点延时
        jlstream_node_ioctl(player->stream, NODE_UUID_PLAY_SYNC, NODE_IOC_SET_PARAM, bypass_latency);
        jlstream_set_node_specify_param(NODE_UUID_LLNS_DNS, "LLNS_DNS3", NODE_IOC_SET_PRIV_FMT, &node_bypass1, sizeof(node_bypass1));
        jlstream_set_node_specify_param(NODE_UUID_LLNS_DNS, "LLNS_DNS4", NODE_IOC_SET_PRIV_FMT, &node_bypass2, sizeof(node_bypass2));
    }
    y_printf("llns_dns1_bypass:%d, llns_dns2_bypass:%d", node_bypass1, node_bypass2);
#endif
}

int iis_player_open(void)
{
    int err;
    struct iis_file_player *player;

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
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"iis");
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

    //设置 IIS 中断点数
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_IIS_IRQ_POINTS);
    jlstream_set_callback(player->stream, player->stream, iis_player_callback);
    jlstream_set_scene(player->stream, STREAM_SCENE_IIS);
#if defined(TCFG_VIRTUAL_SURROUND_EFF_MODULE_NODE_ENABLE) && TCFG_VIRTUAL_SURROUND_EFF_MODULE_NODE_ENABLE
    //解码帧长短得情况下，使用三线程推数
    jlstream_add_thread(player->stream, "media0");
    jlstream_add_thread(player->stream, "media1");
#if defined(CONFIG_CPU_BR28)
    jlstream_add_thread(player->stream, "media2");
#endif
#endif

#if (TCFG_AS_WIRELESS_MIC_DSP_ENABLE && TCFG_LLNS_DNS_NODE_ENABLE)
    //根据llns_dns是否bypass重设延时
    llns_dns_bypass_reset_latency(player);
    jlstream_set_node_task(player->stream, NODE_UUID_LLNS_DNS, "LLNS_DNS3", "llns_dns");
    jlstream_set_node_task(player->stream, NODE_UUID_LLNS_DNS, "LLNS_DNS4", "llns_dns1");
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

    g_iis_player = player;

    return 0;

__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);
    return err;
}

bool iis_player_runing(void)
{
    return g_iis_player != NULL;
}

void iis_player_close()
{
    struct iis_file_player *player = g_iis_player;

    if (!player) {
        return;
    }
    jlstream_stop(player->stream, 50);
    jlstream_release(player->stream);

    free(player);
    g_iis_player = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"iis");
}

static void iis_open_player(int arg)
{
    printf("================ open iis player\n");
    if (!g_iis_player) {
        iis_player_open();
    }
    wait_iis_player_open_flag = 0;
}

/*
 * @description: 在app_core 线程打开 iis 数据流
 * @return：0 表示消息发送成功
 *			-1 表示已经成功发送了消息
 * @node:
 */
int iis_open_player_by_taskq(void)
{
    if (wait_iis_player_open_flag == 1) {
        return -1;
    }
    int msg[2];
    msg[0] = (int)iis_open_player;
    msg[1] = 0;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, msg);
    if (ret == 0) {
        //消息发送成功, 等待iis player 打开
        wait_iis_player_open_flag = 1;
    }
    return ret;
}

static void iis_close_player(int arg)
{
    printf("================ close iis player\n");
    if (g_iis_player) {
        iis_player_close();
    }
    wait_iis_player_close_flag = 0;
}

/*
 * @description: 在app_core 线程关闭 iis 数据流
 * @return：0 表示消息发送成功
 *			-1 表示已经成功发送了消息
 * @node:
 */
int iis_close_player_by_taskq(void)
{
    if (wait_iis_player_close_flag == 1) {
        return -1;
    }
    int msg[2];
    msg[0] = (int)iis_close_player;
    msg[1] = 0;
    int ret = os_taskq_post_type("app_core", Q_CALLBACK, 2, msg);
    if (ret == 0) {
        //消息发送成功, 等待iis player 打开
        wait_iis_player_close_flag = 1;
    }
    return ret;
}




