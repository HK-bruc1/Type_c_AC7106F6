#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE)
#include "icsd_wind.h"
#include "icsd_adt.h"
#include "anc_ext_tool.h"
#include "audio_anc_common.h"
#include "icsd_wind_app.h"

//====================风噪检测配置=====================
const u8 ICSD_WIND_PHONE_TYPE  = SDK_WIND_PHONE_TYPE;
const u8 ICSD_WIND_MIC_TYPE    = SDK_WIND_MIC_TYPE;
const u8 wdt_log_en     = ICSD_WDT_LOG_EN; //调试离线log使能
const u8 icsd_wdt_debug = 0; //调试使能
const u8 wdt_debug_dlen = 5;//调试帧数:ramsize = (36 * howl_debug_dlen) byte
const u8 wdt_debug_thr  = 1; //调试触发阈值:wind_lvl >= wdt_debug_thr 触发调试log
const u8 wdt_debug_type = 0; //0:参数调试  1:原始数据调试

//====================离线调试:打印值放大了100倍=====================
const float icsd_wdt_sen = 0.8;//lff_tlk

const u8 wdt_lff_tlk_debug = WDT_LFF_TLK_DEBUG_EN;//1:触发调试 2:误触调试
const u8 wdt_lff_lfb_debug = WDT_LFF_LFB_DEBUG_EN;//
const u8 wdt_lff_rff_debug = WDT_LFF_RFF_DEBUG_EN;//



int (*win_printf)(const char *format, ...) = _win_printf;

void wind_lff_lfb_config(__wind_lfflfb_config *wind_lfflfb_config)
{
    wind_lfflfb_config->wind_corr_select = 0;
    wind_lfflfb_config->wind_fcorr_fpoint = 10;
    wind_lfflfb_config->wind_fcorr_min_thr = 0.6;
    wind_lfflfb_config->wind_ref2err_cnt = 13;
    wind_lfflfb_config->wind_sat_thr = 80000;
    wind_lfflfb_config->wind_lpf_alpha = 0.0625;
    wind_lfflfb_config->wind_hpf_alpha = 0.001;
    wind_lfflfb_config->wind_iir_alpha = 16;
    wind_lfflfb_config->correrr_thr = 380;
    wind_lfflfb_config->wind_max_min_diff_thr = 42;
    wind_lfflfb_config->wind_timepwr_diff_thr0 = 10;
    wind_lfflfb_config->wind_timepwr_diff_thr1 = 150;
    wind_lfflfb_config->wind_ref2err_diff_thr = 30;
    wind_lfflfb_config->wind_ref2err_diffmin_thr = 15;
    wind_lfflfb_config->wind_margin_dB = -9;
    wind_lfflfb_config->wind_pwr_ref_thr = 35;
    wind_lfflfb_config->wind_pwr_err_thr = 50;
    wind_lfflfb_config->wind_lowfreq_point = 10;
    wind_lfflfb_config->wind_pwr_cnt_thr = 0;
    wind_lfflfb_config->icsd_wind_num_thr2 = 3;
    wind_lfflfb_config->wind_stable_cnt_thr = 1;
    wind_lfflfb_config->wind_lvl_scale = 2;
    wind_lfflfb_config->wind_num_thr = 4;
    wind_lfflfb_config->wind_out_mode = 1;//1:滤波输出(滤波模式) 0:连续3包无风输出0(快速模式)
    for (int i = 0; i < 25; i++) {
        wind_lfflfb_config->wind_ref_cali_table[i] = 0;
    }
}

