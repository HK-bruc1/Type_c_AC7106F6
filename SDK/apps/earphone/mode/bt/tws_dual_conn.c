#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".tws_dual_conn.data.bss")
#pragma data_seg(".tws_dual_conn.data")
#pragma const_seg(".tws_dual_conn.text.const")
#pragma code_seg(".tws_dual_conn.text")
#endif
#include "btstack/avctp_user.h"
#include "btstack/bluetooth.h"
#include "classic/tws_api.h"
#include "app_main.h"
#include "earphone.h"
#include "app_config.h"
#include "bt_tws.h"
#include "ble_rcsp_server.h"
#include "tws_dual_conn.h"
#include "user_cfg.h"
#include "tws_dual_share.h"
#include "esco_player.h"
#include "app_testbox.h"
#include "update.h"
#include "btstack_rcsp_user.h"
#include "adapter/include/misc_interface.h"
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN))
#include "app_le_connected.h"
#endif
/********edr蓝牙相关状态debug打印说明***********************
  *** tws快连page:             打印 'c'(小写)
  *** tws快连page_scan:        打印 'p'(小写)
  *** 等待被发现inquiry_scan   打印 'I'(大写)
  *** 回连手机page:            打印 'C'(大写)
  *** 等待手机连接page_scan:   打印 'P'(大写)
  ***共享从share快连page_scan: 打印 'q'(小写)
  ***共享主share快连page:      打印 's'(小写)
 *********************************************************/

#if TCFG_USER_TWS_ENABLE
#define MAX_PAGE_DEVICE_NUM 2

#define TIMEOUT_CONN_DEVICE_OPEN_PAGE  1 //第二台设备超时断开回连一直开启page

#define TCFG_TWS_CONN_DISABLE    0//不配对成tws,作为单耳使用,关闭tws相关配对接口

struct page_device_info {
    struct list_head entry;
    u32 timeout;
    u16 timer;
    u8 dev_type;
    u8 mac_addr[6];
};

struct dual_conn_handle {
    u16 timer;
    u16 inquiry_scan_time;
    u16 page_scan_timer;
    u8 device_num_recorded;
    u8 remote_addr[4][6];
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

bool page_list_empty()
{
    return list_empty(&g_dual_conn.page_head);
}

static void auto_close_page_scan(void *p)
{
    puts("auto_close_page_scan\n");
    g_dual_conn.page_scan_timer = 0;
    g_dual_conn.page_scan_auto_disable = 1;
    lmp_hci_write_scan_enable((0 << 1) | 0);

    u8 data[2] = {0, 0};
    data[0] = g_dual_conn.page_scan_auto_disable |
              (g_dual_conn.inquiry_scan_disable << 1) |
              (g_dual_conn.device_num_recorded << 2);
    data[1] = g_dual_conn.device_num_recorded << 1;
    tws_api_send_data_to_slave(data, 2, 0xF730EBC7);
}

void write_scan_conn_enable(bool scan_enable, bool conn_enable)
{
    u32 rets_addr = 0;
    __asm__ volatile("%0 = rets ;" : "=r"(rets_addr));
    printf("write_scan_conn_enable rets=0x%x\n", rets_addr);
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
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
    if (is_cig_phone_conn() || is_cig_other_phone_conn()) {
        g_printf("bt_get_total_connect_dev=%d\n", bt_get_total_connect_dev());
        scan_enable = 0;
        conn_enable = 0;
    }
#elif (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
    g_printf("bt_get_total_connect_dev=%d\n", bt_get_total_connect_dev());
#if LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH
    if (bt_get_total_connect_dev() == 0) {
        if (tws_api_get_role() != TWS_ROLE_SLAVE) {
            scan_enable = 1;
            conn_enable = 1;

        }

    } else if (is_cig_phone_conn() && bt_get_total_connect_dev()) {
        scan_enable = 0;
        conn_enable = 0;

    }
#else
    if (is_cig_phone_conn() || is_cig_other_phone_conn()) {
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
#if TCFG_TWS_AUDIO_SHARE_ENABLE
    int share_state = get_share_phone_conn_state();
    r_printf("get_share_phone_conn_state=%d,is_share_page_scan=%d\n", share_state, check_open_share_wait_page_scan());
    if (share_state == SHARE_CONN_PHONE_DISCONN) {
        scan_enable = 1;
        conn_enable = 1;
        bt_tws_audio_share_search_pair_start(0);
        bt_tws_audio_share_wait_pair(0);
    } else if (share_state == SHARE_DISCON_PHONE_1_CONN) {
        if (check_open_share_wait_page_scan()) {
            bt_tws_audio_share_wait_page_scan();
        }
#if (TCFG_TWS_SHARE_2PHONE_CONN_ENABLE==0)
        scan_enable = 0;
        conn_enable = 0;
#endif

    } else if (share_state == SHARE_DISCON_PHONE_DISCONN) {
        if (check_open_share_wait_page_scan()) {
            bt_tws_audio_share_wait_page_scan();
        }
        scan_enable = 1;
        conn_enable = 1;
    } else if (share_state == SHARE_CONN_PHONE_1_CONN) {
        scan_enable = 0;
        conn_enable = 0;
        bt_tws_audio_share_search_pair_start(0);
        bt_tws_audio_share_wait_pair(0);
    } else if (share_state == SHARE_TWS_SLAVE) {
        scan_enable = 0;
        conn_enable = 0;

    } else if (share_state == SHARE_DISCON_PHONE_2_CONN) {
        scan_enable = 0;
        conn_enable = 0;
        bt_tws_audio_share_search_pair_start(0);
        bt_tws_audio_share_wait_pair(0);

    }
#endif
    if (classic_update_task_exist_flag_get()) {
        g_printf("bt dual close for update\n");
        scan_enable = 0;
        conn_enable = 0;
    }
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    // local_tws模式，关可发现可连接，防止带宽不够
    if (!app_in_mode(APP_MODE_BT)) {
        scan_enable = 0;
        conn_enable = 0;
    }
#endif
    r_printf("write_scan_conn_enable=%d,%d\n", scan_enable, conn_enable);

    lmp_hci_write_scan_enable((conn_enable << 1) | scan_enable);

    if ((scan_enable || conn_enable)) {
        int connect_device = bt_get_total_connect_dev();
        if (page_list_empty()) {
            app_send_message(APP_MSG_BT_IN_PAIRING_MODE, connect_device);
        } else {
            app_send_message(APP_MSG_BT_IN_PAGE_MODE, 0);
        }
    }

#if TCFG_DUAL_CONN_PAGE_SCAN_TIME
    if (tws_api_get_role() == TWS_ROLE_SLAVE || (conn_enable && !scan_enable)) {
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
    puts("check close_inquiry_scan\n");
    g_dual_conn.inquiry_scan_time = 0;
    g_dual_conn.inquiry_scan_disable = 1;
    if (g_dual_conn.device_num_recorded == 1 && bt_get_total_connect_dev() == 1) {
        puts("check close_inquiry_scan true\n");
        write_scan_conn_enable(0, 0);
#if RCSP_MODE && TCFG_RCSP_DUAL_CONN_ENABLE
        rcsp_close_inquiry_scan(true);
#endif
    }
    u8 data[2] = {0, 0};
    data[0] = g_dual_conn.page_scan_auto_disable |
              (g_dual_conn.inquiry_scan_disable << 1) |
              (g_dual_conn.device_num_recorded << 2);
    data[1] = g_dual_conn.device_num_recorded << 1;
    tws_api_send_data_to_slave(data, 2, 0xF730EBC7);
}

static int dual_conn_try_open_inquiry_scan()
{
#if TCFG_DUAL_CONN_INQUIRY_SCAN_TIME
    if (g_dual_conn.inquiry_scan_disable) {
        return 0;
    }
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
    write_scan_conn_enable(1, 1);
#endif
    return 1;
}
static int dual_conn_try_open_inquiry_page_scan()
{
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
#if TCFG_DUAL_CONN_INQUIRY_SCAN_TIME
    int connect_device      = bt_get_total_connect_dev();
    //y_printf("connect_device=%d,need_keep_scan=%d,device_num_recorded=%d,inquiry_scan_disable=%d\n",connect_device,g_dual_conn.need_keep_scan,g_dual_conn.device_num_recorded,g_dual_conn.inquiry_scan_disable);
    if (connect_device == 0) {
        write_scan_conn_enable(1, 1);
    } else {
        write_scan_conn_enable(g_dual_conn.inquiry_scan_disable ? 0 : 1, g_dual_conn.inquiry_scan_disable ? 0 : 1);
    }
#else
    if (g_dual_conn.need_keep_scan) {
        write_scan_conn_enable(0, 1);
    }
#endif
    return 1;

}

int add_device_2_page_list(u8 *mac_addr, u32 timeout, u8 dev_type)
{
    struct page_device_info *info;

    printf("add_device_2_page_list: %d,%d\n", timeout, dev_type);
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
    info->dev_type = dev_type;
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
#if TCFG_TWS_AUDIO_SHARE_ENABLE
    if (check_is_share_addr_for_vm(mac_addr)) {
        del_device_type_from_page_list(PAGE_DEV_SHARE);
        return;
    }
#endif
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
    printf("clr_device_in_page_list\n");
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
static int get_device_type_in_page_list(u8 *addr)
{
    struct page_device_info *info, *n;
    list_for_each_entry_safe(info, n, &g_dual_conn.page_head, entry) {
        if (memcmp(addr, info->mac_addr, 6) == 0) {
            return info->dev_type;
        }
    }
    return -1;
}
void del_device_type_from_page_list(u8 dev_type)
{
    struct page_device_info *info;

    if (!g_dual_conn.page_head_inited) {
        return;
    }

    list_for_each_entry(info, &g_dual_conn.page_head, entry) {
        if (info->dev_type == dev_type) {
            printf("del_device=%d\n", dev_type);
            __list_del_entry(&info->entry);
            if (info->timer) {
                sys_timeout_del(info->timer);
            }
            free(info);
            return;
        }
    }

}


static void tws_wait_pair_timeout(void *p)
{
    g_dual_conn.timer = 0;
    tws_api_cancle_wait_pair();
    dual_conn_page_device();
}

static void tws_auto_pair_timeout(void *p)
{
    g_dual_conn.timer = 0;
    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    write_scan_conn_enable(0, 0);
    dual_conn_page_device();
}

u16 tws_auto_pair_timeout_rand(void)
{
    u16 time_out_rand;
    pseudo_random_genrate((void *)&time_out_rand, 2);

    time_out_rand = time_out_rand & 0x7FF;
    printf("tws_auto_pair_timeout_rand = %d\n", time_out_rand);
    return time_out_rand;  /*0-2047*/
}

static void tws_pair_new_tws(void *p)
{
    tws_api_cancle_create_connection();
    tws_api_auto_pair(0);
    g_dual_conn.timer = sys_timeout_add(NULL, tws_auto_pair_timeout, 1000 + tws_auto_pair_timeout_rand());
}

static void tws_wait_conn_timeout(void *p)
{
    write_scan_conn_enable(0, 0);
    dual_conn_page_device();
}


void tws_dual_conn_state_handler()
{
    int state               = tws_api_get_tws_state();
    int connect_device      = bt_get_total_connect_dev();
    int have_page_device    = page_list_empty() ? false : true;

#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    u8 edr_background_active = 1;
    if (bt_background_active()) {
        edr_background_active = 0;
    }
#endif

#if (TCFG_BT_BACKGROUND_ENABLE == 0)
    if (g_bt_hdl.wait_exit) { //非后台正在退出就不处理消息了，防止退出之后资源释放了，流程又打开可发现可连接导致异常
        return;
    }
#endif
    g_printf("page_state: %d, %x, %d\n", connect_device, state, have_page_device);

    if (g_dual_conn.timer) {
        sys_timeout_del(g_dual_conn.timer);
        g_dual_conn.timer = 0;
    }
    if (app_var.goto_poweroff_flag) {
        return;
    }

    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            write_scan_conn_enable(0, 0);
            return;
        }
        if (connect_device == 0) {
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
            if (!edr_background_active) {
            } else
#endif
            {
                write_scan_conn_enable(1, 1);
            }
        } else if (connect_device == 1) {
            if (g_dual_conn.device_num_recorded > 1) {
#if TIMEOUT_CONN_DEVICE_OPEN_PAGE
                if (have_page_device) {
                    r_printf("page_and_page_sacn");
                    u8 *page_addr = get_device_addr_in_page_list();
                    if (page_addr) {
#if TCFG_TWS_AUDIO_SHARE_ENABLE
                        if (get_device_type_in_page_list(page_addr) == PAGE_DEV_SHARE) {
                            bt_cmd_prepare(USER_CTRL_TWS_AUDIO_SHARE_START_CONNECT, 6, page_addr);
                        } else
#endif
                        {
                            bt_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, page_addr);
                        }
                    }
                }
#endif
                if (esco_player_runing() && have_page_device) {
                    write_scan_conn_enable(0, 0);
                } else {
                    write_scan_conn_enable(0, 1);

                }
            } else if (g_dual_conn.need_keep_scan) {
                write_scan_conn_enable(0, 1);
            } else {
#if TCFG_TWS_AUDIO_SHARE_ENABLE
                write_scan_conn_enable(0, 1);
#endif

            }
        } else if (connect_device == 2) {
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
            if (esco_player_runing() && have_page_device) {
                write_scan_conn_enable(0, 0);
            } else {
                write_scan_conn_enable(0, 1);

            }
#endif

        }
        if (have_page_device) {
#if TCFG_TWS_AUDIO_SHARE_ENABLE
            if (get_share_phone_conn_state() == SHARE_DISCON_PHONE_1_CONN) {//只给一个手机连接，一台手机连接后，直接进去回连共享设备
                tws_wait_conn_timeout(NULL);
            } else
#endif
            {
                g_dual_conn.timer = sys_timeout_add(NULL, tws_wait_conn_timeout, 6000);
                tws_api_auto_role_switch_disable();

            }
        } else {
            if (g_dual_conn.device_num_recorded == 1) {
                dual_conn_try_open_inquiry_page_scan();
            } else if (connect_device == 2) {
#if (TCFG_PREEMPT_CONNECTION_ENABLE || TCFG_BT_DUAL_1T3_CONN_ENABLE)
                write_scan_conn_enable(0, 1);
#endif
            } else if (connect_device == 3) {
#if TCFG_PREEMPT_CONNECTION_ENABLE
                write_scan_conn_enable(0, 1);
#endif
            }
            tws_api_auto_role_switch_enable();
        }
    } else if (state & TWS_STA_TWS_PAIRED) {
        if (connect_device == 0) {
            tws_api_create_connection(0);
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
            if (!edr_background_active) {
            } else
#endif
            {
                write_scan_conn_enable(1, 1);
            }
#if CONFIG_TWS_AUTO_PAIR_WITHOUT_UNPAIR
            g_dual_conn.timer = sys_timeout_add(NULL, tws_pair_new_tws,
                                                TCFG_TWS_CONN_TIMEOUT * 1000);
            return;
#endif
        } else if (connect_device == 1) {
            if (bt_get_remote_test_flag()) {
                return;
            }
            tws_api_wait_connection(0);
            if (g_dual_conn.device_num_recorded > 1 || g_dual_conn.need_keep_scan) {
                write_scan_conn_enable(0, 1);
            } else {
                dual_conn_try_open_inquiry_page_scan();
            }
        } else {
            tws_api_wait_connection(0);
#if (TCFG_PREEMPT_CONNECTION_ENABLE || TCFG_BT_DUAL_1T3_CONN_ENABLE)
            write_scan_conn_enable(0, 1);
#endif
        }
        if (have_page_device) {
            g_dual_conn.timer = sys_timeout_add(NULL, tws_auto_pair_timeout,
                                                TCFG_TWS_CONN_TIMEOUT * 1000);
        }
    } else {
        /* TWS未配对 */
        if (connect_device == 2) {
#if (TCFG_PREEMPT_CONNECTION_ENABLE==0)
            return;
#endif
        }
        if (connect_device == 1 && bt_get_remote_test_flag()) {
            return;
        }
#if TCFG_TWS_CONN_DISABLE
        if (connect_device == 1) {
            write_scan_conn_enable(0, 1);
        } else {
            write_scan_conn_enable(1, 1);
        }
        if (have_page_device) {
            g_dual_conn.timer = sys_timeout_add(NULL, tws_auto_pair_timeout,
                                                TCFG_TWS_CONN_TIMEOUT * 1000);
        }
        return;

#endif
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
        const char *bt_name;
        bt_name = connect_device ? NULL : bt_get_local_name();
        tws_api_wait_pair_by_code(0, bt_name, 0);
        if (connect_device == 0) {
            write_scan_conn_enable(1, 1);
        } else if (g_dual_conn.device_num_recorded > 1) {
            /*write_scan_conn_enable(0, 1);*/
        }
        if (have_page_device) {
            g_dual_conn.timer = sys_timeout_add(NULL, tws_wait_pair_timeout,
                                                TCFG_TWS_PAIR_TIMEOUT * 1000);
        }
#elif CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
        if (connect_device == 0) {
            tws_api_auto_pair(0);
            write_scan_conn_enable(1, 1);
        } else if (connect_device == 1) {
            if (g_dual_conn.device_num_recorded > 1 || g_dual_conn.need_keep_scan) {
                write_scan_conn_enable(0, 1);
            } else {
                dual_conn_try_open_inquiry_page_scan();

            }
        }
        if (have_page_device) {
            g_dual_conn.timer = sys_timeout_add(NULL, tws_auto_pair_timeout,
                                                TCFG_TWS_PAIR_TIMEOUT * 1000);
        }
#else
        if (connect_device == 0) {
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLET
            if (!edr_background_active) {
            } else
#endif
            {
                write_scan_conn_enable(1, 1);
            }
        } else if (connect_device == 1) {
            if (g_dual_conn.device_num_recorded > 1 || g_dual_conn.need_keep_scan) {
                write_scan_conn_enable(0, 1);
            }
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
                free(info);
            } else {
                list_add_tail(&info->entry, &g_dual_conn.page_head);
            }
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
            page_mode_active = 0;
            tws_dual_conn_state_handler();
            break;
        }
    }
}
void dual_conn_page_device_phone_dev_timeout(u8 dev_type)
{
    struct page_device_info *info;

    if (!g_dual_conn.page_head_inited) {
        return;
    }

    /* 参数有效性检查 */
    list_for_each_entry(info, &g_dual_conn.page_head, entry) {
        if (info->dev_type == dev_type) {
            printf("page_device_timeout: %lu, %d\n", jiffies, info->timeout);
            info->timer = 0;
            list_del(&info->entry);
            if (time_after(jiffies, info->timeout)) {
                free(info);
            } else {
                list_add_tail(&info->entry, &g_dual_conn.page_head);
            }
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
            page_mode_active = 0;
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

#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    if (bt_background_active()) {
        return;
    }
#endif

    list_for_each_entry_safe(info, n, &g_dual_conn.page_head, entry) {
        if (info->timer) {
            return;
        }
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
        printf("start_page_device: %lu, %d\n", jiffies, info->timeout);
        put_buf(info->mac_addr, 6);
        int page_timeout = TCFG_BT_PAGE_TIMEOUT * 1000;

        info->timer = sys_timeout_add(info, dual_conn_page_device_timeout,
                                      page_timeout);
#if TCFG_TWS_AUDIO_SHARE_ENABLE
        if (info->dev_type == PAGE_DEV_SHARE) {
            bt_cmd_prepare(USER_CTRL_TWS_AUDIO_SHARE_START_CONNECT, 6, info->mac_addr);
        } else
#endif
        {
            bt_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, info->mac_addr);
        }
        page_mode_active = 1;
        return;
    }

    tws_dual_conn_state_handler();
}


void dual_conn_page_devices_init()
{
    u8 mac_addr[6];

    INIT_LIST_HEAD(&g_dual_conn.page_head);
    g_dual_conn.page_head_inited = 1;
    g_dual_conn.page_scan_auto_disable = 0;

    int num = btstack_get_num_of_remote_device_recorded();
#if (TCFG_TWS_AUDIO_SHARE_ENABLE&&TCFG_TWS_SHARE_2PHONE_CONN_ENABLE)//最后2个设备是否有dongle连接记录
    for (int j = num - 1; j >= 0 && j + 2 >= num ; j--) {
        btstack_get_remote_addr(mac_addr, j);
        if (check_is_share_addr_for_vm(mac_addr)) {
            set_rec_share_device(1);
            if (num == 1) {
                num = 0;
            }
            break;
        }
    }
#endif
    for (int i = num - 1; i >= 0 && i + TCFG_BT_SUPPORT_CONN_NUM >= num ; i--) {
        btstack_get_remote_addr(mac_addr, i);
#if TCFG_TWS_AUDIO_SHARE_ENABLE
        if (check_is_share_addr_for_vm(mac_addr)) {
            continue;
        }
        add_device_2_page_list(mac_addr, TCFG_BT_POWERON_PAGE_TIME * 1000, PAGE_DEV_PHONE);
#if (TCFG_TWS_SHARE_2PHONE_CONN_ENABLE==0)
        num = 1;
        break;
#endif
#else
        add_device_2_page_list(mac_addr, TCFG_BT_POWERON_PAGE_TIME * 1000, PAGE_DEV_PHONE);
#endif
        g_printf("add_device_2_page_list=%d", num);
        put_buf(mac_addr, 6);
    }

#if TCFG_TWS_AUDIO_SHARE_ENABLE
    bt_tws_share_device_init(1, 1);
#endif
    g_dual_conn.device_num_recorded = num;

    if (num == 1) {
        memcpy(g_dual_conn.remote_addr[3], mac_addr, 6);
    }
#if TCFG_DUAL_CONN_INQUIRY_SCAN_TIME
    g_dual_conn.inquiry_scan_disable = 0;
    g_dual_conn.inquiry_scan_time = sys_timeout_add(NULL, close_inquiry_scan, TCFG_DUAL_CONN_INQUIRY_SCAN_TIME * 1000);
#else
    g_dual_conn.inquiry_scan_disable = 1;
#endif
}

