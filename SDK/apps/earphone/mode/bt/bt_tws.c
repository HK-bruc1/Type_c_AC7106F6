#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".bt_tws.data.bss")
#pragma data_seg(".bt_tws.data")
#pragma const_seg(".bt_tws.text.const")
#pragma code_seg(".bt_tws.text")
#endif
#include "system/includes.h"
#include "media/includes.h"
#include "device/vm.h"
#include "app_tone.h"
#include "a2dp_player.h"

#include "app_config.h"
#include "earphone.h"

#include "btstack/avctp_user.h"
#include "classic/hci_lmp.h"
#include "user_cfg.h"
#include "bt_tws.h"
#include "asm/charge.h"
#include "app_charge.h"

#include "app_testbox.h"
#include "app_online_cfg.h"
#include "app_main.h"
#include "app_power_manage.h"
#include "battery_manager.h"
#include "audio_config.h"
#include "bt_slience_detect.h"
#include "esco_recoder.h"
#include "bt_common.h"
#include "tws_dual_conn.h"
#include "update.h"
#include "in_ear_detect/in_ear_manage.h"
#include "multi_protocol_main.h"
#include "phone_call.h"
#include "btstack_rcsp_user.h"
#include "local_tws.h"

#include "multi_protocol_main.h"
#include "audio_anc_includes.h"

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "app_le_connected.h"
#endif

#if (TCFG_USER_TWS_ENABLE && TCFG_APP_BT_EN)

#define LOG_TAG             "[BT-TWS]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define    CONFIG_BT_TWS_SNIFF                  1       // TWS通讯允许进入sniff


struct tws_user_var {
    u16 state;				// TWS状态
    u16 sniff_timer;		// sniff检查进入定时器
};

struct tws_user_var  gtws;

/**
 * @brief 获取TWS连接状态
 *
 * @return 如BT_TWS_UNPAIRED
 */
u16 bt_tws_get_state(void)
{
    return gtws.state;
}

/**
 * @brief 主从同步调用函数处理
 */
static void tws_sync_call_fun(int cmd, int err)
{
    log_d("TWS_EVENT_SYNC_FUN_CMD: %d\n", cmd);

    switch (cmd) {
    case SYNC_CMD_EARPHONE_CHAREG_START:
        if (bt_a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
        break;
    case SYNC_CMD_IRSENSOR_EVENT_NEAR:
        if (bt_a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
        break;
    case SYNC_CMD_IRSENSOR_EVENT_FAR:
        if (bt_a2dp_get_status() == BT_MUSIC_STATUS_STARTING) {
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
        }
        break;
    }
}

TWS_SYNC_CALL_REGISTER(tws_tone_sync) = {
    .uuid = 'T',
    .task_name = "app_core",
    .func = tws_sync_call_fun,
};

#if TCFG_TWS_POWER_BALANCE_ENABLE
/**
 * @brief TWS底层获取电池电量信息
 *
 * @return 当前设备电量信息
 */
u16 tws_host_get_battery_voltage()
{
    return get_vbat_value();
}

/**
 * @brief TWS底层使用电量大的做主机
 *
 * @return 1:进行主从切换
 */
bool tws_host_role_switch_by_power_balance(u16 m_voltage, u16 s_voltage)
{
    if (m_voltage + 100 <= s_voltage) {
        return 1;
    }
    return 0;
}

/**
 * @brief TWS底层判断电量检测主从切换的间隔时间
 *
 * @return int 间隔时间
 */
int tws_host_role_switch_by_power_update_time()
{
    return (60 * 1000);
}
#endif

/**
 * @brief TWS底层判断本地与远端设备声道是否相同
 *
 * @return 1:不相同;0:相同
 */
int tws_host_channel_match(char remote_channel)
{
    /*r_printf("tws_host_channel_match: %c, %c\n", remote_channel,
             bt_tws_get_local_channel());*/
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_LEFT || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_RIGHT || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_RIGHT || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_CHANNEL_SELECT_BY_BOX
    return 1;
#else
    if (remote_channel != bt_tws_get_local_channel()) {
        return 1;
    }
#endif

    return 0;
}

/**
 * @brief TWS底层获取芯片版本
 *
 * 注意：不允许删除本函数
 */
u8 get_tws_soft_version()
{
    u8 soft_version = 0x20;
    u8 cpu_info = 0;
#if defined(CONFIG_CPU_BR28)
    cpu_info = 28;
#elif defined(CONFIG_CPU_BR36)
    cpu_info = 36;
#elif defined(CONFIG_CPU_BR27)
    cpu_info = 27;
#elif defined(CONFIG_CPU_BR50)
    cpu_info = 50;
#elif defined(CONFIG_CPU_BR52)
    cpu_info = 52;
#elif defined(CONFIG_CPU_BR56)
    cpu_info = 56;
#elif defined(CONFIG_CPU_BR42)
    cpu_info = 42;
#else
#error "not define cpu tws soft version"
#endif
    return (soft_version + cpu_info);
}

/**
 * @brief TWS底层获取本地声道信息
 *
 * @return 声道信息
 * 注意：不允许删除本函数
 */
char tws_host_get_local_channel()
{
    char channel;

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_RIGHT)
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 'R';
    } else {
        return 'L';
    }

#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_LEFT)
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 'L';
    } else {
        return 'R';
    }
