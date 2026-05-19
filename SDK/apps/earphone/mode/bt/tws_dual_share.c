#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".tws_dual_share.data.bss")
#pragma data_seg(".tws_dual_share.data")
#pragma const_seg(".tws_dual_share.text.const")
#pragma code_seg(".tws_dual_share.text")
#endif
#include "btstack/avctp_user.h"
#include "btstack/bluetooth.h"
#include "classic/tws_api.h"
#include "app_main.h"
#include "earphone.h"
#include "app_config.h"
#include "bt_tws.h"
#include "ble_rcsp_server.h"
#include "app_tone.h"
#include "tws_dual_share.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "bt_common.h"
#include "user_cfg.h"


#if TCFG_USER_TWS_ENABLE
#if TCFG_TWS_AUDIO_SHARE_ENABLE
struct dual_share_handle {
    u8 share_page_scan_open;
    u16 share_conn_timeout;
    u8 share_slave_to_master_step;
    u8 rec_share_device_flag;
    u16 share_conn_timer;
};
enum {
    SHARE_SLAVE_TO_MASTER_STEP_START = 1,
    SHARE_SLAVE_TO_MASTER_STEP_DISCONN,
    SHARE_SLAVE_TO_MASTER_STEP_TIMEOUT,

};
struct dual_share_handle g_share_handle;
struct share_info_t {
    u8 save_addr[6];
    u8 role;
};
#define BT_SYNC_SHARE_TO_SHARE_DATE_FUNC_ID 0xF06127CD
#define BT_SYNC_TWS_DATA_FUNC_ID 0xFEC06127
enum {
    AVCTP_OPID_PLAY        = 0x44,
    AVCTP_OPID_STOP        = 0x45,
    AVCTP_OPID_PAUSE       = 0x46,
};
enum {
    DATA_ID_SHARE_A2DP_PREEMPTED_ADDR,
    DATA_ID_SHARE_TWS_ROLE_SWITCH_OPEN_PAGE_SCAN_ADDR,
};
static const u8 tws_audio_share_pair_addr[6] = {0x18, 0x28, 0xaa, 0x28, 0x33, 0x44};//共享搜索配对公共地址
static u8 tws_audio_share_addr[6];

static u8 a2dp_share_preempted_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

struct share_info_t share_info;


//获取共享设备和手机的连接状态
int get_share_phone_conn_state()
{
    int role = tws_api_get_role();
    u8 connect_device = bt_get_total_connect_dev();
    if (role == TWS_ROLE_SLAVE) {
        return SHARE_TWS_SLAVE;
    }
    if (connect_device == 0) {
        return SHARE_DISCON_PHONE_DISCONN;
    } else if (connect_device == 1) {
        if (bt_check_is_have_tws_share_dev_conn()) {
            return SHARE_CONN_PHONE_DISCONN;
        } else {
            return SHARE_DISCON_PHONE_1_CONN;
        }
    } else if (connect_device == 2) {
        if (bt_check_is_have_tws_share_dev_conn()) {
            return SHARE_CONN_PHONE_1_CONN;
        } else {
            return SHARE_DISCON_PHONE_2_CONN;

        }

    }
    return 0;
}
bool check_open_share_wait_page_scan()
{
    if (g_share_handle.share_page_scan_open) {
        return 1;
    }
    return 0;
}
bool check_have_rec_share_device()
{
    if (g_share_handle.rec_share_device_flag) {
        return 1;
    }
    return 0;
}
void set_rec_share_device(u8 rec_share_dev)
{
    g_share_handle.rec_share_device_flag = rec_share_dev;
    r_printf("set_rec_share_device=%d\n", g_share_handle.rec_share_device_flag);

}

static void close_share_scan(void *p)
{
    g_printf("close_share_scan\n");
    g_share_handle.share_page_scan_open = 0;
    g_share_handle.share_conn_timeout = 0;
    bt_tws_audio_share_wait_pair(0);
}

//开启共享连接，快速连接用的地址
u8 *bt_get_share_audio_pair_addr()
{
    r_printf("bt_get_share_audio_pair_addr");
    put_buf(tws_audio_share_addr, 6);
    return tws_audio_share_addr;
}

//设置共享连接地址
void set_share_audio_pair_addr(u8 *addr)
{
    r_printf("set_share_audio_pair_addr");
    if (addr) {//有设备记录用  remote_mac+local_mac 共享连接地址
        puts("remote mac addr\n");
        put_buf(addr, 6);
        memcpy(tws_audio_share_addr, addr, 6);
        const u8 *local_mac = bt_get_mac_addr();
        for (int i = 0; i < 6; i++) {
            tws_audio_share_addr[i] += local_mac[i];
        }
    } else {//公共地址+名字+tws code 共享搜索配对
        u16 crc = CRC16(bt_get_local_name(), strlen(bt_get_local_name()));
        u16 tws_code = bt_get_tws_device_indicate(NULL);
        u16 pair_code = crc + tws_code;
        for (int i = 0; i < 6; i++) {
            tws_audio_share_addr[i] = tws_audio_share_pair_addr[i] + pair_code;
        }
    }
    put_buf(tws_audio_share_addr, 6);

}

//保持共享连接地址到vm
void bt_tws_audio_save_share_addr(u8 *share_addr, u8 role)
{
    r_printf("bt_tws_audio_save_share_addr=%d\n", role);
    put_buf(share_addr, 6);
    syscfg_read(CFG_TWS_CONNECT_AA, &share_info, sizeof(struct share_info_t));
    if (memcmp(share_info.save_addr, share_addr, 6) || share_info.role != role) {
        puts("update remote share_addr\n");
        memcpy(share_info.save_addr, share_addr, 6);
        share_info.role = role;
        set_share_audio_pair_addr(share_addr);
        syscfg_write(CFG_TWS_CONNECT_AA, &share_info, sizeof(struct share_info_t));
    }
}

//根据地址判断当前设备是否共享设备
u8 check_is_share_addr_for_vm(u8 *addr)
{
    int size = sizeof(struct share_info_t);
    int ret = syscfg_read(CFG_TWS_CONNECT_AA, &share_info, size);
    if ((ret == size) && (memcmp(share_info.save_addr, addr, 6) == 0)) {
        return 1;
    }
    return 0;
}

//根据地址获取当前觉色是共享主机还是共享从机
int get_share_role_for_vm(u8 *addr)
{
    int size = sizeof(struct share_info_t);
    int ret = syscfg_read(CFG_TWS_CONNECT_AA, &share_info, size);
    if (ret == size && memcmp(share_info.save_addr, addr, 6) == 0) {
        return share_info.role;
    }
    return -1;
}

//从vm获取共享设备地址
u8 *get_share_addr_for_vm()
{
    int size = sizeof(struct share_info_t);
    int ret = syscfg_read(CFG_TWS_CONNECT_AA, &share_info, size);
    if (ret == size) {
        return (u8 *)(share_info.save_addr);
    }
    return NULL;
}

//根据地址获取共享设备 info，共享主:开启回连，共享从:开启page_scan等待连接
static int bt_tws_share_device_disconn_deal(u8 reason, u8 *addr)
{
    r_printf("bt_tws_share_device_disconn_deal=%x", reason);
    if (addr && check_is_share_addr_for_vm(addr)) { //断开连接链路已断开，根据地址从vm获取当前是否share dev
        int share_role = get_share_role_for_vm(addr);
        set_share_audio_pair_addr(addr);
        g_share_handle.share_page_scan_open = 0;
        if (g_share_handle.share_conn_timeout) {
            sys_timeout_del(g_share_handle.share_conn_timeout);
            g_share_handle.share_conn_timeout = 0;
        }
        if (share_role == TWS_ROLE_MASTER) {
            add_device_2_page_list(bt_get_share_audio_pair_addr(), TCFG_BT_SHARE_PAGE_CONN_TIMEOUT * 1000, PAGE_DEV_SHARE);
        } else {
            g_share_handle.share_page_scan_open = 1;
            g_share_handle.share_conn_timeout = sys_timeout_add(NULL, close_share_scan, TCFG_BT_SHARE_PAGE_CONN_TIMEOUT * 1000);
        }
        return 1;
    }
    return 0;
}

