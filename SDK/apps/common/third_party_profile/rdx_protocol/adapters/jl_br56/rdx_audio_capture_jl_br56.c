#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE

#include "system/includes.h"
#include "rdx/ports/rdx_audio_capture_port.h"
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE && TCFG_RESOURCE_MONITOR_ENABLE
#include "resource_monitor.h"
#endif
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#include "rdx_protocol/adapters/jl_br56/audio/rdx_meeting_capture_jlstream.h"
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
#include "rdx_protocol/adapters/jl_br56/audio/rdx_call_capture_jlstream.h"
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

static void rdx_audio_capture_diag_pipeline(const char *reason)
{
#if RDX_CFG_LOCAL_STORAGE_O0_DIAG_ENABLE && TCFG_RESOURCE_MONITOR_ENABLE
    resource_monitor_snapshot(reason);
    resource_monitor_dump_key_tasks(reason);
#else
    (void)reason;
#endif
}

enum rdx_audio_capture_result rdx_audio_capture_check_structure(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id)
{
    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        return rdx_audio_capture_map_result(
            rdx_meeting_capture_jlstream_check_structure(format_id));
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        return rdx_audio_capture_map_result(
            rdx_call_capture_jlstream_check_structure(format_id));
#endif
    default:
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
}

enum rdx_audio_capture_result rdx_audio_capture_check_readiness(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id)
{
    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        return format_id == RDX_RECORD_FORMAT_MEETING_V1
               ? RDX_AUDIO_CAPTURE_OK : RDX_AUDIO_CAPTURE_UNSUPPORTED;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        return rdx_audio_capture_map_result(
            rdx_call_capture_jlstream_check_readiness(format_id));
#endif
    default:
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
}

enum rdx_audio_capture_result rdx_audio_capture_open(
    const struct rdx_audio_capture_open_params *params)
{
    enum rdx_audio_capture_result result;

    if (!params) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }

    switch (params->scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        result = rdx_audio_capture_map_result(
            rdx_meeting_capture_jlstream_open(params));
        break;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        result = rdx_audio_capture_map_result(
            rdx_call_capture_jlstream_open(params));
        break;
#endif
    default:
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
    if (result == RDX_AUDIO_CAPTURE_OK) {
        rdx_audio_capture_diag_pipeline("audio_pipeline_ready");
    }
    return result;
}

void rdx_audio_capture_pause(enum rdx_record_scene scene)
{
    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        rdx_meeting_capture_jlstream_close();
        break;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        rdx_call_capture_jlstream_pause();
        break;
#endif
    default:
        break;
    }
}

enum rdx_audio_capture_result rdx_audio_capture_resume(
    const struct rdx_audio_capture_open_params *params)
{
    enum rdx_audio_capture_result result;

    if (!params) {
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }

    switch (params->scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        result = rdx_audio_capture_map_result(
            rdx_meeting_capture_jlstream_open(params));
        break;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        result = rdx_audio_capture_map_result(
            rdx_call_capture_jlstream_resume(params));
        break;
#endif
    default:
        return RDX_AUDIO_CAPTURE_UNSUPPORTED;
    }
    if (result == RDX_AUDIO_CAPTURE_OK) {
        rdx_audio_capture_diag_pipeline("audio_pipeline_resumed");
    }
    return result;
}

void rdx_audio_capture_close(enum rdx_record_scene scene)
{
    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        rdx_meeting_capture_jlstream_close();
        break;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        rdx_call_capture_jlstream_close();
        break;
#endif
    default:
        break;
    }
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE */
