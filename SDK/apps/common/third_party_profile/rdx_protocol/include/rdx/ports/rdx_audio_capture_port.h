#ifndef RDX_AUDIO_CAPTURE_PORT_H
#define RDX_AUDIO_CAPTURE_PORT_H

#include "rdx/rdx_record_types.h"

enum rdx_audio_capture_result {
    RDX_AUDIO_CAPTURE_OK = 0,
    RDX_AUDIO_CAPTURE_BUSY,
    RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE,
    RDX_AUDIO_CAPTURE_UNSUPPORTED,
    RDX_AUDIO_CAPTURE_FAILED,
};

/*
 * Encoded frame data is borrowed and is valid only for the duration of the
 * callback.  A non-zero return value is owned by the Record Engine contract;
 * the Audio Capture Adapter must not retain or retry the borrowed frame.
 */
typedef int (*rdx_audio_capture_frame_callback_t)(
    void *priv,
    uint32_t engine_generation,
    uint32_t session_id,
    uint32_t capture_generation,
    const uint8_t *data,
    uint32_t len);

typedef void (*rdx_audio_capture_fault_callback_t)(
    void *priv, enum rdx_record_cause cause);

struct rdx_audio_capture_open_params {
    enum rdx_record_scene scene;
    enum rdx_record_format_id format_id;
    uint8_t mic_gain_override_valid;
    uint8_t mic_gain;
    uint32_t engine_generation;
    uint32_t session_id;
    uint32_t capture_generation;
    rdx_audio_capture_frame_callback_t frame_callback;
    rdx_audio_capture_fault_callback_t fault_callback;
    void *frame_priv;
};

/* Stable boot-time structure; must not depend on an active USB/audio source. */
enum rdx_audio_capture_result rdx_audio_capture_check_structure(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id);
/* Transient command-time conditions such as source activity or arbitration. */
enum rdx_audio_capture_result rdx_audio_capture_check_readiness(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id);
enum rdx_audio_capture_result rdx_audio_capture_open(
    const struct rdx_audio_capture_open_params *params);
void rdx_audio_capture_pause(enum rdx_record_scene scene);
enum rdx_audio_capture_result rdx_audio_capture_resume(
    const struct rdx_audio_capture_open_params *params);
void rdx_audio_capture_close(enum rdx_record_scene scene);

#endif
