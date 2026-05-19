#ifndef _ICSD_AFQ_APP_H
#define _ICSD_AFQ_APP_H

#include "typedef.h"
#include "icsd_afq.h"
#include "anc.h"

#define ICSD_AFQ_EN_MODE_NEXT_SW		1	/*提示音播完才允许下一次切换*/

//ICSD AFQ 状态
enum {
    ICSD_AFQ_STATE_CLOSE = 0,
    ICSD_AFQ_STATE_OPEN,
};

//ICSD AFQ TONE状态
enum {
    ICSD_AFQ_STATE_TONE_STOP = 0,
    ICSD_AFQ_STATE_TONE_START,
};

//查询afq是否活动中
u8 audio_icsd_afq_is_running(void);

void icsd_afq_output(__afq_output *_AFQ_OUT);

/*
   音频频响(SZ/PZ)获取启动(还原为上一个ANC模式)
	param: 	tone_en  是否播放提示音
*/
int audio_icsd_afq_open(u8 tone_en);

/*
   音频频响(SZ/PZ)获取启动(指定退出ANC模式)
	param: 	tone_en  是否播放提示音
			exit_anc_mode  退出时指定ANC模式 如 ANC_MODE_NULL 则返回上一个模式
*/
int audio_icsd_afq_open_fix_exit_mode(u8 tone_en, ANC_mode_t exit_mode);

int audio_icsd_afq_init(void);

int audio_icsd_afq_output(__afq_output *out);

int audio_icsd_afq_close();

void audio_afq_dac_check_flag_set(u8 flag);

void audio_afq_inear_state_set(u8 inear);

u8 audio_afq_inear_check(u8 anc_mode);

#endif
