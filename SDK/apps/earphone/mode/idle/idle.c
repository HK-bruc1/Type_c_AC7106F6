#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".idle.data.bss")
#pragma data_seg(".idle.data")
#pragma const_seg(".idle.text.const")
#pragma code_seg(".idle.text")
#endif
#include "idle.h"
#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "app_tone.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "app_main.h"
#include "vm.h"
#include "app_chargestore.h"
#include "app_testbox.h"
#include "user_cfg.h"
#include "key/key_driver.h"
#include "audio_config.h"
#include "app_default_msg_handler.h"
#include "pwm_led/led_ui_api.h"


#if TCFG_SMART_VOICE_ENABLE
#include "smart_voice.h"
#endif

#include "btstack/avctp_user.h"


#if TCFG_ANC_BOX_ENABLE
#include "app_ancbox.h"
#endif

#define LOG_TAG             "[APP_IDLE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


static void wait_led_ui_stop_timeout(void *p)
{
    app_send_message(APP_MSG_SOFT_POWEROFF, 0);
}


static void app_idle_enter_softoff(void)
{

#if TCFG_CHARGE_ENABLE
    if (get_lvcmp_det() && (0 == get_charge_full_flag())) {
        log_info("charge inset, system reset!\n");
        charge_system_reset();
    }
#endif

#if TCFG_PWMLED_ENABLE
    if (!led_ui_state_is_idle()) {
        sys_timeout_add(NULL, wait_led_ui_stop_timeout, 50);
        return;
    }
#endif

#if TCFG_SMART_VOICE_ENABLE
    audio_smart_voice_detect_close();
#endif

    dac_power_off();    // 关机前先关dac

    dlog_flush2flash(100);
    power_set_soft_poweroff();
}


static int app_power_off_tone_cb(void *priv, enum stream_event event)
{
    if (event == STREAM_EVENT_STOP) {
        app_send_message(APP_MSG_SOFT_POWEROFF, 0);
    }
    return 0;
}

static int idle_mode_enter(int param)
{
    log_info("idle_mode_enter: %d\n", param);

    switch (param) {
    case IDLE_MODE_PLAY_POWEROFF:
        if (app_var.goto_poweroff_flag) {
            syscfg_write(CFG_MUSIC_VOL, &app_var.music_volume, 2);
            //如果开启了VM配置项暂存RAM功能则在关机前保存数据到vm_flash
            if (get_vm_ram_storage_enable() || get_vm_ram_storage_in_irq_enable()) {
                vm_flush2flash(1);
            }
            os_taskq_flush();
            int ret = play_tone_file_callback(get_tone_files()->power_off, NULL,
                                              app_power_off_tone_cb);
            printf("power_off tone play ret:%d", ret);
            if (ret) {
                if (app_var.goto_poweroff_flag) {
                    app_send_message(APP_MSG_SOFT_POWEROFF, 0);
                }
            }
        }
        break;
    case IDLE_MODE_POWEROFF:
        if (app_var.goto_poweroff_flag) {
            syscfg_write(CFG_MUSIC_VOL, &app_var.music_volume, 2);
            //如果开启了VM配置项暂存RAM功能则在关机前保存数据到vm_flash
            if (get_vm_ram_storage_enable() || get_vm_ram_storage_in_irq_enable()) {
                vm_flush2flash(1);
            }
            os_taskq_flush();
            app_send_message(APP_MSG_SOFT_POWEROFF, 0);
        }
        break;
    case IDLE_MODE_WAIT_POWEROFF:
        os_taskq_flush();
        syscfg_write(CFG_MUSIC_VOL, &app_var.music_volume, 2);
        break;
    case IDLE_MODE_CHARGE:
        break;
    case IDLE_MODE_WAIT_DEVONLINE://等设备上线
        break;
    }

    return 0;
}

int idle_app_msg_handler(int *msg)
{
    if (!app_in_mode(APP_MODE_IDLE)) {
        return 0;
    }

    switch (msg[0]) {
    case APP_MSG_POWER_OFF:
        break;
    case APP_MSG_SOFT_POWEROFF:
        if (app_var.goto_poweroff_flag) {
            app_idle_enter_softoff();
        }
        break;
    default:
        break;
    }

    return 0;
}

struct app_mode *app_enter_idle_mode(int arg)
{
    int msg[16] = {0};
    struct app_mode *next_mode;

    idle_mode_enter(arg);

    while (1) {
        if (!app_get_message(msg, ARRAY_SIZE(msg), idle_mode_key_table)) {
            continue;
        }
        next_mode = app_mode_switch_handler(msg);
        if (next_mode) {
            break;
        }

        switch (msg[0]) {
        case MSG_FROM_APP:
            idle_app_msg_handler(msg + 1);
            break;
        case MSG_FROM_DEVICE:
            break;
        }

        app_default_msg_handler(msg);
    }

    return next_mode;
}

static int idle_mode_try_enter(int arg)
{
    return 0;
}

static int idle_mode_try_exit()
{
    return 0;
}

static const struct app_mode_ops idle_mode_ops = {
    .try_enter          = idle_mode_try_enter,
    .try_exit           = idle_mode_try_exit,
};
REGISTER_APP_MODE(idle_mode) = {
    .name   = APP_MODE_IDLE,
    .index  = 0xff,
    .ops    = &idle_mode_ops,
};
