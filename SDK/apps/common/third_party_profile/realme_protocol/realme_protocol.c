#include "system/includes.h"
#include "sdk_config.h"
#include "app_msg.h"
// #include "earphone.h"
#include "bt_tws.h"
#include "app_main.h"
#include "battery_manager.h"
#include "app_power_manage.h"
#include "btstack/avctp_user.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "clock_manager/clock_manager.h"
#include "btstack/third_party/app_protocol_event.h"
#include "multi_protocol_main.h"
#include "app_ble_spp_api.h"
#include "asm/charge.h"
#include "realme_platform_api.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)

extern bool check_tws_master_role();

u8 realme_hfp_cmd_support = 1;

bool realme_is_tws_master(void)
{
    return check_tws_master_role();
}

int realme_hci_event_handler(int *event)
{
    struct bt_event *bt = (struct bt_event *)event;
    switch (bt->event) {
    case HCI_EVENT_CONNECTION_COMPLETE:
        switch (bt->value) {
        case ERROR_CODE_SUCCESS :
            printf("HCI_EVENT_CONNECTION_COMPLETE ERROR_CODE_SUCCESS\n");
            put_buf(bt->args, 6);
            realme_remote_connect_complete_callback(bt->args);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return 0;
}

APP_MSG_HANDLER(realme_hci_msg_handler) = {
    .owner      = 0xFF,
    .from       = MSG_FROM_BT_HCI,
    .handler    = realme_hci_event_handler,
};

static int realme_bt_status_event_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;

    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        break;
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_THIRD_CONNECTED:
        break;
    case BT_STATUS_PHONE_INCOME:
    case BT_STATUS_PHONE_OUT:
    case BT_STATUS_PHONE_ACTIVE:
        break;
    case BT_STATUS_PHONE_HANGUP:
        break;
    }
    return 0;
}
APP_MSG_HANDLER(realme_stack_msg_handler) = {
    .owner      = 0xFF,
    .from       = MSG_FROM_BT_STACK,
    .handler    = realme_bt_status_event_handler,
};

#if TCFG_USER_TWS_ENABLE

static int realme_tws_msg_handler(int *_msg)
{
    struct tws_event *evt = (struct tws_event *)_msg;
    int role = evt->args[0];
    int reason = evt->args[2];

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        realme_update_tws_state_to_lib(USER_NOTIFY_STATE_TWS_CONNECT);
        if (role == TWS_ROLE_MASTER) {
            realme_tws_sync_state_send();
            realme_ble_adv_enable(1);
        } else {
            realme_ble_adv_enable(0);
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        //对耳主从切换时，将旧主机的蓝牙地址和pair_state同步给新主机
        /* gfps_disconnect(NULL); */
        /* if (role == TWS_ROLE_SLAVE) { */
        /* gfps_sibling_data_send(GFPS_SIBLING_SYNC_TYPE_ADV_MAC_STATE, buf, sizeof(buf)); */
        /* printf("gfps_send_pair_state:%d, ble_addr:", buf[0]); */
        /* put_buf(buf + 1, 6); */
        /* } */
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        realme_update_tws_state_to_lib(USER_NOTIFY_STATE_TWS_DISCONNECT);
        break;
    }
    return 0;
}

APP_MSG_HANDLER(realme_tws_msg_entry) = {
    .owner = 0xff,
    .from = MSG_FROM_TWS,
    .handler = realme_tws_msg_handler,
};

#endif

#define REALME_PRODUCT_TEST 0

static uint8_t realme_codec = 1;

struct __realme_info *user_realme_info;

static void realme_info_init(void)
{
    user_realme_info = malloc(sizeof(struct __realme_info));
    memset(user_realme_info, 0, sizeof(struct __realme_info));

    // initial value
    user_realme_info->min_bass_engine = -5;
    user_realme_info->max_bass_engine = 5;
    user_realme_info->cur_bass_engine = 1;

    realme_info_register(user_realme_info);
}

int realme_hfp_oesf_str_get(char *str)
{
    printf("realme_hfp_oesf_str_get");
    strcpy(str, "AT+OESF=6,47\r\0");
    return 0;
}

int realme_hfp_battery_get(u8 *left, u8 *right, u8 *box)
{
    printf("realme_hfp_battery_get");
#if TCFG_USER_TWS_ENABLE
    *left  = realme_platform_get_battery_left();
    *right = realme_platform_get_battery_right();
    *box   = realme_platform_get_battery_box();
    return 0;
#else
    return -1;
#endif
}

static int realme_product_test_recieve_cbk(uint8_t *buf, uint16_t len)
{
    printf("realme_product_test_recieve_cbk, len:%d", len);
    put_buf(buf, len);

    // 解析发现是产测命令就返回0，其他返回1
    return 1;
}

static u8 test_text1[] = "{\"cmd\": \"wakeup\",\"earside\": \"R\",\"backward\": 5, \"forward\": 1,\"volume_up\": 3,\"volume_down\": 4,\"play\": 7, \"stop\": 3, \"anc_on\": 9, \"anc_tt\": 10, \"take_photo\": 21, \"favorite_music\": 5 }\0";
static u8 test_text[] = "{\"cmd\":\"fectory_reset\",\"count\":3}\0";

void test_send_buried_point(void *priv)
{
    printf(">>>>>>>>>>>>> ttttt test\n");
    realme_event_notify(NULL,
                        OPPO_EVENT_CODE_DEVICE_BURIED_POINT,
                        (u8 *)test_text,
                        strlen((const char *)test_text));
}

int realme_protocol_init(void)
{
    realme_ota_breakpoint_init();
    realme_product_test_set(REALME_PRODUCT_TEST, realme_product_test_recieve_cbk);
    realme_message_callback_register(realme_message_callback_handler);
    realme_is_tws_master_callback_register(realme_is_tws_master);
    realme_info_init();
    realme_all_init(0xB, 0x0);
    realme_ble_adv_enable(1);
    return 0;
}

int realme_protocol_exit(void)
{
    realme_all_exit();
    return 0;
}

#endif

