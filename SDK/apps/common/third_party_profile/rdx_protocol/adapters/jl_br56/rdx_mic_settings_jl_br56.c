#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE

#include "rdx/ports/rdx_mic_settings_port.h"
#include "rdx_protocol/adapters/jl_br56/audio/rdx_meeting_capture_jlstream.h"

int rdx_mic_settings_get_factory_gain(uint8_t *gain)
{
    return rdx_meeting_capture_jlstream_get_factory_gain(gain);
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE */
