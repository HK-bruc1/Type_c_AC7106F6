#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE

#include "system/includes.h"
#include "rdx_device_state.h"
#include "rdx_mic_gain_service.h"
#include "rdx_record_audio_br56.h"

int rdx_mic_gain_get_override(u8 mode, u8 *valid, u8 *gain)
{
    if (!valid || !gain || mode != RDX_MIC_GAIN_MODE_CONFERENCE) {
        return -EINVAL;
    }

    *valid = rdx_device_state_get_mic_gain_override(gain);
    if (!*valid) {
        *gain = 0;
    }
    return 0;
}

int rdx_mic_gain_get_configured(u8 mode, u8 *gain)
{
    u8 configured_gain;
    u8 override_valid;
    int ret;

    if (!gain || mode != RDX_MIC_GAIN_MODE_CONFERENCE) {
        printf("[RDX][MIC_GAIN] query result=failed reason=%s mode=%u\n",
               gain ? "unsupported_mode" : "invalid_output", mode);
        return -EINVAL;
    }

    ret = rdx_mic_gain_get_override(mode, &override_valid,
                                    &configured_gain);
    if (ret) {
        return ret;
    }
    if (override_valid) {
        *gain = configured_gain;
        printf("[RDX][MIC_GAIN] query result=ok mode=%u source=override gain=%u\n",
               mode, configured_gain);
        return 0;
    }

    ret = rdx_record_audio_br56_get_factory_gain(&configured_gain);
    if (ret) {
        printf("[RDX][MIC_GAIN] query result=failed mode=%u source=factory ret=%d\n",
               mode, ret);
        return ret;
    }

    *gain = configured_gain;
    printf("[RDX][MIC_GAIN] query result=ok mode=%u source=factory gain=%u\n",
           mode, configured_gain);
    return 0;
}

int rdx_mic_gain_set_configured(u8 mode, u8 requested, u8 *effective)
{
    u8 old_gain = 0;
    int ret;

    if (!effective || mode != RDX_MIC_GAIN_MODE_CONFERENCE
        || requested > RDX_MIC_GAIN_LEVEL_MAX) {
        printf("[RDX][MIC_GAIN] set result=failed reason=invalid mode=%u requested=%u\n",
               mode, requested);
        return -EINVAL;
    }

    ret = rdx_mic_gain_get_configured(mode, &old_gain);
    if (ret) {
        *effective = 0;
        printf("[RDX][MIC_GAIN] set result=failed reason=current_unavailable mode=%u requested=%u ret=%d\n",
               mode, requested, ret);
        return ret;
    }

    ret = rdx_device_state_set_mic_gain_override(requested);
    if (ret) {
        *effective = old_gain;
        printf("[RDX][MIC_GAIN] set result=failed reason=persist mode=%u requested=%u effective=%u ret=%d\n",
               mode, requested, old_gain, ret);
        return ret;
    }

    *effective = requested;
    printf("[RDX][MIC_GAIN] set result=ok mode=%u configured_gain=%u apply=next_session\n",
           mode, requested);
    return 0;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE */
