/*********************************************************************************************
    *   Filename        : btctrler_config.c

    *   Description     : Optimized Code & RAM (编译优化配置)

    *   Author          : Bingquan

    *   Email           : caibingquan@zh-jieli.com

    *   Last modifiled  : 2019-03-16 11:49

    *   Copyright:(c)JIELI  2011-2019  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "system/includes.h"
#include "btcontroller_config.h"
#include "bt_common.h"
#include "classic/tws_api.h"

// *INDENT-OFF*
/**
 * @brief Bluetooth Module
 */
#if TCFG_BT_DONGLE_ENABLE
	const int CONFIG_DONGLE_SPEAK_ENABLE  = 1;//dongle_slave
#else
	const int CONFIG_DONGLE_SPEAK_ENABLE  = 0;//dongle slave
#endif
const int CONFIG_BTCTLER_JL_DONGLE_SOURCE_ENABLE=0;//dongle master
const int config_master_qos_poll=0;
#if TCFG_BT_DUAL_CONN_ENABLE
#if TCFG_BT_DUAL_1T3_CONN_ENABLE//1t3
const int CONFIG_1T3_CONNECTION_ENABLE = 1;
const int CONFIG_LMP_CONNECTION_NUM = 3;
const int CONFIG_LMP_CONNECTION_LIMIT_NUM = 3;
#else
const int CONFIG_1T3_CONNECTION_ENABLE = 0;
const int CONFIG_LMP_CONNECTION_NUM = 2;
const int CONFIG_LMP_CONNECTION_LIMIT_NUM = 2;
#endif
#else
const int CONFIG_1T3_CONNECTION_ENABLE = 0;
const int CONFIG_LMP_CONNECTION_NUM = 1;
const int CONFIG_LMP_CONNECTION_LIMIT_NUM = 1;
#endif

const int CONFIG_DISTURB_SCAN_ENABLE = 0;

#define TWS_PURE_MONITOR_MODE    0//1:纯监听模式

#if TWS_PURE_MONITOR_MODE
	u8 get_extws_nack_adjust(u8 per_v, int a2dp_dly_paly_time, int msec, int num)
	{
		return 0;
	}
#endif
const int CONFIG_TWS_PURE_MONITOR_MODE = TWS_PURE_MONITOR_MODE; /*自适应延时策略使用*/

#if TCFG_TWS_AUDIO_SHARE_ENABLE
	const int CONFIG_TWS_AUDIO_SHARE_ENABLE  = 1;
#else
	const int CONFIG_TWS_AUDIO_SHARE_ENABLE  = 0;
#endif

/* const int CONFIG_TWS_FORWARD_TIMES = 1; */
const int CONFIG_TWS_FORWARD_TIMES_NEW = 8;
const int CONFIG_TWS_RUN_SLOT_MAX = 48;
const int CONFIG_TWS_RUN_SLOT_AT_A2DP_FORWARD = 10;
const int CONFIG_TWS_RUN_SLOT_AT_LOW_LATENCY = 8;
const int CONFIG_TWS_RUN_SLOT_AT_LOCAL_MEDIA_TRANS = 48;

#ifdef TCFG_LE_AUDIO_PLAY_LATENCY
const int CONFIG_LE_AUDIO_PLAY_LATENCY = TCFG_LE_AUDIO_PLAY_LATENCY; // le_audio延时（us）
#else
const int CONFIG_LE_AUDIO_PLAY_LATENCY = 0; // le_audio延时（us）
#endif

#ifdef TCFG_JL_DONGLE_PLAYBACK_LATENCY
const int CONFIG_JL_DONGLE_PLAYBACK_LATENCY = TCFG_JL_DONGLE_PLAYBACK_LATENCY; // dongle下行播放延时(msec)
#else
const int CONFIG_JL_DONGLE_PLAYBACK_LATENCY = 0; // dongle下行播放延时(msec)
#endif