#endif

    channel = bt_tws_get_local_channel();
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT
    if (channel != 'R') {
        channel = 'L';
    }
#else
    if (channel != 'L') {
        channel = 'R';
    }
#endif
    /*y_printf("tws_host_get_local_channel: %c\n", channel);*/

    return channel;
}

#if CONFIG_TWS_COMMON_ADDR_SELECT != CONFIG_TWS_COMMON_ADDR_AUTO
/**
 * @brief TWS底层获取tws公共地址
 *
 * @param remote_mac_addr 远端的蓝牙地址
 * @param common_addr 最后确定的共同地址
 * @param channel 声道信息
 */
void tws_host_get_common_addr(u8 *remote_mac_addr, u8 *common_addr, char channel)
{
#if CONFIG_TWS_COMMON_ADDR_SELECT == CONFIG_TWS_COMMON_ADDR_USED_LEFT
    if (channel == 'L') {
        memcpy(common_addr, bt_get_mac_addr(), 6);
    } else {
        memcpy(common_addr, remote_mac_addr, 6);
    }
#elif CONFIG_TWS_COMMON_ADDR_SELECT == CONFIG_TWS_COMMON_ADDR_USED_RIGHT
    if (channel == 'R') {
        memcpy(common_addr, bt_get_mac_addr(), 6);
    } else {
        memcpy(common_addr, remote_mac_addr, 6);
    }
#elif CONFIG_TWS_COMMON_ADDR_SELECT == CONFIG_TWS_COMMON_ADDR_USED_MASTER
    memcpy(common_addr, bt_get_mac_addr(), 6);
#endif
}
#endif

/**
 * @brief 设置TWS设备的搜索状态
 */
void tws_set_search_sbiling_state(u8 state)
{
    if (state) {
        gtws.state |= BT_TWS_SEARCH_SIBLING;
    } else {
        gtws.state &= ~BT_TWS_SEARCH_SIBLING;
    }
}

/**
 * @brief 获取TWS是否配对
 *
 * @return true:已配对 false:未配对
 */
bool bt_tws_is_paired()
{
    return gtws.state & BT_TWS_PAIRED;
}

/**
 * @brief 获取TWS对端设备地址
 *
 * @param addr 对端设备地址
 * @param result 获取成功会返回地址长度
 */
u8 tws_get_sibling_addr(u8 *addr, int *result)
{
    u8 all_ff[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    int len = syscfg_read(CFG_TWS_REMOTE_ADDR, addr, 6);
    if (len != 6 || !memcmp(addr, all_ff, 6)) {
        *result = len;
        return -ENOENT;
    }
    return 0;
}


/**
 * @brief 获取左右声道信息
 *
 * @return 'L': 左声道; 'R': 右声道; 'U': 未知
 */
char bt_tws_get_local_channel()
{
    char channel = 'U';

    syscfg_read(CFG_TWS_CHANNEL, &channel, 1);

    return channel;
}

/**
 * @brief 获取TWS是否已连接对端设备
 *
 * @return 1:已连接对端设备；0:未连接对端设备
 */
int get_bt_tws_connect_status()
{
    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        return 1;
    }

    return 0;
}

/**
 * @brief 根据代码配置或者硬件信息设置TWS本地channel
 */
static u8 set_channel_by_code_or_res(void)
{
    u8 count = 0;
    char channel = 0;
    char last_channel = 0;
#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_UP_AS_LEFT) || \
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_UP_AS_RIGHT)
    gpio_set_mode(IO_PORT_SPILT(CONFIG_TWS_CHANNEL_CHECK_IO), PORT_INPUT_PULLDOWN_10K);
    for (int i = 0; i < 5; i++) {
        os_time_dly(2);
        if (gpio_read(CONFIG_TWS_CHANNEL_CHECK_IO)) {
            count++;
        }
    }
    gpio_set_mode(IO_PORT_SPILT(CONFIG_TWS_CHANNEL_CHECK_IO), PORT_HIGHZ);
