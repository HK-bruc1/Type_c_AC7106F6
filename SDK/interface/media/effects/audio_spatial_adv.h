#ifndef _AUDIO_SPATIAL_ADV_H_
#define _AUDIO_SPATIAL_ADV_H_
#include "system/includes.h"
#include "effects/spatial_adv_api.h"

#define SW_EQ_SEG_NUM_MAX   4 //每个sw_eq的最高段数

struct sw_eq_param {
    int enable;            //eq段使能
    int freq;              //频率
    float gain;            //增益
    float q;               //Q值
    int iir_type;          //滤波器类型类型
};

struct spatial_adv_udpate {
    struct sw_eq_param eq0[SW_EQ_SEG_NUM_MAX];
    struct sw_eq_param eq1[SW_EQ_SEG_NUM_MAX];
    struct sw_eq_param eq2[SW_EQ_SEG_NUM_MAX];
    int delay;
    float gain_stereomix[4];
    float gain_bandmerge[3];
};

struct spatial_adv_param_tool_set {
    int is_bypass; // 1-> byass 0 -> no bypass
    struct spatial_adv_udpate parm;
};


#endif

