#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_anc.data.bss")
#pragma data_seg(".icsd_anc.data")
#pragma const_seg(".icsd_anc.text.const")
#pragma code_seg(".icsd_anc.text")
#endif

#define USE_BOARD_CONFIG 0
#define EXT_PRINTF_DEBUG 0

#include "app_config.h"
#include "audio_config_def.h"
#if ((TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN || TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE) && \
	 TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_EAR_ADAPTIVE_VERSION == ANC_EXT_V2)

#include "audio_anc_includes.h"
#include "icsd_common_v2.h"

#if (USE_BOARD_CONFIG == 1)
#include "icsd_anc_v2_board.c"
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
/*
   RTANC 默认配置：
   		固定高频滤波器，左右独立收敛性能
*/
#define ANC_ADAPTIVE_HIGH_FILTER_FIX_FGQ	1	//固定高频滤波器
#define ANC_ADAPPTIVE_DE_ALG_SEL			1	//左右独立收敛到最优, 不做平衡

#else
/*
   耳道自适应 默认配置：
   		不固定高频滤波器，平衡左右性能
*/
#define ANC_ADAPTIVE_HIGH_FILTER_FIX_FGQ	0
#define ANC_ADAPPTIVE_DE_ALG_SEL			0
#endif

const u8 ICSD_ANC_VERSION = 2;
const u8 ICSD_ANC_TOOL_PRINTF = 0;
const u8 msedif_en = 0;
const u8 target_diff_en = 0;

struct icsd_anc_v2_tool_data *TOOL_DATA = NULL;

const int cmp_idx_begin = 0;
const int cmp_idx_end = 59;
const int cmp_total_len = 60;
const int cmp_en = 0;
const float pz_gain = 0;
const u8 ICSD_ANC_V2_BYPASS_ON_FIRST = 0;//播放提示音过程中打开BYPASS节省BYPASS稳定时间


void fgq_getfrom_table(s16 *fgq_table, u8 idx, float *fgq)
{
    u8 idx_div2 = idx / 2;
    s16 *fgq_sel = &fgq_table[31 * idx_div2];

    fgq[0] = fgq_sel[0] / 1e3;
    for (int i = 0; i < 10; i++) {
        fgq[3 * i + 1] = fgq_sel[3 * i + 1];
        fgq[3 * i + 2] = fgq_sel[3 * i + 2] / 1e2;
        fgq[3 * i + 3] = fgq_sel[3 * i + 3] / 1e3;
    }

#if 0
    printf("total_gian:%d\n", (int)(fgq[0] * 1e3));
    for (int i = 0; i < 10; i++) {
        printf("fgq:%d, %d, %d\n", (int)(fgq[3 * i + 1]), (int)(fgq[3 * i + 2] * 1e3), (int)(fgq[3 * i + 3] * 1e3));
    }
#endif

}

/***************************************************************/
/****************** ANC MODE bypass fgq ************************/
/***************************************************************/
const float gfq_bypass[] = {
    -10,  5000,  1,
};
const u8 tap_bypass = 1;
const u8 type_bypass[] = {3};
const float fs_bypass = 375e3;
double bbbaa_bypass[1 * 5];

void icsd_anc_v2_board_config()
{
    double a0, a1, a2, b0, b1, b2;
    for (int i = 0; i < tap_bypass; i++) {
        icsd_biquad2ab_out_v2(gfq_bypass[3 * i], gfq_bypass[3 * i + 1], fs_bypass, gfq_bypass[3 * i + 2], &a0, &a1, &a2, &b0, &b1, &b2, type_bypass[i]);
        bbbaa_bypass[5 * i + 0] = b0 / a0;
        bbbaa_bypass[5 * i + 1] = b1 / a0;
        bbbaa_bypass[5 * i + 2] = b2 / a0;
        bbbaa_bypass[5 * i + 3] = a1 / a0;
        bbbaa_bypass[5 * i + 4] = a2 / a0;
    }
}

/***************************************************************/
/**************************** FF *******************************/
/***************************************************************/
const int FF_objFunc_type  = 3;

const float FSTOP_IDX = 2700;
const float FSTOP_IDX2 = 2700;

const float Gold_csv_Perf_Range[] = {
    2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,
    2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,
    2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,
    2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,   2.0,
};

//degree_level, gain_limit, limit_mse_begin, limit_mse_end, over_mse_begin, over_mse_end,biquad_cut
float degree_set0[] = {3, -3, 50, 2700, 50, 2700, 5};
float degree_set1[] = {8, 10, 50, 2700, 50, 2700, 8};
float degree_set2[] = {11, 5, 50, 2700, 50, 2700, 11};

const u8 mem_list[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 21, 23, 25, 27, 31, 35, 39, 43, 51, 59};
extern const float ff_filter[];

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

// ref_gain: liner
// err_gain: liner
// eq_freq range: 40 --> 2.7khz
static void szpz_cmp_get(float *pz_cmp, float *sz_cmp, float ref_gain, float err_gain, struct aeq_default_seg_tab *eq_par, float eq_freq_l, float eq_freq_h, u8 eq_total_gain_en)
{
    const int len = 25;
    float *freq = anc_malloc("ICSD_ANC_CONFIG", len * 4);
    ASSERT(freq, "freq alloc err\n");
    float *eq_hz = anc_malloc("ICSD_ANC_CONFIG", len * 8);
    ASSERT(eq_hz, "eq_hz alloc err\n");
    float *iir_ab = anc_malloc("ICSD_ANC_CONFIG", 5 * 10 * 4);	 // eq tap <= 10
    ASSERT(iir_ab, "iir_ab alloc err\n");

    ASSERT(pz_cmp, "pz_cmp input err\n");
    ASSERT(sz_cmp, "sz_cmp input err\n");

    ref_gain = 1.0f / ref_gain;  //= test_cur /  gold_cur
    err_gain = 1.0f / err_gain;  //= test_cur /  gold_cur

    for (int i = 0; i < len; i++) {
        freq[i] = 46875.0 / 1024 * mem_list[i];
        pz_cmp[2 * i + 0] = 1;
        pz_cmp[2 * i + 1] = 0;
        sz_cmp[2 * i + 0] = 1;
        sz_cmp[2 * i + 1] = 0;
    }

    u8 eq_idx_l = 0;
    u8 eq_idx_h = 0;
    for (int i = 0; i < len - 1; i++) {
        if ((eq_freq_l > freq[i]) && (eq_freq_l <= freq[i + 1])) {
            eq_idx_l = i;
        }
        if ((eq_freq_h > freq[i]) && (eq_freq_h <= freq[i + 1])) {
            eq_idx_h = i;
        }
    }
    //printf("freq:%d, %d; ind:%d, %d\n", (int)eq_freq_l, (int)eq_freq_h, eq_idx_l, eq_idx_h);
    if (eq_par) {
        icsd_aeq_fgq2eq(eq_par, iir_ab, eq_hz, freq, 46875, len * 2);
    } else {
        for (int i = 0; i < len; i++) {
            eq_hz[2 * i + 0] = 1;
            eq_hz[2 * i + 1] = 0;
        }
    }

    //for(int i=0; i<len; i++){
    //    printf("%d, %d\n", (int)(eq_hz[2*i]*1e6), (int)(eq_hz[2*i+1]));
    //}


    if (eq_total_gain_en) {
        float eq_gain = 1;
        float cnt = 0;
        float db = 0;
        for (int i = eq_idx_l; i < eq_idx_h; i++) {
            db += 10 * icsd_log10_anc(eq_hz[2 * i] * eq_hz[2 * i] + eq_hz[2 * i + 1] * eq_hz[2 * i + 1]);
            cnt = cnt + 1;
        }
        if (cnt != 0) {
            db = db / cnt;
        }
        eq_gain = eq_gain * icsd_anc_pow10(db / 20);

        float pz_cmp_gain = err_gain;
        float sz_cmp_gain = err_gain;

        if (ref_gain != 0) {
            pz_cmp_gain = err_gain / ref_gain;
        }

        if (eq_gain != 0) {
            sz_cmp_gain = err_gain / eq_gain;
        }

        for (int i = 0; i < len; i++) {
            pz_cmp[2 * i + 0] = pz_cmp_gain;
            pz_cmp[2 * i + 1] = 0;
            sz_cmp[2 * i + 0] = sz_cmp_gain;
            sz_cmp[2 * i + 1] = 0;
        }
    } else {
        float err_tmp[2];
        err_tmp[0] = err_gain;
        err_tmp[1] = 0;

        for (int i = 0; i < len; i++) {
            pz_cmp[2 * i + 0] = err_gain / ref_gain;
            pz_cmp[2 * i + 1] = 0;
            icsd_complex_div_v2(err_tmp, &eq_hz[2 * i], &sz_cmp[2 * i], 2);
        }
    }

    anc_free(freq);
    anc_free(eq_hz);
    anc_free(iir_ab);

}

// 产测相关
static const u8 biquad_type_gold[] = {3, 2, 2, 4, 2, 2, 2, 2, 2, 2};

