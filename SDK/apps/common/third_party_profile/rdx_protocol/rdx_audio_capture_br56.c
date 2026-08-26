#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE

#include "system/includes.h"
#include "rdx_audio_capture.h"
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#include "rdx_record_audio_br56.h"
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
#include "rdx_record_call_audio_br56.h"
#endif

static enum rdx_audio_capture_result rdx_audio_capture_map_result(int ret)
{
    if (!ret) {
        return RDX_AUDIO_CAPTURE_OK;
    }
    if (ret == -EBUSY) {
        return RDX_AUDIO_CAPTURE_BUSY;
    }
    if (ret == -ENODEV) {
        return RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE;
    }
    if (ret == -EINVAL) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
    return RDX_AUDIO_CAPTURE_FAILED;
}

enum rdx_audio_capture_result rdx_audio_capture_probe(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id)
{
    (void)format_id;

    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        return RDX_AUDIO_CAPTURE_OK;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        return rdx_audio_capture_map_result(
            rdx_record_call_audio_br56_probe(format_id));
#endif
    default:
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
}

enum rdx_audio_capture_result rdx_audio_capture_open(
    const struct rdx_audio_capture_open_params *params)
{
    if (!params) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }

    switch (params->scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        return rdx_audio_capture_map_result(
            rdx_record_audio_br56_open(params));
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        return rdx_audio_capture_map_result(
            rdx_record_call_audio_br56_open(params));
#endif
    default:
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
}

void rdx_audio_capture_pause(enum rdx_record_scene scene)
{
    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        rdx_record_audio_br56_close();
        break;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        rdx_record_call_audio_br56_pause();
        break;
#endif
    default:
        break;
    }
}

enum rdx_audio_capture_result rdx_audio_capture_resume(
    const struct rdx_audio_capture_open_params *params)
{
    if (!params) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }

    switch (params->scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        return rdx_audio_capture_map_result(
            rdx_record_audio_br56_open(params));
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        return rdx_audio_capture_map_result(
            rdx_record_call_audio_br56_resume(params));
#endif
    default:
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
}

void rdx_audio_capture_close(enum rdx_record_scene scene)
{
    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        rdx_record_audio_br56_close();
        break;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        rdx_record_call_audio_br56_close();
        break;
#endif
    default:
        break;
    }
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE */
