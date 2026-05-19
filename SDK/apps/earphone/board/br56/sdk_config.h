/**
* 注意点：
* 0.此文件变化，在工具端会自动同步修改到工具配置中
* 1.功能块通过【---------xxx------】和 【#endif // xxx 】，是工具识别的关键位置，请勿随意改动
* 2.目前工具暂不支持非文件已有的C语言语法，此文件应使用文件内已有的语法增加业务所需的代码，避免产生不必要的bug
* 3.修改该文件出现工具同步异常或者导出异常时，请先检查文件内语法是否正常
**/

#ifndef SDK_CONFIG_H
#define SDK_CONFIG_H

#include "jlstream_node_cfg.h"

// ------------JLStudio Auto generate------------
#define PROJECT_CONFIG_NAME                       ""               // 默认配置为空，非默认配置为配置名称
// ------------JLStudio Auto generate------------

// ------------电源配置.json------------
#define TCFG_CLOCK_OSC_HZ                         24000000         // 晶振频率
#define TCFG_LOWPOWER_OSC_TYPE                    OSC_TYPE_LRC     // 低功耗时钟源
#define TCFG_LOWPOWER_POWER_SEL                   PWR_LDO15        // 电源模式
#define TCFG_LOWPOWER_VDDIOM_LEVEL                VDDIOM_VOL_34V   // 强VDDIO
#define TCFG_LOWPOWER_VDDIOW_LEVEL                VDDIOW_VOL_26V   // 弱VDDIO
#define TCFG_LOWPOWER_VDDIO_KEEP                  1                // 关机保持VDDIO
#define TCFG_LOWPOWER_LOWPOWER_SEL                0                // 低功耗模式
#define TCFG_AUTO_POWERON_ENABLE                  1                // 上电自动开机

#define TCFG_CHARGESTORE_ENABLE                   0                // 智能仓

#define TCFG_CHARGE_ENABLE                        0                // 充电配置
#if TCFG_CHARGE_ENABLE
#define TCFG_CHARGE_TRICKLE_MA                    CHARGE_mA_45     // 涓流档位选择
#define TCFG_CHARGE_TRICKLE_DIV                   CHARGE_DIV_5     // 涓流分频系数
#define TCFG_CHARGE_MA                            CHARGE_mA_45     // 恒流档位选择
#define TCFG_CHARGE_DIV                           CHARGE_DIV_1     // 恒流分频系数
#define TCFG_CHARGE_FULL_V                        CHARGE_FULL_V_MIN_4200 // 截止电压
#define TCFG_CHARGE_FULL_MA                       CHARGE_FC_IS_CC_DIV_5 // 截止电流
#define TCFG_CHARGE_POWERON_ENABLE                0                // 开机充电
#define TCFG_CHARGE_OFF_POWERON_EN                1                // 拔出开机
#define TCFG_CHARGE_NVDC_EN                       0                // NVDC架构使能
#define TCFG_LDOIN_PULLDOWN_EN                    1                // 下拉电阻开关
#define TCFG_LDOIN_PULLDOWN_KEEP                  0                // 下拉电阻保持开关
#define TCFG_LDOIN_PULLDOWN_LEV                   CHARGE_PULLDOWN_200K // 下拉电阻档位
#define TCFG_LDOIN_ON_FILTER_TIME                 100              // 入舱滤波时间(ms)
#define TCFG_LDOIN_OFF_FILTER_TIME                200              // 出舱滤波时间(ms)
#define TCFG_LDOIN_KEEP_FILTER_TIME               440              // 维持电压滤波时间(ms)
#endif // TCFG_CHARGE_ENABLE

#define TCFG_SYS_LVD_EN                           0                // 电池电量检测
#if TCFG_SYS_LVD_EN
#define TCFG_POWER_OFF_VOLTAGE                    3300             // 关机电压(mV)
#define TCFG_POWER_WARN_VOLTAGE                   3400             // 低电电压(mV)
#endif // TCFG_SYS_LVD_EN

#define TCFG_BATTERY_CURVE_ENABLE                 0                // 电池曲线配置

// ------------电源配置.json------------

// ------------板级配置.json------------
#define TCFG_DEBUG_UART_ENABLE                    1                // 调试串口
#if TCFG_DEBUG_UART_ENABLE
#define TCFG_DEBUG_UART_TX_PIN                    IO_PORTA_05      // 输出IO
#define TCFG_DEBUG_UART_BAUDRATE                  2000000          // 波特率
#define TCFG_EXCEPTION_LOG_ENABLE                 1                // 打印异常信息
#define TCFG_EXCEPTION_RESET_ENABLE               1                // 异常自动复位
#endif // TCFG_DEBUG_UART_ENABLE

#define TCFG_CFG_TOOL_ENABLE                      0                // FW编辑、在线调音
#if TCFG_CFG_TOOL_ENABLE
#define TCFG_ONLINE_TX_PORT                       IO_PORT_DP       // 串口引脚TX
#define TCFG_ONLINE_RX_PORT                       IO_PORT_DM       // 串口引脚RX
#define TCFG_COMM_TYPE                            TCFG_UART_COMM   // 通信方式
#endif // TCFG_CFG_TOOL_ENABLE

#define CONFIG_SPI_DATA_WIDTH                     4                // flash通信
#define CONFIG_FLASH_SIZE                         0x100000         // flash容量
#define TCFG_VM_SIZE                              40               // VM大小(K)
#define TCFG_JLSTREAM_EFFICIENT_MODE              0                // 音频流高效模式

#define TCFG_PWMLED_ENABLE                        0                // LED配置
#if TCFG_PWMLED_ENABLE
#define TCFG_LED_LAYOUT                           ONE_IO_TWO_LED   // 连接方式
#define TCFG_LED_RED_ENABLE                       1                // 红灯
#define TCFG_LED_RED_GPIO                         IO_PORTC_02      // IO
#define TCFG_LED_RED_LOGIC                        BRIGHT_BY_HIGH   // 点亮方式
#define TCFG_LED_BLUE_ENABLE                      1                // 蓝灯
#define TCFG_LED_BLUE_GPIO                        IO_PORTC_02      // IO
#define TCFG_LED_BLUE_LOGIC                       BRIGHT_BY_LOW    // 点亮方式
#endif // TCFG_PWMLED_ENABLE

#define TCFG_SD0_ENABLE                           0                // SD配置
#if TCFG_SD0_ENABLE
#define TCFG_SD0_DAT_MODE                         1                // 线数设置
#define TCFG_SD0_DET_MODE                         SD_CMD_DECT      // 检测方式
#define TCFG_SD0_CLK                              12000000         // SD时钟频率
#define TCFG_SD0_DET_IO                           NO_CONFIG_PORT   // 检测IO
#define TCFG_SD0_DET_IO_LEVEL                     0                // IO检测方式
#define TCFG_SD0_POWER_SEL                        SD_PWR_NULL      // SD卡电源
#define TCFG_SDX_CAN_OPERATE_MMC_CARD             0                // 支持MMC卡
#define TCFG_KEEP_CARD_AT_ACTIVE_STATUS           0                // 保持卡活跃状态
#define TCFG_SD0_PORT_CMD                         IO_PORTC_04      // SD_PORT_CMD
#define TCFG_SD0_PORT_CLK                         IO_PORTC_03      // SD_PORT_CLK
#define TCFG_SD0_PORT_DA0                         IO_PORTC_02      // SD_PORT_DATA0
#define TCFG_SD0_PORT_DA1                         NO_CONFIG_PORT   // SD_PORT_DATA1
#define TCFG_SD0_PORT_DA2                         NO_CONFIG_PORT   // SD_PORT_DATA2
#define TCFG_SD0_PORT_DA3                         NO_CONFIG_PORT   // SD_PORT_DATA3
#endif // TCFG_SD0_ENABLE

#define FUSB_MODE                                 1                // USB工作模式
#define TCFG_USB_HOST_ENABLE                      0                // USB主机总开关
#define USB_H_MALLOC_ENABLE                       1                // 主机使用malloc
#define TCFG_USB_HOST_MOUNT_RESET                 40               // usb reset时间
#define TCFG_USB_HOST_MOUNT_TIMEOUT               50               // 握手超时时间
#define TCFG_USB_HOST_MOUNT_RETRY                 3                // 枚举失败重试次数
#define TCFG_UDISK_ENABLE                         0                // U盘使能
#define TCFG_USB_SLAVE_MSD_ENABLE                 0                // 读卡器使能
#define TCFG_USB_SLAVE_HID_ENABLE                 1                // HID使能
#define MSD_BLOCK_NUM                             1                // MSD缓存块数
#define USB_AUDIO_VERSION                         USB_AUDIO_VERSION_1_0 // UAC协议版本
#define TCFG_USB_SLAVE_AUDIO_SPK_ENABLE           1                // USB扬声器使能
#define SPK_AUDIO_RATE_NUM                        3                // SPK采样率列表
#define SPK_AUDIO_RES                             24               // SPK位宽1
#define SPK_AUDIO_RES_2                           16               // SPK位宽2
#define TCFG_USB_SLAVE_AUDIO_MIC_ENABLE           1                // USB麦克风使能
#define MIC_AUDIO_RATE_NUM                        3                // MIC采样率列表
#define MIC_AUDIO_RES                             16               // MIC位宽1
#define MIC_AUDIO_RES_2                           0                // MIC位宽2

#define TCFG_LINEIN_DETECT_ENABLE                 0                // LINEIN检测配置
#if TCFG_LINEIN_DETECT_ENABLE
#define TCFG_LINEIN_DETECT_IO                     IO_PORTB_03      // 检测IO选择
#define TCFG_LINEIN_DETECT_PULL_UP_ENABLE         1                // 检测IO上拉使能
#define TCFG_LINEIN_DETECT_PULL_DOWN_ENABLE       0                // 检测IO下拉使能
#define TCFG_LINEIN_AD_DETECT_ENABLE              0                // 使能AD检测
#define TCFG_LINEIN_AD_DETECT_VALUE               0                // AD检测时阈值
#endif // TCFG_LINEIN_DETECT_ENABLE

#define TCFG_IO_CFG_AT_POWER_ON                   0                // 开机时IO配置

#define TCFG_IO_CFG_AT_POWER_OFF                  0                // 关机时IO配置

#define TCFG_CHARGESTORE_PORT                     IO_PORT_LDOIN    // 通信IO
#define TDM_WORK_MODE                             4                // Work Mode
#define TDM_CH_NUM                                2                // Channels

#define NTC_DET_EN                                0                // NTC检测
#if NTC_DET_EN
#define NTC_DETECT_IO                             IO_PORTC_04      // 检测IO选择
#define NTC_DET_PULLUP_TYPE                       1                // 上拉电阻
#define NTC_POWER_IO                              IO_PORTC_03      // 供电IO选择
#define NTC_UPPER_RES                             235              // NTC阻值上限
#define NTC_LOWER_RES                             34               // NTC阻值下限
#endif // NTC_DET_EN
// ------------板级配置.json------------

// ------------按键配置.json------------
#define TCFG_TWS_COMBINATIION_KEY_ENABLE          0                // TWS两边同时按消息使能
#define TCFG_SEND_HOLD_SEC_MSG_DURING_HOLD        0                // 按住过程中发送按住几秒消息
#define TCFG_MAX_HOLD_SEC                         ((KEY_ACTION_HOLD_5SEC << 8) | 5) // 最长按住消息

#define TCFG_IOKEY_ENABLE                         0                // IO按键配置

#define TCFG_ADKEY_ENABLE                         0                // AD按键配置

#define TCFG_MICKEY_ENABLE                        1                // MIC按键配置

#define TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE          0                // 内置触摸在线调试

#define TCFG_LP_TOUCH_KEY_ENABLE                  0                // 内置触摸按键配置
#if TCFG_LP_TOUCH_KEY_ENABLE
#define TCFG_LP_KEY_LIMIT_VOLTAGE_DELTA           800              // 上下限电压差
#define TCFG_LP_KEY_CHARGE_FREQ_KHz               2500             // 充放电频率
#define TCFG_LP_KEY_ENABLE_IN_CHARGE              0                // 充电保持触摸
#define TCFG_LP_KEY_LONG_PRESS_RESET              0                // 长按复位功能
#define TCFG_LP_KEY_LONG_PRESS_RESET_TIME         8000             // 长按复位时间
#define TCFG_LP_KEY_SLIDE_ENABLE                  0                // 两个按键滑动
#define TCFG_LP_KEY_SLIDE_VALUE                   KEY_SLIDER       // 键值
#define TCFG_LP_EARTCH_KEY_ENABLE                 0                // 入耳检测总开关
#define TCFG_LP_EARTCH_DETECT_RELY_AUDIO          0                // 检测方式
#endif // TCFG_LP_TOUCH_KEY_ENABLE
// ------------按键配置.json------------

// ------------蓝牙配置.json------------
#define TCFG_BT_NAME_SEL_BY_AD_ENABLE             0                // 蓝牙名(AD采样)

