#ifndef _AUDIO_FADE_H_
#define _AUDIO_FADE_H_

struct fade_update_parm {
    int type;
    int dump_time;
    int fade_time;
};

struct fade_update_parm_with_callback {
    struct fade_update_parm parm;
    void *priv;
    int (*callback)(void *fade_hdl, int status);//淡出结束时的回调函数
};

typedef struct _FadeParam_TOOL_SET {
    int is_bypass;
    struct fade_update_parm parm;
} fade_param_tool_set;

struct fade_open_parm {
    struct fade_update_parm parm;
    u32 samplerate;
    u32 channel_num;
    u8 bit_width;
};

typedef struct _fade_hdl {
    void *priv;
    int (*callback)(void *fade_hdl, int status);//淡出结束时的回调函数
    struct fade_open_parm fade_parm;
    float value;
    float step;
    u32 dump_len; //开头丢弃的数据长度（byte），对应dump_time
    u8 status;
} fade_hdl;


#define AUDIO_FADE_IN_FINISH   1
#define AUDIO_FADE_OUT_FINISH  0

//打开
fade_hdl *audio_fade_open(struct fade_open_parm *param);

//关闭
void audio_fade_close(fade_hdl *hdl);

//参数更新
void audio_fade_update_parm(fade_hdl *hdl, struct fade_update_parm *parm);
void audio_fade_update_parm_with_callback(fade_hdl *hdl, struct fade_update_parm_with_callback *parm);

//数据处理
int audio_fade_run(fade_hdl *hdl, s16 *indata, s16 *outdata, int len);

//暂停处理
void audio_fade_bypass(fade_hdl *hdl, u8 bypass);


struct audio_xfade {
    float fin_value;
    float fout_value;
    float step;
    u8 bit_width;
    u8 ch_num;
};
void audio_xfade_run(struct audio_xfade *xfade, void *fin, void *fout, void *out, u32 len);
float audio_fade_out(void *data, float step, float value, int channel, int per_channel_npoint, u8 bit_width);
float audio_fade_in(void *data, float step, float value, int channel, int per_channel_npoint, u8 bit_width);

#endif
