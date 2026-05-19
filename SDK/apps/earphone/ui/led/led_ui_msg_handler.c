#include "app_msg.h"
#include "app_main.h"
#include "bt_tws.h"
#include "asm/charge.h"
#include "battery_manager.h"
#include "pwm_led/led_ui_api.h"
#include "a2dp_player.h"
#include "esco_player.h"
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "cig.h"
#include "app_le_connected.h"
#endif


#define LOG_TAG             "[LED_UI]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


#if (TCFG_PWMLED_ENABLE)


static int ui_battery_msg_handler(int *msg)
{
    switch (msg[0]) {
    case BAT_MSG_CHARGE_START:
        led_ui_set_state(LED_STA_RED_ON, DISP_CLEAR_OTHERS);
        break;
    case BAT_MSG_CHARGE_FULL:
    case BAT_MSG_CHARGE_CLOSE:
    case BAT_MSG_CHARGE_ERR:
    case BAT_MSG_CHARGE_LDO5V_OFF:
        led_ui_set_state(LED_STA_BLUE_ON_3S, DISP_CLEAR_OTHERS);
        break;
    }
    return 0;
}

static void led_enter_mode(u8 mode)
{
    switch (mode) {
    case APP_MODE_POWERON:
        break;
    case APP_MODE_BT:
        break;
    case APP_MODE_PC:
    case APP_MODE_LINEIN:
        led_ui_set_state(LED_STA_ALL_OFF, DISP_TWS_SYNC);
        break;

    }
}

static int ui_app_msg_handler(int *msg)
{
    switch (msg[0]) {
    case APP_MSG_ENTER_MODE:
        led_enter_mode(msg[1] & 0xff);
        break;
    case APP_MSG_POWER_ON:
        led_ui_set_state(LED_STA_POWERON, DISP_NON_INTR);
        break;
    case APP_MSG_POWER_OFF:
        // 超时自动关机
        if (msg[1] == POWEROFF_NORMAL ||
            msg[1] == POWEROFF_NORMAL_TWS) {
            led_ui_set_state(LED_STA_RED_ON_1S, DISP_CLEAR_OTHERS);
        }
        break;
    case APP_MSG_TWS_PAIRED:
    case APP_MSG_TWS_UNPAIRED:
        //tws配对状态
        led_ui_set_state(LED_STA_BLUE_1S_FLASHS_3_TIMES, 0);
        break;
    case APP_MSG_BT_IN_PAGE_MODE:
        // 开机回连手机状态
        log_info("APP_MSG_BT_IN_PAGE_MODE\n");
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            led_ui_set_state(LED_STA_BLUE_FAST_FLASH, 0);
        } else {
            led_ui_set_state(LED_STA_BLUE_ON, 0);
        }
        break;
    case APP_MSG_BT_IN_PAIRING_MODE:
        // 等待手机连接状态
        log_info("APP_MSG_BT_IN_PAIRING_MODE\n");
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        if ((bt_get_total_connect_dev() == 0) && !is_cig_phone_conn() && !is_cig_other_phone_conn() && !app_var.goto_poweroff_flag) {
#else
        if ((bt_get_total_connect_dev() == 0) && !app_var.goto_poweroff_flag) {
#endif
            led_ui_set_state(LED_STA_BLUE_FLASH_1TIMES_PER_1S, DISP_TWS_SYNC);
        }
        break;
    }
    return 0;
}



static int ui_bt_stack_msg_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;

#if TCFG_USER_TWS_ENABLE
    int tws_role = tws_api_get_role_async();
    if (tws_role == TWS_ROLE_SLAVE) {
        return 0;
    }
