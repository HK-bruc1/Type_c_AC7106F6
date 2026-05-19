#ifndef _AUDIO_CONFIG_DEF_H_
#define _AUDIO_CONFIG_DEF_H_
/*
 *******************************************************************
 *						Audio CPU Definitions
 *
 *Note(s):
 *		 Only macro definitions can be defined here.
 *******************************************************************
 */

#include "app_config.h"
#include "audio_def.h"
#include "audio_platform_config.h"

//**************************************
// 			ASS通用配置
//**************************************
#define MEDIA_24BIT_ENABLE					TCFG_AUDIO_BIT_WIDTH
#define AUD_DAC_TRIM_ENABLE					1
#define AUD_DAC_TRIM_FADE_ENABLE            1 //A/B版芯片建议开启，C版及以后芯片可关闭，节省代码空间
#define TCFG_AUDIO_DAC_NOISEGATE_ENABLE     1
#define AUDIO_DAC_MAX_SAMPLE_RATE           48000
#ifdef CONFIG_EARPHONE_CASE_ENABLE
#define TCFG_AUDIO_DAC_CLASSH_EN            1
#else
#define TCFG_AUDIO_DAC_CLASSH_EN            0
#endif
/*
 * Hi-Res Audio使能LHDC/LDAC要求：
 * <1>DAC最高采样率调整到96K
 * <2>同时全局采样率至少要96kHz，或者Disable（即自适应）
 */
#ifdef TCFG_HI_RES_AUDIO_ENEBALE
#undef AUDIO_DAC_MAX_SAMPLE_RATE
#define AUDIO_DAC_MAX_SAMPLE_RATE           96000
#if (TCFG_AUDIO_GLOBAL_SAMPLE_RATE && (TCFG_AUDIO_GLOBAL_SAMPLE_RATE < 96000))
#error "Hi-Res Audio：请将全局采样率TCFG_AUDIO_GLOBAL_SAMPLE_RATE设置到至少96000，或Disable！"
#endif
#endif

#if (TCFG_AUDIO_GLOBAL_SAMPLE_RATE && (TCFG_AUDIO_GLOBAL_SAMPLE_RATE > AUDIO_DAC_MAX_SAMPLE_RATE))
#undef AUDIO_DAC_MAX_SAMPLE_RATE
#define AUDIO_DAC_MAX_SAMPLE_RATE           TCFG_AUDIO_GLOBAL_SAMPLE_RATE
#endif

#define TCFG_AUDIO_IIS_CLOCK_CLOSE          0

//**************************************
// 		    场景参数更新使能
//**************************************
#define TCFG_SCENE_UPDATE_ENABLE            0

//**************************************
// 			音频模块链接配置
//**************************************
#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)) || (defined TCFG_TYPEC_EARPHONE_CASE && (TCFG_TYPEC_EARPHONE_CASE == 1)))
/*音频模块代码全部不放RAM*/
#define AFx_VBASS_AT_RAM				    0	//虚拟低音
#define AFx_VBASS_PRO_AT_RAM			    0	//虚拟低音pro
#define AFx_REVERB_AT_RAM				    0	//混响
#define AFx_ECHO_AT_RAM				        0	//回声
#define AFx_VOICECHANGER_AT_RAM			    0	//变声
#define AFx_DRC_AT_RAM 					    0	//DRC
#define AFx_HARMONIC_EXCITER_AT_RAM 	    0	//谐波激励
#define AFx_DYN_EQ_AT_RAM 				    0	//动态EQ
#define AFx_DYN_EQ_PRO_AT_RAM 				0	//动态EQ Pro
#define AFx_NOTCH_HOWLING_AT_RAM 		    0	//啸叫抑制：陷波
#define AFx_FREQ_SHIFT_AT_RAM	 		    0	//啸叫抑制：移频
#define AFx_NOISEGATE_AT_RAM	 		    0	//噪声门
#define AFx_ADVAUDIO_PLC_AT_RAM	    	    0
#define AFX_AUDIO_LINK_AT_RAM               0   //iis驱动
#define AFX_AUDIO_SYNC_AT_RAM               0   //sync
#define AFx_EQ_AT_RAM                       0	//eq
#define AFx_VOCAL_TRACK_AT_RAM              0   //声道组合与声道拆分
#define AFX_AUDIO_DIGITAL_VOL_AT_RAM        0   //数字音量
#define AFX_AUDIO_ENERGY_DET_AT_RAM         0   //能量检测
#define AFX_LIMITER_AT_RAM                  0   //限幅器
#define AFX_MULTIBAND_CROSSOVER_AT_RAM      0   //多带限幅器与多带drc使用的多带分频器
#define AFX_MULTIBAND_LIMITER_AT_RAM        0   //多带限幅器
#define AFX_MULTIBAND_DRC_AT_RAM            0   //多带drc
#define AFX_VIRTUAL_SURRUOUND_PRO_AT_RAM    0   //虚拟环绕声pro/2t4/2t5
#define AFX_SW_EQ_AT_RAM                    0   //软件EQ
#define AFx_SPATIAL_EFFECT_AT_RAM           0   //空间音效
#define AFx_STEREO_TO_LCR_AT_RAM            0   //virtual surround headphone 2t4
#define AUDIO_LLNS_DNS_CODEC_AT_RAM         0   //llns_dns

