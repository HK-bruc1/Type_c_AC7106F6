#ifndef RDX_CALL_CAPTURE_JLSTREAM_H
#define RDX_CALL_CAPTURE_JLSTREAM_H

#include "rdx_protocol/adapters/jl_br56/audio/rdx_meeting_capture_jlstream.h"

int rdx_call_capture_jlstream_check_structure(
    enum rdx_record_format_id format_id);
int rdx_call_capture_jlstream_check_readiness(
    enum rdx_record_format_id format_id);
int rdx_call_capture_jlstream_get_format(
    enum rdx_record_format_id *format_id);
int rdx_call_capture_jlstream_open(
    const struct rdx_audio_capture_open_params *params);
int rdx_call_capture_jlstream_pause(void);
int rdx_call_capture_jlstream_resume(
    const struct rdx_audio_capture_open_params *params);
void rdx_call_capture_jlstream_close(void);

#endif