void wind_config_init(__wind_config *_wind_config)
{
    __wind_config *wind_config = _wind_config;

#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE && TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
    struct __anc_ext_wind_det_cfg *cfg = anc_ext_ear_adaptive_cfg_get()->wind_det_cfg;
    if (cfg) {
        wind_config->msc_lp_thr    = cfg->msc_lp_thr;
        wind_config->msc_mp_thr    = cfg->msc_mp_thr;
        wind_config->corr_thr      = cfg->corr_thr;//tws 不需要
        wind_config->cpt_1p_thr  = cfg->cpt_1p_thr;
        wind_config->tlk_pwr_thr   = 28;//现在工具没传这个值
        wind_config->ref_pwr_thr   = cfg->ref_pwr_thr;
        wind_config->wind_iir_alpha = cfg->wind_iir_alpha;
        wind_config->wind_lvl_scale = cfg->wind_lvl_scale;
        wind_config->icsd_wind_num_thr2 = cfg->icsd_wind_num_thr2;
        wind_config->icsd_wind_num_thr1 = cfg->icsd_wind_num_thr1;
        if ((ICSD_WIND_MIC_TYPE == ICSD_WIND_LFF_RFF) || (ICSD_WIND_MIC_TYPE == ICSD_WIND_LFF_LFB_RFF)) {
            wind_config->pwr_mode = 0;
        } else {
            wind_config->pwr_mode = 1;
        }
#if 0
        printf("cfg->msc_lp_thr:%d\n", (int)(100 * cfg->msc_lp_thr));
        printf("cfg->msc_mp_thr:%d\n", (int)(100 * cfg->msc_mp_thr));
        printf("cfg->corr_thr:%d\n", (int)(100 * cfg->corr_thr));
        printf("cfg->cpt_1p_thr:%d\n", (int)(100 * cfg->cpt_1p_thr));
        printf("cfg->ref_pwr_thr:%d\n", (int)(100 * cfg->ref_pwr_thr));
        printf("cfg->wind_iir_alpha:%d\n", (int)(100 * cfg->wind_iir_alpha));
        printf("cfg->wind_lvl_scale:%d\n", (int)(100 * cfg->wind_lvl_scale));
        printf("cfg->icsd_wind_num_thr2:%d\n", (int)(100 * cfg->icsd_wind_num_thr2));
        printf("cfg->icsd_wind_num_thr1:%d\n", (int)(100 * cfg->icsd_wind_num_thr1));
#endif
    } else
#endif
    {
        wind_config->msc_lp_thr    = 2.4;
        wind_config->msc_mp_thr    = 2.4;
        wind_config->corr_thr      = 0.4;//tws 不需要
        wind_config->cpt_1p_thr  = 20;
        wind_config->ref_pwr_thr   = 30;
        wind_config->tlk_pwr_thr   = 28;//现在工具没传这个值
        wind_config->wind_iir_alpha = 16;
        wind_config->wind_lvl_scale = 2;
        wind_config->icsd_wind_num_thr2 = 3;
        wind_config->icsd_wind_num_thr1 = 1;
        if ((ICSD_WIND_MIC_TYPE == ICSD_WIND_LFF_RFF) || (ICSD_WIND_MIC_TYPE == ICSD_WIND_LFF_LFB_RFF)) {
            wind_config->pwr_mode = 0;
        } else {
            wind_config->pwr_mode = 1;
        }
    }

}

void icsd_wind_demo()
{
    static int *alloc_addr = NULL;
    struct icsd_win_libfmt libfmt;
    struct icsd_win_infmt  fmt;
    icsd_wind_get_libfmt(&libfmt);
    win_printf("WIND RAM SIZE:%d\n", libfmt.lib_alloc_size);
    if (alloc_addr == NULL) {
        alloc_addr = zalloc(libfmt.lib_alloc_size);
    }
    fmt.alloc_ptr = alloc_addr;
    icsd_wind_set_infmt(&fmt);
}


const struct wind_function WIND_FUNC_t = {
    .wind_lff_lfb_config = wind_lff_lfb_config,
    .wind_config_init = wind_config_init,
    .HanningWin_pwr_float = icsd_HanningWin_pwr_float,
    .HanningWin_pwr_s1 = icsd_HanningWin_pwr_s1,
    .FFT_radix256 = icsd_FFT_radix256,
    .FFT_radix128 = icsd_FFT_radix128,
    .complex_mul  = icsd_complex_mul_v2,
    .complex_div  = icsd_complex_div_v2,
    .pxydivpxxpyy = icsd_pxydivpxxpyy,
    .log10_float  = icsd_log10_anc,
    .cal_score    = icsd_cal_score,
    .icsd_adt_tws_msync = icsd_adt_tws_msync,
    .icsd_adt_tws_ssync = icsd_adt_tws_ssync,
    .icsd_wind_run_part2_cmd = icsd_wind_run_part2_cmd,
    .icsd_adt_wind_part1_rx = icsd_adt_wind_part1_rx,
    .anc_debug_malloc = anc_debug_malloc,
    .anc_debug_free = anc_debug_free,
};

struct wind_function *WIND_FUNC = (struct wind_function *)(&WIND_FUNC_t);
#endif/*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
