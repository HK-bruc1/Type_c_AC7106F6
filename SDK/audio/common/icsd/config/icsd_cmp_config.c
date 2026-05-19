
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_anc.data.bss")
#pragma data_seg(".icsd_anc.data")
#pragma const_seg(".icsd_anc.text.const")
#pragma code_seg(".icsd_anc.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if (TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN && \
	 TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN)

#include "audio_anc.h"
#include "icsd_cmp_config.h"
#include "icsd_common_v2.h"

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
//***************************** cmp ****************************/
/***************************************************************/
const int cmp_objfunc_type = 1;
const float cmp_gain_limit_all = 30;
const float cmp_fstop_idx = 2000;
const float cmp_fstop_idx2 = 2000;
const float cmp_degree_set_0[] = {3, -3, 50, 2200, 50, 2700, 5};

#if 0
const u8 cmp_type[] = { 3, 2, 4, 2, 2, 2, 2, 2, 2, 2 };

const float cmp_vrange_m[] = {   // 62
    -10, 20, 20,   100,  0.8, 1.2,
    -10, 20, 100,  300,  0.8, 1.2,
    -10, 20, 300,  800,  0.6, 1.1,
    -10, 20, 800,  1500,  0.8, 1.2,
    1.2, 0.1,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};
const float cmp_biquad_init_m[] = { // 217
    -4.67, 3076, 1.2,
    -3.874,  6506,  1.0,
    //
    0.75, 50, 1.0,
    3.81,  202, 1.0,
    1.59,  410, 0.8,
    0.40,  888,  0.7,
    0.307946,
};

const float cmp_weight_m[] = { //354
    5.000, 5.000, 5.000, 5.000, 5.00,  5.00,  5.00,  5.00,  5.000, 5.000, 5.000, 5.000, 5.000, 5.000, 5.000,
    5.000, 5.000, 5.000, 5.000, 5.00,  5.00,  5.00,  5.00,  5.000, 5.000, 5.000, 5.000, 5.000, 5.000, 5.000,
    5.000, 5.000, 5.000, 5.000, 5.00,  5.00,  5.00,  5.00,  5.000, 5.000, 5.000, 5.000, 5.000, 5.000, 5.000,
    2.000, 2.000, 2.000, 2.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000,
};
const float cmp_gold_csv_m[] = {
    -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20,
    -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20,
    -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20,
    -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20,
    -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20, -20,
};


const int cmp_iir_num_flex = 4;
const int cmp_iir_num_fix  = 2;
const int cmp_iir_coef = cmp_iir_num_flex * 3 + 1;
#endif


void icsd_cmp_cfg_set(__adpt_cmp_cfg *cmp_cfg, void *_ext_cfg, u8 ch, u8 order)
{
    struct anc_ext_ear_adaptive_param *ext_cfg = (struct anc_ext_ear_adaptive_param *)_ext_cfg;

    struct __anc_ext_ear_adaptive_iir_gains *cmp_gains;
    struct __anc_ext_ear_adaptive_iir *cmp_iir;
    struct __anc_ext_ear_adaptive_weight *cmp_weight;
    struct __anc_ext_ear_adaptive_mse *cmp_mse;

    if (ch == 1) { // right channel
        cmp_gains   = ext_cfg->rcmp_gains;
        cmp_iir     = ext_cfg->rcmp_iir;
        cmp_weight  = ext_cfg->rcmp_weight;
        cmp_mse     = ext_cfg->rcmp_mse;
    } else {
        cmp_gains   = ext_cfg->cmp_gains;
        cmp_iir     = ext_cfg->cmp_iir;
        cmp_weight  = ext_cfg->cmp_weight;
        cmp_mse     = ext_cfg->cmp_mse;
    }

    cmp_cfg->objfunc_type = cmp_objfunc_type;
    cmp_cfg->gain_limit_all = cmp_gain_limit_all;
    cmp_cfg->fstop_idx = cmp_fstop_idx;
    cmp_cfg->fstop_idx2 = cmp_fstop_idx2;
    cmp_cfg->degree_set_0 = (float *)cmp_degree_set_0;

    cmp_cfg->weight_m = cmp_weight->data;
    cmp_cfg->gold_csv_m = cmp_mse->data;

    u8 biquad_idx = 0;
    u8 flex_idx = 0;

    for (int i = 0; i < order; i++) {
        if (cmp_iir[i].fixed_en) { // fix
            cmp_cfg->biquad_init_m[3 * biquad_idx + 0] = cmp_iir[i].def.gain;
            cmp_cfg->biquad_init_m[3 * biquad_idx + 1] = cmp_iir[i].def.fre;
            cmp_cfg->biquad_init_m[3 * biquad_idx + 2] = cmp_iir[i].def.q;
            cmp_cfg->type[biquad_idx] = cmp_iir[i].type;
            biquad_idx += 1;
        } else { // not fix
            cmp_cfg->vrange_m[6 * flex_idx + 0] = cmp_iir[i].lower_limit.gain;
            cmp_cfg->vrange_m[6 * flex_idx + 1] = cmp_iir[i].upper_limit.gain;
            cmp_cfg->vrange_m[6 * flex_idx + 2] = cmp_iir[i].lower_limit.fre;
            cmp_cfg->vrange_m[6 * flex_idx + 3] = cmp_iir[i].upper_limit.fre;
            cmp_cfg->vrange_m[6 * flex_idx + 4] = cmp_iir[i].lower_limit.q;
            cmp_cfg->vrange_m[6 * flex_idx + 5] = cmp_iir[i].upper_limit.q;
            flex_idx += 1;
        }
    }

    cmp_cfg->vrange_m[flex_idx * 6 + 0] = -cmp_gains->upper_limit_gain;
    cmp_cfg->vrange_m[flex_idx * 6 + 1] = -cmp_gains->lower_limit_gain;


    cmp_cfg->iir_num_flex = flex_idx;
    cmp_cfg->iir_num_fix = biquad_idx;
    cmp_cfg->iir_coef = flex_idx * 3 + 1;

    for (int i = 0; i < order; i++) {
        if (!cmp_iir[i].fixed_en) { // not fix
            cmp_cfg->biquad_init_m[3 * biquad_idx + 0] = cmp_iir[i].def.gain;
            cmp_cfg->biquad_init_m[3 * biquad_idx + 1] = cmp_iir[i].def.fre;
            cmp_cfg->biquad_init_m[3 * biquad_idx + 2] = cmp_iir[i].def.q;
            cmp_cfg->type[biquad_idx] = cmp_iir[i].type;
            biquad_idx += 1;
        }
    }

    cmp_cfg->biquad_init_m[biquad_idx * 3] = -cmp_gains->def_total_gain;

#if EXT_PRINTF_DEBUG
    de_vrange_printf(cmp_cfg->vrange_m, cmp_cfg->iir_num_flex + cmp_cfg->iir_num_fix);
    de_biquad_printf(cmp_cfg->biquad_init_m, cmp_cfg->iir_num_flex + cmp_cfg->iir_num_fix);
#endif

}

#endif
