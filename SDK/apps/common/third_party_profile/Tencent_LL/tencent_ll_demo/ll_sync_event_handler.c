#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ll_sync_event_handler.data.bss")
#pragma data_seg(".ll_sync_event_handler.data")
#pragma const_seg(".ll_sync_event_handler.text.const")
#pragma code_seg(".ll_sync_event_handler.text")
#endif
#include "app_config.h"
#include "3th_profile_api.h"
#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "bt_tws.h"
#include "ble_qiot_export.h"
#include "app_main.h"
#include "ble_iot_anc_manager.h"
#include "bt_common.h"
#include "user_cfg.h"
#include "ble_qiot_template.h"
#include "ble_user.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

#define LOG_TAG             "[ll_sync]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

extern u16 ll_sync_get_core_data(u8 *data);

#define LL_TWS_FUNC_BLE_STATE_SYNC \
	TWS_FUNC_ID('L' + 'L', \
			'B' + 'L'  + 'E', \
			'S' + 'T' + 'A' + 'T' + 'E', \
			'S' + 'Y' + 'N' + 'C')

int ll_sync_earphone_state_tws_connected(int first_pair, u8 *comm_addr)
{
    if (first_pair) {
        /* bt_ble_adv_enable(0); */
        u8 tmp_ble_addr[6] = {0};
        bt_make_ble_address(tmp_ble_addr, comm_addr);
        le_controller_set_mac(tmp_ble_addr);//将ble广播地址改成公共地址
        bt_update_mac_addr(comm_addr);
        /* bt_ble_adv_enable(1); */

        /*新的连接，公共地址改变了，要重新将新的地址广播出去*/
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("\nNew Connect Master!!!\n\n");
            ble_app_disconnect();
            ble_qiot_advertising_start(1);
        } else {
            printf("\nConnect Slave!!!\n\n");
            /*从机ble关掉*/
            ble_app_disconnect();
            ble_qiot_advertising_stop();
        }
    }

    return 0;
}

extern u16 bt_ble_is_connected(void);
void bt_tws_sync_ll_sync_state()
{
    u8 data[20];
    u16 data_len;

    printf("bt_tws_sync_ll_sync_state role: %s\n", (tws_api_get_role() == TWS_ROLE_MASTER) ? "master" : "slave");

    if (bt_ble_is_connected()) {
        data_len = ll_sync_get_core_data(data);

        printf("ll sync data send to tws");
        put_buf((const u8 *)&data, data_len);

        tws_api_send_data_to_sibling(data, data_len, TWS_FUNC_ID_LL_SYNC_STATE);
    }
}

/* int ll_bt_hci_event_handler(int *msg) */
/* { */
/* 	struct bt_event *bt = (struct bt_event *)msg; */
/*     log_info("-----------bt_hci_event_handler reason %x %x", bt->event, bt->value); */
/* 		 */
/* 	switch (bt->event) { */
/* 		case HCI_EVENT_CONNECTION_COMPLETE: */
/* 			log_info("AHCI_EVENT_CONNECTION_COMPLETE \n"); */
/* 			// 启动主动上报设备信息定时任务 */
/* 			#<{(| qiot_start_report_property_timer(); |)}># */
/* 			break; */
/* 		case HCI_EVENT_DISCONNECTION_COMPLETE : */
/* 			log_info("AHCI_EVENT_DISCONNECTION_COMPLETE \n"); */
/* 			// 关闭主动上报设备信息定时任务 */
/* 			#<{(| qiot_stop_report_property_timer(); |)}># */
/* 			break; */
/* 		default: */
/* 			break; */
/* 	} */
/* 	return 0; */
/* } */
/*  */
/* APP_MSG_HANDLER(ll_bt_hci_msg_handler_user_ble) = { */
/* 	.owner      = 0xff, */
/* 	.from       = MSG_FROM_BT_HCI, */
/* 	.handler    = ll_bt_hci_event_handler, */
/* }; */

int ll_sync_tws_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    u8 role = evt->args[0];
    u8 state = evt->args[1];
    u8 *bt_addr = evt->args + 3;
    u8 addr[6];

    printf(">>>>>>>>>>>>>>>>>>>>> ll_sync_tws_event_handler %d\n", evt->event);
    printf("tws role: %s\n", (tws_api_get_role() == TWS_ROLE_MASTER) ? "master" : "slave");
    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        bt_tws_sync_ll_sync_state();
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            //master enable
            printf("\nConnect Slave!!!\n\n");
            /*从机ble关掉*/
            ble_app_disconnect();
            ble_qiot_advertising_stop();
        }
#if CONFIG_ANC_ENABLE
        iot_anc_effect_setting_sync();
