#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".a2dp_player.data.bss")
#pragma data_seg(".a2dp_player.data")
#pragma const_seg(".a2dp_player.text.const")
#pragma code_seg(".a2dp_player.text")
#endif
#include "jlstream.h"
#include "classic/tws_api.h"
#include "media/audio_base.h"
#include "a2dp_player.h"
#include "btstack/a2dp_media_codec.h"
#include "media/bt_audio_timestamp.h"
#include "effects/audio_pitchspeed.h"
#include "sdk_config.h"
#include "effects/audio_vbass.h"
#include "audio_effect_demo.h"
#include "audio_config_def.h"
#include "scene_switch.h"
#include "audio_anc_includes.h"
#include "audio_general_config.h"
#include "bt_tws.h"

#include "le_audio_recorder.h"

#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif/*TCFG_AUDIO_DUT_ENABLE*/

//tws音箱是否两个DAC通道都输出相同数据
#define TCFG_TWS_DUAL_CHANNEL  0

#if TCFG_SMART_VOICE_ENABLE
#include "smart_voice/smart_voice.h"
#endif

#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
#include "spatial_effects_process.h"
#include "spatial_effect.h"
#endif

#if AUDIO_EQ_LINK_VOLUME
#include "effects/eq_config.h"
#endif

#if TCFG_AUDIO_AVC_NODE_ENABLE
#include "env_noise_recoder.h"
#endif

extern struct audio_dac_hdl dac_hdl;
struct a2dp_player {
    u8 bt_addr[6];
    u16 retry_timer;
    s8 a2dp_pitch_mode;
    struct jlstream *stream;
    int channel; //记录当前是tws左声道还是右声道
#if TCFG_TWS_DUAL_CHANNEL
    stereo_mix_gain_param_tool_set steromix_cfg;
#endif
    jlstream_breaker_t breaker;
};





#if TCFG_TWS_DUAL_CHANNEL
u8  a2dp_player_update_steromix_param(struct a2dp_player *player, int channel)
{
    if (!player) {
        return -1;
    }
    switch (channel) {
    case AUDIO_CH_MIX:
        player->steromix_cfg.parm.gain[0] = 1;
        player->steromix_cfg.parm.gain[1] = 0;
        player->steromix_cfg.parm.gain[2] = 0;
        player->steromix_cfg.parm.gain[3] = 1;
        break;
    case AUDIO_CH_L:
        player->steromix_cfg.parm.gain[0] = 1;
        player->steromix_cfg.parm.gain[1] = 0;
        player->steromix_cfg.parm.gain[2] = 1;
        player->steromix_cfg.parm.gain[3] = 0;
        break;
    case AUDIO_CH_R:
        player->steromix_cfg.parm.gain[0] = 0;
        player->steromix_cfg.parm.gain[1] = 1;
        player->steromix_cfg.parm.gain[2] = 0;
        player->steromix_cfg.parm.gain[3] = 1;
        break;
    default:
        return -1;
    }
    int err = jlstream_node_ioctl(player->stream, NODE_UUID_STEROMIX, NODE_IOC_SET_PARAM, (int) & (player->steromix_cfg));
    return err ;
}
#endif

static struct a2dp_player *g_a2dp_player = NULL;
extern const int CONFIG_BTCTLER_TWS_ENABLE;

void a2dp_player_breaker_mode(u8 mode,
                              u16 uuid_a, const char *name_a,
                              u16 uuid_b, const char *name_b)
{
    struct a2dp_player *player = g_a2dp_player;
    /* printf("%s:  %d, %x, %x", __func__, mode, (int)player, (int)player->breaker); */
    if (!player) {
        return ;
    }
    if (mode) {
        if (player->breaker) {
            printf("breaker : delete\n");
            jlstream_delete_breaker(player->breaker, 1);
            player->breaker = NULL;
        }
    } else {
        if (player->breaker == NULL) {
            printf("breaker : insert\n");
            player->breaker = jlstream_insert_breaker(player->stream,
                              uuid_a, name_a,
                              uuid_b, name_b);
            if (!player->breaker) {
                printf("breaker : insert fail !!!\n");
            }
        }
    }
}

