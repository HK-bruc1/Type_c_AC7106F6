#include "sdk_config.h"
#include "app_msg.h"
//#include "earphone.h"
#include "bt_tws.h"
#include "app_main.h"
#include "battery_manager.h"
#include "app_power_manage.h"
#include "btstack/avctp_user.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "app_protocol_api.h"
#include "clock_manager/clock_manager.h"
#include "app_ble_spp_api.h"
#include "asm/charge.h"
#include "mma_config.h"
#include "mma_platform_api.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & MMA_EN)

#if 1
#define log_info(x, ...)       printf("[MMA PROTOCOL]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

//不想搞头文件，直接在这里定义
extern void *xm_app_ble_hdl;
extern u32 unactice_device_cmd_prepare(USER_CMD_TYPE cmd, u16 param_len, u8 *param);
extern void tws_dual_conn_close();
extern void bt_set_need_keep_scan(u8 en);
extern void set_wait_poweroff_time(u16 time);
// extern u8 *get_mac_memory_by_index(u8 index);

//小米库里面的函数
/* extern bool xm_get_pair_state(void); */
/* extern void xm_set_pair_state(u8 state); */
//extern void xm_all_info_recover(u8 name_reset);
//extern void xm_sibling_sync_info();
//extern int  xm_bt_ble_disconnect(void *priv);
//extern void xm_bt_ble_adv_enable(u8 enable);
//extern void xm_set_adv_interval(u16 value);
//extern void xm_sibling_data_send(u8 type, u8 *data, u16 len);
//extern void xm_battery_update(void);

//小米广播包数据定义=================================================end====

#define XM_SIBLING_SYNC_TYPE_ADV_MAC_STATE    0x05

#define ADV_INIT_PAIR_IN_PAIR_MODE      0 //进入配对状态广播初始配对广播

struct xm_protocol_t {
    u8 poweron_reconn;
    u8 close_icon;
};
static struct xm_protocol_t xm_protocol = {0};

#define __this          (&xm_protocol)

#define INIT_PAIR_ADV_INTERVAL      150
#define SUBS_PAIR_ADV_INTERVAL      352

void xm_set_pair_mode_by_user(u8 disconn_ble)
{
    if (disconn_ble) {
        xm_bt_ble_disconnect(NULL);
    }
    xm_set_adv_interval(INIT_PAIR_ADV_INTERVAL);
    xm_set_pair_state(0);
    xm_bt_ble_adv_enable(0);
    xm_bt_ble_adv_enable(1);
}


static void xm_ctl_bt_enter_pair_mode()
{
    log_info("%s", __func__);
    tws_dual_conn_close();
    lmp_hci_write_scan_enable((1 << 1) | 1);
    bt_set_need_keep_scan(1);
}

static void xm_before_pair_new_device_in_task(u8 *seeker_addr)
{
    u8 remote_edr_addr[6];
    u8 *tmp_addr = NULL;

    for (int i = 0; i < 6; i ++) {
        remote_edr_addr[i] = seeker_addr[5 - i];
    }

    free(seeker_addr);

#if TCFG_BT_DUAL_CONN_ENABLE
    void *devices[2] = {NULL, NULL};
    int num = btstack_get_conn_devices(devices, 2);
    u8 active_addr[6];

    for (int i = 0; i < num; i++) {
        if (!memcmp(btstack_get_device_mac_addr(devices[i]), remote_edr_addr, 6)) { //地址相同
            log_info("Retroactive Pair\n");
            return;
        }
    }

    if (num == 2) { //连接了两个手机，断开其中一个
        if (esco_player_get_btaddr(active_addr) || a2dp_player_get_btaddr(active_addr)) { //断开非当前播歌通话的设备
            tmp_addr = btstack_get_other_dev_addr(active_addr);
            bt_cmd_prepare_for_addr(tmp_addr, USER_CTRL_DISCONNECTION_HCI, 0, NULL);
        } else { //断开非活跃的设备
            unactice_device_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
        }
    } else { //开可连接
        int msg[] = {(int)xm_ctl_bt_enter_pair_mode, 0};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
    }

#else
    if (bt_get_curr_channel_state()) {
        tmp_addr = get_cur_connect_phone_mac_addr();
        if (memcmp(tmp_addr, remote_edr_addr, 6) != 0) {
            log_info("Subsequent pair\n");
            log_info("USER_CTRL_DISCONNECTION_HCI send\n");
            bt_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
        } else {
            log_info("Retroactive Pair\n");
        }
    }
#endif
    log_info("func:%s, line:%d", __FUNCTION__, __LINE__);
}