/*通话语音处理算法*/
#define AUDIO_CVP_TEXT_AT_RAM		        0	//COMMON TEXT
#define AUDIO_CVP_AEC_AT_RAM		        0	//AEC
#define AUDIO_CVP_NLP_AT_RAM		        0	//NLP
#define AUDIO_CVP_NS_AT_RAM			        0	//ANS/下行降噪
#define AUDIO_CVP_COMMON_AT_RAM		        0	//COMMON
#define AUDIO_CVP_DNS_AT_RAM		        0	//DNS
#define AUDIO_CVP_AGC_AT_RAM		        0	//AGC
#define AUDIO_CVP_DMS_AT_RAM		        0	//双MIC DNS
#define AUDIO_CVP_PREP_AT_RAM		        0	//COMMON 预处理
#define AUDIO_CVP_WN_AT_RAM			        0	//抗风噪
#define AUDIO_CVP_THIRD_AT_RAM		        0	//3MIC

/*编解码编译链接配置*/
#define AUD_AAC_DEC_AT_RAM		            0   //AAC解码
#define AUDIO_LDAC_AT_RAM			        0	//LDAC解码
#define AUDIO_LHDC_AT_RAM			        0	//LHDCv3/v4解码 :78K左右,如果ram不够可以只放L2的段，40K左右
#define AUDIO_LHDC_V5_AT_RAM			    0	//LHDCV5解码 : 18K左右
#define AUDIO_MSBC_CODEC_AT_RAM		        0	//MSBC 编解码
#define AUDIO_CVSD_CODEC_AT_RAM		        0	//CVSD 编解码
#define AUDIO_JLA_CODEC_AT_RAM		        0	//JLA 编解码

#else