void page_next_device(void *p)
{
    g_dual_conn.timer = 0;
    dual_conn_page_device();
}


static int dual_conn_btstack_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;
    int state = tws_api_get_tws_state();

    printf("dual_conn_btstack_event_handler:%d\n", event->event);
    switch (event->event) {
    case BT_STATUS_INIT_OK:
        puts("dual_conn BT_STATUS_INIT_OK");
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN) && TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
        if (jl_unicast_edr_mode_get() == JL_UNICAST_MODE_DEFAULT) {
            INIT_LIST_HEAD(&g_dual_conn.page_head);
            g_dual_conn.page_head_inited = 1;
            printf("cis mode, edr not page\n");
            break;
        } else {
            dual_conn_page_devices_init();
        }
#else
        dual_conn_page_devices_init();
#endif
        return 0;
    case BT_STATUS_FIRST_CONNECTED:
        bt_set_need_keep_scan(0);
        if (g_dual_conn.timer) {
            sys_timeout_del(g_dual_conn.timer);
            g_dual_conn.timer = 0;
        }
        del_device_from_page_list(event->args);

        if (bt_get_remote_test_flag()) {
            return 0;
        }
        memcpy(g_dual_conn.remote_addr[0], event->args, 6);
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            if (!page_list_empty()) {
                if (g_dual_conn.device_num_recorded == 1) {
                    if (memcmp(event->args, g_dual_conn.remote_addr[3], 6)) {
                        g_dual_conn.device_num_recorded++;
                    }
                }
                g_dual_conn.timer = sys_timeout_add(NULL, page_next_device, 500);
                return 0;
            }
        }
        if ((state & TWS_STA_TWS_PAIRED) && (state & TWS_STA_SIBLING_DISCONNECTED)) {
            tws_api_wait_connection(0);
        } else if (state & TWS_STA_SIBLING_CONNECTED) {
            tws_api_auto_role_switch_enable();
        }

        page_mode_active = 0;
