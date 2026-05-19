#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".poweroff.data.bss")
#pragma data_seg(".poweroff.data")
#pragma const_seg(".poweroff.text.const")
#pragma code_seg(".poweroff.text")
#endif
#include "classic/hci_lmp.h"
#include "btstack/avctp_user.h"

#include "app_config.h"
#include "app_tone.h"
#include "app_main.h"
#include "earphone.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "idle.h"
#include "app_charge.h"
#include "bt_slience_detect.h"
#include "poweroff.h"
#include "bt_background.h"
#include "usb/otg.h"
#include "btstack/le/le_user.h"
#include "asm/charge.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "app_le_connected.h"
#endif
#include "health_long_detect.h"


#if (TCFG_USER_TWS_ENABLE == 0)

#define LOG_TAG             "[POWEROFF]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"




static u16 g_poweroff_timer = 0;
static u16 g_bt_detach_timer = 0;


static void sys_auto_shut_down_deal(void *priv);


void sys_auto_shut_down_disable(void)
{
#if TCFG_AUTO_SHUT_DOWN_TIME
    log_info("sys_auto_shut_down_disable\n");
    if (g_poweroff_timer) {
        sys_timeout_del(g_poweroff_timer);
        g_poweroff_timer = 0;
    }
#endif
}

void sys_auto_shut_down_enable(void)
{
#if TCFG_AUTO_SHUT_DOWN_TIME
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    if (is_cig_phone_conn() || is_cig_other_phone_conn()) {
        printf("is_cig_phone_conn not auto shut down");
        return;
    }
#endif
#if TCFG_BT_BACKGROUND_ENABLE
    if (bt_background_active()) {
        log_info("sys_auto_shut_down_enable cannot in background\n");
        return;
    }
#endif
    /*ANC打开，不支持自动关机*/
#if TCFG_AUDIO_ANC_ENABLE
#if defined(TCFG_AUDIO_ANC_ON_AUTO_SHUT_DOWN) && (TCFG_AUDIO_ANC_ON_AUTO_SHUT_DOWN == 0)
    if (anc_status_get()) {
        return;
    }
#endif
#endif

    log_info("sys_auto_shut_down_enable\n");

    if (g_poweroff_timer == 0) {
        g_poweroff_timer = sys_timeout_add(NULL, sys_auto_shut_down_deal,
                                           app_var.auto_off_time * 1000);
    }
#endif
}

static void sys_auto_shut_down_deal(void *priv)
{
    sys_enter_soft_poweroff(POWEROFF_NORMAL);
}


static int poweroff_app_event_handler(int *msg)
{
    switch (msg[0]) {
    case APP_MSG_BT_IN_PAIRING_MODE:
        if (msg[1] == 0) {
#if TCFG_APP_BT_EN
            if (bt_mode_is_try_exit()) {
                break;
            }
#endif
            sys_auto_shut_down_enable();
        }
        break;
    }
    return 0;
}
APP_MSG_HANDLER(poweroff_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = poweroff_app_event_handler,
};


static int poweroff_btstack_event_handler(int *_event)
{
    struct bt_event *bt = (struct bt_event *)_event;

    switch (bt->event) {
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_THIRD_CONNECTED:
        sys_auto_shut_down_disable();
        break;
    }
    return 0;
}
APP_MSG_HANDLER(poweroff_btstack_msg_stub) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = poweroff_btstack_event_handler,
};

static void wait_exit_btstack_flag(void *_reason)
{
    int reason = (int)_reason;

    if (!a2dp_player_runing() && !esco_player_runing()) {
        lmp_hci_reset();
        os_time_dly(2);
        sys_timer_del(g_bt_detach_timer);

        switch (reason) {
        case POWEROFF_NORMAL:
            log_info("task_switch to idle...\n");
            app_send_message(APP_MSG_GOTO_MODE, APP_MODE_IDLE | (IDLE_MODE_PLAY_POWEROFF << 8));

            break;
        case POWEROFF_RESET:
            log_info("cpu_reset!!!\n");
            cpu_reset();
            break;
#if TCFG_CHARGE_ENABLE
        case POWEROFF_POWER_RESET:
            log_info("system_reset!!!\n");
            charge_system_reset();
            break;
        case POWEROFF_POWER_KEEP:
            app_charge_power_off_keep_mode();
            break;
#endif
        }
    } else {
        if (++app_var.goto_poweroff_cnt > 200) {
            log_info("cpu_reset!!!\n");
            cpu_reset();
        }
        printf("wait_poweroff_cnt: %d\n", app_var.goto_poweroff_cnt);
    }
}


void sys_enter_soft_poweroff(enum poweroff_reason reason)
{
    log_info("sys_enter_soft_poweroff: %d\n", reason);
#if TCFG_HRSENSOR_ENABLE && JL_RCSP_EAR_SENSORS_DATA_OPT
#if HEALTH_ALL_DAY_CHECK_ENABLE
    save_on_shutdown();
#endif
#endif
#if ((TCFG_OTG_MODE & OTG_SLAVE_MODE) && (TCFG_OTG_MODE & OTG_CHARGE_MODE))
    u32 otg_status = usb_otg_online(0);
    if (otg_status == SLAVE_MODE) {
        return;
    }
#endif

    if (app_var.goto_poweroff_flag) {
        return;
    }

    app_var.goto_poweroff_flag = 1;
    app_var.goto_poweroff_cnt = 0;
    sys_auto_shut_down_disable();

#if TCFG_APP_BT_EN
    void bt_sniff_disable();
    bt_sniff_disable();
#endif

    bt_stop_a2dp_slience_detect(NULL);

    app_send_message(APP_MSG_POWER_OFF, reason);

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    le_audio_disconn_le_audio_link_no_reconnect();
#endif

    bt_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);

#if (SYS_DEFAULT_VOL == 0)
    syscfg_write(CFG_SYS_VOL, &app_var.music_volume, 2);
#endif

#if TCFG_AUDIO_ANC_ENABLE
    anc_poweroff();
#endif

    g_bt_detach_timer = sys_timer_add((void *)reason, wait_exit_btstack_flag, 50);
}

#endif // (TCFG_USER_TWS_ENABLE == 0)