/*音效处理链接配置*/
#define AFx_VBASS_AT_RAM				    0	//虚拟低音
#define AFx_VBASS_PRO_AT_RAM			    0	//虚拟低音pro
#define AFx_REVERB_AT_RAM				    0	//混响
#define AFx_ECHO_AT_RAM				        0	//回声
#define AFx_VOICECHANGER_AT_RAM			    0	//变声
#define AFx_DRC_AT_RAM 					    0	//DRC
#define AFx_HARMONIC_EXCITER_AT_RAM 	    0	//谐波激励
#define AFx_DYN_EQ_AT_RAM 				    0	//动态EQ
#define AFx_DYN_EQ_PRO_AT_RAM 				0	//动态EQ Pro
#define AFx_NOTCH_HOWLING_AT_RAM 		    0	//啸叫抑制：陷波
#define AFx_FREQ_SHIFT_AT_RAM	 		    0	//啸叫抑制：移频
#define AFx_NOISEGATE_AT_RAM	 		    0	//噪声门
#define AFx_ADVAUDIO_PLC_AT_RAM	    	    0
#define AFX_AUDIO_LINK_AT_RAM               0   //iis驱动
#define AFX_AUDIO_SYNC_AT_RAM               0   //sync
#define AFx_EQ_AT_RAM                       0	//eq
#define AFx_VOCAL_TRACK_AT_RAM              0   //声道组合与声道拆分
#define AFX_AUDIO_DIGITAL_VOL_AT_RAM        0   //数字音量
#define AFX_AUDIO_ENERGY_DET_AT_RAM         0   //能量检测
#define AFX_LIMITER_AT_RAM                  0   //限幅器
#define AFX_MULTIBAND_CROSSOVER_AT_RAM      0   //多带限幅器与多带drc使用的多带分频器
#define AFX_MULTIBAND_LIMITER_AT_RAM        0   //多带限幅器
#define AFX_MULTIBAND_DRC_AT_RAM            0   //多带drc
#define AFX_VIRTUAL_SURRUOUND_PRO_AT_RAM    0   //虚拟环绕声pro/2t4/2t5
#define AFX_SW_EQ_AT_RAM                    0   //软件EQ
#if TCFG_VIR_UDISK_ENABLE
#define AFx_SPATIAL_EFFECT_AT_RAM           0   //空间音效
#define AFx_DAC_IO_IRQ_HANDLER_AT_RAM       0   //dac io中断代码放ram
#else
#define AFx_SPATIAL_EFFECT_AT_RAM           1   //空间音效
#define AFx_DAC_IO_IRQ_HANDLER_AT_RAM       1   //dac io中断代码放ram
#endif
#define AFx_MIXER_TEXT_AT_RAM               0   //mixer
#define AFx_DAC_TEXT_AT_RAM                 0   //dac
#define AFx_CONVERT_TEXT_AT_RAM             0   //convert,bitwidth convert
#define AFx_VOCAL_REMOVER_TEXT_AT_RAM       0   //人声消除
#define AFx_SPECTRUM_ADV_TEXT_AT_RAM        0   //频谱计算
#define AUDIO_LLNS_DNS_CODEC_AT_RAM			0	//llns_dns降噪算法库的代码段，包括神经网络参数的段
#define AUDIO_LLNS_DNS_BASIC_CODEC_AT_RAM			0	// llns_dns降噪算法库的代码段，不包括神经网络参数的段
#define AUDIO_JLSTREAM_TEXT_AT_RAM          0   //数据流

/*通话语音处理算法*/
#define AUDIO_CVP_TEXT_AT_RAM		        1	//COMMON TEXT
#define AUDIO_CVP_AEC_AT_RAM		        1	//AEC
#define AUDIO_CVP_NLP_AT_RAM		        1	//NLP
#define AUDIO_CVP_NS_AT_RAM			        1	//ANS/下行降噪
#define AUDIO_CVP_COMMON_AT_RAM		        1	//COMMON
#define AUDIO_CVP_DNS_AT_RAM		        1	//DNS
#define AUDIO_CVP_AGC_AT_RAM		        1	//AGC
#define AUDIO_CVP_DMS_AT_RAM		        1	//双MIC DNS
#define AUDIO_CVP_PREP_AT_RAM		        1	//COMMON 预处理
#define AUDIO_CVP_WN_AT_RAM			        1	//抗风噪
#define AUDIO_CVP_THIRD_AT_RAM		        1	//3MIC

/*编解码编译链接配置*/
#define AUD_AAC_DEC_AT_RAM		            1   //AAC解码
#define AUDIO_LDAC_AT_RAM			        1	//LDAC解码
#define AUDIO_LHDC_AT_RAM			        0	//LHDCv3/v4解码 :78K左右,如果ram不够可以只放L2的段，40K左右
#define AUDIO_LHDC_V5_AT_RAM			    0	//LHDCV5解码 : 18K左右
#define AUDIO_MSBC_CODEC_AT_RAM		        0	//MSBC 编解码
#define AUDIO_CVSD_CODEC_AT_RAM		        0	//CVSD 编解码
#define AUDIO_JLA_CODEC_AT_RAM		        1	//JLA 编解码

#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#define AUDIO_LC3_CODEC_AT_RAM		        1	//LC3 编解码
#else
#define AUDIO_LC3_CODEC_AT_RAM		        0	//LC3 编解码
#endif
#define AUDIO_JLA_V2_CODEC_AT_RAM	0	//JLA_V2 编解码

