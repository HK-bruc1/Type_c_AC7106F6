#ifndef RDX_RECORD_AUDIO_BR56_H
#define RDX_RECORD_AUDIO_BR56_H

#include "system/includes.h"
#include "rdx_record_defs.h"

typedef int (*rdx_record_audio_frame_callback_t)(
    void *priv,
    u32 engine_generation,
    u32 session_id,
    u32 capture_generation,
    const u8 *data,
    u32 len);

struct rdx_record_audio_open_params {
    enum rdx_record_format_id format_id;
    u32 engine_generation;
    u32 session_id;
    u32 capture_generation;
    rdx_record_audio_frame_callback_t frame_callback;
    void *frame_priv;
};

int rdx_record_audio_br56_open(
    const struct rdx_record_audio_open_params *params);
void rdx_record_audio_br56_close(void);

#endif
