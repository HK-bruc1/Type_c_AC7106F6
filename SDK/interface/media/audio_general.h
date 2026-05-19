#ifndef _AUDIO_GENERAL_H
#define _AUDIO_GENERAL_H

#include "generic/typedef.h"
struct audio_general_params {
    int sample_rate;
    u8 system_bit_width;
    u8 media_bit_width;
    u8 esco_bit_width;
    u8 mic_bit_width;
    u8 usb_audio_bit_width;
};

enum {
    BYPASS_OFF = 0,
    BYPASS_ON,
    FADE_BYPASS_OFF,
    FADE_BYPASS_ON,
};

struct audio_dac_noisegate {
    u8 threshold;			//底噪阈值，pcm数据绝对值，|pcm| ≤threshold判定为noise
    u16 detect_interval;	//检测间隔，单位ms
};

struct audio_general_params *audio_general_get_param(void);
int audio_general_out_dev_bit_width();
int audio_general_in_dev_bit_width();
int audio_general_init();
int audio_general_set_global_sample_rate(int sample_rate);

#endif