static void a2dp_player_callback(void *private_data, int event)
{
    struct a2dp_player *player = g_a2dp_player;

    printf("a2dp_callback: %d\n", event);
    switch (event) {
    case STREAM_EVENT_START:
#if 0 //v300默认流程不添加动态eq
#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
        if (CONFIG_SPATIAL_EFFECT_VERSION == SPATIAL_EFFECT_V3) {
            //播歌打开时，设置spatial_eff_v300流程中的dynamic_eq状态
            u8 is_bypass = get_a2dp_spatial_audio_mode() ? 0 : 1;
            spatial_effect_dy_eq_bypass(is_bypass);
        }
#endif
#endif
#if AUDIO_VBASS_LINK_VOLUME
        vbass_link_volume();
#endif
#if AUDIO_AUTODUCK_LINK_VOLUME
        autoduck_link_volume();
#endif
#if AUDIO_EQ_LINK_VOLUME
        eq_link_volume();
#endif
#if TCFG_TWS_DUAL_CHANNEL
        a2dp_player_update_steromix_param(player, player->channel);
#endif
        music_vocal_remover_update_parm();
        break;
    }
}

static void a2dp_player_set_audio_channel(struct a2dp_player *player)
{
#if (defined(TCFG_AUDIO_FLOW_PRESET_TWS_DUAL_DRIVER_ENABLE) && TCFG_AUDIO_FLOW_PRESET_TWS_DUAL_DRIVER_ENABLE)
    int channel = AUDIO_CH_MIX;
#else
    int channel = (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR) ? AUDIO_CH_LR : AUDIO_CH_MIX;
#endif
    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
        channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
    } else {
#if defined(TCFG_TWS_SIBLING_DISCONNECTED_CH_SEL) && \
            (TCFG_TWS_SIBLING_DISCONNECTED_CH_SEL == CH_SEL_AUTO)
        char local_ch = bt_tws_get_local_channel();
        printf("tws sibling disconnected local ch %c\n", local_ch);
        if ((local_ch == 'L') || (local_ch == 'U')) {
            channel = AUDIO_CH_L;
        } else if (local_ch == 'R') {
            channel = AUDIO_CH_R;
        }
#endif
    }

#if defined(TCFG_DECODER_OUTPUT_CH_SEL) && (TCFG_DECODER_OUTPUT_CH_SEL != CH_SEL_AUTO)
    player->channel = TCFG_DECODER_OUTPUT_CH_SEL;
    jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, TCFG_DECODER_OUTPUT_CH_SEL);
#else
    player->channel = channel;
    jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, channel);
#endif
}

void a2dp_player_tws_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    u8 state = evt->args[1];

    switch (evt->event) {
    case TWS_EVENT_MONITOR_START:
        if (!(state & TWS_STA_SBC_OPEN)) {
            break;
        }
    case TWS_EVENT_CONNECTION_DETACH:
    case TWS_EVENT_REMOVE_PAIRS:
        if (g_a2dp_player) {
            a2dp_player_set_audio_channel(g_a2dp_player);
        }
        break;
    default:
        break;
    }
    a2dp_tws_timestamp_event_handler(evt->event, evt->args);
}

static int a2dp_player_create(u8 *btaddr)
{
    int uuid;
    struct a2dp_player *player = g_a2dp_player;

    if (player) {
        if (player->stream) {
            if (!memcmp(player->bt_addr, btaddr, 6))  {
                return -EEXIST;
            }
            puts("a2dp_player_busy\n");
            return -EBUSY;
        }
        if (player->retry_timer) {
            sys_timer_del(player->retry_timer);
            player->retry_timer = 0;
        }
    } else {
        player = zalloc(sizeof(*player));
        if (!player) {
            return -ENOMEM;
        }
        g_a2dp_player = player;
    }

    uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"a2dp");

    memcpy(player->bt_addr, btaddr, 6);

#if defined(TCFG_SPATIAL_AUDIO_ENABLE) && TCFG_SPATIAL_AUDIO_ENABLE
#if SPATIAL_AUDIO_EFFECT_SW_TONE_PLAY
    if (get_a2dp_spatial_audio_mode()) {
        player->stream = jlstream_pipeline_parse_by_node_name(uuid, A2DP_SPATIAL_ON);
    } else {
        player->stream = jlstream_pipeline_parse_by_node_name(uuid, A2DP_SPATIAL_OFF);
    }
#else
    //固定打开空间音效的流
    player->stream = jlstream_pipeline_parse_by_node_name(uuid, A2DP_SPATIAL_ON);
#endif
#else
    player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_A2DP_RX);
#endif
    if (!player->stream) {
        printf("create a2dp stream faild\n");
        return -EFAULT;
    }

    return 0;
}

