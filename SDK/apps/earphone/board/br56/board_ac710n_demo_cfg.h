#ifndef CONFIG_BOARD_AC710N_DEMO_CFG_H
#define CONFIG_BOARD_AC710N_DEMO_CFG_H

#include "board_ac710n_demo_global_build_cfg.h"

#ifdef CONFIG_BOARD_AC710N_DEMO

#define CONFIG_SDFILE_ENABLE
//*********************************************************************************//
//                                 配置开始                                        //
//*********************************************************************************//
#define ENABLE_THIS_MOUDLE					1
#define DISABLE_THIS_MOUDLE					0

#define ENABLE								1
#define DISABLE								0

#define NO_CONFIG_PORT						(-1)

//*********************************************************************************//
//                                 UART配置                                        //
//*********************************************************************************//
//#define TCFG_UART0_ENABLE					ENABLE_THIS_MOUDLE                     //串口打印模块使能
//#define TCFG_UART0_RX_PORT					NO_CONFIG_PORT                         //串口接收脚配置（用于打印可以选择NO_CONFIG_PORT）
//#define TCFG_UART0_TX_PORT  				IO_PORT_DP//IO_PORTA_05                             //串口发送脚配置
//#define TCFG_UART0_TX_PORT  				IO_PORTA_05                             //串口发送脚配置
//#define TCFG_UART0_BAUDRATE  				1000000                                //串口波特率配置
//*********************************************************************************//
//                                  NTC配置                                       //
//*********************************************************************************//
// #define NTC_DET_EN      0
// #define NTC_POWER_IO    IO_PORTC_03
// #define NTC_DETECT_IO   IO_PORTC_04
// #define NTC_DET_AD_CH   (0x4)   //根据adc_api.h修改通道号

// #define NTC_DET_UPPER        235  //正常范围AD值上限，0度时
// #define NTC_DET_LOWER        34  //正常范围AD值下限，45度时

//*********************************************************************************//
//                                 IIC配置                                        //
//*********************************************************************************//
/*软件IIC设置*/
#define TCFG_SW_I2C0_CLK_PORT               IO_PORTD_07                             //软件IIC  CLK脚选择
#define TCFG_SW_I2C0_DAT_PORT               IO_PORTD_08                             //软件IIC  DAT脚选择
#define TCFG_SW_I2C0_DELAY_CNT              50                                      //IIC延时参数，影响通讯时钟频率
/*硬件IIC设置*/
#define TCFG_HW_I2C0_CLK_PORT               IO_PORTC_04                             //硬件IIC  CLK脚选择
#define TCFG_HW_I2C0_DAT_PORT               IO_PORTC_05                             //硬件IIC  DAT脚选择
#define TCFG_HW_I2C0_CLK                    100000                                  //硬件IIC波特率

//*********************************************************************************//
//                                 key 配置                                        //
//*********************************************************************************//
#define KEY_NUM_MAX                        	10
#define KEY_NUM                            	3

#define MULT_KEY_ENABLE						DISABLE 		//是否使能组合按键消息, 使能后需要配置组合按键映射表

#define TCFG_KEY_TONE_EN					DISABLE 		// 按键提示音。

//*********************************************************************************//
//                                 iokey 配置                                      //
//*********************************************************************************//
//#define TCFG_IOKEY_ENABLE					ENABLE_THIS_MOUDLE //是否使能IO按键

//#define TCFG_IOKEY_POWER_CONNECT_WAY		ONE_PORT_TO_LOW    //按键一端接低电平一端接IO
//#define TCFG_IOKEY_POWER_ONE_PORT			IO_PORTB_01        //IO按键端口

//*********************************************************************************//
//                                 adkey 配置                                      //
//*********************************************************************************//
//#define TCFG_ADKEY_ENABLE                   DISABLE_THIS_MOUDLE//是否使能AD按键
//#define TCFG_ADKEY_PORT                     IO_PORT_DM         //AD按键端口(需要注意选择的IO口是否支持AD功能)

//#define TCFG_ADKEY_AD_CHANNEL               AD_CH_DM
//#define TCFG_ADKEY_EXTERN_UP_ENABLE         ENABLE_THIS_MOUDLE //是否使用外部上拉

//#if TCFG_ADKEY_EXTERN_UP_ENABLE
//#define R_UP    220                 //22K，外部上拉阻值在此自行设置
//#else
//#define R_UP    100                 //10K，内部上拉默认10K
//#endif

//必须从小到大填电阻，没有则同VDDIO,填0x3ffL
//#define TCFG_ADKEY_AD0      (0)                                 //0R
//#define TCFG_ADKEY_AD1      (0x3ffL * 30   / (30   + R_UP))     //3k
//#define TCFG_ADKEY_AD2      (0x3ffL * 62   / (62   + R_UP))     //6.2k
//#define TCFG_ADKEY_AD3      (0x3ffL * 91   / (91   + R_UP))     //9.1k
//#define TCFG_ADKEY_AD4      (0x3ffL * 150  / (150  + R_UP))     //15k
//#define TCFG_ADKEY_AD5      (0x3ffL * 240  / (240  + R_UP))     //24k
//#define TCFG_ADKEY_AD6      (0x3ffL * 330  / (330  + R_UP))     //33k
//#define TCFG_ADKEY_AD7      (0x3ffL * 510  / (510  + R_UP))     //51k
//#define TCFG_ADKEY_AD8      (0x3ffL * 1000 / (1000 + R_UP))     //100k
//#define TCFG_ADKEY_AD9      (0x3ffL * 2200 / (2200 + R_UP))     //220k
//#define TCFG_ADKEY_VDDIO    (0x3ffL)
//
//#define TCFG_ADKEY_VOLTAGE0 ((TCFG_ADKEY_AD0 + TCFG_ADKEY_AD1) / 2)
//#define TCFG_ADKEY_VOLTAGE1 ((TCFG_ADKEY_AD1 + TCFG_ADKEY_AD2) / 2)
//#define TCFG_ADKEY_VOLTAGE2 ((TCFG_ADKEY_AD2 + TCFG_ADKEY_AD3) / 2)
//#define TCFG_ADKEY_VOLTAGE3 ((TCFG_ADKEY_AD3 + TCFG_ADKEY_AD4) / 2)
//#define TCFG_ADKEY_VOLTAGE4 ((TCFG_ADKEY_AD4 + TCFG_ADKEY_AD5) / 2)
//#define TCFG_ADKEY_VOLTAGE5 ((TCFG_ADKEY_AD5 + TCFG_ADKEY_AD6) / 2)
//#define TCFG_ADKEY_VOLTAGE6 ((TCFG_ADKEY_AD6 + TCFG_ADKEY_AD7) / 2)
//#define TCFG_ADKEY_VOLTAGE7 ((TCFG_ADKEY_AD7 + TCFG_ADKEY_AD8) / 2)
//#define TCFG_ADKEY_VOLTAGE8 ((TCFG_ADKEY_AD8 + TCFG_ADKEY_AD9) / 2)
//#define TCFG_ADKEY_VOLTAGE9 ((TCFG_ADKEY_AD9 + TCFG_ADKEY_VDDIO) / 2)
//
//#define TCFG_ADKEY_VALUE0                   0
//#define TCFG_ADKEY_VALUE1                   1
//#define TCFG_ADKEY_VALUE2                   2
//#define TCFG_ADKEY_VALUE3                   3
//#define TCFG_ADKEY_VALUE4                   4
//#define TCFG_ADKEY_VALUE5                   5
//#define TCFG_ADKEY_VALUE6                   6
//#define TCFG_ADKEY_VALUE7                   7
//#define TCFG_ADKEY_VALUE8                   8
//#define TCFG_ADKEY_VALUE9                   9

//*********************************************************************************//
//                                 irkey 配置                                      //
//*********************************************************************************//
//#define TCFG_IRKEY_ENABLE                   DISABLE_THIS_MOUDLE//是否使能AD按键
//#define TCFG_IRKEY_PORT                     IO_PORTA_08        //IR按键端口

//*********************************************************************************//
//                             tocuh key 配置                                      //
//*********************************************************************************//
//*********************************************************************************//
//                             lp tocuh key 配置                                   //
//*********************************************************************************//
//#define TCFG_LP_TOUCH_KEY_ENABLE 			DISABLE_THIS_MOUDLE 		//是否使能触摸总开关

