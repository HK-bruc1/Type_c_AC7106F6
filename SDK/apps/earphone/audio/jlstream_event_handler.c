#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".jlstream_event_handler.data.bss")
#pragma data_seg(".jlstream_event_handler.data")
#pragma const_seg(".jlstream_event_handler.text.const")
#pragma code_seg(".jlstream_event_handler.text")
#endif
#include "jlstream.h"
#include "overlay_code.h"
#include "media/audio_base.h"
#include "clock_manager/clock_manager.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "app_tone.h"
#include "app_config.h"
#include "earphone.h"
#include "effects/effects_adj.h"
#include "audio_manager.h"
#include "classic/tws_api.h"
#include "esco_recoder.h"
#include "clock.h"
#include "dual_a2dp_play.h"
#include "mix_record_api.h"
#include "app_main.h"
#include "local_tws.h"
#include "bt_tws.h"
#include "usb_call_audio_tap.h"

#if TCFG_AUDIO_DUT_ENABLE
#include "test_tools/audio_dut_control.h"
#endif

#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif

#if TCFG_AI_TRANSLATOR_ENABLE
#include "ai_translator.h"
#endif

#if !TCFG_JLSTREAM_EFFICIENT_MODE
extern const int CONFIG_EXTWS_NACK_LIMIT_INT_CNT;
#endif

#define PIPELINE_UUID_TONE_NORMAL   0x7674
#define PIPELINE_UUID_A2DP          0xD96F
#define PIPELINE_UUID_ESCO          0xBA11
#define PIPELINE_UUID_AI_VOICE      0x5475
#define PIPELINE_UUID_A2DP_DUT      0xC9DB
#define PIPELINE_UUID_DEV_FLOW      0x959E
#define PIPELINE_UUID_PC_AUDIO		0xDC8D
#define PIPELINE_UUID_MIC_EFFECT    0x9C2D
#define PIPELINE_UUID_PC_AUDIO		0xDC8D
#define PIPELINE_UUID_LE_AUDIO      0x99AA
#define PIPELINE_UUID_RECODER       0x49EC
#define PIPELINE_UUID_USER_DEFINED  0x9795


static const struct stream_coexist_policy coexist_policy_table_rewrite[] = {
    [0] = {
        .scene_a = STREAM_SCENE_TONE, .coding_a = AUDIO_CODING_AAC,
        .scene_b = STREAM_SCENE_A2DP, .coding_b = AUDIO_CODING_AAC,
    },
    [1] = {
        .scene_a = STREAM_SCENE_RING, .coding_a = 0xffffffff,
        .scene_b = STREAM_SCENE_A2DP, .coding_b = 0xffffffff,
    },
    [2] = {
        .scene_a = STREAM_SCENE_ESCO, .coding_a = AUDIO_CODING_MSBC | AUDIO_CODING_CVSD,
        .scene_b = STREAM_SCENE_A2DP, .coding_b = AUDIO_CODING_AAC | AUDIO_CODING_SBC,
    },
#if TCFG_AUDIO_HEARING_AID_ENABLE && !TCFG_AUDIO_DHA_AND_MUSIC_COEXIST
    {
        .scene_a = STREAM_SCENE_A2DP,        .coding_a = 0xffffffff,
        .scene_b = STREAM_SCENE_HEARING_AID, .coding_b = 0xffffffff,
    },
#endif
#if TCFG_AUDIO_HEARING_AID_ENABLE && !TCFG_AUDIO_DHA_AND_TONE_COEXIST
    {
        .scene_a = STREAM_SCENE_TONE,        .coding_a = 0xffffffff,
        .scene_b = STREAM_SCENE_HEARING_AID, .coding_b = 0xffffffff,
    },
#endif
#if TCFG_AUDIO_HEARING_AID_ENABLE && !TCFG_AUDIO_DHA_AND_CALL_COEXIST
    {
        .scene_a = STREAM_SCENE_ESCO,        .coding_a = 0xffffffff,
        .scene_b = STREAM_SCENE_HEARING_AID, .coding_b = 0xffffffff,
    },
    {
        .scene_a = STREAM_SCENE_RING,        .coding_a = 0xffffffff,
        .scene_b = STREAM_SCENE_HEARING_AID, .coding_b = 0xffffffff,
    },
#endif
    { 0, 0, 0, 0 }
};

int get_system_stream_bit_width(void *par)
{
    return jlstream_read_bit_width(PIPELINE_UUID_TONE_NORMAL, par);
}

int get_media_stream_bit_width(void *par)
{
    return jlstream_read_bit_width(PIPELINE_UUID_A2DP, par);
}

int get_esco_stream_bit_width(void *par)
{
    return jlstream_read_bit_width(PIPELINE_UUID_ESCO, par);
}

int get_mic_eff_stream_bit_width(void *par)
{
    return jlstream_read_bit_width(PIPELINE_UUID_MIC_EFFECT, par);
}


int get_usb_audio_stream_bit_width(void *par)
{
    return jlstream_read_bit_width(PIPELINE_UUID_PC_AUDIO, par);
}

static int get_pipeline_uuid(const char *name)
{
    if (!strcmp(name, "tone")) {
        return PIPELINE_UUID_TONE_NORMAL;
    }

    if (!strcmp(name, "ring")) {
        clock_alloc("esco", 48 * 1000000UL);
        return PIPELINE_UUID_TONE_NORMAL;
    }

    if (!strcmp(name, "esco")) {
        clock_alloc("esco", 48 * 1000000UL);
        audio_event_to_user(AUDIO_EVENT_ESCO_START);
        return PIPELINE_UUID_ESCO;
    }

    if (!strcmp(name, "ai_rx_call")) {
        clock_alloc("ai_rx", 48 * 1000000UL);
        return PIPELINE_UUID_ESCO;
    }
    if (!strcmp(name, "ai_rx_media")) {
        clock_alloc("ai_rx", 48 * 1000000UL);
        return PIPELINE_UUID_A2DP;
    }

    if (!strcmp(name, "a2dp")) {
        clock_alloc("a2dp", 48 * 1000000UL);
        audio_event_to_user(AUDIO_EVENT_A2DP_START);
#if TCFG_AUDIO_DUT_ENABLE
        if (audio_dec_dut_en_get(0)) {
            return PIPELINE_UUID_A2DP_DUT;
        }
#endif
        return PIPELINE_UUID_A2DP;
    }

#if TCFG_APP_LINEIN_EN
    if (!strcmp(name, "linein")) {
        //此处设置时钟不低于120M是由于切时钟会停止cpu,多次切会导致DAC缓存少于1ms
        clock_alloc("linein", 120 * 1000000UL);
        return PIPELINE_UUID_A2DP;
    }
#endif

#if TCFG_APP_PC_EN
    if (!strcmp(name, "pc_spk")) {
        clock_alloc("pc_spk", 96 * 1000000UL);
#if TCFG_LOCAL_TWS_ENABLE
        return PIPELINE_UUID_A2DP;
#else
        /* return PIPELINE_UUID_PC_AUDIO; */
        return PIPELINE_UUID_A2DP;//PCspk放在媒体,使用24bit
#endif
    }
    if (!strcmp(name, "pc_mic")) {
        clock_alloc("pc_mic", 96 * 1000000UL);
        return PIPELINE_UUID_PC_AUDIO;
    }
#endif

#if TCFG_APP_MUSIC_EN
    if (!strcmp(name, "music")) {
        clock_alloc("music", 64 * 1000000UL);
        return PIPELINE_UUID_A2DP;
    }
#endif

    if (!strcmp(name, "user_defined")) {
        return PIPELINE_UUID_USER_DEFINED;
    }

    if (!strcmp(name, "ai_voice")) {
        /* clock_alloc("a2dp", 24 * 1000000UL); */
        return PIPELINE_UUID_AI_VOICE;
    }

#if TCFG_MIX_RECORD_ENABLE
    if (!strcmp(name, "mix_recorder")) {
        return PIPELINE_UUID_RECODER;
    }
#endif

    if (!strcmp(name, "dev_flow")) {
        return PIPELINE_UUID_DEV_FLOW;
    }
#if (TCFG_MIC_EFFECT_ENABLE || TCFG_AUDIO_HEARING_AID_ENABLE)
    if (!strcmp(name, "mic_effect")) {
        clock_alloc("mic_effect", 24 * 1000000UL);
        return PIPELINE_UUID_MIC_EFFECT;
    }
#endif
#if LE_AUDIO_STREAM_ENABLE
    if (!strcmp(name, "le_audio")) {
//#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
//        clock_alloc("le_audio", clk_get_max_frequency());
//#endif
        return PIPELINE_UUID_LE_AUDIO;
    }
    if (!strcmp(name, "le_audio_call") || \
        !strcmp(name, "mic_le_audio_call")) {
//#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
//        clock_alloc("le_audio", clk_get_max_frequency());
//#endif
        return PIPELINE_UUID_ESCO;
    }
#endif
    if (!strcmp(name, "adda_loop")) {
        clock_alloc("adda_loop", 24 * 1000000UL);
        return PIPELINE_UUID_A2DP_DUT;
    }

#if TCFG_LOCAL_TWS_ENABLE
    if (!strcmp(name, "local_tws")) {
        clock_alloc("local_tws", 48 * 1000000UL);
        return PIPELINE_UUID_A2DP;
    }
#endif

    return 0;
}

