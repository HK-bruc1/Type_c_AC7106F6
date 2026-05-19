/*
 *******************************************************************
 *						Audio Common Definitions
 *
 *Note(s):
 *		 (1)Only macro definitions can be defined here.
 *		 (2)Use (1UL << (n)) instead of BIT(n)
 *******************************************************************
 */
#ifndef _AUDIO_DEF_H_
#define _AUDIO_DEF_H_

/*
 *******************************************************************
 *						Audio Common Definitions
 *******************************************************************
 */
//不同位宽数据类型对应的最大最小值定义
#define DATA_INT16_MAX					(32767)			//16bit正最大值
#define DATA_INT16_MIN					(-32768)		//16bit负最大值
#define DATA_INT24_MAX					(8388607)		//24bit正最大值
#define DATA_INT24_MIN					(-8388608)		//24bit负最大值
#define DATA_INT32_MAX					(2147483647)	//32bit正最大值
#define DATA_INT32_MIN					(-2147483648)	//32bit负最大值

//Audio I/O Mode
#define AUDIO_IO_SINGLE_ENDED			0	//单端:Single-Ended
#define AUDIO_IO_DIFFERENTIAL			1	//差分:Differential

//Volume Mode
#define  VOL_TYPE_DIGITAL				0	//软件数字音量
#define  VOL_TYPE_ANALOG				1	//硬件模拟音量
#define  VOL_TYPE_AD					2	//联合音量(模拟数字混合调节)
#define  VOL_TYPE_DIGITAL_HW			3  	//硬件数字音量

//数据位宽定义
#define  DATA_BIT_WIDE_16BIT  			0
#define  DATA_BIT_WIDE_24BIT  			1
#define  DATA_BIT_WIDE_32BIT  			2
#define  DATA_BIT_WIDE_32BIT_FLOAT  	3

/*
 *audio state define
 */
#define APP_AUDIO_STATE_IDLE        	0
#define APP_AUDIO_STATE_MUSIC       	1
#define APP_AUDIO_STATE_CALL        	2
#define APP_AUDIO_STATE_WTONE       	3
#define APP_AUDIO_STATE_KTONE       	4
#define APP_AUDIO_STATE_RING       		5
#define APP_AUDIO_CURRENT_STATE     	6
/*
 *******************************************************************
 *						DAC Definitions
 *******************************************************************
 */
#define DAC_DSM_6MHz                       (0)
#define DAC_DSM_12MHz                      (1)

#define DAC_OUTPUT_MONO_L                  ((1 << 4) | 0)   //左声道
#define DAC_OUTPUT_MONO_R                  ((1 << 4) | 1)   //右声道
#define DAC_OUTPUT_LR                      ((2 << 4) | 2)   //立体声
#define DAC_OUTPUT_MONO_LR_DIFF            ((1 << 4) | 3)   //左右差分输出

#define DAC_OUTPUT_DUAL_LR_DIFF            ((2 << 4) | 6)   //双声道差分
#define DAC_OUTPUT_FRONT_LR_REAR_L         ((3 << 4) | 7)   //三声道单端输出 前L+前R+后L (不可设置vcmo公共端)
#define DAC_OUTPUT_FRONT_LR_REAR_R         ((3 << 4) | 8)   //三声道单端输出 前L+前R+后R (可设置vcmo公共端);
#define DAC_OUTPUT_FRONT_LR_REAR_LR        ((4 << 4) | 9)   //四声道单端输出

#define DAC_BIT_WIDTH_16                   (0)   //16bit 位宽
#define DAC_BIT_WIDTH_24                   (1)   //24bit 位宽

#define DAC_CH_FL                          (1UL << 0)
#define DAC_CH_FR                          (1UL << 1)
#define DAC_CH_RL                          (1UL << 2)
#define DAC_CH_RR                          (1UL << 3)

//DAC通道序号
#define DA_LEFT        						0	//CH0:DAC Front Left Channel (FL)
#define DA_RIGHT       						1	//CH1:DAC Front Right Channel (FR)
#define DA_REAR_RIGHT  						2	//CH2:DAC Rear Left Channel (RL)
#define DA_REAR_LEFT   						3	//CH3:DAC Rear Right Channel (RR)

#define DAC_UNMUTE                         (0)
#define DAC_MUTE                           (1)