void a2dp_player_low_latency_enable(u8 enable)
{
    a2dp_file_low_latency_enable(enable);
}

static void retry_open_a2dp_player(void *p)
{
    if (g_a2dp_player && !g_a2dp_player->stream) {
        a2dp_player_open(g_a2dp_player->bt_addr);
    }
}

static void retry_start_a2dp_player(void *p)
{
    if (g_a2dp_player && g_a2dp_player->stream) {
        int err = jlstream_start(g_a2dp_player->stream);
        if (err == 0) {
            sys_timer_del(g_a2dp_player->retry_timer);
            g_a2dp_player->retry_timer = 0;
        }
    }
}

static void a2dp_player_set_channel_by_tws(struct a2dp_player *player)
{
    if (CONFIG_BTCTLER_TWS_ENABLE) {
#if (defined(TCFG_AUDIO_FLOW_PRESET_TWS_DUAL_DRIVER_ENABLE) && TCFG_AUDIO_FLOW_PRESET_TWS_DUAL_DRIVER_ENABLE)
        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
            player->channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
        } else {
            player->channel = AUDIO_CH_MIX;
        }
#else

        int channel = AUDIO_CH_MIX;
        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
            channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;

        } else {
#if defined(TCFG_TWS_SIBLING_DISCONNECTED_CH_SEL) && \
            (TCFG_TWS_SIBLING_DISCONNECTED_CH_SEL == CH_SEL_AUTO)

            char local_ch = bt_tws_get_local_channel();
            printf("tws sibling disconnected local ch %c\n", local_ch);
            if ((local_ch == 'L') || (local_ch == 'U')) {
                channel = AUDIO_CH_L;
            } else if (local_ch == 'R') {
                channel = AUDIO_CH_R;
            }
#endif
        }
        //如果dac配置的立体声，tws 连接上时解码也要配置输出立体声，由channel_adapter节点做tws 声道适配，避免断开tws 连接时，立体声输出无法声道分离
        player->channel = (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR) ? AUDIO_CH_LR : channel;
#endif
        printf("a2dp player channel setup:0x%x", player->channel);
        jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, player->channel);
    }
}


int a2dp_player_open(u8 *btaddr)
{
    int err;
    err = a2dp_player_create(btaddr);
    if (err) {
        if (err == -EFAULT) {
            g_a2dp_player->retry_timer = sys_timer_add(NULL, retry_open_a2dp_player, 200);
        }
        return err;
    }

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    icsd_adt_a2dp_scene_set(1);
#endif

    struct a2dp_player *player =  g_a2dp_player;

    player->a2dp_pitch_mode = PITCH_0; //默认打开是原声调

    jlstream_set_callback(player->stream, NULL, a2dp_player_callback);
    jlstream_set_scene(player->stream, STREAM_SCENE_A2DP);

#if (SPATIAL_AUDIO_EFFECT_SW_TONE_PLAY && (defined(TCFG_SPATIAL_AUDIO_ENABLE) && TCFG_SPATIAL_AUDIO_ENABLE))
    //重开数据流方式切换空间音效模式，需要根据空间音效是否开关分别设置解码声道
    if (get_a2dp_spatial_audio_mode()) {
        //空间音效开，输出双声道真立体声
        jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, AUDIO_CH_LR);
    } else {
        //空间音效关，根据tws适配
        a2dp_player_set_channel_by_tws(player);
    }
#else /*SPATIAL_AUDIO_EFFECT_SW_TONE_PLAY*/

#if defined(TCFG_DECODER_OUTPUT_CH_SEL) && (TCFG_DECODER_OUTPUT_CH_SEL != CH_SEL_AUTO)
    jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, TCFG_DECODER_OUTPUT_CH_SEL);
#else
    a2dp_player_set_channel_by_tws(player);
#endif

#endif /*SPATIAL_AUDIO_EFFECT_SW_TONE_PLAY*/

    err = jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE,
                              NODE_IOC_SET_BTADDR, (int)player->bt_addr);

#if TCFG_AI_TX_NODE_ENABLE
    struct stream_fmt ai_tx_fmt = {0};
    ai_tx_fmt.sample_rate = 16000;
    ai_tx_fmt.coding_type = AUDIO_CODING_OPUS;
    jlstream_node_ioctl(player->stream, NODE_UUID_AI_TX, NODE_IOC_SET_FMT, (int)&ai_tx_fmt);
