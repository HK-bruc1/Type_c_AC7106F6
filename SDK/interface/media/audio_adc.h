#ifndef AUDIO_ADC_H
#define AUDIO_ADC_H

#include "generic/typedef.h"
#include "generic/list.h"
#include "system/spinlock.h"
#include "audio_def.h"
#include "cpu/includes.h"
#include "asm/adc.h"
#include "media_config.h"

#ifndef AUDIO_ADC_LPADC_NUM
#define AUDIO_ADC_LPADC_NUM      0
#endif

//ADC模拟增益档位对应的dB table表
extern const s8 audio_adc_gain_dB_table[];
/*
*********************************************************************
*                  Audio ADC Initialize
* Description: 初始化Audio_ADC模块的相关数据结构
* Arguments  : adc	ADC模块操作句柄
*			   pd	ADC模块硬件相关配置参数
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_init(struct audio_adc_hdl *adc, struct audio_adc_private_param *private);

/*
*********************************************************************
*                  Audio ADC Output Callback
* Description: 注册adc采样输出回调函数
* Arguments  : adc		adc模块操作句柄
*			   output  	采样输出回调
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_add_output_handler(struct audio_adc_hdl *adc, struct audio_adc_output_hdl *output);

/*
*********************************************************************
*                  Audio ADC Output Callback
* Description: 删除adc采样输出回调函数
* Arguments  : adc		adc模块操作句柄
*			   output  	采样输出回调
* Return	 : None.
* Note(s)    : 采样通道关闭的时候，对应的回调也要同步删除，防止内存释
*              放出现非法访问情况
*********************************************************************
*/
void audio_adc_del_output_handler(struct audio_adc_hdl *adc, struct audio_adc_output_hdl *output);

/*
*********************************************************************
*                  Audio ADC IRQ Handler
* Description: Audio ADC中断回调函数
* Arguments  : adc  adc模块操作句柄
* Return	 : None.
* Note(s)    : 仅供Audio_ADC中断使用
*********************************************************************
*/
void audio_adc_irq_handler(struct audio_adc_hdl *adc);

/*
*********************************************************************
*                  Audio ADC Mic Open
* Description: 打开mic采样通道
* Arguments  : mic	    mic操作句柄
*			   ch_map	mic通道索引
*			   adc      adc模块操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/

int audio_adc_mic_open(struct adc_mic_ch *mic, u32 ch_map, struct audio_adc_hdl *adc, struct mic_open_param *param);

/*
*********************************************************************
*                  Audio ADC Mic Gain
* Description: 设置mic增益
* Arguments  : mic	mic操作句柄
*			   gain	mic增益
* Return	 : 0 成功	其他 失败
* Note(s)    : MIC增益范围：0(-8dB)~19(30dB),step:2dB,level(4)=0dB
*********************************************************************
*/
int audio_adc_mic_set_gain(struct adc_mic_ch *mic, u32 ch_map, int gain);

/*
*********************************************************************
*                  Audio ADC Mic Digital Gain
* Description: 设置mic数字增益
* Arguments  : 数字增益
* Return	 : none
* Note(s)    : 单位dB，可配范围参考可视化工具(不同系列范围不同):
*              "音频配置->ADC配置->数字增益"
*********************************************************************
*/
void audio_adc_set_digital_gain(float gain_dB);

/*
*********************************************************************
*                  Audio ADC Mic Pre_Gain
* Description: 设置mic第一级/前级增益
* Arguments  : en 前级增益使能(0:6dB 1:0dB)
* Return	 : None.
* Note(s)    : 前级增益只有0dB和6dB两个档位
*********************************************************************
*/
void audio_adc_mic_0dB_en(u32 ch_map, bool en);

/*
*********************************************************************
*                  Audio ADC Mic Gain Boost
* Description: 设置mic第一级/前级增益
* Arguments  : ch_map AUDIO_ADC_MIC_0/AUDIO_ADC_MIC_1/AUDIO_ADC_MIC_2/AUDIO_ADC_MIC_3,多个通道可以或上同时设置
*              level 前级增益档位(AUD_MIC_GB_0dB/AUD_MIC_GB_6dB)
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_mic_gain_boost(u32 ch_map, u8 level);

/*
*********************************************************************
*                  Audio ADC linein Open
* Description: 打开linein采样通道
* Arguments  : linein	linein操作句柄
*			   ch_map	linein通道索引
*			   adc      adc模块操作句柄
* Return	 : 0 成功	其他 失败
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_linein_open(struct adc_linein_ch *linein, u32 ch_map, struct audio_adc_hdl *adc, struct linein_open_param *param);

/*
*********************************************************************
*                  Audio ADC linein Gain
* Description: 设置linein增益
* Arguments  : linein	linein操作句柄
*			   gain	linein增益
* Return	 : 0 成功	其他 失败
* Note(s)    : linein增益范围：0(-8dB)~19(30dB),step:2dB,level(4)=0dB
*********************************************************************
*/
int audio_adc_linein_set_gain(struct adc_linein_ch *linein, u32 ch_map, int gain);

