#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE

#include "rdx_mic_settings.h"
#include "rdx_record_audio_br56.h"

int rdx_mic_settings_get_factory_gain(uint8_t *gain)
{
    return rdx_record_audio_br56_get_factory_gain(gain);
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE */
