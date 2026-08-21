#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "sdk_config.h"
#include "board_config.h"
#include "resource_monitor_config.h"
#include "audio_cvp_def.h"
#include "rcsp_define.h"

/*
 * 系统打印总开关
 */


#define CONFIG_DEBUG_ENABLE TCFG_DEBUG_UART_ENABLE

#if CONFIG_DEBUG_ENABLE
#define LIB_DEBUG    1
#define CONFIG_DEBUG_LIB(x)         (x & LIB_DEBUG)
#else
#define LIB_DEBUG    0
#define CONFIG_DEBUG_LIB(x)         (x & LIB_DEBUG)
#define CONFIG_DEBUG_LITE_ENABLE    0//轻量级打印开关, 默认关闭
#endif

#if CONFIG_DEBUG_LITE_ENABLE   //轻量级打印配置
#define TCFG_DEBUG_UART_TX_PIN IO_PORT_DP // 输出IO
#define TCFG_DEBUG_UART_BAUDRATE 2000000 // 波特率
#define TCFG_EXCEPTION_LOG_ENABLE 1 // 打印异常信息
#define TCFG_EXCEPTION_RESET_ENABLE 1 // 异常自动复位
#endif

#define CONFIG_KEY_SCENE_ENABLE  				  			1      //情景配置中的按键功能配置是否使能




//*********************************************************************************//
//                              BREDR && BLE配置    	                           //
//*********************************************************************************//
#define TCFG_USER_BT_CLASSIC_ENABLE        1
#ifndef TCFG_APP_BT_EN
#define TCFG_APP_BT_EN										1
#endif
#ifndef TCFG_BT_BACKGROUND_ENABLE
#define TCFG_BT_BACKGROUND_ENABLE   						0
#endif
#if !TCFG_APP_BT_EN
#undef TCFG_BT_BACKGROUND_ENABLE
#define TCFG_BT_BACKGROUND_ENABLE   						0
#endif

#define TCFG_BT_SUPPORT_BIP         0X0
#if TCFG_BT_SUPPORT_BIP
#undef TCFG_BT_MUSIC_INFO_ENABLE
#define TCFG_BT_MUSIC_INFO_ENABLE 1 // 歌曲信息显示
#endif

#if TCFG_BT_DUAL_CONN_ENABLE
#define TCFG_BT_DUAL_1T3_CONN_ENABLE  0//1t3 enable

#if TCFG_BT_DUAL_1T3_CONN_ENABLE
#define TCFG_BT_SUPPORT_CONN_NUM      3
#else
#define TCFG_BT_SUPPORT_CONN_NUM      2
#endif
#else
#define TCFG_BT_DUAL_1T3_CONN_ENABLE  0
#define TCFG_BT_SUPPORT_CONN_NUM      2
#endif

//*********************************************************************************//
//                                  AI配置                                         //
//*********************************************************************************//
#define    RCSP_MODE_EN             (1 << 0)
#define    TRANS_DATA_EN            (1 << 1)
#define    LL_SYNC_EN               (1 << 2)
#define    TUYA_DEMO_EN             (1 << 3)
#define    ANCS_CLIENT_EN           (1 << 4)
#define    GFPS_EN                  (1 << 5)
#define    REALME_EN                (1 << 6)
#define    TME_EN                   (1 << 7)
#define    DMA_EN                   (1 << 8)
#define    GMA_EN                   (1 << 9)
#define    MMA_EN                   (1 << 10)
#define    FMNA_EN                  (1 << 11)
#define    SWIFT_PAIR_EN            (1 << 12)
#define    HONOR_EN                 (1 << 13)
#define    ONLINE_DEBUG_EN          (1 << 14)
#define    CUSTOM_DEMO_EN           (1 << 15)   // 第三方协议的demo，用于示例客户开发自定义协议
#define    XIMALAYA_EN              (1 << 16)
#define    RDX_EN                   (1 << 17)
#define    MULTI_CLIENT_EN          (1 << 18)
#define    ANCS_AMS_MODE_EN         (1 << 19)
#define    JL_SBOX_EN               (1 << 20)
#define    HID_ISO_EN               (1 << 21)

#if TCFG_THIRD_PARTY_PROTOCOLS_ENABLE
#define THIRD_PARTY_PROTOCOLS_SEL  TCFG_THIRD_PARTY_PROTOCOLS_SEL
#else
#define THIRD_PARTY_PROTOCOLS_SEL  0
#define TCFG_THIRD_PARTY_PROTOCOLS_SEL 0 // 第三方协议选择
#endif

#if THIRD_PARTY_PROTOCOLS_SEL && (TCFG_USER_BLE_ENABLE == 0)
#error "开启 第三方协议功能 需要使能 TCFG_USER_BLE_ENABLE"
#endif

#ifndef TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED
// 三方协议简化版本，目前仅适用于RCSP
#define TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED				0
#endif

#if TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED && TCFG_RCSP_DUAL_CONN_ENABLE
#error "开启RCSP简化宏不支持两路rcsp"
#endif

//代码放RAM压缩宏，在RAM资源足够的情况下将代码放RAM进行压缩
#define TCFG_CODE_TO_RAM_COMPRESS_ENABLE					0

//*********************************************************************************//
//                                  edr dongle 配置                                //
//*********************************************************************************//
//通过edr方式连接dongle，需要开启以下宏：
#if 0
#define  TCFG_BT_DONGLE_ENABLE                             1
#define  TCFG_JL_DONGLE_PLAYBACK_LATENCY                   30   //延时设置
#endif

//*********************************************************************************//
//                                  le_audio 配置                                       //
//*********************************************************************************//

#define    LE_AUDIO_UNICAST_SOURCE_EN           (1 << 0)
#define    LE_AUDIO_UNICAST_SINK_EN             (1 << 1)
#define    LE_AUDIO_AURACAST_SOURCE_EN          (1 << 2)
#define    LE_AUDIO_AURACAST_SINK_EN            (1 << 3)
#define    LE_AUDIO_JL_UNICAST_SOURCE_EN        (1 << 4)
#define    LE_AUDIO_JL_UNICAST_SINK_EN          (1 << 5)
#define    LE_AUDIO_JL_AURACAST_SOURCE_EN       (1 << 6)
#define    LE_AUDIO_JL_AURACAST_SINK_EN         (1 << 7)

#ifndef TCFG_LE_AUDIO_APP_CONFIG
#define TCFG_LE_AUDIO_APP_CONFIG        (0)
#endif

#if TCFG_LE_AUDIO_APP_CONFIG && (TCFG_USER_BLE_ENABLE == 0)
#error "开启 le audio 功能需要使能 TCFG_USER_BLE_ENABLE"
#endif

#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN))

#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN)

// rcsp与le audio共用 BLE ACL
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)
#define TCFG_LE_AUDIO_RCSP_USE_SAME_ACL      (0)
#else
#define TCFG_LE_AUDIO_RCSP_USE_SAME_ACL      (1)
#endif

// rcsp 与 le audio 使用不同地址
#undef  TCFG_BT_BLE_BREDR_SAME_ADDR
#define  TCFG_BT_BLE_BREDR_SAME_ADDR 0x0
#endif

#if (TCFG_THIRD_PARTY_PROTOCOLS_SEL & GFPS_EN) && (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN)
#error "谷歌快连和unicast不支持同时配置，这两个功能开机不能同时初始化；如果想要同时配置，可以自行添加流程让上位机实现两个功能的二选一后开机重启初始化"
#endif

#ifndef TCFG_JL_UNICAST_BOUND_PAIR_EN
#define TCFG_JL_UNICAST_BOUND_PAIR_EN 0				// 可通过JL小板实现耳机和Dongle的绑定配对
#endif

#define JL_UNICAST_ACL_MAX_PDU_CTOP				36 // 与LEA dongle传输的私有命令最大数据长度

#define JL_UNICAST_DUAL_UAC_ENABLE              0 // JL_UNICAST双声卡模式使能

// #undef TCFG_LOWPOWER_LOWPOWER_SEL
// #define  TCFG_LOWPOWER_LOWPOWER_SEL 0x0//低功耗连接还有问题
#endif

#define LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_PREEMPTEDK           BIT(1) //支持dongle和手机保持连接,声音互抢
#define LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_MIX                  BIT(2) //支持dongle和手机保持连接,声音叠加

#ifndef LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH
#define LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH 0 // JL_UNICAST、EDR一拖二使能
#endif
#if (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN) && LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_SWITCH
#if TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE
#error "SDK中JL_UNICAST一拖二与模式切换功能默认不共存，如需使用可参考现有逻辑与接口自行开发新UI"
#endif
#ifndef LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG
#define LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG  LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_PLAY_PREEMPTEDK
#endif
#else
#undef LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG
#define LE_AUDIO_JL_DONGLE_UNICAST_WITH_PHONE_CONN_CONFIG  0
#endif


#define LE_AUDIO_STREAM_ENABLE 	(TCFG_LE_AUDIO_APP_CONFIG)

// 公共配置.json
#define LE_AUDIO_CODEC_TYPE AUDIO_CODING_LC3 //编解码格式
#define LE_AUDIO_CODEC_CHANNEL 0x1 //编解码声道数
#define LE_AUDIO_CODEC_FRAME_LEN 0x64 //帧持续时间
#define LE_AUDIO_CODEC_SAMPLERATE 24000 //采样率
#define LE_AUDIO_CODEC_BIT_RATR 48000 //采样率
#define LEA_DEC_OUTPUT_CHANNEL 0x25 //接收端解码输出


/* -----------------独立模式 PC/LINEIN/SPDIF 模块默认控制------- */
//没有独立模式的 需要在sdk_config.h定义对应的宏，有独立模式的，根据模式宏的值默认定义对应的宏
#ifndef TCFG_APP_PC_EN
#define TCFG_APP_PC_EN										0
#endif

#ifndef TCFG_PC_ENABLE
#define TCFG_PC_ENABLE       TCFG_APP_PC_EN
#endif

#ifndef TCFG_APP_LINEIN_EN
#define TCFG_APP_LINEIN_EN									0
#endif

#ifndef TCFG_AUDIO_LINEIN_ENABLE
#define TCFG_AUDIO_LINEIN_ENABLE   TCFG_APP_LINEIN_EN
#endif

#ifndef TCFG_MUSIC_PLAYER_ENABLE
#define TCFG_MUSIC_PLAYER_ENABLE   TCFG_APP_MUSIC_EN
#endif

#ifndef TCFG_MIX_RECORD_ENABLE
#define TCFG_MIX_RECORD_ENABLE     0
#endif

#if TCFG_APP_MUSIC_EN || TCFG_MIX_RECORD_ENABLE
#define TCFG_DEV_MANAGER_ENABLE					  			1
#elif TCFG_APP_PC_EN || TCFG_APP_LINEIN_EN
#define TCFG_DEV_MANAGER_ENABLE					  			0
#else
#define TCFG_DEV_MANAGER_ENABLE					  			0
#endif

#if TCFG_APP_MUSIC_EN
#define TCFG_FILE_MANAGER_ENABLE							1
#else
#define TCFG_FILE_MANAGER_ENABLE							0
#endif

#if TCFG_LOCAL_TWS_ENABLE
// 耳机local_tws使用编码数据进行音频同步
#define LOCAL_TWS_SOURCE_PLAYER_USE_ENC_DATA				1
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & FMNA_EN)
//存放token信息
#define CONFIG_FINDMY_INFO_ENABLE      		    1		//配置是否支持FINDMY存储
#else
#define CONFIG_FINDMY_INFO_ENABLE      		    0
#endif


//*********************************************************************************//
//                          	文件系统相关配置                                   //
//*********************************************************************************//
#if (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)
#define CONFIG_FATFS_ENABLE                                 1
#elif TCFG_SD0_ENABLE || TCFG_SD1_ENABLE || TCFG_UDISK_ENABLE || TCFG_NOR_FAT || TCFG_NANDFLASH_DEV_ENABLE
#define CONFIG_FATFS_ENABLE                                 1
#else
#define CONFIG_FATFS_ENABLE                                 0
#endif
#define SDFILE_DEV								  			"sdfile"

#define LIB_SUPPORT_MULTI_SECTOER_READ  		  			0 //对应配合库修改是否一次性多sector读
#if LIB_SUPPORT_MULTI_SECTOER_READ
#define MAX_READ_LEN 										8192
#endif

/* ------------------rule check------------------ */
#ifndef TCFG_USER_TWS_ENABLE
#define TCFG_USER_TWS_ENABLE 0
#endif

#ifndef TCFG_APP_MUSIC_EN
#define TCFG_APP_MUSIC_EN  0
#endif


#ifndef TCFG_NOR_FS
#define TCFG_NOR_FS         0
#endif

#ifndef MBEDTLS_SELF_TEST
#define MBEDTLS_SELF_TEST   0
#endif

#ifndef TCFG_JSA1221_ENABLE
#define TCFG_JSA1221_ENABLE 0
#endif

#ifndef MBEDTLS_AES_SELF_TEST
#define MBEDTLS_AES_SELF_TEST   0
#endif

#ifndef MBEDTLS_AES_SELF_TEST
#define MBEDTLS_AES_SELF_TEST   0
#endif

#ifndef TCFG_NOR_REC
#define TCFG_NOR_REC         0
#endif

#ifndef FLASH_INSIDE_REC_ENABLE
#define FLASH_INSIDE_REC_ENABLE     0
#endif

#ifndef TCFG_REC_FOLDER_NAME
#define TCFG_REC_FOLDER_NAME "JL_REC"
#endif
#ifndef TCFG_REC_FILE_NAME
#define TCFG_REC_FILE_NAME "aud_****"
#define TCFG_REC_FILE_NAME_PREFIX	"aud_"			//录音文件前缀名
#endif

// ====================手机APP第三方协议连接方式===================
#if !(THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN)
// 非RCSP协议优先BLE
#define	   RCSP_MODE			     RCSP_MODE_OFF
#define ATT_OVER_EDR_DEMO_EN          0 //enalbe gatt_over_edr_demo

#else

// RCSP协议的连接方式
#ifdef RCSP_CHANNEL_SEL

// 根据可视化工具界面进行配置
#if (RCSP_CHANNEL_SEL == RCSP_USE_GATT_OVER_EDR)
#define ATT_OVER_EDR_DEMO_EN          1 //enalbe gatt_over_edr_demo
#else
#define ATT_OVER_EDR_DEMO_EN          0 //enalbe gatt_over_edr_demo
#endif

#else

// RCSP协议优先ATT OVER EDR
#define ATT_OVER_EDR_DEMO_EN          1 //enalbe gatt_over_edr_demo
#define RCSP_CHANNEL_SEL RCSP_USE_GATT_OVER_EDR // RCSP连接方式

#endif

#endif
// ====================手机APP第三方协议连接方式===================

#ifndef TCFG_USER_EDR_ENABLE
#define TCFG_USER_EDR_ENABLE        0
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & JL_SBOX_EN)
#undef TCFG_USER_EDR_ENABLE
#define TCFG_USER_EDR_ENABLE        1
#endif

//单双备份的配置在board_xxx_global_cfg里配置，需要注意只有RCSP才支持单双备份，其余升级都是只支持双备份升级
//支持TWS同步升级，OTA_TWS_SAME_TIME_NEW宏需要配置为1，旧的流程已不再支持
#if (THIRD_PARTY_PROTOCOLS_SEL & RCSP_MODE_EN)

#define	   RCSP_MODE			     RCSP_MODE_EARPHONE
#include "rcsp_cfg.h"
// 详细功能参考rcsp_cfg.h
#if RCSP_ADV_TRANSLATOR
#define    TCFG_ENC_SPEEX_ENABLE     0
#define    TCFG_AI_TRANSLATOR_ENABLE 1
#endif

#elif ((THIRD_PARTY_PROTOCOLS_SEL & (TME_EN | DMA_EN | GMA_EN | XIMALAYA_EN)))
#define    BT_MIC_EN                 1
#define    TCFG_ENC_SPEEX_ENABLE     0
#define    OTA_TWS_SAME_TIME_ENABLE  0     //是否支持TWS同步升级
#elif (THIRD_PARTY_PROTOCOLS_SEL & (LL_SYNC_EN | TUYA_DEMO_EN))
#define    OTA_TWS_SAME_TIME_ENABLE  (TCFG_USER_TWS_ENABLE)
#define    OTA_TWS_SAME_TIME_NEW     (TCFG_USER_TWS_ENABLE)     //使用新的tws ota流程
#define    TCFG_ENC_SPEEX_ENABLE     0
#else
#define    OTA_TWS_SAME_TIME_ENABLE  0
#define    OTA_TWS_SAME_TIME_NEW     0     //使用新的tws ota流程
#define    TCFG_ENC_SPEEX_ENABLE     0
#endif

#define CONFIG_MEDIA_LIB_USE_MALLOC    1
#ifndef TCFG_JLSTREAM_EFFICIENT_MODE
#define TCFG_JLSTREAM_EFFICIENT_MODE	0 		// 音频流高效模式
#endif

///USB 配置重定义
// #undef USB_DEVICE_CLASS_CONFIG
// #define USB_DEVICE_CLASS_CONFIG 									(AUDIO_CLASS)
/////要确保 上面 undef 后在include usb

#define USB_PC_NO_APP_MODE                        0

#ifndef TCFG_CHARGESTORE_WAKEUP_INDEX
#define TCFG_CHARGESTORE_WAKEUP_INDEX       2
#endif

#if TCFG_CHARGE_ENABLE
#define TCFG_USB_PORT_CHARGE                1
#endif

#include "btcontroller_mode.h"

#include "user_cfg_id.h"

//*********************************************************************************//
//                                 测试模式配置                                    //
//*********************************************************************************//
#if (CONFIG_BT_MODE == BT_NORMAL) && (!TCFG_NORMAL_SET_DUT_MODE)
//enable dut mode,need disable sleep(TCFG_LOWPOWER_LOWPOWER_SEL = 0)
// #define TCFG_NORMAL_SET_DUT_MODE                  0
#if TCFG_NORMAL_SET_DUT_MODE
#undef  TCFG_LOWPOWER_LOWPOWER_SEL
#define TCFG_LOWPOWER_LOWPOWER_SEL                0

#undef  TCFG_AUTO_SHUT_DOWN_TIME
#define TCFG_AUTO_SHUT_DOWN_TIME		          0

#undef  TCFG_USER_TWS_ENABLE
#define TCFG_USER_TWS_ENABLE                      0     //tws功能使能

#endif

#else
#undef TCFG_BT_DUAL_CONN_ENABLE

#undef  TCFG_USER_TWS_ENABLE
#define TCFG_USER_TWS_ENABLE                      0     //tws功能使能

#undef TCFG_LOCAL_TWS_ENABLE
#define TCFG_LOCAL_TWS_ENABLE                          0

//#undef  TCFG_USER_BLE_ENABLE
//#define TCFG_USER_BLE_ENABLE                      1     //BLE功能使能

#undef  TCFG_AUTO_SHUT_DOWN_TIME
#define TCFG_AUTO_SHUT_DOWN_TIME		          0

#undef  TCFG_SYS_LVD_EN
#define TCFG_SYS_LVD_EN						      0

#undef  TCFG_LOWPOWER_LOWPOWER_SEL
#define TCFG_LOWPOWER_LOWPOWER_SEL                0

//#undef TCFG_AUDIO_DAC_LDO_VOLT
//#define TCFG_AUDIO_DAC_LDO_VOLT			   DUT_AUDIO_DAC_LDO_VOLT

//#undef TCFG_LOWPOWER_POWER_SEL
//#define TCFG_LOWPOWER_POWER_SEL				PWR_LDO15

//#undef  TCFG_PWMLED_ENABLE
//#define TCFG_PWMLED_ENABLE					DISABLE_THIS_MOUDLE

//#undef  TCFG_ADKEY_ENABLE
//#define TCFG_ADKEY_ENABLE                   DISABLE_THIS_MOUDLE

//#undef  TCFG_IOKEY_ENABLE
//#define TCFG_IOKEY_ENABLE					DISABLE_THIS_MOUDLE

//#undef TCFG_TEST_BOX_ENABLE
//#define TCFG_TEST_BOX_ENABLE			    0

#undef TCFG_AUTO_POWERON_ENABLE
#define TCFG_AUTO_POWERON_ENABLE    1

/* #undef TCFG_UART0_ENABLE
#define TCFG_UART0_ENABLE					DISABLE_THIS_MOUDLE */

#endif //(CONFIG_BT_MODE != BT_NORMAL)


//*********************************************************************************//
//                                 对耳配置方式配置                                    //
//*********************************************************************************//


#if TCFG_USER_TWS_ENABLE

#define CONFIG_TWS_COMMON_ADDR_AUTO             1       /* 自动生成TWS配对后的MAC地址 */
#define CONFIG_TWS_COMMON_ADDR_USED_LEFT        2       /* 使用左耳的MAC地址作为TWS配对后的地址
                                                           可配合烧写器MAC地址自增功能一起使用
                                                           多台交叉配对会出现MAC地址相同情况 */
#define CONFIG_TWS_COMMON_ADDR_USED_RIGHT       3
#define CONFIG_TWS_COMMON_ADDR_USED_MASTER      4      /* 不使用COMMON_ADDR, 单台时使用自己的MAC地址,
                                                          TWS连上时使用主耳MAC地址,默认会关掉自动主从切换,
                                                          只支持在硬关机或者按键同步关机的方案使用,
                                                          否则在单台耳机开关的情况下会出现MAC地址相同情况，
                                                          进而影响和手机的正常连接
                                                        */

#define CONFIG_TWS_COMMON_ADDR_SELECT           CONFIG_COMMON_ADDR_MODE

#if CONFIG_TWS_COMMON_ADDR_SELECT == CONFIG_TWS_COMMON_ADDR_USED_MASTER
#undef TCFG_TWS_AUTO_ROLE_SWITCH_ENABLE
#define TCFG_TWS_AUTO_ROLE_SWITCH_ENABLE 0
#endif


#define CONFIG_TWS_CONNECT_SIBLING_TIMEOUT    4    /* 开机或超时断开后对耳互连超时时间，单位s */
// #define CONFIG_TWS_REMOVE_PAIR_ENABLE              [> 不连手机的情况下双击按键删除配对信息 <]
//#define CONFIG_TWS_POWEROFF_SAME_TIME         1    /*按键关机时两个耳机同时关机*/

#define ONE_KEY_CTL_DIFF_FUNC                 1    /*通过左右耳实现一个按键控制两个功能*/
#define CONFIG_TWS_SCO_ONLY_MASTER			  0	   /*通话的时候只有主机出声音*/

/* 配对方式选择 */
#define CONFIG_TWS_PAIR_BY_CLICK            0      /* 按键发起配对 */
#define CONFIG_TWS_PAIR_BY_AUTO             1      /* 开机自动配对 */
#define CONFIG_TWS_PAIR_BY_BOX              2      /* 测试盒/充电仓配对 */
#define CONFIG_TWS_PAIR_BY_CHIP_CONN        3
#define CONFIG_TWS_PAIR_MODE                TCFG_BT_TWS_PAIR_MODE

#define CONFIG_TWS_DIFF_NAME_NOT_MATCH      1      /*tws 不同名字不进行配对*/

/* 声道确定方式选择 */
#define CONFIG_TWS_MASTER_AS_LEFT               0 //主机作为左耳
#define CONFIG_TWS_MASTER_AS_RIGHT              1 //主机作为右耳
#define CONFIG_TWS_AS_LEFT                      2 //固定左耳
#define CONFIG_TWS_AS_RIGHT                     3 //固定右耳
#define CONFIG_TWS_START_PAIR_AS_LEFT           4 //双击发起配对的耳机做左耳
#define CONFIG_TWS_START_PAIR_AS_RIGHT          5 //双击发起配对的耳机做右耳
#define CONFIG_TWS_EXTERN_UP_AS_LEFT            6 //外部有上拉电阻作为左耳
#define CONFIG_TWS_EXTERN_DOWN_AS_LEFT          7 //外部有下拉电阻作为左耳
#define CONFIG_TWS_EXTERN_UP_AS_RIGHT           8 //外部有上拉电阻作为右耳
#define CONFIG_TWS_EXTERN_DOWN_AS_RIGHT         9 //外部有下拉电阻作为右耳
#define CONFIG_TWS_CHANNEL_SELECT_BY_BOX        10 //充电仓/测试盒决定左右耳
#define CONFIG_TWS_CHANNEL_SELECT             TCFG_BT_TWS_CHANNEL_SELECT //配对方式选择

#ifndef CONFIG_TWS_CHANNEL_CHECK_IO
#define CONFIG_TWS_CHANNEL_CHECK_IO           IO_PORTA_07					//上下拉电阻检测引脚
#endif


#if CONFIG_TWS_PAIR_MODE != CONFIG_TWS_PAIR_BY_CLICK
#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_LEFT) ||\
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_START_PAIR_AS_RIGHT)
#undef CONFIG_TWS_CHANNEL_SELECT
#define CONFIG_TWS_CHANNEL_SELECT             CONFIG_TWS_MASTER_AS_LEFT
#endif
#endif


#if TCFG_CHARGESTORE_ENABLE
#undef CONFIG_TWS_CHANNEL_SELECT
#define CONFIG_TWS_CHANNEL_SELECT             CONFIG_TWS_CHANNEL_SELECT_BY_BOX //充电仓区分左右
#endif


#if TCFG_TEST_BOX_ENABLE && (!TCFG_CHARGESTORE_ENABLE)
#define CONFIG_TWS_SECECT_CHARGESTORE_PRIO    1 //测试盒配置左右耳优先
#else
#define CONFIG_TWS_SECECT_CHARGESTORE_PRIO    0
#endif //TCFG_TEST_BOX_ENABLE

#define CONFIG_A2DP_GAME_MODE_ENABLE            0  //游戏模式
#define CONFIG_A2DP_GAME_MODE_DELAY_TIME        35  //游戏模式延时ms


//*********************************************************************************//
//      低延时游戏模式脚步声、枪声增强,需使能蓝牙音乐10段eq以及蓝牙音乐drc
//      用户开关宏AUDIO_GAME_EFFECT_CONFIG(开关蓝牙低延时模式的游戏音效)
//      低延时eq效果文件使用eq_game_eff.bin,调试时需保存成该文件,并在批处理-res后添加
//      非低延时eq效果文件使用eq_cfg_hw.bin,也需在批处理-res后添加
//*********************************************************************************//
#if CONFIG_A2DP_GAME_MODE_ENABLE
#define AUDIO_GAME_EFFECT_CONFIG  1  //低延时游戏模式脚步声、枪声增强 1:使能、0：关闭
#else
#define AUDIO_GAME_EFFECT_CONFIG  0  //低延时游戏模式脚步声、枪声增强 1:使能、0：关闭
#endif


//*********************************************************************************//
//                                 对耳电量显示方式                                //
//*********************************************************************************//

#if TCFG_BT_DISPLAY_BAT_ENABLE
#define CONFIG_DISPLAY_TWS_BAT_LOWER          1 //对耳手机端电量显示，显示低电量耳机的电量
#define CONFIG_DISPLAY_TWS_BAT_HIGHER         2 //对耳手机端电量显示，显示高电量耳机的电量
#define CONFIG_DISPLAY_TWS_BAT_LEFT           3 //对耳手机端电量显示，显示左耳的电量
#define CONFIG_DISPLAY_TWS_BAT_RIGHT          4 //对耳手机端电量显示，显示右耳的电量

#define CONFIG_DISPLAY_TWS_BAT_TYPE           CONFIG_DISPLAY_TWS_BAT_LOWER
#endif

#define CONFIG_DISPLAY_DETAIL_BAT             0 //BLE广播显示具体的电量
#define CONFIG_NO_DISPLAY_BUTTON_ICON         1 //BLE广播不显示按键界面,智能充电仓置1

#endif //TCFG_USER_TWS_ENABLE



//*********************************************************************************//
//                                 电源切换配置                                    //
//*********************************************************************************//

#define PHONE_CALL_USE_LDO15	CONFIG_PHONE_CALL_USE_LDO15

//*********************************************************************************//
//                                 时钟切换配置                                    //
//*********************************************************************************//
#define BT_NORMAL_HZ	            24000000

//*********************************************************************************//
#define RTC_CLK_RES_SEL	            CLK_SEL_LRC

//*********************************************************************************//
//                                  RTC测试配置                                    //
//*********************************************************************************//
#define RTC_TEST_ITEM_NONE                  0x00000000UL
#define RTC_TEST_ITEM_REG_DUMP              0x00000001UL
#define RTC_TEST_ITEM_BASIC_RW              0x00000002UL
#define RTC_TEST_ITEM_TIMING                0x00000004UL
#define RTC_TEST_ITEM_ALARM                 0x00000008UL
#define RTC_TEST_ITEM_UTILS                 0x00000010UL
#define RTC_TEST_ITEM_SOFTOFF_WAKE          0x00000020UL
#define RTC_TEST_ITEM_SAFE                  (RTC_TEST_ITEM_REG_DUMP | \
                                             RTC_TEST_ITEM_TIMING | \
                                             RTC_TEST_ITEM_UTILS)
#define RTC_TEST_ITEM_SOFTOFF               (RTC_TEST_ITEM_REG_DUMP | \
                                             RTC_TEST_ITEM_SOFTOFF_WAKE)
#define RTC_TEST_ITEM_ALL                   (RTC_TEST_ITEM_SAFE | \
                                             RTC_TEST_ITEM_BASIC_RW | \
                                             RTC_TEST_ITEM_ALARM)

#ifndef RTC_TEST_ENABLE
#define RTC_TEST_ENABLE                     0
#endif

#ifndef RTC_TEST_ITEM_MASK
#define RTC_TEST_ITEM_MASK                  RTC_TEST_ITEM_ALL
#endif

#ifndef RTC_TEST_TIMING_SAMPLE_PERIOD_MS
#define RTC_TEST_TIMING_SAMPLE_PERIOD_MS    (10 * 1000UL)
#endif

#ifndef RTC_TEST_TIMING_SAMPLE_COUNT
#define RTC_TEST_TIMING_SAMPLE_COUNT        6
#endif

#ifndef RTC_TEST_ALARM_DELAY_SEC
#define RTC_TEST_ALARM_DELAY_SEC            20
#endif

#ifndef RTC_TEST_ALARM_START_DELAY_MS
#define RTC_TEST_ALARM_START_DELAY_MS       3000
#endif

#ifndef RTC_TEST_SOFTOFF_ALARM_DELAY_SEC
#define RTC_TEST_SOFTOFF_ALARM_DELAY_SEC    60
#endif

#ifndef RTC_TEST_SOFTOFF_ENTER_DELAY_MS
#define RTC_TEST_SOFTOFF_ENTER_DELAY_MS     2000
#endif

#ifndef RTC_TEST_TIMING_TOLERANCE_SEC
#define RTC_TEST_TIMING_TOLERANCE_SEC       2
#endif

//*********************************************************************************//
//                                  低功耗配置                                     //
//*********************************************************************************//
#if TCFG_IRKEY_ENABLE
#undef  TCFG_LOWPOWER_LOWPOWER_SEL
#define TCFG_LOWPOWER_LOWPOWER_SEL			0                     //开红外不进入低功耗
#endif  /* #if TCFG_IRKEY_ENABLE */

//*********************************************************************************//
//                            LED使用 16SLOT TIMER 同步                            //
//*********************************************************************************//
//LED模块使用slot timer同步使用注意点:
//	1.soundbox不开该功能, 原因: 默认打开osc时钟, 使用原来的osc流程同步即可
//	2.带sd卡earphone不开该功能, 一般为单耳, 不需要同步, 使用原来的流程(lrc)
//	3.一般用在tws应用中, 而且默认关闭osc;
#if TCFG_USER_TWS_ENABLE
#define TCFG_PWMLED_USE_SLOT_TIME			ENABLE_THIS_MOUDLE
#endif


#define	MAX_LIMIT_SYS_CLOCK_128M  128000000
#define MAX_LIMIT_SYS_CLOCK_160M  160000000
#if TCFG_BT_DONGLE_ENABLE
#if (TCFG_MAX_LIMIT_SYS_CLOCK==MAX_LIMIT_SYS_CLOCK_128M) //dongle sbc解码+msbc编码+回音消除时钟需要跑160M
#error "CHECK TCFG_MAX_LIMIT_SYS_CLOCK = 160M dongle sbc解码+msbc编码+回音消除时钟需要跑160M!!!"

#endif
#endif


//*********************************************************************************//
//                                 升级配置                                        //
//*********************************************************************************//
//升级LED显示使能
#define UPDATE_LED_REMIND
//升级提示音使能
#define UPDATE_VOICE_REMIND

#ifndef CONFIG_UPDATE_JUMP_TO_MASK
#define CONFIG_UPDATE_JUMP_TO_MASK 	0
#endif

#if CONFIG_UPDATE_JUMP_TO_MASK
//升级IO保持使能
#define DEV_UPDATE_SUPPORT_JUMP           //目前只有br23\br25支持
#endif


#if TCFG_APP_MUSIC_EN
#if TCFG_SD0_ENABLE
#define CONFIG_SD_UPDATE_ENABLE
#endif
#if TCFG_USB_HOST_ENABLE && TCFG_UDISK_ENABLE
#define CONFIG_USB_UPDATE_ENABLE
#endif
#define TCFG_DEV_UPDATE_IF_NOFILE_ENABLE   0//0：设备上线直接查找升级文件 1：无音乐文件时才查找升级文件
#endif

#define TCFG_UPDATE_UART_DEV 0x1 //普通IO串口升级选择串口号
#define CONFIG_UPDATE_MUTIL_CPU_UART_DEV  0x2 //多mcu串口交互升级选择串口号

#if CONFIG_UPDATE_MUTIL_CPU_UART
#if TCFG_UPDATE_UART_ROLE
#error "they can not enable at the same time,just select one!!!"
#endif
#endif


// #define CONFIG_USER_FILE_UPDATE_V2_EN           1    //开启升级预留区域功能v2模式
// #define CONFIG_USER_FILE_UPDATE_V2_TWS          1    //开启升级预留区域功能v2模式同时支持tws升级

//*********************************************************************************//
//                                 Audio配置                                        //
//*********************************************************************************//

//头部姿态检测
#ifndef TCFG_AUDIO_SOMATOSENSORY_ENABLE
#define TCFG_AUDIO_SOMATOSENSORY_ENABLE     0
#endif

/*
 *Hi-Res Audio:Aux/USB Audio
 *Hi-Res Wireless Audio::LDAC/LHDC
 */
#if (TCFG_BT_SUPPORT_LHDC_V5 || TCFG_BT_SUPPORT_LHDC || TCFG_BT_SUPPORT_LDAC)
#define TCFG_HI_RES_AUDIO_ENEBALE
#endif


#ifndef TCFG_AUDIO_MIC_PWR_PORT
#define TCFG_AUDIO_MIC_PWR_PORT NO_CONFIG_PORT
#endif

#ifndef TCFG_AUDIO_MIC1_PWR_PORT
#define TCFG_AUDIO_MIC1_PWR_PORT NO_CONFIG_PORT
#endif

#ifndef TCFG_AUDIO_MIC2_PWR_PORT
#define TCFG_AUDIO_MIC2_PWR_PORT NO_CONFIG_PORT
#endif

#ifndef TCFG_AUDIO_PLNK_SCLK_PIN
#define TCFG_AUDIO_PLNK_SCLK_PIN NO_CONFIG_PORT
#endif

#ifndef TCFG_AUDIO_PLNK_DAT0_PIN
#define TCFG_AUDIO_PLNK_DAT0_PIN NO_CONFIG_PORT
#endif

#ifndef TCFG_AUDIO_PLNK_DAT1_PIN
#define TCFG_AUDIO_PLNK_DAT1_PIN NO_CONFIG_PORT
#endif

#ifndef TCFG_AUDIO_BIT_WIDTH
#define TCFG_AUDIO_BIT_WIDTH 0
#endif

#define  SYS_VOL_TYPE 	VOL_TYPE_DIGITAL/*目前仅支持软件数字音量模式*/

#define AUDIO_ANC_DATA_EXPORT_VIA_UART						0		//ANC数据导出使能

//环境噪声检测衍生功能
#define AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE        BIT(0) //环境自适应(TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE)
#define AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE      BIT(1) //音量自适应(TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE)

//icsd 环境噪声检测模式：标准
#define TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE             (TCFG_AUDIO_ANC_ENV_NOISE_DET_MODE && TCFG_AUDIO_ANC_ENV_ENABLE)
#define TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE                   (TCFG_AUDIO_ANC_ENV_NOISE_DET_MODE && TCFG_AUDIO_ANC_AVC_ENABLE)

//anc硬件 环境噪声检测模式：轻量
#define TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE        (!TCFG_AUDIO_ANC_ENV_NOISE_DET_MODE && TCFG_AUDIO_ANC_ENV_ENABLE)
#define TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE      (!TCFG_AUDIO_ANC_ENV_NOISE_DET_MODE && TCFG_AUDIO_ANC_AVC_ENABLE)

#define TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE (TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE | TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE)

#if (TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE && (TCFG_AUDIO_AVC_NODE_ENABLE && (TCFG_AVC_ALGO_SELECT == 1)))
#error "AVC节点配置了ICSD算法 与 环境自适应(标准版)互斥"
#endif

/*智能免摘，广域点击，风噪检测，实时自适应*/
#if (TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE || \
	 TCFG_AUDIO_WIDE_AREA_TAP_ENABLE || \
	 TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE || \
	 TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE || \
	 TCFG_AUDIO_ANC_HOWLING_DET_ENABLE || \
     TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE)
#if TCFG_AUDIO_ANC_TRAIN_MODE != ANC_HYBRID_EN
#error "ANC智能免摘，广域点击，风噪检测，仅支持ANC HYBRID方案"
#endif
#define TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN		ENABLE_THIS_MOUDLE		//声音检测(使能智能免摘需要打开)
#else
#define TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN		DISABLE_THIS_MOUDLE		//声音检测
#endif

#if (TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN 			|| \
	 TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE 	|| \
	 TCFG_AUDIO_ANC_HOWLING_DET_ENABLE 			|| \
	 TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE		|| \
	 TCFG_AUDIO_ADAPTIVE_EQ_ENABLE)
#define TCFG_AUDIO_ANC_EXT_TOOL_ENABLE		1
#else
#define TCFG_AUDIO_ANC_EXT_TOOL_ENABLE		0
#endif

#if (TCFG_AUDIO_CVP_SMS_ANS_MODE || TCFG_AUDIO_CVP_SMS_DNS_MODE)
/*1mic-CVP：单mic通话算法*/
#define TCFG_AUDIO_SINGLE_MIC_ENABLE	ENABLE_THIS_MOUDLE
#define TCFG_AUDIO_DUAL_MIC_ENABLE		DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_TRIPLE_MIC_ENABLE    DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_NORMAL
#if (TCFG_AUDIO_CVP_SMS_ANS_MODE)			/*单MIC+ANS通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_ANS_MODE
#elif (TCFG_AUDIO_CVP_SMS_DNS_MODE) 		/*单MIC+DNS通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_DNS_MODE
#endif

#elif (TCFG_AUDIO_CVP_DMS_ANS_MODE || TCFG_AUDIO_CVP_DMS_DNS_MODE || \
	TCFG_AUDIO_CVP_DMS_FLEXIBLE_ANS_MODE ||TCFG_AUDIO_CVP_DMS_FLEXIBLE_DNS_MODE || \
	TCFG_AUDIO_CVP_DMS_HYBRID_DNS_MODE  || TCFG_AUDIO_CVP_DMS_AWN_DNS_MODE )
/*2mic-CVP:双mic通话算法*/
#define TCFG_AUDIO_SINGLE_MIC_ENABLE	DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_DUAL_MIC_ENABLE		ENABLE_THIS_MOUDLE
#define TCFG_AUDIO_TRIPLE_MIC_ENABLE    DISABLE_THIS_MOUDLE
#if (TCFG_AUDIO_CVP_DMS_ANS_MODE) /*双MIC+ANS通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_ANS_MODE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_NORMAL

#elif (TCFG_AUDIO_CVP_DMS_DNS_MODE) /*双MIC+DNS通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_DNS_MODE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_NORMAL

#elif (TCFG_AUDIO_CVP_DMS_FLEXIBLE_ANS_MODE) /*话务双MIC+ANS通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_ANS_MODE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_FLEXIBLE

#elif (TCFG_AUDIO_CVP_DMS_FLEXIBLE_DNS_MODE) /*话务双MIC+DNS通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_DNS_MODE
#define TCFG_AUDIO_DMS_SEL	  	        DMS_FLEXIBLE

#elif (TCFG_AUDIO_CVP_DMS_HYBRID_DNS_MODE) /*dms_hybrid通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_DNS_MODE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_HYBRID

#elif (TCFG_AUDIO_CVP_DMS_AWN_DNS_MODE) /*dms_AWN通话*/
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_DNS_MODE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_AWN
#endif

#elif (TCFG_AUDIO_CVP_3MIC_MODE)
/*3mic-CVP:3mic通话算法*/
#define TCFG_AUDIO_SINGLE_MIC_ENABLE	DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_DUAL_MIC_ENABLE		DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_TRIPLE_MIC_ENABLE    ENABLE_THIS_MOUDLE
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_DNS_MODE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_NORMAL
#else
#define TCFG_AUDIO_SINGLE_MIC_ENABLE	DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_TRIPLE_MIC_ENABLE    DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_DUAL_MIC_ENABLE		DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_CVP_NS_MODE	  	    CVP_ANS_MODE
#define TCFG_AUDIO_DMS_SEL	  	    	DMS_NORMAL
#endif

#if TCFG_ESCO_DL_CVSD_SR_USE_16K
#define TCFG_AUDIO_CVP_BAND_WIDTH_CFG   (CVP_WB_EN) //只保留16k参数
#else
#define TCFG_AUDIO_CVP_BAND_WIDTH_CFG   (CVP_NB_EN | CVP_WB_EN)  //同时保留8k和16k的参数
#endif

/*使能iis输出外部参考数据*/
#if (TCFG_IIS_NODE_ENABLE == 1) && (TCFG_DAC_NODE_ENABLE == 0)
#define TCFG_AUDIO_CVP_OUTPUT_WAY_IIS_ENABLE    1
#else
#define TCFG_AUDIO_CVP_OUTPUT_WAY_IIS_ENABLE    0
#endif

//*********************************************************************************//
//                                  imu-sensor配置                                   //
//*********************************************************************************//
#define TCFG_IMUSENSOR_ENABLE                	1    //imu Sensor使能
//mpu6887 cfg
#define TCFG_MPU6887P_ENABLE                  	1
#define TCFG_MPU6887P_INTERFACE_TYPE          	0 //0:iic, 1:spi
#define TCFG_MPU6887P_USER_IIC_TYPE           	0 //iic有效:1:硬件iic, 0:软件iic
#define TCFG_MPU6887P_USER_IIC_INDEX          	0 //IIC 序号
#define TCFG_MPU6887P_DETECT_IO               	(-1) //传感器中断io
#define TCFG_MPU6887P_AD0_SELETE_IO             IO_PORTC_03 //iic地址选择io

//icm42670 cfg
#define TCFG_ICM42670P_ENABLE                  	0
#define TCFG_ICM42670P_INTERFACE_TYPE          	0 //0:iic, 1:spi
#define TCFG_ICM42670P_USER_IIC_TYPE           	0 //iic有效:1:硬件iic, 0:软件iic
#define TCFG_ICM42670P_USER_IIC_INDEX          	0 //IIC 序号
#define TCFG_ICM42670P_DETECT_IO               	(-1) //传感器中断io
#define TCFG_ICM42670P_AD0_SELETE_IO            (-1) //iic地址选择io

#define VERSION_P                               1 //icm42670p
#define VERSION_L                               2 //icm42670l
#define TCFG_ICM42670_VERSION                   VERSION_L //版本选择

//lsm6dsl cfg
#define TCFG_LSM6DSL_ENABLE                     0
#define TCFG_LSM6DSL_INTERFACE_TYPE             0 //0:iic, 1:spi
#define TCFG_LSM6DSL_USER_IIC_TYPE              0 //1:硬件iic, 0:软件iic
#define TCFG_LSM6DSL_USER_IIC_INDEX          	0 //IIC 序号
#define TCFG_LSM6DSL_DETECT_IO               	(-1) //传感器中断io
#define TCFG_LSM6DSL_AD0_SELETE_IO              (-1) //iic地址选择io

/*
 *imu-sensor power manager
 *不用独立IO供电，则配置 NO_CONFIG_PORT
 */
#define TCFG_IMU_SENSOR_PWR_PORT                NO_CONFIG_PORT

//CVP测试代码使能
#define TCFG_AUDIO_CVP_DEMO_ENABLE				0

