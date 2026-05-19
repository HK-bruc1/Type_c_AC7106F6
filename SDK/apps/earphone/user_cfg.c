#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".user_cfg.data.bss")
#pragma data_seg(".user_cfg.data")
#pragma const_seg(".user_cfg.text.const")
#pragma code_seg(".user_cfg.text")
#endif
#include "app_config.h"
#include "user_cfg.h"
#include "fs.h"
#include "string.h"
#include "system/includes.h"
#include "vm.h"
#include "btcontroller_config.h"
#include "app_main.h"
#include "media/includes.h"
#include "audio_config.h"
#include "audio_cvp.h"
#include "app_power_manage.h"
#include "sdk_config.h"
#include "config/bt_name_parse.h"
#include "classic/hci_lmp.h"
#if TCFG_CHARGE_CALIBRATION_ENABLE
#include "asm/charge_calibration.h"
#endif
#include "scene_switch.h"
#include "volume_node.h"

#define LOG_TAG             "[USER_CFG]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

void app_set_sys_vol(s16 vol_l, s16  vol_r);

BT_CONFIG bt_cfg = {
    .edr_name        = {'Y', 'L', '-', 'B', 'R', '3', '0'},
    .mac_addr        = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    .tws_local_addr  = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    .rf_power        = 10,
    .tws_device_indicate = 0x6688,
};

//======================================================================================//
//                                 		BTIF配置项表                               		//
//	参数1: 配置项名字                                			   						//
//	参数2: 配置项需要多少个byte存储														//
//	说明: 配置项ID注册到该表后该配置项将读写于BTIF区域, 其它没有注册到该表       		//
//		  的配置项则默认读写于VM区域.													//
//======================================================================================//
const struct btif_item btif_table[] = {
// 	 	item id 		   	   len   	//
    {CFG_BT_MAC_ADDR, 			6 },
    {CFG_BT_FRE_OFFSET,   		6 },   //测试盒矫正频偏值
    {CFG_BT_FRE_OFFSET1,   		6 },   //测试盒矫正频偏值
    {CFG_BT_FRE_OFFSET2,   		6 },   //测试盒矫正频偏值
    {CFG_BT_FRE_OFFSET3,   		6 },   //测试盒矫正频偏值
    {CFG_BT_FRE_OFFSET4,   		6 },   //测试盒矫正频偏值
#if TCFG_CHARGE_CALIBRATION_ENABLE
    {VM_CHARGE_CALIBRATION,     sizeof(calibration_result) },   //充电电流校准
#endif
    //{CFG_DAC_DTB,   			2 },
    //{CFG_MC_BIAS,   			1 },
    {0, 						0 },   //reserved cfg
};

#ifndef CONFIG_LRC_WIN_SIZE
#define CONFIG_LRC_WIN_SIZE 400 // LRC窗口初值
#endif
#ifndef CONFIG_LRC_WIN_STEP
#define CONFIG_LRC_WIN_STEP 400 // LRC窗口步进
#endif
#ifndef CONFIG_OSC_WIN_SIZE
#define CONFIG_OSC_WIN_SIZE 400 // OSC窗口初值
#endif
#ifndef CONFIG_OSC_WIN_STEP
#define CONFIG_OSC_WIN_STEP 400 // OSC窗口步进
#endif
const struct lp_ws_t lp_winsize = {
    .lrc_ws_inc = CONFIG_LRC_WIN_STEP,      //260
    .lrc_ws_init = CONFIG_LRC_WIN_SIZE,
    .bt_osc_ws_inc = CONFIG_OSC_WIN_STEP,
    .bt_osc_ws_init = CONFIG_OSC_WIN_SIZE,
    .osc_change_mode = 1,                       //低功耗时钟，0：仅使用LRC 1：自动切换 2：保留
};

u16 bt_get_tws_device_indicate(u8 *tws_device_indicate)
{
    u16 crc = 0;
#if CONFIG_TWS_DIFF_NAME_NOT_MATCH
    crc = CRC16(bt_get_local_name(), strlen(bt_get_local_name()));
#endif
    return bt_cfg.tws_device_indicate + crc;
}

const u8 *bt_get_mac_addr()
{
    return bt_cfg.mac_addr;
}

void bt_update_mac_addr(u8 *addr)
{
    memcpy(bt_cfg.mac_addr, addr, 6);
}

static u8 bt_mac_addr_for_testbox[6] = {0};
void bt_get_vm_mac_addr(u8 *addr)
{
#if 0
    //中断不能调用syscfg_read;
    int ret = 0;

    ret = syscfg_read(CFG_BT_MAC_ADDR, addr, 6);
    if ((ret != 6)) {
        syscfg_write(CFG_BT_MAC_ADDR, addr, 6);
    }
#else

    memcpy(addr, bt_mac_addr_for_testbox, 6);
#endif
}

