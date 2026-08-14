#ifndef RDX_MIC_GAIN_SERVICE_H
#define RDX_MIC_GAIN_SERVICE_H

#include "system/includes.h"

#define RDX_MIC_GAIN_MODE_CONFERENCE          0

int rdx_mic_gain_get_configured(u8 mode, u8 *gain);
int rdx_mic_gain_get_override(u8 mode, u8 *valid, u8 *gain);
int rdx_mic_gain_set_configured(u8 mode, u8 requested, u8 *effective);

#endif