//固定使用正常发射功率的等级:0-使用不同模式的各自等级;1~10-固定发射功率等级
const int config_force_bt_pwr_tab_using_normal_level  = 0;
//配置BLE广播发射功率的等级:0-最大功率等级;1~10-固定发射功率等级
const int config_ble_adv_tx_pwr_level  = 0;

//only for br52
#ifdef CONFIG_CPU_BR52
const u8 config_fre_offset_trim_mode = 1; //0:trim pll 1:trim osc 2:trim pll&osc
#else
const u8 config_fre_offset_trim_mode = 0; //0:trim pll 1:trim osc 2:trim pll&osc
#endif

const int CONFIG_BLE_SYNC_WORD_BIT = 30;
const int CONFIG_LNA_CHECK_VAL = -80;

#if TCFG_USER_TWS_ENABLE
	#if (BT_FOR_APP_EN || TCFG_USER_BLE_ENABLE)
		const int config_btctler_modules        = BT_MODULE_CLASSIC | BT_MODULE_LE;
	#else
 		#ifdef CONFIG_NEW_BREDR_ENABLE
 	    	#if (BT_FOR_APP_EN)
 		    	const int config_btctler_modules        = (BT_MODULE_CLASSIC | BT_MODULE_LE);
 	        #else
 		        const int config_btctler_modules        = (BT_MODULE_CLASSIC);
 	        #endif
        #else
 	        const int config_btctler_modules        = (BT_MODULE_CLASSIC | BT_MODULE_LE);
        #endif
    #endif


	#ifdef CONFIG_NEW_BREDR_ENABLE
		const int CONFIG_BTCTLER_FAST_CONNECT_ENABLE     = 0;

	     #if TCFG_DEC2TWS_ENABLE
	     	const int CONFIG_TWS_WORK_MODE                  = 1;
	     #else
	     	const int CONFIG_TWS_WORK_MODE                  = 2;
	     #endif

		#ifdef CONFIG_SUPPORT_EX_TWS_ADJUST
			const int CONFIG_EX_TWS_ADJUST                  = 1;
		#else
			const int CONFIG_EX_TWS_ADJUST                  = 0;
		#endif


	#else//CONFIG_NEW_BREDR_ENABLE
		const int CONFIG_BTCTLER_FAST_CONNECT_ENABLE     = 0;
	#endif//end CONFIG_NEW_BREDR_ENABLE

	const int CONFIG_BTCTLER_TWS_ENABLE     = 1;

    #if TCFG_TWS_AUTO_ROLE_SWITCH_ENABLE
		const int CONFIG_TWS_AUTO_ROLE_SWITCH_ENABLE = 1;
	#else
		const int CONFIG_TWS_AUTO_ROLE_SWITCH_ENABLE = 0;
	#endif

    const int CONFIG_TWS_POWER_BALANCE_ENABLE   = TCFG_TWS_POWER_BALANCE_ENABLE;
    const int CONFIG_LOW_LATENCY_ENABLE         = 1;
#if TCFG_LOCAL_TWS_ENABLE
    const int CONFIG_TWS_DATA_TRANS_ENABLE = 1;
#else
    const int CONFIG_TWS_DATA_TRANS_ENABLE = 0;
#endif
#else //TCFG_USER_TWS_ENABLE
	#if (TCFG_USER_BLE_ENABLE)
		const int config_btctler_modules        = BT_MODULE_CLASSIC | BT_MODULE_LE;
	#else
		const int config_btctler_modules        = BT_MODULE_CLASSIC;
	#endif

	const int config_btctler_le_tws         = 0;
	const int CONFIG_BTCTLER_TWS_ENABLE     = 0;
	const int CONFIG_LOW_LATENCY_ENABLE     = 0;
	const int CONFIG_TWS_POWER_BALANCE_ENABLE   = 0;
	const int CONFIG_BTCTLER_FAST_CONNECT_ENABLE     = 0;
    const int CONFIG_TWS_DATA_TRANS_ENABLE = 0;
