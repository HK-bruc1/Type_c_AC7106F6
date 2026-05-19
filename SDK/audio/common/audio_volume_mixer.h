#ifndef _AUD_VOLUME_MIXER_H_
#define _AUD_VOLUME_MIXER_H_

#include "generic/typedef.h"

struct volume_mixer {
    u16(*hw_dvol_max)(void);
};

struct vol_offset_fade_handle {
    int timer_ms; //配置定时器周期
    int timer_id; //记录定时器id
    float cur_offset; //记录当前增益
    float fade_step;//记录淡入淡出步进
    float target_offset;
};
int audio_app_set_vol_offset_dB_fade_process_by_app_core(struct vol_offset_fade_handle *hdl, float target_offset, int fade_time_ms);

int audio_app_set_vol_offset_dB_fade_process(struct vol_offset_fade_handle *hdl, float target_offset, int fade_time_ms);

void audio_volume_mixer_init(struct volume_mixer *param);

/*
*********************************************************************
*                  Audio Volume Get
* Description: 音量获取
* Arguments  : state	要获取音量值的音量状态
* Return	 : 返回指定状态对应得音量值
* Note(s)    : None.
*********************************************************************
*/
s16 app_audio_get_volume(u8 state);

#endif
