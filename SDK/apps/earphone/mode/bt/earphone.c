
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".earphone.data.bss")
#pragma data_seg(".earphone.data")
#pragma const_seg(".earphone.text.const")
#pragma code_seg(".earphone.text")
#endif
#include "system/includes.h"
#include "media/includes.h"
#include "app_tone.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "esco_recoder.h"
#include "earphone.h"
#include "idle.h"
#include "bt_slience_detect.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "btstack/a2dp_media_codec.h"
#include "btctrler/btctrler_task.h"
#include "btctrler/btcontroller_modules.h"
#include "user_cfg.h"
#include "audio_cvp.h"
#include "classic/hci_lmp.h"
#include "bt_common.h"
#include "bt_ble.h"
#include "pbg_user.h"
#include "btstack/bluetooth.h"
#include "fs/sdfile.h"
#include "battery_manager.h"
#include "online_db/spp_online_db.h"
#include "online_debug/audio_capture.h"
#include "app_default_msg_handler.h"
#include "bt_key_func.h"
#include "low_latency.h"
#include "tws_dual_share.h"
#include "poweroff.h"
#include "realme_protocol/realme_platform_api.h"
#include "lib_btctrler_config.h"
#include "local_tws.h"

#if TCFG_USER_TWS_ENABLE
#include "tws_dual_conn.h"
#else
#include "dual_conn.h"
#endif

#if RCSP_MODE
#include "rcsp.h"
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "app_le_connected.h"
#endif
#if RCSP_MODE && TCFG_RCSP_DUAL_CONN_ENABLE
#include "adv_1t2_setting.h"
#endif

#include "audio_anc_includes.h"

#include "bt_tws.h"
#include "bt_event_func.h"


#include "asm/lp_touch_key_api.h"
#include "asm/charge.h"
#include "app_charge.h"

#include "fs/resfile.h"
#include "app_chargestore.h"
#include "app_testbox.h"
#include "app_online_cfg.h"
#include "app_main.h"
#include "app_power_manage.h"
#include "gSensor/gSensor_manage.h"
#include "classic/tws_api.h"
#include "ir_sensor/ir_manage.h"
#include "in_ear_detect/in_ear_manage.h"
#include "vol_sync.h"
#include "audio_config.h"
#include "clock_manager/clock_manager.h"

#if TCFG_MIX_RECORD_ENABLE
#include "dev_manager.h"
#include "mix_record_api.h"
#endif

#if THIRD_PARTY_PROTOCOLS_SEL & JL_SBOX_EN
#include "edr_hid_user.h"
#include "sbox_user_app.h"
#endif

#if TCFG_APP_BT_EN

#if ((THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN | GFPS_EN | MMA_EN | FMNA_EN | REALME_EN | SWIFT_PAIR_EN | DMA_EN | ONLINE_DEBUG_EN | CUSTOM_DEMO_EN | XIMALAYA_EN | MULTI_CLIENT_EN | JL_SBOX_EN | HID_ISO_EN)) || \
		(TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN | LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN | LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))
#include "multi_protocol_main.h"
#endif

#define LOG_TAG             "[EARPHONE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define AVC_VOLUME_UP			0x41
#define AVC_VOLUME_DOWN			0x42

u16 disturb_scan_timer = 0xff;

#if defined(CONFIG_CPU_BR28)||defined(CONFIG_CPU_BR36)/*JL700N\JL701N*/

#define OPTIMIZATION_CONN_NOISE    0//回连噪声优化,根据样机实际情况来开 硬件需求短接io
#define RF_RXTX_STATE_PROT   IO_PORTB_08//rf rx pa口
#define EDGE_SLECT_PORART    PORTB
#define EDGE_SLECT_POART_IO  PORT_PIN_5//PB边沿检测
#define BT_RF_CURRENT_BALANCE_SUPPORT_ONLY_ONE_PORT  0//单io, JL700N\JL701N not support

#elif defined(CONFIG_CPU_BR50)/*JL708*/

#define OPTIMIZATION_CONN_NOISE    1//回连噪声优化,默认开启
#define BT_RF_CURRENT_BALANCE_SUPPORT_ONLY_ONE_PORT  1
#define RF_RXTX_STATE_PROT   	IO_PORTC_07//default PC7 内邦，可以不用修改io口
#define EDGE_SLECT_PORART    	PORTC
#define EDGE_SLECT_POART_IO  	PORT_PIN_7//边沿检测 default PC7
#define EDGE_SLECT_POART_OPEN  {JL_PORTC->DIR &= ~BIT(7);JL_PORTC->SPL |= BIT(7);}
#define EDGE_SLECT_POART_CLOSE {JL_PORTC->DIR |= BIT(7);JL_PORTC->SPL &= ~BIT(7);}

#elif defined(CONFIG_CPU_BR52) || defined(CONFIG_CPU_BR56)//JL709 or JL710

#define OPTIMIZATION_CONN_NOISE    1//回连噪声优化,默认开启
#define BT_RF_CURRENT_BALANCE_SUPPORT_NOT_PORT//不需要io

#else

#define OPTIMIZATION_CONN_NOISE    0//回连噪声优化

#endif

#ifdef CONFIG_CPU_BR56
#define BT_TIMER  JL_TIMER2
#define BT_IRQ_TIME_IDX  IRQ_TIME2_IDX
#else
#define BT_TIMER  JL_TIMER3
#define BT_IRQ_TIME_IDX  IRQ_TIME3_IDX
#endif

void bredr_link_disturb_scan_timeout(void *priv)
{
    puts("\n++++++++disturb_scan_timeout\n");
    disturb_scan_timeout_cb_api();
#if TCFG_USER_TWS_ENABLE
    bt_tws_poweron();
#else
    lmp_hci_write_scan_enable((1 << 1) | 1);
#endif
}

void bredr_link_disturb_scan()
{
    puts("\n++++++++disturb_scan\n");
    link_disturb_scan_enable();
    disturb_scan_timer =  sys_timeout_add(NULL, bredr_link_disturb_scan_timeout, 5000);
}

void bt_get_btstack_device(u8 *addr_a, void **device_a, void **device_b)
{
    void *device[2] = { NULL, NULL };
    btstack_get_conn_devices(device, 2);

    *device_a = btstack_get_conn_device(addr_a);
    *device_b = *device_a == device[0] ? device[1] : device[0];
}
void bt_get_btstack_device3(u8 *addr_a, void **device_a, void **device_b, void **device_c)
{
    int i;
    void *device[3] = { NULL, NULL, NULL };
    btstack_get_conn_devices(device, 3);
    *device_a = NULL;
    *device_b = NULL;
    *device_c = NULL;

    *device_a = btstack_get_conn_device(addr_a);
    for (i = 0; i < 3; i++) {
        if (device[i] && (device[i] != (*device_a))) {
            *device_b = device[i];
        }

    }
    for (i = 0; i < 3; i++) {
        if (device[i] && (device[i] != (*device_a) && (device[i] != (*device_b)))) {
            *device_c = device[i];
        }

    }
}

bool bt_in_phone_call_state(void *device)
{
    if (!device) {
        return false;
    }
    int state = btstack_bt_get_call_status(device);

    return (state == BT_CALL_ACTIVE || state == BT_CALL_ALERT  ||
            state == BT_CALL_OUTGOING || state == BT_CALL_INCOMING ||
            (state == BT_SIRI_STATE && btstack_get_call_esco_status(device) == BT_ESCO_STATUS_OPEN));
}

bool bt_not_in_phone_call_state(void *device)
{
    if (!device) {
        return true;
    }
    int state = btstack_bt_get_call_status(device);
    if (state == BT_CALL_HANGUP) {
        return true;
    }
    if (state == BT_SIRI_STATE &&
        btstack_get_call_esco_status(device) == BT_ESCO_STATUS_CLOSE) {
        return true;
    }
    return false;
}


static u8 sniff_out = 0;

u8 get_sniff_out_status()
{
    return sniff_out;
}
void clear_sniff_out_status()
{
    sniff_out = 0;
}

/**********进入蓝牙dut模式
*  mode=0:使能可以进入dut，原本流程不变。
*  mode=1:删除一些其它切换状态，产线中通过工具调用此接口进入dut模式，提高测试效率
 *********************/
void bt_bredr_enter_dut_mode(u8 mode, u8 inquiry_scan_en)
{
    puts("<<<<<<<<<<<<<bt_bredr_enter_dut_mode>>>>>>>>>>>>>>\n");

#if (defined CONFIG_CPU_BR56)
    u32 curr_clk = clk_get_max_frequency();
    y_printf("DUT test,set clock:%d\n", curr_clk);
    clock_alloc("DUT", curr_clk);
#endif
    bredr_set_dut_enble(1, 1);
    if (mode) {
        clr_device_in_page_list();
        dut_idle_run_slot(2);
        g_bt_hdl.auto_connection_counter = 0;
#if TCFG_USER_TWS_ENABLE
        bt_page_scan_for_test(inquiry_scan_en);
#endif

    }
}

static u8 *bt_get_sdk_ver_info(u8 *len)
{
    const char *p = sdk_version_info_get();
    if (len) {
        *len = strlen(p);
    }

    log_info("sdk_ver:%s %x\n", p, *len);
    return (u8 *)p;
}
int bt_get_battery_percent(void)
{
    return get_vbat_percent();
}

void bredr_handle_register()
{
#if (TCFG_BT_SUPPORT_SPP==1)
    bt_spp_data_deal_handle_register(spp_data_handler);
#endif
    bt_fast_test_handle_register(bt_fast_test_api);//测试盒快速测试接口
#if TCFG_BT_VOL_SYNC_ENABLE
    bt_music_vol_change_handle_register(set_music_device_volume, phone_get_device_vol);
#endif
#if TCFG_BT_DISPLAY_BAT_ENABLE
    bt_get_battery_value_handle_register(bt_get_battery_value);   /*电量显示获取电量的接口*/
    bt_get_battery_percent_handle_register(bt_get_battery_percent);
#endif

    bt_dut_test_handle_register(bt_dut_api);
}

#if TCFG_USER_TWS_ENABLE
static void rx_dual_conn_info(u8 *data, int len)
{
    log_info("tws_sync_dual_conn_info_func: %d\n", data[0]);
    g_bt_hdl.bt_dual_conn_config = data[0];
    syscfg_write(CFG_TWS_DUAL_CONFIG, &(g_bt_hdl.bt_dual_conn_config), 1);
    free(data);
}
static void tws_sync_dual_conn_info_func(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = malloc(len);
        memcpy(data, _data, len);
        int msg[4] = { (int)rx_dual_conn_info, 2, (int)data, len};
        int err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
        if (err != OS_NO_ERR) {
            free(data);
        }
    }
}
REGISTER_TWS_FUNC_STUB(app_vol_sync_stub) = {
    .func_id = 0x1A782C1B,
    .func    = tws_sync_dual_conn_info_func,
};
void tws_sync_dual_conn_info()
{
    u8 data[2];
    data[0] = g_bt_hdl.bt_dual_conn_config;
    tws_api_send_data_to_slave(data, 2, 0x1A782C1B);

}
#endif

u8 get_bt_dual_config()
{
    return g_bt_hdl.bt_dual_conn_config;
}
void set_dual_conn_config(u8 *addr, u8 dual_conn_en)
{
#if TCFG_BT_DUAL_CONN_ENABLE
    if (dual_conn_en) {
        g_bt_hdl.bt_dual_conn_config = DUAL_CONN_SET_TWO;
        log_info(">>>>%s: dual_conn_en\n", __FUNCTION__);
        // 如果是双连接，重新判断是否需要开启可发现可连接
#if TCFG_USER_TWS_ENABLE
        tws_dual_conn_state_handler();
#else
        dual_conn_state_handler();
#endif
    } else {
        g_bt_hdl.bt_dual_conn_config = DUAL_CONN_SET_ONE;
        u8 *other_conn_addr;
        other_conn_addr = btstack_get_other_dev_addr(addr);
        if (other_conn_addr) {
            btstack_device_detach(btstack_get_conn_device(other_conn_addr));
        }
        if (addr) {
            updata_last_link_key(addr, get_remote_dev_info_index());
        }
    }
#if TCFG_USER_TWS_ENABLE
    tws_sync_dual_conn_info();
#endif
    bt_set_user_ctrl_conn_num((get_bt_dual_config() == DUAL_CONN_SET_ONE) ? 1 : 2);
    bt_set_auto_conn_device_num((get_bt_dual_config() == DUAL_CONN_SET_TWO) ? 2 : 1);
    syscfg_write(CFG_TWS_DUAL_CONFIG, &(g_bt_hdl.bt_dual_conn_config), 1);
    log_info("set_dual_conn_config=%d\n", g_bt_hdl.bt_dual_conn_config);
#endif

}
void test_set_dual_config()
{
    u8 *addr = bt_get_current_remote_addr();
    set_dual_conn_config(addr, (get_bt_dual_config() == DUAL_CONN_SET_TWO ? 0 : 1));
}

void user_read_remote_name_handle(u8 status, u8 *addr, u8 *name)
{
    //字符串名字，添加结束符
    name[31] = 0;   //earphone类的SDK，默认配置名字长度32byte
    //void set_bt_full_name_event(u8 en);
    //name[47] = 0;   //set_bt_full_name_event配置名字长度48byte

    log_info("\nuser_read_remote_name_handle:\n");
    put_buf(addr, 6);
    log_info("name=%s\n", name);
#if RCSP_MODE && TCFG_RCSP_DUAL_CONN_ENABLE
    rcsp_1t2_set_edr_info(addr, name);
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)
    realme_remote_name_callback(status, addr, name);
#endif
}
void bt_function_select_init()
{
    int support_set_conn_num = 2;
    /* set_edr_wait_conn_run_slot(1500,8,16,10); */

#if 0//3mb set
    set_bt_data_rate_acl_3mbs_mode(1);
    bt_set_support_3M_size(1);
#endif
#if TCFG_BT_DUAL_CONN_ENABLE
    g_bt_hdl.bt_dual_conn_config = DUAL_CONN_SET_TWO;//DUAL_CONN_SET_TWO:默认可以连接1t2  DUAL_CONN_SET_ONE:默认只支持一个连接
    syscfg_read(CFG_TWS_DUAL_CONFIG, &(g_bt_hdl.bt_dual_conn_config), 1);
#if TCFG_BT_DUAL_1T3_CONN_ENABLE
    support_set_conn_num = 3;
#endif
#else
    g_bt_hdl.bt_dual_conn_config = DUAL_CONN_SET_ONE;
#endif
    log_info("<<<<<<<<<<<<<<bt_dual_conn_config=%d>>>>>>>>>>\n", g_bt_hdl.bt_dual_conn_config);
    if (g_bt_hdl.bt_dual_conn_config != DUAL_CONN_SET_TWO) {
        set_tws_task_interval(120);
    }
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    if (get_bt_le_audio_config_for_vm()) {
        log_info("le_audio en");
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN)))
        g_bt_hdl.bt_dual_conn_config = DUAL_CONN_SET_ONE;
#endif
    }
#endif
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
    log_info("app_auracast en");
    g_bt_hdl.bt_dual_conn_config = DUAL_CONN_SET_ONE;
