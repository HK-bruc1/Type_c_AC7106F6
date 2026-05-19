#ifndef _AUDIO_EFFECTS_API_H_
#define _AUDIO_EFFECTS_API_H_
#include "system/includes.h"
#include "effects/audio_phaser.h"
#include "effects/audio_flanger.h"
#include "effects/audio_chorus_advance.h"
#include "effects/audio_pingpong_echo.h"
#include "effects/audio_stereo_spatial_wider.h"
#include "effects/audio_distortion_clipping.h"
#include "effects/audio_frequency_compressor.h"
#include "effects/convert_data.h"
#include "effects/audio_spatial_adv.h"
#include "effects/audio_virtual_bass_pro.h"
#include "effects/audio_vbass.h"
#include "effects/audio_virtual_surround_hp.h"
#include "effects/audio_vibrato.h"
#include "effects/audio_harmony.h"
#include "effects/audio_tremolo.h"
#include "effects/audio_plate_reverb_echo_combined.h"
#include "effects/audio_bass_boost_combined.h"
#include "effects/audio_fade.h"


struct effects_param {
    u32 sample_rate;
    u16 param_len;//算法参数结构 长度
    u8 in_ch_num;
    u8 out_ch_num;
    u8 in_bit_width;
    u8 out_bit_width;
    u8 qval;
    u8 bypass;
    u8 aud_module_type;
    u8 aud_module_tmpbuf_type;
    u8 set_tmpbuf_before_init;//EFFECTS_SET_TMPBUF_BEF_INIT:根据算法特性在init前,设置tmpbuf EFFECTS_SET_TMPBUF_BEF_RUN:run前设置tmpbuf
    u8 cross_fade_en;//CONFIG_CROSS_FADE_POSITION_IN_BYPASS:bypass时，做cross fade处理, CONFIG_CROSS_FADE_POSITION_IN_RUN: run时，做cross fade处理
    u8 hardware_ctrl;//算法调用到固件层的接口，通过update接口,做释放处理, BIT(0):EFFECTS_HARDWARE_NEED_CLOSE,BIT(1):EFFECTS_HARDWARE_TO_CLOSE,
    int (*private_update)(void *ptr, void *param); //参数改变需要重新初始化算法,需实现该接口
    int (*tmpbuf_size_update)(void *ptr, void *param, int len); //需要使用run的长度更新tmpbuf_size
    void *param;//算法参数结构
    void *node_hdl;//node_hdl
    struct effects_func_api *ops;//算法接口
};

struct audio_effects {
    void *workbuf;                 //effects 运行句柄及buf
    u32 workbuf_size;              //当前申请的算法buf大小
    void *tmp_buf;                 //部分算法可能需要堆的临时buf
    void *cross_fade_buf;
    void *cross_fade_param;
    fade_hdl *fade;
    struct stream_frame *frame[2]; //带det输入算法
    struct effects_param parm;
    u16  tmpbuf_size;
    u16 cross_fade_param_len;
    u8 status;
    u8 output_frame;
};
struct effects_fade_parm {
    int fade_time;
    void *priv;
    int (*callback)(void *fade_hdl, int status);//淡出结束时的回调函数
};


int audio_effect_update_need_buf_check(struct audio_effects *hdl, void *param);
struct audio_effects *audio_effects_open(struct effects_param *parm);
int audio_effects_close(struct audio_effects *hdl);
int audio_effects_run(struct audio_effects *hdl, void *datain, void *dataout, u32 len);
int audio_effects_det_run(struct audio_effects *hdl, void *datain, void *det_datain, void *dataout, u32 len);
int audio_effects_update_parm(struct audio_effects *hdl, void *param, int param_len);
void audio_effects_update_param_ptr(struct audio_effects *hdl, void *param_ptr, int new_param_len);
int audio_effects_out_len(struct audio_effects *hdl, int inlen);
int audio_effects_handle_frame(struct audio_effects *hdl, struct stream_iport *iport, struct stream_note *note);
int audio_effects_update_bypass(struct audio_effects *hdl, int bypass);
//带det的算法
int audio_effects_iport_handle_frame(struct audio_effects *hdl, struct stream_iport *iport, struct stream_note *note);

//fade_out->update_param->fade_in
void audio_effects_fade_update(struct audio_effects *hdl, struct effects_fade_parm *par);

#define EFFECTS_RUN_NORMAL 0
#define EFFECTS_RUN_BYPASS           BIT(0)
#define EFFECTS_RUN_REINIT           BIT(1)
#define EFFECTS_RUN_UPDATE           BIT(2)
#define EFFECTS_RUN_CROSS_FADE_EN    BIT(3)
#define EFFECTS_RUN_CROSS_FADE_IN_EN BIT(4)  //由bypass ->run
#define EFFECTS_RUN_WORKBUF_REINIT_ERR BIT(5)//由于内存不足无法重新申请buf

//tembuf使用的位置
#define EFFECTS_SET_TMPBUF_BEF_INIT           BIT(0)
#define EFFECTS_SET_TMPBUF_BEF_RUN            BIT(1)
#define EFFECTS_SET_TMPBUF_BEF_INIT_AND_RUN   (BIT(0)|BIT(1))


//corssfade处理的位置
#define CONFIG_CROSS_FADE_POSITION_IN_BYPASS             BIT(0)
#define CONFIG_CROSS_FADE_POSITION_IN_RUN                BIT(1)
#define CONFIG_CROSS_FADE_POSITION_IN_BYPASS_AND_RUN     (BIT(1) | BIT(0))

//hardware ctrl
#define EFFECTS_HARDWARE_NEED_CLOSE   BIT(0)
#define EFFECTS_HARDWARE_TO_CLOSE     BIT(1)

#endif