/*语音识别算法编译链接配置*/
#ifdef CONFIG_SOUNDBOX_CASE_ENABLE
#define AUDIO_KWS_COMMON_AT_RAM             0   //kws公共部分 ，0:放flash，1:放ram
#define AUDIO_KWS_YES_NO_AT_RAM             0   //yes/no识别 ， 0:放flash，1:放ram
#define AUDIO_KWS_INDIA_ENGLISH_AT_RAM      0   //印度英语识别，0:放flash，1:放ram
#if TCFG_VIR_UDISK_ENABLE
#define AUDIO_KWS_CHINESE_AT_RAM            0   //近场中文识别，0:放flash，1:放ram，2:一部分放ram，一部分放flash
#define AUDIO_KWS_CHINESE_FAR_AT_RAM        0   //远场中文识别，0:放flash，1:放ram
#else
#define AUDIO_KWS_CHINESE_AT_RAM            2   //近场中文识别，0:放flash，1:放ram，2:一部分放ram，一部分放flash
#define AUDIO_KWS_CHINESE_FAR_AT_RAM        1   //远场中文识别，0:放flash，1:放ram
#endif
#endif

/*语音识别算法编译链接配置*/
#ifdef CONFIG_EARPHONE_CASE_ENABLE
#define AUDIO_KWS_COMMON_AT_RAM             0   //kws公共部分 ，0:放flash，1:放ram
#define AUDIO_KWS_YES_NO_AT_RAM             0   //yes/no识别 ， 0:放flash，1:放ram
#define AUDIO_KWS_CHINESE_AT_RAM            2   //近场中文识别，0:放flash，1:放ram，2:一部分放ram，一部分放flash
#define AUDIO_KWS_INDIA_ENGLISH_AT_RAM      0   //印度英语识别，0:放flash，1:放ram
#define AUDIO_KWS_CHINESE_FAR_AT_RAM        1   //远场中文识别，0:放flash，1:放ram
#endif
//**************************************
// 			模块使能控制
//**************************************

//***************End********************

//**************************************
// 			音效使能控制
//**************************************
#define AUDIO_VBASS_LINK_VOLUME     0 //虚拟低音与音量联动调节
#define AUDIO_EQ_LINK_VOLUME        0 //EQ与音量联动调节
#define AUDIO_AUTODUCK_LINK_VOLUME  0 //自动闪避与音量联动调节
//人声消除串在每个模式的数据流中
#define AUD_VOCAL_REMOVE_SUB_PATH_ENABLE      0

//***************End********************


//**************************************
// 			ADC模块配置
//**************************************
//ADC中断点数
#define AUDIO_ADC_IRQ_POINTS 256
#define AUDIO_ADC_IRQ_POINTS_MUSIC_MODE 256


//**************************************
// 			IIS/TDM模块配置
//**************************************
//IIS/TDM中断点数
#if LEA_DUAL_STREAM_MERGE_TRANS_MODE
#define AUDIO_IIS_IRQ_POINTS 128
#else
#define AUDIO_IIS_IRQ_POINTS 64
#endif

//**************************************
// 			TDM模块配置,由可视化板级配置
//**************************************
#ifndef TDM_WORK_MODE
#define TDM_WORK_MODE DSP0_SHORT
#endif
#ifndef TDM_CH_NUM
#define TDM_CH_NUM  2
#endif


//**************************************
// 			麦克风音效配置
//**************************************
#define DUAL_ADC_EFFECT     0 //两路ADC输入使能(如：麦克风+吉他)


//***************End********************

#if TCFG_BT_VOL_SYNC_ENABLE
#define TCFG_MAX_VOL_PROMPT						 0
#else
#define TCFG_MAX_VOL_PROMPT						 1
#endif

/*
 *该配置适用于没有音量按键的产品，防止打开音量同步之后
 *连接支持音量同步的设备，将音量调小过后，连接不支持音
 *量同步的设备，音量没有恢复，导致音量小的问题
 *默认是没有音量同步的，将音量设置到最大值，可以在vol_sync.c
 *该宏里面修改相应的设置。
 */
#define TCFG_VOL_RESET_WHEN_NO_SUPPORT_VOL_SYNC	 0 //不支持音量同步的设备默认最大音量

#define TCFG_1T2_VOL_RESUME_WHEN_NO_SUPPORT_VOL_SYNC	1	//1T2不支持音量同步的设备则恢复上次设置的音量值

/*省电容mic模块使能*/
#if ((TCFG_ADC0_ENABLE && (TCFG_ADC0_MODE == 2)) || \
     (TCFG_ADC1_ENABLE && (TCFG_ADC1_MODE == 2)))
