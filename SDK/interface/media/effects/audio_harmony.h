#ifndef _AUDIO_harmony_API_H_
#define _AUDIO_harmony_API_H_
#include "system/includes.h"
#include "effects/voiceChanger_api.h"

struct harmony_udpate {
    u32 effect_v;
    int keyval;
    u32 formantshift;
    u32 mode;
    u32 reserved0;
};

struct harmony_param_tool_set {
    int is_bypass; // 1-> byass 0 -> no bypass
    struct harmony_udpate parm;
};

#endif