static void player_close_handler(const char *name)
{
    if (CONFIG_HEAP_MEMORY_TRACE) {
        memory_trace_reset();
    }
    clock_free(name);
    if (!strcmp(name, "a2dp")) {
        audio_event_to_user(AUDIO_EVENT_A2DP_STOP);
        return;
    }
    if (!strcmp(name, "esco")) {
        audio_event_to_user(AUDIO_EVENT_ESCO_STOP);
        return;
    }
}

#if defined(TCFG_HI_RES_AUDIO_ENEBALE) || TCFG_VIRTUAL_SURROUND_PRO_MODULE_NODE_ENABLE
//调整解码器输出帧长
static const int frame_unit_size[] = { 64, 128, 256, 384, 512, 1024, 2048, 4096, 8192};
int decoder_check_frame_unit_size(int dest_len)
{
    for (int i = 0; i < ARRAY_SIZE(frame_unit_size); i++) {
        if (dest_len <= frame_unit_size[i]) {
            dest_len = frame_unit_size[i];
            return dest_len;
        }
    }
    dest_len = 8192;
    return dest_len ;
}

#endif


static int load_decoder_handler(struct stream_decoder_info *info)
{
#if TCFG_BT_SUPPORT_AAC
    if (info->coding_type == AUDIO_CODING_AAC) {
        printf("overlay_lode_code: aac\n");
        audio_overlay_load_code(AUDIO_CODING_AAC);
    }
#endif
    if (info->scene == STREAM_SCENE_A2DP) {
#if TCFG_JLSTREAM_EFFICIENT_MODE
        switch (info->coding_type) {
        case AUDIO_CODING_OPUS:
            //音视频翻译创建AI_RX->DAC音频流不要创建a2dp_dec线程，A2DP_RX->DAC
            //创建了一次，再创建会重复，报断言
            break;
        default:
            info->task_name = "a2dp_dec";
            break;
        }
#endif

#if TCFG_VIRTUAL_SURROUND_PRO_MODULE_NODE_ENABLE
        info->frame_time = 16;
#endif
    }
    if (info->scene == STREAM_SCENE_LEA_CALL || info->scene == STREAM_SCENE_LE_AUDIO) {
        //printf("decoder scene:LEA CALL\n");
        info->frame_time = 10;
        /* info->task_name = "a2dp_dec"; */
    }
    if (info->scene == STREAM_SCENE_MUSIC) {
        info->task_name = "file_dec";
    }
    if (info->scene == STREAM_SCENE_AI_VOICE) {
        info->frame_time = 10;
    }
    return 0;
}

static int load_encoder_handler(struct stream_encoder_info *info)
{
    if (info->scene == STREAM_SCENE_ESCO) {
        //AEC overlay归节点自己管理, 不依赖编码
    }
    return 0;
}


/*
 *获取需要指定得默认配置
 * */