static const float vrange_gold[] = {
    1, 6.5, 50, 70, 1, 1,
    -1, 3, 80, 120, 0.55, 0.75,
    -6, -2.5, 130, 170, 0.25, 0.45,
    -5, -1.5, 300, 500, 0.68, 0.88,
    -3, 0.5, 900, 1100, 0.55, 0.75,
    -2, -2, 1700, 2100, 0.62, 0.82,
    17.7,  17.8, 3600, 3900, 1.2, 1.4,
    -0.4, -0.6,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

static const float biquad_gold[] = {
    13.000000, 17093.000000, 0.500000,
    -22.348000, 6186.000000, 1.400000,
    -8.000000, 7369.000000, 5.000000,
    6.121107, 50.008835, 1.000000,
    -0.540640, 80.892845, 0.644431,
    -2.918771, 150.000000, 0.329090,
    -2.068776, 403.170227, 0.787310,
    -0.838458, 985.938599, 0.650040,
    -2.044488, 1990.087646, 0.726398,
    17.727093, 3810.000000, 1.287747,
    -0.4566
};

static const float mse_gold[] = {
    -8.000000, -7.948685, -10.86845, -15.64812, -18.40401, -19.49375, -19.85448, -19.93217, -19.88426, -19.76206, -19.58076, -19.34377, -19.05159, -18.70536, -18.30807,
    -17.86478, -17.38221, -16.86805, -16.33033, -15.77681, -15.21462, -14.65002, -14.08835, -13.53403, -12.99070, -12.46129, -11.94816, -11.45323, -10.97807, -10.52400,
    -10.09221, -9.683781, -9.299810, -8.941458, -8.610031, -8.307064, -8.034416, -7.794382, -7.589824, -7.424325, -7.302374, -7.229548, -7.212703, -7.260058, -7.381096,
    -7.585963, -7.883880, -8.279675, -8.767015, -9.316906, -9.862436, -10.29029, -10.46497, -10.30011, -9.821467, -9.143281, -8.390392, -7.649750, -6.966335, -6.356695,
};

static const float weight_gold[] = {
    0.00, 10.0,   45.0,   45.0,   45.0,   45.0,   45.0,   45.0,   45.0,   55.0,   55.0,   55.0,   55.0,   55.0,   55.0,
    55.0, 55.0,   55.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,
    50.0, 50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,
    50.0, 50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   50.0,   0.00,
};

static const float degree_set_gold[] = {11, 30, 50, 2700, 50, 2700, 5};


float gfq_default[] = { // TODO
    0.354,
    -15, 22000, 1,
    -21,  6000, 1.5,
    -15, 12000, 3,
    -6,     46, 1,
    0,     64, 1,
    -4,    120, 0.54,
    -2.4,   600, 0.3,
    0,    900, 0.9,
    1,   1800, 1,
    8.3, 2510, 1,
};
float gfq_default_r[] = { // TODO
    0.354,
    -15, 22000, 1,
    -21,  6000, 1.5,
    -15, 12000, 3,
    -6,     46, 1,
    0,     64, 1,
    -4,    120, 0.54,
    -2.4,   600, 0.3,
    0,    900, 0.9,
    1,   1800, 1,
    8.3, 2510, 1,
};


void icsd_sd_cfg_set(__icsd_anc_config_data *SD_CFG, void *_ext_cfg)
{
    icsd_common_version();
    struct anc_ext_ear_adaptive_param *ext_cfg = (struct anc_ext_ear_adaptive_param *)_ext_cfg;
    struct rt_anc_tmp_buffer *tmp_buff;
#if (USE_BOARD_CONFIG == 1)

    printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>anc use board config\n");
    icsd_anc_config_board_init(SD_CFG);

    return ;
#endif
    if (SD_CFG) {
        //界面信息配置
        SD_CFG->pnc_times = ext_cfg->base_cfg->adaptive_times;
        SD_CFG->vld1 = ext_cfg->base_cfg->vld1;
        SD_CFG->vld2 = ext_cfg->base_cfg->vld2;
        SD_CFG->sz_pri_thr = ext_cfg->base_cfg->sz_pri_thr;
        SD_CFG->bypass_vol = ext_cfg->base_cfg->bypass_vol;
        SD_CFG->sz_calr_sign = ext_cfg->base_cfg->calr_sign[0];
        SD_CFG->pz_calr_sign = ext_cfg->base_cfg->calr_sign[1];
        SD_CFG->bypass_calr_sign = ext_cfg->base_cfg->calr_sign[2];
        SD_CFG->perf_calr_sign = ext_cfg->base_cfg->calr_sign[3];
        SD_CFG->train_mode = ext_cfg->train_mode;	//自适应训练模式设置 */
        SD_CFG->tonel_delay = ext_cfg->base_cfg->tonel_delay;
        SD_CFG->toner_delay = ext_cfg->base_cfg->toner_delay;
        SD_CFG->pzl_delay = ext_cfg->base_cfg->pzl_delay;
        SD_CFG->pzr_delay = ext_cfg->base_cfg->pzr_delay;
        SD_CFG->ear_recorder = ext_cfg->ff_ear_mem_param->ear_recorder;
        SD_CFG->fb_agc_en = 0;
        //其他配置
        SD_CFG->ff_yorder  = ANC_ADAPTIVE_FF_ORDER;
        SD_CFG->fb_yorder  = ANC_ADAPTIVE_FB_ORDER;
        //SD_CFG->cmp_yorder = ANC_ADAPTIVE_CMP_ORDER;
        if (ICSD_ANC_CPU == ICSD_BR28) {
            printf("ICSD BR28 SEL\n");
            SD_CFG->normal_out_sel_l = BR28_ANC_USER_TRAIN_OUT_SEL_L;
            SD_CFG->normal_out_sel_r = BR28_ANC_USER_TRAIN_OUT_SEL_R;
            SD_CFG->tone_out_sel_l   = BR28_ANC_TONE_TRAIN_OUT_SEL_L;
            SD_CFG->tone_out_sel_r   = BR28_ANC_TONE_TRAIN_OUT_SEL_R;
        } else if (ICSD_ANC_CPU == ICSD_BR50) {
            printf("ICSD BR50 SEL\n");
            SD_CFG->normal_out_sel_l = BR50_ANC_USER_TRAIN_OUT_SEL_L;
            SD_CFG->normal_out_sel_r = BR50_ANC_USER_TRAIN_OUT_SEL_R;
            SD_CFG->tone_out_sel_l   = BR50_ANC_TONE_TRAIN_OUT_SEL_L;
            SD_CFG->tone_out_sel_r   = BR50_ANC_TONE_TRAIN_OUT_SEL_R;
        } else {
            printf("CPU ERR\n");
        }
        /***************************************************/
        /**************** left channel config **************/
        /***************************************************/
        SD_CFG->adpt_cfg.pnc_times = ext_cfg->base_cfg->adaptive_times;

        // de alg config
        SD_CFG->adpt_cfg.high_fgq_fix = ANC_ADAPTIVE_HIGH_FILTER_FIX_FGQ;
        SD_CFG->adpt_cfg.de_alg_sel = (ext_cfg->dut_mode == 2) ? 0 : ANC_ADAPPTIVE_DE_ALG_SEL;

        // target配置
        SD_CFG->adpt_cfg.cmp_en = ext_cfg->ff_target_param->cmp_en;
        SD_CFG->adpt_cfg.target_cmp_num = ext_cfg->ff_target_param->cmp_curve_num;
        SD_CFG->adpt_cfg.target_sv = ext_cfg->ff_target_sv->data;
        SD_CFG->adpt_cfg.target_cmp_dat = ext_cfg->ff_target_cmp->data;

        // 算法配置
        SD_CFG->adpt_cfg.IIR_NUM_FLEX = 0;
        int flex_idx = 0;
        int biquad_idx = 0;
        for (int i = 0; i < SD_CFG->ff_yorder; i++) {
            if (ext_cfg->ff_iir_high[i].fixed_en) { // fix
                SD_CFG->adpt_cfg.Biquad_init_H[3 * biquad_idx + 0] = ext_cfg->ff_iir_high[i].def.gain;
                SD_CFG->adpt_cfg.Biquad_init_H[3 * biquad_idx + 1] = ext_cfg->ff_iir_high[i].def.fre;
                SD_CFG->adpt_cfg.Biquad_init_H[3 * biquad_idx + 2] = ext_cfg->ff_iir_high[i].def.q;
                SD_CFG->adpt_cfg.Biquad_init_M[3 * biquad_idx + 0] = ext_cfg->ff_iir_medium[i].def.gain;
                SD_CFG->adpt_cfg.Biquad_init_M[3 * biquad_idx + 1] = ext_cfg->ff_iir_medium[i].def.fre;
                SD_CFG->adpt_cfg.Biquad_init_M[3 * biquad_idx + 2] = ext_cfg->ff_iir_medium[i].def.q;
                SD_CFG->adpt_cfg.Biquad_init_L[3 * biquad_idx + 0] = ext_cfg->ff_iir_low[i].def.gain;
                SD_CFG->adpt_cfg.Biquad_init_L[3 * biquad_idx + 1] = ext_cfg->ff_iir_low[i].def.fre;
                SD_CFG->adpt_cfg.Biquad_init_L[3 * biquad_idx + 2] = ext_cfg->ff_iir_low[i].def.q;
                SD_CFG->adpt_cfg.biquad_type[biquad_idx] = ext_cfg->ff_iir_high[i].type;
                biquad_idx += 1;
            } else { // not fix
                SD_CFG->adpt_cfg.Vrange_H[6 * flex_idx + 0] = ext_cfg->ff_iir_high[i].lower_limit.gain;
                SD_CFG->adpt_cfg.Vrange_H[6 * flex_idx + 1] = ext_cfg->ff_iir_high[i].upper_limit.gain;
                SD_CFG->adpt_cfg.Vrange_H[6 * flex_idx + 2] = ext_cfg->ff_iir_high[i].lower_limit.fre;
                SD_CFG->adpt_cfg.Vrange_H[6 * flex_idx + 3] = ext_cfg->ff_iir_high[i].upper_limit.fre;
                SD_CFG->adpt_cfg.Vrange_H[6 * flex_idx + 4] = ext_cfg->ff_iir_high[i].lower_limit.q;
                SD_CFG->adpt_cfg.Vrange_H[6 * flex_idx + 5] = ext_cfg->ff_iir_high[i].upper_limit.q;
                SD_CFG->adpt_cfg.Vrange_M[6 * flex_idx + 0] = ext_cfg->ff_iir_medium[i].lower_limit.gain;
                SD_CFG->adpt_cfg.Vrange_M[6 * flex_idx + 1] = ext_cfg->ff_iir_medium[i].upper_limit.gain;
                SD_CFG->adpt_cfg.Vrange_M[6 * flex_idx + 2] = ext_cfg->ff_iir_medium[i].lower_limit.fre;
                SD_CFG->adpt_cfg.Vrange_M[6 * flex_idx + 3] = ext_cfg->ff_iir_medium[i].upper_limit.fre;
                SD_CFG->adpt_cfg.Vrange_M[6 * flex_idx + 4] = ext_cfg->ff_iir_medium[i].lower_limit.q;
                SD_CFG->adpt_cfg.Vrange_M[6 * flex_idx + 5] = ext_cfg->ff_iir_medium[i].upper_limit.q;
                SD_CFG->adpt_cfg.Vrange_L[6 * flex_idx + 0] = ext_cfg->ff_iir_low[i].lower_limit.gain;
                SD_CFG->adpt_cfg.Vrange_L[6 * flex_idx + 1] = ext_cfg->ff_iir_low[i].upper_limit.gain;
                SD_CFG->adpt_cfg.Vrange_L[6 * flex_idx + 2] = ext_cfg->ff_iir_low[i].lower_limit.fre;
                SD_CFG->adpt_cfg.Vrange_L[6 * flex_idx + 3] = ext_cfg->ff_iir_low[i].upper_limit.fre;
                SD_CFG->adpt_cfg.Vrange_L[6 * flex_idx + 4] = ext_cfg->ff_iir_low[i].lower_limit.q;
                SD_CFG->adpt_cfg.Vrange_L[6 * flex_idx + 5] = ext_cfg->ff_iir_low[i].upper_limit.q;
                flex_idx += 1;
            }
        }

        SD_CFG->adpt_cfg.Vrange_H[flex_idx * 6 + 0] = -ext_cfg->ff_iir_high_gains->lower_limit_gain;
        SD_CFG->adpt_cfg.Vrange_H[flex_idx * 6 + 1] = -ext_cfg->ff_iir_high_gains->upper_limit_gain;
        SD_CFG->adpt_cfg.Vrange_M[flex_idx * 6 + 0] = -ext_cfg->ff_iir_medium_gains->lower_limit_gain;
        SD_CFG->adpt_cfg.Vrange_M[flex_idx * 6 + 1] = -ext_cfg->ff_iir_medium_gains->upper_limit_gain;
        SD_CFG->adpt_cfg.Vrange_L[flex_idx * 6 + 0] = -ext_cfg->ff_iir_low_gains->lower_limit_gain;
        SD_CFG->adpt_cfg.Vrange_L[flex_idx * 6 + 1] = -ext_cfg->ff_iir_low_gains->upper_limit_gain;


        SD_CFG->adpt_cfg.IIR_NUM_FLEX = flex_idx;
        SD_CFG->adpt_cfg.IIR_NUM_FIX = biquad_idx;

        for (int i = 0; i < SD_CFG->ff_yorder; i++) {
            if (!ext_cfg->ff_iir_high[i].fixed_en) { // not fix
                SD_CFG->adpt_cfg.Biquad_init_H[3 * biquad_idx + 0] = ext_cfg->ff_iir_high[i].def.gain;
                SD_CFG->adpt_cfg.Biquad_init_H[3 * biquad_idx + 1] = ext_cfg->ff_iir_high[i].def.fre;
                SD_CFG->adpt_cfg.Biquad_init_H[3 * biquad_idx + 2] = ext_cfg->ff_iir_high[i].def.q;
                SD_CFG->adpt_cfg.Biquad_init_M[3 * biquad_idx + 0] = ext_cfg->ff_iir_medium[i].def.gain;
                SD_CFG->adpt_cfg.Biquad_init_M[3 * biquad_idx + 1] = ext_cfg->ff_iir_medium[i].def.fre;
                SD_CFG->adpt_cfg.Biquad_init_M[3 * biquad_idx + 2] = ext_cfg->ff_iir_medium[i].def.q;
                SD_CFG->adpt_cfg.Biquad_init_L[3 * biquad_idx + 0] = ext_cfg->ff_iir_low[i].def.gain;
                SD_CFG->adpt_cfg.Biquad_init_L[3 * biquad_idx + 1] = ext_cfg->ff_iir_low[i].def.fre;
                SD_CFG->adpt_cfg.Biquad_init_L[3 * biquad_idx + 2] = ext_cfg->ff_iir_low[i].def.q;
                SD_CFG->adpt_cfg.biquad_type[biquad_idx] = ext_cfg->ff_iir_high[i].type;
                biquad_idx += 1;
            }
        }

        SD_CFG->adpt_cfg.Biquad_init_H[biquad_idx * 3] = -ext_cfg->ff_iir_high_gains->def_total_gain;
        SD_CFG->adpt_cfg.Biquad_init_M[biquad_idx * 3] = -ext_cfg->ff_iir_medium_gains->def_total_gain;
        SD_CFG->adpt_cfg.Biquad_init_L[biquad_idx * 3] = -ext_cfg->ff_iir_low_gains->def_total_gain;

        for (int i = 0; i < 7; i++) {
            SD_CFG->adpt_cfg.degree_set0[i] = degree_set0[i];
            SD_CFG->adpt_cfg.degree_set1[i] = degree_set1[i];
            SD_CFG->adpt_cfg.degree_set2[i] = degree_set2[i];
        }

        SD_CFG->adpt_cfg.degree_set0[0] = ext_cfg->ff_iir_general->biquad_level_l;
        SD_CFG->adpt_cfg.degree_set1[0] = ext_cfg->ff_iir_general->biquad_level_h;
        SD_CFG->adpt_cfg.degree_set2[0] = ext_cfg->ff_target_param->cmp_curve_num;
        SD_CFG->adpt_cfg.degree_set0[6] = ext_cfg->ff_iir_general->biquad_level_l;
        SD_CFG->adpt_cfg.degree_set1[6] = ext_cfg->ff_iir_general->biquad_level_h;
        SD_CFG->adpt_cfg.degree_set2[6] = ext_cfg->ff_target_param->cmp_curve_num;

        SD_CFG->adpt_cfg.degree_set0[1] = ext_cfg->ff_iir_high_gains->iir_target_gain_limit;
        SD_CFG->adpt_cfg.degree_set1[1] = ext_cfg->ff_iir_medium_gains->iir_target_gain_limit;
        SD_CFG->adpt_cfg.degree_set2[1] = ext_cfg->ff_iir_low_gains->iir_target_gain_limit;

        SD_CFG->adpt_cfg.Weight_H = ext_cfg->ff_weight_high->data;
        SD_CFG->adpt_cfg.Weight_M = ext_cfg->ff_weight_medium->data;
        SD_CFG->adpt_cfg.Weight_L = ext_cfg->ff_weight_low->data;
        SD_CFG->adpt_cfg.Gold_csv_H = ext_cfg->ff_mse_high->data;
        SD_CFG->adpt_cfg.Gold_csv_M = ext_cfg->ff_mse_medium->data;
        SD_CFG->adpt_cfg.Gold_csv_L = ext_cfg->ff_mse_low->data;

        SD_CFG->adpt_cfg.total_gain_adj_begin = ext_cfg->ff_iir_general->total_gain_freq_l;
        SD_CFG->adpt_cfg.total_gain_adj_end = ext_cfg->ff_iir_general->total_gain_freq_h;
        SD_CFG->adpt_cfg.gain_limit_all = ext_cfg->ff_iir_general->total_gain_limit;

        // 耳道记忆曲线配置
        SD_CFG->adpt_cfg.mem_curve_nums = ext_cfg->ff_ear_mem_param->mem_curve_nums;
        SD_CFG->adpt_cfg.sz_table = ext_cfg->ff_ear_mem_sz->data;
        SD_CFG->adpt_cfg.pz_table = ext_cfg->ff_ear_mem_pz->data;

        /* extern const float pz_coef_table[]; */
        /* extern const float sz_coef_table[]; */
        /* SD_CFG->adpt_cfg.pz_coef_table = (float *)pz_coef_table; */
        /* SD_CFG->adpt_cfg.sz_coef_table = (float *)sz_coef_table; */
        if (ext_cfg->ff_ear_mem_pz_coeff) {
            SD_CFG->adpt_cfg.pz_coef_table = ext_cfg->ff_ear_mem_pz_coeff->data;
        } else {
            SD_CFG->adpt_cfg.pz_coef_table = NULL;
        }
        if (ext_cfg->ff_ear_mem_sz_coeff) {
            SD_CFG->adpt_cfg.sz_coef_table = ext_cfg->ff_ear_mem_sz_coeff->data;
        } else {
            SD_CFG->adpt_cfg.sz_coef_table = NULL;
        }

        float *sz_sel0 = &SD_CFG->adpt_cfg.sz_table[0];
        float *sz_sel1 = &SD_CFG->adpt_cfg.sz_table[59 * 50];

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        tmp_buff = audio_rtanc_tmp_buff_get(0);
        if (tmp_buff) {
#if AUDIO_RT_ANC_SZ_PZ_CMP_EN
            SD_CFG->adpt_cfg.pz_table_cmp = tmp_buff->pz_cmp;
            SD_CFG->adpt_cfg.sz_table_cmp = tmp_buff->sz_cmp;
#endif
            SD_CFG->adpt_cfg.sz_angle_rangel = tmp_buff->sz_angle_rangel;
            SD_CFG->adpt_cfg.sz_angle_rangeh = tmp_buff->sz_angle_rangeh;
        }
        if (SD_CFG->adpt_cfg.sz_angle_rangel && SD_CFG->adpt_cfg.sz_angle_rangeh) {
            for (int i = 0; i < MEMLEN / 2; i++) {
                SD_CFG->adpt_cfg.sz_angle_rangel[i] = angle_float(sz_sel0[2 * i + 0], sz_sel0[2 * i + 1]) * 180 - 30;
                SD_CFG->adpt_cfg.sz_angle_rangeh[i] = angle_float(sz_sel1[2 * i + 0], sz_sel1[2 * i + 1]) * 180 + 30;
                printf("angle : %d, %d\n", (int)(SD_CFG->adpt_cfg.sz_angle_rangel[i]), (int)(SD_CFG->adpt_cfg.sz_angle_rangeh[i]));
            }
        } else {
            printf("[ICSD] adaptive anc sz angle range is empty\n");
        }
        SD_CFG->adpt_cfg.gfq_default = gfq_default;   // TODO 从多组滤波器转化得到
#else
        SD_CFG->adpt_cfg.sz_angle_rangel = NULL;
        SD_CFG->adpt_cfg.sz_angle_rangeh = NULL;
        SD_CFG->adpt_cfg.gfq_default = NULL;
#endif
#if !(TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE && AUDIO_RT_ANC_SZ_PZ_CMP_EN)
        SD_CFG->adpt_cfg.pz_table_cmp = NULL;
        SD_CFG->adpt_cfg.sz_table_cmp = NULL;
#endif

        // 预置FF滤波器
        SD_CFG->adpt_cfg.ff_filter = (float *)ff_filter;

        // 产测相关
        SD_CFG->adpt_cfg.test_mode = ext_cfg->dut_mode;
        if (SD_CFG->adpt_cfg.test_mode == 1) {
            SD_CFG->adpt_cfg.IIR_NUM_FLEX = 7;
            SD_CFG->adpt_cfg.IIR_NUM_FIX = 3;
            SD_CFG->adpt_cfg.total_gain_adj_begin = 300;
            SD_CFG->adpt_cfg.total_gain_adj_end = 600;
            SD_CFG->adpt_cfg.gain_limit_all = 50;
            for (int i = 0; i < 10; i++) {
                SD_CFG->adpt_cfg.biquad_type[i] = biquad_type_gold[i];
            }
        }
        SD_CFG->adpt_cfg.vrange_gold = (float *)vrange_gold;
        SD_CFG->adpt_cfg.biquad_gold = (float *)biquad_gold;
        SD_CFG->adpt_cfg.weight_gold = (float *)weight_gold;
        SD_CFG->adpt_cfg.mse_gold = (float *)mse_gold;
        SD_CFG->adpt_cfg.degree_set_gold = (float *)degree_set_gold;

#if EXT_PRINTF_DEBUG
        for (int i = 0; i < 60; i++) {
            printf("idx=%d, mse=%d, weight=%d\n", i, (int)SD_CFG->adpt_cfg.Gold_csv_H[i], (int)SD_CFG->adpt_cfg.Weight_H[i]);
        }
        for (int i = 0; i < 60; i++) {
            printf("idx=%d, mse=%d, weight=%d\n", i, (int)SD_CFG->adpt_cfg.Gold_csv_M[i], (int)SD_CFG->adpt_cfg.Weight_M[i]);
        }
        for (int i = 0; i < 60; i++) {
            printf("idx=%d, mse=%d, weight=%d\n", i, (int)SD_CFG->adpt_cfg.Gold_csv_L[i], (int)SD_CFG->adpt_cfg.Weight_L[i]);
        }

        de_vrange_printf(SD_CFG->adpt_cfg.Vrange_H, SD_CFG->adpt_cfg.IIR_NUM_FLEX + SD_CFG->adpt_cfg.IIR_NUM_FIX);
        de_vrange_printf(SD_CFG->adpt_cfg.Vrange_M, SD_CFG->adpt_cfg.IIR_NUM_FLEX + SD_CFG->adpt_cfg.IIR_NUM_FIX);
        de_vrange_printf(SD_CFG->adpt_cfg.Vrange_L, SD_CFG->adpt_cfg.IIR_NUM_FLEX + SD_CFG->adpt_cfg.IIR_NUM_FIX);

        de_biquad_printf(SD_CFG->adpt_cfg.Biquad_init_H, SD_CFG->adpt_cfg.IIR_NUM_FLEX + SD_CFG->adpt_cfg.IIR_NUM_FIX);
        de_biquad_printf(SD_CFG->adpt_cfg.Biquad_init_M, SD_CFG->adpt_cfg.IIR_NUM_FLEX + SD_CFG->adpt_cfg.IIR_NUM_FIX);
        de_biquad_printf(SD_CFG->adpt_cfg.Biquad_init_L, SD_CFG->adpt_cfg.IIR_NUM_FLEX + SD_CFG->adpt_cfg.IIR_NUM_FIX);
#endif

#if AUDIO_ANC_STEREO_ENABLE
        /***************************************************/
        /*************** right channel config **************/
        /***************************************************/
        SD_CFG->adpt_cfg_r.pnc_times = ext_cfg->base_cfg->adaptive_times;

        // de alg config
        SD_CFG->adpt_cfg_r.high_fgq_fix = 1; // gali TODO
        SD_CFG->adpt_cfg_r.de_alg_sel = 1; // TODO

        // target配置
        SD_CFG->adpt_cfg_r.cmp_en = ext_cfg->rff_target_param->cmp_en;
        SD_CFG->adpt_cfg_r.target_cmp_num = ext_cfg->rff_target_param->cmp_curve_num;
        SD_CFG->adpt_cfg_r.target_sv = ext_cfg->rff_target_sv->data;
        SD_CFG->adpt_cfg_r.target_cmp_dat = ext_cfg->rff_target_cmp->data;

        // 算法配置
        SD_CFG->adpt_cfg_r.IIR_NUM_FLEX = 0;
        flex_idx = 0;
        biquad_idx = 0;
        for (int i = 0; i < SD_CFG->ff_yorder; i++) {
            if (ext_cfg->rff_iir_high[i].fixed_en) { // fix
                SD_CFG->adpt_cfg_r.Biquad_init_H[3 * biquad_idx + 0] = ext_cfg->rff_iir_high[i].def.gain;
                SD_CFG->adpt_cfg_r.Biquad_init_H[3 * biquad_idx + 1] = ext_cfg->rff_iir_high[i].def.fre;
                SD_CFG->adpt_cfg_r.Biquad_init_H[3 * biquad_idx + 2] = ext_cfg->rff_iir_high[i].def.q;
                SD_CFG->adpt_cfg_r.Biquad_init_M[3 * biquad_idx + 0] = ext_cfg->rff_iir_medium[i].def.gain;
                SD_CFG->adpt_cfg_r.Biquad_init_M[3 * biquad_idx + 1] = ext_cfg->rff_iir_medium[i].def.fre;
                SD_CFG->adpt_cfg_r.Biquad_init_M[3 * biquad_idx + 2] = ext_cfg->rff_iir_medium[i].def.q;
                SD_CFG->adpt_cfg_r.Biquad_init_L[3 * biquad_idx + 0] = ext_cfg->rff_iir_low[i].def.gain;
                SD_CFG->adpt_cfg_r.Biquad_init_L[3 * biquad_idx + 1] = ext_cfg->rff_iir_low[i].def.fre;
                SD_CFG->adpt_cfg_r.Biquad_init_L[3 * biquad_idx + 2] = ext_cfg->rff_iir_low[i].def.q;
                SD_CFG->adpt_cfg_r.biquad_type[biquad_idx] = ext_cfg->rff_iir_high[i].type;
                biquad_idx += 1;
            } else { // not fix
                SD_CFG->adpt_cfg_r.Vrange_H[6 * flex_idx + 0] = ext_cfg->rff_iir_high[i].lower_limit.gain;
                SD_CFG->adpt_cfg_r.Vrange_H[6 * flex_idx + 1] = ext_cfg->rff_iir_high[i].upper_limit.gain;
                SD_CFG->adpt_cfg_r.Vrange_H[6 * flex_idx + 2] = ext_cfg->rff_iir_high[i].lower_limit.fre;
                SD_CFG->adpt_cfg_r.Vrange_H[6 * flex_idx + 3] = ext_cfg->rff_iir_high[i].upper_limit.fre;
                SD_CFG->adpt_cfg_r.Vrange_H[6 * flex_idx + 4] = ext_cfg->rff_iir_high[i].lower_limit.q;
                SD_CFG->adpt_cfg_r.Vrange_H[6 * flex_idx + 5] = ext_cfg->rff_iir_high[i].upper_limit.q;
                SD_CFG->adpt_cfg_r.Vrange_M[6 * flex_idx + 0] = ext_cfg->rff_iir_medium[i].lower_limit.gain;
                SD_CFG->adpt_cfg_r.Vrange_M[6 * flex_idx + 1] = ext_cfg->rff_iir_medium[i].upper_limit.gain;
                SD_CFG->adpt_cfg_r.Vrange_M[6 * flex_idx + 2] = ext_cfg->rff_iir_medium[i].lower_limit.fre;
                SD_CFG->adpt_cfg_r.Vrange_M[6 * flex_idx + 3] = ext_cfg->rff_iir_medium[i].upper_limit.fre;
                SD_CFG->adpt_cfg_r.Vrange_M[6 * flex_idx + 4] = ext_cfg->rff_iir_medium[i].lower_limit.q;
                SD_CFG->adpt_cfg_r.Vrange_M[6 * flex_idx + 5] = ext_cfg->rff_iir_medium[i].upper_limit.q;
                SD_CFG->adpt_cfg_r.Vrange_L[6 * flex_idx + 0] = ext_cfg->rff_iir_low[i].lower_limit.gain;
                SD_CFG->adpt_cfg_r.Vrange_L[6 * flex_idx + 1] = ext_cfg->rff_iir_low[i].upper_limit.gain;
                SD_CFG->adpt_cfg_r.Vrange_L[6 * flex_idx + 2] = ext_cfg->rff_iir_low[i].lower_limit.fre;
                SD_CFG->adpt_cfg_r.Vrange_L[6 * flex_idx + 3] = ext_cfg->rff_iir_low[i].upper_limit.fre;
                SD_CFG->adpt_cfg_r.Vrange_L[6 * flex_idx + 4] = ext_cfg->rff_iir_low[i].lower_limit.q;
                SD_CFG->adpt_cfg_r.Vrange_L[6 * flex_idx + 5] = ext_cfg->rff_iir_low[i].upper_limit.q;
                flex_idx += 1;
            }
        }

        SD_CFG->adpt_cfg_r.Vrange_H[flex_idx * 6 + 0] = -ext_cfg->rff_iir_high_gains->lower_limit_gain;
        SD_CFG->adpt_cfg_r.Vrange_H[flex_idx * 6 + 1] = -ext_cfg->rff_iir_high_gains->upper_limit_gain;
        SD_CFG->adpt_cfg_r.Vrange_M[flex_idx * 6 + 0] = -ext_cfg->rff_iir_medium_gains->lower_limit_gain;
        SD_CFG->adpt_cfg_r.Vrange_M[flex_idx * 6 + 1] = -ext_cfg->rff_iir_medium_gains->upper_limit_gain;
        SD_CFG->adpt_cfg_r.Vrange_L[flex_idx * 6 + 0] = -ext_cfg->rff_iir_low_gains->lower_limit_gain;
        SD_CFG->adpt_cfg_r.Vrange_L[flex_idx * 6 + 1] = -ext_cfg->rff_iir_low_gains->upper_limit_gain;

        SD_CFG->adpt_cfg_r.IIR_NUM_FLEX = flex_idx;
        SD_CFG->adpt_cfg_r.IIR_NUM_FIX = biquad_idx;

        for (int i = 0; i < SD_CFG->ff_yorder; i++) {
            if (!ext_cfg->rff_iir_high[i].fixed_en) { // not fix
                SD_CFG->adpt_cfg_r.Biquad_init_H[3 * biquad_idx + 0] = ext_cfg->rff_iir_high[i].def.gain;
                SD_CFG->adpt_cfg_r.Biquad_init_H[3 * biquad_idx + 1] = ext_cfg->rff_iir_high[i].def.fre;
                SD_CFG->adpt_cfg_r.Biquad_init_H[3 * biquad_idx + 2] = ext_cfg->rff_iir_high[i].def.q;
                SD_CFG->adpt_cfg_r.Biquad_init_M[3 * biquad_idx + 0] = ext_cfg->rff_iir_medium[i].def.gain;
                SD_CFG->adpt_cfg_r.Biquad_init_M[3 * biquad_idx + 1] = ext_cfg->rff_iir_medium[i].def.fre;
                SD_CFG->adpt_cfg_r.Biquad_init_M[3 * biquad_idx + 2] = ext_cfg->rff_iir_medium[i].def.q;
                SD_CFG->adpt_cfg_r.Biquad_init_L[3 * biquad_idx + 0] = ext_cfg->rff_iir_low[i].def.gain;
                SD_CFG->adpt_cfg_r.Biquad_init_L[3 * biquad_idx + 1] = ext_cfg->rff_iir_low[i].def.fre;
                SD_CFG->adpt_cfg_r.Biquad_init_L[3 * biquad_idx + 2] = ext_cfg->rff_iir_low[i].def.q;
                SD_CFG->adpt_cfg_r.biquad_type[biquad_idx] = ext_cfg->rff_iir_high[i].type;
                biquad_idx += 1;
            }
        }

        SD_CFG->adpt_cfg_r.Biquad_init_H[biquad_idx * 3] = -ext_cfg->rff_iir_high_gains->def_total_gain;
        SD_CFG->adpt_cfg_r.Biquad_init_M[biquad_idx * 3] = -ext_cfg->rff_iir_medium_gains->def_total_gain;
        SD_CFG->adpt_cfg_r.Biquad_init_L[biquad_idx * 3] = -ext_cfg->rff_iir_low_gains->def_total_gain;

        for (int i = 0; i < 7; i++) {
            SD_CFG->adpt_cfg_r.degree_set0[i] = degree_set0[i];
            SD_CFG->adpt_cfg_r.degree_set1[i] = degree_set1[i];
            SD_CFG->adpt_cfg_r.degree_set2[i] = degree_set2[i];
        }

        SD_CFG->adpt_cfg_r.degree_set0[0] = ext_cfg->rff_iir_general->biquad_level_l;
        SD_CFG->adpt_cfg_r.degree_set1[0] = ext_cfg->rff_iir_general->biquad_level_h;
        SD_CFG->adpt_cfg_r.degree_set2[0] = ext_cfg->rff_target_param->cmp_curve_num;
        SD_CFG->adpt_cfg_r.degree_set0[6] = ext_cfg->rff_iir_general->biquad_level_l;
        SD_CFG->adpt_cfg_r.degree_set1[6] = ext_cfg->rff_iir_general->biquad_level_h;
        SD_CFG->adpt_cfg_r.degree_set2[6] = ext_cfg->rff_target_param->cmp_curve_num;

        SD_CFG->adpt_cfg_r.degree_set0[1] = ext_cfg->rff_iir_high_gains->iir_target_gain_limit;
        SD_CFG->adpt_cfg_r.degree_set1[1] = ext_cfg->rff_iir_medium_gains->iir_target_gain_limit;
        SD_CFG->adpt_cfg_r.degree_set2[1] = ext_cfg->rff_iir_low_gains->iir_target_gain_limit;

        SD_CFG->adpt_cfg_r.Weight_H = ext_cfg->rff_weight_high->data;
        SD_CFG->adpt_cfg_r.Weight_M = ext_cfg->rff_weight_medium->data;
        SD_CFG->adpt_cfg_r.Weight_L = ext_cfg->rff_weight_low->data;
        SD_CFG->adpt_cfg_r.Gold_csv_H = ext_cfg->rff_mse_high->data;
        SD_CFG->adpt_cfg_r.Gold_csv_M = ext_cfg->rff_mse_medium->data;
        SD_CFG->adpt_cfg_r.Gold_csv_L = ext_cfg->rff_mse_low->data;

        SD_CFG->adpt_cfg_r.total_gain_adj_begin = ext_cfg->rff_iir_general->total_gain_freq_l;
        SD_CFG->adpt_cfg_r.total_gain_adj_end = ext_cfg->rff_iir_general->total_gain_freq_h;
        SD_CFG->adpt_cfg_r.gain_limit_all = ext_cfg->rff_iir_general->total_gain_limit;

        // 耳道记忆曲线配置
        SD_CFG->adpt_cfg_r.mem_curve_nums = ext_cfg->rff_ear_mem_param->mem_curve_nums;
        SD_CFG->adpt_cfg_r.sz_table = ext_cfg->rff_ear_mem_sz->data;
        SD_CFG->adpt_cfg_r.pz_table = ext_cfg->rff_ear_mem_pz->data;

        /* extern const float pz_coef_table[]; */
        /* extern const float sz_coef_table[]; */
        /* SD_CFG->adpt_cfg.pz_coef_table = (float *)pz_coef_table; */
        /* SD_CFG->adpt_cfg.sz_coef_table = (float *)sz_coef_table; */
        if (ext_cfg->rff_ear_mem_pz_coeff) {
            SD_CFG->adpt_cfg_r.pz_coef_table = ext_cfg->rff_ear_mem_pz_coeff->data;
        } else {
            SD_CFG->adpt_cfg_r.pz_coef_table = NULL;
        }
        if (ext_cfg->rff_ear_mem_sz_coeff) {
            SD_CFG->adpt_cfg_r.sz_coef_table = ext_cfg->rff_ear_mem_sz_coeff->data;
        } else {
            SD_CFG->adpt_cfg_r.sz_coef_table = NULL;
        }

        sz_sel0 = &SD_CFG->adpt_cfg_r.sz_table[0];
        sz_sel1 = &SD_CFG->adpt_cfg_r.sz_table[59 * 50];

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        tmp_buff = audio_rtanc_tmp_buff_get(1);
        if (tmp_buff) {
#if AUDIO_RT_ANC_SZ_PZ_CMP_EN
            SD_CFG->adpt_cfg_r.pz_table_cmp = tmp_buff->pz_cmp;
            SD_CFG->adpt_cfg_r.sz_table_cmp = tmp_buff->sz_cmp;
#endif
            SD_CFG->adpt_cfg_r.sz_angle_rangel = tmp_buff->sz_angle_rangel;
            SD_CFG->adpt_cfg_r.sz_angle_rangeh = tmp_buff->sz_angle_rangeh;
        }
        if (SD_CFG->adpt_cfg_r.sz_angle_rangel && SD_CFG->adpt_cfg_r.sz_angle_rangeh) {
            for (int i = 0; i < MEMLEN / 2; i++) {
                SD_CFG->adpt_cfg_r.sz_angle_rangel[i] = angle_float(sz_sel0[2 * i + 0], sz_sel0[2 * i + 1]) * 180 - 30;
                SD_CFG->adpt_cfg_r.sz_angle_rangeh[i] = angle_float(sz_sel1[2 * i + 0], sz_sel1[2 * i + 1]) * 180 + 30;
                printf("angle : %d, %d\n", (int)(SD_CFG->adpt_cfg_r.sz_angle_rangel[i]), (int)(SD_CFG->adpt_cfg_r.sz_angle_rangeh[i]));
            }
        } else {
            printf("[ICSD] adaptive anc sz angle range is empty\n");
        }
        SD_CFG->adpt_cfg_r.gfq_default = gfq_default_r;   // TODO 从多组滤波器转化得到
#else
        SD_CFG->adpt_cfg_r.sz_angle_rangel = NULL;
        SD_CFG->adpt_cfg_r.sz_angle_rangeh = NULL;
        SD_CFG->adpt_cfg_r.gfq_default = NULL;
#endif
#if !(TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE && AUDIO_RT_ANC_SZ_PZ_CMP_EN)
        SD_CFG->adpt_cfg.pz_table_cmp = NULL;
        SD_CFG->adpt_cfg.sz_table_cmp = NULL;
#endif

        // 预置FF滤波器
        SD_CFG->adpt_cfg_r.ff_filter = (float *)ff_filter;

        // 产测相关
        SD_CFG->adpt_cfg_r.test_mode = ext_cfg->dut_mode;
        if (SD_CFG->adpt_cfg_r.test_mode == 1) {
            SD_CFG->adpt_cfg_r.IIR_NUM_FLEX = 7;
            SD_CFG->adpt_cfg_r.IIR_NUM_FIX = 3;
            SD_CFG->adpt_cfg_r.total_gain_adj_begin = 300;
            SD_CFG->adpt_cfg_r.total_gain_adj_end = 600;
            SD_CFG->adpt_cfg_r.gain_limit_all = 50;
            for (int i = 0; i < 10; i++) {
                SD_CFG->adpt_cfg_r.biquad_type[i] = biquad_type_gold[i];
            }
        }
        SD_CFG->adpt_cfg_r.vrange_gold = (float *)vrange_gold;
        SD_CFG->adpt_cfg_r.biquad_gold = (float *)biquad_gold;
        SD_CFG->adpt_cfg_r.weight_gold = (float *)weight_gold;
        SD_CFG->adpt_cfg_r.mse_gold = (float *)mse_gold;
        SD_CFG->adpt_cfg_r.degree_set_gold = (float *)degree_set_gold;



#if EXT_PRINTF_DEBUG
        for (int i = 0; i < 60; i++) {
            printf("idx=%d, mse=%d, weight=%d\n", i, (int)SD_CFG->adpt_cfg_r.Gold_csv_H[i], (int)SD_CFG->adpt_cfg_r.Weight_H[i]);
        }

        de_vrange_printf(SD_CFG->adpt_cfg_r.Vrange_H, SD_CFG->adpt_cfg_r.IIR_NUM_FLEX + SD_CFG->adpt_cfg_r.IIR_NUM_FIX);
        de_vrange_printf(SD_CFG->adpt_cfg_r.Vrange_M, SD_CFG->adpt_cfg_r.IIR_NUM_FLEX + SD_CFG->adpt_cfg_r.IIR_NUM_FIX);
        de_vrange_printf(SD_CFG->adpt_cfg_r.Vrange_L, SD_CFG->adpt_cfg_r.IIR_NUM_FLEX + SD_CFG->adpt_cfg_r.IIR_NUM_FIX);

        de_biquad_printf(SD_CFG->adpt_cfg_r.Biquad_init_H, SD_CFG->adpt_cfg_r.IIR_NUM_FLEX + SD_CFG->adpt_cfg_r.IIR_NUM_FIX);
        de_biquad_printf(SD_CFG->adpt_cfg_r.Biquad_init_M, SD_CFG->adpt_cfg_r.IIR_NUM_FLEX + SD_CFG->adpt_cfg_r.IIR_NUM_FIX);
        de_biquad_printf(SD_CFG->adpt_cfg_r.Biquad_init_L, SD_CFG->adpt_cfg_r.IIR_NUM_FLEX + SD_CFG->adpt_cfg_r.IIR_NUM_FIX);
#endif

#endif
    }
}

#if RTANC_SZ_SEL_FF_OUTPUT
int icsd_anc_sz_select_from_memory(float *out_iir, float *sz, float *msc, int sz_points)
{
    struct anc_ext_ear_adaptive_param *ext_cfg = anc_ext_ear_adaptive_cfg_get();
    if (!ext_cfg) {
        return -1;
    }
    int sel_idx;
    __sz_sel_mem *hdl = anc_malloc("ICSD_ANC_CONFIG", sizeof(__sz_sel_mem));

    hdl->nums = ext_cfg->ff_ear_mem_param->mem_curve_nums;
    hdl->mem_len = MEMLEN;
    hdl->l_idx0 = 2;
    hdl->l_idx1 = 7;
    hdl->h_idx0 = 7;
    hdl->h_idx1 = 9;
    hdl->mem_list = (u8 *)mem_list;
    hdl->sz_in = sz;
    hdl->msc = msc;
    hdl->msc_thr0[0] = 0.30;
    hdl->msc_thr0[1] = 0.05;
    hdl->msc_thr0[2] = 0.05;
    hdl->msc_thr1[0] = 0.40;
    hdl->msc_thr1[1] = 0.01;
    hdl->msc_thr1[2] = 0.01;
    hdl->msc_thr2[0] = 0.20;
    hdl->msc_thr2[1] = 0.18;
    hdl->msc_thr2[2] = 0.16;

    hdl->msc_idx0 = 2;
    hdl->msc_idx1 = 7;
#if AUDIO_RT_ANC_SZ_PZ_CMP_EN
    hdl->sz_table_cmp = audio_rtanc_sz_cmp_get(0);
#else
    hdl->sz_table_cmp = NULL;
#endif
    hdl->sz_table = ext_cfg->ff_ear_mem_sz->data;
    hdl->ff_filter = (float *)ff_filter;
    sz_select_from_memory(hdl);

    memcpy(out_iir, hdl->ff_gfq, 31 * sizeof(float));

    float tmp;
    //gfq -> fgq
    for (int i = 0; i < 10; i++) {
        tmp = out_iir[1 + (3 * i)];
        out_iir[1 + (3 * i)] = out_iir[1 + (3 * i) + 1];
        out_iir[1 + (3 * i) + 1] = tmp;
    }

#if 0
    for (int i = 0; i < 60; i++) {
        printf("sz:%9d, %9d, %9d, %9d, %9d\n", (int)(hdl->sz_in[2 * i + 0] * 1e6), \
               (int)(hdl->sz_in[2 * i + 1] * 1e6), (int)(hdl->sz_out[2 * i + 0] * 1e6), \
               (int)(hdl->sz_out[2 * i + 1] * 1e6), (int)(hdl->msc[i] * 1e6));
    }

    for (int i = 0; i < 31; i++) {
        printf("ff sel gfq:%d\n", (int)(out_iir[i] * 1e3));
    }
#endif

    //AFQ在环境不稳定时会抖动，需替换为sz_list内稳定的SZ
    memcpy(sz, hdl->sz_out, sz_points * sizeof(float));
    sel_idx = hdl->sel_idx + 1; //[0 - 59] + 1， 用于非0判断
    if (sel_idx >= 51) {
        sel_idx = 101;
    }
    anc_free(hdl);
    return sel_idx;
}
#endif

void icsd_anc_v2_sz_pz_cmp_calculate(struct sz_pz_cmp_cal_param *p)
{
    float eq_freq_l = 50;
    float eq_freq_h = 500;
    u8 mode = 1;	//1 eq gain; 0 eq 频响

#if 0
    printf("ff_gain %d/100\n", (int)(p->ff_gain * 100.0f));
    printf("fb_gain %d/100\n", (int)(p->fb_gain * 100.0f));

    struct aeq_default_seg_tab *eq_tab = p->spk_eq_tab;
    if (eq_tab) {
        printf("global_gain %d/100\n", (int)(eq_tab->global_gain * 100.f));
        printf("seg_num %d\n", eq_tab->seg_num);
        for (int i = 0; i < eq_tab->seg_num; i++) {
            printf("index %d\n", eq_tab->seg[i].index);
            printf("iir_type %d\n", eq_tab->seg[i].iir_type);
            printf("freq %d\n", eq_tab->seg[i].freq);
            printf("gain %d/100\n", (int)(eq_tab->seg[i].gain * 100.f));
            printf("q %d/100\n", (int)(eq_tab->seg[i].q * 100.f));
        }
    }
#endif
    szpz_cmp_get(p->pz_cmp_out, p->sz_cmp_out, p->ff_gain, p->fb_gain, \
                 (struct aeq_default_seg_tab *)p->spk_eq_tab, eq_freq_l, eq_freq_h, mode);
#if 0
    printf("sz_pz output\n");
    for (int i = 0; i < 50; i++) {
        printf("pz %d/100\n", (int)(p->pz_cmp_out[i] * 100.0f));
        printf("sz %d/100\n", (int)(p->sz_cmp_out[i] * 100.0f));
    }
#endif
}

const float pz_coef_table[] = {
    0.6053, -37.5068, 2188.5000, 0.8421, 4.2984, 1798.0800, 1.2126, 3.9308, 968.1250, 0.7204, 3.8398, 301.1070, 0.3161, 4.0718, 94.2334, 0.8899,
    0.6715, -38.8833, 3091.0100, 1.0000, 3.7490, 1377.5900, 0.9212, -3.0355, 1374.2200, 1.4268, 2.7937, 242.1620, 0.3497, 5.0000, 68.9791, 0.8248,
    0.7192, -35.5065, 2704.0300, 1.0000, 4.0530, 1318.6900, 0.8461, -2.2787, 1270.2000, 1.3828, 2.3194, 189.0040, 0.3193, 3.3688, 67.6214, 0.8080,
    0.7323, -38.0327, 2614.4500, 0.9148, 6.5949, 1502.4800, 0.8281, -3.0764, 1383.7800, 1.3811, 2.4164, 443.7650, 0.3130, 3.0423, 110.8380, 0.8220,
    0.7844, -39.9796, 3562.9300, 0.9309, -1.4555, 1482.2400, 1.4093, 3.0842, 983.8670, 0.4822, 1.3438, 183.7170, 0.7368, 3.2672, 77.3150, 0.8009,
    0.7609, -38.9806, 3595.0300, 1.0000, -2.4176, 1304.2200, 1.0920, 1.6691, 1313.4000, 0.8769, 1.8592, 392.2480, 0.3002, 2.9685, 94.9392, 0.8063,
    0.7720, -39.2148, 3200.2900, 0.8646, -0.2475, 1251.5400, 1.2906, 4.0305, 1124.3500, 0.4770, 1.6244, 132.8300, 0.3000, 2.6648, 50.0000, 0.8180,
    0.8297, -36.2362, 2960.4700, 0.9202, 0.7556, 1800.0000, 1.2427, 3.0318, 940.0480, 0.6541, 1.2948, 200.8050, 0.6177, 2.4982, 84.0920, 0.9441,
    0.8651, -35.7559, 3064.1400, 1.0000, -0.8324, 1619.4400, 0.9638, 0.5216, 1477.6600, 1.1021, 1.9395, 790.2880, 0.3928, 2.1988, 134.3680, 0.8207,
    0.8394, -38.0588, 3335.2300, 0.9290, 1.1797, 1652.5400, 1.4603, 2.3142, 890.1490, 0.6321, 1.0711, 164.8020, 0.4370, 1.6418, 80.1392, 0.8550,
    0.9064, -37.0181, 2207.5100, 0.8793, -1.7342, 1799.0600, 1.2806, 9.2116, 1460.4600, 0.3237, -2.0288, 478.8600, 0.9049, 1.6744, 113.9980, 0.8364,
    0.9044, -34.7881, 2547.2300, 0.9139, 0.5678, 1701.1700, 1.4040, 3.9558, 1208.6600, 0.4768, 0.5359, 781.3200, 1.4330, 1.3950, 150.0000, 0.8004,
    0.9074, -27.7367, 2097.4600, 0.9849, 4.4112, 1521.4900, 1.0596, -1.2267, 1495.6100, 0.8583, 1.7108, 794.6920, 0.7054, 1.2938, 150.0000, 0.9174,
    0.9649, -33.8297, 2307.3700, 0.8478, 4.6366, 1219.8300, 0.6581, -1.7397, 390.8780, 0.6411, 2.8010, 674.5920, 0.3000, 1.3342, 52.1020, 0.8524,
    0.9964, -36.5796, 2665.6500, 0.9251, 4.0912, 1625.9500, 0.9356, 0.8867, 991.9400, 1.0318, 2.1525, 795.6810, 0.9409, 0.5304, 54.4080, 0.9812,
    0.9693, -31.8891, 2036.1500, 0.9057, 6.8869, 1700.4600, 0.8276, 3.0175, 933.2960, 0.7400, -0.3359, 610.8070, 1.2325, 1.2098, 60.6961, 0.9374,
    1.0073, -27.1413, 1980.6900, 1.0000, 4.3881, 1757.1500, 0.9138, 2.3518, 1030.3900, 1.1496, 0.2203, 800.0000, 1.3258, 0.9275, 84.3674, 0.9123,
    1.0212, -33.4533, 2907.2100, 0.9209, -2.8227, 1554.3700, 1.3757, 6.0091, 1436.0300, 0.7195, 0.4031, 799.2990, 1.3849, 0.7743, 66.2687, 1.0000,
    1.0056, -31.7147, 2954.6300, 0.9929, 3.1940, 1569.6900, 0.6515, -3.0597, 1294.0200, 0.3988, 2.9371, 798.8800, 0.7461, 0.4501, 54.3830, 0.8813,
    1.0447, -32.8627, 2996.9900, 0.8663, 7.1497, 1278.6300, 0.7743, -2.0095, 1400.0100, 1.0093, -0.1445, 455.1520, 0.6374, 0.5683, 50.0000, 0.9654,
    1.0719, -35.4326, 2757.7600, 0.8131, 4.7099, 1439.7800, 1.1198, 2.1109, 1447.6500, 0.6196, 2.7394, 800.0000, 1.2119, -0.6268, 73.2220, 0.9120,
    1.0548, -29.9077, 2708.5900, 0.8947, 4.0351, 1359.3400, 0.8790, 0.1816, 1023.5600, 1.2163, 1.0731, 800.0000, 1.4940, -0.2403, 56.7506, 0.8969,
    1.0695, -32.7742, 2413.7900, 0.8200, 7.4048, 1557.8900, 0.7639, 2.2735, 962.2560, 0.8744, -0.8078, 101.8720, 1.4993, 0.0674, 50.0000, 0.8861,
    1.0770, -27.6065, 2709.2200, 0.9954, -0.8499, 1454.9600, 0.7586, 4.9188, 1308.8900, 0.8698, -0.3022, 327.7090, 0.6686, -0.2746, 50.0000, 0.8839,
    1.0708, -29.2758, 2306.4600, 0.8995, 6.8291, 1553.1200, 0.9671, 2.1158, 932.3880, 0.9925, -0.5448, 269.5830, 0.6880, -0.2402, 55.4705, 0.9286,
    1.1127, -32.2939, 2766.7400, 0.8936, 3.2803, 1800.0000, 0.5326, 4.9577, 1280.4800, 0.8106, -0.9116, 548.7620, 0.4846, -1.0279, 50.0000, 0.8862,
    1.0848, -26.9052, 1990.5100, 0.8579, 7.6060, 1598.9800, 0.9855, 3.8013, 976.6510, 0.9091, -0.7272, 535.3270, 0.3000, -0.6319, 50.1649, 0.9179,
    1.0850, -27.3266, 2313.5100, 0.8413, 7.7215, 1402.9400, 0.6847, -0.0898, 442.0020, 1.2937, -0.7098, 312.1120, 0.3798, -0.4052, 83.0811, 0.9963,
    1.1020, -26.8350, 2887.6600, 1.0000, 2.6315, 1644.2500, 1.1393, 2.6586, 1144.9100, 1.1230, -0.3948, 211.0560, 0.3000, -0.1860, 50.0000, 0.8018,
    1.1017, -32.0106, 3097.4200, 0.9946, 3.7173, 1622.4500, 1.2534, 3.0630, 1107.8200, 0.8654, -0.7658, 305.1890, 0.3025, -1.3059, 50.0462, 0.9624,
    1.1039, -24.0496, 2324.3200, 0.9874, 4.4852, 1717.2400, 0.8427, 2.9430, 1335.2800, 0.9320, -0.6170, 490.7200, 0.4791, -0.4027, 76.9432, 0.8175,
    1.1130, -31.6018, 3304.9100, 0.8000, 5.3395, 1696.8000, 1.0366, 3.5992, 1160.4100, 1.5000, 0.9406, 800.0000, 1.4927, -0.4905, 149.9020, 0.9676,
    1.0794, -22.4278, 2266.2500, 0.9848, 5.8060, 1533.9100, 0.8800, 0.6683, 1083.7200, 1.1745, -0.5959, 407.1660, 0.3033, -0.6152, 61.9364, 0.8086,
    1.1126, -24.1664, 2373.5300, 0.9464, 5.8932, 1745.4200, 1.2339, 3.1944, 1154.3000, 1.2888, -0.4875, 328.1160, 1.2968, -0.5167, 131.0160, 0.8402,
    1.1087, -28.4584, 2715.3300, 0.9680, 5.2671, 1582.5400, 0.9555, 2.9917, 1493.1600, 0.9836, -0.7261, 336.1500, 0.7924, -0.7919, 150.0000, 0.9466,
    1.1029, -27.7659, 2425.9600, 0.9258, 7.8982, 1709.2800, 0.9196, 2.5767, 1416.6500, 0.8261, -0.6510, 383.1510, 0.5356, -0.6860, 118.6830, 0.8593,
    1.1036, -21.5835, 2174.3600, 0.8905, 1.9723, 1686.8900, 0.7951, 6.4360, 1478.4000, 0.9580, -0.4985, 295.6010, 0.3000, -0.0721, 114.8120, 0.8092,
    1.1037, -22.8483, 2705.7800, 0.8522, 3.7228, 1647.1900, 1.1330, 4.2599, 1308.3200, 0.8084, -0.8370, 765.0580, 0.3641, -0.4283, 150.0000, 0.9551,
    1.1058, -27.1437, 2539.6500, 0.9612, 10.7215, 1612.8700, 1.0413, 0.2008, 1330.4700, 1.2849, -0.7892, 374.4470, 0.6610, -0.6154, 149.3530, 0.9170,
    1.0843, -21.7140, 2473.1500, 0.9251, 7.9047, 1664.5300, 1.5000, 1.7418, 1107.8700, 1.1681, -0.4721, 271.2800, 0.8269, -0.4090, 146.8930, 0.9805,
    1.0998, -18.5609, 2345.1500, 1.0000, 4.3888, 1645.9100, 1.2453, 3.4307, 1261.9300, 0.7784, -1.3678, 800.0000, 0.5776, -0.3637, 123.1770, 0.9248,
    1.1091, -21.4112, 2750.3900, 0.9002, 6.2833, 1578.5600, 1.1060, 0.8126, 1266.8800, 1.0887, -0.4980, 321.3050, 0.7679, -0.4280, 124.2960, 0.9927,
    1.1194, -20.0546, 2373.2200, 0.9435, 7.0211, 1739.9400, 1.1444, 2.0812, 1357.2900, 1.1018, -0.4658, 284.4800, 0.3227, -0.2218, 112.6790, 0.8434,
    1.0615, -19.9465, 2531.9000, 1.0000, 7.8838, 1759.9900, 0.6164, -2.2114, 989.1120, 0.7683, -0.6243, 344.5380, 0.6936, -0.3369, 150.0000, 0.9781,
    1.0757, -17.3962, 2223.8800, 0.9907, 6.9881, 1621.5100, 1.1183, 0.2489, 929.1170, 0.8030, -0.5554, 276.3200, 0.3000, -0.4261, 50.0000, 0.9647,
    1.0877, -23.7441, 2736.5400, 1.0000, 9.9656, 1779.4100, 0.9838, -0.9266, 611.0640, 0.3107, 0.1788, 346.4270, 1.1543, -0.4136, 150.0000, 0.9718,
    1.0857, -15.7184, 2550.1000, 0.9999, 5.4539, 1800.0000, 1.4640, 1.3665, 1354.3700, 0.9484, -0.2803, 391.1430, 0.5790, -0.0866, 141.2500, 0.8000,
    1.0789, -17.7440, 2486.9500, 0.9113, 7.1671, 1668.9700, 1.5000, 1.4199, 1113.4400, 1.0731, -0.4697, 196.2650, 0.3056, -0.1150, 73.5004, 0.8965,
    1.0752, -20.6303, 3012.4600, 0.9958, 7.6685, 1783.9200, 1.1105, -0.3116, 854.3330, 0.3000, -0.3485, 183.0410, 0.6576, 0.0093, 50.4389, 0.8113,
    1.0676, -17.8090, 2610.5600, 0.9770, 8.9133, 1800.0000, 1.4772, 0.4751, 1104.7400, 0.8483, -0.4172, 221.2790, 0.3000, 0.0604, 77.9972, 0.8161,
    1.0920, -18.8766, 3027.4000, 0.9978, 7.2205, 1800.0000, 1.0159, -0.1424, 1114.6700, 1.3706, -0.4644, 691.2630, 0.3009, -0.3845, 54.9450, 0.9636,
    1.0657, -14.9691, 2264.2100, 0.9682, 9.2996, 1800.0000, 1.3069, -0.3958, 1306.9400, 1.3953, -0.4914, 114.2570, 0.3000, 0.9888, 50.0000, 0.9936,
    1.0625, -20.7472, 3698.3600, 0.9989, 6.6356, 1799.3000, 1.2887, -0.2922, 349.7550, 1.0628, -0.3720, 157.8700, 1.3190, -0.2446, 51.1585, 0.9997,
    1.0935, -17.2603, 2953.9500, 0.9943, 7.4885, 1798.0600, 1.2023, -0.3200, 1000.7400, 0.5528, -0.2977, 114.0410, 0.3143, 0.3325, 54.9029, 0.9733,
    1.0974, -15.3209, 3053.9700, 0.9999, 5.1265, 1799.4800, 1.2391, 0.7480, 1494.1500, 1.1373, -0.3047, 213.6700, 0.3038, 0.0609, 67.0666, 0.9084,
    1.0942, -12.6672, 2102.7400, 0.9999, 8.5647, 1799.8800, 1.4754, 0.3237, 1219.9000, 1.0156, -0.3550, 380.2940, 0.6230, -0.1819, 148.6780, 0.9957,
    1.0812, -10.8443, 2245.1700, 1.0000, 6.5926, 1800.0000, 1.4214, 0.1478, 965.4310, 1.4927, -0.1829, 344.2650, 0.7835, 0.0291, 57.1328, 0.9992,
    1.0527, -15.7417, 3622.0700, 0.9303, 5.8467, 1797.6600, 1.2129, -0.6263, 1432.7100, 1.4978, -0.2594, 165.4110, 0.4405, 0.3142, 50.0017, 0.9907,
    1.0731, -11.7152, 3449.1700, 1.0000, 3.9034, 1800.0000, 1.2817, -0.0597, 1005.0800, 0.6088, -0.1580, 295.3140, 1.2008, 0.0004, 52.3517, 0.8559,
    1.0662, -11.5964, 3587.7300, 0.9856, 3.7310, 1800.0000, 1.4807, 0.1570, 950.1600, 0.9154, -0.1061, 463.8450, 1.2431, 0.0633, 58.2585, 0.9993,
};
const float sz_coef_table[] = {
    1.4644, -39.8166, 1872.6300, 0.8930, 0.7314, 1316.6000, 1.4935, 5.1048, 1255.6300, 0.5237, 7.4561, 226.5790, 0.3209, -7.1856, 76.4820, 0.9185,
    1.5559, -40.0000, 2326.9500, 0.9967, 2.1153, 1188.8800, 1.3530, 0.9417, 213.9550, 0.8073, 5.7330, 199.3900, 0.3036, -9.9355, 65.2249, 0.9329,
    1.5165, -39.9200, 2347.2900, 0.9825, 3.0483, 1313.7500, 0.7780, -0.4220, 1421.1300, 0.7881, 6.8453, 162.3630, 0.3208, -9.3416, 96.9392, 0.8841,
    1.4545, -40.0000, 2722.9400, 0.9891, -1.2516, 1800.0000, 1.5000, 0.2125, 200.0000, 1.4989, 5.5445, 200.3500, 0.3004, -9.9024, 77.0306, 1.0000,
    1.4309, -37.9075, 2242.1700, 0.8957, -2.4320, 1740.9500, 1.0733, 5.6915, 1199.0600, 0.5847, 5.6350, 172.0100, 0.4347, -9.0558, 84.1852, 0.9756,
    1.3622, -39.9017, 2213.9800, 0.9317, 0.1860, 1789.3400, 1.2692, 3.8685, 1357.8100, 0.8623, 5.4300, 232.8360, 0.3000, -8.6901, 81.0826, 0.9925,
    1.3787, -39.6493, 1523.5800, 1.0000, 7.1061, 1800.0000, 0.3675, 3.7679, 1103.3600, 0.5463, 5.6517, 141.1410, 0.6216, -9.7980, 86.9726, 0.8249,
    1.4395, -39.9958, 2399.2500, 1.0000, 2.1091, 1405.1200, 1.4169, -2.1210, 490.3070, 0.9937, 5.2707, 298.4850, 0.3000, -9.7744, 82.4225, 0.9992,
    1.4578, -39.9955, 2729.7400, 1.0000, -3.5687, 1745.8900, 1.2121, 3.5603, 1448.4500, 0.8777, 4.2719, 187.8770, 0.4572, -9.9995, 90.2122, 0.9807,
    1.3170, -39.9220, 2178.0100, 0.9439, 2.5678, 1734.7800, 1.2557, -4.0932, 461.6810, 0.8223, 7.6682, 444.5440, 0.3363, -10.0000, 81.3698, 1.0000,
    1.4144, -39.9368, 2428.9200, 0.8674, -6.2217, 1655.4500, 1.4523, 8.1457, 1468.1100, 1.0372, 3.8330, 401.9310, 0.3000, -8.4404, 95.6310, 0.9986,
    1.3969, -39.8950, 2289.8700, 1.0000, 3.1543, 1714.2100, 0.9572, 2.3671, 957.1400, 0.9260, 2.8406, 187.2400, 0.5824, -9.9970, 95.4894, 0.9980,
    1.2539, -38.9565, 2223.0200, 0.9606, 3.1999, 1763.7000, 1.2714, 3.7138, 1011.5100, 0.8909, 3.6402, 181.1670, 0.5760, -10.0000, 96.8532, 0.9884,
    1.3334, -38.7338, 1607.6400, 0.9980, 8.2136, 1790.7400, 0.5920, 7.7894, 512.0290, 0.3716, -6.0512, 435.8140, 0.7530, -9.9367, 106.5860, 0.8308,
    1.3665, -39.2496, 2312.8100, 0.8016, -5.3752, 1778.5000, 1.2920, 8.9355, 1377.7100, 0.6354, 2.0278, 462.0730, 0.3335, -10.0000, 114.5410, 0.9749,
    1.1966, -39.9918, 2720.9800, 0.9859, 1.3171, 1743.4800, 1.1830, -2.4105, 517.8740, 1.1201, 3.9329, 436.4610, 0.3017, -9.9969, 102.1800, 0.8335,
    1.2127, -36.9360, 2375.2000, 0.9499, 0.3015, 1699.0500, 0.5140, 4.2724, 1171.9900, 0.8513, 1.9491, 210.3300, 0.7009, -9.9848, 113.4920, 0.9941,
    1.2102, -39.4969, 2704.4900, 0.8976, 4.4577, 1116.5000, 1.0426, 0.0584, 1078.1400, 0.8348, 1.7985, 303.9610, 0.3882, -9.9988, 115.5960, 0.9997,
    1.1633, -39.9008, 2613.9800, 0.9635, 0.5124, 1792.6600, 0.9530, 3.9629, 1163.7700, 0.6438, 2.0869, 188.5470, 1.2371, -9.9974, 122.2890, 0.9236,
    1.0618, -38.5800, 2962.4700, 0.9834, -1.3694, 1798.7900, 0.7163, 4.2213, 1286.9600, 0.8054, 2.5287, 170.1920, 1.2220, -10.0000, 131.5240, 1.0000,
    1.1271, -36.8343, 2531.2400, 0.9079, -9.9336, 1767.1200, 0.8571, 14.4872, 1500.0000, 0.7313, -4.2845, 100.0000, 1.1439, -10.0000, 106.6100, 0.9950,
    1.0275, -36.2748, 2076.4800, 0.9511, 5.8414, 1459.4900, 0.5943, 1.6407, 1335.6000, 0.7691, -4.0636, 100.0000, 1.1807, -10.0000, 103.7540, 1.0000,
    0.9892, -34.4726, 2569.6600, 1.0000, 3.6614, 1254.1500, 1.0434, 2.6018, 200.0000, 0.5788, -5.4935, 100.2490, 0.5196, -10.0000, 96.5214, 0.9906,
    0.9028, -36.1090, 2704.4800, 0.9210, 3.2386, 1501.4100, 0.8650, 2.5678, 1116.2700, 1.2138, 1.0378, 242.9240, 0.5919, -10.0000, 140.7610, 0.9979,
    0.8492, -39.5807, 2776.4800, 0.8557, -8.7895, 1660.2400, 0.8336, 16.8869, 1500.0000, 0.6971, -0.7566, 537.0890, 1.4894, -9.9955, 129.0540, 1.0000,
    0.7997, -37.9748, 2881.9600, 0.8634, -7.9596, 1643.3900, 1.4913, 14.4771, 1495.6100, 0.9559, -5.0004, 100.0930, 1.0911, -9.9634, 98.1240, 1.0000,
    0.7706, -27.5817, 1869.2100, 0.8333, 6.9349, 1390.7400, 1.2422, 2.7410, 870.1530, 0.8998, 0.7167, 209.4140, 1.0605, -9.9880, 144.0130, 0.9999,
    0.7664, -27.1867, 1780.8300, 0.8323, -2.0141, 1786.2700, 1.1021, 10.4778, 1383.4200, 0.7219, -9.7028, 100.0330, 1.4138, -9.9987, 97.7458, 1.0000,
    0.7126, -36.0492, 3305.4100, 0.8134, 5.6349, 1289.1200, 1.0576, 1.3332, 447.7580, 0.3007, -6.1177, 100.0070, 0.8705, -9.9947, 93.5083, 1.0000,
    0.6884, -35.9440, 3029.8900, 0.9948, 1.0610, 1685.6300, 0.6744, 4.8818, 1398.1800, 0.9952, -6.9588, 100.0600, 1.1788, -9.9753, 104.2930, 1.0000,
    0.6425, -36.1985, 3695.5400, 0.8311, -8.4843, 1738.6900, 1.1031, 12.9764, 1500.0000, 0.9398, -5.6802, 100.0000, 1.1839, -9.9945, 99.7429, 1.0000,
    0.6148, -32.9910, 2972.6500, 0.8801, 3.8418, 1393.3500, 1.1551, 3.2280, 1231.3500, 0.6985, -7.3018, 100.0820, 1.2534, -9.9970, 105.4120, 0.9998,
    0.5515, -28.1599, 2441.7100, 0.9370, 6.3180, 1446.4600, 1.1631, 0.8750, 806.7660, 1.3395, -7.3203, 100.0340, 1.4741, -10.0000, 104.1680, 0.9952,
    0.5412, -31.5761, 2916.8400, 0.8400, -0.0002, 1341.5400, 0.9924, 7.5606, 1365.9400, 0.8385, -8.1654, 100.0000, 1.2711, -9.9988, 99.8581, 0.9992,
    0.5044, -31.9215, 2693.2100, 0.8475, 10.1466, 1469.2700, 0.7733, 0.1234, 592.9110, 1.4377, -10.5621, 100.1470, 1.4064, -9.9969, 101.6750, 1.0000,
    0.4894, -36.3286, 3512.0400, 0.8139, 9.3726, 1443.8400, 0.8634, 0.6559, 496.0610, 1.3213, -10.6177, 100.0000, 1.4117, -9.9975, 96.7650, 1.0000,
    0.4724, -28.1998, 3093.9700, 0.8685, -9.6661, 1596.9000, 1.1425, 16.0422, 1497.9300, 1.0514, -8.5134, 100.0000, 1.1931, -9.9949, 99.7465, 0.9993,
    0.4147, -28.6936, 3405.5400, 0.8231, 6.4533, 1423.5700, 1.0179, 1.0545, 375.8620, 0.3000, -9.8051, 100.0030, 1.1157, -9.9778, 103.7400, 0.9990,
    0.3777, -23.2048, 2115.7700, 0.8905, 11.3844, 1503.1700, 1.1817, 1.9608, 417.0320, 0.3035, -16.5522, 100.0570, 1.2807, -9.9806, 99.4910, 0.9999,
    0.3731, -26.4353, 2522.9000, 0.8021, 10.1789, 1557.2700, 0.9139, 0.6321, 358.6200, 0.3085, -9.2865, 100.0840, 1.1866, -9.9997, 102.9050, 0.9984,
    0.3538, -21.9945, 2738.4500, 0.9242, 5.3724, 1415.1000, 1.4584, 1.8255, 811.6460, 0.3130, -8.7108, 100.0000, 0.9522, -9.9967, 96.5397, 0.9989,
    0.3283, -25.7517, 2953.7600, 0.8000, 7.3668, 1487.7600, 1.1215, 2.1170, 996.1490, 0.3341, -11.2174, 100.0080, 1.2515, -9.9303, 100.0840, 1.0000,
    0.3029, -26.0956, 2940.5700, 0.8081, 9.2117, 1606.0800, 1.0638, 1.8103, 650.8660, 0.3179, -14.3423, 100.0640, 1.2466, -9.9993, 91.6653, 0.9991,
    0.2754, -19.1423, 2703.3200, 0.8079, 6.1520, 1509.2700, 0.9632, 1.5680, 378.4780, 0.3265, -10.1512, 100.0000, 1.0052, -10.0000, 104.3030, 0.9998,
    0.2636, -23.5815, 2654.9300, 0.8221, -8.4776, 1444.5500, 1.1719, 18.2317, 1483.7200, 0.9264, -13.7667, 100.0130, 1.4428, -9.9948, 101.4270, 0.9995,
    0.2408, -28.1990, 3700.0000, 0.8002, -9.1181, 1281.7100, 0.8843, 19.0551, 1433.0900, 0.7271, -16.8620, 100.0290, 1.4757, -9.9983, 98.0379, 1.0000,
    0.2374, -17.2140, 3446.8200, 0.8281, 4.6932, 1452.1800, 1.2679, 1.9079, 364.9740, 0.3065, -11.4282, 100.5830, 1.0201, -9.9731, 96.8061, 0.9982,
    0.2102, -21.5331, 2927.6300, 0.8798, 7.8838, 1630.4600, 1.2312, 2.8419, 791.8280, 0.3072, -16.1019, 100.0060, 1.4695, -10.0000, 103.0800, 0.9967,
    0.1941, -22.2752, 3427.4400, 0.8320, 8.9883, 1687.6500, 0.9185, 3.7528, 218.0650, 0.3002, -20.7841, 100.0700, 1.1746, -9.9782, 87.5968, 0.9989,
    0.1684, -11.9736, 2595.8200, 0.8020, 6.1339, 1598.2200, 1.1453, 3.1961, 580.7340, 0.3000, -15.6398, 100.0700, 1.2634, -10.0000, 98.9591, 1.0000,
    0.1641, -15.3222, 3340.7500, 0.9735, 6.1857, 1570.5700, 0.9532, 3.2853, 339.8990, 0.3083, -20.9296, 100.0010, 1.4974, -10.0000, 94.4657, 0.9449,
    0.1532, -13.6467, 2880.6400, 0.9665, 6.8882, 1662.5900, 0.9771, 2.7089, 426.9540, 0.3201, -16.7336, 100.0670, 1.4130, -9.9595, 105.9330, 1.0000,
    0.1378, -16.8233, 3569.1900, 0.9070, 7.4853, 1620.7500, 0.5306, 4.9921, 200.4630, 0.6202, -23.6840, 100.0510, 1.3394, -10.0000, 89.0785, 0.9998,
    0.1429, -22.3886, 3175.0300, 0.8158, -11.3969, 1002.8200, 0.4469, 19.9698, 1432.9300, 0.3033, -19.0205, 100.0000, 1.4127, -9.9872, 98.7533, 0.8918,
    0.1322, -21.3501, 3486.6300, 1.0000, -8.8594, 1000.6500, 0.6342, 15.4778, 1335.4500, 0.3608, -18.7137, 100.0110, 1.4237, -9.9956, 98.4845, 0.9981,
    0.1220, -12.4918, 3572.6800, 0.8103, 6.5796, 1552.9300, 1.0451, 5.4590, 271.1100, 0.3031, -16.3480, 100.0220, 0.7127, -9.9470, 103.3910, 0.9892,
    0.1208, -9.8484, 2978.7000, 0.8042, 6.0313, 1423.9800, 0.6290, 6.0572, 200.0000, 0.5263, -22.2229, 100.0390, 1.0263, -9.9991, 84.4742, 1.0000,
    0.1000, -5.6184, 2970.6400, 0.9256, 5.0465, 1318.1900, 0.7355, 4.6570, 225.6150, 0.3638, -21.5999, 100.2230, 1.1897, -9.9318, 98.4510, 1.0000,
    0.1000, -10.1273, 3468.6300, 0.9884, 3.9821, 1744.4400, 1.2656, 3.1897, 758.8910, 0.3015, -17.3834, 100.0200, 1.4736, -10.0000, 101.4920, 1.0000,
    0.1000, -9.2066, 3416.9300, 0.8425, 4.4884, 1682.6000, 0.9671, 2.5291, 406.1830, 0.3000, -19.3951, 100.0230, 1.3584, -9.9875, 99.2437, 0.9998,
};

const float ff_filter[] = {
    0.53, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 10.07, 51, 1.00, -0.20, 150, 0.73, -4.00, 196, 0.39, -1.09, 417, 0.56, 0.29, 775, 0.89, -0.39, 1965, 0.79, 15.49, 3810, 0.79,
    0.56, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 10.88, 53, 1.00, 0.43, 108, 0.62, -4.47, 155, 0.42, -1.77, 492, 0.66, 0.57, 850, 0.90, -0.55, 2169, 0.80, 15.53, 3810, 0.80,
    0.63, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 6.89, 70, 1.00, -0.64, 146, 0.74, -4.10, 198, 0.41, -0.99, 443, 0.68, 0.21, 795, 0.79, -0.70, 2192, 0.74, 15.38, 3810, 0.79,
    0.71, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 9.97, 54, 1.00, -0.84, 135, 0.71, -4.39, 243, 0.38, -0.66, 498, 0.58, 0.87, 727, 0.71, -1.00, 2116, 0.70, 15.54, 3810, 0.79,
    0.77, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 8.98, 66, 1.00, -1.06, 123, 0.70, -4.18, 211, 0.36, -0.49, 455, 0.80, 0.14, 798, 0.86, -1.51, 2059, 0.74, 15.43, 3810, 0.73,
    0.82, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 9.42, 58, 1.00, -1.32, 121, 0.63, -1.38, 232, 0.41, -3.60, 448, 0.34, 1.76, 805, 0.81, -0.68, 2104, 0.70, 16.35, 3810, 0.80,
    0.91, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 12.03, 55, 1.00, -1.44, 80, 0.73, -3.96, 207, 0.48, -1.45, 470, 0.54, 0.22, 705, 0.63, -2.65, 2146, 0.80, 16.63, 3810, 0.77,
    1.03, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 9.90, 60, 1.00, -1.72, 150, 0.67, -1.82, 223, 0.42, -2.46, 426, 0.53, 0.54, 805, 0.78, -2.00, 2171, 0.67, 16.71, 3810, 0.80,
    1.07, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 12.70, 60, 1.00, -1.21, 118, 0.75, -1.54, 166, 0.45, -3.24, 462, 0.41, 1.03, 874, 0.88, -1.98, 2135, 0.73, 16.16, 3810, 0.80,
    1.19, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 9.47, 67, 1.00, -0.92, 128, 0.60, -3.03, 193, 0.41, -1.93, 400, 0.47, 0.63, 904, 0.80, -1.64, 2017, 0.67, 14.77, 3810, 0.80,
    1.27, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 13.81, 53, 1.00, 0.92, 132, 0.76, -5.14, 242, 0.39, -0.09, 492, 0.71, 0.35, 743, 0.65, -1.28, 1841, 0.60, 14.77, 3810, 0.79,
    1.34, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 14.61, 51, 1.00, -0.54, 144, 0.68, -2.46, 224, 0.55, -2.37, 524, 0.52, 1.02, 938, 0.71, -3.23, 1950, 0.67, 15.22, 3810, 0.73,
    1.54, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 13.51, 59, 1.00, -1.96, 150, 0.69, -1.20, 173, 0.40, -4.14, 549, 0.57, 2.37, 871, 0.60, -2.64, 2003, 0.63, 14.90, 3810, 0.79,
    1.54, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 19.36, 50, 1.00, 0.02, 150, 0.60, 0.19, 215, 0.41, -3.83, 418, 0.37, 1.69, 911, 0.88, -3.78, 2200, 0.50, 17.45, 3810, 0.80,
    1.83, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 12.98, 64, 1.00, -1.56, 150, 0.77, -0.64, 197, 0.60, -3.27, 413, 0.76, 0.72, 796, 0.89, -2.72, 1859, 0.50, 14.59, 3810, 0.80,
    1.92, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 13.57, 69, 1.00, 1.70, 149, 0.80, -4.14, 200, 0.44, -1.74, 578, 0.52, 0.88, 920, 0.60, -2.68, 1873, 0.66, 13.69, 3810, 0.73,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 18.47, 66, 1.00, 1.41, 150, 0.80, -2.96, 199, 0.40, -1.86, 467, 0.73, 0.18, 888, 0.85, -3.36, 2157, 0.69, 16.44, 3810, 0.80,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 16.53, 63, 1.00, 1.03, 103, 0.75, -1.63, 181, 0.36, -1.90, 491, 0.72, 0.07, 715, 0.84, -2.45, 1917, 0.74, 13.90, 3810, 0.69,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 20.48, 52, 1.00, 3.16, 99, 0.78, -1.62, 238, 0.38, -1.12, 583, 0.53, 0.59, 872, 0.90, -1.88, 2200, 0.77, 14.35, 3810, 0.80,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 21.00, 62, 1.00, 0.96, 113, 0.68, -0.04, 203, 0.40, -2.29, 586, 0.53, 1.36, 868, 0.73, -3.69, 2185, 0.70, 16.81, 3810, 0.69,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 24.27, 61, 1.00, -1.51, 128, 0.60, 4.34, 237, 0.41, -3.91, 401, 0.59, 1.32, 925, 0.69, -4.08, 2125, 0.66, 19.17, 3810, 0.74,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 21.88, 70, 1.00, -1.84, 142, 0.61, 4.60, 157, 0.37, -2.82, 435, 0.64, 1.25, 850, 0.89, -3.92, 2176, 0.66, 18.02, 3810, 0.68,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 26.76, 59, 1.00, 5.18, 95, 0.76, -0.06, 194, 0.30, 0.46, 454, 0.80, -1.05, 962, 0.60, -4.20, 2148, 0.75, 18.08, 3810, 0.52,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 26.75, 66, 1.00, 1.22, 131, 0.67, 2.29, 157, 0.42, -2.44, 585, 0.67, 3.00, 746, 0.61, -3.04, 1970, 0.67, 19.86, 3810, 0.76,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 27.38, 65, 1.00, 1.09, 121, 0.80, 3.71, 150, 0.46, 0.27, 400, 0.67, -0.43, 705, 0.62, -4.37, 2200, 0.75, 19.51, 3810, 0.50,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 24.49, 54, 1.00, 10.00, 80, 0.60, -0.16, 191, 0.60, 1.99, 442, 0.50, -0.75, 952, 0.90, -3.72, 2189, 0.71, 19.31, 3810, 0.50,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 29.86, 66, 1.00, 4.16, 80, 0.78, 3.36, 150, 0.46, 1.03, 451, 0.45, 1.34, 986, 0.77, -3.67, 1874, 0.50, 19.65, 3810, 0.59,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 30.00, 64, 1.00, 6.24, 89, 0.71, 1.77, 157, 0.39, 1.85, 419, 0.30, 1.27, 1000, 0.90, -2.75, 1800, 0.70, 20.00, 3810, 0.68,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 29.13, 66, 1.00, 6.79, 80, 0.62, 2.17, 173, 0.40, 2.74, 473, 0.46, -0.24, 715, 0.74, -2.67, 1843, 0.79, 19.91, 3810, 0.56,
    2.00, 13.00, 17093, 0.50, -22.35, 6186, 1.40, -8.00, 7369, 5.00, 29.97, 60, 1.00, 10.00, 81, 0.67, 3.16, 212, 0.53, 0.73, 520, 0.70, 1.23, 700, 0.62, -3.08, 1995, 0.79, 19.97, 3810, 0.50,

};

const s16 cmp_fgq_table[] = {
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 135, 3362, 508, 20, -23224, 600, 228, 6822, 504, 600, 3908, 714, 838, -1861, 879, 1219, -23, 952,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 119, -296, 1033, 20, -17392, 600, 219, 6984, 507, 556, 2157, 671, 895, 10, 869, 1443, -9, 909,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 102, -1246, 589, 20, -23787, 600, 215, 6281, 504, 634, 1245, 706, 806, 586, 837, 1393, -133, 728,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 100, -3321, 1648, 20, -17296, 600, 233, 5347, 501, 591, 1318, 746, 863, -45, 852, 1365, -38, 972,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 101, -3806, 1685, 20, -10938, 600, 267, 4942, 518, 460, -582, 596, 1000, 3081, 710, 1213, -1516, 812,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 125, -8693, 500, 20, -10162, 600, 200, 6115, 521, 692, 550, 710, 959, 1646, 791, 1170, -224, 980,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 104, -7122, 1725, 20, -7446, 600, 252, 2428, 500, 674, -43, 641, 954, 966, 807, 1197, -67, 742,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 100, -9614, 1094, 20, -23912, 600, 220, 1371, 812, 647, 967, 400, 994, 1926, 716, 1500, -155, 845,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 100, -10985, 1291, 20, -15846, 600, 257, 855, 826, 696, 74, 452, 997, 1627, 821, 1110, 0, 733,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 101, -11897, 1049, 20, -19665, 600, 226, 705, 720, 689, -251, 579, 989, 1916, 950, 1181, -69, 838,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 120, -13717, 1279, 20, -12938, 600, 229, -1211, 844, 625, -1347, 664, 997, 3437, 729, 1408, -32, 997,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 132, -12869, 1159, 20, -17167, 600, 260, -755, 823, 592, -2286, 592, 972, 2984, 709, 1386, 0, 735,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 111, -14927, 1216, 20, -25000, 600, 214, -2887, 823, 635, -3149, 703, 1000, 3057, 877, 1445, -246, 756,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 127, -16386, 1191, 20, -15900, 600, 240, -3151, 909, 624, -4096, 710, 991, 4012, 828, 1100, -1081, 700,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 109, -19769, 1553, 20, -11990, 600, 222, -6086, 865, 700, -5878, 663, 1000, 5209, 859, 1161, -361, 777,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 126, -21083, 1120, 20, 10000, 600, 279, -3353, 703, 620, -4373, 632, 1000, 3205, 853, 1103, -847, 786,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 103, -29358, 787, 20, -23164, 600, 381, -2415, 806, 642, -3532, 797, 969, 1054, 915, 1369, 0, 761,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 127, -24285, 1301, 20, -24364, 600, 287, -7794, 866, 700, -8581, 791, 958, 7908, 758, 1278, -2239, 755,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 119, -28429, 1187, 20, -12810, 600, 290, -6958, 809, 657, -6599, 765, 994, 2839, 708, 1352, -130, 884,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 145, -27420, 1373, 20, -23913, 600, 330, -9346, 951, 686, -8174, 742, 1000, 6064, 829, 1358, -1730, 822,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 140, -26975, 902, 20, -23902, 600, 389, -6546, 544, 590, -1039, 642, 801, -1430, 983, 1395, -18, 816,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 139, -26075, 1480, 20, -22957, 600, 302, -13356, 772, 653, -5461, 727, 897, 323, 965, 1280, -17, 968,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 140, -27334, 1410, 20, -25000, 600, 308, -13512, 701, 643, -5546, 737, 951, 647, 943, 1420, -182, 915,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 139, -24516, 1654, 20, -10280, 600, 289, -17908, 700, 637, -4271, 712, 942, -1458, 973, 1317, -153, 913,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 149, -24699, 1714, 20, -23814, 600, 304, -19903, 867, 617, -5949, 662, 969, 881, 835, 1252, -2074, 795,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 149, -25345, 1685, 20, -22921, 600, 309, -20000, 817, 621, -7612, 584, 905, 3363, 903, 1387, -3719, 718,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 147, -28025, 1422, 20, -24479, 600, 200, -17295, 503, 451, -9054, 734, 953, -3979, 1000, 1321, -438, 939,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 149, -27105, 1476, 20, -25000, 600, 200, -19497, 503, 459, -8511, 727, 918, -3932, 1000, 1369, 0, 706,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 149, -29346, 1406, 20, -23865, 600, 200, -14104, 501, 452, -12602, 675, 939, -5059, 992, 1227, 0, 736,
    1258, 3611, -32000, 1000, 4000, 0, 1000, 10378, 0, 1000, 2740, -12500, 850, 147, -28317, 1406, 20, -23053, 600, 200, -17200, 500, 451, -11591, 567, 930, -4418, 896, 1162, -1421, 996,
};

const s16 eq_fgq_table[] = {
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 800, 687, 500, 400, -692, 400, 256, 0, 999,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 813, -854, 500, 516, 1400, 838, 284, 0, 851,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 835, -1297, 515, 469, 3026, 823, 200, 683, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 899, -1412, 891, 511, 2836, 560, 200, 853, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 899, -1878, 500, 478, 3758, 727, 300, 1765, 755,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 999, -1955, 885, 443, 3909, 652, 200, 2811, 701,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 858, -2460, 775, 497, 5429, 572, 253, 3826, 700,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 964, -3218, 791, 467, 4947, 632, 297, 5116, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 900, -3405, 684, 524, 6351, 557, 300, 5481, 701,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 857, -4539, 749, 526, 7681, 539, 267, 6229, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 850, -4063, 889, 493, 7290, 551, 266, 7968, 700,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 850, -6366, 816, 588, 9803, 476, 299, 8468, 762,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 965, -3006, 763, 506, 8341, 559, 297, 10142, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 911, -2877, 808, 508, 8866, 543, 300, 11335, 705,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 900, -4581, 873, 565, 10357, 436, 300, 11401, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 951, -2636, 840, 510, 9819, 472, 299, 12381, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 1000, -2009, 806, 498, 10316, 503, 300, 14165, 706,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 944, -2484, 790, 493, 10602, 483, 294, 15431, 700,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 948, -1685, 756, 492, 11213, 488, 283, 15876, 702,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 996, -1169, 899, 463, 10897, 476, 295, 17723, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 800, 1988, 765, 401, 12109, 508, 245, 18136, 700,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 854, 2601, 560, 419, 10716, 530, 290, 19593, 704,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 801, 5331, 599, 414, 10072, 654, 300, 19529, 805,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 856, 1624, 775, 475, 13870, 497, 276, 19101, 710,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 891, -205, 822, 521, 15635, 470, 281, 18306, 734,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 961, -213, 696, 565, 15661, 508, 292, 19402, 714,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 800, 2735, 818, 547, 14574, 478, 273, 19177, 766,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 815, 3871, 523, 576, 12613, 470, 300, 19218, 757,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 1000, 2691, 500, 648, 14241, 409, 294, 19334, 699,
    1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 100, 0, 1000, 941, 10586, 618, 452, 11398, 409, 250, 17559, 842,
};


#endif
