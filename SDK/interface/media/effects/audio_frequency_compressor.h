#ifndef _AUDIO_FREQUENCY_COMPRESSOR_API_H_
#define _AUDIO_FREQUENCY_COMPRESSOR_API_H_
#include "system/includes.h"
#include "effects/frequency_compressor_api.h"


struct frequency_compressor_param_tool_set {
    int is_bypass; // 1-> byass 0 -> no bypass
    int nSection;
    float detect_time;// 0~20ms, 默认0
    struct frequency_compressor_effect parm[4];
};

#endif