static int get_node_parm(int arg)
{
    int ret = 0;
    ret = get_eff_default_param(arg);
    return ret ;
}
/*
*获取ram内在线音效参数
*/
static int get_eff_online_parm(int arg)
{
    int ret = 0;
#if TCFG_CFG_TOOL_ENABLE
    struct eff_parm {
        int uuid;
        char name[16];
        u8 data[0];
    };
    struct eff_parm *parm = (struct eff_parm *)arg;
    /* printf("eff_online_uuid %x, %s\n", parm->uuid, parm->name); */
    ret = get_eff_online_param(parm->uuid, parm->name, (void *)arg);
#endif
    return ret;
}


static int tws_switch_get_status()
{
    int state = tws_api_get_tws_state();
#if TCFG_LOCAL_TWS_ENABLE
    struct app_mode *mode;
    mode = app_get_current_mode();
    if (mode == NULL) { //有可能此时处于模式切换临界情况
        return 0;
    }
    if (mode->name == APP_MODE_BT || mode->name == APP_MODE_SINK || local_tws_mode_exit()) {
        return 0;
    } else {
        if (state & TWS_STA_SIBLING_CONNECTED && tws_api_data_trans_connect() && (local_tws_get_role() == LOCAL_TWS_ROLE_SOURCE) && bt_tws_get_state()) {
            return 1;
        }
        return 0;
    }
#else
    if (state & TWS_STA_SIBLING_DISCONNECTED) {
        return 0;
    }
    return 1;
#endif
}

#if TCFG_AI_TRANSLATOR_ENABLE
static int esco_trans_switch_get_status()
{
    int trans = 0; //获取翻译/录音状态
    struct ai_trans_mode minfo;
    ai_translator_get_mode_info(&minfo);
    if (minfo.mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION ||
        minfo.mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC ||
        minfo.mode == AI_TRANSLATOR_MODE_CALL_RECORD_STEREO_ENC) {
        trans = 1;
    }
    //putchar('e');
    //putchar('0' + trans);
    return trans;
}

static int esco_switch_get_status()
{
    int trans = 0; //获取翻译/录音状态
    struct ai_trans_mode minfo;
    ai_translator_get_mode_info(&minfo);
    if (minfo.mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION ||
        minfo.mode == AI_TRANSLATOR_MODE_CALL_TRANSLATION_STEREO_ENC) {
        trans = 1;
    }
    //putchar('f');
    //putchar('0' + trans);
    return !trans;
}

static int media_trans_switch_get_status()
{
    int trans = 0;//获取翻译状态
    struct ai_trans_mode minfo;
    ai_translator_get_mode_info(&minfo);
    if (minfo.mode == AI_TRANSLATOR_MODE_A2DP_TRANSLATION) {
        trans = 1;
    }
    return trans;
}

static int media_switch_get_status()
{
    return !media_trans_switch_get_status();
}
#endif

static int usb_call_near_tap_get_status(void)
{
#if RDX_CFG_CALL_RECORDING_ENABLE
    /* Keep the Tap branch negotiated; the Tap gate controls PCM consumption. */
    return 1;
#else
    return usb_call_audio_tap_gate_is_open(USB_CALL_AUDIO_TAP_NEAR);
#endif
}

static int usb_call_far_tap_get_status(void)
{
#if RDX_CFG_CALL_RECORDING_ENABLE
    return 1;
#else
    return usb_call_audio_tap_gate_is_open(USB_CALL_AUDIO_TAP_FAR);
#endif
}

