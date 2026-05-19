#ifndef _AUDIO_VIRTUAL_SURROUND_HP_H_
#define _AUDIO_VIRTUAL_SURROUND_HP_H_
#include "system/includes.h"
#include "effects/spatial_brir_ctrl.h"
#include "effects/StereoToLCR_Lib.h"


struct virtual_surround_hp_udpate {
    //环境声渲染
    int render_angle;
    int crosstalk_mode;
    float input_gain;
    float output_gain;
    float float_rev1;
    int int_rev2;
    //upmix分解
    int frequency_cut_low;
    int frequency_cut_high;
    float middle_extract_ratio;
    int run_model_512;
    int enable_smooth;
    float smooth_coeff;
    //delay
    float delay_ms;
};

struct virtual_surround_hp_param_tool_set {
    int is_bypass;
    struct virtual_surround_hp_udpate parm;
};


#endif