#define TCFG_SUPPORT_MIC_CAPLESS        1
#else
#define TCFG_SUPPORT_MIC_CAPLESS        0
#endif

/*省电容mic校准方式选择*/
#define MC_BIAS_ADJUST_DISABLE			0	//省电容mic偏置校准关闭
#define MC_BIAS_ADJUST_ONE			 	1	//省电容mic偏置只校准一次（跟dac trim一样）
#define MC_BIAS_ADJUST_POWER_ON		 	2	//省电容mic偏置每次上电复位都校准(Power_On_Reset)
#define MC_BIAS_ADJUST_ALWAYS		 	3	//省电容mic偏置每次开机都校准(包括上电复位和其他复位)
/*
 *省电容mic偏置电压自动调整(因为校准需要时间，所以有不同的方式)
 *1、烧完程序（完全更新，包括配置区）开机校准一次
 *2、上电复位的时候都校准,即断电重新上电就会校准是否有偏差(默认)
 *3、每次开机都校准，不管有没有断过电，即校准流程每次都跑
 */
#define TCFG_MC_BIAS_AUTO_ADJUST	 	MC_BIAS_ADJUST_ONE
#define TCFG_MC_CONVERGE_PRE			0  //省电容mic预收敛
#define TCFG_MC_CONVERGE_TRACE			0  //省电容mic收敛值跟踪
/*
 *省电容mic收敛步进限制
 *0:自适应步进调整, >0:收敛步进最大值
 *注：当mic的模拟增益或者数字增益很大的时候，mic_capless模式收敛过程,
 *变化的电压放大后，可能会听到哒哒声，这个时候就可以限制住这个收敛步进
 *让收敛平缓进行(前提是预收敛成功的情况下)
 */
#define TCFG_MC_DTB_STEP_LIMIT			3  //最大收敛步进值
/*
 *省电容mic使用固定收敛值
 *可以用来测试默认偏置是否合理：设置固定收敛值7000左右，让mic的偏置维持在1.5v左右即为合理
 *正常使用应该设置为0,让程序动态收敛
 */
#define TCFG_MC_DTB_FIXED				0

#define TCFG_ESCO_PLC					1  	//通话丢包修复(1T2已修改为节点)
#ifdef CONFIG_EARPHONE_CASE_ENABLE
#define TCFG_AEC_ENABLE					1	//通话回音消除使能
#else
#define TCFG_AEC_ENABLE					0	//通话回音消除使能
#endif

/*
 * 音量节点支持DAC动态模拟增益切换
 * 1、模拟/数字增益配置为0dB:
 *  音频配置->DAC配置->Analog Gain 0dB、Digital Gain 0dB
 * 2、特殊配置:
 *  (1) D版及后续版本音量节点需要特殊配置
 *      <1>音量表类型:自定义
 *      <2>在需要使用模拟音量切换的档位，该档位的音量值与高一级的音量值相同，程序内会自行检索。
 *      例:小于7级使能动态模拟增益切换，则第7级音量与第8级音量配置相同音量值
 *  (2) A/B/C版
        音量节点不需要特殊处理，手动配置AVOL_SET_LVL 确定需要使用模拟音量切换的档位
 */
#define ANA_VOL_DYNAMIC_SWITCH          0
#define AVOL_SET_LVL                    7       //A/B/C版配置有效，D版无效

#define MAX_ANA_VOL               (3)	// 系统最大模拟音量,范围: 0 ~ 3
//#define MAX_COM_VOL             (16)    // 数值应该大于等于16，具体数值应小于联合音量等级的数组大小 (combined_vol_list)
//#define MAX_DIG_VOL             (16)    // 数值应该大于等于16，因为手机是16级，如果小于16会导致某些情况手机改了音量等级但是小机音量没有变化

