#ifndef RDX_MIC_GAIN_SERVICE_H
#define RDX_MIC_GAIN_SERVICE_H

#include <stdint.h>

#define RDX_MIC_GAIN_MODE_CONFERENCE          0

int rdx_mic_gain_get_configured(uint8_t mode, uint8_t *gain);
int rdx_mic_gain_get_override(uint8_t mode, uint8_t *valid,
                              uint8_t *gain);
int rdx_mic_gain_set_configured(uint8_t mode, uint8_t requested,
                                uint8_t *effective);

#endif
