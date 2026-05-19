#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".dual_conn.data.bss")
#pragma data_seg(".dual_conn.data")
#pragma const_seg(".dual_conn.text.const")
#pragma code_seg(".dual_conn.text")
#endif
#include "btstack/avctp_user.h"
#include "app_main.h"
#include "earphone.h"
#include "app_config.h"
#include "user_cfg.h"
#include "bt_background.h"
#include "dual_conn.h"
#include "update.h"
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "app_le_connected.h"
#endif

#if TCFG_APP_BT_EN

#if (TCFG_USER_TWS_ENABLE == 0)

#define MAX_PAGE_DEVICE_NUM 2

#define TIMEOUT_CONN_DEVICE_OPEN_PAGE  1 //第二台设备超时断开回连一直开启page

static void page_next_device(void *p);

struct page_device_info {
    struct list_head entry;
    u32 timeout;
    u16 timer;
    u8 mac_addr[6];
};

struct dual_conn_handle {
    u16 timer;
    u16 page_scan_timer;
    u16 close_inquiry_scan_timer;
    u8 device_num_recorded;
    u8 remote_addr[3][6];
    u8 page_head_inited;
    u8 page_scan_auto_disable;
    u8 inquiry_scan_disable;
    u8 need_keep_scan;
    struct list_head page_head;
};

static struct dual_conn_handle g_dual_conn;
static u8 page_mode_active = 0;

void dual_conn_page_device();

void bt_set_need_keep_scan(u8 en)
{
    g_dual_conn.need_keep_scan = en;
}

static bool page_list_empty()
{
    return list_empty(&g_dual_conn.page_head);
}

static void auto_close_page_scan(void *p)
{
    puts("auto_close_page_scan\n");
    g_dual_conn.page_scan_timer = 0;
    g_dual_conn.page_scan_auto_disable = 1;
    lmp_hci_write_scan_enable((0 << 1) | 0);
}

void write_scan_conn_enable(bool scan_enable, bool conn_enable)
{
    u32 rets_addr = 0;
    __asm__ volatile("%0 = rets ;" : "=r"(rets_addr));
    r_printf("write_scan_conn_enable rets=0x%x,%d,%d\n", rets_addr, scan_enable, conn_enable);
    if (g_dual_conn.page_scan_auto_disable) {
        if (!scan_enable && conn_enable) {
            return;
        }
    }

    if ((get_bt_dual_config() == DUAL_CONN_SET_ONE) && (bt_get_total_connect_dev())) { //关闭1t2功能，或者关闭双连，已连接一台手机，屏蔽其它状态
        g_printf("bt dual close\n");
        scan_enable = 0;
        conn_enable = 0;
    }
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN)))
    if (is_cig_phone_conn() || is_cig_other_phone_conn()) {
        g_printf("bt_get_total_connect_dev=%d\n", bt_get_total_connect_dev());
        scan_enable = 0;
        conn_enable = 0;
    }
#elif (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
#if LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH
    if (bt_get_total_connect_dev() == 0) {
        scan_enable = 1;
        conn_enable = 1;

    } else if (is_cig_phone_conn() && bt_get_total_connect_dev()) {
        scan_enable = 0;
        conn_enable = 0;

    }
#else
    if (is_cig_phone_conn() || is_cig_other_phone_conn()) {
        g_printf("bt_get_total_connect_dev=%d\n", bt_get_total_connect_dev());
        scan_enable = 0;
        conn_enable = 0;
    }
#endif
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
    if (jl_unicast_edr_mode_get() == JL_UNICAST_MODE_DEFAULT) {
        scan_enable = 0;
        conn_enable = 0;
        y_printf("JL_UNICAST_MODE_DEFAULT disbale scan conn\n");
    } else if (jl_unicast_edr_mode_get() == JL_UNICAST_MODE_EDR) {
        le_audio_adv_api_enable(0);
        y_printf("JL_UNICAST_MODE_EDR disbale le_audio_adv\n");
    }