#define TCFG_BT_3M_ENABLE                         0                // 3M包选项
#define TCFG_BT_OSC_OFFSET                        0                // 固定频偏(kHz)
#define TCFG_BT_PAGE_TIMEOUT                      8                // 单次回连时间(s)
#define TCFG_BT_POWERON_PAGE_TIME                 60               // 开机回连超时(s)
#define TCFG_BT_TIMEOUT_PAGE_TIME                 120              // 超距断开回连超时(s)
#define TCFG_DUAL_CONN_INQUIRY_SCAN_TIME          120              // 开机可被发现时间 (s)
#define TCFG_DUAL_CONN_PAGE_SCAN_TIME             0                // 等待第二台连接时间(s)
#define TCFG_AUTO_SHUT_DOWN_TIME                  180              // 无连接关机时间(s)
#define TCFG_A2DP_DELAY_TIME_SBC                  300              // A2DP延时SBC(msec)
#define TCFG_A2DP_DELAY_TIME_SBC_LO               60               // A2DP低延时SBC(msec)
#define TCFG_A2DP_DELAY_TIME_AAC                  300              // A2DP延时AAC(msec)
#define TCFG_A2DP_DELAY_TIME_AAC_LO               100              // A2DP低延时AAC(msec)
#define TCFG_A2DP_ADAPTIVE_MAX_LATENCY            550              // A2DP自适应最大延时(msec)
#define TCFG_A2DP_DELAY_TIME_LDAC                 300              // A2DP延时LDAC(msec)
#define TCFG_A2DP_DELAY_TIME_LDAC_LO              300              // A2DP低延时LDAC(msec)
#define TCFG_A2DP_DELAY_TIME_LHDC                 300              // A2DP延时LHDC(msec)
#define TCFG_A2DP_DELAY_TIME_LHDC_LO              300              // A2DP低延时LHDC(msec)
#define TCFG_BT_DUAL_CONN_ENABLE                  0                // 一拖二
#define TCFG_A2DP_PREEMPTED_ENABLE                0                // A2DP抢播
#define TCFG_BT_VOL_SYNC_ENABLE                   0                // 音量同步
#define TCFG_BT_DISPLAY_BAT_ENABLE                0                // 电量显示
#define TCFG_BT_INBAND_RING                       0                // 手机铃声
#define TCFG_BT_PHONE_NUMBER_ENABLE               0                // 来电报号
#define TCFG_BT_SUPPORT_AAC                       0                // AAC
#define TCFG_BT_MSBC_EN                           0                // MSBC
#define TCFG_BT_SBC_BITPOOL                       43               // sbcBitPool
#define TCFG_BT_SUPPORT_LHDC                      0                // LHDC_V3/V4
#define TCFG_BT_SUPPORT_LHDC_V5                   0                // LHDC_V5
#define TCFG_BT_SUPPORT_LDAC                      0                // LDAC
#define TCFG_BT_SUPPORT_HFP                       0                // HFP
#define TCFG_BT_SUPPORT_AVCTP                     0                // AVRCP
#define TCFG_BT_SUPPORT_A2DP                      0                // A2DP
#define TCFG_BT_SUPPORT_HID                       0                // HID
#define TCFG_BT_SUPPORT_SPP                       0                // SPP
#define TCFG_BT_BACKGROUND_ENABLE                 0                // 蓝牙后台
#define TCFG_BT_BACKGROUND_GOBACK                 0                // 蓝牙后台连接断开返回
#define TCFG_BT_BACKGROUND_DETECT_TIME            750              // 音乐检测时间
#define CONFIG_BT_MODE                            BT_NORMAL        // 模式选择
#define TCFG_NORMAL_SET_DUT_MODE                  0                // NORMAL模式下使能DUT测试