#endif//end TCFG_USER_TWS_ENABLE


#if (TCFG_BT_SUPPORT_LHDC_V5 || TCFG_BT_SUPPORT_LHDC || TCFG_BT_SUPPORT_LDAC) //LHDC/LDAC使用较高码率时需要增大蓝牙buf
	const int CONFIG_EXTWS_NACK_LIMIT_INT_CNT       = 40;
#else
	#if TWS_PURE_MONITOR_MODE
		const int CONFIG_EXTWS_NACK_LIMIT_INT_CNT       = 63;
	#else
		#if defined(CONFIG_CPU_BR56) //JL710
			const int CONFIG_EXTWS_NACK_LIMIT_INT_CNT       = 3;
		#else
			const int CONFIG_EXTWS_NACK_LIMIT_INT_CNT       = 4;
		#endif
	#endif
#endif

const int CONFIG_A2DP_MAX_BUF_SIZE      = 25 * 1024;    //不再使用
const int CONFIG_A2DP_AAC_MAX_BUF_SIZE  = 20 * 1024;
const int CONFIG_A2DP_SBC_MAX_BUF_SIZE  = 25 * 1024;
const int CONFIG_A2DP_LHDC_MAX_BUF_SIZE = 50 * 1024;
const int CONFIG_A2DP_LDAC_MAX_BUF_SIZE = 50 * 1024;

u32 get_a2dp_max_buf_size(u8 codec_type)
{
    //#define A2DP_CODEC_SBC        0x00
    //#define A2DP_CODEC_MPEG12     0x01
    //#define A2DP_CODEC_MPEG24     0x02
    //#define A2DP_CODEC_ATRAC      0x03
    //#define A2DP_CODEC_LDAC       0x0B
    //#define A2DP_CODEC_LHDC_V5    0x0C
    //#define A2DP_CODEC_APTX       0x0D
    //#define A2DP_CODEC_LHDC       0x0E
    //#define A2DP_CODEC_NON_A2DP   0xFF
    u32 a2dp_max_buf_size = CONFIG_A2DP_MAX_BUF_SIZE;
    if (codec_type == 0x0) {
        a2dp_max_buf_size = CONFIG_A2DP_SBC_MAX_BUF_SIZE;
    } else if (codec_type == 0x1 || codec_type == 0x2) {
        a2dp_max_buf_size = CONFIG_A2DP_AAC_MAX_BUF_SIZE;
    } else if (codec_type == 0xB) {
        a2dp_max_buf_size = CONFIG_A2DP_LDAC_MAX_BUF_SIZE;
    } else if (codec_type == 0xE || codec_type == 0xC) {
        a2dp_max_buf_size = CONFIG_A2DP_LHDC_MAX_BUF_SIZE;
    }

    return a2dp_max_buf_size;
}

const int CONFIG_BT_DUAL_MODE_MANAGER_ENABLE = 0;
#if 0
// 可重写函数实时调试qos硬件开关状态，判断当前qos是开还是关
void user_qos_run_debug(u8 en)
{
	if(en){
		putchar('<');//qos open ing
	}else{
		putchar('>');//qos close ing
	}
}
#endif
#if 0
//可重写函数,a2dp播放时，根据当前延时，控制qos开关。延时太低关闭qos，延时高，开启qos省功耗
//delay_set_timer =CONFIG_A2DP_DELAY_TIME_AAC or CONFIG_A2DP_DELAY_TIME_SBC
//delay_lo_timer =CONFIG_A2DP_DATA_CACHE_LOW_AAC or CONFIG_A2DP_DATA_CACHE_LOW_AAC
//delay_hi_timer = CONFIG_A2DP_DATA_CACHE_HI_AAC or CONFIG_A2DP_DATA_CACHE_HI_SBC
u8 auto_check_a2dp_play_control_qos(u16 cur_delay_timer,u16 delay_set_timer,u16 delay_lo_timer,u16 delay_hi_timer)
{

	u8 ret=0;
	if ( cur_delay_timer <= delay_lo_timer || cur_delay_timer >= (delay_set_timer + 100)) {
		/* r_printf("QOS dis %d\n", msec); */
		ret = 1;
	} else if (cur_delay_timer >= delay_hi_timer) {
		ret = 2;
		/* r_printf("QOS en=%d\n",msec ); */
	}
	return ret;

}
#endif
const int CONFIG_TWS_SUPER_TIMEOUT          = 4000;
const int CONFIG_TWS_SAVE_POWER_ENABLE      = 1;     //tws省功耗策略
const int CONFIG_BTCTLER_QOS_ENABLE         = 1;
const int CONFIG_A2DP_DATA_CACHE_LOW_AAC    = 100;
const int CONFIG_A2DP_DATA_CACHE_HI_AAC     = 250;
const int CONFIG_A2DP_DATA_CACHE_LOW_SBC    = 150;
const int CONFIG_A2DP_DATA_CACHE_HI_SBC     = 260;
const int CONFIG_A2DP_DELAY_TIME_AAC = TCFG_A2DP_DELAY_TIME_AAC;
const int CONFIG_A2DP_DELAY_TIME_SBC = TCFG_A2DP_DELAY_TIME_SBC;
const int CONFIG_A2DP_DELAY_TIME_SBC_LO = TCFG_A2DP_DELAY_TIME_SBC_LO;
const int CONFIG_A2DP_DELAY_TIME_AAC_LO = TCFG_A2DP_DELAY_TIME_AAC_LO;
const int CONFIG_A2DP_ADAPTIVE_MAX_LATENCY = TCFG_A2DP_ADAPTIVE_MAX_LATENCY;
#ifdef TCFG_A2DP_DELAY_TIME_LDAC
const int CONFIG_A2DP_DELAY_TIME_LDAC = TCFG_A2DP_DELAY_TIME_LDAC;
#endif
#ifdef TCFG_A2DP_DELAY_TIME_LDAC_LO
const int CONFIG_A2DP_DELAY_TIME_LDAC_LO = TCFG_A2DP_DELAY_TIME_LDAC_LO;
#endif
#ifdef TCFG_A2DP_DELAY_TIME_LHDC
const int CONFIG_A2DP_DELAY_TIME_LHDC = TCFG_A2DP_DELAY_TIME_LHDC;
#endif
#ifdef TCFG_A2DP_DELAY_TIME_LHDC_LO
const int CONFIG_A2DP_DELAY_TIME_LHDC_LO = TCFG_A2DP_DELAY_TIME_LHDC_LO;
#endif
const int CONFIG_JL_DONGLE_PLAYBACK_DYNAMIC_LATENCY_ENABLE  = 1;    //jl_dongle 动态延时

const int CONFIG_PAGE_POWER                 = 9;
const int CONFIG_PAGE_SCAN_POWER            = 9;
const int CONFIG_PAGE_SCAN_POWER_DUT        = 4;
const int CONFIG_INQUIRY_POWER              = 7;
const int CONFIG_INQUIRY_SCAN_POWER         = 7;
const int CONFIG_DUT_POWER                  = 10;

#if (CONFIG_BT_MODE != BT_NORMAL)
	const int config_btctler_hci_standard   = 1;
#else
	const int config_btctler_hci_standard   = 0;
#endif

const int config_btctler_mode        = CONFIG_BT_MODE;
const int CONFIG_BTCTLER_TWS_FUN     = TWS_ESCO_FORWARD ; // TWS_ESCO_FORWARD

/*-----------------------------------------------------------*/

/**
 * @brief Bluetooth Classic setting
 */
const u8 rx_fre_offset_adjust_enable = 1;

const int config_bredr_fcc_fix_fre = 0;
const int ble_disable_wait_enable = 1;

const int config_btctler_eir_version_info_len = 21;

