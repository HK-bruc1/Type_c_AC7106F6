#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_aeq.data.bss")
#pragma data_seg(".icsd_aeq.data")
#pragma const_seg(".icsd_aeq.text.const")
#pragma code_seg(".icsd_aeq.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"
#include "../tool/anc_ext_tool.h"

#if (TCFG_AUDIO_ADAPTIVE_EQ_ENABLE && TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2) && \
	(defined TCFG_EQ_ENABLE)

#include "icsd_aeq_config.h"

#define USE_BOARD_CONFIG 0
#define EXT_PRINTF_DEBUG 0



#if EXT_PRINTF_DEBUG
static void de_vrange_printf(float *vrange, int order)
{
    printf(" ff oreder = %d\n", order);
    for (int i = 0; i < order; i++) {
        printf("%d >>>>>>>>>>> g:%d, %d, f:%d, %d, q:%d, %d\n", i, (int)(vrange[6 * i + 0] * 1000), (int)(vrange[6 * i + 1] * 1000)
               , (int)(vrange[6 * i + 2] * 1000), (int)(vrange[6 * i + 3] * 1000)
               , (int)(vrange[6 * i + 4] * 1000), (int)(vrange[6 * i + 5] * 1000));
    }
}

static void de_biquad_printf(float *biquad, int order)
{
    printf(" ff oreder = %d\n", order);
    for (int i = 0; i < order; i++) {
        printf("%d >>>>> g:%d, f:%d, q:%d\n", i, (int)(biquad[3 * i + 0] * 1000), (int)(biquad[3 * i + 1]), (int)(biquad[3 * i + 2] * 1000));
    }
    printf("total gain = %d\n", (int)(biquad[order * 3] * 1000));
}
#endif

/***************************************************************/
//***************************** aeq ****************************/
/***************************************************************/
const int aeq_objfunc_type = 2;
const float aeq_gain_limit_all = 20;
const float aeq_degree_set_0[] = { 3, -3, 50, 2200, 50, 2700, 5,};
const float aeq_fstop_idx = 2000;
const float aeq_fstop_idx2 = 2000;
const int aeq_iter_max  = 30;

