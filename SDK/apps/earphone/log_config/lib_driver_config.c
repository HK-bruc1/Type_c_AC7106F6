/*********************************************************************************************
    *   Filename        : lib_driver_config.c

    *   Description     : Optimized Code & RAM (编译优化配置)

    *   Author          : Bingquan

    *   Email           : caibingquan@zh-jieli.com

    *   Last modifiled  : 2019-03-18 14:58

    *   Copyright:(c)JIELI  2011-2019  @ , All Rights Reserved.
*********************************************************************************************/
#include "app_config.h"
#include "system/includes.h"
#include "gpadc.h"

#if TCFG_SD0_SD1_USE_THE_SAME_HW
const int sd0_sd1_use_the_same_hw = 1;
#else
const int sd0_sd1_use_the_same_hw = 0;
#endif

#if TCFG_KEEP_CARD_AT_ACTIVE_STATUS
const int keep_card_at_active_status = 1;
#else
const int keep_card_at_active_status = 0;
#endif

#if TCFG_SDX_CAN_OPERATE_MMC_CARD
const int sdx_can_operate_mmc_card = TCFG_SDX_CAN_OPERATE_MMC_CARD;
#else
const int sdx_can_operate_mmc_card = 0;
#endif

#if TCFG_POWER_MODE_QUIET_ENABLE
const int config_dcdc_mode = 1;
#else
const int config_dcdc_mode = 0;
#endif

#if(TCFG_CLOCK_SYS_SRC == SYS_CLOCK_INPUT_PLL_RCL) //系统时钟源选择
const int  clock_sys_src_use_lrc_hw = 1; //当使用lrc时timer.c需要特殊设置
#else
const int  clock_sys_src_use_lrc_hw = 0;
#endif

#ifdef TCFG_VAD_LOWPOWER_CLOCK
const char config_vad_lowpower_clock = TCFG_VAD_LOWPOWER_CLOCK;
#else
const char config_vad_lowpower_clock = 0;
#endif

#if defined(CONFIG_CPU_BR28) || (TCFG_CHARGESTORE_PORT == IO_PORT_DP)
const int config_otg_slave_detect_method_2 = 1;
#else
const int config_otg_slave_detect_method_2 = 0;
#endif

//gpadc驱动开启 “电池电压”和“温度采集功能”
const u8 adc_vbat_ch_en = 1;
const u8 adc_vtemp_ch_en = 1;
const u32 lib_adc_clk_max = 500 * 1000;
const u8 gpadc_battery_mode = WEIGHTING_MODE; //使用IOVDD供电时,禁止使用 MEAN_FILTERING_MODE 模式

#if (defined(TCFG_POWER_SUPPLY_MODE) && (TCFG_POWER_SUPPLY_MODE == 0))

const u32 gpadc_ch_power = AD_CH_IOVDD;    //根据供电方式选择通道
const u8 gpadc_ch_power_div = 1;                //未定义默认配置1
const u8 gpadc_power_supply_mode = 0; //映射供电方式

#elif (defined(TCFG_POWER_SUPPLY_MODE) && (TCFG_POWER_SUPPLY_MODE == 1))

const u32 gpadc_ch_power = AD_CH_PMU_VPWR_4;    //根据供电方式选择通道
const u8 gpadc_ch_power_div = 4;                //未定义默认配置4
const u8 gpadc_power_supply_mode = 1; //映射供电方式

#else

#if (defined(TCFG_VBAT_CH) && (TCFG_VBAT_CH != ADC_CH_PMU_VBAT_4))

const u32 gpadc_ch_power = TCFG_VBAT_CH;    //根据供电方式选择通道

#else

#ifdef ADC_CH_PMU_VBAT_2
const u32 gpadc_ch_power = AD_CH_PMU_VBAT_2; //未定义默认VBAT通道
#else
const u32 gpadc_ch_power = AD_CH_PMU_VBAT_4; //未定义默认VBAT通道
#endif

#endif

#if (defined(TCFG_BAT_EXT_DIV) && defined(TCFG_VBAT_CH) && (TCFG_VBAT_CH != ADC_CH_PMU_VBAT_4))

const u8 gpadc_ch_power_div = TCFG_BAT_EXT_DIV; //分压系数,需和gpadc_ch_power匹配 例如:200/(200+600)
const u8 gpadc_power_supply_mode = 3; //映射供电方式

#else

#ifdef ADC_CH_PMU_VBAT_2
const u8 gpadc_ch_power_div = 2;                //未定义默认配置2
#else
const u8 gpadc_ch_power_div = 4;                //未定义默认配置4
#endif
const u8 gpadc_power_supply_mode = 2; //映射供电方式

#endif

#endif

const u16 gpadc_battery_trim_vddiom_voltage = 2800; //电池trim 使用的vddio电压
const u16 gpadc_battery_trim_voltage = 3700; //电池trim 使用的vbat电压

#ifdef TCFG_BAT_EX_ADJUST_ENABLE
const u16 gpadc_extern_voltage_trim = TCFG_BAT_EX_ADJUST_ENABLE ? 2500 : 0; //使用外部输入方式校准的电压
#else
const u16 gpadc_extern_voltage_trim = 0;
#endif

/* 是否开启把vm配置项暂存到ram的功能 */
/* 具体使用方法和功能特性参考《项目帮助文档》的“11.4. 配置项管理 -VM配置项暂存RAM功能描述” */
const char vm_ram_storage_enable = FALSE;

const char vm_ram_storage_in_irq_enable = TRUE;

/* 设置vm的ram内存缓存区总大小限制，0为不限制，非0为限制大小（单位/byte）。 */
const int  vm_ram_storage_limit_data_total_size  = 16 * 1024;

// 设置vm写是否进行组包:
// 0 :不组包
// -1:使用malloc的buf进行组包(vm_write时分配)
// 具体数值:使用固定的buf进行组包(vm_init时分配)
const int vm_packet_size = 256;

const int config_rtc_enable = 0;

//gptimer
const u8 lib_gptimer_src_lsb_clk = 0; //时钟源选择lsb_clk, 单位:MHz
const u8 lib_gptimer_src_std_clk = 12; //时钟源选择std_x_clk, 单位:MHz
const u8 lib_gptimer_timer_mode_en = 1; //gptimer timer功能使能
const u8 lib_gptimer_pwm_mode_en = 1; //gptimer pwm功能使能
const u8 lib_gptimer_capture_mode_en = 1; //gptimer capture功能使能
const u8 lib_gptimer_auto_tid_en = 1; //gptimer_tid 内部自动分配使能
const u8 lib_gptimer_extern_use = GPTIMER_EXTERN_USE; //gptimer 模块外部已经占用, bit0=1表示timer0 外部占用，以此类推

const u32 lib_config_uart_flow_enable = 1;

//秘钥鉴权相关
#define     AUTH_multi_algorithm (1<<0)   //多算法授权
#define     AUTH_mkey_check      (1<<1)   //二级秘钥
#define     AUTH_sdk_chip_key    (1<<2)   //SDK秘钥校验

//需要对应的功能，就或上对应的宏定义，支持多种鉴权同时打开
#if (defined(TCFG_BURNER_CURRENT_CALIBRATION) && TCFG_BURNER_CURRENT_CALIBRATION)
const u32 lib_config_enable_auth_check = 0b0000 | AUTH_multi_algorithm;
#else
const u32 lib_config_enable_auth_check = 0b0000;
#endif