#endif
#endif
    if (classic_update_task_exist_flag_get()) {
        g_printf("bt dual close for update\n");
        scan_enable = 0;
        conn_enable = 0;
    }
    r_printf("write_scan_conn_enable=%d,%d\n", scan_enable, conn_enable);

    lmp_hci_write_scan_enable((conn_enable << 1) | scan_enable);

    if ((scan_enable || conn_enable) && page_list_empty()) {
        int connect_device = bt_get_total_connect_dev();
        app_send_message(APP_MSG_BT_IN_PAIRING_MODE, connect_device);
    }

#if TCFG_DUAL_CONN_PAGE_SCAN_TIME
    if (conn_enable && !scan_enable) {
        if (g_dual_conn.page_scan_timer) {
            sys_timer_modify(g_dual_conn.page_scan_timer,
                             TCFG_DUAL_CONN_PAGE_SCAN_TIME * 1000);
        } else {
            g_dual_conn.page_scan_timer = sys_timeout_add(NULL, auto_close_page_scan,
                                          TCFG_DUAL_CONN_PAGE_SCAN_TIME * 1000);
        }
    } else {
        if (g_dual_conn.page_scan_timer) {
            sys_timeout_del(g_dual_conn.page_scan_timer);
            g_dual_conn.page_scan_timer = 0;
        }
    }
#endif
}

static void close_inquiry_scan(void *p)
{
    g_dual_conn.inquiry_scan_disable = 1;
    if (g_dual_conn.device_num_recorded == 1 && bt_get_total_connect_dev() == 1) {
        write_scan_conn_enable(0, 0);
    }
    g_dual_conn.close_inquiry_scan_timer = 0;
}

static int dual_conn_try_open_inquiry_scan()
{
#if TCFG_DUAL_CONN_INQUIRY_SCAN_TIME
    if (g_dual_conn.inquiry_scan_disable) {
        return 0;
    }
    write_scan_conn_enable(1, 1);
#endif
    return 1;
}

static int add_device_2_page_list(u8 *mac_addr, u32 timeout)
{
    struct page_device_info *info;

    r_printf("add_device_2_page_list: %d\n", timeout);
    put_buf(mac_addr, 6);

    if (!g_dual_conn.page_head_inited) {
        return 0;
    }

    list_for_each_entry(info, &g_dual_conn.page_head, entry) {
        if (memcmp(info->mac_addr, mac_addr, 6) == 0) {
            if (info->timer) {
                sys_timeout_del(info->timer);
                info->timer = 0;
            }
            info->timeout = jiffies + msecs_to_jiffies(timeout);

            __list_del_entry(&info->entry);
            list_add_tail(&info->entry, &g_dual_conn.page_head);
            return 1;
        }
    }

    info = malloc(sizeof(*info));
    info->timer = 0;
    info->timeout = jiffies + msecs_to_jiffies(timeout);
    memcpy(info->mac_addr, mac_addr, 6);
    list_add_tail(&info->entry, &g_dual_conn.page_head);
    return 0;
}

static void del_device_from_page_list(u8 *mac_addr)
{
    struct page_device_info *info;

    if (!g_dual_conn.page_head_inited) {
        return;
    }

    list_for_each_entry(info, &g_dual_conn.page_head, entry) {
        if (memcmp(info->mac_addr, mac_addr, 6) == 0) {
            puts("del_device\n");
            put_buf(mac_addr, 6);
            __list_del_entry(&info->entry);
            if (info->timer) {
                sys_timeout_del(info->timer);
            }
            free(info);
            return;
        }
    }

}

void clr_device_in_page_list()
{
    r_printf("clr_device_in_page_list\n");
    struct page_device_info *info, *n;

    if (!g_dual_conn.page_head_inited) {
        return;
    }

    list_for_each_entry_safe(info, n, &g_dual_conn.page_head, entry) {
        __list_del_entry(&info->entry);
        if (info->timer) {
            sys_timeout_del(info->timer);
        }
        free(info);
    }
}

static u8 *get_device_addr_in_page_list()
{
    struct page_device_info *info, *n;
    list_for_each_entry_safe(info, n, &g_dual_conn.page_head, entry) {
        return info->mac_addr;
    }
    return NULL;
}

void dual_conn_state_handler()
{
    int connect_device      = bt_get_total_connect_dev();
    int have_page_device    = page_list_empty() ? false : true;
    printf("page_state: %d, %d\n", connect_device, have_page_device);
    if (g_dual_conn.timer) {
        sys_timeout_del(g_dual_conn.timer);
        g_dual_conn.timer = 0;
    }
    if (connect_device == 0) {
        write_scan_conn_enable(1, 1);
    } else if (connect_device == 1) {
#if TCFG_BT_DUAL_CONN_ENABLE
        if (g_dual_conn.device_num_recorded > 1 || g_dual_conn.need_keep_scan) {
            write_scan_conn_enable(0, 1);
        } else {
            write_scan_conn_enable(0, 1);
        }
#endif
    }
}

static void dual_conn_page_device_timeout(void *p)
{
    struct page_device_info *info;

    if (!g_dual_conn.page_head_inited) {
        return;
    }

    /* 参数有效性检查 */
    list_for_each_entry(info, &g_dual_conn.page_head, entry) {
        if (info == p) {
            printf("page_device_timeout: %lu, %d\n", jiffies, info->timeout);
            info->timer = 0;
            list_del(&info->entry);
            if (time_after(jiffies, info->timeout)) {
                del_device_from_page_list(info->mac_addr);
                free(info);
            } else {
                list_add_tail(&info->entry, &g_dual_conn.page_head);
            }
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
            if (!page_list_empty()) {
                if (g_dual_conn.timer) {
                    sys_timeout_del(g_dual_conn.timer);
                    g_dual_conn.timer = 0;
                }
                g_dual_conn.timer = sys_timeout_add(NULL, page_next_device, 2000);
                //增加2s可发现可连接
                bt_discovery_and_connectable_using_loca_mac_addr(1, 1);
                return;
            }
            page_mode_active = 0;
            dual_conn_state_handler();
            break;
        }
    }
}

void dual_conn_page_device()
{
    struct page_device_info *info, *n;

    if (!g_dual_conn.page_head_inited) {
        return;
    }

    list_for_each_entry_safe(info, n, &g_dual_conn.page_head, entry) {
        if (info->timer) {
            return;
        }
        printf("start_page_device: %lu, %d\n", jiffies, info->timeout);
        put_buf(info->mac_addr, 6);
        info->timer = sys_timeout_add(info, dual_conn_page_device_timeout,
                                      TCFG_BT_PAGE_TIMEOUT * 1000);
        bt_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, info->mac_addr);
        page_mode_active = 1;
        return;
    }

    dual_conn_state_handler();
}


void dual_conn_page_devices_init()
{
    u8 mac_addr[6];

    INIT_LIST_HEAD(&g_dual_conn.page_head);
    g_dual_conn.page_head_inited = 1;
    g_dual_conn.page_scan_auto_disable = 0;

    int num = btstack_get_num_of_remote_device_recorded();
    for (int i = num - 1; i >= 0 && i + 2 >= num ; i--) {
        btstack_get_remote_addr(mac_addr, i);
        add_device_2_page_list(mac_addr, TCFG_BT_POWERON_PAGE_TIME * 1000);
    }
    g_dual_conn.device_num_recorded = num;
    if (num == 1) {
        memcpy(g_dual_conn.remote_addr[2], mac_addr, 6);
    }

#if TCFG_DUAL_CONN_INQUIRY_SCAN_TIME
    g_dual_conn.inquiry_scan_disable = 0;
    g_dual_conn.close_inquiry_scan_timer = sys_timeout_add(NULL, close_inquiry_scan, TCFG_DUAL_CONN_INQUIRY_SCAN_TIME * 1000);
#else
    g_dual_conn.inquiry_scan_disable = 1;
#endif

#if (TCFG_LP_NFC_TAG_ENABLE && TCFG_LP_NFC_TAG_TYPE == JL_BT_TAG)
    static u8 nfc_wakeup_disable_page = 1;
    if ((is_reset_source(MSYS_P2M_RST)) && (is_wakeup_source(PWR_WK_REASON_LPNFC)) && nfc_wakeup_disable_page) {
        nfc_wakeup_disable_page = 0;
        write_scan_conn_enable(1, 1);
    } else {            //非nfc唤醒
        dual_conn_page_device();
    }
#else
    dual_conn_page_device();
#endif
}