static int get_switch_node_callback(const char *arg)
{
    if (!strcmp(arg, USB_CALL_AUDIO_NEAR_SWITCH_NAME)) {
        return (int)usb_call_near_tap_get_status;
    }
    if (!strcmp(arg, USB_CALL_AUDIO_FAR_SWITCH_NAME)) {
        return (int)usb_call_far_tap_get_status;
    }

    if (!strcmp(arg, "TWS_Switch")) {
        return (int)tws_switch_get_status;
    }

#if TCFG_AUDIO_SIDETONE_ENABLE
    if (!strcmp(arg, SIDETONE_SWITCH_NAME)) {
        return (int)get_audio_sidetone_state;
    }
#endif

#if TCFG_LOCAL_TWS_ENABLE
    if (!strncmp(arg, "Switch1", strlen("Switch1"))) {
        return (int)tws_switch_get_status;
    }
#endif

#if TCFG_AI_TRANSLATOR_ENABLE
    if (!strcmp(arg, "ESCO_Switch")) {
        return (int)esco_switch_get_status;
    }
    if (!strcmp(arg, "ESCO_Trans")) {
        return (int)esco_trans_switch_get_status;
    }
    if (!strcmp(arg, "ESCO_MIC_Switch")) {
        return (int)esco_switch_get_status;
    }
    if (!strcmp(arg, "ESCO_MIC_Trans")) {
        return (int)esco_trans_switch_get_status;
    }
    if (!strcmp(arg, "Media_Switch")) {
        return (int)media_switch_get_status;
    }
    if (!strcmp(arg, "Media_Trans")) {
        return (int)media_trans_switch_get_status;
    }
#endif

#if TCFG_MIX_RECORD_ENABLE
    if (!strncmp(arg, "SW_Rec", strlen("SW_Rec"))) {
        return (int)get_mix_recorder_status;
    }
#endif // TCFG_MIX_RECORD_ENABLE

    return 0;
}

static int tws_get_output_channel()
{
    int channel = AUDIO_CH_MIX;
    if (tws_api_is_connect()) {
        channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
    }
    return channel;
}

static void get_noisegate_gain(u32 frame_time_ms, float gain)
{
    /* #define debug_dig(x)  __builtin_abs((int)((x - (int)x) * 1000)) */
    /* printf("frame_time %d ms, gain %d.%03d\n", frame_time_ms, (int)gain, debug_dig(gain)); */
}

static int get_noisegate_node_callback(const char *arg)
{
    /* if (!strcmp(arg, "my_nsgate_name")) { */
    /* 可根据node_name区分不同的noisegate状态 */
    /* } */
    return (int)get_noisegate_gain;
}

static int get_merge_node_callback(const char *arg)
{
    return (int)tws_get_output_channel;
}

static int get_spatial_adv_node_callback(const char *arg)
{
    return (int)tws_get_output_channel;
}

static int get_output_node_delay(int arg)
{
#if !TCFG_JLSTREAM_EFFICIENT_MODE
    if (arg == STREAM_SCENE_A2DP && CONFIG_EXTWS_NACK_LIMIT_INT_CNT < 63) {
#ifdef TCFG_HI_RES_AUDIO_ENEBALE
        return 50/*ms*/;
#else
        /*A2DP模式下，DAC或输出设备在监听+转发的机制下最大延时约束为30ms，其他延时补偿到蓝牙缓冲预留转发时间*/
        return 30/*ms*/;
#endif
    }
#endif
    return 0;
}

static int get_output_node_fade_out_state(int arg)
{
    if ((arg == STREAM_SCENE_TONE) || (arg == STREAM_SCENE_RING) || (arg == STREAM_SCENE_KEY_TONE)) {
        return 0;//提示音不做处理
    }
    //其余场景默认处理,如不需要,则可以判断对应场景并返回0即可
    return 1;
}

static int a2dp_get_disconnect_state(u8 *bt_addr)
{
    return get_charge_online_flag();
}

