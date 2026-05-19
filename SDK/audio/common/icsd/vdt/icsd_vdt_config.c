#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE)
#include "icsd_vdt.h"
#include "icsd_adt.h"



const u8 ICSD_VDT_ALG_BT_INF = 0;
const u8 ICSD_VDT_BT_DATA = 0;
//调试信息使能
const u16 ADT_DEBUG_INF = 0;//ADT_INF_5 | ADT_INF_1 ;//| ADT_INF_12 ;//| ADT_INF_9; //state
const u8 VDT_TRAIN_EN = 0;

//==============================================//
//    智能免摘参数配置
//==============================================//
u8    ADT_PATH_CONFIG = ADT_PATH_3M_EN;

const float vdt_refgain = 1;
const float vdt_errgain = 1;
const float vdt_tlkgain = 1;
const float vdt_dmahgain = 2;

const u16 adt_dov_thr = 100;
const u16 adt_dov_diff_thr = 3;
const u8 final_speech_even_thr = 4;     //dov
const u8 final_speech_even_thr_env = 8;
const u8 final_speech_even_thr_T = 4;     //dov
const u8 final_speech_even_thr_env_T = 8;
const u8 env_spl_flt_thr = 200;
const float dc_alpha_thr = 0.125;
const u8 ana_len_start = 2;//ana_len_start * 92hz
const u8 icsd_ana_len = 8;//max:16
const float wdac_senc = 0.7; //越小播音乐时灵敏度越高
const float ang_thr[10] = {0, 15, 16, 13, 13, 13, 12, 10, 10, 10};
const float wref[8]   = {0.886847, 0.516234, 0.456076, 0.378177, 0.312354, 0.277951, 0.253154, 0.244882}; //播放外部噪声 PNC
const float wref_tran[8]   = {1.016303, 0.714220, 0.742538, 0.827135, 0.875243, 0.871669, 0.843512, 0.832892}; //播放外部噪声 TRANS
const float wref_ancs[8]   = {0.541736, 0.101034, 0.064317, 0.071613, 0.066163, 0.057105, 0.051868, 0.054195}; //播放外部噪声 ANC ON
const float wdac[8] = { 0.13, 0.21, 0.37, 0.76, 0.84, 0.79, 0.72, 0.65}; //播放音乐/100
const float fdac[8] = { 0.18, 0.051, 0.02, 0.02, 0.02, 0.02, 0.02, 0.05}; //播放音乐/1000 手动修改
//const u8 pnc_ref_pwr_thr = 60;
//const u8 pnc_err_pwr_thr = 100;
//const u8 tpc_ref_pwr_thr = 60;
//const u8 tpc_err_pwr_thr = 100;
//const u8 anc_ref_pwr_thr = 60;
//const u8 anc_err_pwr_thr = 80;
const u8 adt_errpxx_dB_thr = 100;//60;
const u8 adt_err_zcv_thr = 0;//12;
const u8 pwr_inside_even_flag0_thr = 4; //pwr
const u8 angle_talk_even_flag0_thr = 5; //angle_tlk
const u8 pwr_inside_even_flag0_thr_T = 4; //pwr
const u8 angle_talk_even_flag0_thr_T = 5; //angle_tlk
const u8 angle_err_even_flag0_thr = 3;//4;  //angle_err
const float f1f2_angsum_thr = -200;//401;
const float target_mscohere1_thr = 0.5;
const float target_out_thr = 0;
const u8 msco_start = 3;
const u8 msco_end = 7;

float angle_fb_dn[5] = {15, 15, 15, 15, 15};
float angle_fb_up[5] = {-130, -130, -130, -130, -130};
u8 icsd_get_ang_flag(float *_target_angle)
{
    u8 af1 = _target_angle[1] > angle_fb_dn[1] || _target_angle[1] < angle_fb_up[1];
    u8 af2 = _target_angle[2] > angle_fb_dn[2] || _target_angle[2] < angle_fb_up[2];
    u8 af3 = _target_angle[3] > angle_fb_dn[3] || _target_angle[3] < angle_fb_up[3];
    u8 ang_flag = af1 && af2 && af3;
    return ang_flag;
}
#if VDT_USE_ANCDMAL_DATA
const u16 VDT_EXT_CONIFG = VDT_EXT_DIT | VDT_EXT_COR;// | VDT_EXT_NOISE_PWR;
const u8 VDT_USE_ANCDMAL = 1;
#else
const u16 VDT_EXT_CONIFG = 0;// | VDT_EXT_NOISE_PWR;
const u8 VDT_USE_ANCDMAL = 0;
#endif

const float OE_thr = -10;
//DIT
const float icsd_dither1_thr = 0.2;
const float icsd_dither2_thr = 2;
const u8 vdt_dit_flag_thr = 2;
//COR
const float F_cxy_thr = 0.36;
const u8 vdt_cor_flag_thr = 2;
//NOISE_PWR
const float noise_pwr_thr = 12;
const float vdt_dnoise_thr = 20;



int (*vdt_printf)(const char *format, ...) = _vdt_printf;


void vdt_config_init(__vdt_config *_vdt_config)
{
    printf("vdt_config_init\n");
    //PWR
    _vdt_config->pnc_ref_pwr_thr = 60;
    _vdt_config->tpc_ref_pwr_thr = 60;
    _vdt_config->anc_ref_pwr_thr = 60;
    _vdt_config->pnc_err_pwr_thr = 100;
    _vdt_config->tpc_err_pwr_thr = 100;
    _vdt_config->anc_err_pwr_thr = 80;
    //DITHER
}

const struct vdt_function VDT_FUNC_t = {
    .vdt_config_init = vdt_config_init,
};
struct vdt_function *VDT_FUNC = (struct vdt_function *)(&VDT_FUNC_t);

#endif/*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