#define TCFG_USER_TWS_ENABLE                      0                // TWS
#if TCFG_USER_TWS_ENABLE
#define CONFIG_COMMON_ADDR_MODE                   1                // MAC地址
#define TCFG_BT_TWS_PAIR_MODE                     CONFIG_TWS_PAIR_BY_AUTO // 配对方式
#define TCFG_BT_TWS_CHANNEL_SELECT                CONFIG_TWS_MASTER_AS_LEFT // 声道选择
#define CONFIG_TWS_CHANNEL_CHECK_IO               0                // 声道选择IO
#define TCFG_TWS_PAIR_TIMEOUT                     6                // 开机配对超时(s)
#define TCFG_TWS_CONN_TIMEOUT                     6                // 单次连接超时(s)
#define TCFG_TWS_POWERON_AUTO_PAIR_ENABLE         0                // 开机自动配对/连接
#define TCFG_TWS_AUTO_ROLE_SWITCH_ENABLE          0                // 自动主从切换
#define TCFG_TWS_POWER_BALANCE_ENABLE             0                // 主从电量平衡
#define CONFIG_TWS_AUTO_PAIR_WITHOUT_UNPAIR       0                // TWS连接超时自动配对新耳机
#define TCFG_TWS_AUDIO_SHARE_ENABLE               0                // TWS共享音频（BETA）
#define TCFG_BT_SHARE_PAGE_TIMEOUT                20               // 共享配对连接超时(s)
#define TCFG_BT_SHARE_PAGE_CONN_TIMEOUT           120              // 共享回连超时(s)
#define TCFG_TWS_AUDIO_SHARE_AUTO_SWITCH_ENABLE   0                // 互相共享音频自动切换
#define TCFG_TWS_SHARE_2PHONE_CONN_ENABLE         0                // 开启共享功能可连接2个手机
#endif // TCFG_USER_TWS_ENABLE

#define TCFG_BT_SNIFF_ENABLE                      0                // sniff
#if TCFG_BT_SNIFF_ENABLE
#define TCFG_SNIFF_CHECK_TIME                     5                // 检测时间
#define CONFIG_LRC_WIN_SIZE                       400              // LRC窗口初值
#define CONFIG_LRC_WIN_STEP                       400              // LRC窗口步进
#define CONFIG_OSC_WIN_SIZE                       400              // OSC窗口初值
#define CONFIG_OSC_WIN_STEP                       400              // OSC窗口步进
#endif // TCFG_BT_SNIFF_ENABLE

#define TCFG_USER_BLE_ENABLE                      0                // BLE
#if TCFG_USER_BLE_ENABLE
#define TCFG_BT_BLE_TX_POWER                      9                // 最大发射功率
#define TCFG_BT_BLE_BREDR_SAME_ADDR               1                // 和2.1同地址
#define TCFG_BT_BLE_ADV_ENABLE                    0                // 广播
#define TCFG_BLE_HIGH_PRIORITY_ENABLE             0                // 高优先级
#endif // TCFG_USER_BLE_ENABLE

#define TCFG_THIRD_PARTY_PROTOCOLS_ENABLE         0                // 第三方协议配置
#if TCFG_THIRD_PARTY_PROTOCOLS_ENABLE
#define TCFG_THIRD_PARTY_PROTOCOLS_SEL            0                // 第三方协议选择
#define RCSP_CHANNEL_SEL                          RCSP_USE_GATT_OVER_EDR // RCSP连接方式
#define TCFG_RCSP_DUAL_CONN_ENABLE                0                // 支持连接两路RCSP
#define TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED     0                // 三方协议轻量化
#endif // TCFG_THIRD_PARTY_PROTOCOLS_ENABLE

#define TCFG_LE_AUDIO_APP_CONFIG                  0                // LE_AUDIO 应用选择（选择后自动开启LC3编解码）
#define TCFG_LE_AUDIO_PLAY_LATENCY                30000            // LE_AUDIO播放延时（us）
#define TCFG_JL_UNICAST_EDR_MODE_SWITCH_ENABLE    0                // JL_UNICAST和EDR独立模式切换
#define TCFG_JL_UNICAST_BOUND_PAIR_EN             0                // JL_UNICAST绑定配对
// ------------蓝牙配置.json------------

// ------------功能配置.json------------
#define TCFG_APP_BT_EN                            0                // 蓝牙模式
#define TCFG_APP_MUSIC_EN                         0                // 音乐模式
#define TCFG_APP_LINEIN_EN                        0                // LINEIN模式
#define TCFG_APP_PC_EN                            1                // PC模式
#define TCFG_EARPHONE_TYPE                        2                // 耳机类型选择
#define TCFG_MIC_EFFECT_ENABLE                    0                // 混响使能
#define TCFG_DEC_ID3_V2_ENABLE                    0                // ID3_V2
#define TCFG_DEC_ID3_V1_ENABLE                    0                // ID3_V1
#define FILE_DEC_REPEAT_EN                        0                // 无缝循环播放
#define FILE_DEC_DEST_PLAY                        0                // 指定时间播放
#define FILE_DEC_AB_REPEAT_EN                     0                // AB点复读
#define TCFG_DEC_DECRYPT_ENABLE                   0                // 加密文件播
#define TCFG_DEC_DECRYPT_KEY                      0x12345678       // 加密KEY
#define MUSIC_PLAYER_CYCLE_ALL_DEV_EN             1                // 循环播放模式是否循环所有设备
#define MUSIC_PLAYER_PLAY_FOLDER_PREV_FIRST_FILE_EN 0                // 切换文件夹播放时从第一首歌开始
#define TCFG_MUSIC_DEVICE_TONE_EN                 0                // 设备提示音
#define TWFG_APP_POWERON_IGNORE_DEV               0                // 设备忽略时间（单位：ms）
#define TCFG_FIX_CLOCK_FREQ                       192000000        // 固定时钟频率
// ------------功能配置.json------------