#if TCFG_TWS_AUDIO_SHARE_ENABLE
        int share_state = get_share_phone_conn_state();
        if (share_state == SHARE_CONN_PHONE_DISCONN) {
            write_scan_conn_enable(1, 1);
        }
#endif
        if (g_dual_conn.device_num_recorded == 0) {
            g_dual_conn.device_num_recorded++;
            memcpy(g_dual_conn.remote_addr[3], event->args, 6);

#if TCFG_TWS_CONN_DISABLE
            write_scan_conn_enable(0, 1);
#else
            if (state & TWS_STA_SIBLING_CONNECTED) {
                dual_conn_try_open_inquiry_scan();
            } else {
            }
#endif
            break;
        }
        if (g_dual_conn.device_num_recorded == 1) {
#if TCFG_TWS_CONN_DISABLE
            write_scan_conn_enable(0, 1);
#else
            if (memcmp(event->args, g_dual_conn.remote_addr[3], 6) == 0) {
                if (state & TWS_STA_SIBLING_CONNECTED) {
                    dual_conn_try_open_inquiry_scan();
                }
                break;
            }
#endif
            g_dual_conn.device_num_recorded++;
        }
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            if (get_bt_dual_config() == DUAL_CONN_SET_ONE) {
                write_scan_conn_enable(0, 0);
            } else {
                write_scan_conn_enable(0, 1);
            }
        }
        break;
    case BT_STATUS_SECOND_CONNECTED:
    case BT_STATUS_THIRD_CONNECTED:
        int connect_device      = bt_get_total_connect_dev();
        r_printf("BT_STATUS_SECOND_THIRD_CONNECTED=%d\n", connect_device);
        bt_set_need_keep_scan(0);
        if (g_dual_conn.device_num_recorded == 1) {
            g_dual_conn.device_num_recorded++;
        }
        if (g_dual_conn.timer) {
            sys_timeout_del(g_dual_conn.timer);
            g_dual_conn.timer = 0;
        }

#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        del_device_from_page_list(event->args);
        if (connect_device == 3) {
            clr_device_in_page_list();
            memcpy(g_dual_conn.remote_addr[2], event->args, 6);

        } else {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                if (!page_list_empty()) {
                    if (g_dual_conn.device_num_recorded == 2) {
                        if (memcmp(event->args, g_dual_conn.remote_addr[3], 6)) {
                            g_dual_conn.device_num_recorded++;
                        }
                    }
                    g_dual_conn.timer = sys_timeout_add(NULL, page_next_device, 500);
                    return 0;
                }
            }
            memcpy(g_dual_conn.remote_addr[1], event->args, 6);

        }
#else
        clr_device_in_page_list();
        memcpy(g_dual_conn.remote_addr[1], event->args, 6);
#endif
        page_mode_active = 0;
        if ((state & TWS_STA_TWS_PAIRED) && (state & TWS_STA_SIBLING_DISCONNECTED)) {
            tws_api_wait_connection(0);
        } else {
            tws_api_auto_role_switch_enable();
        }
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            break;
        }
#if (TCFG_PREEMPT_CONNECTION_ENABLE)
        write_scan_conn_enable(0, 1);
#else
        write_scan_conn_enable(0, 0);
#endif

#if TCFG_BT_DUAL_1T3_CONN_ENABLE
        if (connect_device == 2) {
            write_scan_conn_enable(0, 1);
        }