#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_UP_AS_LEFT)
    channel = (count >= 3) ? 'L' : 'R';
#else
    channel = (count >= 3) ? 'R' : 'L';
#endif
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_DOWN_AS_LEFT) || \
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_DOWN_AS_RIGHT)
    gpio_set_mode(IO_PORT_SPILT(CONFIG_TWS_CHANNEL_CHECK_IO), PORT_INPUT_PULLUP_10K);
    for (int i = 0; i < 5; i++) {
        os_time_dly(2);
        if (gpio_read(CONFIG_TWS_CHANNEL_CHECK_IO)) {
            count++;
        }
    }
    gpio_set_mode(IO_PORT_SPILT(CONFIG_TWS_CHANNEL_CHECK_IO), PORT_HIGHZ);
#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_DOWN_AS_LEFT)
    channel = (count >= 3) ? 'R' : 'L';
#else
    channel = (count >= 3) ? 'L' : 'R';
#endif
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_LEFT)
    channel = 'L';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_RIGHT)
    channel = 'R';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_CHANNEL_SELECT_BY_BOX)
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &channel, 1);
#endif

#if CONFIG_TWS_SECECT_CHARGESTORE_PRIO
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &channel, 1);
#endif

    if (channel) {
        syscfg_read(CFG_TWS_CHANNEL, &last_channel, 1);
        if (channel != last_channel) {
            syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        }
        tws_api_set_local_channel(channel);
        return 1;
    }
    return 0;
}
#if 0
/**
 * @brief a2dp播歌根据信号强度主从切换重写函数，可改变范围
 *
 * @param m_rssi 主机rssi
 * @param s_rssi 从机rssi
 */
int a2dp_role_switch_check_rssi(char master_rssi, char slave_rssi)
{
    if (master_rssi < -58 && slave_rssi > -70 && master_rssi + 12 <= slave_rssi) {
        return 1;
    }
    return 0;
}
#endif

#if 0
/**
 * @brief esco通话根据信号强度主从切换重写函数，可改变范围
 *
 * @param m_rssi 主机rssi
 * @param s_rssi 从机rssi
 */
bool tws_esco_rs_rssi_check(char m_rssi, char s_rssi)
{
    static char old_s_rssi, old_m_rssi;

    if (((old_s_rssi - s_rssi) < 20 && (old_s_rssi - s_rssi) > -20) && ((old_m_rssi - m_rssi) < 20 && (old_m_rssi - m_rssi) > -20)) {
        if (m_rssi < -60 && s_rssi > -70 && m_rssi + 25 <= s_rssi) {
            return TRUE;
        }
    }
    old_s_rssi = s_rssi;
    old_m_rssi = m_rssi;

    return FALSE;
}
#endif

/**
 * @brief TWS初始化
 */
int bt_tws_poweron()
{
    int err;
    u8 addr[6];
    char channel;

#if (TCFG_BT_BACKGROUND_ENABLE == 0)
    if (g_bt_hdl.wait_exit) { //非后台正在退出不进行初始化,底层资源已经释放
        return 0;
    }
#endif

#if TCFG_TEST_BOX_ENABLE
    if (testbox_get_ex_enter_dut_flag()) {
        log_info("tws poweron enter dut case\n");
        bt_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        bt_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        return 0;
    }
#endif

    /*支持ANC训练快速连接，不连接tws*/
#if TCFG_ANC_BOX_ENABLE && TCFG_AUDIO_ANC_ENABLE
    if (ancbox_get_status()) {
        bt_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        bt_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        return 0;
    }
#endif
    if (gtws.state) {
        return 0;
    }

#if TCFG_NORMAL_SET_DUT_MODE
    bt_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
    bt_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    return 0;
#endif

#if CONFIG_TWS_COMMON_ADDR_SELECT == CONFIG_TWS_COMMON_ADDR_USED_MASTER
    tws_api_common_addr_en(0);
#endif

    gtws.state = BT_TWS_POWER_ON;

    u16 pair_code = 0xAABB;
    syscfg_read(CFG_TWS_PAIR_CODE_ID, &pair_code, 2);
    printf("tws pair_code:0x%x\n", pair_code);
    tws_api_set_pair_code(pair_code);

#if TCFG_TWS_AUTO_ROLE_SWITCH_ENABLE
    tws_api_auto_role_switch_enable();
#else
    tws_api_auto_role_switch_disable();
#endif


#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CHIP_CONN
    tws_pair_by_chip_conn_init();
#endif

    int result = 0;
    err = tws_get_sibling_addr(addr, &result);
    if (err == 0) {
        /* 获取到对方地址, 开始连接 */
        printf("\n---------have tws info----------\n");
        put_buf(addr, 6);
        gtws.state |= BT_TWS_PAIRED;

        tws_api_set_sibling_addr(addr);

        if (set_channel_by_code_or_res() == 0) {
            channel = bt_tws_get_local_channel();
            tws_api_set_local_channel(channel);
        }

#if TCFG_TEST_BOX_ENABLE
        if (testbox_get_status()) {
            return 0;
        }
#endif
        u8 conn_addr[6];
        bt_get_tws_local_addr(conn_addr);
        for (int i = 0; i < 6; i++) {
            conn_addr[i] += addr[i];
        }
        tws_api_set_quick_connect_addr(conn_addr);
        put_buf(conn_addr, 6);
#if TCFG_TWS_POWERON_AUTO_PAIR_ENABLE
        app_send_message(APP_MSG_TWS_PAIRED, 0);
#endif
    } else {
        printf("\n ---------no tws info----------\n");
        gtws.state |= BT_TWS_UNPAIRED;
        if (set_channel_by_code_or_res() == 0) {
            tws_api_set_local_channel('U');
        }
#if TCFG_TEST_BOX_ENABLE
        if (testbox_get_status()) {
            return 0;
        }
#endif
#if TCFG_TWS_POWERON_AUTO_PAIR_ENABLE
        app_send_message(APP_MSG_TWS_UNPAIRED, 0);
#endif
    }

    return 0;
}

/**
 * @brief TWS配对成功
 *
 * @param remote_addr TWS远端地址
 */
void bt_tws_set_pair_suss(const u8 *remote_addr)
{
    u8 addr[6];
    int result = 0;
    int err = tws_get_sibling_addr(addr, &result);
    if (err == 0 && memcmp(remote_addr, addr, 6) == 0) {
        return;
    }
    syscfg_write(CFG_TWS_REMOTE_ADDR, remote_addr, 6);
    tws_api_set_sibling_addr((u8 *)remote_addr);

    u8 conn_addr[6];
    bt_get_tws_local_addr(conn_addr);
    for (int i = 0; i < 6; i++) {
        conn_addr[i] += remote_addr[i];
    }
    tws_api_set_quick_connect_addr(conn_addr);
    gtws.state |= BT_TWS_PAIRED;
#if TCFG_TWS_POWERON_AUTO_PAIR_ENABLE
    app_send_message(APP_MSG_TWS_PAIRED, 0);
#endif
}


/**
 * @brief 设置TWS设备连接手机状态信息
 */
int bt_tws_phone_connected()
{
    printf("bt_tws_phone_connected: %x\n", gtws.state);

#if TCFG_TEST_BOX_ENABLE
    if (testbox_get_status()) {
        return 1;
    }
#endif

    gtws.state |= BT_TWS_PHONE_CONNECTED;
    app_send_message(APP_MSG_BT_CLOSE_PAGE_SCAN, 0);
    return 0;
}

/**
 * @brief TWS进入dut测试使用
 */
void bt_page_scan_for_test(u8 inquiry_en)
{
    u8 local_addr[6];

    log_info("\n\n\n\n -------------bt test page scan\n");

    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);

    tws_api_detach(TWS_DETACH_BY_POWEROFF, 5000);

    bt_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);

    if (0 == bt_get_total_connect_dev()) {
        bt_get_vm_mac_addr(local_addr);
        lmp_hci_write_local_address(local_addr);
        if (inquiry_en) {
            bt_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        }
        bt_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    }

    sys_auto_shut_down_disable();
    sys_auto_shut_down_enable();

    gtws.state = 0;
}

/**
 * @brief 入仓没有连接edr需要主动调用ble主从切换
 * 功能：入仓关盖tws主从切换
 * 条件：入仓+没连edr+有ble连接+主机
 */
void soft_poweroff_tws_role_switch()
{
    if (!get_bt_tws_connect_status()) {
        printf("soft_poweroff_tws_role_switch, %d\n", __LINE__);
        return;
    }

    if (!get_charge_online_flag()) {
        printf("soft_poweroff_tws_role_switch, %d\n", __LINE__);
        return;
    }

    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        printf("soft_poweroff_tws_role_switch, %d\n", __LINE__);
        return;
    }

#if RCSP_MODE
    if (bt_rcsp_ble_conn_num() <= 0) {
        printf("soft_poweroff_tws_role_switch, %d\n", __LINE__);
        return;
    }
#endif

    /* 连上手机后，软关机流程会触发主从切 */
    // if (BT_STATUS_CONNECTING == bt_get_connect_status()) {
    //     return;
    // }

    printf("soft_poweroff_tws_role_switch\n");
    tws_api_role_switch();
}

/**
 * @brief TWS功能关闭退出
 */
int bt_tws_poweroff()
{
    log_info("bt_tws_poweroff\n");

#if TCFG_USER_BLE_ENABLE
    soft_poweroff_tws_role_switch();
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN | GFPS_EN | MMA_EN | FMNA_EN | REALME_EN | SWIFT_PAIR_EN | DMA_EN | ONLINE_DEBUG_EN | CUSTOM_DEMO_EN | XIMALAYA_EN | JL_SBOX_EN)) && !TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED
    multi_protocol_bt_tws_poweroff_handler();
#endif

    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        send_page_device_addr_2_sibling();
    }
    tws_api_detach(TWS_DETACH_BY_POWEROFF, 8000);

    tws_profile_exit();

    gtws.state = 0;

    if (tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED) {
        return 1;
    }

    return 0;
}

/**
 * @brief 解除配对，清掉对方地址信息和本地声道信息
 */
void bt_tws_remove_pairs()
{
    u8 mac_addr[6];

    gtws.state &= ~BT_TWS_PAIRED;
    gtws.state |= BT_TWS_UNPAIRED;

    memset(mac_addr, 0xFF, 6);
    syscfg_write(CFG_TWS_COMMON_ADDR, mac_addr, 6);
    syscfg_write(CFG_TWS_REMOTE_ADDR, mac_addr, 6);
    syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
    lmp_hci_write_local_address(mac_addr);

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_LEFT) ||\
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_RIGHT) ||\
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT) || \
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_RIGHT) && \
    CONFIG_TWS_SECECT_CHARGESTORE_PRIO
    char channel = 'U';
    char tws_channel = 0;
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &tws_channel, 1);
    if ((tws_channel != 'L') && (tws_channel != 'R')) {
        syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        tws_api_set_local_channel(channel);
    }
#endif
}


#define TWS_FUNC_ID_VOL_SYNC    TWS_FUNC_ID('V', 'O', 'L', 'S')

static void bt_tws_vol_sync_in_task(int vol)
{
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, vol & 0xff, 1);
    app_audio_set_volume(APP_AUDIO_STATE_CALL, vol >> 8, 1);
    r_printf("vol_sync: %d, %d\n", vol & 0xff, vol >> 8);
}

static void bt_tws_vol_sync_in_irq(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    if (rx) {
        int msg[3];
        msg[0] = (int)bt_tws_vol_sync_in_task;
        msg[1] = 1;
        msg[2] = data[0] | (data[1] << 8);
        os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    }
}
REGISTER_TWS_FUNC_STUB(app_vol_sync_stub) = {
    .func_id = TWS_FUNC_ID_VOL_SYNC,
    .func    = bt_tws_vol_sync_in_irq,
};

/**
 * @brief TWS同步播歌、通话音量
 */
void bt_tws_sync_volume()
{
    u8 data[2];

    data[0] = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    data[1] = app_audio_get_volume(APP_AUDIO_STATE_CALL);

    tws_api_send_data_to_slave(data, 2, TWS_FUNC_ID_VOL_SYNC);
}

#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
#include "spatial_effects_process.h"
#define TWS_FUNC_ID_SPATIAL_EFFECT_STATE_SYNC    TWS_FUNC_ID('A', 'S', 'E', 'S')
static void bt_tws_spatial_effect_state_sync(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        //r_printf("[slave]spatial_sync: %d, %d\n", data[0], data[1]);
        set_a2dp_spatial_audio_mode(data[0]);
    }
}

REGISTER_TWS_FUNC_STUB(app_spatial_effect_state_sync_stub) = {
    .func_id = TWS_FUNC_ID_SPATIAL_EFFECT_STATE_SYNC,
    .func    = bt_tws_spatial_effect_state_sync,
};

/**
 * @brief TWS同步空间音效状态
 */
void bt_tws_sync_spatial_effect_state(void)
{
    u8 data[2];
    data[0] = get_a2dp_spatial_audio_mode();
    data[1] = 0;
    //r_printf("[master]spatial_sync: %d, %d\n", data[0], data[1]);
    tws_api_send_data_to_slave(data, 2, TWS_FUNC_ID_SPATIAL_EFFECT_STATE_SYNC);
}
#endif

static void bt_tws_tx_jiffies_offset()
{
    int clkn = (jiffies_msec() + log_get_time_offset()) / 5 * 8;
    int offset = tws_api_get_mclkn() - clkn;
    tws_api_send_data_to_sibling(&offset, 4, 0x1E782CEB);
}

static void bt_tws_jiffies_sync(void *_data, u16 len, bool rx)
{
    if (!rx) {
        return;
    }

    int offset = *(int *)_data;
    int a_jiffies = (tws_api_get_mclkn() - offset) / 8 * 5;
    int b_jiffies = jiffies_msec() + log_get_time_offset();

    if (time_after(a_jiffies, b_jiffies)) {
        log_set_time_offset(a_jiffies - jiffies_msec());
    } else {
        bt_tws_tx_jiffies_offset();
    }
}
REGISTER_TWS_FUNC_STUB(jiffies_sync_stub) = {
    .func_id = 0x1E782CEB,
    .func    = bt_tws_jiffies_sync,
};

/**
 * @brief TWS底层获取来电状态信息
 *
 * @result 1:来电中;2:非来电
 */
int tws_host_get_phone_income_state()
{
    if (bt_get_call_status() == BT_CALL_INCOMING) {
        return 1;
    }
    return 0;
}

/**
 * @brief TWS状态消息处理函数
 *
 * @param msg TWS状态消息
 */
int bt_tws_connction_status_event_handler(int *msg)
{
    struct tws_event *evt = (struct tws_event *)msg;
    u8 addr[4][6];
    int pair_suss = 0;
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int reason = evt->args[2];
    u16 random_num = 0;
    char channel = 0;
    u8 mac_addr[6];
    int work_state = 0;

    if ((evt->event == TWS_EVENT_CONNECTED) || (evt->event == TWS_EVENT_ROLE_SWITCH) || (evt->event == TWS_EVENT_ESCO_ROLE_SWITCH_START)) {
        log_info("tws-user: role=%d, reason=%d, event=%d\n", role, reason, evt->event);
    } else if (evt->event == TWS_EVENT_CONNECTION_DETACH) {
        log_info("tws-user: phone_link_connection %d, reason=%d, event=TWS_EVENT_CONNECTION_DETACH\n",  phone_link_connection, reason);
    } else {
        log_info("tws-user: reason=%d, event=%d\n", reason, evt->event);
    }

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        log_info("tws_event_pair_suss: %x\n", gtws.state);

        g_bt_hdl.phone_ring_sync_tws = 0;
        syscfg_read(CFG_TWS_REMOTE_ADDR, addr[0], 6);
        syscfg_read(CFG_TWS_COMMON_ADDR, addr[1], 6);
        tws_api_get_sibling_addr(addr[2]);
        tws_api_get_local_addr(addr[3]);

        log_info("local tws addr\n");
        put_buf(addr[3], 6);
        /* 记录对方地址 */
        if (memcmp(addr[0], addr[2], 6)) {
            syscfg_write(CFG_TWS_REMOTE_ADDR, addr[2], 6);
            pair_suss = 1;
            log_info("rec tws addr\n");
            put_buf(addr[2], 6);
        }
        u8 comm_mac_addr_memcmp = 1;
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        u8 comm_mac_addr[12];
        int ret = syscfg_read(CFG_TWS_COMMON_ADDR, comm_mac_addr, 12);
        if (ret == 12) {
            r_printf("comm_mac_addr_memcmp\n");
            put_buf(comm_mac_addr, 12);
            put_buf(addr[3], 6);
            if (memcmp(&comm_mac_addr[6], addr[3], 6) == 0) {
                comm_mac_addr_memcmp = 0;
                lmp_hci_write_local_address(comm_mac_addr);
                g_printf("reset lmp_hci_write_local_address\n");
            }

        }
#endif
        if (comm_mac_addr_memcmp && memcmp(addr[1], addr[3], 6)) {
            syscfg_write(CFG_TWS_COMMON_ADDR, addr[3], 6);
            pair_suss = 1;
            log_info("rec comm addr\n");
            put_buf(addr[3], 6);
        }
        /* 记录左右声道 */
        channel = tws_api_get_local_channel();
        if (channel != bt_tws_get_local_channel()) {
            syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        }
        r_printf("tws_local_channel: %c\n", channel);

        u8 conn_addr[6];
        bt_get_tws_local_addr(conn_addr);
        for (int i = 0; i < 6; i++) {
            conn_addr[i] += addr[2][i];
        }
        tws_api_set_quick_connect_addr(conn_addr);


        if (pair_suss) {
            gtws.state = BT_TWS_PAIRED;
#if CONFIG_TWS_COMMON_ADDR_SELECT != CONFIG_TWS_COMMON_ADDR_USED_MASTER
            bt_update_mac_addr((void *)addr[3]);
#endif
            app_send_message(APP_MSG_TWS_PAIR_SUSS, 0);
        } else {
            app_send_message(APP_MSG_TWS_CONNECTED, 0);
        }
        EARPHONE_STATE_TWS_CONNECTED(pair_suss, addr[3]);


        if (role == TWS_ROLE_MASTER) {
#if TCFG_LOCAL_TWS_ENABLE
            local_tws_connect_mode_report();
#endif
            bt_tws_tx_jiffies_offset();
        }
        if (reason & (TWS_STA_ESCO_OPEN | TWS_STA_SBC_OPEN)) {
            if (role == TWS_ROLE_SLAVE) {
                gtws.state |= BT_TWS_AUDIO_PLAYING;
            }
        }

        gtws.state &= ~BT_TWS_TIMEOUT;
        gtws.state |= BT_TWS_SIBLING_CONNECTED;

#if TCFG_AUDIO_ANC_ENABLE
        bt_tws_sync_anc();
#endif

#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
        bt_tws_sync_spatial_effect_state();
#endif

        tws_sync_bat_level(); //同步电量到对端设备
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        u32 slave_info =  evt->args[3] | (evt->args[4] << 8) | (evt->args[5] << 16) | (evt->args[6] << 24) ;
        printf("====slave_info:%x\n", slave_info);
        if (!(slave_info & TWS_STA_LE_AUDIO_PLAYING)) {
            //only set to slave while slave not playing
            bt_tws_sync_volume();
        }
#else
        bt_tws_sync_volume();
#endif
        tws_sync_dual_conn_info();

#if TCFG_EAR_DETECT_ENABLE
        tws_sync_ear_detect_state(0);
#endif

        tws_sniff_controle_check_enable();

        break;
    case TWS_EVENT_CONNECTION_DETACH:
        /*
         * TWS连接断开
         */
        if (app_var.goto_poweroff_flag) {
            break;
        }
        g_bt_hdl.phone_ring_sync_tws = 0;
        work_state = evt->args[3];
        log_info("tws_event_connection_detach: state: %x,%x\n", gtws.state, work_state);
#if CONFIG_TWS_COMMON_ADDR_SELECT == CONFIG_TWS_COMMON_ADDR_USED_MASTER
        lmp_hci_write_local_address(bt_get_mac_addr());
#endif

        app_power_set_tws_sibling_bat_level(0xff, 0xff);

#if TCFG_TEST_BOX_ENABLE
        if (testbox_get_status()) {
            break;
        }
#endif

#if TCFG_LOCAL_TWS_ENABLE
        local_tws_disconnect_deal();
#endif

        if (phone_link_connection) {
            //对端设备断开后如果手机还连着，主动推一次电量给手机
            batmgr_send_msg(POWER_EVENT_POWER_CHANGE, 0);
            bt_cmd_prepare(USER_CTRL_HFP_CMD_UPDATE_BATTARY, 0, NULL);
        }

        tws_sniff_controle_check_disable();

        gtws.state &= ~BT_TWS_SIBLING_CONNECTED;

        if (reason == TWS_DETACH_BY_REMOVE_PAIRS) {
            gtws.state = BT_TWS_UNPAIRED;
            puts("<<<< tws detach by remove pairs >>>>\n");
            break;
        }

        if (bt_get_esco_coder_busy_flag()) {
            tws_api_connect_in_esco();
            break;
        }

        //非测试盒在仓测试，直连蓝牙
        if (reason & TWS_DETACH_BY_TESTBOX_CON) {
            puts("<<<<< TWS_DETACH_BY_TESTBOX_CON >>>>n");
            gtws.state &= ~BT_TWS_PAIRED;
            gtws.state |= BT_TWS_UNPAIRED;
            if (!phone_link_connection) {
                get_random_number(mac_addr, 6);
                lmp_hci_write_local_address(mac_addr);
            }
            break;
        }
        app_send_message(APP_MSG_TWS_DISCONNECTED, 0);
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        /*
         * 跟手机的链路LMP层已完全断开, 只有tws在连接状态才会收到此事件
         */
        printf("tws_event_phone_link_detach: %x\n", gtws.state);
        if (app_var.goto_poweroff_flag) {
            break;
        }

#if TCFG_TEST_BOX_ENABLE
        if (testbox_get_status()) {
            break;
        }
#endif
        tws_sniff_controle_check_enable();
        break;
    case TWS_EVENT_REMOVE_PAIRS:
        log_info("tws_event_remove_pairs\n");
        bt_tws_remove_pairs();
        app_power_set_tws_sibling_bat_level(0xff, 0xff);
        break;
    case TWS_EVENT_ROLE_SWITCH:
        r_printf("TWS_EVENT_ROLE_SWITCH=%d\n", role);
        /* if (role != TWS_ROLE_SLAVE) { */
        /* 	play_tone_file(get_tone_files()->low_latency_in); */
        /* } */
        u8 *esco_addr = lmp_get_esco_link_addr();
        if (esco_addr) {
            bt_phone_esco_play(esco_addr);
        }
        if (role == TWS_ROLE_SLAVE) {
            esco_recoder_switch(0);
        } else {
            esco_recoder_switch(1);
        }
        if (!(tws_api_get_tws_state() & TWS_STA_PHONE_CONNECTED)) {
            if (role == TWS_ROLE_MASTER) {
                os_time_dly(2);
                tws_sniff_controle_check_enable();
            }
        }
        break;
    case TWS_EVENT_ESCO_ROLE_SWITCH_START:
        r_printf("TWS_EVENT_ESCO_ROLE_SWITCH_START=%d\n", role);
        u8 *esco_addr1 = lmp_get_esco_link_addr();
        if (esco_addr1) {
            bt_phone_esco_play(esco_addr1);
        }
        if (role == TWS_ROLE_SLAVE) {
            esco_recoder_switch(1);
        }
        break;
    case TWS_EVENT_ESCO_ADD_CONNECT:
        bt_tws_sync_volume();
        break;

    case TWS_EVENT_MODE_CHANGE:
        log_info("TWS_EVENT_MODE_CHANGE : %d", evt->args[0]);
        if (evt->args[0] == 0) {
            tws_sniff_controle_check_enable();
        }
        break;
    case TWS_EVENT_TONE_TEST:
        log_info("TWS_EVENT_TEST : %d", evt->args[0]);
        /*play_tone_file(get_tone_files()->num[evt->args[0]]);*/
        break;
    }

    a2dp_player_tws_event_handler((int *)evt);
    return 0;
}
APP_MSG_HANDLER(tws_msg_handler) = {
    .owner      = APP_MODE_BT,
    .from       = MSG_FROM_TWS,
    .handler    = bt_tws_connction_status_event_handler,
};

