#ifndef RDX_RECORD_AUDIO_BR56_H
#define RDX_RECORD_AUDIO_BR56_H

#include "rdx_audio_capture.h"

int rdx_record_audio_br56_get_factory_gain(uint8_t *gain);
int rdx_record_audio_br56_open(
    const struct rdx_audio_capture_open_params *params);
void rdx_record_audio_br56_close(void);

#endif