#endif
        if (get_bt_dual_config() == DUAL_CONN_SET_ONE) { //判断关闭双连，第二台手机连接，断开第一台手机
            u8 *other_conn_addr = btstack_get_other_dev_addr(event->args);
            if (other_conn_addr) {
                r_printf("DUAL_CONN_SET_ONE dis other conn\n");
                btstack_device_detach(btstack_get_conn_device(other_conn_addr));
            }

        }
        break;

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
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE     //后台不支持edr连接，不处理直接返回
    if (g_bt_hdl.wait_exit) {
        return 0;
    }
#endif

    int is_remote_test = bt_get_remote_test_flag();

    printf("dual_conn_hci_event_handler:0x%x 0x%x\n", event->event, event->value);
    switch (event->event) {
    case HCI_EVENT_VENDOR_NO_RECONN_ADDR:
        break;
    case HCI_EVENT_VENDOR_REMOTE_TEST:
        if (event->value != VENDOR_TEST_DISCONNECTED) {
            if (g_dual_conn.timer) {
                sys_timeout_del(g_dual_conn.timer);
                g_dual_conn.timer = 0;
            }
            tws_api_cancle_wait_pair();
            clr_device_in_page_list();
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        }
        return 0;
    case HCI_EVENT_DISCONNECTION_COMPLETE :
        if (event->value == ERROR_CODE_CONNECTION_TIMEOUT || event->value == ERROR_CODE_ROLE_CHANGE_NOT_ALLOWED) {
            if (is_remote_test == 0) {
#if TCFG_TWS_AUDIO_SHARE_ENABLE
                if (bt_tws_share_device_disconn_deal(ERROR_CODE_CONNECTION_TIMEOUT, event->args)) {
                    break;
                }
#endif
                add_device_2_page_list(event->args, TCFG_BT_TIMEOUT_PAGE_TIME * 1000, PAGE_DEV_PHONE);
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
        case ERROR_CODE_ROLE_CHANGE_NOT_ALLOWED:
        case ERROR_CODE_CONNECTION_TIMEOUT:
        case ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS  :
            if (is_remote_test == 0) {
#if TCFG_TWS_AUDIO_SHARE_ENABLE
                if (bt_tws_share_device_disconn_deal(ERROR_CODE_CONNECTION_TIMEOUT, event->args)) {
                    break;
                }
#endif
                add_device_2_page_list(event->args, TCFG_BT_TIMEOUT_PAGE_TIME * 1000, PAGE_DEV_PHONE);
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

    tws_dual_conn_state_handler();

    return 0;
}
APP_MSG_HANDLER(dual_conn_hci_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_HCI,
    .handler    = dual_conn_hci_event_handler,
};

static void rx_device_info(u8 *data, int len)
{
    if (g_dual_conn.inquiry_scan_disable == 0) {
        g_dual_conn.inquiry_scan_disable = data[0] & 0x02;
    }
    g_dual_conn.page_scan_auto_disable = data[0] & 0x01;
    g_dual_conn.device_num_recorded = data[0] >> 2;

    if (g_dual_conn.page_scan_auto_disable) {
        if (g_dual_conn.page_scan_timer) {
            sys_timeout_del(g_dual_conn.page_scan_timer);
            g_dual_conn.page_scan_timer = 0;
        }
    }
    if ((data[1] & 0x01) == 1) {
        if (tws_api_get_role() == TWS_ROLE_SLAVE || len > 2) {
            clr_device_in_page_list();
            for (int i = 2; i < len; i += 7) {
                add_device_2_page_list(data + i, TCFG_BT_POWERON_PAGE_TIME * 1000, data[i + 6]);
            }
        }
    }
    free(data);
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        tws_dual_conn_state_handler();
    }
}

static void tws_page_device_info_sync(void *_data, u16 len, bool rx)
{
    if (!rx) {
        return;
    }
    u8 *data = malloc(len);
    memcpy(data, _data, len);
    int msg[4] = { (int)rx_device_info, 2, (int)data, len};
    os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
}
REGISTER_TWS_FUNC_STUB(tws_dual_conn_stub) = {
    .func_id = 0xF730EBC7,
    .func   = tws_page_device_info_sync,
};

void send_page_device_addr_2_sibling()
{
    u8 data[23] = {0};
    int offset = 2;
    struct page_device_info *info, *n;

    if (!g_dual_conn.page_head_inited) {
        return;
    }
    data[0] = g_dual_conn.page_scan_auto_disable |
              (g_dual_conn.inquiry_scan_disable << 1) |
              (g_dual_conn.device_num_recorded << 2);
    data[1] = 1;
    list_for_each_entry_safe(info, n, &g_dual_conn.page_head, entry) {
        memcpy(data + offset, info->mac_addr, 6);
        offset += 6;
        data[offset] = info->dev_type;
        offset += 1;

    }
    tws_api_send_data_to_sibling(data, offset, 0xF730EBC7);
}


static int dual_conn_tws_event_handler(int *_event)
{
    struct tws_event *event = (struct tws_event *)_event;
    int role = event->args[0];
    int reason = event->args[2];

    switch (event->event) {
    case TWS_EVENT_CONNECTED:
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CHIP_CONN
        tws_stop_pair_by_chip_conn();
#endif
        if (app_var.goto_poweroff_flag) {
            break;
        }

        if (role == TWS_ROLE_MASTER) {
            tws_api_auto_role_switch_disable();
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN))
            u32 slave_info =  event->args[3] | (event->args[4] << 8) | (event->args[5] << 16) | (event->args[6] << 24) ;
            printf("====slave_info:%x\n", slave_info);
            if (slave_info & TWS_STA_LE_AUDIO_CONNECTED) {
                clr_device_in_page_list();
                break;
            }
#endif
            if (!page_list_empty()) {
                dual_conn_page_device();
                app_send_message(APP_MSG_BT_IN_PAGE_MODE, 0);
            } else {
                tws_dual_conn_state_handler();
            }
            send_page_device_addr_2_sibling();
        } else {
            write_scan_conn_enable(0, 0);
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        tws_set_search_sbiling_state(0);
        if (g_dual_conn.timer) {
            sys_timeout_del(g_dual_conn.timer);
            g_dual_conn.timer = 0;
        }
        bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);

        if (app_var.goto_poweroff_flag) {
            break;
        }

        if (reason & TWS_DETACH_BY_REMOVE_PAIRS) {
            if (role == TWS_ROLE_MASTER) {
                if (bt_get_total_connect_dev()) {
                    break;
                }
            }
            write_scan_conn_enable(1, 1);
            break;
        }
        if (reason & TWS_DETACH_BY_TESTBOX_CON) {
            write_scan_conn_enable(1, 1);
            break;
        }
#if TCFG_TEST_BOX_ENABLE
        if (testbox_get_status()) {
            break;
        }
#endif
        if (reason & TWS_DETACH_BY_POWEROFF) {
            tws_dual_conn_state_handler();
            break;
        }
        if (reason & TWS_DETACH_BY_SUPER_TIMEOUT) {
            if (role == TWS_ROLE_SLAVE) {
                clr_device_in_page_list();
            }
            tws_dual_conn_state_handler();
            break;
        }
        if (!page_list_empty()) {
            dual_conn_page_device();
        } else {
            tws_api_wait_connection(0);
            int connect_device = bt_get_total_connect_dev();
            if (connect_device == 0) {
                write_scan_conn_enable(1, 1);
            } else if (connect_device == 1) {
                if (g_dual_conn.device_num_recorded > 1 || g_dual_conn.need_keep_scan) {
                    write_scan_conn_enable(0, 1);
                } else {
                    write_scan_conn_enable(0, 0);
                }
            }
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_SLAVE) {
            write_scan_conn_enable(0, 0);
            bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
            if (app_var.goto_poweroff_flag == 0) {
                send_page_device_addr_2_sibling();
            }
        }

        break;
    }

    return 0;
}

APP_MSG_HANDLER(dual_conn_tws_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = dual_conn_tws_event_handler,
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
        if (memcmp(mac_addr, g_dual_conn.remote_addr[2], 6) == 0) {
            continue;
        }
        int ret = add_device_2_page_list(mac_addr, TCFG_BT_POWERON_PAGE_TIME * 1000, PAGE_DEV_PHONE);
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
    tws_api_cancle_create_connection();
    tws_api_cancle_wait_pair();
    write_scan_conn_enable(0, 0);
    dual_conn_page_device();
}

int tws_host_get_local_role()
{
    int state = tws_api_get_tws_state();
    if (!page_list_empty()) {
        state |= TWS_STA_HAVE_PAGE_INFO;
    }
#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN))
    if (is_cig_phone_conn()) {
        state |= TWS_STA_LE_AUDIO_CONNECTED;
    }
    if (is_cig_music_play() || is_cig_phone_call_play()) {
        state |= TWS_STA_LE_AUDIO_PLAYING;
    }
#endif
    /* r_printf("tws_host_get_local_role=%x\n",state ); */
    return state;

}

bool tws_host_role_switch(int remote_info, int local_info)
{
    /* r_printf("tws_host_role_switch=%x,%x\n",remote_info,local_info ); */
    if (remote_info & TWS_STA_PHONE_CONNECTED) { //对方已连手机
        return FALSE;
    }
    if (remote_info & TWS_STA_LE_AUDIO_CONNECTED) { //对方已连手机le_audio
        return FALSE;
    }
#if RCSP_MODE
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    if (local_info & TWS_STA_LE_AUDIO_CONNECTED) { //本机已连手机le_audio
        y_printf("tws_role_switch lea ok---%x---%x\n", local_info & TWS_STA_HAVE_PAGE_INFO, !(remote_info & TWS_STA_HAVE_PAGE_INFO));
        if ((local_info & TWS_STA_HAVE_PAGE_INFO) && !(remote_info & TWS_STA_HAVE_PAGE_INFO)) {
            //tws超时断开同时与手机也超时断开，再次tws连接上后，还需要继续回连手机，有回连信息端要继续做回主机
            return TRUE;
        }
    } else
#endif
    {
        if (bt_rcsp_ble_conn_num()) {
            //只连ble不连edr的情况下，tws重新连接后有ble连接的做主机
            y_printf("tws_host_role_switch = 1, %s %d\n", __func__, __LINE__);
            return TRUE;
        }
    }
#else
    if ((local_info & TWS_STA_HAVE_PAGE_INFO) && !(remote_info & TWS_STA_HAVE_PAGE_INFO)) {
        //tws超时断开同时与手机也超时断开，再次tws连接上后，还需要继续回连手机，有回连信息端要继续做回主机
        return TRUE;
    }
#endif
    return FALSE;

}



/*
 * 设置自动回连的识别码 6个byte
 * */
static u8 auto_pair_code[6] = {0x34, 0x66, 0x33, 0x87, 0x09, 0x42};

static u8 *tws_set_auto_pair_code(void)
{
    u16 code = bt_get_tws_device_indicate(NULL);
    auto_pair_code[0] = code >> 8;
    auto_pair_code[1] = code & 0xff;
    return auto_pair_code;
}