/*
*********************************************************************
*                  Audio ADC Linein Gain Boost
* Description: 设置linein第一级/前级增益
* Arguments  : ch_map AUDIO_ADC_LINE0/AUDIO_ADC_LINE1/AUDIO_ADC_LINE2/AUDIO_ADC_LINE3,多个通道可以或上同时设置
*              level 前级增益档位(AUD_LINEIN_GB_0dB/AUD_LINEIN_GB_6dB)
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_linein_gain_boost(u32 ch_map, u8 level);

/*
*********************************************************************
*                  Audio ADC linein Pre_Gain
* Description: 设置linein第一级/前级增益
* Arguments  : en 前级增益使能(0:6dB 1:0dB)
* Return	 : None.
* Note(s)    : 前级增益只有0dB和6dB两个档位
*********************************************************************
*/
void audio_adc_linein_0dB_en(u32 ch_map, bool en);

/*
*********************************************************************
*                  AUDIO MIC_LDO Control
* Description: mic电源mic_ldo控制接口
* Arguments  : index	ldo索引(MIC_LDO/MIC_LDO_BIAS0/MIC_LDO_BIAS1)
* 			   en		使能控制
*			   pd		audio_adc模块配置
* Return	 : 0 成功 其他 失败
* Note(s)    : (1)MIC_LDO输出不经过上拉电阻分压
*				  MIC_LDO_BIAS输出经过上拉电阻分压
*			   (2)打开一个mic_ldo示例：
*				audio_adc_mic_ldo_en(MIC_LDO,1);
*			   (2)打开多个mic_ldo示例：
*				audio_adc_mic_ldo_en(MIC_LDO | MIC_LDO_BIAS,1);
*********************************************************************
*/
int audio_adc_mic_ldo_en(u8 index, u8 en, u8 mic_bias_rsel);

int audio_adc_mic_set_sample_rate(struct adc_mic_ch *mic, int sample_rate);

/*
*********************************************************************
*                  Audio ADC Mic Get Sample Rate
* Description: 获取mic采样率
* Arguments  : None
* Return	 : sample_rate
* Note(s)    : None.
*********************************************************************
*/
int audio_adc_mic_get_sample_rate();

int audio_adc_sample_rate_mapping(int sample_rate);

int audio_adc_mic_set_buffs(struct adc_mic_ch *mic, s16 *bufs, u16 buf_size, u8 buf_num);

int audio_adc_mic_start(struct adc_mic_ch *mic);

int audio_adc_mic_close(struct adc_mic_ch *mic);
int audio_adc_mic_ch_close(struct adc_mic_ch *mic, u8 adc_ch);

int audio_adc_linein_set_sample_rate(struct adc_linein_ch *linein, int sample_rate);
int audio_adc_linein_set_buffs(struct adc_linein_ch *linein, s16 *bufs, u16 buf_size, u8 buf_num);

int audio_adc_linein_start(struct adc_linein_ch *linein);

int audio_adc_linein_close(struct adc_linein_ch *linein);

int audio_adc_mic_type(u8 mic_idx);

u8 audio_adc_is_active(void);

void audio_adc_add_ch(struct audio_adc_hdl *adc, u8 amic_seq);

int get_adc_seq(struct audio_adc_hdl *adc, u16 ch_map);

int audio_adc_set_buf_fix(u8 fix_en, struct audio_adc_hdl *adc);
/*
*********************************************************************
*                  Audio ADC Mic PGA Mute
* Description: 打开ADC的PGA Mute时，相当与设置了PGA增益为-40dB
* Arguments  : ch_map	mic的通道
*			   mute     mute的控制位
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_mic_set_pga_mute(u32 ch_map, bool mute);

/*
*********************************************************************
*                  Audio ADC Linein PGA Mute
* Description: 打开ADC的PGA Mute时，相当与设置了PGA增益为-40dB
* Arguments  : ch_map	linein的通道
*			   mute     mute的控制位
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_adc_linein_set_pga_mute(u32 ch_map, bool mute);

#ifdef CONFIG_CPU_BR36
void audio_anc_mic_gain(u8 gain0, u8 gain1);
#else
void audio_anc_mic_gain(audio_adc_mic_mana_t *mic_param, u8 mult_gain_en);
#endif

void audio_anc_mic_open(audio_adc_mic_mana_t *mic_param, u8 trans_en, u8 adc_ch);

void audio_anc_mic_close(audio_adc_mic_mana_t *mic_param);

#if SUPPORT_MIC_CAPLESS
int audio_mic_bias_adjust(struct mic_capless_trim_result *capless_trim, struct mic_capless_trim_param *capless_param);
#endif

#endif/*AUDIO_ADC_H*/