/*
 * 使能该配置后，需搭配[串口写卡小板]使用
 * 注：需要手动[使能] AUDIO_DATA_EXPORT_VIA_UART
 */
#define SENSOR_DATA_EXPORT_USE_UART 	        1
/*
 * 使能该配置后，需搭配[手机APP Audio Tools]使用
 * Audio Tools->记录 可录制陀螺仪数据，录制前需要[打开]使用陀螺仪的应用，如头部姿态检测/空间音效
 * Audio Tools->空间音效->传感器->校准 可校准陀螺仪，校准前需要[关闭]使用陀螺仪的应用，如头部姿态检测/空间音效
 * 注：1、若是tws方案，需要使用单耳连接手机(tws不配对)
 *     2、需要[关闭] AUDIO_DATA_EXPORT_VIA_SPP，否则与mic数据导出冲突
 */
#define SENSOR_DATA_EXPORT_USE_SPP 	            2
/*
 * 使能该配置后，需搭配[PC端工具 EffectTool->音频录制]使用
 * 注：需要手动[使能] AUDIO_DATA_EXPORT_VIA_SPP
 */
#define SENSOR_DATA_EXPORT_USE_PC_SPP           3
/*陀螺仪数据导出配置：量产版本应该关闭，仅作debug调试使用*/
#define TCFG_SENSOR_DATA_EXPORT_ENABLE          DISABLE_THIS_MOUDLE


/*Audio数据导出配置:通过蓝牙spp导出/sd写卡导出/uart写卡导出*/
#define AUDIO_DATA_EXPORT_VIA_UART	1
#define AUDIO_DATA_EXPORT_VIA_SPP 	2
#define AUDIO_DATA_EXPORT_VIA_SD	3
#define TCFG_AUDIO_DATA_EXPORT_DEFINE		DISABLE_THIS_MOUDLE

#if TCFG_DATA_EXPORT_NODE_ENABLE || AUDIO_ANC_DATA_EXPORT_VIA_UART
#undef TCFG_AUDIO_DATA_EXPORT_DEFINE
#define TCFG_AUDIO_DATA_EXPORT_DEFINE		AUDIO_DATA_EXPORT_VIA_UART
#endif

/*
 *蓝牙spp数据导出的mic 通道，调试双麦ENC时,需要和ENC的mic通道保持一致
 *目前支持导出2路mic数据
 */
#if TCFG_AUDIO_TRIPLE_MIC_ENABLE
#define TCFG_SPP_DATA_EXPORT_ADC_MIC_CHA	(AUDIO_ADC_MIC_0 | AUDIO_ADC_MIC_1 | AUDIO_ADC_MIC_2)
#else
#define TCFG_SPP_DATA_EXPORT_ADC_MIC_CHA	(AUDIO_ADC_MIC_0 | AUDIO_ADC_MIC_1)
#endif

#if TCFG_AUDIO_CVP_DEVELOP_ENABLE			/*通话第三方算法*/
#define TCFG_CVP_DEVELOP_ENABLE 			CVP_THIRD_ALGO_TYPE
#endif/*TCFG_AUDIO_CVP_DEVELOP_ENABLE*/

#if (TCFG_CVP_DEVELOP_ENABLE == CVP_CFG_AIS_3MIC)
#define CONFIG_BOARD_AISPEECH_NR
#endif /*TCFG_CVP_DEVELOP_ENABLE*/

#if TCFG_SPATIAL_AUDIO_ENABLE
#define TCFG_AUDIO_SPATIAL_EFFECT_ENABLE    ENABLE_THIS_MOUDLE
#else
#define TCFG_AUDIO_SPATIAL_EFFECT_ENABLE    DISABLE_THIS_MOUDLE
#endif /*TCFG_SPATIAL_AUDIO_ENABLE*/
#if (TCFG_TWS_SPATIAL_AUDIO_AS_CHANNEL == 0) && (TCFG_AUDIO_SOMATOSENSORY_ENABLE == 0)
#define TCFG_SPATIAL_AUDIO_SENSOR_ENABLE    0 //不使用传感器
#else
#define TCFG_SPATIAL_AUDIO_SENSOR_ENABLE    1//需要使用传感器
#endif /*TCFG_TWS_SPATIAL_AUDIO_AS_CHANNEL*/

/*空间音效、头部姿态检测和传感器的依赖*/
#if ((TCFG_AUDIO_SPATIAL_EFFECT_ENABLE == 0) || (TCFG_SPATIAL_AUDIO_SENSOR_ENABLE == 0)) && (TCFG_AUDIO_SOMATOSENSORY_ENABLE == 0)
#undef TCFG_IMUSENSOR_ENABLE
#undef TCFG_MPU6887P_ENABLE
#undef TCFG_ICM42670P_ENABLE
#undef TCFG_LSM6DSL_ENABLE
#define TCFG_IMUSENSOR_ENABLE              		0
#define TCFG_MPU6887P_ENABLE                  	0
#define TCFG_ICM42670P_ENABLE                  	0
#define TCFG_LSM6DSL_ENABLE                     0
#endif/*TCFG_AUDIO_SPATIAL_EFFECT_ENABLE*/

/*  device NOT supported */
#undef TCFG_QMI8658_ENABLE
#undef TCFG_TP_MPU9250_ENABLE
#undef TCFG_SH3001_ENABLE
#undef TCFG_MPU6050_EN
#define TCFG_QMI8658_ENABLE                     0
#define TCFG_TP_MPU9250_ENABLE                	0
#define TCFG_SH3001_ENABLE                    	0
#define TCFG_MPU6050_EN                     	0

/*Audio Smart Voice*/
#ifndef TCFG_SMART_VOICE_ENABLE
#define TCFG_SMART_VOICE_ENABLE 		    DISABLE_THIS_MOUDLE
#endif
#ifndef TCFG_AUDIO_KWS_LANGUAGE_SEL
#define TCFG_AUDIO_KWS_LANGUAGE_SEL    KWS_CH//近场中文
#endif

#define TCFG_KWS_VOICE_EVENT_HANDLE_ENABLE 	TCFG_SMART_VOICE_ENABLE //语音事件处理流程开关
#if TCFG_SMART_VOICE_ENABLE
#define TCFG_VAD_LOWPOWER_CLOCK             VAD_CLOCK_USE_PMU_STD12M
#else
#define TCFG_VAD_LOWPOWER_CLOCK             VAD_CLOCK_USE_LRC
#endif/*TCFG_AUDIO_SMART_VOICE_ENABLE*/
#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
#define TCFG_CALL_KWS_SWITCH_ENABLE         DISABLE_THIS_MOUDLE
#else
#define TCFG_CALL_KWS_SWITCH_ENABLE         TCFG_SMART_VOICE_ENABLE
#endif /*TCFG_KWS_VOICE_RECOGNITION_ENABLE*/


/*
 *第三方ASR（语音识别）配置
 *(1)用户自己开发算法
 *#define TCFG_AUDIO_ASR_DEVELOP             ASR_CFG_USER_DEFINED
 *(2)使用思必驰ASR算法
 *#define TCFG_AUDIO_ASR_DEVELOP             ASR_CFG_AIS
 */
#define ASR_CFG_USER_DEFINED	1
#define ASR_CFG_AIS				2
#define TCFG_AUDIO_ASR_DEVELOP				DISABLE_THIS_MOUDLE

#if (TCFG_AUDIO_ASR_DEVELOP	== ASR_CFG_AIS)
#undef TCFG_SMART_VOICE_ENABLE
#undef TCFG_KWS_VOICE_EVENT_HANDLE_ENABLE
#undef TCFG_VAD_LOWPOWER_CLOCK
#define TCFG_SMART_VOICE_ENABLE 		    DISABLE_THIS_MOUDLE
#define TCFG_KWS_VOICE_EVENT_HANDLE_ENABLE 	TCFG_SMART_VOICE_ENABLE //语音事件处理流程开关
#define TCFG_VAD_LOWPOWER_CLOCK             VAD_CLOCK_USE_RC_AND_BTOSC
#define CONFIG_BOARD_AISPEECH_VAD_ASR
#endif /*TCFG_AUDIO_ASR_DEVELOP*/

//关键词KWS是离线识别的一个子功能，不能同时开启
#if TCFG_SMART_VOICE_ENABLE && TCFG_KWS_VOICE_RECOGNITION_ENABLE
#error "smart voice and kws voice recognition can not be enabled at the same time"
#endif

/*
 *蓝牙音频能量检测使能配置
 *(1)1t2抢断播放需要使能能量检测
 *(2)蓝牙后台需要使能能量检测
 */
// #if ((TCFG_A2DP_PREEMPTED_ENABLE == 1) || (TCFG_BT_BACKGROUND_ENABLE == 1))
#if 1
#define TCFG_A2DP_SBC_SILENCE_DETECT_ENABLE	1
#define TCFG_A2DP_AAC_SILENCE_DETECT_ENABLE TCFG_BT_SUPPORT_AAC
#define TCFG_A2DP_LDAC_SILENCE_DETECT_ENABLE TCFG_BT_SUPPORT_LDAC
#else
#define TCFG_A2DP_SILENCE_DETECT_ENABLE	0
#define TCFG_A2DP_SBC_SILENCE_DETECT_ENABLE	0
#define TCFG_A2DP_AAC_SILENCE_DETECT_ENABLE	0
#define TCFG_A2DP_LDAC_SILENCE_DETECT_ENABLE 0
#endif

