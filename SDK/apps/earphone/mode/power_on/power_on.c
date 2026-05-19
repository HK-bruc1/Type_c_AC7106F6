#include "system/includes.h"
#include "app_action.h"
#include "app_main.h"
#include "tone_player.h"
#include "app_tone.h"
#include "app_default_msg_handler.h"
#include "local_tws.h"

#define LOG_TAG             "[APP_POWER_ON]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

//*----------------------------------------------------------------------------*/
/**@brief   开机启动
  @param    无
  @return
  @note
 */
/*----------------------------------------------------------------------------*/
static void poweron_task_start()
{
#if TCFG_APP_BT_EN
    app_send_message(APP_MSG_GOTO_MODE, APP_MODE_BT);
#else
    app_send_message(APP_MSG_GOTO_NEXT_MODE, 0);
#endif
}

static int poweron_tone_play_end_callback(void *priv, enum stream_event event)
{
    if (!app_in_mode(APP_MODE_POWERON)) {
        return 0;
    }

    if (event == STREAM_EVENT_STOP) {
        poweron_task_start();
    }
    return 0;
}

static int poweron_mode_init()
{
    log_info("power on");

    if (app_var.play_poweron_tone) {
        int ret = play_tone_file_callback(get_tone_files()->power_on, NULL, poweron_tone_play_end_callback);
        if (ret != 0) {
            log_error("power on tone play err!!!");
            poweron_task_start();
        }
    } else {
        poweron_task_start();
    }
    app_send_message(APP_MSG_ENTER_MODE, APP_MODE_POWERON);
    return 0;
}

static int poweron_mode_msg_handler(int *msg)
{
    return 0;
}

static int poweron_mode_try_exit()
{
    //上电提示音播放结束前不退出模式
#if TCFG_BT_BACKGROUND_ENABLE
    if (tone_player_runing()) {
        return 1;
    } else {
        return 0;
    }
#else
    return 0;
#endif
}

static int poweron_mode_exit()
{
    log_info("exit power on mode");
    app_send_message(APP_MSG_EXIT_MODE, APP_MODE_POWERON);

    return 0;
}

struct app_mode *app_enter_poweron_mode(int arg)
{
    int msg[16] = {0};
    struct app_mode *next_mode;

    poweron_mode_init();

    while (1) {
        if (!app_get_message(msg, ARRAY_SIZE(msg), NULL)) {
            continue;
        }
        next_mode = app_mode_switch_handler(msg);
        if (next_mode) {
            break;
        }

        switch (msg[0]) {
        case MSG_FROM_APP:
            break;
        case MSG_FROM_DEVICE:
            break;
        }

        app_default_msg_handler(msg);
    }

    poweron_mode_exit();

    return next_mode;
}

static const struct app_mode_ops poweron_mode_ops = {
    .try_exit       = poweron_mode_try_exit,
};

/*
 * 注册power_on模式
 */
REGISTER_APP_MODE(poweron_mode) = {
    .name 	= APP_MODE_POWERON,
    .index  = 0xff,
    .ops 	= &poweron_mode_ops,
};

