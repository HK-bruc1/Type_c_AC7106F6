#ifndef _AUDIO_CBUF_PCM_DELAY_H
#define _AUDIO_CBUF_PCM_DELAY_H

#include "generic/circular_buf.h"

struct pcm_delay {
    cbuffer_t cbuffer;
    u16 delay_points;
    void *buf;
    u32 len;
};

struct pcm_delay *audio_cbuf_init(u16 delay_points, u8 bit_width, u8 ch_num);
void audio_cbuf_run(struct pcm_delay *delay, s16 *data, s16 len);
void audio_cbuf_uninit(struct pcm_delay *delay);
#endif
