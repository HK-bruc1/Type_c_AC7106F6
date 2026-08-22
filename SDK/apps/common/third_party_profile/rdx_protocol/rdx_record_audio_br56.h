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

typedef void (*rdx_record_audio_fault_callback_t)(
    void *priv, enum rdx_record_cause cause);

struct rdx_record_audio_open_params {
    enum rdx_record_format_id format_id;
    u8 mic_gain_override_valid;
    u8 mic_gain;
    u32 engine_generation;
    u32 session_id;
    u32 capture_generation;
    rdx_record_audio_frame_callback_t frame_callback;
    rdx_record_audio_fault_callback_t fault_callback;
    void *frame_priv;
};

int rdx_record_audio_br56_get_factory_gain(u8 *gain);
int rdx_record_audio_br56_open(
    const struct rdx_record_audio_open_params *params);
void rdx_record_audio_br56_close(void);

#endif