static void page_next_device(void *p)
{
    g_dual_conn.timer = 0;
    dual_conn_page_device();
}

static void dual_conn_bt_connect_timeout(struct bt_event *bt)
{
    add_device_2_page_list(bt->args, TCFG_BT_TIMEOUT_PAGE_TIME * 1000);

    if (g_dual_conn.timer) {
        sys_timeout_del(g_dual_conn.timer);
        g_dual_conn.timer = 0;
    }
    dual_conn_page_device();
}

static int dual_conn_btstack_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;

    printf("dual_conn_btstack_event_handler:%d\n", event->event);
    switch (event->event) {
    case BT_STATUS_INIT_OK:
#if TCFG_NORMAL_SET_DUT_MODE
        break;
#endif
        puts("dual_conn BT_STATUS_INIT_OK");
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
        if (jl_unicast_edr_mode_get() == JL_UNICAST_MODE_DEFAULT) {
            printf("cis mode, edr not page\n");
            break;
        } else {
            dual_conn_page_devices_init();
        }
#else
        dual_conn_page_devices_init();
#endif
#if (TCFG_BT_BACKGROUND_ENABLE)
        bt_background_switch_mode_after_initializes();
#endif
        return 0;
    case BT_STATUS_FIRST_CONNECTED:
        puts("dual_conn BT_STATUS_FIRST_CONNECTED");
        bt_set_need_keep_scan(0);
        if (g_dual_conn.timer) {
            sys_timeout_del(g_dual_conn.timer);
            g_dual_conn.timer = 0;
        }
        del_device_from_page_list(event->args);
        memcpy(g_dual_conn.remote_addr[0], event->args, 6);
        if (!page_list_empty()) {
            g_dual_conn.timer = sys_timeout_add(NULL, page_next_device, 500);
            return 0;
        }

        page_mode_active = 0;
        if (g_dual_conn.device_num_recorded == 0) {
            g_dual_conn.device_num_recorded++;
            memcpy(g_dual_conn.remote_addr[2], event->args, 6);
            break;
        }
        if (g_dual_conn.device_num_recorded == 1) {
            if (memcmp(event->args, g_dual_conn.remote_addr[2], 6) == 0) {
                break;
            }
            g_dual_conn.device_num_recorded++;
        }
        if (get_bt_dual_config() == DUAL_CONN_SET_ONE) {
            write_scan_conn_enable(0, 0);
        } else {
            write_scan_conn_enable(0, 1);
        }
        break;
#if TCFG_BT_DUAL_CONN_ENABLE
    case BT_STATUS_SECOND_CONNECTED:
        puts("dual_conn BT_STATUS_SECOND_CONNECTED");
        bt_set_need_keep_scan(0);
        if (g_dual_conn.device_num_recorded == 1) {
            g_dual_conn.device_num_recorded++;
        }
        if (g_dual_conn.timer) {
            sys_timeout_del(g_dual_conn.timer);
            g_dual_conn.timer = 0;
        }
        clr_device_in_page_list();
        page_mode_active = 0;
        memcpy(g_dual_conn.remote_addr[1], event->args, 6);
        break;
#endif
    }

    return 0;
}

APP_MSG_HANDLER(dual_conn_stack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = dual_conn_btstack_event_handler,
};


