#ifndef RDX_RECORD_CALL_AUDIO_BR56_H
#define RDX_RECORD_CALL_AUDIO_BR56_H

#include "rdx_record_audio_br56.h"

int rdx_record_call_audio_br56_probe(
    enum rdx_record_format_id format_id);
int rdx_record_call_audio_br56_get_format(
    enum rdx_record_format_id *format_id);
int rdx_record_call_audio_br56_open(
    const struct rdx_record_audio_open_params *params);
int rdx_record_call_audio_br56_pause(void);
int rdx_record_call_audio_br56_resume(
    const struct rdx_record_audio_open_params *params);
void rdx_record_call_audio_br56_close(void);

#endif