static void share_role_switch_timeout(void *p)
{
    r_printf("share_role_switch_timeout");
    del_device_type_from_page_list(PAGE_DEV_SHARE);
    g_share_handle.share_conn_timeout = 0;
    bt_tws_share_device_disconn_deal(ERROR_CODE_CONNECTION_TIMEOUT, get_share_addr_for_vm());
    tws_dual_conn_state_handler();

}
static int bt_tws_share_device_disconn_role_switch_deal(u8 reason, u8 *addr)
{
    r_printf("bt_tws_share_device_disconn_role_switch_deal=%x", reason);
    if (check_is_share_addr_for_vm(addr)) {//断开连接链路已断开，根据地址从vm获取当前是否share dev
        int share_role = get_share_role_for_vm(addr);
        if (reason == ERROR_CODE_SHARE_ROLE_SWITCH) {
            if (share_role == TWS_ROLE_MASTER) {
                share_role = TWS_ROLE_SLAVE;
            } else {
                share_role = TWS_ROLE_MASTER;
            }
        }
        g_share_handle.share_page_scan_open = 0;
        set_share_audio_pair_addr(addr);
        if (g_share_handle.share_conn_timeout) {
            sys_timeout_del(g_share_handle.share_conn_timeout);
            g_share_handle.share_conn_timeout = 0;
        }
        if (share_role == TWS_ROLE_MASTER) {

            //share page 放到回连链表头,发起share的page连接
            add_device_2_page_list(bt_get_share_audio_pair_addr(), 16 * 1000, PAGE_DEV_SHARE);
            lmp_hci_write_scan_enable((0 << 1) | 0);
            dual_conn_page_device_phone_dev_timeout(PAGE_DEV_PHONE);
            dual_conn_page_device();

        } else {
            g_share_handle.share_page_scan_open = 1;
            lmp_hci_write_scan_enable((0 << 1) | 0);
            bt_tws_audio_share_wait_page_scan();
        }
        g_share_handle.share_conn_timeout = sys_timeout_add(NULL, share_role_switch_timeout, 16 * 1000);
        return 1;
    }
    return 0;
}

//开机初始化从vm获取共享设备 info，共享主:开启回连，共享从:开启page_scan等待连接
int bt_tws_share_device_init(u8 open_master, u8 open_slave)
{
    u8 *vm_share_addr = get_share_addr_for_vm();

    r_printf("bt_tws_share_device_init=%x,%d", vm_share_addr, check_have_rec_share_device());
    if (vm_share_addr) {
#if (TCFG_TWS_SHARE_2PHONE_CONN_ENABLE)
        if (check_have_rec_share_device() == 0) {
            return 0;
        }
#endif
        int share_role = get_share_role_for_vm(vm_share_addr);
        set_share_audio_pair_addr(vm_share_addr);
        r_printf("vm share_role=%d\n", share_role);
        if (g_share_handle.share_conn_timeout) {
            sys_timeout_del(g_share_handle.share_conn_timeout);
            g_share_handle.share_conn_timeout = 0;
        }
        if (share_role == TWS_ROLE_MASTER) {
            if (open_master) {
                add_device_2_page_list(bt_get_share_audio_pair_addr(), TCFG_BT_SHARE_PAGE_CONN_TIMEOUT * 1000, PAGE_DEV_SHARE);

            }
        } else {
            if (open_slave) {
                g_share_handle.share_page_scan_open = 1;
                g_share_handle.share_conn_timeout = sys_timeout_add(NULL, close_share_scan, TCFG_BT_SHARE_PAGE_CONN_TIMEOUT * 1000);

            }
        }
        return 1;
    }
    return 0;
}


