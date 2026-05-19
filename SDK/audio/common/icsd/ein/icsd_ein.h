#ifndef _ICSD_EIN_H
#define _ICSD_EIN_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "timer.h"
#include "math.h"
#include "icsd_common.h"
#include "icsd_common_v2.h"

#if EIN_PRINTF_EN
#define _ein_printf printf
#else
#define _ein_printf icsd_printf_off
#endif
extern int (*ein_printf)(const char *format, ...);

struct icsd_ein_libfmt {
    int lib_alloc_size;  //算法ram需求大小
};

struct icsd_ein_infmt {
    u8 ein_state;
    void *alloc_ptr;     //外部申请的ram地址
    int lib_alloc_size;  //算法ram需求大小
};

typedef struct {
    u8 tot_checkin_cnt_thr;

    float thr1_trn2in_pz;
    float thr1_anc2in_pz;
    float thr1_pnc2in_pz;

    float thr1_trn2out_pz;
    float thr1_anc2out_pz;
    float thr1_pnc2out_pz;

    float thr2_trn2out_pz;
    float thr2_anc2out_pz;
    float thr2_pnc2out_pz;

    float thr1_trn2out_pz_spk;
    float thr1_anc2out_pz_spk;
    float thr1_pnc2out_pz_spk;

    float thr1_trn2out_sz;
    float thr1_anc2out_sz;
    float thr1_pnc2out_sz;

    float thr2_trn2out_sz;
    float thr2_anc2out_sz;
    float thr2_pnc2out_sz;

    float pzcorr_thr_anc;
    float pzcorr_thr_pnc;
    float pzcorr_thr_trans;
} __ein_config;

struct ein_function {
    void (*ein_config_init)(__ein_config *_ein_config);
    void (*HanningWin_pwr_s1)(s16 *input, int *output, int len);
    void (*FFT_radix256)(int *in_cur, int *out);
    void (*FFT_radix64)(int *in_cur, int *out);
    void (*complex_mul)(int *input1, float *input2, float *out, int len);
    void (*complex_div)(float *input1, float *input2, float *out, int len);
    double (*log10_float)(double x);
};
extern struct ein_function *EIN_FUNC;

typedef struct {
    s16 *adc_ref_buf;
    s16 *adc_err_buf;
    s16 *inear_sz_spko_buf;
    s16 *inear_sz_emic_buf;
    s16 *inear_sz_dac_buf;
    u8 resume_mode;
    u8 anc_onoff;
    u8 run_mode;
} __icsd_ein_run_parm;

typedef struct {
    u8 ein_output;
    void *ein_alg_bt_inf;
    u16 ein_alg_bt_len;
} __icsd_ein_output;

void icsd_ein_get_libfmt(struct icsd_ein_libfmt *libfmt);
void icsd_ein_set_infmt(struct icsd_ein_infmt *fmt);
void icsd_alg_ein_run(__icsd_ein_run_parm *run_parm, __icsd_ein_output *output);


extern const u8 ein_train;
extern const u8 EIN_BT_INF_EN;
extern const float pz_in_anc[28];
extern const float pz_out_anc[28];
extern const float pz_in_off[28];
extern const float pz_out_off[28];
extern const float pz_in_trans[28];
extern const float pz_out_trans[28];
extern const float sz_in[14];
extern const float sz_out[14];


extern char lib_ein_version[];
#endif
