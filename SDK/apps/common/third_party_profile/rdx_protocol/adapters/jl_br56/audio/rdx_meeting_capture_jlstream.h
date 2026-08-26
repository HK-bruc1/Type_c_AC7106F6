#ifndef RDX_MEETING_CAPTURE_JLSTREAM_H
#define RDX_MEETING_CAPTURE_JLSTREAM_H

#include "rdx/ports/rdx_audio_capture_port.h"

int rdx_meeting_capture_jlstream_get_factory_gain(uint8_t *gain);
int rdx_meeting_capture_jlstream_open(
    const struct rdx_audio_capture_open_params *params);
void rdx_meeting_capture_jlstream_close(void);

#endif