#if 0
const float aeq_vrange_m[] = {   // 62
    0, 20, 200,   500,  0.4, 1.0,
    -2, 20, 500,  800,  0.5, 1.0,
    1, 1,

    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

//aeq config
const int aeq_type[] = { 2, 2, 2, 2, 2, 2, 2, 2, 4, 2 };
const float aeq_biquad_init_m[] = { // 217
    0, 3076, 1.2,
    0, 400,  1.0,
    0, 400,  1.0,
    0, 700,  1.0,
    0, 7000, 1.0,
    0, 8000, 1.0,
    0, 9000, 1.0,
    0, 10000, 1.0,
    //
    1,  400, 0.5,
    6,  600, 1.0,
    1,
};

const float aeq_weight_m[] = { //354
    0.000, 6.000, 6.000, 6.000, 6.00,  6.00,  8.00,  8.00,  8.000, 8.000, 5.000, 5.000, 5.000, 5.000, 5.000,
    2.000, 2.000, 2.000, 2.000, 2.00,  2.00,  2.00,  2.00,  1.000, 1.000, 1.000, 0.000, 0.000, 0.000, 0.000,
    0.000, 0.000, 0.000, 0.000, 0.00,  0.00,  0.00,  0.00,  0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000,
    0.000, 0.000, 0.000, 0.000, 1.00,  0.00,  0.00,  0.00,  0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000,
};

const float aeq_gold_csv_m[] = {
    -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20,
    -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
    -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0,
    -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0,
    -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0, -0,
};

const int aeq_iir_num_flex = 2;
const int aeq_iir_num_fix  = 8;
const int aeq_iir_coef = aeq_iir_num_flex * 3 + 1;
#endif



void icsd_aeq_cfg_set(__adpt_aeq_cfg *aeq_cfg, void *_ext_cfg, u8 ch, u8 order)
{
    struct anc_ext_ear_adaptive_param *ext_cfg = (struct anc_ext_ear_adaptive_param *)_ext_cfg;

    struct __anc_ext_ear_adaptive_iir_gains *aeq_gains;
    struct __anc_ext_ear_adaptive_iir *aeq_iir;
    struct __anc_ext_ear_adaptive_weight *aeq_weight;
    struct __anc_ext_ear_adaptive_mse *aeq_mse;

    if (ch == 1) { // right channel
        aeq_gains   = ext_cfg->raeq_gains;
        aeq_iir     = ext_cfg->raeq_iir;
        aeq_weight  = ext_cfg->raeq_weight;
        aeq_mse     = ext_cfg->raeq_mse;
    } else {
        aeq_gains   = ext_cfg->aeq_gains;
        aeq_iir     = ext_cfg->aeq_iir;
        aeq_weight  = ext_cfg->aeq_weight;
        aeq_mse     = ext_cfg->aeq_mse;
    }

    aeq_cfg->objfunc_type = aeq_objfunc_type;
    aeq_cfg->gain_limit_all = aeq_gain_limit_all;
    aeq_cfg->fstop_idx = aeq_fstop_idx;
    aeq_cfg->fstop_idx2 = aeq_fstop_idx2;
    aeq_cfg->iter_max = aeq_iter_max;
    aeq_cfg->degree_set_0 = (float *)aeq_degree_set_0;

    aeq_cfg->weight_m = aeq_weight->data;
    aeq_cfg->gold_csv_m = aeq_mse->data;

    u8 biquad_idx = 0;
    u8 flex_idx = 0;

    for (int i = 0; i < order; i++) {
        if (aeq_iir[i].fixed_en) { // fix
            aeq_cfg->biquad_init_m[3 * biquad_idx + 0] = aeq_iir[i].def.gain;
            aeq_cfg->biquad_init_m[3 * biquad_idx + 1] = aeq_iir[i].def.fre;
            aeq_cfg->biquad_init_m[3 * biquad_idx + 2] = aeq_iir[i].def.q;
            aeq_cfg->type[biquad_idx] = aeq_iir[i].type;
            biquad_idx += 1;
        } else { // not fix
            aeq_cfg->vrange_m[6 * flex_idx + 0] = aeq_iir[i].lower_limit.gain;
            aeq_cfg->vrange_m[6 * flex_idx + 1] = aeq_iir[i].upper_limit.gain;
            aeq_cfg->vrange_m[6 * flex_idx + 2] = aeq_iir[i].lower_limit.fre;
            aeq_cfg->vrange_m[6 * flex_idx + 3] = aeq_iir[i].upper_limit.fre;
            aeq_cfg->vrange_m[6 * flex_idx + 4] = aeq_iir[i].lower_limit.q;
            aeq_cfg->vrange_m[6 * flex_idx + 5] = aeq_iir[i].upper_limit.q;
            flex_idx += 1;
        }
    }

    aeq_cfg->vrange_m[flex_idx * 6 + 0] = -aeq_gains->lower_limit_gain;
    aeq_cfg->vrange_m[flex_idx * 6 + 1] = -aeq_gains->upper_limit_gain;


    aeq_cfg->iir_num_flex = flex_idx;
    aeq_cfg->iir_num_fix = biquad_idx;
    aeq_cfg->iir_coef = flex_idx * 3 + 1;

    for (int i = 0; i < order; i++) {
        if (!aeq_iir[i].fixed_en) { // not fix
            aeq_cfg->biquad_init_m[3 * biquad_idx + 0] = aeq_iir[i].def.gain;
            aeq_cfg->biquad_init_m[3 * biquad_idx + 1] = aeq_iir[i].def.fre;
            aeq_cfg->biquad_init_m[3 * biquad_idx + 2] = aeq_iir[i].def.q;
            aeq_cfg->type[biquad_idx] = aeq_iir[i].type;
            biquad_idx += 1;
        }
    }

    aeq_cfg->biquad_init_m[biquad_idx * 3] = -aeq_gains->def_total_gain;

#if EXT_PRINTF_DEBUG
    de_vrange_printf(aeq_cfg->vrange_m, aeq_cfg->iir_num_flex + aeq_cfg->iir_num_fix);
    de_biquad_printf(aeq_cfg->biquad_init_m, aeq_cfg->iir_num_flex + aeq_cfg->iir_num_fix);
#endif

}

#endif

