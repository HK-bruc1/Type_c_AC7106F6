#include "typedef.h"
#include "event.h"
#include "app_msg.h"
#include "btstack/avctp_user.h"
#include "app_config.h"
#include "net/bt_netif.h"



#if (defined TCFG_BT_SUPPORT_PAN && (TCFG_BT_SUPPORT_PAN==1))
#include "lwip.h"
static int bt_dhcp_flag = 0;
extern u8 pan_profile_support;

int bt_lwip_event_cb(void *lwip_ctx, enum LWIP_EVENT event)
{
    struct sys_event e = {0};
    e.type = SYS_NET_EVENT;
    e.u.net.event = event;
    app_send_message_from(MSG_FROM_NET_BT, sizeof(e), (int *)&e);
    return 0;
}



int net_get_dhcp_flag()
{
    return bt_dhcp_flag;
}


int net_event_cb(int *msg)
{
    struct sys_event *event = (struct sys_event *)msg;
    if (!pan_profile_support) {
        return 0;
    }


    switch (event->u.net.event) {
    case LWIP_BT_DHCP_BOUND_TIMEOUT:
        puts("LWIP_EXT_WIRELESS_DHCP_BOUND_SUCC\n");
        break;
    case LWIP_BT_DHCP_BOUND_RELEASE:
        puts("LWIP_BT_DHCP_BOUND_RELEASE\n");
        bt_dhcp_flag = 0;
        break;
    case LWIP_BT_DHCP_BOUND_SUCC:
        puts("LWIP_BT_DHCP_BOUND_SUCC\n");
        bt_dhcp_flag = 1;
        /* jl_net_time_sync(); */
        break;
    }
    return 0;
}

APP_MSG_HANDLER(net_bt_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_NET_BT,
    .handler    = net_event_cb,
};

static int bt_net_conn_btstack_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;

    printf("bt_net_conn_btstack_event_handler:%d\n", event->event);
    switch (event->event) {
    case BT_STATUS_RECONN_OR_CONN:
        user_pan_send_cmd(event->args, 2, 0, NULL);
        break;
    }
    return 0;
}

APP_MSG_HANDLER(bt_net_conn_stack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = bt_net_conn_btstack_event_handler,
};

#endif