// ------------升级配置.json------------
#define TCFG_UPDATE_ENABLE                        0                // 升级选择
#if TCFG_UPDATE_ENABLE
#define TCFG_UPDATE_STORAGE_DEV_EN                0                // 设备升级
#define TCFG_UPDATE_BLE_TEST_EN                   1                // ble蓝牙升级
#define TCFG_UPDATE_BT_LMP_EN                     1                // edr蓝牙升级
#define TCFG_TEST_BOX_ENABLE                      1                // 测试盒串口升级
#define TCFG_UPDATE_UART_IO_EN                    0                // 普通io串口升级
#define TCFG_UPDATE_UART_ROLE                     0                // 串口升级主从机选择
#define TCFG_DUAL_BANK_ENABLE                     0                // 支持双备份升级
#define TCFG_DUAL_BANK_UPDATE_NO_ERASE            0                // 允许边升级边播歌
#define TCFG_UPDATE_COMPRESS                      0                // 压缩双备份
#endif // TCFG_UPDATE_ENABLE
// ------------升级配置.json------------

// ------------音频配置.json------------
#define TCFG_AUDIO_DAC_CONNECT_MODE               DAC_OUTPUT_LR    // 声道配置
#define TCFG_AUDIO_DAC_MODE                       DAC_MODE_DIFF    // 输出方式
#define TCFG_AUDIO_DAC_LIGHT_CLOSE_ENABLE         0X0              // 轻量关闭
#define TCFG_AUDIO_DAC_BUFFER_TIME_MS             50               // DMA长度(ms)
#define TCFG_DAC_PERFORMANCE_MODE                 DAC_MODE_HIGH_PERFORMANCE // 性能模式
#define TCFG_AUDIO_DAC_PA_ISEL0                   5                // PA_ISEL0
#define TCFG_AUDIO_DAC_PA_ISEL1                   6                // PA_ISEL1
#define TCFG_AUDIO_VCM_CAP_EN                     0x1              // VCM电容
#define TCFG_DAC_POWER_MODE                       1                // 输出功率
#define TCFG_AUDIO_L_CHANNEL_GAIN                 0x03             // L Channel
#define TCFG_AUDIO_R_CHANNEL_GAIN                 0x03             // R Channel
#define TCFG_AUDIO_DIGITAL_GAIN                   0                // Digital Gain

#define TCFG_AUDIO_ADC_ENABLE                     1                // ADC配置
#if TCFG_AUDIO_ADC_ENABLE
#define TCFG_AUDIO_MIC_LDO_VSEL                   4                // MICLDO电压
#define TCFG_ADC_PERFORMANCE_MODE                 ADC_MODE_HIGH_PERFORMANCE // 性能模式
#define TCFG_CAPLESS_MIC_POWER_MODE               1                // 省电容MIC电压
#define TCFG_ADC_DIGITAL_GAIN                     0                // 数字增益
#define TCFG_ADC0_ENABLE                          1                // 使能
#define TCFG_ADC0_MODE                            0                // 模式
#define TCFG_ADC0_AIN_SEL                         2                // 输入端口
#define TCFG_ADC0_BIAS_SEL                        1                // 供电端口
#define TCFG_ADC0_BIAS_RSEL                       3                // MIC BIAS上拉电阻挡位
#define TCFG_ADC0_POWER_IO                        IO_PORTA_00      // IO供电选择
#define TCFG_ADC0_DCC_EN                          1                // DCC使能
#define TCFG_ADC0_DCC_LEVEL                       1                // DCC 截止频率
#define TCFG_ADC1_ENABLE                          0                // 使能
#define TCFG_ADC1_MODE                            0                // 模式
#define TCFG_ADC1_AIN_SEL                         1                // 输入端口
#define TCFG_ADC1_BIAS_SEL                        1                // 供电端口
#define TCFG_ADC1_BIAS_RSEL                       3                // MIC BIAS上拉电阻挡位
#define TCFG_ADC1_POWER_IO                        IO_PORTA_00      // IO供电选择
#define TCFG_ADC1_DCC_EN                          1                // DCC使能
#define TCFG_ADC1_DCC_LEVEL                       1                // DCC 截止频率
#endif // TCFG_AUDIO_ADC_ENABLE

#define CONFIG_ANC_ENABLE                         0                // ANC 配置
#if CONFIG_ANC_ENABLE
#define TCFG_AUDIO_ANC_TRAIN_MODE                 ANC_FF_EN        // ANC类型
#define TCFG_ANC_TOOL_DEBUG_ONLINE                0                // 蓝牙调试/产测
#define TCFG_ANC_BOX_ENABLE                       0                // 串口调试/产测
#define TCFG_AUDIO_ANC_MULT_ORDER_ENABLE          0                // 多场景滤波器
#define TCFG_AUDIO_ANC_ON_AUTO_SHUT_DOWN          0                // ANC_ON允许自动关机
#define TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE          0                // Debug调试
#define TCFG_ANC_MODE_ANC_EN                      0                // 降噪
#define TCFG_ANC_MODE_TRANS_EN                    0                // 通透
#define TCFG_ANC_MODE_OFF_EN                      0                // 关闭
#define TCFG_AUDIO_ANCL_FF_MIC                    A_MIC1           // FFMIC
#define TCFG_AUDIO_ANCR_FF_MIC                    0XFF             // 右声道FFMIC
#define TCFG_AUDIO_ANCL_FB_MIC                    0XFF             // FBMIC
#define TCFG_AUDIO_ANCR_FB_MIC                    0XFF             // 右声道FBMIC
#define TCFG_AUDIO_ANC_TALK_MIC                   A_MIC1           // TALK MIC
#define TCFG_ANCIF_FILE_LOAD_SEL                  3                // 配置文件选择
#define TCFG_ANCIF_FILE_SPACE                     4096             // 配置文件大小
#define TCFG_ANC_MUSIC_ANTI_CLIPPING_MODE         0                // 防破音模式
#define TCFG_ANC_MUSIC_DYNAMIC_GAIN_THR           -12              // ANC动态增益-触发阈值
#endif // CONFIG_ANC_ENABLE

#define TCFG_AUDIO_ANC_EXT_EN                     0                // ANC_EXT 扩展功能
#if TCFG_AUDIO_ANC_EXT_EN
#define TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE      0                // 风噪检测
#define TCFG_AUDIO_FIT_DET_ENABLE                 0                // 贴合度检测
#define TCFG_AUDIO_ANC_ENV_NOISE_DET_MODE         0                // 环境声检测模式
#define TCFG_AUDIO_ANC_ENV_ENABLE                 0                // 环境自适应
#define TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE  0                // 耳道自适应类型
#define TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN            0                // ANC耳道自适应
#define TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN            0                // ANC自适应CMP
#define TCFG_AUDIO_ADAPTIVE_EQ_ENABLE             0                // 自适应EQ
#endif // TCFG_AUDIO_ANC_EXT_EN