#endif
    bt_set_user_ctrl_conn_num((get_bt_dual_config() == DUAL_CONN_SET_ONE) ? 1 : support_set_conn_num);
    set_lmp_support_dual_con((get_bt_dual_config() == DUAL_CONN_SET_ONE) ? 1 : support_set_conn_num);
    bt_set_auto_conn_device_num((get_bt_dual_config() == DUAL_CONN_SET_TWO) ? support_set_conn_num : 1);

    bt_set_support_msbc_flag(TCFG_BT_MSBC_EN);

#if (!CONFIG_A2DP_GAME_MODE_ENABLE)
    bt_set_support_aac_flag(TCFG_BT_SUPPORT_AAC);
    bt_set_aac_bitrate(0x20000);
#endif

#if (defined(TCFG_BT_SUPPORT_LHDC) && TCFG_BT_SUPPORT_LHDC)
    bt_set_support_lhdc_flag(TCFG_BT_SUPPORT_LHDC);
#endif

#if (defined(TCFG_BT_SUPPORT_LHDC_V5) && TCFG_BT_SUPPORT_LHDC_V5)
    bt_set_support_lhdc_v5_flag(TCFG_BT_SUPPORT_LHDC_V5);
#endif

#if (defined(TCFG_BT_SUPPORT_LDAC) && TCFG_BT_SUPPORT_LDAC)
    bt_set_support_ldac_flag(TCFG_BT_SUPPORT_LDAC);
#endif

#if TCFG_BT_DISPLAY_BAT_ENABLE
    bt_set_update_battery_time(60);
#else
    bt_set_update_battery_time(0);
#endif
    /*回连搜索时间长度设置,可使用该函数注册使用，ms单位,u16*/
    bt_set_page_timeout_value(0);

    /*回连时超时参数设置。ms单位。做主机有效*/
    bt_set_super_timeout_value(8000);


#if TCFG_BT_VOL_SYNC_ENABLE
    vol_sys_tab_init();
#endif
    /* io_capabilities
     * 0: Display only 1: Display YesNo 2: KeyboardOnly 3: NoInputNoOutput
     *  authentication_requirements: 0:not protect  1 :protect
    */
    bt_set_simple_pair_param(3, 0, 2);
    bt_testbox_ex_info_get_handle_register(TESTBOX_INFO_VBAT_VALUE, get_vbat_value);
    bt_testbox_ex_info_get_handle_register(TESTBOX_INFO_VBAT_PERCENT, get_vbat_percent);
    bt_testbox_ex_info_get_handle_register(TESTBOX_INFO_BURN_CODE, sdfile_get_burn_code);
    bt_testbox_ex_info_get_handle_register(TESTBOX_INFO_SDK_VERSION, bt_get_sdk_ver_info);
    bt_read_remote_name_handle_register(user_read_remote_name_handle);

    bt_set_sbc_cap_bitpool(TCFG_BT_SBC_BITPOOL);

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN)))
    if (get_bt_le_audio_config()) {
        bt_change_hci_class_type(BD_CLASS_WEARABLE_HEADSET | LE_AUDIO_CLASS);   //经典蓝牙地址跟le audio地址一样要置上BIT(14)

    }
#endif
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG & LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX)
    set_le_audio_unicast_witch_phone_play_mix(1);
#if TCFG_USER_TWS_ENABLE
    tws_api_pure_monitor_enable(1);
#endif

#endif
#endif
#if TCFG_USER_BLE_ENABLE
    u8 tmp_ble_addr[6];
#if TCFG_BT_BLE_BREDR_SAME_ADDR
    memcpy(tmp_ble_addr, (void *)bt_get_mac_addr(), 6);
#else
    bt_make_ble_address(tmp_ble_addr, (void *)bt_get_mac_addr());
#endif
    le_controller_set_mac((void *)tmp_ble_addr);
    puts("-----edr + ble 's address-----\n");
    log_info_hexdump((void *)bt_get_mac_addr(), 6);
    log_info_hexdump((void *)tmp_ble_addr, 6);
#endif

    set_bt_enhanced_power_control(1);

#if TCFG_PREEMPT_CONNECTION_ENABLE
    void set_a2dp_esco_not_preempt(u8 en);
    set_a2dp_esco_not_preempt(1);
#endif
#if TCFG_TWS_AUDIO_SHARE_ENABLE
    bt_tws_share_function_select_init();
#endif

#if TCFG_BT_MUSIC_INFO_ENABLE
    bt_music_info_handle_register(user_get_bt_music_info);
#endif

#if ((TCFG_BT_SUPPORT_HID == 1) && TCFG_USER_EDR_ENABLE) && (!TCFG_BT_DUAL_CONN_ENABLE)
    edr_hid_config_init();
#endif
}


__attribute__((weak))
void arch_trim()
{

}

/*
 * 对应原来的状态处理函数，连接，电话状态等
 */

static int bt_connction_status_event_handler(struct bt_event *bt)
{

    switch (bt->event) {
    case BT_STATUS_INIT_OK:

#ifndef  CONFIG_FPGA_ENABLE
        // 未调通,会引起复位,临时注释
        /* arch_trim(); */
#endif
#if defined(TCFG_BT_OSC_OFFSET) && (TCFG_BT_OSC_OFFSET != 0)
        if (bt_fre_offset_trim_flag_get() == 0) {
            printf("TCFG_BT_OSC_OFFSET:%d\n", TCFG_BT_OSC_OFFSET);
            bt_osc_offset_set(TCFG_BT_OSC_OFFSET);
        }
#endif
        /*
         * 蓝牙初始化完成
         */
        g_bt_hdl.init_ok = 1;
        log_info("BT_STATUS_INIT_OK\n");

#if TCFG_TEST_BOX_ENABLE
        testbox_set_bt_init_ok(1);
#endif

#if TCFG_NORMAL_SET_DUT_MODE
        log_info("edr set dut mode\n");
        bredr_set_dut_enble(1, 1);
        bt_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
        bt_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
#if TCFG_USER_BLE_ENABLE
        log_info("ble set dut mode\n");
        ble_bqb_test_thread_init();
#endif
        break;
#endif

#if TCFG_BLE_AUDIO_TEST_EN

        /* df_aoa_tx_connected_test_open(); */
        /* df_aoa_broadcast_test_open(); */
        /* big_tx_test_open(); */
        /* big_rx_test_open(); */
        cig_perip_test_open();
        /* cig_central_test_open(); */
        break;
#endif

#if TCFG_AUDIO_DATA_EXPORT_DEFINE
#if (TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SPP)
        lmp_hci_set_role_switch_supported(0);
#endif /*TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SPP*/
#if ((TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SPP) \
    || (TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SD))
        audio_capture_init();
#endif
#endif /*TCFG_AUDIO_DATA_EXPORT_DEFINE*/

#if TCFG_AUDIO_ANC_ENABLE
        anc_poweron();
#endif

#if (TCFG_USER_BLE_ENABLE || TCFG_BT_BLE_ADV_ENABLE || (THIRD_PARTY_PROTOCOLS_SEL & TUYA_DEMO_EN))
        if (BT_MODE_IS(BT_BQB)) {
            ble_bqb_test_thread_init();
        } else {
#if RCSP_MODE
            rcsp_init();
#endif
            bt_ble_init();
        }
#endif

#if ((THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN | GFPS_EN | MMA_EN | FMNA_EN | REALME_EN | SWIFT_PAIR_EN | DMA_EN | ONLINE_DEBUG_EN | CUSTOM_DEMO_EN | XIMALAYA_EN | MULTI_CLIENT_EN | JL_SBOX_EN | HID_ISO_EN)) || \
		(TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN | LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN | LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))
        multi_protocol_bt_init();
#endif

        bt_init_ok_search_index();

#if TCFG_CHARGESTORE_ENABLE
        chargestore_set_bt_init_ok(1);
#endif

#if ((CONFIG_BT_MODE == BT_BQB)||(CONFIG_BT_MODE == BT_PER))
        break;
#endif
        sys_auto_shut_down_enable();
        app_send_message(APP_MSG_STATUS_INIT_OK, APP_MODE_BT);
