#include "app_msg.h"
#include "bt_tws.h"
#include "tws_dual_conn.h"
#include "classic/tws_api.h"
#include "app_main.h"
#include "sbox_core_config.h"
#include "sbox_user_app.h"
#include "user_video_ctr.h"
#include "sbox_connect_emitter.h"


static u8 addr_change_flag = 0;
static u8 emitter_info[6];
static u8 real_phone_addr[6];

#define     CFG_LAST_PHONE_ADDR          7

extern void delete_link_key(bd_addr_t bd_addr, u8 id);
extern int add_device_2_page_list(u8 *mac_addr, u32 timeout, u8 dev_type);
extern void dual_conn_page_device();
void sync_change_lmp_addr_deal(u8 *data, u16 len);
void sync_addr_change_flag_deal(u8 *data, u16 len);

void emitter_last_phone_mac_init(void)
{
    u8 len = syscfg_read(CFG_LAST_PHONE_ADDR, real_phone_addr, 6);
    if (len != 6) {
        memset(real_phone_addr, 2, 6);
    } else {
        printf("real_phone_addr");
        put_buf(real_phone_addr, 6);
    }
}

void save_real_phone_addr_to_VM(doid)
{
    syscfg_write(CFG_LAST_PHONE_ADDR, real_phone_addr, 6);
}



#define TWS_FUNC_ID_SBOX_SYNC    TWS_FUNC_ID('S', 'B', 'O', 'X')

void earphone_sync_sbox_setting(u8 type, u8 *data, u8 len)
{
    u8 buff[1 + len];
    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
        buff[0] = type;
        if (len) {
            memcpy(buff + 1, data, len);
        }
        tws_api_send_data_to_sibling(buff, 1 + len, TWS_FUNC_ID_SBOX_SYNC);
    }
}

static void remote_emitter_addr(u8 *data, u16 len)
{
    printf("%s[line:%d]\n", __func__, __LINE__);
    memcpy(emitter_info, data, len);
#if LOCAL_PALY_KAIGAI_RECON
    save_emitter_addr2vm(data, len);
#endif  /* LOCAL_PALY_KAIGAI_RECON */
}


u8 *get_remote_emitter_addr(void)
{
    printf("%s[line:%d]\n", __func__, __LINE__);
    // put_buf(emitter_info, 6);
#if LOCAL_PALY_KAIGAI_RECON
    int ret = syscfg_read(CFG_USER_EMITTER_ADDR, emitter_info, 6);
    if (ret != 6) {
        printf("read emitter addr err!!!\n");
    }
#endif  /* LOCAL_PALY_KAIGAI_RECON */
    return emitter_info;
}

void clear_last_info(void)
{
    u8 mac_addr[6];
    printf("%s[line:%d]\n", __func__, __LINE__);
    u8 flag = bt_restore_remote_device_info_opt((bd_addr_t *)mac_addr, 1, get_remote_dev_info_index());
    if (flag) {
        printf("%s[line:%d, last edr addr]\n", __func__, __LINE__);
        put_buf(mac_addr, 6);
        if (!memcmp(get_remote_emitter_addr(), mac_addr, sizeof(mac_addr))) {
            bt_cmd_prepare(USER_CTRL_DEL_LAST_REMOTE_INFO, 0, NULL);
            // delete_last_device_from_vm();
            if (bt_get_total_connect_dev() == 1) {
                bt_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            }
        }
    }
    delete_link_key(get_remote_emitter_addr(), 1);
}

static void earphone_sync_sbox_setting_receive(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        printf("%s[line:%d]\n", __func__, __LINE__);
        put_buf(data, len);
        switch (data[0]) {
        case SYNC_CMD_EMITTER_ADDR:
            remote_emitter_addr(&data[1], len - 1);
            break;
        case SYNC_CMD_CHANGE_LMP_ADDR:
            sync_change_lmp_addr_deal(&data[1], len - 1);
            break;
        case SYNC_CMD_ADDR_CHANGE_FLAG:
            sync_addr_change_flag_deal(&data[1], len - 1);
            break;
        case SYNC_CMD_CLEAN_EMITTER_LINKKEY:
            clear_last_info();
            break;
        case SYNC_CMD_REMOTE_TYPE:
#if TCFG_USER_EDR_ENABLE && (!TCFG_BT_DUAL_CONN_ENABLE)
            usr_set_remote_type(data[1]);
#endif
            break;
        // case SYNC_CMD_REMOTE_ADDR:
        //     set_save_remote_addr(&data[1]);
        //     break;
        default:
            break;
        }


    }
}

REGISTER_TWS_FUNC_STUB(sync_sbox_setting) = {
    .func_id = TWS_FUNC_ID_SBOX_SYNC,
    .func    = earphone_sync_sbox_setting_receive,
};



void sync_emitter_addr(void)
{
    printf("%s[line:%d]\n", __func__, __LINE__);
    earphone_sync_sbox_setting(SYNC_CMD_EMITTER_ADDR, emitter_info, sizeof(custom_edr_info));
}


void custom_ctrle_emitter_info(u8 *data, u16 len)
{
    printf("%s[line:%d, len:%d]\n", __func__, __LINE__, len);
    memcpy(emitter_info, data, len);
#if LOCAL_PALY_KAIGAI_RECON
    save_emitter_addr2vm(data, len);
#endif  /* LOCAL_PALY_KAIGAI_RECON */
    // put_buf(emitter_info, 6);
    sync_emitter_addr();
    // sync_clear_last_info();
    // clear_last_info();
}







void sync_change_lmp_addr_deal(u8 *data, u16 len)
{
    printf("%s[line:%d]\n", __func__, __LINE__);
    put_buf(data, len);

    // lmp_hci_write_local_address(data);
    memcpy(emitter_info, data, len);

}
void sync_change_lmp_addr(void)
{
    earphone_sync_sbox_setting(SYNC_CMD_CHANGE_LMP_ADDR, emitter_info, sizeof(emitter_info));
}

void sync_addr_change_flag_deal(u8 *data, u16 len)
{
    addr_change_flag = data[0];
    printf("%s[line:%d, addr_change_flag:%d]\n", __func__, __LINE__, addr_change_flag);
    put_buf(data, len);


}
void sync_addr_change_flag(void)
{
    earphone_sync_sbox_setting(SYNC_CMD_ADDR_CHANGE_FLAG, &addr_change_flag, 1);
}

void sync_clear_last_info(void)
{
    earphone_sync_sbox_setting(SYNC_CMD_CLEAN_EMITTER_LINKKEY, NULL, 0);
}



void custom_ctrle_edr_conn(u8 cmd)
{
    u8 phone_mac[6];
    u8 test_mac[10][6];
    memset(test_mac, 0, 10 * 6);
    bt_get_current_poweron_memory_search_index(phone_mac);
    printf("%s[line:%d, last edr addr]\n", __func__, __LINE__);
    put_buf(phone_mac, 6);
    g_printf("%s[line:%d, cmd:%d]\n", __func__, __LINE__, cmd);
    switch (cmd) {
    case 1:         /* 断开手机连接 */
        delete_link_key(get_remote_emitter_addr(), 1);
        if (bt_get_total_connect_dev()) {
            bt_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            bt_get_current_poweron_memory_search_index(phone_mac);
            put_buf(phone_mac, 6);
            printf("%s[line:%d, bt_get_total_connect_dev 1 \n", __func__, __LINE__);
        } else {
            clr_device_in_page_list();
            write_scan_conn_enable(1, 1);
            printf("%s[line:%d,  bt_get_total_connect_dev 0 \n", __func__, __LINE__);
        }
        custom_sync_wait_emitter_conn();
        break;
    case 2:
        clear_last_info();
        sync_clear_last_info();

        puts("coonet to mac ");
        u8 flag = bt_restore_remote_device_info_opt(test_mac, 10, 0);
        put_buf((u8 *)test_mac, 10 * 6);
        clr_device_in_page_list();
        if (flag) {
            if (!memcmp(get_remote_emitter_addr(), test_mac[0], 6)) {
                printf("mac error\n");
                return;
            }
            memcpy(phone_mac, test_mac[0], 6);
            add_device_2_page_list(phone_mac, TCFG_BT_POWERON_PAGE_TIME * 1000, 0);
        }
        dual_conn_page_device();
    default:
        break;
    }
}


/*
 * tws事件状态处理函数
 */
static int emitter_tws_connction_status_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        sync_emitter_addr();
        break;
    default:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(emitter_tws_msg_handler) = {
    .owner      = APP_MODE_BT,
    .from       = MSG_FROM_TWS,
    .handler    = emitter_tws_connction_status_event_handler,
};



static int emitter_connction_status_event_handler(struct bt_event *bt)
{

    switch (bt->event) {
    case BT_STATUS_INIT_OK:

        break;
    case BT_STATUS_SECOND_CONNECTED:
        bt_clear_current_poweron_memory_search_index(0);
    case BT_STATUS_FIRST_CONNECTED:
        printf("BT_STATUS_CONNECTED\n");
        // int compare_is_phone_mac;
        // u8 is_phone_connet=0;
        // put_buf(bt->args,6);
        // put_buf(real_phone_addr,6);
        // compare_is_phone_mac=memcmp(emitter_info,bt->args,6);

        // printf("compare_is_phone_mac %d",compare_is_phone_mac);
        // if(compare_is_phone_mac){
        //     printf("bt_get_total_connect_dev %d",bt_get_total_connect_dev());
        //     if(bt_get_total_connect_dev()){
        //         is_phone_connet=1;
        //         //recourd phone mac
        //         memcpy(real_phone_addr, bt->args, 6);
        //         sys_timeout_add(NULL,save_real_phone_addr_to_VM,100);
        //     }
        // }
        break;
    }
    return 0;
}

int emitter_btstack_event_handler(int *event)
{
    emitter_connction_status_event_handler((struct bt_event *)event);
    return 0;
}
APP_MSG_HANDLER(emitter_stack_msg_handler) = {
    .owner      = APP_MODE_BT,
    .from       = MSG_FROM_BT_STACK,
    .handler    = emitter_btstack_event_handler,
};