/*#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
#define SYS_MAX_VOL             16
#define SYS_DEFAULT_VOL         16
#define SYS_DEFAULT_TONE_VOL    10
#define SYS_DEFAULT_SIN_VOL    	8

#elif (SYS_VOL_TYPE == VOL_TYPE_DIGITAL_HW)
#define SYS_MAX_VOL             MAX_DIG_VOL
#define SYS_DEFAULT_VOL         SYS_MAX_VOL
#define SYS_DEFAULT_TONE_VOL    10
#define SYS_DEFAULT_SIN_VOL    	8

#elif (SYS_VOL_TYPE == VOL_TYPE_ANALOG)
#define SYS_MAX_VOL             MAX_ANA_VOL
#define SYS_DEFAULT_VOL         SYS_MAX_VOL
#define SYS_DEFAULT_TONE_VOL    10
#define SYS_DEFAULT_SIN_VOL    	8

#elif (SYS_VOL_TYPE == VOL_TYPE_AD)
#define SYS_MAX_VOL             MAX_COM_VOL
#define SYS_DEFAULT_VOL         SYS_MAX_VOL
#define SYS_DEFAULT_TONE_VOL    14
#define SYS_DEFAULT_SIN_VOL    	8
#else
#error "SYS_VOL_TYPE define error"
#endif*/

/*数字音量最大值定义*/
#define DEFAULT_DIGITAL_VOLUME   (TCFG_AUDIO_DIGITAL_GAIN)
#define IDLE_DEFAULT_MAX_VOLUME  16     //音量调节IDLE 状态的最大音量

#define ANC_CLIPPING_MODE_MUSIC_LIMIT		1	//ANC防破音-限制音乐幅度，牺牲音乐
#define ANC_CLIPPING_MODE_DYNAMIC_ANC_GAIN	2	//ANC防破音-ANC音乐动态增益，牺牲ANC性能

#define BT_MUSIC_VOL_LEAVE_MAX	16		/*高级音频音量等级*/
#define BT_CALL_VOL_LEAVE_MAX	15		/*通话音量等级*/
// #define BT_CALL_VOL_STEP		(-2.0f)	[>通话音量等级衰减步进<]

#define TONE_BGM_FADEOUT            0   //播叠加提示音时是否将背景音淡出

#define VOL_TAB_CUSTOM_EN           1  //使能音量表功能

#define TCFG_AUDIO_MIC_DUT_ENABLE	0   //麦克风测试和传递函数测试

//支持 ADC DIGITAL 按最大通道数开启, 中断拿数需按使用的ADC_CH拆分
#ifdef CONFIG_SOUNDBOX_CASE_ENABLE
#define TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
#endif

#define ANC_EXT_V1			1	//仅支持入耳
#define ANC_EXT_V2			2	//支持入耳、半入耳
//ANC耳道自适应版本
#define TCFG_AUDIO_ANC_EAR_ADAPTIVE_VERSION 	ANC_EXT_V2
#if TCFG_AUDIO_ANC_MULT_ORDER_ENABLE || TCFG_AUDIO_ANC_EXT_EN
#define TCFG_AUDIO_ANC_EXT_VERSION 				ANC_EXT_V2
#else
#define TCFG_AUDIO_ANC_EXT_VERSION 				0
#endif

/*
 * ANC MIC不使用可视化界面配置，单独使用此处配置
 * 通常用于通话与ANC的MIC复用(使用不同的IO通道)
 * 获取该配置方法：
 * (1)在可视化工具界面，配置ANC对应的MIC配置，将其填到下面
 * (2)还原可视化界面MIC参数
 */
#define ANC_MIC_REUSE_ENABLE       0

#define ANC_ADC0_MODE 1 // 模式
#define ANC_ADC0_AIN_SEL 1 // 输入端口
#define ANC_ADC0_BIAS_SEL 0 // 供电端口
#define ANC_ADC0_BIAS_RSEL 3 // MIC BIAS上拉电阻挡位
#define ANC_ADC0_POWER_IO IO_PORTC_01 // IO供电选择
#define ANC_ADC0_DCC_EN 1 // DCC使能
#define ANC_ADC0_DCC_LEVEL 1 // DCC 截止频率

#define ANC_ADC1_MODE 1 // 模式
#define ANC_ADC1_AIN_SEL 1 // 输入端口
#define ANC_ADC1_BIAS_SEL 0 // 供电端口
#define ANC_ADC1_BIAS_RSEL 3 // MIC BIAS上拉电阻挡位
#define ANC_ADC1_POWER_IO IO_PORTC_01 // IO供电选择
#define ANC_ADC1_DCC_EN 1 // DCC使能
#define ANC_ADC1_DCC_LEVEL 1 // DCC 截止频率

#endif/*_AUDIO_CONFIG_DEF_H_*/