static void tws_pair_timeout(void *p)
{
    r_printf("tws_pair_timeout\n");
    g_dual_conn.timer = 0;

    tws_api_cancle_create_connection();

    if (!page_list_empty()) {
        dual_conn_page_device();
        app_send_message(APP_MSG_BT_IN_PAGE_MODE, 0);
    } else {
#if TCFG_TWS_CONN_DISABLE
        write_scan_conn_enable(1, 1);
#else
        tws_api_wait_pair_by_code(0, bt_get_local_name(), 0);
        app_send_message(APP_MSG_BT_IN_PAIRING_MODE, 0);
#endif
#if TCFG_TWS_AUDIO_SHARE_ENABLE
        write_scan_conn_enable(1, 1);
#endif
    }
    app_send_message(APP_MSG_TWS_POWERON_PAIR_TIMEOUT, 0);
}

static void tws_create_conn_timeout(void *p)
{
    g_dual_conn.timer = 0;
    tws_api_cancle_create_connection();

    if (!page_list_empty()) {
        dual_conn_page_device();
        app_send_message(APP_MSG_BT_IN_PAGE_MODE, 0);
    } else {
        tws_dual_conn_state_handler();
    }
    app_send_message(APP_MSG_TWS_POWERON_CONN_TIMEOUT, 0);
}

static void msg_tws_wait_pair_timeout(void *p)
{
    g_dual_conn.timer = 0;
    tws_api_cancle_wait_pair();
}

static void msg_tws_start_pair_timeout(void *p)
{
    tws_set_search_sbiling_state(0);
    g_dual_conn.timer = 0;
    tws_api_wait_pair_by_code(0, bt_get_local_name(), 0);
    tws_set_search_sbiling_state(0);
    app_send_message(APP_MSG_TWS_START_PAIR_TIMEOUT, 0);
}

static void msg_tws_start_conn_timeout(void *p)
{
    g_dual_conn.timer = 0;
    app_send_message(APP_MSG_TWS_START_CONN_TIMEOUT, 0);
}

static int dual_conn_app_event_handler(int *msg)
{
    switch (msg[0]) {
    case APP_MSG_TWS_PAIRED:
#if TCFG_TWS_CONN_DISABLE
        tws_pair_timeout(NULL);
        break;
#endif
        tws_api_create_connection(0);
        g_dual_conn.timer = sys_timeout_add(NULL, tws_create_conn_timeout,
                                            TCFG_TWS_CONN_TIMEOUT * 1000);
        break;
    case APP_MSG_TWS_UNPAIRED:
#if TCFG_TWS_CONN_DISABLE
        tws_pair_timeout(NULL);
        break;
#endif
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
        /* 未配对, 开始自动配对 */
        tws_api_set_quick_connect_addr(tws_set_auto_pair_code());
        tws_api_auto_pair(0);
        g_dual_conn.timer = sys_timeout_add(NULL, tws_pair_timeout,
                                            TCFG_TWS_PAIR_TIMEOUT  * 1000);
#elif CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CHIP_CONN
        tws_start_pair_by_chip_conn();
#else
        /* 未配对, 等待发起配对 */
        if (!list_empty(&g_dual_conn.page_head)) {
            dual_conn_page_device();
        } else {
            tws_api_wait_pair_by_code(0, bt_get_local_name(), 0);
        }
#endif
        break;
    case APP_MSG_BT_PAGE_DEVICE:
        page_device_msg_handler();
        break;
    case APP_MSG_TWS_START_PAIR_AND_CONN_PROCESS:
        if (bt_tws_is_paired()) {
            app_send_message(APP_MSG_TWS_PAIRED, 0);
        } else {
            app_send_message(APP_MSG_TWS_UNPAIRED, 0);
        }
        break;
    case APP_MSG_TWS_WAIT_PAIR:
        tws_api_wait_pair_by_code(0, bt_get_local_name(), 0);
        g_dual_conn.timer = sys_timeout_add(NULL, msg_tws_wait_pair_timeout,
                                            TCFG_TWS_PAIR_TIMEOUT * 1000);
        break;
    case APP_MSG_TWS_START_PAIR:
        tws_set_search_sbiling_state(1);
        tws_api_search_sibling_by_code();
        tws_set_search_sbiling_state(1);
        if (g_dual_conn.timer) {
            sys_timeout_del(g_dual_conn.timer);
        }
        g_dual_conn.timer = sys_timeout_add(NULL, msg_tws_start_pair_timeout,
                                            TCFG_TWS_PAIR_TIMEOUT * 1000);
        break;
    case APP_MSG_TWS_WAIT_CONN:
        tws_api_wait_connection(0);
        break;
    case APP_MSG_TWS_START_CONN:
        tws_api_create_connection(0);
        if (g_dual_conn.timer) {
            sys_timeout_del(g_dual_conn.timer);
        }
        g_dual_conn.timer = sys_timeout_add(NULL, msg_tws_start_conn_timeout,
                                            TCFG_TWS_CONN_TIMEOUT * 1000);
        break;
    }

    return 0;
}

APP_MSG_HANDLER(dual_conn_app_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = dual_conn_app_event_handler,
};


void tws_dual_conn_close()
{
    if (g_dual_conn.timer) {
        sys_timeout_del(g_dual_conn.timer);
        g_dual_conn.timer = 0;
    }
    //clr_device_in_page_list();
    tws_api_cancle_create_connection();
    tws_api_cancle_wait_pair();
    write_scan_conn_enable(0, 0);
    bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
}
bool check_page_mode_active(void)
{
    return (page_mode_active) ? TRUE : FALSE;
}
#endif