//等待共享配对用记忆地址,进行被连接
static void bt_tws_audio_share_wait_page_scan()
{
    lmp_hci_write_quick_scan_enable(0);
    lmp_hci_write_quick_scan_enable(1);
}
void bt_tws_audio_share_search_pair_enable()
{
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        puts("slave return\n");
        return;
    }
    if (bt_check_is_have_tws_share_dev_conn()) {
        puts("bt_check_is_have_tws_share_dev_conn\n");
        return;
    }
    /* lmp_hci_write_quick_page_enable(1); */
    lmp_hci_write_quick_scan_enable(0);
    bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
    tws_play_tone_file(get_tone_files()->share_search_pairing, 400);
    del_device_type_from_page_list(PAGE_DEV_SHARE);
    set_share_audio_pair_addr(NULL);

    //share page 放到回连链表头,发起share的page连接
    add_device_2_page_list(bt_get_share_audio_pair_addr(), TCFG_BT_SHARE_PAGE_TIMEOUT * 1000, PAGE_DEV_SHARE);
    dual_conn_page_device_phone_dev_timeout(PAGE_DEV_PHONE);
    dual_conn_page_device();

}
void check_bt_tws_audio_share_search_pair(void *priv)
{
    int share_state = get_share_phone_conn_state();
    if (share_state != SHARE_DISCON_PHONE_2_CONN) {
        bt_tws_audio_share_search_pair_enable();
        g_share_handle.share_conn_timeout = 0;
    } else {
        if (g_share_handle.share_conn_timeout) {
            sys_timer_modify(g_share_handle.share_conn_timeout, 200);
        }
    }
}