#define DAC_NG_THRESHOLD_CLEAR		  	   (1)	//BIT(0)：信号小于等于噪声门阈值，清0
#define DAC_NG_THRESHOLD_MUTE			   (5) 	//BIT(0)|BIT(2)：信号小于等于噪声门阈值，清0并mute
#define DAC_NG_SILENCE_MUTE				   (2)	//BIT(1)：信号静音(全0)时候mute
#define DAC_NG_POST_ENABLE				   (1UL << 15)	//BIT(15)：NoiseGate后处理使能

//DAC输出模式定义
#define DAC_MODE_SINGLE                    (0)	//单端
#define DAC_MODE_DIFF                      (1)	//差分
#define DAC_MODE_VCMO                      (2)	//共模VCOMO

//DAC性能模式定义
#define	DAC_MODE_HIGH_PERFORMANCE          (0)
#define	DAC_MODE_LOW_POWER		           (1)

//DAC开关状态定义
#define DAC_ANALOG_OPEN_PREPARE         	(1) //DAC打开前，即准备打开
#define DAC_ANALOG_OPEN_FINISH          	(2)	//DAC打开后，即打开完成
#define DAC_ANALOG_CLOSE_PREPARE        	(3) //DAC关闭前，即准备关闭
#define DAC_ANALOG_CLOSE_FINISH         	(4) //DAC关闭后，即关闭完成
/*
 *******************************************************************
 *						Class-D Driver Definitions
 *******************************************************************
 */

#define CLASSD_PA_MODE                     (1UL << 0)
#define CLASSD_APIO_MODE                   (1UL << 1)
#define CLASSD_GPIO_MODE				   (1UL << 2)

#define EPA_DSM_MODE_375K                  (0)
#define EPA_DSM_MODE_750K                  (1)
#define EPA_DSM_MODE_1500K                 (2)

#define EPA_PWM_MODE0                      (0)
#define EPA_PWM_MODE1                      (1)
#define EPA_PWM_MODE2                      (2)
/*
 *******************************************************************
 *						PDM Definitions
 *******************************************************************
 */
//PDM Version definitions
#define AUDIO_PDM_V1						(1UL << 0)//Version-1：独立PDM模块版本(700N/701N/703N)
#define AUDIO_PDM_V2						(1UL << 1)//Version-2：复用ADC模块版本(706N/708N/709N/710N)

#define AUDIO_PDM_MIC_0						(1UL << 0)
#define AUDIO_PDM_MIC_1                     (1UL << 1)
#define AUDIO_PDM_MIC_2                     (1UL << 2)
#define AUDIO_PDM_MIC_3                     (1UL << 3)

/*
 *******************************************************************
 *						Analog Aux Definitions
 *******************************************************************
 */
#define AUX_CH0                            (1UL << 0)
#define AUX_CH1                            (1UL << 1)

#define AUX_AIN_PORT0                      (1UL << 0)
#define AUX_AIN_PORT1                      (1UL << 1)
#define AUX_AIN_PORT2                      (1UL << 2)
#define AUX_AIN_PORT3                      (1UL << 3)
#define AUX_AIN_PORT4                      (1UL << 4)
/*
 *******************************************************************
 *						ADC Definitions
 *******************************************************************
 */
#define ADC_BIT_WIDTH_16                    (0)   //16bit 位宽
#define ADC_BIT_WIDTH_24                    (1)   //24bit 位宽

#define ADC_AIN_PORT0                       (1UL << 0)
#define ADC_AIN_PORT1                       (1UL << 1)
#define ADC_AIN_PORT2                       (1UL << 2)
#define ADC_AIN_PORT3                       (1UL << 3)
#define ADC_AIN_PORT4                       (1UL << 4)

#define MIC_LDO_STA_CLOSE                   (0)   //MICLDO电源关闭
#define MIC_LDO_STA_OPEN                    (1)   //MICLDO电源开启

/*省电容MIC版本定义*/
#define MIC_CAPLESS_VER0					(0)	//693N 695N 696N
#define MIC_CAPLESS_VER1					(1)	//697N 897N 698N
#define MIC_CAPLESS_VER2					(2)	//700N 701N
#define MIC_CAPLESS_VER3					(3)	//703N 706N AW32N

