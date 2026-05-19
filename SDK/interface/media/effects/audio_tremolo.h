#ifndef _AUDIO_tremolo_API_H_
#define _AUDIO_tremolo_API_H_
#include "system/includes.h"
#include "effects/tremble_api.h"

struct tremolo_udpate {
    int LRdiffphase;
    int waveType;
    int outgain;
    float freq[TREM_N];
    int gain[TREM_N];
    int reserved;
};

struct tremolo_param_tool_set {
    int is_bypass; // 1-> byass 0 -> no bypass
    struct tremolo_udpate parm;
};

#endif

