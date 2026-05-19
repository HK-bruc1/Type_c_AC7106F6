#include "system/includes.h"
#include "app_action.h"
#include "app_main.h"
#include "earphone.h"
#include "tone_player.h"
#include "app_tone.h"
#include "linein.h"
#include "linein_dev.h"
#include "linein_player.h"
#include "audio_config.h"
#include "app_default_msg_handler.h"
#include "local_tws.h"

#if TCFG_APP_LINEIN_EN

//开关linein后，是否保持变调状态
#define LINEIN_PLAYBACK_PITCH_KEEP          0

#define LOG_TAG             "[APP_LINEIN]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

struct linein_opr {
    s16 volume;
    u8 onoff;
    u8 audio_state; /*判断linein模式使用模拟音量还是数字音量*/
    u8 linein_is_active;
};
static struct linein_opr linein_hdl = {0};
#define __this 	(&linein_hdl)

/**
 * @brief linein 音量设置函数
 * @param 需要设置的音量
 * @note 在linein 情况下针对了某些情景进行了处理，设置音量需要使用独立接口
 */
void linein_volume_set(s16 vol)
{
    app_audio_set_volume(__this->audio_state, vol, 1);
    log_info("linein vol: %d", __this->volume);
    __this->volume = vol;
}

/**
 * @brief linein 状态/音频流开启
 * @return 无
 */
static void linein_start(void)
{
    if (__this->onoff == 1) {
        log_info("linein is aleady start\n");
    }

    /* set_dac_start_delay_time(1, 1); //设置dac通道的启动延时 */
    linein_player_open();
#if (TCFG_PITCH_SPEED_NODE_ENABLE && LINEIN_PLAYBACK_PITCH_KEEP)
    audio_pitch_default_parm_set(app_var.pitch_mode);
    linein_file_pitch_mode_init(app_var.pitch_mode);
#endif
    __this->audio_state = APP_AUDIO_STATE_MUSIC;
    __this->volume = app_audio_get_volume(__this->audio_state);
    __this->onoff = 1;
}

/**
 * @brief linein 状态/音频流关闭
 */
static void linein_stop(void)
{
    if (__this->onoff == 0) {
        log_info("linein is aleady stop\n");
        return;
    }
    /* set_dac_start_delay_time(0, 0); //把dac的延时设置回默认的配置 */
    linein_player_close();
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, __this->volume, 1);
    __this->onoff = 0;
}

/**
 * @brief    linein 播放暂停切换
 * @return   播放暂停状态
 */
int linein_pp(void)
{
    if (__this->onoff) {
        linein_stop();
    } else {
        linein_start();
    }
    log_info("pp:%d \n", __this->onoff);
    return  __this->onoff;
}

u8 linein_get_status(void)
{
    return __this->onoff;
}

void linein_key_vol_up()
{
    s16 vol;
    if (__this->volume < app_audio_volume_max_query(AppVol_LINEIN)) {
        __this->volume ++;
        linein_volume_set(__this->volume);
    } else {
        linein_volume_set(__this->volume);
        if (tone_player_runing() == 0) {
#if TCFG_MAX_VOL_PROMPT
            play_tone_file(get_tone_files()->max_vol);
#endif
        }
    }
    vol = __this->volume;
    app_send_message(APP_MSG_VOL_CHANGED, vol);
    log_info("vol+:%d\n", __this->volume);
}

void linein_key_vol_down()
{
    s16 vol;
    if (__this->volume) {
        __this->volume --;
        linein_volume_set(__this->volume);
    }
    vol = __this->volume;
    app_send_message(APP_MSG_VOL_CHANGED, vol);
    log_info("vol-:%d\n", __this->volume);
}

static int linein_tone_play_end_callback(void *priv, enum stream_event event)
{
    if (false == app_in_mode(APP_MODE_LINEIN)) {
        return 0;
    }
    if (!__this->linein_is_active) {
        return 0;
    }
    switch (event) {
    case STREAM_EVENT_NONE:
    case STREAM_EVENT_STOP:
        app_send_message(APP_MSG_LINEIN_START, 0);
        break;
    default:
        break;
    }
    return 0;
}

static int linein_mode_try_enter(int arg)
{
    if (linein_is_online()) {
        return 0;
    }
    return -1;
}

static int linein_mode_init()
{
    printf("linein mode\n");
    __this->linein_is_active = 1;

    int ret = -1;

#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    ret = local_tws_enter_mode(get_tone_files()->linein_mode, NULL);
#endif //TCFG_LOCAL_TWS_ENABLE

    //cppcheck-suppress knownConditionTrueFalse
    if (ret != 0) {
        tone_player_stop();
        ret = play_tone_file_callback(get_tone_files()->linein_mode, NULL, linein_tone_play_end_callback);
        if (ret != 0) {
            printf("linein tone play err!!!");
            app_send_message(APP_MSG_LINEIN_START, 0);
        }
    }
#if TCFG_PITCH_SPEED_NODE_ENABLE
    app_var.pitch_mode = PITCH_0;    //设置变调初始模式
#endif
    app_send_message(APP_MSG_ENTER_MODE, APP_MODE_LINEIN);
    return 0;
}

static int linein_mode_try_exit()
{
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    local_tws_exit_mode();
#endif
    linein_stop();
    __this->linein_is_active = 0;
    return 0;
}

static int linein_mode_exit()
{
    app_send_message(APP_MSG_EXIT_MODE, APP_MODE_LINEIN);
    return 0;
}

static int linein_app_msg_handler(int *msg)
{
    if (!app_in_mode(APP_MODE_LINEIN)) {
        return 0;
    }
    switch (msg[0]) {
    case APP_MSG_LINEIN_START:
        printf("app msg linein start\n");
        linein_start();
        break;
    case APP_MSG_MUSIC_PP:
        linein_pp();
        break;
    case APP_MSG_VOL_UP:
        linein_key_vol_up();
        break;
    case APP_MSG_VOL_DOWN:
        linein_key_vol_down();
        break;
    default:
        break;
    }

    return 0;
}

struct app_mode *app_enter_linein_mode(int arg)
{
    int msg[16] = {0};
    struct app_mode *next_mode;

    linein_mode_init();

    while (1) {
        if (!app_get_message(msg, ARRAY_SIZE(msg), linein_mode_key_table)) {
            continue;
        }
        next_mode = app_mode_switch_handler(msg);
        if (next_mode) {
            break;
        }

        switch (msg[0]) {
        case MSG_FROM_APP:
            linein_app_msg_handler(msg + 1);
            break;
        case MSG_FROM_DEVICE:
            break;
        }

        app_default_msg_handler(msg);
    }

    linein_mode_exit();

    return next_mode;
}

static u8 linein_idle_query(void)
{
    return !__this->linein_is_active;
}

REGISTER_LP_TARGET(linein_lp_target) = {
    .name = "linein",
    .is_idle = linein_idle_query,
};


static const struct app_mode_ops linein_mode_ops = {
    .try_enter      = linein_mode_try_enter,
    .try_exit       = linein_mode_try_exit,
};


REGISTER_APP_MODE(linein_mode) = {
    .name   = APP_MODE_LINEIN,
    .index  = APP_MODE_LINEIN_INDEX,
    .ops    = &linein_mode_ops,
};

#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE

void linein_local_start(void *priv)
{
    linein_start();
}

REGISTER_LOCAL_TWS_OPS(linein) = {
    .name 	= APP_MODE_LINEIN,
    .local_audio_open = linein_local_start,
    .get_play_status = linein_get_status,
};

#endif //TCFG_LOCAL_TWS_ENABLE

#endif