#endif

#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
    a2dp_player_breaker_mode(get_a2dp_spatial_audio_mode(),
                             BREAKER_SOURCE_NODE_UUID, BREAKER_SOURCE_NODE_NEME,
                             BREAKER_TARGER_NODE_UUID, BREAKER_TARGER_NODE_NEME);
#endif

#if defined(TCFG_VIRTUAL_SURROUND_EFF_MODULE_NODE_ENABLE) && TCFG_VIRTUAL_SURROUND_EFF_MODULE_NODE_ENABLE
    //iphone sbc解码帧长短得情况下，使用三线程推数
    jlstream_add_thread(player->stream, "media0");
    jlstream_add_thread(player->stream, "media1");

    int codec_type = 0xff;
    void *file = a2dp_open_media_file(player->bt_addr);
    if (file) {
        codec_type = a2dp_media_get_codec_type(file);
    }
    if (codec_type == A2DP_CODEC_SBC) {
#if defined(CONFIG_CPU_BR28)
        jlstream_add_thread(player->stream, "media2");
#endif
    }

#endif



    if (err == 0) {
        err = jlstream_start(player->stream);
        if (err) {
            g_a2dp_player->retry_timer = sys_timer_add(NULL, retry_start_a2dp_player, 200);
            return 0;
        }
    }
    if (err) {
        jlstream_release(player->stream);
        free(player);
        g_a2dp_player = NULL;
        return err;
    }

#if (TCFG_SMART_VOICE_ENABLE && TCFG_SMART_VOICE_USE_AEC)
    audio_smart_voice_aec_open();
#endif
#if TCFG_AUDIO_AVC_NODE_ENABLE
    env_noise_recoder_open();
#endif

    puts("a2dp_open_dec_file_suss\n");

    return 0;
}

int a2dp_player_runing()
{
    return g_a2dp_player ? 1 : 0;
}

int a2dp_player_get_btaddr(u8 *btaddr)
{
    if (g_a2dp_player) {
        memcpy(btaddr, g_a2dp_player->bt_addr, 6);
        return 1;
    } else {
#if ((TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_BIS_TX_EN) || TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_BIS_TX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_JL_CIS_CENTRAL_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))
        u8 *addr =  le_audio_a2dp_recorder_get_btaddr();
        if (addr) {
            memcpy(btaddr, addr, 6);
            return 1;
        }
#endif
    }
    return 0;
}

bool a2dp_player_is_playing(u8 *bt_addr)
{
    if (g_a2dp_player && memcmp(bt_addr, g_a2dp_player->bt_addr, 6) == 0) {
        return 1;
    }
#if ((TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_BIS_TX_EN) || TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_BIS_TX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_JL_CIS_CENTRAL_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))
    u8 *addr =  le_audio_a2dp_recorder_get_btaddr();
    if (addr && memcmp(bt_addr, addr, 6) == 0) {
        return 1;
    }
#endif
    return 0;
}

int a2dp_player_start_slience_detect(u8 *btaddr, void (*handler)(u8 *, bool), int msec)
{
    if (!g_a2dp_player || memcmp(g_a2dp_player->bt_addr, btaddr, 6))  {
        return -EINVAL;
    }
    return 0;
}

void a2dp_player_set_ai_tx_node_func(int (*func)(u8 *, u32))
{
    struct a2dp_player *player = g_a2dp_player;

    if (player && player->stream) {
        jlstream_node_ioctl(player->stream, NODE_UUID_AI_TX, NODE_IOC_SET_PRIV_FMT, (int)func);
    }
}

void a2dp_player_close(u8 *btaddr)
{
    struct a2dp_player *player = g_a2dp_player;

    if (!player) {
        return;
    }
    if (memcmp(player->bt_addr, btaddr, 6)) {
        return;
    }
    if (player->retry_timer) {
        sys_timer_del(player->retry_timer);
        player->retry_timer = 0;
    }
#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
    if (player->breaker) {
        printf("breaker : delete\n");
        jlstream_delete_breaker(player->breaker, 0);
        player->breaker = NULL;
    }
#endif
    if (player->stream) {
        jlstream_stop(player->stream, 100);
        jlstream_release(player->stream);
    }
    free(player);
    g_a2dp_player = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"a2dp");

#if (TCFG_SMART_VOICE_ENABLE && TCFG_SMART_VOICE_USE_AEC)
    audio_smart_voice_aec_close();
