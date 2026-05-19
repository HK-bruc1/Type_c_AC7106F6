
#ifndef _AUDIO_ANC_FADE_CTR_H_
#define _AUDIO_ANC_FADE_CTR_H_

#include "generic/typedef.h"

//默认淡入增益，dB换算公式 fade_gain = 10^(dB/20) * 16384;
#define AUDIO_ANC_FADE_GAIN_DEFAULT		16384	//(0dB)

//ANC全部通道
#define AUDIO_ANC_FDAE_CH_ALL  AUDIO_ANC_FADE_CH_LFF | AUDIO_ANC_FADE_CH_LFB | AUDIO_ANC_FADE_CH_RFF | AUDIO_ANC_FADE_CH_RFB
#define AUDIO_ANC_FDAE_CH_FF  AUDIO_ANC_FADE_CH_LFF | AUDIO_ANC_FADE_CH_RFF
#define AUDIO_ANC_FDAE_CH_FB  AUDIO_ANC_FADE_CH_LFB | AUDIO_ANC_FADE_CH_RFB

/*
    ANC增益管理：
    1、不同模式之间需操控ANC增益时，会以最小的增益需求为准；
    2、用户应用层需调整增益时，如手机APP，出入耳等，需新增模式，不可直接调用SDK已有模式
*/
enum anc_fade_mode_t {
    //=========SDK专用，用户不可调用===========
    ANC_FADE_MODE_RESET = 0,		//复位
    ANC_FADE_MODE_SWITCH,			//ANC模式切换
    ANC_FADE_MODE_MUSIC_DYNAMIC,	//音乐动态增益
    ANC_FADE_MODE_SCENE_ADAPTIVE,	//ANC场景噪声自适应
    ANC_FADE_MODE_WIND_NOISE,		//ANC风噪检测
    ANC_FADE_MODE_SUSPEND,			//ANC挂起
    ANC_FADE_MODE_HOWLDET,			//啸叫检测
    ANC_FADE_MODE_ENV_ADAPTIVE_GAIN,//ANC环境噪声自适应增益
    ANC_FADE_MODE_EXT,              //ANC(扩展数据流)使用
    ANC_FADE_MODE_DCC_TRIM,         //DCC 校准
    ANC_FADE_MODE_ADAPTIVE_TONE,	//入耳自适应同步处理

    //======用户可再此继续添加模式使用=========
    ANC_FADE_MODE_USER,				//用户模式
};

#define ANC_FADE_GAIN_MAX_FLOAT (16384.0f)
struct anc_fade_handle {
    int timer_ms; //配置定时器周期
    u8 fade_setp_flag; //配置0:增益，1：dB值
    int timer_id; //记录定时器id
    int cur_gain; //记录当前增益
    int fade_setp;//记录淡入淡出步进
    float fade_setp_dB;//记录淡入淡出步进,dB
    int target_gain; //记录目标增益
    u8 fade_gain_mode;//记录当前设置fade gain的模式

    int cur_gain_tmp;
    float fade_setp_dB_tmp;
};
/*anc增益淡入淡出*/
int audio_anc_gain_fade_process(struct anc_fade_handle *hdl, enum anc_fade_mode_t mode, int target_gain, int fade_time_ms);

/*
   ANC淡入淡出增益设置
	param:  mode 		场景模式
			ch  		设置目标通道(支持多通道)
			gain 设置增益
	notes: ch 支持配置多个通道，但mode 必须与 ch配置一一对应;
			当设置gain = 16384, 会自动删除对应模式
    cpu差异：AC700N/JL701N 仅支持AUDIO_ANC_FDAE_CH_ALL;
             JL708N 支持所有通道独立配置;
 */
void audio_anc_fade_ctr_set(enum anc_fade_mode_t mode, u8 ch, u16 gain);

//删除fade mode
void audio_anc_fade_ctr_del(enum anc_fade_mode_t mode);

//fade ctr 初始化
void audio_anc_fade_ctr_init(void);

u16 audio_anc_fade_ctr_get_min();


#endif/*_AUDIO_ANC_FADE_CTR_H_*/