static int dual_conn_hci_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;

    if (app_var.goto_poweroff_flag) {
        return 0;
    }
    int is_remote_test = bt_get_remote_test_flag();

    printf("dual_conn_hci_event_handler:0x%x 0x%x\n", event->event, event->value);
    switch (event->event) {
    case HCI_EVENT_VENDOR_NO_RECONN_ADDR:
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE :
        if (event->value == ERROR_CODE_CONNECTION_TIMEOUT) {
            printf("dual_conn ERROR_CODE_CONNECTION_TIMEOUT");
            put_buf(event->args, 7);
            if (is_remote_test == 0) {
                dual_conn_bt_connect_timeout(event);
            }
        }
        break;
    case HCI_EVENT_CONNECTION_COMPLETE:
        switch (event->value) {
        case ERROR_CODE_SUCCESS :
            if (g_dual_conn.timer) {
                sys_timeout_del(g_dual_conn.timer);
                g_dual_conn.timer = 0;
            }
            del_device_from_page_list(event->args);
            return 0;
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR:
        case ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED:
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES:
            if (!list_empty(&g_dual_conn.page_head)) {
                struct page_device_info *info;
                info = list_first_entry(&g_dual_conn.page_head,
                                        struct page_device_info, entry);
                list_del(&info->entry);
                list_add_tail(&info->entry, &g_dual_conn.page_head);
            }
            break;
        case ERROR_CODE_PIN_OR_KEY_MISSING:
        case ERROR_CODE_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED :
        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
        case ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST :
        case ERROR_CODE_AUTHENTICATION_FAILURE :
            break;
        case ERROR_CODE_PAGE_TIMEOUT:
            break;
        case ERROR_CODE_CONNECTION_TIMEOUT:
            printf("dual_conn ERROR_CODE_CONNECTION_TIMEOUT 2");
            put_buf(event->args, 7);
            dual_conn_bt_connect_timeout(event);
            break;
        case ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS  :
            if (is_remote_test == 0) {
                add_device_2_page_list(event->args, TCFG_BT_TIMEOUT_PAGE_TIME * 1000);
                if (g_dual_conn.timer) {
                    sys_timeout_del(g_dual_conn.timer);
                    g_dual_conn.timer = 0;
                }
                dual_conn_page_device();
            }
            break;
        default:
            return 0;
        }
        break;
    default:
        return 0;
    }
    g_dual_conn.page_scan_auto_disable = 0;
    for (int i = 0; i < 2; i++) {
        if (memcmp(event->args, g_dual_conn.remote_addr[i], 6) == 0) {
            memset(g_dual_conn.remote_addr[i], 0xff, 6);
        }
    }

    dual_conn_state_handler();

    return 0;
}
APP_MSG_HANDLER(dual_conn_hci_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_HCI,
    .handler    = dual_conn_hci_event_handler,
};

static void page_device_msg_handler()
{
    u8 mac_addr[6];
    struct page_device_info *info;
    int device_num = bt_get_total_connect_dev();

    if (!g_dual_conn.page_head_inited) {
        return;
    }

    list_for_each_entry(info, &g_dual_conn.page_head, entry) {
        device_num++;
    }
    if (device_num >= TCFG_BT_SUPPORT_CONN_NUM) {
        return;
    }

    int num = btstack_get_num_of_remote_device_recorded();
    for (int i = num - 1; i >= 0; i--) {
        btstack_get_remote_addr(mac_addr, i);
        if (memcmp(mac_addr, g_dual_conn.remote_addr[0], 6) == 0) {
            continue;
        }
        if (memcmp(mac_addr, g_dual_conn.remote_addr[1], 6) == 0) {
            continue;
        }
        int ret = add_device_2_page_list(mac_addr, TCFG_BT_POWERON_PAGE_TIME * 1000);
        if (ret == 0) {
            if (++device_num >= TCFG_BT_SUPPORT_CONN_NUM) {
                break;
            }
        }
    }
    if (g_dual_conn.timer) {
        sys_timeout_del(g_dual_conn.timer);
        g_dual_conn.timer = 0;
    }
    write_scan_conn_enable(0, 0);
    dual_conn_page_device();
}



static int dual_conn_app_event_handler(int *msg)
{
    switch (msg[0]) {
    case APP_MSG_BT_PAGE_DEVICE:
        page_device_msg_handler();
        break;
    }

    return 0;
}

APP_MSG_HANDLER(dual_conn_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = dual_conn_app_event_handler,
};


void dual_conn_close()
{
    if (g_dual_conn.timer) {
        sys_timeout_del(g_dual_conn.timer);
        g_dual_conn.timer = 0;
    }
#if TCFG_DUAL_CONN_INQUIRY_SCAN_TIME
    if (g_dual_conn.close_inquiry_scan_timer) {
        sys_timeout_del(g_dual_conn.close_inquiry_scan_timer);
        g_dual_conn.close_inquiry_scan_timer = 0;
    }
#endif
    clr_device_in_page_list();
    write_scan_conn_enable(0, 0);
    bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
}

bool check_page_mode_active(void)
{
    return (page_mode_active) ? TRUE : FALSE;
}
#endif

#endif