/*
 * <<蓝牙音频播放使能控制>>
 * 比如关闭A2DP播放器，则手机播歌的时候，蓝牙数据传输是正常的，但是因为没有使能解码器，
 * 所以没有声音
 */
#define TCFG_BT_A2DP_PLAYER_ENABLE		1
#define TCFG_BT_ESCO_PLAYER_ENABLE		1

/*
 * 自研ENC、ANC产测的依赖关系
 */
#if TCFG_ANC_HAT_DUT || AUDIO_ENC_MPT_SELF_ENABLE || TCFG_APP_KEY_DUT_ENABLE
#if (!TCFG_ANC_BOX_ENABLE) || (!TCFG_TEST_BOX_ENABLE) || (!TCFG_AUDIO_DUT_ENABLE) || (!TCFG_BT_VOL_SYNC_ENABLE)
#error "自研ENC、ANC产测必须开启测试盒串口升级、ANC串口调试、音频/通话产测、音量同步！！"
#endif
#endif

#if TCFG_ANC_HAT_DUT && (TCFG_AUDIO_ANC_TRAIN_MODE != ANC_FF_EN)
#if TCFG_SPEAKER_EQ_NODE_ENABLE == 0
#error "自研ANC产测 FB(HYBRID)方案， 需打开SPK_EQ功能"
#endif
#define TCFG_ANC_SELF_DUT_GET_SZ 	ENABLE_THIS_MOUDLE
#if (!TCFG_TEST_BOX_ENABLE) || (!TCFG_ANC_BOX_ENABLE)
#error "自研ANC产测，必须打开测试盒串口升级以及ANC串口调试/产测功能"
#endif

#else
#define TCFG_ANC_SELF_DUT_GET_SZ 	DISABLE_THIS_MOUDLE
#endif

#if TCFG_AUDIO_MIC_ARRAY_TRIM_ENABLE
#if !TCFG_AUDIO_DUT_ENABLE
#error "麦克风阵列校准需要打开音频/通话产测！！"
#endif
#endif

#if (TCFG_SMART_VOICE_ENABLE && TCFG_SMART_VOICE_USE_AEC)
#if (TCFG_AUDIO_GLOBAL_SAMPLE_RATE == 0) || (TCFG_AUDIO_GLOBAL_SAMPLE_RATE % 16000)
#error "TCFG_AUDIO_GLOBAL_SAMPLE_RATE 需要设置16000的倍数"
#endif /*TCFG_AUDIO_GLOBAL_SAMPLE_RATE*/
#endif /*TCFG_SMART_VOICE_ENABLE*/

#if TCFG_AUDIO_HEARING_AID_ENABLE && !TCFG_AUDIO_GLOBAL_SAMPLE_RATE
#error "开辅听需要固定全局输出采样率"
#endif
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_UNICAST_SINK_EN)))
#if ((TCFG_AUDIO_GLOBAL_SAMPLE_RATE % 16000 ) != 0)
#error "open LE_AUDIO_JL_UNICAST TCFG_AUDIO_GLOBAL_SAMPLE_RATE 需要设置采样率为16k的倍数，方便做变采样给通话算法处理"
#endif
#endif

#if (TCFG_AUDIO_SINGLE_MIC_ENABLE + TCFG_AUDIO_DUAL_MIC_ENABLE + TCFG_AUDIO_TRIPLE_MIC_ENABLE > 1)
#error "整个SDK数据流里面只能使用一种模式的CVP通话节点"
#endif

#if TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN

// dongle项目的DAC缓冲长度至少需要最少80ms
#undef TCFG_AUDIO_DAC_BUFFER_TIME_MS
#define TCFG_AUDIO_DAC_BUFFER_TIME_MS 80 // 缓冲长度（ms）

#endif

//*********************************************************************************//
//                    充电中按键清除手机配对信息配置                               //
//*********************************************************************************//

#define CHARGING_CLEAN_PHONE_INFO       0



//*********************************************************************************//
//                                 调音工具                                        //
//*********************************************************************************//
#define TCFG_ONLINE_ENABLE                  TCFG_CFG_TOOL_ENABLE    //是否支持音效在线调试功能
#define TCFG_NULL_COMM						0				//不支持通信
#define TCFG_UART_COMM						1				//串口通信
#define TCFG_SPP_COMM						2				//SPP通信
#define TCFG_USB_COMM						3				//USB通信

/***********************************非用户配置区***********************************/
#include "usb_std_class_def.h"
#if TCFG_CFG_TOOL_ENABLE
#if (TCFG_COMM_TYPE == TCFG_USB_COMM)
#undef TCFG_USB_CDC_BACKGROUND_RUN
#define TCFG_USB_CDC_BACKGROUND_RUN         ENABLE
#endif
#if (TCFG_COMM_TYPE == TCFG_UART_COMM)
#undef TCFG_USB_CDC_BACKGROUND_RUN
#define TCFG_USB_CDC_BACKGROUND_RUN         DISABLE
//1M波特率（1us）即使加了IO唤醒流程，也无法唤醒（信号时间太短），需要关了低功耗
#undef TCFG_LOWPOWER_LOWPOWER_SEL
#define TCFG_LOWPOWER_LOWPOWER_SEL          0
#endif
#endif
#include "usb_common_def.h"

#if TCFG_AI_TRANSLATOR_ENABLE
//通话翻译支持opus立体声编码（使能celt编码）
#undef  TCFG_ENCODER_CHANNEL_NUM
#define TCFG_ENCODER_CHANNEL_NUM            (BIT(0)|BIT(1))
#endif

#include "ai_audio_config.h"

#if (TCFG_AUDIO_ASR_DEVELOP && TCFG_CVP_DEVELOP_ENABLE) || TCFG_VIRTUAL_SURROUND_PRO_MODULE_NODE_ENABLE || (TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_JL_UNICAST_SINK_EN)

#define TCFG_LOWPOWER_RAM_SIZE				0	                //ram:640-128*TCFG_LOWPOWER_RAM_SIZE 低功耗掉电ram大小，单位：128K，可设置值：0、2、3

#elif (TCFG_BT_DONGLE_ENABLE||TCFG_SMART_VOICE_ENABLE || TCFG_AUDIO_ASR_DEVELOP || TCFG_CVP_DEVELOP_ENABLE \
		|| TCFG_AUDIO_SPATIAL_EFFECT_ENABLE || TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN || TCFG_ANC_SELF_DUT_GET_SZ \
		|| TCFG_AUDIO_TRIPLE_MIC_ENABLE || (TCFG_USB_SLAVE_AUDIO_MIC_ENABLE && (TCFG_AUDIO_CVP_DMS_DNS_MODE)) \
		|| TCFG_AUDIO_FIT_DET_ENABLE)

#define TCFG_LOWPOWER_RAM_SIZE				2	                // 低功耗掉电ram大小，单位：128K，可设置值：0、2、3

#elif TCFG_USER_BLE_ENABLE

#define TCFG_LOWPOWER_RAM_SIZE              2	                // 低功耗掉电ram大小，单位：128K，可设置值：0、2、3

#else

#define TCFG_LOWPOWER_RAM_SIZE				3	                // 低功耗掉电ram大小，单位：128K，可设置值：0、2、3

#if ((TCFG_LE_AUDIO_APP_CONFIG & LE_AUDIO_UNICAST_SINK_EN) || TCFG_AUDIO_BIT_WIDTH || TCFG_BT_SUPPORT_LHDC_V5 || TCFG_BT_SUPPORT_LHDC || TCFG_BT_SUPPORT_LDAC) && (TCFG_LOWPOWER_RAM_SIZE >= 2)
#undef TCFG_LOWPOWER_RAM_SIZE
#define TCFG_LOWPOWER_RAM_SIZE              2                   // 低功耗掉电ram大小，单位：128K，可设置值：0、2、3
#endif

#endif

/*spp数据导出配置*/
#if ((TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SPP) || TCFG_AUDIO_MIC_DUT_ENABLE)
#undef TCFG_USER_TWS_ENABLE
#undef TCFG_USER_BLE_ENABLE
#undef TCFG_BD_NUM
#undef TCFG_BT_SUPPORT_SPP
#undef TCFG_BT_SUPPORT_A2DP
#define TCFG_USER_TWS_ENABLE        0//spp数据导出，关闭tws
#define TCFG_USER_BLE_ENABLE        0//spp数据导出，关闭ble
#define TCFG_BD_NUM					1//连接设备个数配置
#define TCFG_BT_SUPPORT_SPP	1
#define TCFG_BT_SUPPORT_A2DP   0
#define APP_ONLINE_DEBUG            1//通过spp导出数据
#else
#define APP_ONLINE_DEBUG            0//在线APP调试,发布默认不开
#endif/*TCFG_AUDIO_DATA_EXPORT_DEFINE*/

