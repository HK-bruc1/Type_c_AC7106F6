#include <string.h>

#include "rdx_audio_capture_fake.h"

static struct rdx_audio_capture_fake_state fake_capture;

static uint8_t rdx_audio_capture_fake_params_valid(
    const struct rdx_audio_capture_open_params *params)
{
    if (!params || !params->frame_callback || !params->engine_generation
        || !params->session_id || !params->capture_generation) {
        return 0;
    }
    if (params->scene == RDX_RECORD_SCENE_MEETING) {
        return params->format_id == RDX_RECORD_FORMAT_MEETING_V1;
    }
    if (params->scene == RDX_RECORD_SCENE_CALL) {
        return params->format_id == RDX_RECORD_FORMAT_CALL_V1;
    }
    return 0;
}

void rdx_audio_capture_fake_reset(void)
{
    memset(&fake_capture, 0, sizeof(fake_capture));
    fake_capture.structure_result = RDX_AUDIO_CAPTURE_OK;
    fake_capture.readiness_result = RDX_AUDIO_CAPTURE_OK;
    fake_capture.open_result = RDX_AUDIO_CAPTURE_OK;
}

const struct rdx_audio_capture_fake_state *
rdx_audio_capture_fake_get_state(void)
{
    return &fake_capture;
}

void rdx_audio_capture_fake_set_structure_result(
    enum rdx_audio_capture_result result)
{
    fake_capture.structure_result = result;
}

void rdx_audio_capture_fake_set_readiness_result(
    enum rdx_audio_capture_result result)
{
    fake_capture.readiness_result = result;
}

void rdx_audio_capture_fake_set_open_result(
    enum rdx_audio_capture_result result)
{
    fake_capture.open_result = result;
}

int rdx_audio_capture_fake_emit_frame(const uint8_t *data, uint32_t len)
{
    if (!fake_capture.active || !data || !len) {
        return -1;
    }
    return fake_capture.params.frame_callback(
        fake_capture.params.frame_priv,
        fake_capture.params.engine_generation,
        fake_capture.params.session_id,
        fake_capture.params.capture_generation,
        data, len);
}

int rdx_audio_capture_fake_emit_fault(enum rdx_record_cause cause)
{
    if (!fake_capture.active || !fake_capture.params.fault_callback) {
        return -1;
    }
    fake_capture.params.fault_callback(fake_capture.params.frame_priv, cause);
    return 0;
}

static uint8_t rdx_audio_capture_fake_format_supported(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id)
{
    if ((scene == RDX_RECORD_SCENE_MEETING
         && format_id != RDX_RECORD_FORMAT_MEETING_V1)
        || (scene == RDX_RECORD_SCENE_CALL
            && format_id != RDX_RECORD_FORMAT_CALL_V1)
        || (scene != RDX_RECORD_SCENE_MEETING
            && scene != RDX_RECORD_SCENE_CALL)) {
        return 0;
    }
    return 1;
}

enum rdx_audio_capture_result rdx_audio_capture_check_structure(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id)
{
    if (!rdx_audio_capture_fake_format_supported(scene, format_id)) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
    return fake_capture.structure_result;
}

enum rdx_audio_capture_result rdx_audio_capture_check_readiness(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id)
{
    if (!rdx_audio_capture_fake_format_supported(scene, format_id)) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
    return fake_capture.readiness_result;
}

enum rdx_audio_capture_result rdx_audio_capture_open(
    const struct rdx_audio_capture_open_params *params)
{
    if (!rdx_audio_capture_fake_params_valid(params)) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
    if (fake_capture.active || fake_capture.paused) {
        return RDX_AUDIO_CAPTURE_BUSY;
    }
    if (fake_capture.open_result != RDX_AUDIO_CAPTURE_OK) {
        return fake_capture.open_result;
    }

    fake_capture.params = *params;
    fake_capture.active = 1;
    fake_capture.open_count++;
    return RDX_AUDIO_CAPTURE_OK;
}

void rdx_audio_capture_pause(enum rdx_record_scene scene)
{
    if (!fake_capture.active || fake_capture.params.scene != scene) {
        return;
    }
    fake_capture.active = 0;
    fake_capture.paused = 1;
    fake_capture.pause_count++;
}

enum rdx_audio_capture_result rdx_audio_capture_resume(
    const struct rdx_audio_capture_open_params *params)
{
    if (!rdx_audio_capture_fake_params_valid(params)) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
    if (fake_capture.active || !fake_capture.paused
        || fake_capture.params.scene != params->scene) {
        return RDX_AUDIO_CAPTURE_BUSY;
    }
    if (fake_capture.open_result != RDX_AUDIO_CAPTURE_OK) {
        return fake_capture.open_result;
    }

    fake_capture.params = *params;
    fake_capture.active = 1;
    fake_capture.paused = 0;
    fake_capture.resume_count++;
    return RDX_AUDIO_CAPTURE_OK;
}

void rdx_audio_capture_close(enum rdx_record_scene scene)
{
    if ((!fake_capture.active && !fake_capture.paused)
        || fake_capture.params.scene != scene) {
        return;
    }
    fake_capture.active = 0;
    fake_capture.paused = 0;
    memset(&fake_capture.params, 0, sizeof(fake_capture.params));
    fake_capture.close_count++;
}