void xm_before_pair_new_device(u8 *seeker_addr)
{
    int msg[3];
    u8 *tmp_addr = malloc(6);
    memcpy(tmp_addr, seeker_addr, 6);
    msg[0] = (int)xm_before_pair_new_device_in_task;
    msg[1] = 1;
    msg[2] = (int)tmp_addr;
    int err = os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
    if (err) {
        printf("%s post fail\n", __func__);
    }
}


#if TCFG_USER_TWS_ENABLE
int xm_protocol_tws_send_to_sibling(u16 opcode, u8 *data, u16 len)
{
    return tws_api_send_data_to_sibling(data, len, 0x23080315);
}

static void __tws_rx_from_sibling(u8 *data, int len)
{
    //xm_tws_data_deal(data, len);

    switch (data[0]) {
    case XM_SIBLING_SYNC_TYPE_ADV_MAC_STATE:
        xm_set_adv_interval(data[1] ? SUBS_PAIR_ADV_INTERVAL : INIT_PAIR_ADV_INTERVAL);
        xm_set_pair_state(data[1]);
        app_ble_set_mac_addr(xm_app_ble_hdl, (void *)&data[2]);
        xm_bt_ble_adv_enable(0);
        xm_bt_ble_adv_enable(1);
        break;
    }
    free(data);
}

static void xm_protocol_rx_from_sibling(void *_data, u16 len, bool rx)
{
    int err = 0;
    if (rx) {
        log_info(">>>%s \n", __func__);
        log_info("len :%d\n", len);
        log_info_hexdump(_data, len);

        u8 *rx_data = malloc(len);
        if (!rx_data) {
            return;
        }

        memcpy(rx_data, _data, len);

        int msg[4];
        msg[0] = (int)__tws_rx_from_sibling;
        msg[1] = 2;
        msg[2] = (int)rx_data;
        msg[3] = (int)len;
        err = os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
        if (err) {
            log_info("tws rx post fail\n");
        }
    }
}

//发送给对耳
REGISTER_TWS_FUNC_STUB(app_vol_sync_stub) = {
    .func_id = 0x23080315,
    .func    = xm_protocol_rx_from_sibling,
};
#endif

int xm_message_deal_handler(int id, int opcode, u8 *data, u32 len)
{
    int ret = 0;
    switch (opcode) {
    case APP_PROTOCOL_LIB_TWS_DATA_SYNC:
        ret = xm_protocol_tws_send_to_sibling(opcode, data, len);
        break;
    case APP_PROTOCOL_MMA_SAVE_INFO:
        log_info("APP_PROTOCOL_MMA_SAVE_");
        log_info_hexdump(data, len);
        ret = syscfg_write(VM_XM_INFO, data, len);
        break;
    case APP_PROTOCOL_MMA_READ_INFO:
        log_info("APP_PROTOCOL_mMA_READ_");
        ret = syscfg_read(VM_XM_INFO, data, len);
        log_info_hexdump(data, len);
        break;
    case APP_PROTOCOL_MMA_SAVE_ADV_COUNTER:
        syscfg_write(VM_XM_INFO2, data, len);
        break;
    case APP_PROTOCOL_MMA_READ_ADV_COUNTER:
        ret = syscfg_read(VM_XM_INFO2, data, len);
        break;
    case APP_PROTOCOL_MMA_SAVE_ACCOUNT_KEY:
        ret = syscfg_write(VM_XM_INFO3, data, len);
        break;
    case APP_PROTOCOL_MMA_READ_ACCOUNT_KEY:
        ret = syscfg_read(VM_XM_INFO3, data, len);
        break;
    }
    return ret;
}

//关机广播收仓
void xm_need_adv_close_icon_set(u8 en)
{
    log_info("func:%s, en:%d", __FUNCTION__, en);
    __this->close_icon = en;
}

void xm_poweroff_adv_close_icon()
{
    log_info("func:%s, line:%d", __FUNCTION__, __LINE__);
    if (__this->close_icon) {
        set_wait_poweroff_time(2000); //广播800ms
        xm_set_adv_interval(48); //连接上手机间隔改为30ms
    }
}

static int xm_app_msg_handler(int *msg)
{
    switch (msg[0]) {
    case APP_MSG_BT_IN_PAIRING_MODE:
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
#endif
        if (bt_get_total_connect_dev() == 0) {
            xm_set_adv_interval(INIT_PAIR_ADV_INTERVAL); //连接上手机间隔改为93.75ms
            if (__this->poweron_reconn || ADV_INIT_PAIR_IN_PAIR_MODE) { //开机回连超时后，广播初始配对广播
                __this->poweron_reconn = 0;
                xm_set_pair_mode_by_user(0);
            }
        }
        break;
    }
    return 0;
}
APP_MSG_HANDLER(xm_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = xm_app_msg_handler,
};

