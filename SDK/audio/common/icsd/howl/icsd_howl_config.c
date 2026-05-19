#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
        TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_HOWLING_DET_ENABLE)
#include "icsd_adt.h"
#include "icsd_howl.h"

int (*howl_printf)(const char *format, ...) = _howl_printf;

void howl_train_init(__howl_config *_howl_config)
{
    _howl_config->hd_path1_thr = 0;//113; //110;
    _howl_config->hd_diff_pwr_thr = 0;//50;//11;//18;//越大反应越慢，误触会小影响hd_frame_max的成立
    _howl_config->hd_maxvld2nd  = 0;//42;
    _howl_config->hd_maxvld   = 0;//120;
    _howl_config->hd_maxvld1  = 0;//115;
    _howl_config->hd_maxvld2  = 0;//130;
    _howl_config->hd_sat_thr  = 0;//8000;//饱和阈值
    _howl_config->loud_thr    = 0; // 大声压认为是啸叫。
    _howl_config->loud_thr1   = 0; // 大声压认为是啸叫。
    _howl_config->loud_thr2   = 0; // 大声压认为是啸叫。
}


void howl_config_init(__howl_config *_howl_config)
{
    if (howl_train_en) {
        howl_train_init(_howl_config);
    } else {
        _howl_config->hd_path1_thr = 115;
        _howl_config->hd_diff_pwr_thr = 15;//20;//越大反应越慢，误触会小影响hd_frame_max的成立
        _howl_config->hd_maxvld2nd  = 75;
        _howl_config->hd_maxvld   = 85;
        _howl_config->hd_maxvld1  = 115;
        _howl_config->hd_maxvld2  = 78;
        _howl_config->hd_sat_thr = 7208;////饱和阈值
        _howl_config->loud_thr    = 79; // 大声压认为是啸叫。
        _howl_config->loud_thr1   = 108; // 大声压认为是啸叫。
        _howl_config->loud_thr2   = 82; // 大声压认为是啸叫。
    }

    _howl_config->hd_step_pwr = 4;  //最终功率增长 dB
    _howl_config->hd_rise_pwr = 88;
    _howl_config->hd_rise_cnt = 120;//长啸叫计数
    _howl_config->harm_rel_thr_l = 1.2;//1.3;
    _howl_config->harm_rel_thr_h = 2.9;
    _howl_config->hd_det_thr = 2;//4;//满足条件 hd_det_thr次就触发 最小值为4
    _howl_config->hd_det_cnt  = 2; //cnt 超过该值 && maxvld，force是啸叫。
    _howl_config->maxvld_lat_t2_add = 0.1;

    _howl_config->hd_mic_sel = 0; // 0:REF 1:ERR
    _howl_config->hd_mode = 0; // 1 for anco, 0 for ref
    _howl_config->hd_scale = 0.2;//0.25 数字增益
    _howl_config->hd_maxind_thr1 = 7;//频点1： hd_maxind_thr1 * 46875/128 小于这个频点要求更严，次数更多才触发
    _howl_config->hd_maxind_thr2 = 28;//频点2： hd_maxind_thr2 * 46875/128 大于这个频点要求更松，次数更少就触发
}

const struct howl_function HOWL_FUNC_t = {
    .howl_config_init = howl_config_init,
    .anc_debug_malloc = anc_debug_malloc,
    .anc_debug_free = anc_debug_free,
};
struct howl_function *HOWL_FUNC = (struct howl_function *)(&HOWL_FUNC_t);

const u8 howl_log_en     = ICSD_HOWL_LOG_EN;
const u8 icsd_howl_debug = 0;	//误触发调试: 啸叫之后会强行挂起ADT
const u8 howl_debug_dlen = 10;	//ramsize = (96 * howl_debug_dlen) byte

const u8 howl_ft_debug = 0;     //误触发原始数据调试

const u8 howl_train_debug     = 0;
const u8 howl_train_en        = 0;
const u8 howl_parm_correction = 0;

#endif