/**
 * @brief TWS取消对对端设备的连接
 */
void tws_cancle_all_noconn()
{
    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
}

/**
 * @brief 获取TWS是否已连接对端设备
 *
 * @return 1:已连接对端设备；0:未连接对端设备
 */
bool get_tws_sibling_connect_state(void)
{
    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        return TRUE;
    }
    return FALSE;
}

#define TWS_SNIFF_CNT_TIME      5000    //ms

static void bt_tws_enter_sniff(void *parm)
{
    int interval;
#if TCFG_LOCAL_TWS_ENABLE
    //处于本地传输状态不能进入tws_sniff
    if (local_tws_get_role() != LOCAL_TWS_ROLE_NULL) {
        return;
    }
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    if (is_cig_music_play() || is_cig_other_music_play() || is_cig_phone_call_play() || is_cig_other_phone_call_play()) {
        puts("cis music or call , can not enter sniff\n");
        goto __exit;
    }
#endif
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
    if (check_local_not_accept_sniff_by_remote()) {
        goto __exit;
    }
#endif

    int state = tws_api_get_tws_state();
    if (state & TWS_STA_PHONE_DISCONNECTED) {
        interval = 400;
    } else if (state & TWS_STA_PHONE_SNIFF) {
        interval = 800;
    } else {
        goto __exit;
    }

    int err = tws_api_tx_sniff_req(interval, 12);
    if (err == 0) {
        sys_timer_del(gtws.sniff_timer);
        gtws.sniff_timer = 0;
        return;
    }

__exit:
    sys_timer_modify(gtws.sniff_timer, TWS_SNIFF_CNT_TIME);
}

/**
 * @brief TWS通讯开启进入sniff检查定时器
 */
void tws_sniff_controle_check_enable(void)
{
#if (CONFIG_BT_TWS_SNIFF == 0)
    return;
#endif

    if (update_check_sniff_en() == 0) {
        return;
    }

    if (gtws.sniff_timer == 0) {
        gtws.sniff_timer = sys_timer_add(NULL, bt_tws_enter_sniff, TWS_SNIFF_CNT_TIME);
    }
    printf("tws_sniff_check_enable\n");
}

/**
 * @brief TWS通讯关闭进入sniff检查定时器
 */
void tws_sniff_controle_check_disable(void)
{
#if (CONFIG_BT_TWS_SNIFF == 0)
    return;
#endif

    if (gtws.sniff_timer) {
        sys_timer_del(gtws.sniff_timer);
        gtws.sniff_timer = 0;
    }
    puts("tws_sniff_check_disable\n");
}

#else

bool get_tws_sibling_connect_state(void)
{
    return FALSE;
}

#endif
