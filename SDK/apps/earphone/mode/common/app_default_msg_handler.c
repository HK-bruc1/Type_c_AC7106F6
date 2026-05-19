#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".app_default_msg_handler.data.bss")
#pragma data_seg(".app_default_msg_handler.data")
#pragma const_seg(".app_default_msg_handler.text.const")
#pragma code_seg(".app_default_msg_handler.text")
#endif
#include "app_main.h"
#include "init.h"
#include "app_config.h"
#include "app_default_msg_handler.h"
#include "dev_status.h"
#include "audio_config.h"
#include "app_tone.h"
#include "scene_switch.h"
#include "earphone.h"
#include "node_param_update.h"
#include "bass_treble.h"
#include "key_driver.h"
#include "app_msg.h"
#include "btstack/avctp_user.h"
#include "classic/tws_api.h"
#include "bt_key_func.h"
#include "low_latency.h"
#include "rcsp_cfg.h"
#include "app_music.h"
#include "usb/device/usb_stack.h"
#include "effects/audio_spk_eq.h"
#include "mix_record_api.h"
#include "system/event.h"

#include "audio_anc_includes.h"

#if TCFG_MIC_EFFECT_ENABLE
#include "mic_effect.h"
#endif

#if RCSP_MODE && RCSP_ADV_KEY_SET_ENABLE
#include "adv_anc_voice_key.h"
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN))
#include "btstack_rcsp_user.h"
#include "bt_key_func.h"
#endif

#if TCFG_AUDIO_SPATIAL_EFFECT_ENABLE
#include "spatial_effects_process.h"
#endif

#if TCFG_APP_KEY_DUT_ENABLE
#include "app_key_dut.h"
#endif

#if TCFG_VOICE_CHANGER_NODE_ENABLE
#include "audio_voice_changer_api.h"
static u8 esco_eff_uncle;
static u8 esco_eff_goddess;
#endif


static u32 input_number = 0;
static u16 input_number_timer = 0;
static void input_number_timeout(void *p)
{
#if TCFG_APP_MUSIC_EN
    input_number_timer = 0;
    printf("input_number = %d\n", input_number);
    if (app_in_mode(APP_MODE_MUSIC)) {
        app_send_message(APP_MSG_MUSIC_PLAY_BY_NUM, (int)input_number);
    }
    input_number = 0;
#endif
}

static void input_number_deal(u32 num)
{
#if TCFG_APP_MUSIC_EN
    input_number = input_number * 10 + num;
    if (input_number > 9999) {
        input_number = num;
    }
    printf("num = %d, input_number = %d, input_number_timer = %d\n", num, input_number, input_number_timer);
    if (input_number_timer == 0) {
        input_number_timer = sys_timeout_add(NULL, input_number_timeout, 1000);
    } else {
        sys_timer_modify(input_number_timer, 1000);
    }
    app_send_message(APP_MSG_INPUT_FILE_NUM, input_number);
#endif
}
static u8 sys_audio_mute_statu = 0;//记录 audio dac mute

u8 get_sys_audio_mute_statu(void)
{
#if CONFIG_CPU_BR28 || CONFIG_CPU_BR36
    return app_audio_get_dac_digital_mute();
#else
    return 0;
#endif
}

void app_common_key_msg_handler(int *msg)
{
    int from_tws = msg[1];

    switch (msg[0]) {
    case APP_MSG_VOL_UP:
#if (THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN))
        if (bt_rcsp_device_conn_num() && JL_rcsp_get_auth_flag() && (app_get_current_mode()->name != APP_MODE_BT)) {
            bt_key_rcsp_vol_up();
        } else {
            app_audio_volume_up(1);
        }
#else
        app_audio_volume_up(1);
#endif

        if (app_audio_get_volume(APP_AUDIO_CURRENT_STATE) == app_audio_get_max_volume()) {
            if (tone_player_runing() == 0) {
#if TCFG_MAX_VOL_PROMPT
                play_tone_file(get_tone_files()->max_vol);
#endif
            }
        }
        app_send_message(APP_MSG_VOL_CHANGED, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
        break;
    case APP_MSG_VOL_DOWN:
#if (THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN))
        if (bt_rcsp_device_conn_num() && JL_rcsp_get_auth_flag() && (app_get_current_mode()->name != APP_MODE_BT)) {
            bt_key_rcsp_vol_down();
        } else {
            app_audio_volume_down(1);
        }
#else
        app_audio_volume_down(1);
#endif
        app_send_message(APP_MSG_VOL_CHANGED, app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
        break;
#if TCFG_SPEAKER_EQ_NODE_ENABLE
    case APP_MSG_SPK_EQ_TEST:
        spk_eq_test();
        break;
#endif

    case APP_MSG_SWITCH_SOUND_EFFECT:
        effect_scene_switch();
        break;
#if TCFG_MIC_EFFECT_ENABLE
    case APP_MSG_MIC_EFFECT_ON_OFF://混响开关

        if (from_tws == APP_KEY_MSG_FROM_TWS) {
            break;
        }

#if TCFG_APP_BT_EN
        if (bt_get_call_status() != BT_CALL_HANGUP) {//通话中
            break;
        }
#endif
        if (mic_effect_player_runing()) {
            mic_effect_player_close();
        } else {
            mic_effect_player_open();
        }
        break;
    case APP_MSG_SWITCH_MIC_EFFECT://混响音效场景切换
        /* mic_effect_scene_switch(); */
        break;
    case APP_MSG_MIC_VOL_UP:
        mic_effect_dvol_up();
        break;
    case APP_MSG_MIC_VOL_DOWN:
        mic_effect_dvol_down();
        break;
#if TCFG_BASS_TREBLE_NODE_ENABLE
    case APP_MSG_MIC_BASS_UP:
        puts("\n APP_MSG_MIC_BASS_UP \n");
        mic_bass_treble_eq_udpate("BassTreEff", BASS_TREBLE_LOW, 20);
        break;
    case APP_MSG_MIC_BASS_DOWN:
        puts("\n APP_MSG_MIC_BASS_DOWN \n");
        mic_bass_treble_eq_udpate("BassTreEff", BASS_TREBLE_LOW, -20);
        break;
    case APP_MSG_MIC_TREBLE_UP:
        puts("\n APP_MSG_MIC_TREBLE_UP \n");
        mic_bass_treble_eq_udpate("BassTreEff", BASS_TREBLE_HIGH, 20);
        break;
    case APP_MSG_MIC_TREBLE_DOWN:
        puts("\n APP_MSG_MIC_TREBLE_DOWN \n");
        mic_bass_treble_eq_udpate("BassTreEff", BASS_TREBLE_HIGH, -20);
        break;
#endif // TCFG_BASS_TREBLE_NODE_ENABLE
#endif
    case APP_MSG_VOCAL_REMOVE:
#if TCFG_APP_BT_EN
        if (bt_get_call_status() != BT_CALL_HANGUP) {//通话中
            break;
        }
#endif
        if (from_tws == APP_KEY_MSG_FROM_TWS) {
            break;
        }

        music_vocal_remover_switch();
        break;
    case APP_MSG_MUSIC_CHANGE_EQ:
        //  for test eq切换接口,工具上需要配置多份EQ配置
        u8 music_eq_preset_index = get_music_eq_preset_index();
        music_eq_preset_index ^= 1;
        set_music_eq_preset_index(music_eq_preset_index);
        eq_update_parm(get_current_scene(), "MusicEqBt", music_eq_preset_index);
        app_send_message(APP_MSG_EQ_CHANGED, get_music_eq_preset_index() + 1); //显示EQ从1开始
        printf("APP_MSG_MUSIC_CHANGE_EQ:%d\n", music_eq_preset_index);
        break;

    case APP_MSG_SYS_MUTE:
        sys_audio_mute_statu = app_audio_get_dac_digital_mute() ^ 1;
        if (sys_audio_mute_statu) {
            app_audio_mute(AUDIO_MUTE_DEFAULT);
        } else {
            app_audio_mute(AUDIO_UNMUTE_DEFAULT);
        }
        app_send_message(APP_MSG_MUTE_CHANGED, sys_audio_mute_statu);
        break;
    case APP_MSG_REC_PP:
#if TCFG_MIX_RECORD_ENABLE
        printf("\n APP_MSG_REC_PP \n");
        if (get_mix_recorder_status()) {
            mix_recorder_stop();
        } else {
            if (dev_manager_get_phy_logo(dev_manager_find_active(0))) {
                mix_recorder_start();
            }
        }
#endif
        break;
    }
}

int app_common_device_event_handler(int *msg)
{
    int hdl_ret = false;  //默认不拦截消息
    int ret = 0;
    const char *logo = NULL;
    const char *usb_msg = NULL;
    u8 app  = 0xff ;
    switch (msg[0]) {
    case DEVICE_EVENT_FROM_OTG:
        usb_msg = (const char *)msg[2];
        if (usb_msg[0] == 's') {
#if TCFG_USB_SLAVE_ENABLE
            ret = pc_device_event_handler(msg);
            if (ret == 1) {
                if (true != app_in_mode(APP_MODE_PC)) {
                    app = APP_MODE_PC;
                }
            } else if (ret == 2) {
                app_send_message(APP_MSG_GOTO_NEXT_MODE, 0);
            }
#endif
            break;
        } else if (usb_msg[0] == 'h') {
            ///是主机, 统一于SD卡等响应主机处理，这里不break
        } else {
            break;
        }
    case DRIVER_EVENT_FROM_SD0:
    case DRIVER_EVENT_FROM_SD1:
    case DRIVER_EVENT_FROM_SD2:
    case DEVICE_EVENT_FROM_USB_HOST:
#if TCFG_APP_MUSIC_EN
        if (true == app_in_mode(APP_MODE_MUSIC)) {
            music_device_msg_handler(msg);
        }
#endif
#if TCFG_APP_MUSIC_EN || TCFG_MIX_RECORD_ENABLE
        ret = dev_status_event_filter(msg);///解码设备上下线， 设备挂载等处理
#endif
#if TCFG_APP_MUSIC_EN
        if (ret == true) {
            if (msg[1] == DEVICE_EVENT_IN) {
                ///设备上线， 非解码模式切换到解码模式播放
                if (true != app_in_mode(APP_MODE_MUSIC)) {
                    app = APP_MODE_MUSIC;
                }
            }
        }
#endif
#if TCFG_MIX_RECORD_ENABLE
        mix_record_device_msg_handler(msg);
#endif // TCFG_MIX_RECORD_ENABLE
        break;
    case DEVICE_EVENT_FROM_LINEIN:
#if TCFG_APP_LINEIN_EN
        if (msg[1] == DEVICE_EVENT_IN) {
            printf("LINEIN ONLINE");
            if (true != app_in_mode(APP_MODE_LINEIN)) {
                app = APP_MODE_LINEIN;
            }
        } else if (msg[1] == DEVICE_EVENT_OUT) {
            printf("LINEIN OFFLINE");
            if (true == app_in_mode(APP_MODE_LINEIN)) {
                app_send_message(APP_MSG_GOTO_NEXT_MODE, 0);
            }
        }
#endif
        break;

    default:
        /* printf("unknow SYS_DEVICE_EVENT!!, %x\n", (u32)event->arg); */
        break;
    }

    if (app != 0xff) {
        /*一些情况不希望退出蓝牙模式*/
#if TCFG_APP_BT_EN
        if (bt_app_exit_check() == 0) {
            puts("bt_background can not enter\n");
            return hdl_ret;
        }
#endif
        //PC 不响应因为设备上线引发的模式切换
        if (true != app_in_mode(APP_MODE_PC)) {

#if (TCFG_CHARGE_ENABLE && (!TCFG_CHARGE_POWERON_ENABLE))
            if (get_charge_online_flag() && app != APP_MODE_PC) {
                return hdl_ret;
            }
#endif

#if TWFG_APP_POWERON_IGNORE_DEV
            int msec = jiffies_msec2offset(app_var.start_time, jiffies_msec());
            if (msec > TWFG_APP_POWERON_IGNORE_DEV)
#endif
            {
                app_send_message2(APP_MSG_GOTO_MODE, app, 0);
            }
        }
    }
    return hdl_ret;
}


static void app_common_app_event_handler(int *msg)
{
    switch (msg[0]) {
    case APP_MSG_POWER_OFF:
        break;
#if TCFG_AUDIO_ANC_ENABLE
    case APP_MSG_ANC_SWITCH:
#if ANC_EAR_ADAPTIVE_EVERY_TIME && TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
        //每次切模式都自适应需要左右耳一起执行
        anc_mode_next();
        break;
#endif
#if RCSP_MODE && RCSP_ADV_KEY_SET_ENABLE
        update_anc_voice_key_opt();
#else
        anc_mode_next();
#endif
        break;
    case APP_MSG_ANC_ON:
        anc_mode_switch(ANC_ON, 1);
        break;
    case APP_MSG_ANC_OFF:
        anc_mode_switch(ANC_OFF, 1);
        break;
    case APP_MSG_ANC_TRANS:
        anc_mode_switch(ANC_TRANSPARENCY, 1);
        break;
#if (defined TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE) && TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    case APP_MSG_SPEAK_TO_CHAT_SWITCH:
        audio_speak_to_chat_demo();
        break;
#endif
#if (defined TCFG_AUDIO_WIDE_AREA_TAP_ENABLE) && TCFG_AUDIO_WIDE_AREA_TAP_ENABLE
    case APP_MSG_WAT_CLICK_SWITCH:
        audio_wat_click_demo();
        break;
#endif
#if (defined TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE) && TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    case APP_MSG_WIND_DETECT_SWITCH:
        audio_icsd_wind_detect_demo();
        break;
#endif
#if (defined TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE) && TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
    case APP_MSG_ADAPTIVE_VOL_SWITCH:
        audio_icsd_adaptive_vol_demo();
        break;
#endif
#if (defined ANC_EAR_ADAPTIVE_EN) && ANC_EAR_ADAPTIVE_EN
    case APP_MSG_EAR_ADAPTIVE_OPEN:
        audio_anc_mode_ear_adaptive(1);
        break;
#endif

#endif
    case APP_MSG_MIC_OPEN:
        audio_common_mic_mute_en_set(0);
        break;
    case APP_MSG_MIC_CLOSE:
        audio_common_mic_mute_en_set(1);
        break;
#if TCFG_VOICE_CHANGER_NODE_ENABLE
    case APP_MSG_ESCO_UNCLE_VOICE:
        if (esco_eff_uncle) {
            audio_esco_ul_voice_change(VOICE_CHANGER_NONE);
        } else {
            audio_esco_ul_voice_change(VOICE_CHANGER_UNCLE);
        }
        esco_eff_uncle = !esco_eff_uncle;
        break;
    case APP_MSG_ESCO_GODDESS_VOICE:
        if (esco_eff_goddess) {
            audio_esco_ul_voice_change(VOICE_CHANGER_NONE);
        } else {
            audio_esco_ul_voice_change(VOICE_CHANGER_GODDESS);//女声
        }
        esco_eff_goddess = !esco_eff_goddess;
        break;
#endif
#if TCFG_AUDIO_SPATIAL_EFFECT_ENABLE
    case APP_MSG_SPATIAL_EFFECT_SWITCH:
        u8 mode = get_a2dp_spatial_audio_mode();
        if (++mode > 2) {
            mode = 0;
        }
        printf("%s : %d", __func__, mode);
        audio_spatial_effects_mode_switch(mode);
        break;
#endif

#if TCFG_SPEAKER_EQ_NODE_ENABLE
    case APP_MSG_SPK_EQ_TEST:
        spk_eq_test();
        break;
#endif

    default:
        break;
    }
}

int app_default_msg_handler(int *msg)
{
    const struct app_msg_handler *handler;
    struct app_mode *mode;

    mode = app_get_current_mode();
    //消息继续分发
#if (TCFG_BT_BACKGROUND_ENABLE)
    if (!bt_background_msg_forward_filter(msg))
#endif
    {
        for_each_app_msg_handler(handler) {
            if (handler->from == 0xff) {
                handler->handler(msg);
                continue;
            }
            if (handler->from != msg[0]) {
                continue;
            }
            if (mode && mode->name == handler->owner) {
                continue;
            }

            /*蓝牙后台情况下，消息仅转发给后台处理*/
            handler->handler(msg + 1);
        }
    }

    switch (msg[0]) {
    case MSG_FROM_DEVICE:
        app_common_device_event_handler(msg + 1);
        break;
    case MSG_FROM_APP:
        app_common_app_event_handler(msg + 1);
        break;
    default:
        break;
    }

    return 0;
}


