#ifndef _AUDIO_vibrato_API_H_
#define _AUDIO_vibrato_API_H_
#include "system/includes.h"
#include "effects/voiceChanger_api.h"

struct vibrato_udpate {
    u32 amplitude;          //范围 0到10000，单位%% 默认值300
    u32 period;             //范围 0到 10000，单位ms，默认值200
    u32 reserved0;
};

struct vibrato_param_tool_set {
    int is_bypass; // 1-> byass 0 -> no bypass
    struct vibrato_udpate parm;
};

#endif