int jlstream_event_notify(enum stream_event event, int arg)
{
    int ret = 0;

    switch (event) {
    case STREAM_EVENT_LOAD_DECODER:
        ret = load_decoder_handler((struct stream_decoder_info *)arg);
        break;
    case STREAM_EVENT_LOAD_ENCODER:
        ret = load_encoder_handler((struct stream_encoder_info *)arg);
        break;
    case STREAM_EVENT_GET_PIPELINE_UUID:
        ret = get_pipeline_uuid((const char *)arg);
        r_printf("pipeline_uuid: %x\n", ret);
        clock_refurbish();
        break;
    case STREAM_EVENT_CLOSE_PLAYER:
        player_close_handler((const char *)arg);
        break;
    case STREAM_EVENT_INC_SYS_CLOCK:
        clock_refurbish();
        break;
    case STREAM_EVENT_GET_NODE_PARM:
        ret = get_node_parm(arg);
        break;
    case STREAM_EVENT_GET_EFF_ONLINE_PARM:
        ret = get_eff_online_parm(arg);
        break;
#if TCFG_BT_DUAL_CONN_ENABLE
    case STREAM_EVENT_A2DP_ENERGY:
        a2dp_energy_detect_handler((int *)arg);
        break;
#endif
#if TCFG_SWITCH_NODE_ENABLE
    case STREAM_EVENT_GET_SWITCH_CALLBACK:
        ret = get_switch_node_callback((const char *)arg);
        break;
#endif

#if TCFG_AUDIO_HEARING_AID_ENABLE
    case STREAM_EVENT_GET_COEXIST_POLICY:
        ret = (int)coexist_policy_table_rewrite;
        break;
#endif

#if TCFG_CHANNEL_MERGE_NODE_ENABLE
    case STREAM_EVENT_GET_MERGER_CALLBACK:
        ret = get_merge_node_callback((const char *)arg);
        break;
#endif
#if TCFG_SPATIAL_ADV_NODE_ENABLE
    case STREAM_EVENT_GET_SPATIAL_ADV_CALLBACK:
        ret = get_spatial_adv_node_callback((const char *)arg);
        break;
#endif
    case STREAM_EVENT_GLOBAL_PAUSE:
#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN && TCFG_AUDIO_ANC_ENABLE
        audio_anc_ear_adaptive_a2dp_suspend_cb();
#endif
        break;

#if TCFG_NOISEGATE_NODE_ENABLE
    case STREAM_EVENT_GET_NOISEGATE_CALLBACK:
        ret = get_noisegate_node_callback((const char *)arg);
        break;
#endif
#if !TCFG_JLSTREAM_EFFICIENT_MODE
    case STREAM_EVENT_GET_OUTPUT_NODE_DELAY:
        ret = get_output_node_delay(arg);
        break;
#endif

//对有缓存效果的节点，使能输出节点对末尾一帧做淡出处理
#if (defined(TCFG_VIRTUAL_BASS_PRO_MODULE_NODE_ENABLE) && TCFG_VIRTUAL_BASS_PRO_MODULE_NODE_ENABLE) || \
		(defined(TCFG_VIRTUAL_BASS_CLASSIC_NODE_ENABLE) && TCFG_VIRTUAL_BASS_CLASSIC_NODE_ENABLE) || \
		(defined(TCFG_VBASS_NODE_ENABLE) && TCFG_VBASS_NODE_ENABLE) || \
		(defined(TCFG_BASS_BOOST_COMBO_NODE_ENABLE) && TCFG_BASS_BOOST_COMBO_NODE_ENABLE ) || \
		(defined(TCFG_PCM_DELAY_NODE_ENABLE) && TCFG_PCM_DELAY_NODE_ENABLE)

    case STREAM_EVENT_GET_FADE_OUT_STATE:
        if (config_audio_dac_node_fade_out_en) {
            ret = get_output_node_fade_out_state(arg);
        }
        break;
#endif

    case STREAM_EVENT_GET_A2DP_STATE:
        ret = a2dp_get_disconnect_state((u8 *)arg);
        break;
    default:
        break;
    }

    return ret;
}


void aec_code_movable_load(void)
{
#if 0//TCFG_CODE_RUN_RAM_AEC_CODE
    int aec_code_size = __aec_movable_region_end - __aec_movable_region_start;
    mem_stats();
    if (aec_code_size && !aec_code_run_addr) {
        aec_code_run_addr = phy_malloc(aec_code_size);
    }

    if (!aec_code_run_addr) {
        return;
    }
    code_movable_load(__aec_movable_region_start, aec_code_size, aec_code_run_addr, __aec_movable_slot_start, __aec_movable_slot_end);
    printf("aec code load addr : 0x%x, size : %d\n", (u32)aec_code_run_addr, aec_code_size);
    mem_stats();
#endif
}

void aec_code_movable_unload(void)
{
#if 0//TCFG_CODE_RUN_RAM_AEC_CODE
    if (aec_code_run_addr) {
        mem_stats();
        code_movable_unload(__aec_movable_region_start, __aec_movable_slot_start, __aec_movable_slot_end);
        phy_free(aec_code_run_addr);
        aec_code_run_addr = NULL;
        printf("aec code unload\n");
        mem_stats();
    }
#endif
}
