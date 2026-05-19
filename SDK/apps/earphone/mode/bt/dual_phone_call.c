#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "btstack/a2dp_media_codec.h"
#include "app_config.h"
#include "app_msg.h"
#include "a2dp_player.h"
#include "bt_tws.h"
#include "earphone.h"

/**
 * @brief 处理来电信息抢占逻辑
 *
 * @param device_a:当前蓝牙事件的设备
 * @param device_b:另一台蓝牙设备
 * @param status_b:另一台蓝牙设备的状态，详细见枚举STATUS_FOR_USER
 *
 * @return 0
 */
static int bt_dual_phone_call_phone_income_handler(void *device_a, void *device_b, int status_b)
{
    if ((status_b == BT_CALL_INCOMING) &&
        (btstack_get_call_esco_status(device_b) == BT_ESCO_STATUS_OPEN)) {
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        // 后来电的优先级高于先来电
        puts("disconn_sco-b1\n");
        btstack_device_control(device_b, USER_CTRL_DISCONN_SCO);
#else
        // 先来电的优先级高于后来电
        puts("disconn_sco-a1\n");
        btstack_device_control(device_a, USER_CTRL_DISCONN_SCO);
#endif
    } else if (status_b == BT_CALL_OUTGOING) {
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        // 后来电的优先级高于先去电
        puts("disconn_sco-b2\n");
        btstack_device_control(device_b, USER_CTRL_DISCONN_SCO);
#else
        // 去电的优先级高于来电
        puts("disconn_sco-a2\n");
        btstack_device_control(device_a, USER_CTRL_DISCONN_SCO);
#endif
    } else if (status_b == BT_CALL_ACTIVE &&
               btstack_get_call_esco_status(device_b) == BT_ESCO_STATUS_OPEN) {
        // 通话的优先级高于来电
        puts("disconn_sco-a3\n");
        btstack_device_control(device_a, USER_CTRL_DISCONN_SCO);
    }
    return 0;
}

/**
 * @brief 处理去电信息抢占逻辑
 *
 * @param device_a:当前蓝牙事件的设备
 * @param device_b:另一台蓝牙设备
 * @param status_b:另一台蓝牙设备的状态，详细见枚举STATUS_FOR_USER
 *
 * @return 0
 */
static int bt_dual_phone_call_phone_out_handler(void *device_a, void *device_b, int status_b)
{
    if ((status_b == BT_CALL_ACTIVE) &&
        (btstack_get_call_esco_status(device_b) == BT_ESCO_STATUS_OPEN)) {
        // 通话的优先级高于去电
        puts("disconn_sco-a\n");
        btstack_device_control(device_a, USER_CTRL_DISCONN_SCO);
        return 0;
    }
    if (status_b == BT_CALL_OUTGOING) {
        // 后去电的优先级高于先去电
        puts("disconn_sco-b1\n");
        btstack_device_control(device_b, USER_CTRL_DISCONN_SCO);
    } else if (status_b == BT_CALL_INCOMING) {
        // 去电的优先级高于来电
        puts("disconn_sco-b2\n");
        btstack_device_control(device_b, USER_CTRL_DISCONN_SCO);
    }
    return 0;
}

/**
 * @brief 处理BT_STATUS_SCO_CONNECTION_REQ逻辑
 *
 * @param device_a:当前蓝牙事件的设备
 * @param status_a:当前蓝牙设备的状态，详细见枚举STATUS_FOR_USER
 * @param device_b:另一台蓝牙设备
 * @param status_b:另一台蓝牙设备的状态，详细见枚举STATUS_FOR_USER
 *
 * @return 0
 */
static int bt_dual_phone_call_sco_connection_req_handler(void *device_a, int status_a, void *device_b, int status_b)
{
    if (status_a == BT_CALL_ACTIVE) {
        if ((status_b == BT_CALL_OUTGOING) &&
            (btstack_get_call_esco_status(device_b) == BT_ESCO_STATUS_OPEN)) {
            // 设备A切声卡, 抢占设备B的去电
            puts("disconn_sco-b1\n");
            btstack_device_control(device_b, USER_CTRL_DISCONN_SCO);
            return 1;
        }
        if ((status_b == BT_CALL_ACTIVE) &&
            (btstack_get_call_esco_status(device_b) == BT_ESCO_STATUS_OPEN)) {
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
            // 设备A切声卡, 抢占设备B的通话
            puts("disconn_sco-b2\n");
            btstack_device_control(device_b, USER_CTRL_DISCONN_SCO);
#else
            // 设备A切声卡, 不抢占设备B的通话
            puts("disconn_sco-a2\n");
            btstack_device_control(device_a, USER_CTRL_DISCONN_SCO);
#endif
            return 2;
        }

    }
    if (status_a == BT_CALL_HANGUP) {
        if (status_b == BT_CALL_ACTIVE &&
            btstack_get_call_esco_status(device_b) == BT_ESCO_STATUS_OPEN) {
            // 通话挂断另一台设备，部分手机状态显示BT_CALL_HANGUP但是实际上是来电/去电状态
            puts("disconn_sco-b3\n");
            btstack_device_control(device_a, USER_CTRL_DISCONN_SCO);
        }
    }
    return 0;
}