#endif
        break;
    case TWS_EVENT_CONNECTION_TIMEOUT:
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        /*
         * TWS连接断开
         */
        if (app_var.goto_poweroff_flag) {
            break;
        }
        bt_tws_sync_ll_sync_state();
        if (get_app_connect_type() == TYPE_NULL) {
            printf("\ntws detach to open ble~~~\n\n");

            ble_qiot_advertising_start(1);
        }
        set_ble_connect_type(TYPE_NULL);
        ble_event_post(BLE_QIOT_EVENT_ID_TWS_WILL_DISCONNECT);
        break;
    case TWS_EVENT_MONITOR_START:
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        /*
         * 跟手机的链路LMP层已完全断开, 只有tws在连接状态才会收到此事件
         */
        break;
    case TWS_EVENT_ROLE_SWITCH:
        printf("TWS_EVENT_ROLE_SWITCH, %d\n", __LINE__);
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            // 同步ble ccc状态给新主机
            extern bool is_last_ble_sta_notify_indicate_open();
            bool last_ble_sta_notify_indicate_open = is_last_ble_sta_notify_indicate_open();
            /* printf("last_ble_sta_notify_indicate_open:%d!\n", last_ble_sta_notify_indicate_open); */
            u8 ble_sta_data[1] = {0};
            ble_sta_data[0] = (u8)last_ble_sta_notify_indicate_open;
            tws_api_send_data_to_sibling(ble_sta_data, sizeof(ble_sta_data), LL_TWS_FUNC_BLE_STATE_SYNC);

            //master enable
            printf("\nConnect Slave!!!\n\n");
            /*从机ble关掉*/
            ble_app_disconnect();
            ble_qiot_advertising_stop();
        }
        break;
    }
    return 0;
}

APP_MSG_HANDLER(ll_tws_msg_handler_user_ble) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = ll_sync_tws_event_handler,
};

static int ll_app_msg_handler(int *msg)
{
    u8 comm_addr[6];

    switch (msg[0]) {
    case APP_MSG_ENTER_MODE:
        log_info("APP_MSG_ENTER_MODE");
        if ((msg[1] & 0xff) == APP_MODE_BT) {
#if CONFIG_ANC_ENABLE
            iot_anc_effect_setting_init();
#endif
        }
        break;
    case APP_MSG_BT_GET_CONNECT_ADDR:
        log_info("APP_MSG_BT_GET_CONNECT_ADDR");
        break;
    case APP_MSG_BT_OPEN_PAGE_SCAN:
        log_info("APP_MSG_BT_OPEN_PAGE_SCAN");
        break;
    case APP_MSG_BT_CLOSE_PAGE_SCAN:
        log_info("APP_MSG_BT_CLOSE_PAGE_SCAN");
        break;
    case APP_MSG_BT_ENTER_SNIFF:
        break;
    case APP_MSG_BT_EXIT_SNIFF:
        break;
    case APP_MSG_TWS_PAIRED:
        log_info("APP_MSG_TWS_PAIRED");
        break;
    case APP_MSG_TWS_UNPAIRED:
        log_info("APP_MSG_TWS_UNPAIRED");
        break;
    case APP_MSG_TWS_PAIR_SUSS:
        log_info("APP_MSG_TWS_PAIR_SUSS");
        break;
    case APP_MSG_TWS_CONNECTED:
        log_info("APP_MSG_TWS_CONNECTED");
        ll_sync_earphone_state_tws_connected(0, NULL);
        break;
    case APP_MSG_POWER_OFF:
        log_info("APP_MSG_POWER_OFF");
        bt_ble_exit();
        break;
    default:
        break;
    }
    return 0;
}
APP_MSG_HANDLER(ll_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = ll_app_msg_handler,
};

extern void ll_sync_state_deal(void *_data, u16 len);
static void bt_tws_ll_sync_state(void *_data, u16 len, bool rx)
{
    if (rx) {
        printf("slave ll sync data");
        put_buf(_data, len);

        // 该函数是在中断里面调用，实际处理放在task里面
        int argv[4];
        argv[0] = (int)ll_sync_state_deal;
        argv[1] = 2;
        argv[2] = (int)_data;
        argv[3] = (int)len;
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 4, argv);
        if (ret) {
            log_e("taskq post err \n");
        }
    }
}

REGISTER_TWS_FUNC_STUB(app_ll_sync_state_stub) = {
    .func_id = TWS_FUNC_ID_LL_SYNC_STATE,
    .func    = bt_tws_ll_sync_state,
};

static void ll_ble_state_sync_func_t(void *data, u16 len, bool rx)
{
    if (rx) {
        u8 *ble_sta_data = (u8 *)data;
        bool m_last_ble_sta_notify_indicate_open = (bool)ble_sta_data[0];
        /* printf("m_last_ble_sta_notify_indicate_open:%d!\n", m_last_ble_sta_notify_indicate_open); */
        extern ble_state_e get_ble_work_state(void);
        if ((get_ble_work_state() == BLE_ST_CONNECT) && m_last_ble_sta_notify_indicate_open) {
            extern void set_ble_work_state(ble_state_e state);
            set_ble_work_state(BLE_ST_NOTIFY_IDICATE);
        }
    }
}

REGISTER_TWS_FUNC_STUB(ll_ble_state_sync) = {
    .func_id = LL_TWS_FUNC_BLE_STATE_SYNC,
    .func    = ll_ble_state_sync_func_t,
};

#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)
