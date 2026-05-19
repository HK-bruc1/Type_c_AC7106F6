
#ifndef __ICSD_DOT_APP_H_
#define __ICSD_DOT_APP_H_

#include "typedef.h"
#include "icsd_dot.h"
#include "icsd_common_v2_app.h"

/*
	佩戴松紧度阈值thr
	1、thr > DOT_NORM_THR 				  判定:紧
	2、DOT_NORM_THR >= thr > DOT_LOOSE_THR 判定:正常
	3、DOT_LOOSE_THR  >= thr				  判定:松
*/
#define ICSD_DOT_NORM_THR				0.0f
#define ICSD_DOT_LOOSE_THR				-6.0f


//ICSD DOT 状态
enum {
    ICSD_DOT_STATE_CLOSE = 0,
    ICSD_DOT_STATE_OPEN,
};


enum {
    AUDIO_FIT_DET_RESULT_TIGHT = 0,		//佩戴紧
    AUDIO_FIT_DET_RESULT_NORMAL,		//佩戴正常
    AUDIO_FIT_DET_RESULT_LOOSE,			//佩戴松
};

/*
   贴合度检测打开
	param: tone_en  1 使用提示音检测； 0 使用音乐检测(可能存在结果不可靠的问题)
		   result_cb,	检查结果func_callback
	return 0  打开成功； 1 打开失败
*/
int audio_icsd_dot_open(enum audio_adaptive_fre_sel fre_sel, void (*result_cb)(int result));

//贴合度检测关闭
int audio_icsd_dot_close();

int audio_icsd_dot_end(int result_thr);

//查询DOT是否活动中
u8 audio_icsd_dot_is_running(void);

float audio_icsd_dot_light_open(struct audio_afq_output *p);

#endif/*__ICSD_DOT_APP_H_*/