/**
 * @brief 一拖二时，电话相关蓝牙事件消息处理函数 包含通话和通话的抢占
 *
 * 注：如果通话抢占在应用层3秒内没有被处理，蓝牙底层会使用最新的通话链路
 *
 * @param msg  蓝牙事件消息
 *
 */
void bt_dual_phone_call_msg_handler(int *msg)
{
    struct bt_event *event = (struct bt_event *)msg;
    u8 phone_event;

    phone_event = event->event;
    printf("bt_dual_phone_call_msg_handler phone_event:%d\n", phone_event);
    if (phone_event != BT_STATUS_PHONE_INCOME &&
        phone_event != BT_STATUS_PHONE_OUT &&
        phone_event != BT_STATUS_SCO_CONNECTION_REQ) {
        return;
    }
    if (tws_api_get_role_async() == TWS_ROLE_SLAVE) {
        return;
    }

    /*
     * device_a为当前发生事件的设备, device_b为另外一个设备
     */
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
    void *device_a, *device_b, *device_c;
    bt_get_btstack_device3(event->args, &device_a, &device_b, &device_c);
    if (!device_a || (!device_b && !device_c)) {
        return;
    }
#else
    void *device_a, *device_b;
    bt_get_btstack_device(event->args, &device_a, &device_b);
    if (!device_a || !device_b) {
        return;
    }
#endif
    u8 *addr_a = event->args;
    //cppcheck-suppress knownConditionTrueFalse
    u8 *addr_b = device_b ? btstack_get_device_mac_addr(device_b) : NULL;
    if (addr_a) {
        printf("bt_dual_phone_call_msg_handler: a:\n");
        put_buf(addr_a, 6);
    }
    if (addr_b) {
        printf("bt_dual_phone_call_msg_handler: b:\n");
        put_buf(addr_b, 6);
    }

    int status_a = btstack_bt_get_call_status(device_a);
    //cppcheck-suppress knownConditionTrueFalse
    int status_b = device_b ? btstack_bt_get_call_status(device_b) : BT_CALL_HANGUP;
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
    u8 *addr_c = device_c ? btstack_get_device_mac_addr(device_c) : NULL;
    if (addr_c) {
        printf("bt_dual_phone_call_msg_handler: c:\n");
        put_buf(addr_c, 6);
    }
    int status_c = device_c ? btstack_bt_get_call_status(device_c) : BT_CALL_HANGUP;
#endif


    if (phone_event == BT_STATUS_SCO_CONNECTION_REQ) {
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        printf("BT_STATUS_SCO_CONNECTION_REQ: status_a = %d, status_b = %d, status_c = %d\n", status_a, status_b, status_c);
#else
        printf("BT_STATUS_SCO_CONNECTION_REQ: status_a = %d, status_b = %d\n", status_a, status_b);
#endif
        if (status_a == BT_CALL_INCOMING) {
            phone_event = BT_STATUS_PHONE_INCOME;
        } else if (status_a == BT_CALL_OUTGOING) {
            phone_event = BT_STATUS_PHONE_OUT;
        }
    }

    switch (phone_event) {
    case BT_STATUS_PHONE_INCOME:
        printf("BT_STATUS_PHONE_INCOME: status_a = %d, status_b = %d, %d\n", status_a, status_b, btstack_get_call_esco_status(device_b));
        bt_dual_phone_call_phone_income_handler(device_a, device_b, status_b);
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        printf("BT_STATUS_PHONE_INCOME: status_a = %d, status_c = %d, %d\n", status_a, status_c, btstack_get_call_esco_status(device_c));
        bt_dual_phone_call_phone_income_handler(device_a, device_c, status_c);
#endif
        break;
    case BT_STATUS_PHONE_OUT:
        printf("BT_STATUS_PHONE_OUT: status_a = %d, status_b = %d, %d\n", status_a, status_b, btstack_get_call_esco_status(device_b));
        bt_dual_phone_call_phone_out_handler(device_a, device_b, status_b);
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        printf("BT_STATUS_PHONE_OUT: status_a = %d, status_c = %d, %d\n", status_a, status_c, btstack_get_call_esco_status(device_c));
        bt_dual_phone_call_phone_out_handler(device_a, device_c, status_c);
#endif
        break;
    case BT_STATUS_SCO_CONNECTION_REQ:
        printf("BT_STATUS_SCO_CONNECTION_REQ: status_a = %d, status_b = %d, %d\n", status_a, status_b, btstack_get_call_esco_status(device_b));
        bt_dual_phone_call_sco_connection_req_handler(device_a, status_a, device_b, status_b);
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        printf("BT_STATUS_SCO_CONNECTION_REQ: status_a = %d, status_c = %d, %d\n", status_a, status_c, btstack_get_call_esco_status(device_c));
        bt_dual_phone_call_sco_connection_req_handler(device_a, status_a, device_c, status_c);
#endif
        break;
    }

}