#ifdef  CONFIG_FPGA_ENABLE
        lmp_hci_write_scan_enable((1 << 1) | 1);
        break;
#endif

        if (app_var.play_poweron_tone) {
            if (tone_player_runing()) {
                break;
            }
        }

#if TCFG_USER_TWS_ENABLE
        bt_tws_poweron();
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        // 使能没有快连也能进idle，idle_slot越大功耗越低，但tws会越难连接
        extern void set_unconn_tws_phone_enter_idle(u8 en, u16 idle_slot);
        set_unconn_tws_phone_enter_idle(1, 250);
#endif
#else
        lmp_hci_write_scan_enable((1 << 1) | 1);
#endif

        break;

    case BT_STATUS_THIRD_CONNECTED:
    case BT_STATUS_SECOND_CONNECTED:
        bt_clear_current_poweron_memory_search_index(0);
    case BT_STATUS_FIRST_CONNECTED:
        log_info("BT_STATUS_CONNECTED\n");

#if TCFG_CHARGESTORE_ENABLE
        chargestore_set_phone_connect();
#endif
#if TCFG_USER_TWS_ENABLE
        bt_tws_phone_connected();
#endif
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
    case BT_STATUS_THIRD_DISCONNECT:
        log_info("BT_STATUS_DISCONNECT\n");
        if (app_var.goto_poweroff_flag) {
            break;
        }
#if TCFG_CHARGESTORE_ENABLE
        chargestore_set_phone_disconnect();
#endif
        break;

    case BT_STATUS_CONN_A2DP_CH:
        log_info("++++++++ BT_STATUS_CONN_A2DP_CH +++++++++  \n");
        break;
    case BT_STATUS_DISCON_A2DP_CH:
        log_info("++++++++ BT_STATUS_DISCON_A2DP_CH +++++++++  \n");
        break;
    case BT_STATUS_AVRCP_INCOME_OPID:
        log_info("BT_STATUS_AVRCP_INCOME_OPID:0x%x\n", bt->value);
        if (bt->value == AVC_VOLUME_UP) {
            app_audio_volume_up(1);
        } else if (bt->value == AVC_VOLUME_DOWN) {
            app_audio_volume_down(1);
        }
        break;
    case BT_STATUS_RECONN_OR_CONN:
        log_info("++++++++ BT_STATUS_RECONN_OR_CONN +++++++++  \n");
#if TCFG_BT_SUPPORT_MAP
#if (defined TCFG_BT_SUPPORT_MAP_MESSAGE && (TCFG_BT_SUPPORT_MAP_MESSAGE==1))
        bt_cmd_prepare(USER_CTRL_MAP_SET_NOTIFICATION, 0, NULL);
#else
        bt_cmd_prepare(USER_CTRL_MAP_READ_TIME, 0, NULL);
#endif
#endif
        break;
    default:
        log_info(" BT STATUS DEFAULT\n");
        break;
    }
    return 0;
}

static void bt_hci_event_connection(struct bt_event *bt)
{
    //clk_set_sys_lock(BT_CONNECT_HZ, 0);
    log_info("tws_conn_state=%d\n", g_bt_hdl.tws_conn_state);
}

static void bt_hci_event_linkkey_missing(struct bt_event *bt)
{

    if (config_delete_link_key) {
        delete_link_key((u8 *)bt->args, get_remote_dev_info_index());
    }
}

static void bt_hci_event_page_timeout()
{
    log_info("HCI_EVENT_PAGE_TIMEOUT conuter %d\n",
             g_bt_hdl.auto_connection_counter);
}

static void bt_hci_event_connection_exist(struct bt_event *bt)
{
}


enum {
    TEST_STATE_INIT = 1,
    TEST_STATE_EXIT = 3,
};

enum {
    ITEM_KEY_STATE_DETECT = 0,
    ITEM_IN_EAR_DETECT,
};

static u8 in_ear_detect_test_flag = 0;
static void testbox_in_ear_detect_test_flag_set(u8 flag)
{
    in_ear_detect_test_flag = flag;
}

u8 testbox_in_ear_detect_test_flag_get(void)
{
    return in_ear_detect_test_flag;
}

static void bt_in_ear_detection_test_state_handle(u8 state, u8 *value)
{
    switch (state) {
    case TEST_STATE_INIT:
        testbox_in_ear_detect_test_flag_set(1);

#if TCFG_LP_EARTCH_KEY_ENABLE
        lp_touch_key_testbox_inear_trim(1);
#endif
        //start trim
        break;
    case TEST_STATE_EXIT:
        testbox_in_ear_detect_test_flag_set(0);
#if TCFG_LP_EARTCH_KEY_ENABLE
        lp_touch_key_testbox_inear_trim(0);
#endif
        break;
    }
}

static void bt_vendor_meta_event_handle(u8 sub_evt, u8 *arg, u8 len)
{
    log_info("vendor event:%x\n", sub_evt);
    log_info_hexdump(arg, 6);

    if (sub_evt != HCI_SUBEVENT_VENDOR_TEST_MODE_CFG) {
        log_info("unknow_sub_evt:%x\n", sub_evt);
        return;
    }

    u8 test_item = arg[0];
    u8 state = arg[1];

    if (ITEM_IN_EAR_DETECT == test_item) {
        bt_in_ear_detection_test_state_handle(state, NULL);
    }
}