#endif
#if TCFG_AUDIO_AVC_NODE_ENABLE
    env_noise_recoder_close();
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    icsd_adt_a2dp_scene_set(0);
#endif
}

//复位当前的数据流
void a2dp_player_reset(void)
{
    u8 bt_addr[6];
    if (g_a2dp_player) {
        memcpy(bt_addr, g_a2dp_player->bt_addr, 6);
        a2dp_player_close(bt_addr);
        a2dp_player_open(bt_addr);
    }
}

#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
extern void set_a2dp_spatial_audio_mode(enum SPATIAL_EFX_MODE mode);
//此函数用于开空间音频时将全局采样率降低到48k
//关空间音频且开着ldac的时候将采样率还原到96k
void a2dp_player_reset_spatial(void)
{
    u8 bt_addr[6];
    if (g_a2dp_player) {
        memcpy(bt_addr, g_a2dp_player->bt_addr, 6);
        a2dp_player_close(bt_addr);
        u8 mode = get_a2dp_spatial_audio_mode();
        if (mode == SPATIAL_EFX_OFF) { //开空间音频,设置采样率为48k
            mode = SPATIAL_EFX_TRACKED;
            extern int audio_general_set_global_sample_rate(int sample_rate);
            audio_general_set_global_sample_rate(48000);
            set_a2dp_spatial_audio_mode(mode);
        } else { //关空间音频,设置采样率为96k
            mode = SPATIAL_EFX_OFF;
            extern int audio_general_set_global_sample_rate(int sample_rate);
            audio_general_set_global_sample_rate(96000);
            set_a2dp_spatial_audio_mode(mode);
        }
        a2dp_player_open(bt_addr);
    }
}

#if SPATIAL_AUDIO_EFFECT_SW_TONE_PLAY
void a2dp_player_reset_spatial_tone_play(u8 mode)
{
    if (g_a2dp_player) {
        u8 bt_addr[6];
        memcpy(bt_addr, g_a2dp_player->bt_addr, 6);
        a2dp_player_close(bt_addr);
        set_a2dp_spatial_audio_mode(mode);
        a2dp_player_open(bt_addr);
    }
}
#endif
#endif

//变调接口
int a2dp_file_pitch_up()
{
    struct a2dp_player *player = g_a2dp_player;
    if (!player) {
        return -1;
    }
    float pitch_param_table[] = {-12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12};
    player->a2dp_pitch_mode++;
    if (player->a2dp_pitch_mode > ARRAY_SIZE(pitch_param_table) - 1) {
        player->a2dp_pitch_mode = ARRAY_SIZE(pitch_param_table) - 1;
    }
    printf("play pitch up+++%d\n", player->a2dp_pitch_mode);
    int ret = a2dp_file_set_pitch(player->a2dp_pitch_mode);
    ret = (ret == true) ? player->a2dp_pitch_mode : -1;
    return ret;
}

int a2dp_file_pitch_down()
{
    struct a2dp_player *player = g_a2dp_player;
    if (!player) {
        return -1;
    }
    player->a2dp_pitch_mode--;
    if (player->a2dp_pitch_mode < 0) {
        player->a2dp_pitch_mode = 0;
    }
    printf("play pitch down---%d\n", player->a2dp_pitch_mode);
    int ret = a2dp_file_set_pitch(player->a2dp_pitch_mode);
    ret = (ret == true) ? player->a2dp_pitch_mode : -1;
    return ret;
}

int a2dp_file_set_pitch(enum _pitch_level pitch_mode)
{
    struct a2dp_player *player = g_a2dp_player;
    float pitch_param_table[] = {-12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12};
    if (pitch_mode > ARRAY_SIZE(pitch_param_table) - 1) {
        pitch_mode = ARRAY_SIZE(pitch_param_table) - 1;
    }
    pitch_speed_param_tool_set pitch_param = {
        .pitch = pitch_param_table[pitch_mode],
        .speed = 1,
    };
    if (player) {
        player->a2dp_pitch_mode = pitch_mode;
        return jlstream_node_ioctl(player->stream, NODE_UUID_PITCH_SPEED, NODE_IOC_SET_PARAM, (int)&pitch_param);
    }
    return -1;
}

void a2dp_file_pitch_mode_init(enum _pitch_level pitch_mode)
{
    struct a2dp_player *player = g_a2dp_player;
    if (player) {
        player->a2dp_pitch_mode = pitch_mode;
    }
}
