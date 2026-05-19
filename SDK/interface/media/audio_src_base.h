/*****************************************************************
>file name : audio_src_base.h
>create time : Wed 02 Mar 2022 11:12:07 AM CST
*****************************************************************/
#ifndef _AUDIO_SRC_BASE_H_
#define _AUDIO_SRC_BASE_H_
#include "typedef.h"

#define AUDIO_ONLY_RESAMPLE         1
#define AUDIO_SYNC_RESAMPLE         2
#define AUDIO_LOW_LATENCY_RESAMPLE  3
#define AUDIO_RESAMPLE_SYNC_OUTPUT  4
#define AUDIO_SRC_HIGH_PERFORMANCE  5

#define AUDIO_SAMPLE_FMT_16BIT      0
#define AUDIO_SAMPLE_FMT_24BIT      1

#define BIND_AUDSYNC                0x10
#define SET_RESAMPLE_TYPE(fmt, type)    (((fmt) << 4) | (type))
#define RESAMPLE_TYPE_TO_FMT(a)         (((a) >> 4) & 0xf)
#define RESAMPLE_TYPE(a)                ((a) & 0xf)


#define INPUT_FRAME_BITS                            18//20 -- 整数位减少可提高单精度浮点的运算精度
#define RESAMPLE_INPUT_BIT_RANGE                    ((1 << INPUT_FRAME_BITS) - 1)
#define RESAMPLE_INPUT_BIT_NUM                      (1 << INPUT_FRAME_BITS)

struct resample_frame {
    u8 nch;
    int offset;
    int len;
    int size;
    void *data;
};

void *audio_src_base_open(u8 channel, int in_sample_rate, int out_sample_rate, u8 type);

int audio_src_base_set_output_handler(void *resample,
                                      void *priv,
                                      int (*handler)(void *priv, void *data, int len));

int audio_src_base_set_channel(void *resample, u8 channel);

int audio_src_base_set_in_buffer(void *resample, void *buf, int len);

int audio_src_base_set_input_buff(void *resample, void *buf, int len);

int audio_src_base_resample_config(void *resample, int in_rate, int out_rate);

int audio_src_base_write(void *resample, void *data, int len);

int audio_src_base_stop(void *resample);

int audio_src_base_run_scale(void *resample);

int audio_src_base_input_frames(void *resample);

u32 audio_src_base_out_frames(void *resample);

float audio_src_base_position(void *resample);

int audio_src_base_scale_output(void *resample, int in_sample_rate, int out_sample_rate, int frames);

int audio_src_base_bufferd_frames(void *resample);

int audio_src_base_set_silence(void *resample, u8 silence, int fade_time);

int audio_src_base_wait_irq_callback(void *resample, void *priv, void (*callback)(void *));

int audio_src_base_frame_resample(void *resample, struct resample_frame *in_frame, struct resample_frame *out_frame);

int audio_src_base_get_phase(void *resample);

u8 audio_src_base_get_hw_core_id(void *resample);

void audio_src_base_close(void *resample);

int audio_src_base_filter_frames(void *resample);

u8 audio_src_base_get_hw_core_id(void *resample);

int audio_src_base_push_data_out(void *resample);
#endif

