#ifndef _ANC_SDE_H
#define _ANC_SDE_H

#include "icsd_common_v2.h"

#define sFLEN 25
#define TAP 10

typedef struct {
    u8    fgq_type[TAP];
    u8    pz_cmp_en;
    u8    alg_sel;
    u8    param0;
    int   downsample;
    int   loop_max;
    int   gain_cal_f1;		// total gain cal point 1
    int   gain_cal_f2;		// total gain cal point 2
    int   mv_tap;

    float fitness;

    float freq_all[TAP];
    float gain_all[TAP];
    float qval_all[TAP];
    float freq_range[TAP * 2];
    float gain_range[TAP * 2];
    float qval_range[TAP * 2];

    float total_gain_limit;
    float gain_cmp;	// gain cmp
    float gain_pz;
    float gain_sz;
    float stop_fitness;
    float hzs[2 * TAP * sFLEN];
    float hz_temp[2 * sFLEN * 4];
    float hz_tap_p1[4 * sFLEN * 4];
    float exp_w1[2 * sFLEN * 4];
    float exp_w2[2 * sFLEN * 4];
    float fd_tmp[sFLEN * 2];

    float weight[2 * sFLEN];
    float mse_tar[2 * sFLEN];

    float *hp_cmp;

} __sDe_handler;
extern __sDe_handler *ICSD_sDe_handler;


float sDE_cal_delta_tar2cur(float *freqs, float *target, float *hz_all, float freq_loop, int downsample);
void sDE_fgq_candidate(float *fgq_cur, float *fgq_candidate, float gain_delta, int loopcnt, const float *freq_range, const float *gain_range, const float *qval_range);
void sDe_run(float *freqs, float *target, float *mse_pwr, float *weight, float *fgq_update, float *ff_iir_gain, float *ff_wz, float *fb_gz, __sDe_handler *sDe_handler);
float sDE_cal_mse(float *hz, float *target, float *mse_pwr, float *weight, int len, int downsample, u8 cal_type);
void sDe_pzsz(float *freq_ind, float *anc_tg, float *pz_coef, float *sz_coef, float *fgq_update, float *ff_wz, float *fb_gz, float *weight, float *mse_pwr, __sDe_handler *sDe_handler);

#endif
