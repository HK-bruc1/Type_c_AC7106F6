#ifndef _MULTI_CH_MIX_H
#define _MULTI_CH_MIX_H

#include "effects/gain_mix_api.h"

struct mulit_mix_gain {
    float gain0;
    float gain1;
    float gain2;
};
typedef  struct _MULIT_MIX_TOOL_SET {
    int is_bypass;//打开时，只输出 gain0通道的数据
    struct mulit_mix_gain gain;
} multi_mix_param_tool_set;

#endif
