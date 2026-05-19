#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".sniff.data.bss")
#pragma data_seg(".sniff.data")
#pragma const_seg(".sniff.text.const")
#pragma code_seg(".sniff.text")
#endif
#include "btstack/avctp_user.h"

#include "app_config.h"
#include "app_main.h"
#include "earphone.h"
#include "bt_tws.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif
#if (RCSP_ADV_EN)
#include "ble_rcsp_adv.h"
#include "btstack_rcsp_user.h"
#include "ble_rcsp_server.h"
#endif
#if TCFG_LE_AUDIO_APP_CONFIG
#include "le_audio_player.h"
#endif
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
#include "app_le_auracast.h"
#endif


#if TCFG_APP_BT_EN

#define LOG_TAG             "[SNIFF]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


/*
 *以下情况，关闭sniff
 *(1)通过spp在线调试eq
 *(2)通过spp导出数据
 */
#if APP_ONLINE_DEBUG
#if ((TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SPP) || \
    TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE || USE_DMA_UART_TEST)
/*!!! 不要使用MY_SNIFF_EN宏去控制sniff开关，这样会导致协议栈状态错误!!!*/
static u8 sniff_enable = 0;
#else
static u8 sniff_enable = 1;
#endif
#else
static u8 sniff_enable = 1;
#endif

#ifndef TCFG_SNIFF_CHECK_TIME
#define TCFG_SNIFF_CHECK_TIME 5 // 检测时间
#endif
#define SNIFF_CNT_TIME                  TCFG_SNIFF_CHECK_TIME  //空闲6S之后进入sniff模式
#define SNIFF_MAX_INTERVALSLOT          800
#define SNIFF_MIN_INTERVALSLOT          100
#define SNIFF_ATTEMPT_SLOT              4
#define SNIFF_TIMEOUT_SLOT              1


u8 sniff_ready_status = 0; //0:sniff_ready 1:sniff_not_ready


bool bt_is_sniff_close(void)
{
    return (g_bt_hdl.sniff_timer == 0);
}
u8 check_local_not_accept_sniff_by_remote()
{
#if TCFG_LE_AUDIO_APP_CONFIG
    if (le_audio_player_is_playing()) {
        return TRUE;
    }
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
    if (le_auracast_is_running()) {
        return TRUE;
    }
#endif
#endif
    return FALSE;
}

void bt_check_exit_sniff()
{
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    bt_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
}

void bt_sniff_ready_clean(void)
{
    sniff_ready_status = 1;
}

void bt_check_enter_sniff()
{
    u8 addr[18];
#if TCFG_BT_SNIFF_ENABLE

#if TCFG_AUDIO_ANC_ENABLE
    if (anc_train_open_query() || anc_online_busy_get()) { //如果ANC训练则不进入SNIFF
        return;
    }
#endif

#if (RCSP_ADV_EN)
    u8 rcsp_max_con_dev = rcsp_max_support_con_dev_num();
    u8 rcsp_conn_num = bt_rcsp_device_conn_num();
    if (get_ble_adv_modify() || get_ble_adv_notify()) {
        // rcsp广播内容更改需要通知信息到手机 || 手机APP连接后需要主动通知信息
        return;
    }
#endif

    if (sniff_ready_status) {
        sniff_ready_status = 0;
        return;
    }

    if (bt_get_esco_coder_busy_flag()) {
        return;
    }
    if (check_local_not_accept_sniff_by_remote()) {
        return;
    }

    int conn_cnt = bt_api_enter_sniff_status_check(SNIFF_CNT_TIME, addr);
    ASSERT(conn_cnt <= TCFG_BT_SUPPORT_CONN_NUM);
    struct sniff_ctrl_config_t config = {0};
    for (int i = 0; i < conn_cnt; i++) {
        log_info("-----USER SEND SNIFF IN %d %d\n", i, conn_cnt);
        config.sniff_max_interval = SNIFF_MAX_INTERVALSLOT;
        config.sniff_mix_interval = SNIFF_MIN_INTERVALSLOT;
        config.sniff_attemp = SNIFF_ATTEMPT_SLOT;
        config.sniff_timeout  = SNIFF_TIMEOUT_SLOT;
        memcpy(config.sniff_addr, addr + i * 6, 6);
        bt_cmd_prepare(USER_CTRL_SNIFF_IN, sizeof(config), (u8 *)&config);
    }
#endif
}

void sys_auto_sniff_controle(u8 enable, u8 *addr)
{
    if (addr) {
        if (bt_api_conn_mode_check(enable, addr) == 0) {
            log_info("sniff ctr not change\n");
            return;
        }
    }

    if (enable) {

        if (!sniff_enable) {
            //sniff_enable为0时不启动定时器去检测进入sniff
            return;
        }

        if (tws_api_get_role_async() == TWS_ROLE_SLAVE) {
            return;
        }
        if (g_bt_hdl.sniff_timer == 0) {
            log_info("check_sniff_enable\n");
            g_bt_hdl.sniff_timer = sys_timer_add(NULL, bt_check_enter_sniff, 1000);
        }
    } else {
        if (g_bt_hdl.sniff_timer) {
            log_info("check_sniff_disable\n");
            sys_timeout_del(g_bt_hdl.sniff_timer);
            g_bt_hdl.sniff_timer = 0;
        }
    }
}

void bt_sniff_enable()
{
    sniff_enable = 1;
    sys_auto_sniff_controle(1, NULL);
}

void bt_sniff_disable()
{
    sys_auto_sniff_controle(0, NULL);
    bt_check_exit_sniff();
    sniff_enable = 0;
}

void bt_sniff_feature_init()
{
#if TCFG_BT_SNIFF_ENABLE == 0
    u8 feature = lmp_hci_read_local_supported_features(0);
    feature &= ~BIT(7); //BIT_SNIFF_MODE;
    lmp_hci_write_local_supported_features(feature, 0);
#endif
}

static int sniff_btstack_event_handler(int *_event)
{
    struct bt_event *bt = (struct bt_event *)_event;

    switch (bt->event) {
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_THIRD_CONNECTED:
        sys_auto_sniff_controle(1, bt->args);
        break;
    case BT_STATUS_SNIFF_STATE_UPDATE:
        log_info(" BT_STATUS_SNIFF_STATE_UPDATE %d\n", bt->value);    //0退出SNIFF
        if (bt->value == 0) {
            sys_auto_sniff_controle(1, bt->args);
            app_send_message(APP_MSG_BT_EXIT_SNIFF, 0);
        } else {
            sys_auto_sniff_controle(0, bt->args);
            app_send_message(APP_MSG_BT_ENTER_SNIFF, 0);
        }
        break;
    }
    return 0;
}
APP_MSG_HANDLER(sniff_btstack_msg_stub) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = sniff_btstack_event_handler,
};



static int sniff_tws_event_handler(int *_event)
{
    struct tws_event *event = (struct tws_event *)_event;
    int role = event->args[0];
    int reason = event->args[2];

    if (app_var.goto_poweroff_flag) {
        return 0;
    }

    switch (event->event) {
    case TWS_EVENT_CONNECTION_DETACH:
        break;
    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_MASTER) {
            sys_auto_sniff_controle(1, NULL);
        } else {
            sys_auto_sniff_controle(0, NULL);
        }
        break;
    }

    return 0;
}

APP_MSG_HANDLER(sniff_tws_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = sniff_tws_event_handler,
};

#endif
