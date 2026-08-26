#ifndef RDX_AUDIO_CAPTURE_FAKE_H
#define RDX_AUDIO_CAPTURE_FAKE_H

#include "rdx/ports/rdx_audio_capture_port.h"

struct rdx_audio_capture_fake_state {
    struct rdx_audio_capture_open_params params;
    enum rdx_audio_capture_result structure_result;
    enum rdx_audio_capture_result readiness_result;
    enum rdx_audio_capture_result open_result;
    unsigned int open_count;
    unsigned int pause_count;
    unsigned int resume_count;
    unsigned int close_count;
    uint8_t active;
    uint8_t paused;
};

void rdx_audio_capture_fake_reset(void);
const struct rdx_audio_capture_fake_state *
rdx_audio_capture_fake_get_state(void);
void rdx_audio_capture_fake_set_structure_result(
    enum rdx_audio_capture_result result);
void rdx_audio_capture_fake_set_readiness_result(
    enum rdx_audio_capture_result result);
void rdx_audio_capture_fake_set_open_result(
    enum rdx_audio_capture_result result);
int rdx_audio_capture_fake_emit_frame(const uint8_t *data, uint32_t len);
int rdx_audio_capture_fake_emit_fault(enum rdx_record_cause cause);

#endif
