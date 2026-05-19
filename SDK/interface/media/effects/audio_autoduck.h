#ifndef _AUDIO_AUTODUCK_H_
#define _AUDIO_AUTODUCK_H_

/****************************************Auto Duck*****************************************/
#define AUTODUCK_DVOL_MAX           16384

struct autoduck_update_parm {
    int duck_amount; //背景音乐闪避的音量值（dB）
    int attack;      //启动时间（ms）
    int release;     //释放时间（ms）
    int hold_time;   //闪避之后的保持时间 （ms）
};

typedef struct __AutoDuckParam_TOOL_SET__ {
    int is_bypass;
    struct autoduck_update_parm parm;
} autoduck_param_tool_set;

typedef struct _autoduck_hdl_ {
    struct autoduck_update_parm parm;
    s32 sr;
    s16 vol_fade;
    s16 fade_out_step;
    s16 fade_in_step;
    float gain;
    int hold_len;
    int autoduck_cnt;
    u8 update;
    u8 status;
    u8 trigger;
    u8 bit_wide;
} autoduck_hdl;

//打开
autoduck_hdl *audio_autoduck_open(struct autoduck_update_parm *parm);

//关闭
void audio_autoduck_close(autoduck_hdl *hdl);

//参数更新
void audio_autoduck_update_parm(autoduck_hdl *hdl, struct autoduck_update_parm *parm);

//数据处理
int audio_autoduck_run(autoduck_hdl *hdl, s16 *data, int len);

//bypass
void audio_autoduck_bypass(autoduck_hdl *hdl, u8 bypass);

extern void autoduck_lock();
extern void autoduck_unlock();
/***********************************Auto Duck trigger*************************************/

struct autoduck_trigger_update_parm {
    int threshold;   //前台音频的阈值，大于该值，背景音乐闪避 （dB）
};

typedef struct __AutoDuckTriggerParam_TOOL_SET__ {
    int is_bypass;
    struct autoduck_trigger_update_parm parm;
} autoduck_trigger_param_tool_set;

typedef struct _autoduck_trigger_hdl_ {
    struct autoduck_trigger_update_parm parm;
    u8 update;
    u8 status;
    u8 bit_wide; //1-> 32bit; 0-> 16bit
    u8 trigger;
} autoduck_trigger_hdl;

//打开
autoduck_trigger_hdl *audio_autoduck_trigger_open(struct autoduck_trigger_update_parm *parm);

//关闭
void audio_autoduck_trigger_close(autoduck_trigger_hdl *hdl);

//参数更新
void audio_autoduck_trigger_update_parm(autoduck_trigger_hdl *hdl, struct autoduck_trigger_update_parm *parm);

//数据处理
int audio_autoduck_trigger_run(autoduck_trigger_hdl *hdl, s16 *data, int len, u8 ch_num);

//bypass
void audio_autoduck_trigger_bypass(autoduck_trigger_hdl *hdl, u8 bypass);

#endif

#ifndef RUN_NORMAL
#define RUN_NORMAL  0
#endif

#ifndef RUN_BYPASS
#define RUN_BYPASS  1
#endif