void bt_get_tws_local_addr(u8 *addr)
{
    memcpy(addr, bt_cfg.tws_local_addr, 6);
}

const char *sdk_version_info_get(void)
{
    extern u32 __VERSION_BEGIN;
    char *version_str = ((char *)&__VERSION_BEGIN) + 4;

    return version_str;
}

const char *bt_get_local_name()
{
    return (const char *)(bt_cfg.edr_name);
}

const char *bt_get_pin_code()
{
    return "0000";
}


#define USE_CONFIG_BIN_FILE                  0

#define USE_CONFIG_STATUS_SETTING            1                          //状态设置，包括灯状态和提示音
#define USE_CONFIG_AUDIO_SETTING             USE_CONFIG_BIN_FILE        //音频设置
#define USE_CONFIG_KEY_SETTING               USE_CONFIG_BIN_FILE        //按键消息设置
#define USE_CONFIG_MIC_TYPE_SETTING          USE_CONFIG_BIN_FILE        //MIC类型设置
#define USE_CONFIG_AUTO_OFF_SETTING          USE_CONFIG_BIN_FILE        //自动关机时间设置
#define USE_CONFIG_VOL_SETTING               1					        //音量表读配置


__INITCALL_BANK_CODE
void cfg_file_parse(u8 idx)
{
    u8 tmp[128] = {0};
    int ret = 0;

    memset(tmp, 0x00, sizeof(tmp));

    /*************************************************************************/
    /*                      CFG READ IN cfg_tools.bin                        */
    /*************************************************************************/


    //-----------------------------CFG_BT_NAME--------------------------------------//
    ret = syscfg_read(CFG_BT_NAME, tmp, 32);
    if (ret < 0) {
        log_info("read bt name err");
    } else if (ret >= LOCAL_NAME_LEN) {
        memset(bt_cfg.edr_name, 0x00, LOCAL_NAME_LEN);
        memcpy(bt_cfg.edr_name, tmp, LOCAL_NAME_LEN);
        bt_cfg.edr_name[LOCAL_NAME_LEN - 1] = 0;
    } else {
        memset(bt_cfg.edr_name, 0x00, LOCAL_NAME_LEN);
        memcpy(bt_cfg.edr_name, tmp, ret);
    }
#if TCFG_BT_NAME_SEL_BY_AD_ENABLE
    bt_name_config_parse((char *)bt_cfg.edr_name);
#endif

    /* g_printf("bt name config:%s", bt_cfg.edr_name); */
    log_info("bt name config:%s", bt_cfg.edr_name);

    //-----------------------------CFG_TWS_PAIR_CODE_ID----------------------------//
    ret = syscfg_read(CFG_TWS_PAIR_CODE_ID, &bt_cfg.tws_device_indicate, 2);
    if (ret < 0) {
        log_debug("read pair code err");
        bt_cfg.tws_device_indicate = 0x8888;
    }
    /* g_printf("tws pair code config:"); */
    log_info("tws pair code config:");
    put_buf((void *)&bt_cfg.tws_device_indicate, 2);

    //-----------------------------CFG_BT_RF_POWER_ID----------------------------//
    ret = syscfg_read(CFG_BT_RF_POWER_ID, &app_var.rf_power, 1);
    if (ret < 0) {
        log_debug("read rf err");
        app_var.rf_power = 10;
    }
    int ble_power = 9;
#ifdef TCFG_BT_BLE_TX_POWER
    ble_power = TCFG_BT_BLE_TX_POWER;
#endif
    //page /inquiry_scan 功率限制在另外一个地方配置CONFIG_PAGE_POWER/ CONFIG_PAGE_SCAN_POWER
    bt_max_pwr_set(app_var.rf_power, 0, 0, ble_power);//last is ble tx_pwer(0~9)
    /* g_printf("rf config:%d\n", app_var.rf_power); */
    log_info("rf config:%d\n", app_var.rf_power);

    //-----------------------------CFG MIC ARRAY TRIM-------------------------------------//
#if TCFG_AUDIO_MIC_ARRAY_TRIM_ENABLE
    ret = syscfg_read(CFG_MIC_ARRAY_TRIM_EN, (u8 *)(&app_var.audio_mic_array_trim_en), sizeof(app_var.audio_mic_array_trim_en));
    if (ret != sizeof(app_var.audio_mic_array_trim_en)) {
        log_info("audio_mic_array_trim_en vm read fail %d, %d!!!", ret, sizeof(app_var.audio_mic_array_trim_en));
    }
    ret = syscfg_read(CFG_MIC_ARRAY_DIFF_CMP_VALUE, (u8 *)(&app_var.audio_mic_array_diff_cmp), sizeof(app_var.audio_mic_array_diff_cmp));
    if (ret != sizeof(app_var.audio_mic_array_diff_cmp)) {
        log_info("audio_mic_array_diff_cmp vm read fail  %d, %d!!!", ret, sizeof(app_var.audio_mic_array_diff_cmp));
        app_var.audio_mic_array_diff_cmp = 1.0f;
    }
    ret = syscfg_read(CFG_MIC_TARGET_DIFF_CMP, (u8 *)(&app_var.audio_mic_cmp), sizeof(audio_mic_cmp_t));
    if (ret != sizeof(audio_mic_cmp_t)) {
        log_info("audio_mic_array_diff_cmp vm read fail  %d, %d!!!", ret, sizeof(audio_mic_cmp_t));
        app_var.audio_mic_cmp.talk = 1.0f;
        app_var.audio_mic_cmp.ff = 1.0f;
        app_var.audio_mic_cmp.fb = 1.0f;
    }
    log_info("mic array trim en: %d\n", app_var.audio_mic_array_trim_en);
    log_info("mic_array_diff_cmp:");
    put_float(app_var.audio_mic_array_diff_cmp);

    log_info("mic_cmp-talk:");
    put_float(app_var.audio_mic_cmp.talk);
    log_info("mic_cmp-ff:");
    put_float(app_var.audio_mic_cmp.ff);
    log_info("mic_cmp-fb:");
    put_float(app_var.audio_mic_cmp.fb);
#endif

    s16 default_volume;
    s16 music_volume = -1;
    struct volume_cfg music_vol_cfg;
    struct volume_cfg call_vol_cfg;
    struct volume_cfg tone_vol_cfg;
    struct volume_cfg ktone_vol_cfg;
    struct volume_cfg ring_vol_cfg;
    //赋予相关变量初值
    volume_ioc_get_cfg("Vol_BtmMusic", &music_vol_cfg);
    volume_ioc_get_cfg("Vol_BtcCall", &call_vol_cfg);
    volume_ioc_get_cfg("Vol_SysTone", &tone_vol_cfg);
    volume_ioc_get_cfg("Vol_SysKTone", &ktone_vol_cfg);
    volume_ioc_get_cfg("Vol_SysRing", &ring_vol_cfg);

    ret = syscfg_read(CFG_SYS_VOL, &default_volume, 2);
    if (ret < 0) {
        default_volume = music_vol_cfg.cur_vol;
    }
    ret = syscfg_read(CFG_MUSIC_VOL, &music_volume, 2);
    if (ret < 0) {
        printf("CFG_MUSIC_VOL VM null value\n");
        music_volume = -1;
    }
    //读不到VM，则表示当前音量使用默认状态
    app_var.volume_def_state = music_volume == -1 ? 1 : 0;
    app_var.music_volume = music_volume == -1 ? default_volume : music_volume;
    app_var.wtone_volume = tone_vol_cfg.cur_vol;
    app_var.ktone_volume = ktone_vol_cfg.cur_vol;
    app_var.ring_volume = ring_vol_cfg.cur_vol;
    app_var.call_volume = call_vol_cfg.cur_vol;
    app_var.aec_dac_gain = call_vol_cfg.cfg_level_max;
    //主要用于BT音量同步
    app_var.opid_play_vol_sync = default_volume * 127 / music_vol_cfg.cfg_level_max;

    printf("vol_init call %d, music %d, tone %d\n", app_var.call_volume, app_var.music_volume, app_var.wtone_volume);

    ret = syscfg_read(CFG_MIC_EFF_VOLUME_INDEX, &app_var.mic_eff_volume, 2);
    if (ret < 0) {
        app_var.mic_eff_volume = 100;
    }

#if TCFG_POWER_WARN_VOLTAGE
    app_var.warning_tone_v = TCFG_POWER_WARN_VOLTAGE;
#else
    app_var.warning_tone_v = 3300;
#endif
#if TCFG_POWER_OFF_VOLTAGE
    app_var.poweroff_tone_v = TCFG_POWER_OFF_VOLTAGE;
#else
    app_var.warning_tone_v = 3400;
#endif
    log_info("warning_tone_v:%d poweroff_tone_v:%d", app_var.warning_tone_v, app_var.poweroff_tone_v);

#if USE_CONFIG_AUTO_OFF_SETTING
    /* g_printf("auto off time config:"); */
    log_info("auto off time config:");
    AUTO_OFF_TIME_CONFIG auto_off_time;
    ret = syscfg_read(CFG_AUTO_OFF_TIME_ID, &auto_off_time, sizeof(AUTO_OFF_TIME_CONFIG));
    if (ret > 0) {
        app_var.auto_off_time = auto_off_time.auto_off_time * 60;
    }
    log_info("auto_off_time:%d", app_var.auto_off_time);
#else
    app_var.auto_off_time =  TCFG_AUTO_SHUT_DOWN_TIME;
    log_info("auto_off_time:%d", app_var.auto_off_time);
#endif

    /*************************************************************************/
    /*                      CFG READ IN VM                                   */
    /*************************************************************************/
    u8 mac_buf[6];
    u8 mac_buf_tmp[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    u8 mac_buf_tmp2[6] = {0, 0, 0, 0, 0, 0};
#if TCFG_USER_TWS_ENABLE
    int len = syscfg_read(CFG_TWS_LOCAL_ADDR, bt_cfg.tws_local_addr, 6);
    if (len != 6) {
        get_random_number(bt_cfg.tws_local_addr, 6);
        syscfg_write(CFG_TWS_LOCAL_ADDR, bt_cfg.tws_local_addr, 6);
    }
    log_info("tws_local_mac:");
    put_buf(bt_cfg.tws_local_addr, sizeof(bt_cfg.tws_local_addr));

#if CONFIG_TWS_COMMON_ADDR_SELECT != CONFIG_TWS_COMMON_ADDR_USED_MASTER
    ret = syscfg_read(CFG_TWS_COMMON_ADDR, mac_buf, 6);
    if (ret != 6 || !memcmp(mac_buf, mac_buf_tmp, 6))
#endif
#endif
        do {
            ret = syscfg_read(CFG_BT_MAC_ADDR, mac_buf, 6);
            if ((ret != 6) || !memcmp(mac_buf, mac_buf_tmp, 6) || !memcmp(mac_buf, mac_buf_tmp2, 6)) {
                get_random_number(mac_buf, 6);
                syscfg_write(CFG_BT_MAC_ADDR, mac_buf, 6);
            }
        } while (0);


    syscfg_read(CFG_BT_MAC_ADDR, bt_mac_addr_for_testbox, 6);
    if (!memcmp(bt_mac_addr_for_testbox, mac_buf_tmp, 6)) {
        get_random_number(bt_mac_addr_for_testbox, 6);
        syscfg_write(CFG_BT_MAC_ADDR, bt_mac_addr_for_testbox, 6);
        log_info(">>>init mac addr!!!\n");
    }

    log_info("mac:");
    put_buf(mac_buf, sizeof(mac_buf));
    memcpy(bt_cfg.mac_addr, mac_buf, 6);

#if (CONFIG_BT_MODE != BT_NORMAL)
    const u8 dut_name[]  = "AC693x_DUT";
    const u8 dut_addr[6] = {0x12, 0x34, 0x56, 0x56, 0x34, 0x12};
    memcpy(bt_cfg.edr_name, dut_name, sizeof(dut_name));
    memcpy(bt_cfg.mac_addr, dut_addr, 6);
#endif

    /*************************************************************************/
    /*                      CFG READ IN isd_config.ini                       */
    /*************************************************************************/
    /* LRC_CONFIG lrc_cfg; */
    /* ret = syscfg_read(CFG_LRC_ID, &lrc_cfg, sizeof(LRC_CONFIG)); */
    /* if (ret > 0) { */
    /*     log_info("lrc cfg:"); */
    /*     put_buf((void *)&lrc_cfg, sizeof(LRC_CONFIG)); */
    /*     lp_winsize.lrc_ws_inc      = lrc_cfg.lrc_ws_inc; */
    /*     lp_winsize.lrc_ws_init     = lrc_cfg.lrc_ws_init; */
    /*     lp_winsize.bt_osc_ws_inc   = lrc_cfg.btosc_ws_inc; */
    /*     lp_winsize.bt_osc_ws_init  = lrc_cfg.btosc_ws_init; */
    /*     lp_winsize.osc_change_mode = lrc_cfg.lrc_change_mode; */
    /* } */
    /* printf("%d %d %d ",lp_winsize.lrc_ws_inc,lp_winsize.lrc_ws_init,lp_winsize.osc_change_mode); */
    lp_winsize_init(&lp_winsize);
}

int bt_modify_name(u8 *new_name)
{
    u8 new_len = strlen((void *)new_name);

    if (new_len >= LOCAL_NAME_LEN) {
        new_name[LOCAL_NAME_LEN - 1] = 0;
    }

    if (strcmp((void *)new_name, (void *)bt_cfg.edr_name)) {
        syscfg_write(CFG_BT_NAME, new_name, LOCAL_NAME_LEN);
        memcpy(bt_cfg.edr_name, new_name, LOCAL_NAME_LEN);
        lmp_hci_write_local_name(bt_get_local_name());
        log_info("mdy_name sucess\n");
        return 1;
    }
    return 0;
}