/*ADC性能模式*/
#define	ADC_MODE_HIGH_PERFORMANCE           (0) //高性能模式
#define	ADC_MODE_LOW_POWER		            (1)	//低功耗模式

/*MIC输入工作模式定义*/
#define AUDIO_MIC_CAP_MODE                  0   //单端隔直电容模式
#define AUDIO_MIC_CAP_DIFF_MODE             1   //差分隔直电容模式
#define AUDIO_MIC_CAPLESS_MODE              2   //单端省电容模式
/*
 *******************************************************************
 *						FFT Definitions
 *******************************************************************
 */
/*
 * 标准模式:支持2的指数次幂点数
 * Platforms：br23/br25/br30/br34/br36/br29
 */
#define FFT_V3				3

/*
 * 扩展模式 V1:FFT硬件模块支持非2的指数次幂点数.
 * Platforms：br27/br28/br50/br52
 */
#define FFT_EXT				4

/*
 * 扩展模式 V2:
 * Platforms：br56
 */
#define FFT_EXT_V2			5

/*
 *******************************************************************
 *						Codec Definitions
 *******************************************************************
 */

#define AUDIO_CODING_UNKNOW       0x00000000
#define AUDIO_CODING_MP3          0x00000001
#define AUDIO_CODING_WMA          0x00000002
#define AUDIO_CODING_WAV          0x00000004
#define AUDIO_CODING_JLA_LW       0x00000008
#define AUDIO_CODING_SBC          0x00000010
#define AUDIO_CODING_MSBC         0x00000020
#define AUDIO_CODING_G729         0x00000040
#define AUDIO_CODING_CVSD         0x00000080
#define AUDIO_CODING_PCM          0x00000100
#define AUDIO_CODING_AAC          0x00000200
#define AUDIO_CODING_MTY          0x00000400
#define AUDIO_CODING_FLAC         0x00000800
#define AUDIO_CODING_APE          0x00001000
#define AUDIO_CODING_M4A          0x00002000
#define AUDIO_CODING_AMR          0x00004000
#define AUDIO_CODING_DTS          0x00008000
#define AUDIO_CODING_APTX         0x00010000
#define AUDIO_CODING_LDAC         0x00020000
#define AUDIO_CODING_G726         0x00040000
#define AUDIO_CODING_MIDI         0x00080000
#define AUDIO_CODING_OPUS         0x00100000
#define AUDIO_CODING_SPEEX        0x00200000
#define AUDIO_CODING_LC3          0x00400000
#define AUDIO_CODING_JLA_LL       0x00800000
#define AUDIO_CODING_WTGV2        0x01000000
#define AUDIO_CODING_ALAC         0x02000000
#define AUDIO_CODING_SINE         0x04000000
#define AUDIO_CODING_F2A          0x08000000
#define AUDIO_CODING_JLA_V2       0x0A000000
#define AUDIO_CODING_AIFF         0x10000000
#define AUDIO_CODING_JLA          0x20000000
#define AUDIO_CODING_OGG          0x40000000
#define AUDIO_CODING_LHDC         0x80000000
#define AUDIO_CODING_LHDC_V5      0xA0000000
#define AUDIO_CODING_MIDI_CTRL    0xB0000000
#define AUDIO_CODING_ENGINE    	  0xC0000000
#define AUDIO_CODING_STREAM_MP3   0xE0000000
#define AUDIO_CODING_JL_VOCODER   0xF0000000

//#define AUDIO_CODING_STU_PICK     0x10000000
//#define AUDIO_CODING_STU_APP      0x20000000

//MSBC Dec Debug
#define CONFIG_MSBC_INPUT_FRAME_REPLACE_DISABLE		0
#define CONFIG_MSBC_INPUT_FRAME_REPLACE_SILENCE		1
#define CONFIG_MSBC_INPUT_FRAME_REPLACE_SINE		2

/*Opus Pack Header Defines*/
#define OPUS_PACK_HEADER_OGG		(1UL << (0))	//OGG封装
#define OPUS_PACK_HEADER_NULL		(1UL << (1))	//不带头
#define OPUS_PACK_HEADER_STD		(1UL << (2))	//标准头

/*
 *******************************************************************
 *						Linein(Aux) Definitions
 *******************************************************************
 */