#ifdef CONFIG_256K_FLASH
	const int CONFIG_TEST_DUT_CODE            = 1;
	const int CONFIG_TEST_FCC_CODE            = 0;
	const int CONFIG_TEST_DUT_ONLY_BOX_CODE   = 1;
#else
	const int CONFIG_TEST_DUT_CODE            = 1;
	const int CONFIG_TEST_FCC_CODE            = 1;
	const int CONFIG_TEST_DUT_ONLY_BOX_CODE   = 0;
#endif//end CONFIG_256K_FLASH

const int CONFIG_ESCO_MUX_RX_BULK_ENABLE  =  0;

const int CONFIG_BREDR_INQUIRY   =  0;
const int CONFIG_INQUIRY_PAGE_OFFSET_ADJUST =  0;

#if TCFG_RCSP_DUAL_CONN_ENABLE
	const int CONFIG_LMP_NAME_REQ_ENABLE  =  1;
#elif (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)
	const int CONFIG_LMP_NAME_REQ_ENABLE  =  1;
#else
	const int CONFIG_LMP_NAME_REQ_ENABLE  =  0;
#endif
const int CONFIG_LMP_PASSKEY_ENABLE  =  0;
const int CONFIG_LMP_OOB_ENABLE  =  0;
const int CONFIG_LMP_MASTER_ESCO_ENABLE  =  0;

#ifdef CONFIG_SUPPORT_AES_CCM_FOR_EDR
    #if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
        const int CONFIG_AES_CCM_FOR_EDR_ENABLE     = 0;
    #else
        const int CONFIG_AES_CCM_FOR_EDR_ENABLE     = 0;
    #endif
#else
    const int CONFIG_AES_CCM_FOR_EDR_ENABLE     = 0;
#endif

    const int CONFIG_MPR_CLOSE_WHEN_ESCO = 0;
#ifdef CONFIG_BT_CTRLER_USE_SDK
		const int CONFIG_BT_CTRLER_USE_SDK_ENABLE = 1;//br56不用maskrom的lmp，外面重写流程过滤掉
#else
		const int CONFIG_BT_CTRLER_USE_SDK_ENABLE = 0;
#endif

#ifdef CONFIG_SUPPORT_WIFI_DETECT
	#if TCFG_USER_TWS_ENABLE
		const int CONFIG_WIFI_DETECT_ENABLE = 1;
		const int CONFIG_TWS_AFH_ENABLE     = 1;
	#else
		const int CONFIG_WIFI_DETECT_ENABLE = 0;
		const int CONFIG_TWS_AFH_ENABLE     = 0;
	#endif

#else

#if defined CONFIG_CPU_BR27 || defined CONFIG_CPU_BR28 || defined CONFIG_CPU_BR36 || defined CONFIG_CPU_BR42

        const int CONFIG_WIFI_DETECT_ENABLE = 0;
        const int CONFIG_TWS_AFH_ENABLE     = 0;
#else
        const int CONFIG_WIFI_DETECT_ENABLE = 3;

#if TCFG_USER_TWS_ENABLE
        const int CONFIG_TWS_AFH_ENABLE     = 1;
#else
        const int CONFIG_TWS_AFH_ENABLE     = 0;
#endif

#endif
#endif//end CONFIG_SUPPORT_WIFI_DETECT

const int ESCO_FORWARD_ENABLE = 0;


const int config_bt_function  = 0;

///bredr 强制 做 maseter
const int config_btctler_bredr_master = 0;
const int config_btctler_dual_a2dp  = 0;

///afh maseter 使用app设置的map 通过USER_CTRL_AFH_CHANNEL 设置
const int config_bredr_afh_user = 0;
//bt PLL 温度跟随trim
const int config_bt_temperature_pll_trim = 0;
/*security check*/
const int config_bt_security_vulnerability = 0;

const int config_delete_link_key          = 1;           //配置是否连接失败返回PIN or Link Key Missing时删除linkKey

/*-----------------------------------------------------------*/

