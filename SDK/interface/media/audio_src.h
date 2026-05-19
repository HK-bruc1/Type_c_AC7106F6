/*****************************************************************
>file name : audio_src.h
>author : lichao
>create time : Fri 14 Dec 2018 03:05:49 PM CST
*****************************************************************/
#ifndef _AUDIO_SRC_H_
#define _AUDIO_SRC_H_
#include "system/includes.h"
#include "audio_src_base.h"

#define SRC_TYPE_NONE               0
#define SRC_TYPE_RESAMPLE           1
#define SRC_TYPE_AUDIO_SYNC         2

struct audio_src_buffer {
    void *addr;
    int len;
};
// *INDENT-OFF*
struct audio_src_handle {
    void *base;
    struct audio_src_buffer output;
    void *output_priv;
    int (*output_handler)(void *priv, void *data, int len);
    u8 *remain_addr;
    int remain_len;
    u8 output_malloc;
};
// *INDENT-ON*

int audio_hw_src_open(struct audio_src_handle *src, u8 channel, u8 type);

int audio_hw_src_set_rate(struct audio_src_handle *src, u32 input_rate, u32 output_rate);

int audio_hw_src_bufferd_frames(struct audio_src_handle *src);

int audio_src_resample_write(struct audio_src_handle *src, void *data, int len);

void audio_src_set_output_handler(struct audio_src_handle *src, void *priv,
                                  int (*handler)(void *, void *, int));

int audio_hw_src_set_input_buffer(struct audio_src_handle *src, void *addr, int len);

int audio_hw_src_set_output_buffer(struct audio_src_handle *src, void *addr, int len);

int audio_hw_src_stop(struct audio_src_handle *src);

void audio_hw_src_close(struct audio_src_handle *src);

int audio_hw_src_trigger_resume(struct audio_src_handle *src, void *priv, void (*callback)(void *));

int audio_src_push_data_out(struct audio_src_handle *src);

u8 audio_src_get_hw_src_id(struct audio_src_handle *src);
#endif
