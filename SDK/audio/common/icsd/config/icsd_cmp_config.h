#ifndef _CMP_CONFIG_H
#define _CMP_CONFIG_H

typedef struct {
    u8      iir_num_flex;
    u8      iir_num_fix;
    u8      iir_coef;
    u8      objfunc_type;
    u8      gain_limit_all;
    u16     fstop_idx;
    u16     fstop_idx2;


    u8      type[10];
    float   vrange_m[62];
    float   biquad_init_m[31];

    float   *weight_m;
    float   *gold_csv_m;
    float   *degree_set_0;
} __adpt_cmp_cfg;


void icsd_cmp_cfg_set(__adpt_cmp_cfg *cmp_cfg, void *_ext_cfg, u8 ch, u8 order);

//extern const u8     cmp_type[];
//extern const float  cmp_vrange_m[];
//extern const float  cmp_biquad_init_m[];
//extern const float  cmp_weight_m[];
//extern const float  cmp_gold_csv_m[];
////extern const int  cmp_iir_type[];
//extern const int    cmp_iir_num_flex;
//extern const int    cmp_iir_num_fix;
//extern const int    cmp_iir_coef;
//extern const float  cmp_fstop_idx;
//extern const float  cmp_fstop_idx2;
//extern const int    cmp_objfunc_type;
//extern const float  cmp_gain_limit_all;
//extern const float  cmp_degree_set_0[];
#endif