// #define TCFG_LP_TOUCH_KEY0_EN    			0                  		//是否使能触摸按键0 —— PB0
// #define TCFG_LP_TOUCH_KEY1_EN    			1                  		//是否使能触摸按键1 —— PB1
// #define TCFG_LP_TOUCH_KEY2_EN    			0                  		//是否使能触摸按键2 —— PB2
// #define TCFG_LP_TOUCH_KEY3_EN    			0                  		//是否使能触摸按键3 —— PB4
// #define TCFG_LP_TOUCH_KEY4_EN    			0                  		//是否使能触摸按键4 —— PB5
//
// #define TCFG_LP_TOUCH_KEY0_WAKEUP_EN        0                  		//是否使能触摸按键0可以软关机低功耗唤醒
// #define TCFG_LP_TOUCH_KEY1_WAKEUP_EN        1                  		//是否使能触摸按键1可以软关机低功耗唤醒
// #define TCFG_LP_TOUCH_KEY2_WAKEUP_EN        0                  		//是否使能触摸按键2可以软关机低功耗唤醒
// #define TCFG_LP_TOUCH_KEY3_WAKEUP_EN        0                  		//是否使能触摸按键3可以软关机低功耗唤醒
// #define TCFG_LP_TOUCH_KEY4_WAKEUP_EN        0                  		//是否使能触摸按键4可以软关机低功耗唤醒

//两个按键以上，可以做简单的滑动处理
// #define TCFG_LP_SLIDE_KEY_ENABLE            0                       //是否使能触摸按键的滑动功能
//
// #define TCFG_LP_EARTCH_KEY_ENABLE 			0	 	                //是否使能触摸按键用作入耳检测
// #define TCFG_LP_EARTCH_KEY_CH               2                       //带入耳检测流程的按键号：0/1/2/3/4
// #define TCFG_LP_EARTCH_KEY_REF_CH           1                       //入耳检测算法需要的另一个按键通道作为参考：0/1/2/3/4, 即入耳检测至少要使能两个触摸通道
// #define TCFG_LP_EARTCH_SOFT_INEAR_VAL       3000                    //入耳检测算法需要的入耳阈值，参考文档设置
// #define TCFG_LP_EARTCH_SOFT_OUTEAR_VAL      2000                    //入耳检测算法需要的出耳阈值，参考文档设置

//电容检测灵敏度级数配置(范围: 0 ~ 9)
//该参数配置与触摸时电容变化量有关, 触摸时电容变化量跟模具厚度, 触摸片材质, 面积等有关,
//触摸时电容变化量越小, 推荐选择灵敏度级数越大,
//触摸时电容变化量越大, 推荐选择灵敏度级数越小,
//用户可以从灵敏度级数为0开始调试, 级数逐渐增大, 直到选择一个合适的灵敏度配置值.
// #define TCFG_LP_TOUCH_KEY0_SENSITIVITY		5 	//触摸按键0电容检测灵敏度配置(级数0 ~ 9)
// #define TCFG_LP_TOUCH_KEY1_SENSITIVITY		5 	//触摸按键1电容检测灵敏度配置(级数0 ~ 9)
// #define TCFG_LP_TOUCH_KEY2_SENSITIVITY		5 	//触摸按键2电容检测灵敏度配置(级数0 ~ 9)
// #define TCFG_LP_TOUCH_KEY3_SENSITIVITY		5 	//触摸按键3电容检测灵敏度配置(级数0 ~ 9)
// #define TCFG_LP_TOUCH_KEY4_SENSITIVITY		5 	//触摸按键4电容检测灵敏度配置(级数0 ~ 9)

//内置触摸灵敏度调试工具使能, 使能后可以通过连接PC端上位机通过SPP调试,
//打开该宏需要确保同时打开宏:
//1.TCFG_BT_SUPPORT_SPP
//2.APP_ONLINE_DEBUG
//可以针对每款样机校准灵敏度参数表(在lp_touch_key.c ch_sensitivity_table), 详细使用方法请参考《低功耗内置触摸介绍》文档.
// #define TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE 	DISABLE


#if TCFG_LP_TOUCH_KEY_ENABLE
//如果 IO按键与触摸按键有冲突，则需要关掉 IO按键的 宏
#if 1
//取消外置触摸的一些宏定义
#ifdef TCFG_IOKEY_ENABLE
#undef TCFG_IOKEY_ENABLE
#define TCFG_IOKEY_ENABLE 					DISABLE_THIS_MOUDLE
#endif /* #ifdef TCFG_IOKEY_ENABLE */
#endif

//如果 AD按键与触摸按键有冲突，则需要关掉 AD按键的 宏
#if 1
//取消外置触摸的一些宏定义
#ifdef TCFG_ADKEY_ENABLE
#undef TCFG_ADKEY_ENABLE
#define TCFG_ADKEY_ENABLE 					DISABLE_THIS_MOUDLE
#endif /* #ifdef TCFG_ADKEY_ENABLE */
#endif

#if TCFG_LP_EARTCH_KEY_ENABLE
#ifdef TCFG_EARTCH_EVENT_HANDLE_ENABLE
#undef TCFG_EARTCH_EVENT_HANDLE_ENABLE
#endif
#define TCFG_EARTCH_EVENT_HANDLE_ENABLE 	ENABLE_THIS_MOUDLE 		//入耳检测事件APP流程处理使能
#endif /* #if TCFG_LP_EARTCH_KEY_ENABLE */


#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */


//*********************************************************************************//
//                             plcnt tocuh key 配置                                   //
//*********************************************************************************//
#define TCFG_TOUCH_KEY_ENABLE               DISABLE_THIS_MOUDLE             //是否使能plcnt触摸按键
//key0配置
#define TCFG_TOUCH_KEY0_PRESS_DELTA	   	    100//变化阈值，当触摸产生的变化量达到该阈值，则判断被按下，每个按键可能不一样，可先在驱动里加到打印，再反估阈值
#define TCFG_TOUCH_KEY0_PORT 				IO_PORTB_06 //触摸按键key0 IO配置
#define TCFG_TOUCH_KEY0_VALUE 				0x12 		//触摸按键key0 按键值
//key1配置
#define TCFG_TOUCH_KEY1_PRESS_DELTA	   	    100//变化阈值，当触摸产生的变化量达到该阈值，则判断被按下，每个按键可能不一样，可先在驱动里加到打印，再反估阈值
#define TCFG_TOUCH_KEY1_PORT 				IO_PORTB_07 //触摸按键key1 IO配置
#define TCFG_TOUCH_KEY1_VALUE 				0x34        //触摸按键key1 按键值

//*********************************************************************************//
//                                 rdec_key 配置                                      //
//*********************************************************************************//
#define TCFG_RDEC_KEY_ENABLE					DISABLE_THIS_MOUDLE //是否使能RDEC按键
//RDEC0配置
#define TCFG_RDEC0_ECODE1_PORT					IO_PORTA_03
#define TCFG_RDEC0_ECODE2_PORT					IO_PORTA_04
#define TCFG_RDEC0_KEY0_VALUE 				 	0
#define TCFG_RDEC0_KEY1_VALUE 				 	1

//RDEC1配置
#define TCFG_RDEC1_ECODE1_PORT					IO_PORTA_05
#define TCFG_RDEC1_ECODE2_PORT					IO_PORTA_06
#define TCFG_RDEC1_KEY0_VALUE 				 	2
#define TCFG_RDEC1_KEY1_VALUE 				 	3

//RDEC2配置
#define TCFG_RDEC2_ECODE1_PORT					IO_PORTB_00
#define TCFG_RDEC2_ECODE2_PORT					IO_PORTB_01
#define TCFG_RDEC2_KEY0_VALUE 				 	4
#define TCFG_RDEC2_KEY1_VALUE 				 	5

//*********************************************************************************//
//                                 Audio配置                                       //
//*********************************************************************************//
//#define TCFG_AUDIO_ADC_ENABLE				ENABLE_THIS_MOUDLE
//#define TCFG_AUDIO_ADC_LINE_CHA				LADC_LINE0_MASK

//#define TCFG_AUDIO_DAC_ENABLE				ENABLE_THIS_MOUDLE
//#define TCFG_AUDIO_DAC_LDO_VOLT				DACVDD_LDO_1_80V
/*DAC硬件上的连接方式,可选的配置：
    DAC_OUTPUT_MONO_L               单左声道差分
    DAC_OUTPUT_MONO_R               单右声道差分
    DAC_OUTPUT_LR                   双声道差分
*/
//#define TCFG_AUDIO_DAC_CONNECT_MODE         DAC_OUTPUT_MONO_L
// #define TCFG_AUDIO_DAC_CONNECT_MODE         DAC_OUTPUT_MONO_R
// #define TCFG_AUDIO_DAC_CONNECT_MODE         DAC_OUTPUT_LR

/*PDM数字MIC配置*/
// #define TCFG_AUDIO_PLNK_SCLK_PIN			IO_PORTA_07
// #define TCFG_AUDIO_PLNK_DAT0_PIN			IO_PORTA_08
// #define TCFG_AUDIO_PLNK_DAT1_PIN			NO_CONFIG_PORT