#endif

    printf("ui_bt_stack_msg_handler:%d\n", bt->event);
    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        break;
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_SECOND_CONNECTED:
        led_ui_set_state(LED_STA_BLUE_ON_1S, DISP_NON_INTR | DISP_TWS_SYNC);
        break;
    case BT_STATUS_A2DP_MEDIA_START:
    case BT_STATUS_PHONE_ACTIVE:
    case BT_STATUS_PHONE_HANGUP:
        led_ui_set_state(LED_STA_ALL_OFF, DISP_TWS_SYNC);
        break;
    case BT_STATUS_PHONE_INCOME:
        // 来电
        if (!esco_player_runing()) {
            led_ui_set_state(LED_STA_BLUE_FAST_FLASH, DISP_TWS_SYNC);
        }
        break;
    case BT_STATUS_SCO_CONNECTION_REQ:
        u8 call_status = bt_get_call_status_for_addr(bt->args);
        if (call_status == BT_CALL_INCOMING) {
            // 来电
            if (!esco_player_runing()) {
                led_ui_set_state(LED_STA_BLUE_FAST_FLASH, DISP_TWS_SYNC);
            }
        } else if (call_status == BT_CALL_OUTGOING) {

        }
        break;
    case BT_STATUS_A2DP_MEDIA_STOP:
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
    case BT_STATUS_THIRD_DISCONNECT:
        break;
    case BT_STATUS_SNIFF_STATE_UPDATE:
        break;
    }
    return 0;
}

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
/**
 * @brief CIG连接状态事件处理函数
 *
 * @param msg:状态事件附带的返回参数
 *
 * @return
 */
static int led_ui_app_connected_conn_status_event_handler(int *msg)
{
    int *event = msg;
    /* g_printf("led_ui_app_connected_conn_status_event_handler=%d", event[0]); */

    switch (event[0]) {
    case CIG_EVENT_PERIP_CONNECT:
        /* log_info("CIG_EVENT_PERIP_CONNECT\n"); */
        /* led_ui_set_state(LED_STA_ALL_OFF, 0); */
        break;
    case CIG_EVENT_PERIP_DISCONNECT:
        break;
    case CIG_EVENT_ACL_CONNECT:
        break;
    case CIG_EVENT_ACL_DISCONNECT:
        break;
    case CIG_EVENT_JL_DONGLE_CONNECT:
    case CIG_EVENT_PHONE_CONNECT:
        log_info("CIG_EVENT_PHONE_CONNECT\n");
        led_ui_set_state(LED_STA_BLUE_ON_1S, DISP_NON_INTR);
        break;
    case CIG_EVENT_JL_DONGLE_DISCONNECT:
    case CIG_EVENT_PHONE_DISCONNECT:
        break;
    default:
        break;
    }

    return 0;
}
APP_MSG_PROB_HANDLER(app_le_connected_led_ui_msg_entry) = {
    .owner = 0xff,
    .from = MSG_FROM_CIG,
    .handler = led_ui_app_connected_conn_status_event_handler,
};
#endif

void ui_pwm_led_msg_handler(int *msg)
{
    struct led_state_obj *obj;

    switch (msg[0]) {
    case LED_MSG_STATE_END:
        // 执行灯效结束消息, 可通过分配唯一的name来区分是哪个灯效
        obj = led_ui_get_state(msg[1]);
        if (obj) {
            log_info("LED_STATE_END: name = %d\n", obj->name);
            if (obj->name == LED_STA_POWERON) {

            }
        }
#if TCFG_USER_TWS_ENABLE
        led_tws_state_sync_stop();
#endif
        led_ui_state_machine_run();
        break;
    }

}

#if TCFG_USER_TWS_ENABLE
static int ui_tws_msg_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    u8 role = evt->args[0];
    u8 state = evt->args[1];
    u8 *bt_addr = evt->args + 3;

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        if (role == TWS_ROLE_SLAVE) {
            led_ui_set_state(LED_STA_ALL_OFF, 0);
        } else {
            struct led_state_obj *obj;
            obj = led_ui_get_first_state();
            if (obj && obj->disp_mode & DISP_TWS_SYNC) {
                led_ui_state_machine_run();
            }
        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        led_tws_state_sync_stop();
        break;
    }
    return 0;
}
#endif



static int ui_led_msg_handler(int *msg)
{
    switch (msg[0]) {
#if TCFG_USER_TWS_ENABLE
    case MSG_FROM_TWS:
        ui_tws_msg_handler(msg + 1);
        break;
#endif
    case MSG_FROM_BT_STACK:
        ui_bt_stack_msg_handler(msg + 1);
        break;
    case MSG_FROM_APP:
        ui_app_msg_handler(msg + 1);
        break;
    case MSG_FROM_BATTERY:
        ui_battery_msg_handler(msg + 1);
        break;
    case MSG_FROM_PWM_LED:
        ui_pwm_led_msg_handler(msg + 1);
        break;
    default:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(led_msg_entry) = {
    .owner      = 0xff,
    .from       = 0xff,
    .handler    = ui_led_msg_handler,
};


#endif