static int xm_bt_status_event_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;
    int ret;
    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        //要更新count再开广播才能弹窗
        if (btstack_get_num_of_remote_device_recorded()) {
            __this->poweron_reconn = 1;
        }
        break;
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_THIRD_CONNECTED:
        __this->poweron_reconn = 0;
        xm_set_adv_interval(SUBS_PAIR_ADV_INTERVAL); //连接上手机间隔改为220ms，防止fast pair validator测试fail
        xm_set_pair_state(1);
        xm_bt_ble_adv_enable(0);
        xm_bt_ble_adv_enable(1);
        break;
    case BT_STATUS_PHONE_INCOME:
    case BT_STATUS_PHONE_OUT:
    case BT_STATUS_PHONE_ACTIVE:
        xm_bt_ble_adv_enable(0);
        break;
    case BT_STATUS_PHONE_HANGUP:
        xm_bt_ble_adv_enable(1);
        break;
    case BT_STATUS_CONN_HFP_CH:
        log_info("=====BT_STATUS_CONN_HFP_CH=====");
        xm_hfp_connect_handler();
        break;
    case BT_STATUS_DISCON_HFP_CH:
        log_info("=====BT_STATUS_DISCON_HFP_CH=====");
        xm_hfp_disconnect_handler();
        break;
    }
    return 0;
}
APP_MSG_HANDLER(xm_stack_msg_handler) = {
    .owner      = 0xFF,
    .from       = MSG_FROM_BT_STACK,
    .handler    = xm_bt_status_event_handler,
};

int xm_hci_event_handler(int *event)
{
    struct bt_event *bt = (struct bt_event *)event;

    switch (bt->event) {
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        switch (bt->value) {
        case ERROR_CODE_PIN_OR_KEY_MISSING:
            log_info("GFP linkkey missing");
            if (!btstack_get_num_of_remote_device_recorded()) { //没有配对记录的时候，广播初始配对广播
                //xm_all_info_recover(0);
                xm_set_adv_interval(INIT_PAIR_ADV_INTERVAL); //连接上手机间隔改为93.75ms
                xm_set_pair_mode_by_user(0);
            }
            break;
        }
        break;
    }
    return 0;
}
APP_MSG_HANDLER(xm_hci_msg_handler) = {
    .owner      = 0xFF,
    .from       = MSG_FROM_BT_HCI,
    .handler    = xm_hci_event_handler,
};

static int xm_tws_msg_handler(int *_msg)
{
    struct tws_event *evt = (struct tws_event *)_msg;
    int role = evt->args[0];
    int reason = evt->args[2];

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        if (role == TWS_ROLE_MASTER) {
            //xm_sibling_sync_info();
        } else {
            xm_bt_ble_disconnect(NULL);
            xm_bt_ble_adv_enable(0);
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        //对耳主从切换时，将旧主机的蓝牙地址和pair_state同步给新主机
        xm_bt_ble_disconnect(NULL);
        if (role == TWS_ROLE_SLAVE) {
            xm_bt_ble_adv_enable(0);
            u8 buf[7];
            buf[0] = xm_get_pair_state();
            memcpy(buf + 1, app_ble_adv_addr_get(xm_app_ble_hdl), 6);
            //xm_sibling_data_send(XM_SIBLING_SYNC_TYPE_ADV_MAC_STATE, buf, sizeof(buf));
            log_info("xm_send_pair_state:%d, ble_addr:", buf[0]);
            log_info_hexdump(buf + 1, 6);
        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        if (reason & TWS_DETACH_BY_POWEROFF) {
            log_info("%s, role:%d, poweroff:%d", __func__, role, app_var.goto_poweroff_flag);
            if (role == TWS_ROLE_MASTER && app_var.goto_poweroff_flag) {
                xm_poweroff_adv_close_icon();
            }
        }
        break;
    }
    return 0;
}
APP_MSG_HANDLER(xm_tws_msg_entry) = {
    .owner = 0xff,
    .from = MSG_FROM_TWS,
    .handler = xm_tws_msg_handler,
};

static int xm_app_power_event_handler(int *msg)
{
    switch (msg[0]) {
    case POWER_EVENT_SYNC_TWS_VBAT_LEVEL:
        log_info("update xm bat");
        if (get_tws_sibling_bat_persent() != 0xFF) {
        }
        //xm_battery_update();
        break;
    case POWER_EVENT_POWER_CHANGE:
        log_info("update xm bat");
        //xm_battery_update();
        break;
    }
    return 0;
}
APP_MSG_HANDLER(xm_bat_level_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BATTERY,
    .handler    = xm_app_power_event_handler,
};

#endif