/**************
 *ANC配置
 *************/
#define TCFG_AUDIO_ANC_ENABLE				CONFIG_ANC_ENABLE		//ANC总使能,根据global_bulid_cfg板级定义
// #define TCFG_ANC_TOOL_DEBUG_ONLINE 			DISABLE_THIS_MOUDLE		//ANC工具蓝牙spp调试
// #define TCFG_ANC_EXPORT_RAM_EN				DISABLE_THIS_MOUDLE		//ANCdebug数据释放RAM使能
#if TCFG_ANC_EXPORT_RAM_EN
#define TCFG_AUDIO_CVP_CODE_AT_RAM			DISABLE_THIS_MOUDLE
#define TCFG_AUDIO_AAC_CODE_AT_RAM			DISABLE_THIS_MOUDLE
#endif/*TCFG_ANC_EXPORT_RAM_EN*/
/*
 *当通话与ANC共用FF MIC，并且通话要求更大的MIC模拟增益时，使能TCFG_AUDIO_DYNAMIC_ADC_GAIN，
 *则通话MIC模拟增益以配置工具通话参数为准,通话时，ANC使用通话MIC模拟增益
 */
//#define TCFG_AUDIO_DYNAMIC_ADC_GAIN			ENABLE_THIS_MOUDLE

// #define ANC_TRAIN_MODE						ANC_FF_EN				//ANC类型配置：单前馈，单后馈，混合馈
// #define ANC_CH 			 					ANC_L_CH //| ANC_R_CH	//ANC通道使能：左通道 | 右通道
// #define ANCL_FF_MIC							A_MIC0					//ANC左通道FFMIC类型
// #define ANCL_FB_MIC							A_MIC1					//ANC左通道FBMIC类型
// #define ANCR_FF_MIC							MIC_NULL				//ANC右通道FFMIC类型
// #define ANCR_FB_MIC							MIC_NULL                //ANC右通道FBMIC类型
// ANC最大滤波器阶数限制，若设置超过10，ANC配置区则占用8K(range: 10-20)
// #define ANC_MAX_ORDER						10


/*限幅器-噪声门限*/
#define TCFG_AUDIO_NOISE_GATE				DISABLE_THIS_MOUDLE

/*通话下行降噪使能*/
// #define TCFG_ESCO_DL_NS_ENABLE				DISABLE_THIS_MOUDLE

// AUTOMUTE
// #define AUDIO_OUTPUT_AUTOMUTE   ENABLE_THIS_MOUDLE

/*
 *系统音量类型选择
 *软件数字音量是指纯软件对声音进行运算后得到的
 *硬件数字音量是指dac内部数字模块对声音进行运算后输出
 */
#define VOL_TYPE_DIGITAL		0	//软件数字音量(调节解码输出数据的音量)
#define VOL_TYPE_ANALOG			1	//(暂未支持)硬件模拟音量
#define VOL_TYPE_AD				2	//(暂未支持)联合音量(模拟数字混合调节)
#define VOL_TYPE_DIGITAL_HW		3  	//硬件数字音量(调节DAC模块的硬件音量)
/*注意:ANC使能情况下使用软件数字音量*/
#if TCFG_AUDIO_ANC_ENABLE
//#define SYS_VOL_TYPE            VOL_TYPE_DIGITAL
#else
//#define SYS_VOL_TYPE            VOL_TYPE_DIGITAL_HW
#endif/*TCFG_AUDIO_ANC_ENABLE*/

//第三方清晰语音开发使能
// #define TCFG_CVP_DEVELOP_ENABLE             DISABLE_THIS_MOUDLE

/*通话降噪模式配置*/
// #define CVP_ANS_MODE	0	[>传统降噪<]
// #define CVP_DNS_MODE	1	[>神经网络降噪<]
// #define TCFG_AUDIO_CVP_NS_MODE				CVP_ANS_MODE

/*
 * ENC(双mic降噪)配置
 * 双mic降噪包括DMS_NORMAL和DMS_FLEXIBLE，在使能TCFG_AUDIO_DUAL_MIC_ENABLE
 * 的前提下，根据具体需求，选择对应的DMS模式
 */
/*ENC(双mic降噪)使能*/
// #define TCFG_AUDIO_DUAL_MIC_ENABLE			DISABLE_THIS_MOUDLE