//共享搜索配对，用公共地址，根据名字匹配进行
void bt_tws_audio_share_search_pair_start(u8 enable)
{
    r_printf("bt_tws_audio_share_search_pair_start=%d\n", enable);
    if (g_share_handle.share_conn_timer) {
        sys_timeout_del(g_share_handle.share_conn_timer);
        g_share_handle.share_conn_timer = 0;
    }
    if (enable) {
#if TCFG_TWS_SHARE_2PHONE_CONN_ENABLE
        int share_state = get_share_phone_conn_state();
        if (share_state == SHARE_DISCON_PHONE_2_CONN) {
            u8 work_addr[6] = {0x0};
            if (a2dp_player_get_btaddr(work_addr)) {

            } else if (esco_player_get_btaddr(work_addr)) {

            }
            bt_cmd_prepare_for_addr(btstack_get_other_dev_addr(work_addr), USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            g_share_handle.share_conn_timer = sys_timeout_add(NULL, check_bt_tws_audio_share_search_pair, 200);


        }
#else
        bt_tws_audio_share_search_pair_enable();
#endif
    } else {

        del_device_type_from_page_list(PAGE_DEV_SHARE);
        bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        /* lmp_hci_write_quick_page_enable(0); */

    }
}

void bt_tws_audio_share_wait_pair_enable()
{
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    if (bt_check_is_have_tws_share_dev_conn()) {
        return;
    }
    set_share_audio_pair_addr(NULL);
    lmp_hci_write_quick_scan_enable(0);
    tws_play_tone_file(get_tone_files()->share_wait_pairing, 400);
    g_share_handle.share_page_scan_open = 1;
    g_share_handle.share_conn_timeout = sys_timeout_add(NULL, close_share_scan, TCFG_BT_SHARE_PAGE_TIMEOUT * 1000);

}
void check_bt_tws_audio_share_wait_pair(void *priv)
{
    int share_state = get_share_phone_conn_state();
    printf("check_bt_tws_audio_share_wait_pair=%d\n", share_state);
    if (share_state != SHARE_DISCON_PHONE_2_CONN) {
        bt_tws_audio_share_wait_pair_enable();
        g_share_handle.share_conn_timeout = 0;
    } else {
        if (g_share_handle.share_conn_timeout) {
            sys_timer_modify(g_share_handle.share_conn_timeout, 200);
        }
    }
}
//等待共享配对用公共地址，根据名字匹配进行被连接
void bt_tws_audio_share_wait_pair(u8 enable)
{
    r_printf("bt_tws_audio_share_wait_pair=%d,share_conn_timeout=%d\n", enable, g_share_handle.share_conn_timeout);
    g_share_handle.share_page_scan_open = 0;
    if (g_share_handle.share_conn_timeout) {
        sys_timeout_del(g_share_handle.share_conn_timeout);
        g_share_handle.share_conn_timeout = 0;
    }
    if (g_share_handle.share_conn_timer) {
        sys_timeout_del(g_share_handle.share_conn_timer);
        g_share_handle.share_conn_timer = 0;
    }
    if (enable) {
#if TCFG_TWS_SHARE_2PHONE_CONN_ENABLE
        int share_state = get_share_phone_conn_state();
        if (share_state == SHARE_DISCON_PHONE_2_CONN) {
            u8 work_addr[6] = {0x0};
            if (a2dp_player_get_btaddr(work_addr)) {
            } else if (esco_player_get_btaddr(work_addr)) {
            }
            bt_cmd_prepare_for_addr(btstack_get_other_dev_addr(work_addr), USER_CTRL_DISCONNECTION_HCI, 0, NULL);
            g_share_handle.share_conn_timer = sys_timeout_add(NULL, check_bt_tws_audio_share_wait_pair, 200);
        }
#else
        bt_tws_audio_share_wait_pair_enable();
#endif
    }
    lmp_hci_write_quick_scan_enable(enable);

}

//共享主机，控制共享音频暂停和播放
void bt_tws_master_audio_share_control(u8 enable)
{
    if (!bt_check_is_have_tws_share_dev_conn()) {
        return;
    }
    if (enable) {
        bt_cmd_prepare(USER_CTRL_ATWS_AUDIO_SHARE_CMD_START, 0, NULL);
    } else {
        bt_cmd_prepare(USER_CTRL_ATWS_AUDIO_SHARE_CMD_SUSPEND, 0, NULL);
    }
}

//tws从机 rx date deal
static void bt_sync_tws_slave_data_func_in_irq(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    switch (data[0]) {
    case DATA_ID_SHARE_A2DP_PREEMPTED_ADDR:
        memcpy(a2dp_share_preempted_addr, data + 1, 6);
        break;
    case DATA_ID_SHARE_TWS_ROLE_SWITCH_OPEN_PAGE_SCAN_ADDR:
        g_share_handle.share_page_scan_open = 1;
        memcpy(tws_audio_share_addr, data + 1, 6);
        g_share_handle.share_conn_timeout = sys_timeout_add(NULL, close_share_scan, TCFG_BT_SHARE_PAGE_CONN_TIMEOUT * 1000);
        tws_dual_conn_state_handler();
        break;
    }
}
REGISTER_TWS_FUNC_STUB(bt_sync_tws_data_stub) = {
    .func_id = BT_SYNC_TWS_DATA_FUNC_ID,
    .func    = bt_sync_tws_slave_data_func_in_irq,
};

//同步cmd 到tws从机
static void bt_share_slave_tx_data_to_tws_slave(u8 data_id)
{
    u8 data[16];
    data[0] = data_id;
    switch (data_id) {
    case DATA_ID_SHARE_A2DP_PREEMPTED_ADDR:
        memcpy(data + 1, a2dp_share_preempted_addr, 6);
        tws_api_send_data_to_sibling(data, 7, BT_SYNC_TWS_DATA_FUNC_ID);
        break;
    case DATA_ID_SHARE_TWS_ROLE_SWITCH_OPEN_PAGE_SCAN_ADDR:
        memcpy(data + 1, tws_audio_share_addr, 6);
        tws_api_send_data_to_sibling(data, 7, BT_SYNC_TWS_DATA_FUNC_ID);
        break;
    }
}

//set 共享从机a2dp播歌被打断 flag
void share_a2dp_slave_set_preempted_flag(u8 *share_addr)
{
#if TCFG_TWS_AUDIO_SHARE_AUTO_SWITCH_ENABLE==0
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    if (is_slave_device_tws_share_conn(share_addr)) {
        r_printf("share_a2dp_slave_set_preempted_flag");
        put_buf(share_addr, 6);
        memcpy(a2dp_share_preempted_addr, share_addr, 6);
        bt_share_slave_tx_data_to_tws_slave(DATA_ID_SHARE_A2DP_PREEMPTED_ADDR);
    }
#endif
}

//share a2dp paly clear 共享从机a2dp播歌被打断 flag
void share_a2dp_slave_clr_preempted_flag(u8 *share_addr, u8 disconn)
{
#if TCFG_TWS_AUDIO_SHARE_AUTO_SWITCH_ENABLE==0
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    if (is_slave_device_tws_share_conn(share_addr) || disconn) {
        if (memcmp(a2dp_share_preempted_addr, share_addr, 6) == 0) {
            memset(a2dp_share_preempted_addr, 0xff, 6);
            r_printf("share_a2dp_slave_clr_preempted_flag");
            put_buf(share_addr, 6);
            bt_share_slave_tx_data_to_tws_slave(DATA_ID_SHARE_A2DP_PREEMPTED_ADDR);
        }
    }
#endif
}

//phone a2dp stop 共享从a2dp播放打断恢复播放
bool share_a2dp_preempted_resume(u8 *phone_addr)
{
#if TCFG_TWS_AUDIO_SHARE_AUTO_SWITCH_ENABLE==0
    u8 all_ff_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
    if (memcmp(a2dp_share_preempted_addr, all_ff_addr, 6) &&
        memcmp(a2dp_share_preempted_addr, phone_addr, 6)) {
        u8 preempted_addr_is_share = is_slave_device_tws_share_conn(a2dp_share_preempted_addr);
        put_buf(phone_addr, 6);
        put_buf(a2dp_share_preempted_addr, 6);
        u8 cur_addr_is_share = is_slave_device_tws_share_conn(phone_addr);
        printf("preempted_addr_is_share=%d,cur_addr_is_share=%d\n", preempted_addr_is_share, cur_addr_is_share);
        if (preempted_addr_is_share && !cur_addr_is_share) {
            r_printf("share_a2dp_preempted_resume ok\n");
            bt_cmd_prepare_for_addr(a2dp_share_preempted_addr, USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
            memset(a2dp_share_preempted_addr, 0xff, 6);
            bt_share_slave_tx_data_to_tws_slave(DATA_ID_SHARE_A2DP_PREEMPTED_ADDR);
            return 1;
        }
    }
#endif
    return 0;
}

//共享主机切换成共享从机
int share_a2dp_slave_switch_to_share_master()
{
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
    u8 *share_addr = get_tws_share_dev_addr();
    if (is_slave_device_tws_share_conn(share_addr)) {
        u8 *phone_mac_addr = btstack_get_other_dev_addr(share_addr);
        u8 a2dp_start = 0;
        if (phone_mac_addr) {
            u8 btaddr[6];
            if (a2dp_player_get_btaddr(btaddr) && memcmp(phone_mac_addr, btaddr, 6) == 0) {
                a2dp_start = 1;
            }
        }
        r_printf("share_slave_switch_to_share_master a2dp_start=%d", a2dp_start);
        bt_share_tx_data_to_other_share(DATA_ID_SHARE_A2DP_SLAVE_SWITCH_TO_SHARE_MASTER, a2dp_start);
        return 1;

    }
    return 0;
}

//共享从机手机播歌自动切换成共享主机进行分享
int check_phone_a2dp_play_share_auto_slave_to_master(u8 *bt_addr)
{
#if TCFG_TWS_AUDIO_SHARE_AUTO_SWITCH_ENABLE
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0 ;
    }
    u8 *share_addr = get_tws_share_dev_addr();
    if (share_addr && is_slave_device_tws_share_conn(share_addr) && memcmp(bt_addr, share_addr, 6)) {
        r_printf("share_a2dp_slave_switch_to_share_master");
        bt_share_tx_data_to_other_share(DATA_ID_SHARE_A2DP_SLAVE_SWITCH_TO_SHARE_MASTER, 1);
        return 1;
    }
#endif
    return 0;
}


//共享主机收到共享从机avctp控制命令处理,控制共享链路本身,
//与1t2情景抢断命令处理AVCTP_OPID_PLAY/AVCTP_OPID_PAUSE
void share_rx_avctp_opid_deal(u8 *addr, u8 cmd)
{
    u8 is_share_master = is_master_device_tws_share_conn(addr);
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
    int state = bt_get_call_status();
    if (state != BT_CALL_HANGUP) {
        puts("call ing return\n");
        return;
    }
    if (is_share_master) {
        r_printf("share_master_rx_avctp_opid_deal=:%d\n", cmd);
        switch (cmd) {
        case AVCTP_OPID_PLAY:
            puts("AVCTP_OPID_PLAY\n");
            bt_tws_master_audio_share_control(1);//共享链路本身播放
            break;
        case AVCTP_OPID_PAUSE:
            puts("AVCTP_OPID_PAUSE\n");
            bt_tws_master_audio_share_control(0);//共享链路本身暂停
            break;


        }
    }
}

void bt_tws_share_function_select_init()
{
    bt_a2dp_source_init(NULL, 0, 1);
    set_donlge_quick_sniff_run_slot(200);
    memset(&g_share_handle, 0, sizeof(g_share_handle));


}
void bt_tws_share_function_select_close()
{
    bt_a2dp_source_init(NULL, 0, 0);
}

//tws共享连接成功和断开提示音播放
int bt_tws_share_connect_disconn_tone_play(u8 connect, u8 *bt_addr)
{
    if (connect) {
        if (is_master_device_tws_share_conn(bt_addr)) {
            g_share_handle.share_slave_to_master_step = 0;
            tws_play_tone_file(get_tone_files()->share_conn_master, 400);
            return 1;
        } else if (is_slave_device_tws_share_conn(bt_addr)) {
            g_share_handle.share_slave_to_master_step = 0;
            tws_play_tone_file(get_tone_files()->share_conn_slave, 400);
            return 1;
        }

    } else {
        if (check_is_share_addr_for_vm(bt_addr)) {
            if (g_share_handle.share_slave_to_master_step) {
                return 1;
            }
            tws_play_tone_file(get_tone_files()->share_disconnect, 400);
            return 1;
        }

    }
    return 0;
}
static void bt_sync_share_to_share_data_func_in_irq(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    u8 *share_addr = get_tws_share_dev_addr();
    u8 *phone_mac_addr = NULL;
    if (share_addr) {
        phone_mac_addr = btstack_get_other_dev_addr(share_addr);
    }
    g_printf("bt_sync_share_to_share_data_func_in_irq=%d,%d,%d", rx, data[0], len);
    put_buf(data, len);
    if (rx) {
        switch (data[0]) {
        case DATA_ID_SHARE_TO_SHARE_PP:
            puts("DATA_ID_SHARE_TO_SHARE_PP\n");
            if (phone_mac_addr) {
                //共享从控制共享主机的手机暂停和播放
                bt_cmd_prepare_for_addr(phone_mac_addr, USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
            }
            break;
        case DATA_ID_SHARE_TO_SHARE_NEXT:
            puts("DATA_ID_SHARE_TO_SHARE_NEXT\n");
            if (phone_mac_addr) {
                //共享从控制共享主机的手机NEXT
                bt_cmd_prepare_for_addr(phone_mac_addr, USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
            }
            break;
        case DATA_ID_SHARE_TO_SHARE_PREV:
            puts("DATA_ID_SHARE_TO_SHARE_PREV\n");
            if (phone_mac_addr) {
                //共享从控制共享主机的手机PREV
                bt_cmd_prepare_for_addr(phone_mac_addr, USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
            }
            break;

        }

    }
    if (data[0] == DATA_ID_SHARE_A2DP_SLAVE_SWITCH_TO_SHARE_MASTER) {
        r_printf("DATA_ID_SHARE_A2DP_SLAVE_SWITCH_TO_SHARE_MASTER=%d\n", data[1]);
        g_share_handle.share_slave_to_master_step = SHARE_SLAVE_TO_MASTER_STEP_START;
        u8 btaddr[6];
        if (data[1] && phone_mac_addr &&
            a2dp_player_get_btaddr(btaddr) && is_master_device_tws_share_conn(share_addr)) {
            bt_cmd_prepare_for_addr(phone_mac_addr, USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
        }

    }
}

REGISTER_TWS_FUNC_STUB(bt_sync_shate_data_stub) = {
    .func_id = BT_SYNC_SHARE_TO_SHARE_DATE_FUNC_ID,
    .func    = bt_sync_share_to_share_data_func_in_irq,
};

//共享设备之间发送命令控制
void bt_share_tx_data_to_other_share(u8 cmd, u8 value)
{
    u8 data[9];//max len 9
    data[0] = cmd;
    data[1] = value;
    lmp_hci_send_share_user_date(data, 9, BT_SYNC_SHARE_TO_SHARE_DATE_FUNC_ID);
}

//共享主机同步手机音量给共享从机,user avctp conn control
void bt_tws_share_master_sync_vol_to_share_slave()
{
    u8 *addr = get_tws_share_dev_addr();
    if (addr && is_master_device_tws_share_conn(addr)) {
        bt_cmd_prepare_for_addr(addr, USER_CTRL_AVCTP_OPID_SEND_VOL, 0, NULL);
    }

}


static int dual_conn_btstack_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;
    int state = tws_api_get_tws_state();

    switch (event->event) {
    case BT_STATUS_FIRST_CONNECTED:
    case BT_STATUS_SECOND_CONNECTED:
        if (check_is_share_addr_for_vm(event->args)) {
            bt_tws_audio_share_wait_pair(0);
            g_share_handle.share_page_scan_open = 0;
            if (g_share_handle.share_conn_timeout) {
                sys_timeout_del(g_share_handle.share_conn_timeout);
                g_share_handle.share_conn_timeout = 0;
            }
        }
        int share_state = get_share_phone_conn_state();
        if (share_state == SHARE_DISCON_PHONE_1_CONN) {
            if (g_share_handle.share_page_scan_open) {
                bt_tws_audio_share_wait_page_scan();
            }
        } else if (share_state == SHARE_CONN_PHONE_DISCONN) {
            write_scan_conn_enable(1, 1);
        } else if (share_state == SHARE_DISCON_PHONE_2_CONN) {

        }
        break;
    case BT_STATUS_AVRCP_INCOME_OPID:
        share_rx_avctp_opid_deal(event->args, event->value);
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
        share_a2dp_preempted_resume(event->args);
        share_a2dp_slave_clr_preempted_flag(event->args, 1);
        break;
    }
    return 0;

}
static int dual_share_hci_event_handler(int *_event)
{
    struct bt_event *event = (struct bt_event *)_event;

    if (app_var.goto_poweroff_flag) {
        return 0;
    }
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return 0;
    }
    switch (event->event) {
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        switch (event->value) {
        case ERROR_CODE_SHARE_ROLE_SWITCH:
            if (g_share_handle.share_slave_to_master_step == SHARE_SLAVE_TO_MASTER_STEP_START) {
                bt_tws_share_device_disconn_role_switch_deal(ERROR_CODE_SHARE_ROLE_SWITCH, event->args);
                g_share_handle.share_slave_to_master_step = SHARE_SLAVE_TO_MASTER_STEP_DISCONN;
            }
            break;
        }
        break;
    }
    return 0;
}
static int share_conn_tws_event_handler(int *_event)
{
    struct tws_event *event = (struct tws_event *)_event;
    int role = event->args[0];
    int reason = event->args[2];

    switch (event->event) {
    case TWS_EVENT_CONNECTED:
        if (app_var.goto_poweroff_flag) {
            break;
        }
        if (role == TWS_ROLE_MASTER) {
            if (!bt_check_is_have_tws_share_dev_conn()) {
                bt_tws_share_device_init(0, 1);
                if (page_list_empty()) {
                    tws_dual_conn_state_handler();
                }
            }
        } else {
            bt_tws_audio_share_search_pair_start(0);
            bt_tws_audio_share_wait_pair(0);
        }
        break;
    case TWS_EVENT_CONNECTION_DETACH:
        if (app_var.goto_poweroff_flag) {
            break;
        }
        if (!bt_check_is_have_tws_share_dev_conn()) {
            bt_tws_share_device_init(0, 1);
            tws_dual_conn_state_handler();
        }
        break;
    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_SLAVE) {
            g_printf("TWS_EVENT_ROLE_SWITCH share_page_scan_open=%d\n", g_share_handle.share_page_scan_open);
            if (g_share_handle.share_page_scan_open) {

                bt_share_slave_tx_data_to_tws_slave(DATA_ID_SHARE_TWS_ROLE_SWITCH_OPEN_PAGE_SCAN_ADDR);
                bt_tws_audio_share_wait_pair(0);

            }

        }
    }
    return 0;
}
APP_MSG_HANDLER(share_conn_tws_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TWS,
    .handler    = share_conn_tws_event_handler,
};
APP_MSG_HANDLER(dual_share_stack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = dual_conn_btstack_event_handler,
};
APP_MSG_HANDLER(dual_share_hci_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_HCI,
    .handler    = dual_share_hci_event_handler,
};
#endif
#endif