static int bt_hci_event_handler(struct bt_event *bt)
{
    //对应原来的蓝牙连接上断开处理函数  ,bt->value=reason
    log_info("-----------bt_hci_event_handler reason %x %x", bt->event, bt->value);

    if (bt->event == HCI_EVENT_VENDOR_REMOTE_TEST) {
        if (bt->value == VENDOR_TEST_DISCONNECTED) {
#if TCFG_TEST_BOX_ENABLE
            if (testbox_get_status()) {
                if (bt_get_remote_test_flag()) {
                    testbox_clear_connect_status();
                }
            }
#endif
            bt_set_remote_test_flag(0);
            log_info("clear_test_box_flag");
            cpu_reset();
            return 0;
        } else {

#if TCFG_BT_BLE_ADV_ENABLE
#if (CONFIG_BT_MODE == BT_NORMAL)
            //1:edr con;2:ble con;
            if (VENDOR_TEST_LEGACY_CONNECTED_BY_BT_CLASSIC == bt->value) {
                bt_ble_adv_enable(0);
            }
#endif
#endif
#if TCFG_USER_TWS_ENABLE
            if (VENDOR_TEST_CONNECTED_WITH_TWS != bt->value) {
                bt_tws_poweroff();
            }
#endif
        }
    }


    switch (bt->event) {
    case HCI_EVENT_VENDOR_META:
        bt_vendor_meta_event_handle(bt->value, bt->args, 6);
        break;
    case HCI_EVENT_INQUIRY_COMPLETE:
        log_info(" HCI_EVENT_INQUIRY_COMPLETE \n");
        break;
    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        log_info(" HCI_EVENT_USER_CONFIRMATION_REQUEST \n");
        ///<可通过按键来确认是否配对 1：配对   0：取消
        bt_send_pair(1);
        break;
    case HCI_EVENT_USER_PASSKEY_REQUEST:
        log_info(" HCI_EVENT_USER_PASSKEY_REQUEST \n");
        ///<可以开始输入6位passkey
        break;
    case HCI_EVENT_USER_PRESSKEY_NOTIFICATION:
        log_info(" HCI_EVENT_USER_PRESSKEY_NOTIFICATION %x\n", bt->value);
        ///<可用于显示输入passkey位置 value 0:start  1:enrer  2:earse   3:clear  4:complete
        break;
    case HCI_EVENT_PIN_CODE_REQUEST :
        log_info("HCI_EVENT_PIN_CODE_REQUEST  \n");
        break;

    case HCI_EVENT_VENDOR_NO_RECONN_ADDR :
        log_info("HCI_EVENT_VENDOR_NO_RECONN_ADDR \n");
        bt_hci_event_disconnect(bt) ;
        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE :
        log_info("HCI_EVENT_DISCONNECTION_COMPLETE \n");
        put_buf(bt->args, 6);

        if (bt->value ==  ERROR_CODE_CONNECTION_TIMEOUT) {
            bt_hci_event_connection_timeout(bt);
        } else if (bt->value ==  ERROR_CODE_PIN_OR_KEY_MISSING) {
            log_info(" ERROR_CODE_PIN_OR_KEY_MISSING BB \n");
            bt_hci_event_linkkey_missing(bt);
        }
        bt_hci_event_disconnect(bt) ;
        break;

    case BTSTACK_EVENT_HCI_CONNECTIONS_DELETE:
    case HCI_EVENT_CONNECTION_COMPLETE:
        log_info(" HCI_EVENT_CONNECTION_COMPLETE \n");
        put_buf(bt->args, 6);
        switch (bt->value) {
        case ERROR_CODE_SUCCESS :
            log_info("ERROR_CODE_SUCCESS  \n");
            testbox_in_ear_detect_test_flag_set(0);
            bt_hci_event_connection(bt);
            break;
        case ERROR_CODE_PIN_OR_KEY_MISSING:
            log_info(" ERROR_CODE_PIN_OR_KEY_MISSING \n");
            bt_hci_event_linkkey_missing(bt);

        case ERROR_CODE_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED :
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES:
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR:
        case ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED  :
        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION   :
        case ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST :
        case ERROR_CODE_AUTHENTICATION_FAILURE :
            //case CUSTOM_BB_AUTO_CANCEL_PAGE:
            bt_hci_event_disconnect(bt) ;
            break;

        case ERROR_CODE_PAGE_TIMEOUT:
            log_info(" ERROR_CODE_PAGE_TIMEOUT \n");
            bt_hci_event_page_timeout();
            break;

        case ERROR_CODE_CONNECTION_TIMEOUT:
            log_info(" ERROR_CODE_CONNECTION_TIMEOUT \n");
            bt_hci_event_connection_timeout(bt);
            break;

        case ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS  :
            log_info("ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS   \n");
            bt_hci_event_connection_exist(bt);
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



#if TCFG_BT_SUPPORT_MAP
#define PROFILE_CMD_TRY_AGAIN_LATER 	    -1004
void bt_get_time_date()
{
    int error = bt_cmd_prepare(USER_CTRL_HFP_GET_PHONE_DATE_TIME, 0, NULL);
    log_info(">>>>>error = %d\n", error);
    if (error == PROFILE_CMD_TRY_AGAIN_LATER) {
        sys_timeout_add(NULL, bt_get_time_date, 100);
    }
}
void phone_date_and_time_feedback(u8 *data, u16 len)
{
    log_info("time：%s ", data);

#if TCFG_IFLYTEK_ENABLE
    extern void get_time_from_bt(u8 * data);
    get_time_from_bt(data);
    extern void ifly_vad_demo(void);
    ifly_vad_demo();
#endif

#if THIRD_PARTY_PROTOCOLS_SEL & JL_SBOX_EN
    sbox_phone_date_and_time_feedback(data, len);
#endif
}
void map_get_time_data(char *time, int status)
{
    printf("[zwz info] func %s line %d \n", __func__, __LINE__);
    if (status == 0) {
        log_info("time：%s ", time);
#if THIRD_PARTY_PROTOCOLS_SEL & JL_SBOX_EN
        sbox_map_get_time_data(time);
#endif
    } else {
        log_info(">>>map get fail\n");
        sys_timeout_add(NULL, bt_get_time_date, 100);
    }
}
#endif

u8 bt_app_exit_check(void)
{
    if (!app_in_mode(APP_MODE_BT)) {
        return 1;
    }
    if (app_var.siri_stu && app_var.siri_stu != 3) {
        // siri不退出
        log_info("%s, siri_stu:%d\n", __FUNCTION__, app_var.siri_stu);
        return 0;
    }
    int esco_state = bt_get_call_status();
    if (esco_state == BT_CALL_OUTGOING  ||
        esco_state == BT_CALL_ALERT     ||
        esco_state == BT_CALL_INCOMING  ||
        esco_state == BT_CALL_ACTIVE) {
        // 通话不退出
        log_info("%s, esco_state:%d\n", __FUNCTION__, esco_state);
        return 0;
    }

    return 1;
}

static int tone_bt_mode_callback(void *priv, enum stream_event event)
{
    if (g_bt_hdl.init_ok == 0) {
        return 0;
    }
#if TCFG_USER_TWS_ENABLE
    if (event == STREAM_EVENT_STOP) {
        bt_tws_poweron();
    }
#endif
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief  蓝牙非后台模式退出蓝牙等待蓝牙状态可以退出
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void bt_no_background_exit_check(void *priv)
{
    if (g_bt_hdl.init_ok == 0) {
        return;
    }
    if (esco_player_runing() || a2dp_player_runing()) {
        return ;
    }

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    if (is_cig_phone_conn()) {
        //BLE还没完全断开就往下走的话，可能会直接释放导致有些事件流程没有执行了
        return ;
    }
#endif

#if TCFG_USER_BLE_ENABLE
    bt_ble_exit();
#endif

#if ((THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN | GFPS_EN | MMA_EN | FMNA_EN | REALME_EN | SWIFT_PAIR_EN | DMA_EN | ONLINE_DEBUG_EN | CUSTOM_DEMO_EN | XIMALAYA_EN | MULTI_CLIENT_EN | JL_SBOX_EN | HID_ISO_EN)) || \
		(TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN | LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))
    multi_protocol_bt_exit();
#endif

    btstack_exit();
    sys_timer_del(g_bt_hdl.exit_check_timer);
#if TCFG_TWS_AUDIO_SHARE_ENABLE
    bt_tws_share_function_select_close();
#endif
    g_bt_hdl.init_ok = 0;
    g_bt_hdl.init_start = 0;
    g_bt_hdl.exit_check_timer = 0;
    bt_set_stack_exiting(0);
    g_bt_hdl.exiting = 0;
}

/*----------------------------------------------------------------------------*/
/**@brief  蓝牙非后台模式退出模式
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static u8 bt_nobackground_exit()
{
    if (!g_bt_hdl.init_start) {
        g_bt_hdl.exiting = 0;
        return 0;
    }
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    le_audio_profile_exit();
#endif
    bt_set_stack_exiting(1);
#if TCFG_USER_TWS_ENABLE
    void tws_dual_conn_close();
    tws_dual_conn_close();
    bt_tws_poweroff();
#endif
    //app_protocol_exit(0);
    bt_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
    bt_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
    bt_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
    bt_cmd_prepare(USER_CTRL_CONNECTION_CANCEL, 0, NULL);
    bt_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
    if (g_bt_hdl.auto_connection_timer) {
        sys_timeout_del(g_bt_hdl.auto_connection_timer);
        g_bt_hdl.auto_connection_timer = 0;
    }

    if (g_bt_hdl.exit_check_timer == 0) {
        g_bt_hdl.exit_check_timer = sys_timer_add(NULL, bt_no_background_exit_check, 10);
        log_info("set exit timer\n");
    }

    return 0;
}

bool bt_check_already_initializes(void)
{
    return g_bt_hdl.init_start;
}

#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
bool local_tws_without_bg_bt_mode = 0;
#endif

int bt_mode_init()
{
    log_info("bt mode\n");

    /* extern void set_debug_edr_info(int debug_edr_info); */
    /* set_debug_edr_info(BIT(0)|BIT(1)); */
#if (defined CONFIG_CPU_BR56) && (CONFIG_BT_MODE == BT_FCC || CONFIG_BT_MODE == BT_BQB || TCFG_NORMAL_SET_DUT_MODE == 1)
    u32 curr_clk = clk_get_max_frequency();
    y_printf("DUT test,set clock:%d\n", curr_clk);
    clock_alloc("DUT", curr_clk);
#endif

#if (TCFG_BT_BACKGROUND_ENABLE)      //后台返回到蓝牙模式如果是通过模式切换返回的还是要播放提示音
    if (g_bt_hdl.background.backmode == BACKGROUND_GOBACK_WITH_MODE_SWITCH && !bt_background_switch_mode_check())
#endif  //endif TCFG_BLUETOOTH_BACK_MODE
    {
        tone_player_stop();
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
        int ret = local_tws_enter_mode(get_tone_files()->bt_mode, NULL);
        if (ret != 0)
#endif //TCFG_LOCAL_TWS_ENABLE

        {
            play_tone_file_callback(get_tone_files()->bt_mode, NULL, tone_bt_mode_callback);
        }
    }

#if OPTIMIZATION_CONN_NOISE
#ifdef BT_RF_CURRENT_BALANCE_SUPPORT_NOT_PORT
    if (BT_TIMER->CON & 0b11) {
        ASSERT(0, "bt_edge_TIMER use ing\n");
    }
#else
    bt_set_rxtx_status_io(BT_RF_CURRENT_BALANCE_SUPPORT_ONLY_ONE_PORT ? 0xffff : RF_RXTX_STATE_PROT, RF_RXTX_STATE_PROT);
#endif
#endif

    g_bt_hdl.init_start = 1;//蓝牙协议栈已经开始初始化标志位
    g_bt_hdl.init_ok = 0;
    g_bt_hdl.exiting = 0;
    g_bt_hdl.wait_exit = 0;
    g_bt_hdl.ignore_discon_tone = 0;
    g_bt_hdl.control_device_type = CONTROL_ALL;
    u32 sys_clk =  clk_get("sys");
    bt_pll_para(TCFG_CLOCK_OSC_HZ, sys_clk, 0, 0);

#if (TCFG_BT_BACKGROUND_ENABLE)
    if (g_bt_hdl.background.background_working) {
        g_bt_hdl.init_ok = 1;
        bt_background_resume();
        app_send_message(APP_MSG_ENTER_MODE, APP_MODE_BT);
        return 0;
    }

    bt_background_init(bt_hci_event_handler, bt_connction_status_event_handler);
#else
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    if (local_tws_without_bg_bt_mode) {
        // local_tws（没开蓝牙后台）导致
        local_tws_without_bg_bt_mode = 0;
        local_tws_without_background_enter_bt_mode();
        return 0;
    }
#endif
#endif  //endif TCFG_BT_BACKGROUND_ENABLE

    bt_function_select_init();
    bredr_handle_register();
    EARPHONE_STATE_INIT();
    btstack_init();
#if TCFG_USER_TWS_ENABLE

#if TCFG_TWS_AUTO_ROLE_SWITCH_ENABLE
    tws_api_esco_rssi_role_switch(1);//通话根据信号强度主从切换使能
#endif
    tws_profile_init();
#if TCFG_LOCAL_TWS_ENABLE
    // local_tws使用3M包，防止带宽不够
    set_local_tws_3m_type_support(1);
#endif
#endif


    void bt_sniff_feature_init();
    bt_sniff_feature_init();
    app_var.dev_volume = -1;

    app_send_message(APP_MSG_ENTER_MODE, APP_MODE_BT);

    return 0;
}

bool bt_mode_is_try_exit()
{
    return (g_bt_hdl.wait_exit > 0)	? true : false;
}

int bt_mode_try_exit()
{
    if (g_bt_hdl.wait_exit) {
        //等待蓝牙断开或者音频资源释放或者电话资源释放
        if (!g_bt_hdl.exiting) {
            g_bt_hdl.wait_exit++;
            if (g_bt_hdl.wait_exit > 3) {
                //wait two round to do some hci event or other stack event
                return 0;
            }
        }
        return -EBUSY;
    }
    g_bt_hdl.wait_exit = 1;
    g_bt_hdl.exiting = 1;
#if (TCFG_BT_BACKGROUND_ENABLE == 0 || TCFG_LOCAL_TWS_ENABLE == 0) //非后台退出不播放提示音
    g_bt_hdl.ignore_discon_tone = bt_get_total_connect_dev();
#endif
    sys_auto_shut_down_disable();
    //only need to do once
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    local_tws_exit_mode();
#endif

#if (TCFG_BT_BACKGROUND_ENABLE)
    bt_background_suspend();
#else
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE
    if (!local_tws_without_bg_bt_mode) {
        local_tws_without_bg_bt_mode = 1;
        local_tws_without_background_exit_bt_mode();
    }
#else
    bt_nobackground_exit();
#endif
#endif
    return -EBUSY;
}

int bt_mode_exit()
{
    /*~~~~~~~~~~~~ start: 临时修改，库内没关OCH，库内改好请删除~~~~~~~~~~~~~~~~~~*/
#if OPTIMIZATION_CONN_NOISE
#ifdef BT_RF_CURRENT_BALANCE_SUPPORT_NOT_PORT
#else
#if CONFIG_CPU_BR50 || CONFIG_CPU_BR52
#if (BT_RF_CURRENT_BALANCE_SUPPORT_ONLY_ONE_PORT == 0)
    gpio_och_disable_output_signal(RF_RXTX_STATE_PROT, 16);
#endif
    gpio_och_disable_output_signal(RF_RXTX_STATE_PROT, 17);
#endif
#endif
#endif
    /*~~~~~~~~~~~~ end: 临时修改~~~~~~~~~~~~~~~~~~*/
    app_send_message(APP_MSG_EXIT_MODE, APP_MODE_BT);
    return 0;
}

int bt_app_msg_handler(int *msg)
{
    u8 data[1] = {0};
    u8 temp_a2dp_addr[6] = {0};
    u8 temp_call_addr[6] = {0};
    u8 *bt_a2dp_addr = NULL;
    u8 *bt_esco_addr = NULL;
    if (!app_in_mode(APP_MODE_BT)) {
        return 0;
    }
    if (a2dp_player_get_btaddr(temp_a2dp_addr)) {
        bt_a2dp_addr = temp_a2dp_addr;
        printf("temp_a2dp_addr:\n");
        put_buf(temp_a2dp_addr, 6);
    }
    if (esco_player_get_btaddr(temp_call_addr)) {
        bt_esco_addr = temp_call_addr;
        printf("temp_call_addr:\n");
        put_buf(temp_call_addr, 6);
    }

#if (LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG)
    g_bt_hdl.control_device_type = (is_cig_music_play() && !a2dp_player_runing()) ? CONTROL_CIS : CONTROL_EDR;
    log_info("g_bt_hdl.control_device_type:%d\n", g_bt_hdl.control_device_type);
#endif

    int state = bt_get_call_status();
    switch (msg[0]) {
    case APP_MSG_VOL_UP:
        log_info("APP_MSG_VOL_UP\n");
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
        app_audio_volume_up(1);
#else
        bt_volume_up(1);
#endif
#if TCFG_USER_TWS_ENABLE
        bt_tws_sync_volume();
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        data[0] = CIG_EVENT_OPID_VOLUME_UP;
        le_audio_media_control_cmd(data, 1);
#endif
        return 0;
    case APP_MSG_VOL_DOWN:
        log_info("APP_MSG_VOL_DOWN\n");
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
        app_audio_volume_down(1);
#else
        bt_volume_down(1);
#endif
#if TCFG_USER_TWS_ENABLE
        bt_tws_sync_volume();
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        data[0] = CIG_EVENT_OPID_VOLUME_DOWN;
        le_audio_media_control_cmd(data, 1);
#endif
        return 0;
    }
#if JL_UNICAST_DUAL_UAC_ENABLE
    switch (msg[0]) {
    case APP_MSG_UAC0_VOL_DOWN:
        log_info("APP_MSG_UAC0_VOL_DOWN\n");
        app_var.uac0_vol = (app_var.uac0_vol >= 5) ? (app_var.uac0_vol - 5) : 0;
        le_audio_set_dual_uac_vol(app_var.uac0_vol, app_var.uac1_vol);
        return 0;
    case APP_MSG_UAC0_VOL_UP:
        log_info("APP_MSG_UAC0_VOL_UP\n");
        app_var.uac0_vol = (app_var.uac0_vol <= 95) ? (app_var.uac0_vol + 5) : 100;
        le_audio_set_dual_uac_vol(app_var.uac0_vol, app_var.uac1_vol);
        return 0;
    case APP_MSG_UAC1_VOL_DOWN:
        log_info("APP_MSG_UAC1_VOL_DOWN\n");
        app_var.uac1_vol = (app_var.uac1_vol >= 5) ? (app_var.uac1_vol - 5) : 0;
        le_audio_set_dual_uac_vol(app_var.uac0_vol, app_var.uac1_vol);
        return 0;
    case APP_MSG_UAC1_VOL_UP:
        log_info("APP_MSG_UAC1_VOL_UP\n");
        app_var.uac1_vol = (app_var.uac1_vol <= 95) ? (app_var.uac1_vol + 5) : 100;
        le_audio_set_dual_uac_vol(app_var.uac0_vol, app_var.uac1_vol);
        return 0;
    }
#endif
    /* 下面是蓝牙相关消息,从机不用处理  */
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role_async() == TWS_ROLE_SLAVE) {
        return 0;
    }
#endif
    switch (msg[0]) {
    case APP_MSG_MUSIC_PP:
    case APP_MSG_MUSIC_NEXT:
    case APP_MSG_MUSIC_PREV:
        bt_send_a2dp_cmd(msg[0]);
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        bt_send_jl_cis_cmd(msg[0]);
#endif
        break;
    case APP_MSG_CALL_ANSWER:
        if (bt_esco_addr) {
            if (bt_get_call_status() == BT_CALL_INCOMING) {
                log_info("APP_MSG_CALL_ANSWER: esco playing, device_addr:\n");
                put_buf(temp_call_addr, 6);
                bt_cmd_prepare_for_addr(temp_call_addr, USER_CTRL_HFP_CALL_ANSWER, 0, NULL);		// 根据哪个设备使用esco接听哪个
                break;
            }
        } else {
            if (bt_get_call_status() == BT_CALL_INCOMING) {
                log_info("APP_MSG_CALL_ANSWER: esco no playing\n");
                bt_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
                break;
            }
        }
        break;
    case APP_MSG_CALL_HANGUP:
        if (bt_esco_addr) {
            log_info("APP_MSG_CALL_HANGUP: current esco playing\n");		// 根据哪个设备使用esco挂断哪个
            put_buf(temp_call_addr, 6);
            bt_cmd_prepare_for_addr(temp_call_addr, USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
            break;
        } else {
            u8 *addr = bt_get_current_remote_addr();
            if (addr) {
                memcpy(temp_call_addr, addr, 6);
                u8 call_status = bt_get_call_status_for_addr(temp_call_addr);
                if ((call_status >= BT_CALL_INCOMING) && (call_status <= BT_CALL_ACTIVE)) {
                    log_info("APP_MSG_CALL_HANGUP: current addr\n");
                    put_buf(temp_call_addr, 6);
                    bt_cmd_prepare_for_addr(temp_call_addr, USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
                    break;
                }
                u8 *other_conn_addr;
                other_conn_addr = btstack_get_other_dev_addr(addr);
                if (other_conn_addr) {
                    log_info("APP_MSG_CALL_HANGUP: other addr\n");
                    memcpy(temp_call_addr, other_conn_addr, 6);
                    put_buf(temp_call_addr, 6);
                    call_status = bt_get_call_status_for_addr(temp_call_addr);
                    if ((call_status >= BT_CALL_INCOMING) && (call_status <= BT_CALL_ACTIVE)) {
                        bt_cmd_prepare_for_addr(temp_call_addr, USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
                        break;
                    }
                }
            }
        }
        break;
    case APP_MSG_CALL_LAST_NO:
        puts("APP_MSG_CALL_LAST_NO\n");
        bt_key_call_last_on();
        break;
    case APP_MSG_HID_CONTROL:
        // 拍照
        puts("APP_MSG_HID_CONTROL\n");
        if (bt_get_curr_channel_state() & HID_CH) {
            log_info("send USER_CTRL_HID_IOS\n");
            bt_cmd_prepare(USER_CTRL_HID_IOS, 0, NULL);
        }
        break;
    case APP_MSG_OPEN_SIRI:
        puts("APP_MSG_OPEN_SIRI\n");
        if (bt_esco_addr) {
            bt_cmd_prepare_for_addr(temp_call_addr, USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL);
        } else if (bt_a2dp_addr) {
            bt_cmd_prepare_for_addr(temp_a2dp_addr, USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL);
        } else {
            bt_cmd_prepare(USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL);
        }
        break;
    case APP_MSG_CLOSE_SIRI:
        puts("APP_MSG_CLOSE_SIRI\n");
        if (bt_esco_addr) {
            bt_cmd_prepare_for_addr(temp_call_addr, USER_CTRL_HFP_GET_SIRI_CLOSE, 0, NULL);
        } else if (bt_a2dp_addr) {
            bt_cmd_prepare_for_addr(temp_a2dp_addr, USER_CTRL_HFP_GET_SIRI_CLOSE, 0, NULL);
        } else {
            bt_cmd_prepare(USER_CTRL_HFP_GET_SIRI_CLOSE, 0, NULL);
        }
        break;
    case APP_MSG_LOW_LANTECY:
        if (bt_get_low_latency_mode() == 0) {
            bt_enter_low_latency_mode();
        } else {
            bt_exit_low_latency_mode();
        }
        break;
    case APP_MSG_BT_OPEN_DUT:
        puts("APP_MSG_BT_OPEN_DUT\n");
        bt_bredr_enter_dut_mode(0, 0);
        break;
#if TCFG_USER_TWS_ENABLE
    case APP_MSG_TWS_POWER_OFF:
        puts("APP_MSG_TWS_POWER_OFF\n");
        tws_sync_poweroff();
        break;
#endif
    case APP_MSG_KEY_POWER_OFF:
        puts("APP_MSG_KEY_POWER_OFF\n");
#if TCFG_USER_TWS_ENABLE
        tws_sync_poweroff();
#else
        sys_enter_soft_poweroff(POWEROFF_NORMAL);
#endif
        break;
    case APP_MSG_REC_PP:
#if TCFG_MIX_RECORD_ENABLE
        printf("\n APP_MSG_REC_PP \n");
        if (get_mix_recorder_status()) {
            mix_recorder_stop();
        } else {
            if (dev_manager_get_phy_logo(dev_manager_find_active(0))) {
                mix_recorder_start();
            }
        }
#endif
        break;
    case APP_MSG_DEL_ALL_REMOTE_DEV:
        puts("APP_MSG_DEL_ALL_REMOTE_DEV\n");
        if (bt_get_total_connect_dev()) {
            bt_cmd_prepare(USER_CTRL_DISCONNECTION_HCI, 0, NULL);
        }
        bt_cmd_prepare(USER_CTRL_DEL_ALL_REMOTE_INFO, 0, NULL);
        sys_enter_soft_poweroff(POWEROFF_NORMAL);
        break;
#if TCFG_LP_EARTCH_KEY_ENABLE
    case APP_MSG_EARTCH_IN_EAR:
        puts("APP_MSG_EARTCH_IN_EAR\n");
        if (state != BT_CALL_HANGUP) {
            break;
        }
        if (bt_share_call_a2dp(DATA_ID_SHARE_TO_SHARE_PP)) {
            break;
        }
        bt_cmd_prepare_for_addr(bt_a2dp_addr, USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);   // 入耳播歌
        break;
    case APP_MSG_EARTCH_OUT_EAR:
        puts("APP_MSG_EARTCH_OUT_EAR\n");
        if (state != BT_CALL_HANGUP) {
            break;
        }
        if (bt_share_call_a2dp(DATA_ID_SHARE_TO_SHARE_PP)) {
            break;
        }
        bt_cmd_prepare_for_addr(bt_a2dp_addr, USER_CTRL_AVCTP_OPID_STOP, 0, NULL);   // 出耳暂停
        break;
#endif
    default:
        break;
    }

    return 0;
}


struct app_mode *app_enter_bt_mode(int arg)
{
    int msg[16] = {0};
    struct bt_event *event;
    struct app_mode *next_mode;

    bt_mode_init();

    while (1) {
        if (!app_get_message(msg, ARRAY_SIZE(msg), bt_mode_key_table)) {
            continue;
        }
        next_mode = app_mode_switch_handler(msg);
        if (next_mode) {
            break;
        }

        event = (struct bt_event *)(msg + 1);

        switch (msg[0]) {
#if TCFG_USER_TWS_ENABLE
        case MSG_FROM_TWS:
            bt_tws_connction_status_event_handler(msg + 1);
            break;
#endif
        case MSG_FROM_BT_STACK:
            bt_connction_status_event_handler(event);
#if TCFG_BT_DUAL_CONN_ENABLE
            bt_dual_phone_call_msg_handler(msg + 1);
#endif
            break;
        case MSG_FROM_BT_HCI:
            bt_hci_event_handler(event);
            break;
        case MSG_FROM_APP:
            bt_app_msg_handler(msg + 1);
            break;
        }

        app_default_msg_handler(msg);
    }

    bt_mode_exit();

    return next_mode;
}
#if OPTIMIZATION_CONN_NOISE

extern void hw_ctl_open(void);
extern void hw_ctl_close(void);

extern void rx_st_ctl(void);
extern void tx_st_ctl(void);
#ifdef BT_RF_CURRENT_BALANCE_SUPPORT_NOT_PORT
enum bb_irq_edge {
    BB_IRQ_EDGE_FALL = 0,
    BB_IRQ_EDGE_RISE = 1,
    BB_IRQ_EDGE_DISABLE = 2,
    WL_LNAE = 9,
};

AT(.volatile_ram_code)
___interrupt
void bt_edge_isr()
{
    SFR(BT_TIMER->CON, 14, 1, 1);  //clean pnding
    if ((BT_TIMER->CON & 0x3) == 0b10) { //rise edge mode
        //raise edge
        SFR(BT_TIMER->CON, 0, 2, 0b11);  //fall edge capture
        rx_st_ctl();
    } else {
        //fall edge
        SFR(BT_TIMER->CON, 0, 2, 0b10);  //rise edge capture
        tx_st_ctl();
    }
}
int btbb_irq_config(u8 btbb_sig, u8 irq_edge)
{
#ifdef CONFIG_CPU_BR56
    SFR(JL_IOMC->ICH_IOMC1, 0, 5, btbb_sig);   //set lna_en to tmr2_cap
#else
    SFR(JL_IOMC->ICH_IOMC1, 15, 5, btbb_sig);
#endif
    BT_TIMER->CON              = (\
                                  /* dual mode     */ (0 << 16) | \
                                  /* clear pnd     */ (1 << 14) | \
                                  /* pwm inv       */ (0 <<  9) | \
                                  /* psel          */ (0 <<  8) | \
                                  /* capture sel 2 */ (0 <<  2) | \
                                  /* mode        2 */ (0 <<  0));
    BT_TIMER->CNT = 0x5555;
    BT_TIMER->PRD = 0;
    if (irq_edge == 0) {                                //set tmr cap as fall edge
        request_irq(BT_IRQ_TIME_IDX, 1, bt_edge_isr, 0);
        irq_unmask_set(BT_IRQ_TIME_IDX, 1, 0);
        SFR(BT_TIMER->CON, 0, 2, 0b11);                //fall edge
    } else if (irq_edge == 1) {                          //set tmr cap as rise edge
        request_irq(BT_IRQ_TIME_IDX, 1, bt_edge_isr, 0);
        irq_unmask_set(BT_IRQ_TIME_IDX, 1, 0);
        SFR(BT_TIMER->CON, 0, 2, 0b10);                //rise edge
    } else {                                             //disable tmr cap
        unrequest_irq(BT_IRQ_TIME_IDX);
        SFR(BT_TIMER->CON, 0, 2, 0);  //fall edge
    }
    return 0;
}
#else
void gpio_irq_callback_rxen_pa_ctl(enum gpio_port port, u32 pin, enum gpio_irq_edge edge)
{
    if (edge == PORT_IRQ_EDGE_RISE) {
        //raise edge
        gpio_irq_set_edge(port, BIT(pin), PORT_IRQ_EDGE_FALL);
        //printf("rx raise\n");
        rx_st_ctl();
    } else {
        //fall edge
        gpio_irq_set_edge(port, BIT(pin), PORT_IRQ_EDGE_RISE);
        //printf("rx fall\n");
        tx_st_ctl();
    }
}

struct gpio_irq_config_st gpio_irq_config_rx = {
    .pin = EDGE_SLECT_POART_IO,
    .irq_edge = PORT_IRQ_EDGE_FALL,
    .callback = gpio_irq_callback_rxen_pa_ctl
};

struct gpio_irq_config_st gpio_irq_config_rx_off = {
    .pin = EDGE_SLECT_POART_IO,
    .irq_edge = PORT_IRQ_DISABLE,
    .callback = gpio_irq_callback_rxen_pa_ctl
};
#endif

void bt_user_page_enable(u8 enable, u8 type)
{
    /*y_printf("bt_user_page_enable=%d,type=%d", enable, type); */
    if (type == 1) { //tws quick page no doning
        return;
    }
    if (enable) {
        hw_ctl_close();
#ifdef BT_RF_CURRENT_BALANCE_SUPPORT_NOT_PORT
        btbb_irq_config(WL_LNAE, BB_IRQ_EDGE_RISE);        //9 lna_en
#else
        gpio_irq_config(EDGE_SLECT_PORART, &gpio_irq_config_rx_off);
        gpio_irq_config(EDGE_SLECT_PORART, &gpio_irq_config_rx);
#if BT_RF_CURRENT_BALANCE_SUPPORT_ONLY_ONE_PORT
        EDGE_SLECT_POART_OPEN
#endif
#endif
        /*y_printf("gpio_irq_config open-----+++++"); */
    } else {
#ifdef BT_RF_CURRENT_BALANCE_SUPPORT_NOT_PORT
        btbb_irq_config(WL_LNAE, BB_IRQ_EDGE_DISABLE);    //9 lna_en
#else
        gpio_irq_config(EDGE_SLECT_PORART, &gpio_irq_config_rx_off);
#if BT_RF_CURRENT_BALANCE_SUPPORT_ONLY_ONE_PORT
        EDGE_SLECT_POART_CLOSE
#endif
#endif
        hw_ctl_open();
        /* y_printf("btbb_irq_config close-----+++++"); */
    }
}
#endif
static const struct app_mode_ops bt_mode_ops = {
    .try_exit       = bt_mode_try_exit,
};

REGISTER_APP_MODE(bt_mode) = {
    .name   = APP_MODE_BT,
    .index  = APP_MODE_BT_INDEX,
    .ops    = &bt_mode_ops,
};

#endif