/*DMS模式选择*/
// #define DMS_NORMAL		1	//普通双mic降噪(mic距离固定)
// #define DMS_FLEXIBLE	2	//适配mic距离不固定且距离比较远的情况，比如头戴式话务耳机
// #define TCFG_AUDIO_DMS_SEL					DMS_NORMAL

/*ENC双mic配置主mic副mic对应的mic port*/
// #define DMS_MASTER_MIC0		0 //mic0是主mic
// #define DMS_MASTER_MIC1		1 //mic1是主mic
// #define TCFG_AUDIO_DMS_MIC_MANAGE			DMS_MASTER_MIC0
/*双mic降噪/单麦mic降噪 DUT测试模式，配合设备测试mic频响和(双mic)降噪量*/

//MIC通道配置
// #if TCFG_AUDIO_DUAL_MIC_ENABLE
//#define TCFG_AUDIO_ADC_MIC_CHA				(AUDIO_ADC_MIC_0 | AUDIO_ADC_MIC_1)
// #else
//#define TCFG_AUDIO_ADC_MIC_CHA				AUDIO_ADC_MIC_0
// #endif[>TCFG_AUDIO_DUAL_MIC_ENABLE<]

/*MIC模式配置:单端隔直电容模式/差分隔直电容模式/单端省电容模式*/
// #if TCFG_AUDIO_ANC_ENABLE
/*注意:ANC使能情况下，使用差分mic*/
// #define TCFG_AUDIO_MIC_MODE					AUDIO_MIC_CAP_DIFF_MODE
// #define TCFG_AUDIO_MIC1_MODE				AUDIO_MIC_CAP_DIFF_MODE
// #else
// #define TCFG_AUDIO_MIC_MODE					AUDIO_MIC_CAP_MODE
// #define TCFG_AUDIO_MIC1_MODE				AUDIO_MIC_CAP_MODE
// #endif[>TCFG_AUDIO_ANC_ENABLE<]

/*
 *>>MIC电源管理:根据具体方案，选择对应的mic供电方式
 *(1)如果是多种方式混合，则将对应的供电方式或起来即可，比如(MIC_PWR_FROM_GPIO | MIC_PWR_FROM_MIC_BIAS)
 *(2)如果使用固定电源供电(比如dacvdd)，则配置成DISABLE_THIS_MOUDLE
 */
#define MIC_PWR_FROM_GPIO		(1UL << 0)	//使用普通IO输出供电
#define MIC_PWR_FROM_MIC_BIAS	(1UL << 1)	//使用内部mic_ldo供电(有上拉电阻可配)
#define MIC_PWR_FROM_MIC_LDO	(1UL << 2)	//使用内部mic_ldo供电
//配置MIC电源
// #define TCFG_AUDIO_MIC_PWR_CTL				MIC_PWR_FROM_MIC_BIAS

#if 0
//使用普通IO输出供电:不用的port配置成NO_CONFIG_PORT
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_GPIO)
//#define TCFG_AUDIO_MIC_PWR_PORT				IO_PORTC_02
//#define TCFG_AUDIO_MIC1_PWR_PORT			NO_CONFIG_PORT
#define TCFG_AUDIO_MIC2_PWR_PORT			NO_CONFIG_PORT
#endif/*MIC_PWR_FROM_GPIO*/

//使用内部mic_ldo供电(有上拉电阻可配)
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_MIC_BIAS)
#define TCFG_AUDIO_MIC0_BIAS_EN				ENABLE_THIS_MOUDLE/*Port:PA2*/
#define TCFG_AUDIO_MIC1_BIAS_EN				ENABLE_THIS_MOUDLE/*Port:PB7*/
#endif/*MIC_PWR_FROM_MIC_BIAS*/

//使用内部mic_ldo供电(Port:PA0)
#if (TCFG_AUDIO_MIC_PWR_CTL & MIC_PWR_FROM_MIC_LDO)
#define TCFG_AUDIO_MIC_LDO_EN				ENABLE_THIS_MOUDLE
#else
#define TCFG_AUDIO_MIC_LDO_EN				0
#endif/*MIC_PWR_FROM_MIC_LDO*/
#endif
/*>>MIC电源管理配置结束*/

/*通话参数在线调试*/
// #define TCFG_AEC_TOOL_ONLINE_ENABLE         DISABLE_THIS_MOUDLE


/*提示音叠加配置*/
#define TCFG_WAV_TONE_MIX_ENABLE			DISABLE
#define TCFG_MP3_TONE_MIX_ENABLE			DISABLE
#define TCFG_WTG_TONE_MIX_ENABLE			DISABLE
#define TCFG_WTS_TONE_MIX_ENABLE			ENABLE




//*********************************************************************************//
//                         Spatial Audio Effect 空间音效配置                       //
//*********************************************************************************//
// #define TCFG_AUDIO_SPATIAL_EFFECT_ENABLE           	DISABLE_THIS_MOUDLE
// #define TCFG_TWS_SPATIAL_AUDIO_AS_CHANNEL   		'L'

/*独立任务里面跑空间音效*/
#define TCFG_AUDIO_EFFECT_TASK_EBABLE               ENABLE_THIS_MOUDLE

/*空间音效在线调试*/
// #define TCFG_SPATIAL_EFFECT_ONLINE_ENABLE           DISABLE_THIS_MOUDLE

/*空间音频传感器是否在解码任务读传感器数据*/
#define TCFG_SENSOR_DATA_READ_IN_DEC_TASK           DISABLE_THIS_MOUDLE

/*传感器数据读取的频率间隔，单位ms*/
#define TCFG_SENSOR_DATA_READ_INTERVAL              20

/*空间音频独立EQ使能*/
#define TCFG_SPATIAL_EFFECT_EQ_ENABLE               DISABLE_THIS_MOUDLE

//*********************************************************************************//
//                                  IIS 配置                                     //
//*********************************************************************************//
#define TCFG_IIS_ENABLE                     DISABLE_THIS_MOUDLE

#define TCFG_IIS_MODE                       (0)     // 0:master  1:slave

#define TCFG_AUDIO_INPUT_IIS                (ENABLE && TCFG_IIS_ENABLE)
#define TCFG_IIS_INPUT_DATAPORT_SEL         ALINK_CH1

#define TCFG_AUDIO_OUTPUT_IIS               (DISABLE && TCFG_IIS_ENABLE)
#define TCFG_IIS_OUTPUT_DATAPORT_SEL        ALINK_CH0

#define TCFG_IIS_SAMPLE_RATE                (16000L)

//*********************************************************************************//
//                                  充电舱配置                                     //
//   充电舱/蓝牙测试盒/ANC测试三者为同级关系,开启任一功能都会初始化PP0通信接口     //
//*********************************************************************************//
//#define TCFG_CHARGESTORE_ENABLE				DISABLE_THIS_MOUDLE         //是否支持智能充电舱
// #define TCFG_TEST_BOX_ENABLE			    ENABLE_THIS_MOUDLE          //是否支持蓝牙测试盒
// #if TCFG_AUDIO_ANC_ENABLE
// #define TCFG_ANC_BOX_ENABLE			        ENABLE_THIS_MOUDLE          //是否支持ANC测试盒
// #else
// #define TCFG_ANC_BOX_ENABLE			        DISABLE_THIS_MOUDLE         //是否支持ANC测试盒
// #endif[>TCFG_AUDIO_ANC_ENABLE<]
//#define TCFG_CHARGESTORE_PORT				IO_PORTP_00                 //耳机通讯的IO口

//*********************************************************************************//
//                                  充电参数配置                                   //
//*********************************************************************************//
//是否支持芯片内置充电
//#define TCFG_CHARGE_ENABLE					ENABLE_THIS_MOUDLE
//是否支持开机充电
//#define TCFG_CHARGE_POWERON_ENABLE			DISABLE
//是否支持拔出充电自动开机功能
//#define TCFG_CHARGE_OFF_POWERON_EN			ENABLE
/*充电截止电压可选配置*/
//#define TCFG_CHARGE_FULL_V					CHARGE_FULL_V_4199
/*充电截止电流可选配置*/
//#define TCFG_CHARGE_FULL_MA					CHARGE_FULL_mA_10
/*恒流充电电流可选配置*/
//#define TCFG_CHARGE_MA						CHARGE_mA_50
/*涓流充电电流配置*/
//#define TCFG_CHARGE_TRICKLE_MA              CHARGE_mA_10
//是否支持满电电压为参考电压,重新划分电量等级,优化电池电量检测。
#define TCFG_REFERENCE_V_ENABLE                (DISABLE && TCFG_CHARGE_ENABLE)
//产线电量管控使能
#define TCFG_BATTERY_PRODUCT_MANAGE_ENABLE     (DISABLE && TCFG_CHARGE_ENABLE)
//*********************************************************************************//
//                                  LED 配置                                       //
//*********************************************************************************//
//#define TCFG_PWMLED_ENABLE					ENABLE_THIS_MOUDLE			//是否支持PMW LED推灯模块
#define TCFG_PWMLED_IOMODE					LED_ONE_IO_MODE				//LED模式，单IO还是两个IO推灯
#define TCFG_PWMLED_PIN						IO_PORTA_08					//LED使用的IO口
//*********************************************************************************//
//                                  时钟配置                                       //
//*********************************************************************************//
#define TCFG_CLOCK_SYS_SRC					SYS_CLOCK_INPUT_PLL_BT_OSC   //系统时钟源选择
//#define TCFG_CLOCK_SYS_HZ					12000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					16000000                     //系统时钟设置
#define TCFG_CLOCK_SYS_HZ					24000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					32000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					48000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					54000000                     //系统时钟设置
//#define TCFG_CLOCK_SYS_HZ					64000000                     //系统时钟设置
//#define TCFG_CLOCK_OSC_HZ					24000000                     //外界晶振频率设置
#define TCFG_CLOCK_MODE                     CLOCK_MODE_ADAPTIVE //CLOCK_MODE_USR

//*********************************************************************************//
//                                  低功耗配置                                     //
//*********************************************************************************//
//#define TCFG_LOWPOWER_POWER_SEL				PWR_DCDC15//PWR_LDO15                    //电源模式设置，可选DCDC和LDO
#define TCFG_LOWPOWER_BTOSC_DISABLE			0                            //低功耗模式下BTOSC是否保持
//#define TCFG_LOWPOWER_LOWPOWER_SEL			1   //芯片是否进入powerdown
/*强VDDIO等级配置,可选：
    VDDIOM_VOL_20V    VDDIOM_VOL_22V    VDDIOM_VOL_24V    VDDIOM_VOL_26V
    VDDIOM_VOL_30V    VDDIOM_VOL_30V    VDDIOM_VOL_32V    VDDIOM_VOL_36V*/
//#define TCFG_LOWPOWER_VDDIOM_LEVEL			VDDIOM_VOL_28V
/*弱VDDIO等级配置，可选：
    VDDIOW_VOL_21V    VDDIOW_VOL_24V    VDDIOW_VOL_28V    VDDIOW_VOL_32V*/
//#define TCFG_LOWPOWER_VDDIOW_LEVEL			VDDIOW_VOL_26V               //弱VDDIO等级配置
//#define TCFG_LOWPOWER_OSC_TYPE              OSC_TYPE_LRC
#define TCFG_LOWPOWER_LIGHT_SLEEP_ATTRIBUTE 	LOWPOWER_LIGHT_SLEEP_ATTRIBUTE_KEEP_CLOCK 		//低功耗LIGHT模式属性, 可以选择是否保持住一些电源和时钟

//*********************************************************************************//
//                                  g-sensor配置                                   //
//*********************************************************************************//
#define TCFG_GSENSOR_ENABLE                       0     //gSensor使能
#define TCFG_DA230_EN                             0
#define TCFG_SC7A20_EN                            0
#define TCFG_STK832x_EN                           0
#define TCFG_BMA580_EN                            0
#define TCFG_IRSENSOR_ENABLE                      0
#define TCFG_JSA1221_ENABLE                       0
#define TCFG_GSENOR_USER_IIC_TYPE                 0     //0:软件IIC  1:硬件IIC

//*********************************************************************************//
//                                  hr-sensor配置                                   //
//*********************************************************************************//
#define TCFG_HRSENSOR_ENABLE                      0     //hr-sensor使能
#define TCFG_HX3918_ENABLE                        0
#define TCFG_HX3011_ENABLE                        0
#define TCFG_HRSENOR_USER_IIC_TYPE                0     //0:软件IIC  1:硬件IIC

//*********************************************************************************//
//                                  系统配置                                         //
//*********************************************************************************//
//#define TCFG_AUTO_SHUT_DOWN_TIME		          0     //没有蓝牙连接自动关机时间
//#define TCFG_SYS_LVD_EN						      1     //电量检测使能
#define TCFG_POWER_ON_NEED_KEY				      0	    //是否需要按按键开机配置

//*********************************************************************************//
//                                  蓝牙配置                                       //
//*********************************************************************************//



//*********************************************************************************//
//                                 配置结束                                        //
//*********************************************************************************//



#endif //CONFIG_BOARD_AC710N_DEMO
#endif //CONFIG_BOARD_AC710N_DEMO_CFG_H
