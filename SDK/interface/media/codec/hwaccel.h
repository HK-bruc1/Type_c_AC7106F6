/*****************************************************************
>file name : media/include/audio/hwaccel.h
>author : lichao
>create time : Tue 27 Nov 2018 04:29:42 PM CST
*****************************************************************/
#ifndef __AUDIO_HWACCEL_H__
#define __AUDIO_HWACCEL_H__
#include "generic/typedef.h"

#define MSBC_ENC_FRAME_LEN       58
#define MSBC_ENC_PCM_LEN         240

enum {
    ERR_SBC_CODEC_FINISH = 0,	/*正常解码完成*/
    ERR_SBC_CODEC_EXIT,			/*准备退出解码*/
    ERR_SBC_CODEC_IN_REMAIN,	/*input还没消耗完*/
    ERR_SBC_CODEC_OUT_REMAIN,	/*output还没消耗完*/
};

struct audio_stream_ops {
    void *(*alloc)(void *priv, int size);
    void *(*alloc_free_space)(void *priv, int *size);
    int (*finish)(void *priv, void *buf, int size);
};


#define AUDIO_HW_ENC        0
#define AUDIO_HW_DEC        1

/*#define PCM_FMT_ORIG_CH     0*/
/*#define PCM_FMT_L_CH        0
#define PCM_FMT_R_CH        1
#define PCM_FMT_MONO        2
#define PCM_FMT_MONO_LR_CH  3*/

struct audio_hw_codec_info {
    const char *format;
    u8 pcm_format;
    u8 mode; //0 - enc, 1 - dec
    void *priv;
    const struct audio_stream_ops *ops;
};

struct audio_codec_frame {
    void *data;
    int   size; //bytes
    int   done_size; //bytes
};

struct audio_hwaccel_ops {
    const char *name;
    void *(*open)(struct audio_hw_codec_info *info);
    int (*encode_frame)(void *hdl, struct audio_codec_frame *iframe, struct audio_codec_frame *oframe);
    int (*decode_frame)(void *hdl, struct audio_codec_frame *iframe, struct audio_codec_frame *oframe);
    int (*encode)(void *hdl, void *src, int len);
    int (*decode)(void *hdl, void *buf, int len);
    int (*decode_stop)(void *hdl);
    int (*close)(void *hdl);
    int (*reset)(void *hdl);
};

extern struct audio_hwaccel_ops audio_hwaccel_begin[];
extern struct audio_hwaccel_ops audio_hwaccel_end[];

#define REGISTER_AUDIO_CODEC_HWACCEL(ops) \
    struct audio_hwaccel_ops ops SEC(.audio_hwaccel) = { \

#define list_for_each_codec_hwaccel(p) \
    for (p = audio_hwaccel_begin; p < audio_hwaccel_end; p++)

#endif