//*********************************************************************************//
//                    需要spp调试的配置                                            //
//*********************************************************************************//
#if ( \
     TCFG_AEC_TOOL_ONLINE_ENABLE                                        || \
     TCFG_AUDIO_DUT_ENABLE      	                                    || \
     TCFG_ANC_TOOL_DEBUG_ONLINE                                         || \
     TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE                                   || \
     (TCFG_CFG_TOOL_ENABLE && (TCFG_COMM_TYPE == TCFG_SPP_COMM))        || \
     TCFG_SPEAKER_EQ_NODE_ENABLE                                        || \
     (TCFG_SENSOR_DATA_EXPORT_ENABLE == SENSOR_DATA_EXPORT_USE_SPP)     || \
     TCFG_SPATIAL_EFFECT_ONLINE_ENABLE                                  || \
     (TCFG_AUDIO_HEARING_AID_ENABLE && TCFG_AUDIO_DHA_FITTING_ENABLE) /*辅听*/  \
    )
#undef TCFG_BT_SUPPORT_SPP
#undef APP_ONLINE_DEBUG
#define TCFG_BT_SUPPORT_SPP	1
#define APP_ONLINE_DEBUG            1
#endif

#if APP_ONLINE_DEBUG
#undef THIRD_PARTY_PROTOCOLS_SEL
#if (TCFG_THIRD_PARTY_PROTOCOLS_ENABLE && (TCFG_THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN | GFPS_EN | MMA_EN | FMNA_EN | TUYA_DEMO_EN |REALME_EN | SWIFT_PAIR_EN | DMA_EN | CUSTOM_DEMO_EN | XIMALAYA_EN | RDX_EN | MULTI_CLIENT_EN | JL_SBOX_EN | HID_ISO_EN))) || ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#define THIRD_PARTY_PROTOCOLS_SEL  (TCFG_THIRD_PARTY_PROTOCOLS_SEL | ONLINE_DEBUG_EN)
#else
#define THIRD_PARTY_PROTOCOLS_SEL  (ONLINE_DEBUG_EN)
#endif
#endif

#if TCFG_CFG_TOOL_ENABLE
#undef EQ_SECTION_MAX
#define EQ_SECTION_MAX 32
#endif

#ifndef EQ_SECTION_MAX
#if TCFG_EQ_ENABLE
#define EQ_SECTION_MAX 32
#endif
#endif

#define TCFG_USER_RSSI_TEST_EN   0   //通过spp获取耳机RSSI值，需要使能TCFG_BT_SUPPORT_SPP

//合并 配置和board的协议宏，后续放到配置工具配置
#define  TCFG_BT_SUPPORT_PNP 0x1
#define  TCFG_BT_SUPPORT_PBAP 0x0
#define TCFG_BT_SUPPORT_PBAP_LIST 0 // pbap单条查询
#if TCFG_BT_SUPPORT_PBAP_LIST
#undef  TCFG_BT_SUPPORT_PBAP
#define  TCFG_BT_SUPPORT_PBAP 0x1
#endif
#define  TCFG_BT_SUPPORT_MAP 0x0	// 蓝牙连接后，会向系统发送短信权限申请
#define  TCFG_BT_SUPPORT_MAP_MESSAGE 0x0 //map 短信全功能
#if TCFG_BT_SUPPORT_MAP_MESSAGE
#undef  TCFG_BT_SUPPORT_MAP
#define  TCFG_BT_SUPPORT_MAP 0x1
#endif
#define  TCFG_BLE_BRIDGE_EDR_ENALBE 0x0   //ios 一键连接，ctkd
#if TCFG_BLE_BRIDGE_EDR_ENALBE   //一键连接必须同地址
#undef  TCFG_BT_BLE_BREDR_SAME_ADDR
#define  TCFG_BT_BLE_BREDR_SAME_ADDR 0x1
#endif

#define TCFG_IFLYTEK_ENABLE			0
#ifdef TCFG_BT_SUPPORT_PAN
#if TCFG_BT_SUPPORT_PAN
#undef TCFG_IFLYTEK_ENABLE
#define TCFG_IFLYTEK_ENABLE			1
#endif
#endif
#if TCFG_IFLYTEK_ENABLE//目前耳机没有rtc，先用蓝牙时间转UTC再转GMT(earphone.c)
#undef  TCFG_BT_SUPPORT_MAP
#define  TCFG_BT_SUPPORT_MAP 0x1
#endif

//*********************************************************************************//
//                    电流校准(+筛选)范围配置                                      //
//*********************************************************************************//
#if TCFG_CHARGE_ENABLE
#define TCFG_CHARGE_CUR_MIN         0//烧写器电流筛选最低值 -- 配置0不使能筛选,根据方案自行配置筛选范围
#define TCFG_CHARGE_CUR_MAX         0//烧写器电流筛选最高值 -- 配置0不使能筛选,根据方案自行配置筛选范围
#endif

//*********************************************************************************//
//                   NTC配置                                      //
//*********************************************************************************//
#if NTC_DET_EN && NTC_DET_PULLUP_TYPE   //ntc如果选择内部上拉的，检测IO要选择对应的内部上拉引脚，具体看各芯片方案的原理图
#if defined(CONFIG_CPU_BR56) && (NTC_DETECT_IO != IO_PORTC_01)
#error "710 NTC_DETECT_IO must be IO_PORTC_01"
#elif defined(CONFIG_CPU_BR52) && (NTC_DETECT_IO != IO_PORTC_03)
#error "709 NTC_DETECT_IO must be IO_PORTC_03"
#elif defined(CONFIG_CPU_BR50) && (NTC_DETECT_IO != IO_PORTC_05)
#error "708 NTC_DETECT_IO must be IO_PORTC_05"
#endif
#endif

//*********************************************************************************//
//                    异常记录/离线log配置                                      //
//*********************************************************************************//
#define TCFG_CONFIG_DEBUG_RECORD_ENABLE    0

#if !TCFG_DEBUG_UART_ENABLE
#define TCFG_DEBUG_DLOG_ENABLE             0      // 离线log功能
#define TCFG_DEBUG_DLOG_FLASH_SEL          1      // 选择log保存到内置flash还是外置flash; 0:内置flash; 1:外置flash
#define TCFG_DLOG_FLASH_START_ADDR         (0x00)         // 配置外置flash用于存储dlog和异常数据的区域起始地址
#define TCFG_DLOG_FLASH_REGION_SIZE        (512 * 1024)   // 配置外置flash用于存储dlog和异常数据的区域大小
#if (TCFG_DEBUG_DLOG_ENABLE && TCFG_DEBUG_DLOG_FLASH_SEL)
#if (!defined(TCFG_NORFLASH_DEV_ENABLE) || (TCFG_NORFLASH_DEV_ENABLE == 0))
#undef TCFG_NORFLASH_DEV_ENABLE
#define TCFG_NORFLASH_DEV_ENABLE           1              // 使能外置flash驱动
#define TCFG_NORFLASH_START_ADDR           (0x00)         // 配置外置flash起始地址
#define TCFG_NORFLASH_SIZE                 (512 * 1024)   // 配置外置flash大小
#endif
#endif
#define TCFG_DEBUG_DLOG_RESET_ERASE        0      // 开机擦除flash的log数据
#define TCFG_DEBUG_DLOG_AUTO_FLUSH_TIMEOUT (30)   // 主动刷新的超时时间(当指定时间没有刷新过缓存数据到flash, 则主动刷新)(单位秒)
#define TCFG_DEBUG_DLOG_UART_TX_PIN        IO_PORT_DP  // dlog串口打印的引脚
#if (defined(LIB_DEBUG) && TCFG_DEBUG_DLOG_ENABLE)
#undef LIB_DEBUG
#define LIB_DEBUG    1
#undef CONFIG_DEBUG_LIB
#define CONFIG_DEBUG_LIB(x)         (x & LIB_DEBUG)
#endif
#if ((TCFG_DEBUG_DLOG_ENABLE) && (!defined(DLOG_PRINT_FUNC_USE_MACRO) || (DLOG_PRINT_FUNC_USE_MACRO == 0)))
#error "DLOG_PRINT_FUNC_USE_MACRO must be enable"
#endif
#endif


//*********************************************************************************//
//                    指示方案为Type-C耳机(只有PC模式)                             //
//*********************************************************************************//
#if TCFG_APP_PC_EN && (TCFG_APP_BT_EN == 0) && (TCFG_APP_MUSIC_EN == 0) && (TCFG_APP_LINEIN_EN == 0)
#define TCFG_TYPEC_EARPHONE_CASE            1
#undef TCFG_OTG_MODE
#define TCFG_OTG_MODE                       0
#else
#define TCFG_TYPEC_EARPHONE_CASE            0
#endif
#if TCFG_TYPEC_EARPHONE_CASE && (TCFG_EARPHONE_TYPE == 0)
//四线耳机,audio_setup时初始化dac电源
#undef TCFG_AUDIO_DAC_POWER_ON_AT_SETUP
#define TCFG_AUDIO_DAC_POWER_ON_AT_SETUP    1
#endif

/* RDX final gate depends on the resolved BLE and Type-C application config. */
#include "rdx_integration_config.h"

#ifndef __LD__
#include "bt_profile_cfg.h"
#endif

#endif