/**
 * @brief Bluetooth LE setting
 */

#if (TCFG_USER_BLE_ENABLE)
	#define DEFAULT_LE_FEATURES (LE_ENCRYPTION | LE_DATA_PACKET_LENGTH_EXTENSION | LL_FEAT_LE_EXT_ADV | LL_FEAT_CHANNEL_CLASSIFICATION | LE_FEATURES_POWER_CONTROL)

	#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
	    #define LE_AUDIO_CIS_LE_FEATURES (LE_ENCRYPTION | LE_FEATURES_CIS | LE_2M_PHY|CHANNEL_SELECTION_ALGORITHM_2|LL_FEAT_LE_EXT_ADV | LL_FEAT_ISO_HOST_SUPPORT)
    #else
		#define LE_AUDIO_CIS_LE_FEATURES  0
	#endif

	#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN)
		#if TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED
			#define RCSP_MODE_LE_FEATURES (LE_ENCRYPTION)
		#else
			#define RCSP_MODE_LE_FEATURES (LE_ENCRYPTION | LE_DATA_PACKET_LENGTH_EXTENSION | LE_2M_PHY | LL_FEAT_LE_EXT_ADV)
		#endif
    #else
       #define RCSP_MODE_LE_FEATURES 0
	#endif

	#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))
       #define LE_AUDIO_BIS_RX_LE_FEATURES (LE_FEATURES_BIS | LE_CORE_V50_FEATURES | LL_FEAT_ISO_HOST_SUPPORT)
       #define LE_AUDIO_BIS_RX_LE_ROLE (LE_MASTER  | LE_SCAN)
    #else
       #define LE_AUDIO_BIS_RX_LE_FEATURES 0
       #define LE_AUDIO_BIS_RX_LE_ROLE     0
	#endif

    #if (THIRD_PARTY_PROTOCOLS_SEL & MULTI_CLIENT_EN)
        #define MULTI_CLIENT_LE_ROLE (LE_MASTER | LE_SCAN | LE_INIT)
        #define MULTI_CLIENT_LE_FEATURES (LE_ENCRYPTION | LE_DATA_PACKET_LENGTH_EXTENSION | LE_2M_PHY)
    #else
        #define MULTI_CLIENT_LE_ROLE 0
        #define MULTI_CLIENT_LE_FEATURES 0
    #endif

#if TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED
	const int config_btctler_le_roles    = (LE_SLAVE | LE_ADV);
	const uint64_t config_btctler_le_features = LE_ENCRYPTION;
#else
	const int config_btctler_le_roles    = (LE_SLAVE  | LE_ADV|LE_AUDIO_BIS_RX_LE_ROLE | MULTI_CLIENT_LE_ROLE);
	const uint64_t config_btctler_le_features = LE_AUDIO_CIS_LE_FEATURES|DEFAULT_LE_FEATURES|RCSP_MODE_LE_FEATURES|LE_AUDIO_BIS_RX_LE_FEATURES|MULTI_CLIENT_LE_FEATURES;
#endif

#else /* TCFG_USER_BLE_ENABLE */
	const int config_btctler_le_roles    = 0;
	const uint64_t config_btctler_le_features = 0;
#endif//end TCFG_USER_BLE_ENABLE


// Slave multi-link
#if TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED
const int config_btctler_le_slave_multilink = 0;
#else
const int config_btctler_le_slave_multilink = 1;
#endif

// Master multi-link
const int config_btctler_le_master_multilink = 0;
// LE RAM Control

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))

#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
	const int config_btctler_le_hw_nums = 8;
#else
	const int config_btctler_le_hw_nums = 6;
#endif

#elif ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN)))||((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))
	const int config_btctler_le_hw_nums = 8;
#else
	const int config_btctler_le_hw_nums = 2;
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG) & (LE_AUDIO_JL_UNICAST_SINK_EN))
	const int CONFIG_JL_UNICAST_ENABLE     = 1;
#else
	const int CONFIG_JL_UNICAST_ENABLE     = 0;
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG) & (LE_AUDIO_JL_AURACAST_SINK_EN))
	const int CONFIG_JL_AURACAST_ENABLE    = 1;
#else
	const int CONFIG_JL_AURACAST_ENABLE    = 0;
#endif

const int config_btctler_le_slave_conn_update_winden = 2500;//range:100 to 2500
#if (defined CONFIG_CPU_BR28 || defined CONFIG_CPU_BR50 || defined CONFIG_CPU_BR52 || defined CONFIG_CPU_BR56)
//br28 br50 br52 br56 默认开启频道监测
const int config_bb_optimized_ctrl = LE_BB_OPT_FEAT_ISO_DIRECT_PUSH | BIT(11) | BIT(12) | BIT(13);
#else
const int config_bb_optimized_ctrl = LE_BB_OPT_FEAT_ISO_DIRECT_PUSH;
#endif


#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
    #define TWS_LE_AUDIO_LE_ROLE_SW_EN (0)
#elif (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_AURACAST_SINK_EN)
    #define TWS_LE_AUDIO_LE_ROLE_SW_EN (0)
#else
    #define TWS_LE_AUDIO_LE_ROLE_SW_EN (0)
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN) && !TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED
    #define TWS_RCSP_LE_ROLE_SW_EN (1)
#else
    #define TWS_RCSP_LE_ROLE_SW_EN (0)
#endif

#ifdef TCFG_BLE_HIGH_PRIORITY_ENABLE
    const bool config_le_high_priority = TCFG_BLE_HIGH_PRIORITY_ENABLE;  //开启后ble优先级更高，esco下想保证ble一直建立连接和主从切换正常，必须置为1
#else
    const bool config_le_high_priority = 0;
#endif

const int config_btctler_le_afh_en = 0;
const u32 config_vendor_le_bb = 0;
const bool config_tws_le_role_sw =(TWS_LE_AUDIO_LE_ROLE_SW_EN|TWS_RCSP_LE_ROLE_SW_EN);
const int config_btctler_le_rx_nums = 20;
const int config_btctler_le_acl_packet_length = 255;
const int config_btctler_le_acl_total_nums = 15;

/*-----------------------------------------------------------*/
/**
 * @brief Bluetooth Analog setting
 */
/*-----------------------------------------------------------*/
const int config_btctler_single_carrier_en = 0;   // 单载波，如果是单模ble建议设置为1，否则会有部分芯片测试盒连接不上的情况。by zhibin

const int sniff_support_reset_anchor_point = 0;   //sniff状态下是否支持reset到最近一次通信点，用于HID
const int sniff_long_interval = (500 / 0.625);    //sniff状态下进入long interval的通信间隔(ms)
const int config_rf_oob = 0;

const int ll_vendor_ctrl_cmd_support = 1; //1:for testbox or private transmission; 0:for ll bqb mode

//#if TCFG_BT_HFP_ONLY_DISPLAY_BAT_ENABLE
//#if TCFG_BT_SNIFF_ENABLE
//const int CONFIG_BTCTLER_FUN = EDR_LMP_SUPPORT_SNIFF;
//#else
//const int CONFIG_BTCTLER_FUN = 0;
//#endif
//#else
//#if TCFG_BT_SNIFF_ENABLE && TCFG_BT_SUPPORT_HFP
//const int CONFIG_BTCTLER_FUN = EDR_LMP_SUPPORT_ESCO | EDR_LMP_SUPPORT_SNIFF;
//#elif TCFG_BT_SUPPORT_HFP
//const int CONFIG_BTCTLER_FUN = EDR_LMP_SUPPORT_ESCO;
//#elif TCFG_BT_SNIFF_ENABLE
//const int CONFIG_BTCTLER_FUN = EDR_LMP_SUPPORT_SNIFF;
//#else
//const int CONFIG_BTCTLER_FUN = 0;
//#endif
//#endif