/*
 *******************************************************************
 *						ANC Definitions
 *******************************************************************
 */
//ANC Mode Enable
#define ANC_FF_EN 		 			(1UL << (0))
#define ANC_FB_EN  					(1UL << (1))
#define ANC_HYBRID_EN  			 	(1UL << (2))

//ANC芯片版本定义
#define ANC_VERSION_BR30 			0x01	//AD697N/AC897N
#define ANC_VERSION_BR30C			0x02	//AC699N/AD698N
#define ANC_VERSION_BR36			0x03	//AC700N
#define ANC_VERSION_BR28			0x04	//JL701N/BR40
#define ANC_VERSION_BR28_MULT		0xA4	//JL701N 多滤波器
#define ANC_VERSION_BR50			0x05	//JL708N
#define ANC_VERSION_BR52			0x06	//JL709N
#define ANC_VERSION_BR56			0x07	//JL710N

/*
 *******************************************************************
 *						Smart Voice Definitions
 *******************************************************************
 */
/*离线语音识别语言选择*/
#define KWS_CH          1   /*中文*/
#define KWS_INDIA_EN    2   /*印度英语*/
#define KWS_FAR_CH      3   /*音箱中文*/

/*
 *******************************************************************
 *						Bluetooth Audio Definitions
 *******************************************************************
 */
/*TWS通话，从机是否出声*/
#define TWS_ESCO_MASTER_AND_SLAVE      1    //TWS通话主从机同时出声
#define TWS_ESCO_ONLY_MASTER           2    //TWS通话只有主机出声

/*
 *******************************************************************
 *						Effect Definitions
 *******************************************************************
 */
//算法输入输出位宽使能位定义
#define  EFx_BW_UNUSED 	                    (0)
#define  EFx_BW_16t16 	                    (1UL << (0))
#define  EFx_BW_16t32		                (1UL << (1))
#define  EFx_BW_32t16		                (1UL << (2))
#define  EFx_BW_32t32		                (1UL << (3))
#define  EFx_PRECISION_NOR                  (1UL << (4)) //precision(精度为高 普通 最低使能)
#define  EFx_PRECISION_PRO                  (1UL << (5)) //precision+(精度为最高使能)
#define  EFx_MODULE_MONO_EN                 (1UL << (6)) //支持单进单出
#define  EFx_MODULE_STEREO_EN               (1UL << (7)) //支持双进双出

//Limiter精度使能位定义
#define  LIMITER_PRECISION_HIGH_NORMAL_LOW  EFx_PRECISION_NOR//高、普通、最低
#define  LIMITER_PRECISION_MAX              EFx_PRECISION_PRO //最高

//PLC Mode定义
#define LC3_PLC_FADE_MODE					0	//淡入淡出模式
#define LC3_PLC_TD_MODE						1	//时域（TimeDomain）模式
#define LC3_PLC_FD_MODE						2	//频域（FrequencyDomain）模式
#define LC3_PLC_SILENCE_MODE				3	//填充静音包模式
#define LC3_PLC_ADVANCED_MODE				4	//官方高级PLC模式（需授权使用）
#define LC3_PLC_JL_ADVANCED_MODE			5	//JL自研高级PLC模式

#define JLA_PLC_FADE_MODE					0	//淡入淡出模式
#define JLA_PLC_TD_MODE						1	//时域（TimeDomain）模式
#define JLA_PLC_FD_MODE						2	//频域（FrequencyDomain）模式
#define JLA_PLC_SILENCE_MODE				3	//填充静音包模式

#define JLA_V2_PLC_FADE_MODE				0	//淡入淡出模式
#define JLA_V2_PLC_TD_MODE					1	//时域（TimeDomain）模式
#define JLA_V2_PLC_FD_MODE					2	//频域（FrequencyDomain）模式
#define JLA_V2_PLC_SILENCE_MODE				3	//填充静音包模式
#define JLA_V2_PLC_JL_ADVANCED_MODE		    4	//JL自研高级PLC模式


/*
 *******************************************************************
 *						MIDI Definitions
 *******************************************************************
 */
#define MIDI_CTRL_DEC_ENABLE          0
#define MIDI_FILE_DEC_ENABLE          0
#define CONFIG_MIDI_DEC_ADDR

#endif/*_AUDIO_DEF_H_*/

