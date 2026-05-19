#ifndef _ICSD_VDT_APP_H_
#define _ICSD_VDT_APP_H_

#include "generic/typedef.h"
#include "icsd_common.h"
#include "icsd_common_v2.h"

/*默认免摘定时结束时间(单位ms)*/
#define AUDIO_SPEAK_TO_CHAT_END_TIME		5000

/*智能免摘检测到声音和退出通透是否播放提示音*/
#define SPEAK_TO_CHAT_PLAY_TONE_EN  1

/*智能免摘每次检测到声音的测试提示音*/
#define SPEAK_TO_CHAT_TEST_TONE_EN  0


//免摘状态
enum audio_vdt_state {
    AUDIO_VDT_STATE_CLOSE = 0,
    AUDIO_VDT_STATE_OPEN,
    AUDIO_VDT_STATE_TRIGGER,
    AUDIO_VDT_STATE_RESUME,
};

//模式切换: 免摘信息保持状态
enum audio_vdt_keep_state {
    AUDIO_VDT_KEEP_STATE_STOP = 0,
    AUDIO_VDT_KEEP_STATE_PREPARE,
    AUDIO_VDT_KEEP_STATE_START,
};

/***********************USER API*****************************/

/*打开智能免摘*/
int audio_speak_to_chat_open();

/*关闭智能免摘*/
int audio_speak_to_chat_close();

void audio_speak_to_chat_demo();

/*获取智能免摘是否触发 return 1 触发; 0 不触发*/
u8 audio_speak_to_chat_is_trigger();

/*恢复免摘之前的状态*/
void audio_speak_to_chat_resume(void);

/*设置免摘定时结束的时间，单位ms*/
int audio_speak_to_char_end_time_set(u16 time);

/*设置智能免摘检测的灵敏度*/
int audio_speak_to_chat_sensitivity_set(u8 sensitivity);


/***********************INTERNAL API*****************************/

void audio_speak_to_chat_init(void);

u8 audio_vdt_keep_state_get(void);

u8 audio_vdt_keep_state_send_to_adt(u8 state);

u8 audio_vdt_keep_state_set(u8 state);

void audio_vdt_trigger_send_to_adt(u8 state);

int audio_adt_vdt_ioctl(int cmd, int arg);

void audio_vdt_resume_send_to_adt(void);

u8 audio_vdt_state_get(void);

/*智能免摘识别结果输出回调*/
void audio_speak_to_chat_output_handle(u8 voice_state);

/*免摘触发执行函数*/
int audio_vdt_trigger_process(u8 tone);

/*免摘恢复执行函数*/
void audio_vdt_resume_process(void);

u8 audio_vdt_tws_trigger_set(u8 en);




#endif


