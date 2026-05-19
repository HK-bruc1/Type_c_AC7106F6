#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE)

#include "icsd_ein.h"
#include "icsd_adt.h"

int (*ein_printf)(const char *format, ...) = _ein_printf;


void ein_config_init(__ein_config *_ein_config)
{
    ein_printf("ein_config_init \n");
    _ein_config->tot_checkin_cnt_thr = 5;
    _ein_config->thr1_trn2in_pz 	 = 0.51;
    _ein_config->thr1_anc2in_pz 	 = 0.52;//0.85;
    _ein_config->thr1_pnc2in_pz 	 = 0.53;
    _ein_config->thr1_trn2out_pz	 = 0.75;
    _ein_config->thr1_anc2out_pz	 = 0.81;//0.85;
    _ein_config->thr1_pnc2out_pz	 = 0.90;
    _ein_config->thr2_trn2out_pz	 = 0.76;
    _ein_config->thr2_anc2out_pz	 = 0.82;//0.85;
    _ein_config->thr2_pnc2out_pz	 = 0.90;
    _ein_config->thr1_trn2out_pz_spk = 0.6;
    _ein_config->thr1_anc2out_pz_spk = 0.6;
    _ein_config->thr1_pnc2out_pz_spk = 0.7;
    _ein_config->thr1_trn2out_sz 	 = 0.5;
    _ein_config->thr1_anc2out_sz 	 = 0.45;
    _ein_config->thr1_pnc2out_sz 	 = 0.5;
    _ein_config->thr2_trn2out_sz 	 = 0.5;
    _ein_config->thr2_anc2out_sz 	 = 0.5;
    _ein_config->thr2_pnc2out_sz 	 = 0.5;
    _ein_config->pzcorr_thr_anc	 	 = 0.01;//正常0.5
    _ein_config->pzcorr_thr_pnc 	 = 0.1;
    _ein_config->pzcorr_thr_trans    = 0.01;
}

const struct ein_function EIN_FUNC_t = {
    .ein_config_init = ein_config_init,
    .HanningWin_pwr_s1 = icsd_HanningWin_pwr_s1,
    .FFT_radix64 = icsd_FFT_radix64,
    .FFT_radix256 = icsd_FFT_radix256,
    .complex_mul  = icsd_complex_mul_v2,
    .complex_div  = icsd_complex_div_v2,
    .log10_float  = icsd_log10_anc,
};
struct ein_function *EIN_FUNC = (struct ein_function *)(&EIN_FUNC_t);

const u8 ein_train 		  = 0;
const u8 EIN_BT_INF_EN    = 0;

//外部播放白噪声
const float pz_in_anc[28] = {-18.47, -19.84, -21.68, -23.31, -22.47, -21.21, -20.13, -18.61, -17.12, -15.05, -13.00, -11.25, -10.09, -8.99, -8.17, -7.78, -7.55, -7.42, -7.54, -7.69, -7.84, -7.97, -8.16, -8.47, -8.77, -8.93, -9.14, -9.35};
const float pz_out_anc[28] = {-1.12, -0.98, -0.95, -0.89, -0.87, -1.03, -1.25, -1.40, -1.55, -2.02, -2.60, -3.10, -3.53, -3.59, -3.30, -2.94, -2.76, -2.65, -2.38, -1.89, -1.48, -1.24, -1.06, -0.86, -0.73, -0.58, -0.50, -0.23};

const float pz_in_off[28] = {-2.74, -2.91, -3.07, -3.46, -4.04, -4.47, -4.81, -5.26, -5.89, -6.42, -6.77, -7.10, -7.43, -7.80, -8.11, -8.34, -8.57, -8.82, -9.11, -9.34, -9.48, -9.62, -9.75, -9.97, -10.19, -10.34, -10.49, -10.69};
const float pz_out_off[28] = {-0.01, -0.01, -0.02, -0.04, -0.10, -0.12, -0.08, -0.18, -0.38, -0.27, 0.20, 0.75, 1.08, 1.12, 0.88, 0.61, 0.51, 0.50, 0.55, 0.51, 0.35, 0.46, 0.79, 1.09, 1.29, 1.20, 0.92, 0.73};

const float pz_in_trans[28] = {5.38, 5.82, 6.10, 6.26, 6.38, 6.45, 6.36, 6.14, 5.83, 5.50, 5.13, 4.60, 3.96, 3.38, 2.66, 1.97, 1.39, 0.84, 0.31, -0.28, -0.91, -1.47, -2.04, -2.54, -2.99, -3.32, -3.53, -3.67};
const float pz_out_trans[28] = {-1.49, -0.35, 0.73, 1.65, 2.49, 3.59, 4.59, 5.35, 5.84, 5.98, 6.13, 6.64, 7.04, 6.94, 6.99, 7.34, 7.45, 7.14, 7.04, 7.39, 7.66, 7.58, 7.51, 7.60, 8.08, 8.24, 8.02, 7.86};

const float sz_in[14] = {-18.26, -14.01, -11.86, -10.58, -11.08, -12.10, -12.70, -13.50, -14.24, -15.64, -15.86, -17.31, -18.77, -19.49};
const float sz_out[14] = {-29.33, -28.38, -28.35, -27.79, -27.35, -27.09, -26.09, -26.16, -25.50, -24.09, -23.42, -22.62, -22.13, -19.71};


#endif/*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
