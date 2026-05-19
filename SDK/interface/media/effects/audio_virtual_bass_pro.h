#ifndef _AUDIO_VIRTUAL_BASS_PRO_API_H_
#define _AUDIO_VIRTUAL_BASS_PRO_API_H_
#include "system/includes.h"
#include "effects/virtual_bass_pro_api.h"

struct virtual_bass_pro_udpate {
    int fc;
    int wet_low_pass_fc;
    int wet_high_pass_fc;
    int dry_high_pass_fc;
    float dry_gain;


    float compressor_attack_time;
    float compressor_release_time;
    float wet_compressor_threshold;
    float wet_compressor_ratio;
    float wet_gain;


    int noisegate_attack_time;
    int noisegate_release_time;
    int noisegate_attack_hold_time;
    float noisegate_threshold;
    int resever[10];
};

struct virtual_bass_pro_param_tool_set {
    int is_bypass; // 1-> byass 0 -> no bypass
    struct virtual_bass_pro_udpate parm;
};

#endif