#define TCFG_AUDIO_GLOBAL_SAMPLE_RATE             96000            // 全局采样率
#define TCFG_AEC_TOOL_ONLINE_ENABLE               0                // 手机APP在线调试
#define TCFG_AUDIO_CVP_SYNC                       0                // 通话上行同步
#define TCFG_ESCO_DL_CVSD_SR_USE_16K              1                // ESCO通话全局采样率
#define TCFG_LEA_CALL_DL_GLOBAL_SR                3                // LE_Audio通话全局采样率
#define TCFG_AUDIO_SMS_SEL                        SMS_DEFAULT      // 1mic算法选择
#define TCFG_AUDIO_DMS_GLOBAL_VERSION             DMS_GLOBAL_V200  // 2micDNS算法选择
#define TCFG_3MIC_MODE_SEL                        JLSP_3MIC_MODE2  // 3mic算法选择
#define TCFG_MUSIC_PLC_TYPE                       0                // PLC类型选择
#define TCFG_ESCO_PLC_TYPE                        1                // PLC类型选择
#define TCFG_LC3_PLC_TYPE                         1                // LC3 格式PLC类型配置
#define TCFG_JLA_PLC_TYPE                         1                // JLA 格式PLC类型配置
#define TCFG_JLA_V2_PLC_TYPE                      1                // JLA_V2 格式PLC类型配置
#define TCFG_AUDIO_DUT_ENABLE                     0                // 音频/通话产测
#define TCFG_AUDIO_MIC_ARRAY_TRIM_ENABLE          0                // 麦克风阵列校准
#define TCFG_ANC_HAT_DUT                          0                // JL自研ANC产测
#define AUDIO_ENC_MPT_SELF_ENABLE                 0                // JL自研ENC产测
#define TCFG_APP_KEY_DUT_ENABLE                   0                // 按键产测
#define TCFG_KWS_VOICE_RECOGNITION_ENABLE         0                // 关键词检测KWS
#define TCFG_KWS_MIC_CHA_SELECT                   AUDIO_ADC_MIC_0  // 麦克风选择
#define TCFG_SMART_VOICE_ENABLE                   0                // 离线语音识别
#define TCFG_SMART_VOICE_USE_AEC                  0                // 回音消除使能
#define TCFG_SMART_VOICE_MIC_CH_SEL               AUDIO_ADC_MIC_0  // 麦克风选择
#define TCFG_AUDIO_KWS_LANGUAGE_SEL               KWS_CH           // 模式选择
#define TCFG_SPATIAL_EFFECT_VERSION               1                // 音效算法版本选择
#define TCFG_TWS_SPATIAL_AUDIO_AS_CHANNEL         0x0              // 传感器选择
#define TCFG_SPATIAL_EFFECT_ONLINE_ENABLE         0                // 空间音效在线调试
#define TCFG_DEC_WAV_ENABLE                       0                // WAV
#define TCFG_DEC_MP3_ENABLE                       0                // MP3
#define TCFG_DEC_FLAC_ENABLE                      0                // FLAC
#define TCFG_DEC_WMA_ENABLE                       0                // WMA
#define TCFG_DEC_APE_ENABLE                       0                // APE
#define TCFG_DEC_AAC_ENABLE                       0                // AAC
#define TCFG_DEC_SBC_ENABLE                       0                // SBC
#define TCFG_DEC_MSBC_ENABLE                      0                // MSBC
#define TCFG_DEC_OPUS_ENABLE                      0                // OPUS
#define TCFG_ENC_OPUS_TYPE                        2                // opus封装
#define TCFG_OPUS_FRAME_LEN                       40               // 帧长(byte)
#define TCFG_DEC_F2A_ENABLE                       0                // F2A
#define TCFG_DEC_WTG_ENABLE                       0                // WTG
#define TCFG_DEC_MTY_ENABLE                       0                // MTY
#define TCFG_DEC_WTS_ENABLE                       0                // WTS
#define TCFG_DEC_JLA_ENABLE                       0                // JLA
#define TCFG_ENC_OPUS_ENABLE                      0                // OPUS
#define TCFG_ENC_SBC_ENABLE                       0                // SBC
#define TCFG_ENC_MSBC_ENABLE                      0                // MSBC

#define TCFG_AUDIO_HEARING_AID_ENABLE             0                // 辅听配置
#if TCFG_AUDIO_HEARING_AID_ENABLE
#define AUDIO_MIC_EFFECT_SAMPLE_RATE              44100            // 辅听采样率
#define TCFG_AUDIO_DHA_AND_MUSIC_COEXIST          1                // 辅听与音乐播放共存
#define TCFG_AUDIO_DHA_AND_TONE_COEXIST           1                // 辅听与提示音共存
#define TCFG_AUDIO_DHA_AND_CALL_COEXIST           0                // 辅听与蓝牙通话共存
#endif // TCFG_AUDIO_HEARING_AID_ENABLE

#define TCFG_AUDIO_SIDETONE_ENABLE                0                // 通话监听配置
#if TCFG_AUDIO_SIDETONE_ENABLE
#define SIDETONE_ESCO_DAC_NAME                    "esco_dac_ch"    // 通话下行DAC节点名字
#define SIDETONE_SWITCH_NAME                      "sidetone_switch" // 监听Switch节点名字
#endif // TCFG_AUDIO_SIDETONE_ENABLE

#define TCFG_DATA_EXPORT_UART_TX_PORT             IO_PORT_DM       // 串口发送引脚
#define TCFG_DATA_EXPORT_UART_BAUDRATE            2000000          // 串口波特率
#define TCFG_AUDIO_SOMATOSENSORY_ENABLE           0                // 头部姿态检测
// ------------音频配置.json------------
#endif
