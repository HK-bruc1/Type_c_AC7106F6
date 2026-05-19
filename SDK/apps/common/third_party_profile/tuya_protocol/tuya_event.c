#include "app_config.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & TUYA_DEMO_EN)

// #include "earphone.h"
#include "app_main.h"
#include "3th_profile_api.h"
#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "bt_tws.h"
#include "update_tws.h"
#include "update_tws_new.h"

#include "tuya_ble_app_demo.h"
#include "effects/audio_eq.h"
#include "tone_player.h"
#include "user_cfg.h"
#include "key_event_deal.h"
#include "app_power_manage.h"
#include "key_driver.h"
#include "app_tone.h"
#include "audio_config.h"

#include "app_ble_spp_api.h"
#include "tuya_ble_api.h"
#include "tuya_ble_port_JL.h"

#define LOG_TAG             "[tuya_app]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

extern void tuya_ble_app_disconnect(void);
extern u8 get_app_connect_type(void);
extern void set_ble_connect_type(u8 type);
extern void tuya_eq_data_deal(char *eq_info_data);
extern void tuya_set_music_volume(int volume);
extern void tuya_change_bt_name(char *name, u8 name_len);
extern u8 tuya_key_event_swith(u8 event);
extern void tuya_tws_bind_info_sync();
extern void set_triple_info(u8 *data);
extern void tuya_set_adv_disable();
extern void tuya_set_adv_enable();
extern void tuya_ble_adv_change(void);
extern void tuya_conn_state_set_and_indicate(uint8_t state);

extern void *tuya_ble_hdl;
extern struct ble_task_param ble_task;
extern tuya_ble_parameters_settings_t tuya_ble_current_para;

#define TUYA_TRIPLE_LENGTH      0x3E


#define TWS_FUNC_ID_TUYA_TRIPLE     TWS_FUNC_ID('T', 'R', 'I', 'P')
#define TWS_FUNC_ID_TUYA_AUTH_SYNC  TWS_FUNC_ID('T', 'U', 'A', 'U')

int tuya_earphone_state_set_page_scan_enable()
{
    return 0;
}

int tuya_earphone_state_get_connect_mac_addr()
{
    return 0;
}

int tuya_earphone_state_cancel_page_scan()
{
    return 0;
}

int tuya_earphone_state_tws_init(int paired)
{
    return 0;
}

int tuya_earphone_state_tws_connected(int first_pair, u8 *comm_addr)
{
    if (first_pair) {
        extern void ble_module_enable(u8 en);
        extern void bt_update_mac_addr(u8 * addr);
        extern void bt_make_ble_address(u8 * ble_address, u8 * edr_address);
        u8 tmp_ble_addr[6] = {0};
        bt_make_ble_address(tmp_ble_addr, comm_addr);
        app_ble_set_mac_addr(tuya_ble_hdl, (void *)tmp_ble_addr); //将ble广播地址改成公共地址
        bt_update_mac_addr(comm_addr);

        /*新的连接，公共地址改变了，要重新将新的地址广播出去*/
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("\nNew Connect Master!!!\n\n");
            tuya_ble_app_disconnect();
            tuya_set_adv_enable();
            void master_send_triple_info_to_slave();
            master_send_triple_info_to_slave();
        } else {
            printf("\nFirst Connect Slave!!!\n\n");
            /*从机ble关掉*/
            tuya_ble_app_disconnect();
            tuya_set_adv_disable();
        }
    }

    return 0;
}

int tuya_earphone_state_enter_soft_poweroff()
{
    extern void tuya_bt_ble_exit(void);
    tuya_bt_ble_exit();
    return 0;
}


enum {
    TRIPLE_NULL = 0,
    TRIPLE_FLASH = 1,
    TRIPLE_VM = TUYA_TRIPLE_LENGTH,
};
extern u16 get_triple_data(u8 *data);
extern int get_triple_info_result();
void bt_tws_tuya_triple_sync(void *data, u16 len, bool rx)
{
    //先是主机调用sibling 从机调用该函数 通过判断返回的三元组情况，是否给主机sibling
    u8 info_type = 3;
    u8 change_data[62];
    printf("slave rx: %d\n", rx);
    if (rx) {
        u8 slave_data[60];
        u16 slave_data_len;
        slave_data_len = get_triple_data(slave_data);
        info_type = ((u8 *)data)[0];

        printf("slave recv master triple data :[%d],info_type :[%d]\n", len, info_type);
        put_buf(data, len);
        int ret = get_triple_info_result();//获取从机的三元组情况
        put_buf(slave_data, slave_data_len);
        printf("slave self triple data:[%d],info_type:%d\n", slave_data_len, ret);
        switch (info_type) {
        case TRIPLE_NULL:
            if (ret == TRIPLE_FLASH) {
                change_data[0] = 1;
                memcpy(&change_data[1], slave_data, slave_data_len);
                tws_api_send_data_to_sibling(change_data, slave_data_len, TWS_FUNC_ID_TUYA_TRIPLE);
            }
            printf("slave triple data 0");
            break;
        case TRIPLE_FLASH:
            if (ret != TRIPLE_FLASH) {
                printf("slave write triple data 1");
                set_triple_info(&data[1]);
            }
            break;
        case TRIPLE_VM:
            printf("slave triple data 62");
            break;
        }
    }
}

void master_send_triple_info_to_slave()
{
    u8 data[62]  = {0};
    u16 data_len;
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        int ret = get_triple_info_result();
        printf("master read from [%d]\n", ret);
        data[0] = ret;
        data_len = get_triple_data(&data[1]);
        printf("master send triple data");
        put_buf(data, data_len);
        tws_api_send_data_to_sibling(data, data_len, TWS_FUNC_ID_TUYA_TRIPLE);
    }
}

int tuya_earphone_state_init()
{
    /* transport_spp_init(); */
    /* bt_spp_data_deal_handle_register(user_spp_data_handler); */

    return 0;
}



static void bt_tws_tuya_auth_info_received(void *data, u16 len, bool rx)
{
    log_info("bt_tws_tuya_auth_info_received, rx:%d", rx);
    if (rx) {
        tuya_tws_sync_info_t *recv_info = data;
        memcpy(tuya_ble_current_para.sys_settings.login_key, recv_info->login_key, LOGIN_KEY_LEN);
        memcpy(tuya_ble_current_para.sys_settings.device_virtual_id, recv_info->device_virtual_id, DEVICE_VIRTUAL_ID_LEN);
        tuya_ble_current_para.sys_settings.bound_flag = recv_info->bound_flag;
        os_taskq_post_msg(ble_task.ble_task_name, 1, BLE_TASK_PAIR_INFO_SYNC_SYS);
    }
}



REGISTER_TWS_FUNC_STUB(app_tuya_auth_stub) = {
    .func_id = TWS_FUNC_ID_TUYA_AUTH_SYNC,
    .func    = bt_tws_tuya_auth_info_received,
};
REGISTER_TWS_FUNC_STUB(app_tuya_triple_state) = {
    .func_id = TWS_FUNC_ID_TUYA_TRIPLE,
    .func    = bt_tws_tuya_triple_sync,
};

static int tuya_bt_status_event_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;

    printf("\ntuya_bt_status_event_handler event:0x%x\n", bt->event);
    switch (bt->event) {
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_THIRD_CONNECTED:
        tuya_conn_state_set_and_indicate(1);
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
    case BT_STATUS_THIRD_DISCONNECT:
        tuya_conn_state_set_and_indicate(0);
        break;
    case BT_STATUS_AVRCP_VOL_CHANGE:
        tuya_volume_indicate(bt->value * 16 / 127);
        break;
    case BT_STATUS_A2DP_MEDIA_START:
        tuya_play_status_indicate(bt_a2dp_get_status() == BT_MUSIC_STATUS_STARTING ? 1 : 0);
        break;
    case BT_STATUS_A2DP_MEDIA_STOP:
        tuya_play_status_indicate(0);
        break;
    default:
        break;
    }
    return 0;
}

int tuya_bt_tws_event_handler(int *msg)
{
    struct tws_event *bt = (struct tws_event *)msg;
    int role = bt->args[0];
    int phone_link_connection = bt->args[1];
    int reason = bt->args[2];

    printf("\ntuya_bt_tws_event_handler event:0x%x\n", bt->event);
    switch (bt->event) {
    case TWS_EVENT_CONNECTED:
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            //master enable
            printf("\nTws Connect Slave!!!\n\n");
            /*从机ble关掉*/
            tuya_ble_app_disconnect();
            tuya_set_adv_disable();
        } else {
            printf("master send\n");
            master_send_triple_info_to_slave();
        }
        tuya_tws_bind_info_sync();

        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        /*
         * 跟手机的链路LMP层已完全断开, 只有tws在连接状态才会收到此事件
         */
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        /*
         * TWS连接断开
         */
        if (app_var.goto_poweroff_flag) {
            break;
        }
        tuya_set_adv_enable();
        break;
    case TWS_EVENT_SYNC_FUN_CMD:
        break;
    case TWS_EVENT_ROLE_SWITCH:
        printf("tuya_role_switch");
        tuya_ble_adv_change();
        break;
    }

#if OTA_TWS_SAME_TIME_ENABLE
    tws_ota_app_event_deal(bt->event);
#endif
    return 0;
}

static int tuya_hci_event_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;

    printf("\ntuya_hci_event_handler event:0x%x\n", bt->event);
    switch (bt->event) {
    case HCI_EVENT_CONNECTION_COMPLETE:
        switch (bt->value) {
        case ERROR_CODE_PIN_OR_KEY_MISSING:
            break;
        }
        break;
    }

    return 0;
}

static int tuya_ota_event_handler(int *msg)
{
#if OTA_TWS_SAME_TIME_ENABLE
    bt_ota_event_handler(msg);
#else
    printf("todo tuya_ota_event_handler");
#endif
    return 0;
}

int tuya_app_msg_handler(int *msg)
{
    u8 comm_addr[6];

    int ret = false;  //默认不拦截消息
    printf("\ntuya_app_msg_handler event:0x%x\n", msg[0]);
    switch (msg[0]) {
    case APP_MSG_BT_OPEN_PAGE_SCAN:
        tuya_earphone_state_set_page_scan_enable();
        break;
    case APP_MSG_BT_CLOSE_PAGE_SCAN:
        tuya_earphone_state_cancel_page_scan();
        break;
    case APP_MSG_BT_ENTER_SNIFF:
        break;
    case APP_MSG_BT_EXIT_SNIFF:
        break;
    case APP_MSG_TWS_PAIRED:
        tuya_earphone_state_tws_init(1);
        break;
    case APP_MSG_TWS_UNPAIRED:
        tuya_earphone_state_tws_init(0);
        break;
    case APP_MSG_TWS_PAIR_SUSS:
        syscfg_read(CFG_TWS_COMMON_ADDR, comm_addr, 6);
        tuya_earphone_state_tws_connected(1, comm_addr);
        break;
    case APP_MSG_TWS_CONNECTED:
        tuya_earphone_state_tws_connected(0, comm_addr);
        break;
    case APP_MSG_POWER_OFF:
        tuya_earphone_state_enter_soft_poweroff();
        break;
    case APP_MSG_MUSIC_PP:
        printf("TUYA_MUSIC_PP\n");
        break;
    case APP_MSG_MUSIC_NEXT:
        printf("TUYA_MUSIC_NEXT\n");
        break;
    case APP_MSG_MUSIC_PREV:
        printf("TUYA_MUSIC_PREV\n");
        break;
    case APP_MSG_VOL_UP:
        printf("TUYA_MUSIC_VOL_UP\n");
        tuya_volume_indicate(app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
        break;
    case APP_MSG_VOL_DOWN:
        printf("TUYA_MUSIC_VOL_DOWN\n");
        tuya_volume_indicate(app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
        break;
    }
    return ret;
}

int tuya_key_msg_handler(int *msg)
{
    g_printf("tuya key msg receive:0x%x\n", msg[0]);
    int key_msg = 0;
    tuya_earphone_key_remap(&key_msg, msg);
    printf("key_msg:%d\n", key_msg);
#if TCFG_USER_TWS_ENABLE
    bt_tws_key_msg_sync(key_msg);
#else
    app_send_message(key_msg, 0);
#endif
    return true;  //中断消息分发
}
/*****************tuya demo api*******************/

static void bt_tws_tuya_sync_info_received(void *_data, u16 len, bool rx)
{
    if (len < sizeof(struct TUYA_SYNC_INFO)) {
        printf("tws receive len error!");
        return;
    }
    static u16 find_device_timer = 0;
    if (rx) {
        struct TUYA_SYNC_INFO *tuya_sync_info = (struct TUYA_SYNC_INFO *)_data;
        /* put_buf(tuya_sync_info, sizeof(tuya_sync_info)); */
        printf("this is tuya received!\n");
        if (tuya_sync_info->tuya_eq_flag == 1) {
            printf("set remote eq_info!\n");
            static char temp_eq_buf[10];
            memset(temp_eq_buf, 0, sizeof(temp_eq_buf));
            if (!memcmp(temp_eq_buf, tuya_sync_info->eq_info, 10)) {
                tuya_eq_mode_post(EQ_MODE_NORMAL);
            } else {
                memcpy(temp_eq_buf, tuya_sync_info->eq_info, sizeof(temp_eq_buf));
                tuya_eq_info_post(temp_eq_buf);
            }
            tuya_vm_info_post(tuya_sync_info->eq_info, TUYA_EQ_INFO_SYNC_VM, 11);
        }
        printf(">> volume:%d\n", tuya_sync_info->volume);
        if (tuya_sync_info->volume_flag == 1) {
            tuya_set_music_volume(tuya_sync_info->volume);
        }
        if (tuya_sync_info->key_change_flag == 1 || tuya_sync_info->key_reset == 1) {
            if (tuya_sync_info->key_change_flag == 1) {
                tuya_sync_key_info(tuya_sync_info);
            }
            if (tuya_sync_info->key_reset == 1) {
                tuya_reset_key_info();
            }
            printf("%d %d %d %d %d %d\n", key_table_l[0], key_table_l[2], key_table_l[4], key_table_r[0], key_table_r[2], key_table_r[4]);
            u8 key_value_record[2][6] = {0};
            tuya_update_vm_key_info(key_value_record);
            tuya_vm_info_post((char *)key_value_record, TUYA_KEY_SYNC_VM, sizeof(key_value_record));
        }
        if (tuya_sync_info->find_device == 1) {
            find_device_timer = sys_timer_add(NULL, find_device, 1000);
        } else if (tuya_sync_info->find_device == 0) {
            sys_timer_del(find_device_timer);
        }
        if (tuya_sync_info->device_conn_flag == 1) {
            tuya_tone_post(get_tone_files()->bt_connect);
        }
        if (tuya_sync_info->phone_conn_flag == 1) {
            tuya_tone_post(get_tone_files()->bt_connect);
        }
        if (tuya_sync_info->device_disconn_flag == 1) {
            tuya_tone_post(get_tone_files()->bt_disconnect);
        }
        if (tuya_sync_info->phone_disconn_flag == 1) {
            tuya_tone_post(get_tone_files()->bt_disconnect);
        }
        if (tuya_sync_info->tuya_bt_name_flag == 1) {
            printf("sync bt_name:%s\n", tuya_sync_info->bt_name);
            tuya_change_bt_name(tuya_sync_info->bt_name, LOCAL_NAME_LEN);
            tuya_vm_info_post(tuya_sync_info->bt_name, TUYA_BT_NAME_SYNC_VM, LOCAL_NAME_LEN);
        }
    }
}
REGISTER_TWS_FUNC_STUB(app_tuya_state_stub) = {
    .func_id = TWS_FUNC_ID_TUYA_STATE,
    .func    = bt_tws_tuya_sync_info_received,
};

/*****************tuya demo api*******************/

APP_MSG_HANDLER(tuya_bthci_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_HCI,
    .handler    = tuya_hci_event_handler,
};

APP_MSG_HANDLER(tuya_btstack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = tuya_bt_status_event_handler,
};

APP_MSG_HANDLER(tuya_tws_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = tuya_bt_tws_event_handler,
};

APP_MSG_HANDLER(tuya_ota_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_OTA,
    .handler    = tuya_ota_event_handler,
};

APP_MSG_PROB_HANDLER(tuya_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = tuya_app_msg_handler,
};

APP_MSG_PROB_HANDLER(tuya_key_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_KEY,
    .handler    = tuya_key_msg_handler,
};
#endif

