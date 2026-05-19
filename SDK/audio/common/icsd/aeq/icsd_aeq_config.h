#ifndef ICSD_AEQ_CONFIG_H
#define ICSD_AEQ_CONFIG_H

#include "generic/typedef.h"

// extern float target_headset[];

typedef struct {
    u8      iir_num_flex;
    u8      iir_num_fix;
    u8      iir_coef;
    u8      objfunc_type;
    u8      gain_limit_all;
    u8      iter_max;
    u16     fstop_idx;
    u16     fstop_idx2;


    u8      type[10];
    float   vrange_m[62];
    float   biquad_init_m[31];

    float   *weight_m;
    float   *gold_csv_m;
    float   *degree_set_0;
} __adpt_aeq_cfg;

void icsd_aeq_cfg_set(__adpt_aeq_cfg *aeq_cfg, void *_ext_cfg, u8 ch, u8 order);


#endif
