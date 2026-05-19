#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"

#if ((TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE) || \
    (TCFG_AUDIO_AVC_NODE_ENABLE && (TCFG_AVC_ALGO_SELECT == 1)))

#include "audio_config.h"
#include "icsd_avc.h"

int (*avc_printf)(const char *format, ...) = _avc_printf;

float avc_alpha_db = 0.9; //滤波器平滑度 越大越平滑，响应越慢 range (0,1)
void avc_config_init(__avc_config *_avc_config)
{
    /* _avc_config->alpha_db = 0x995; */
    _avc_config->alpha_db   = avc_alpha_db;
    _avc_config->alpha_db_l = 0.4;
    _avc_config->db_cali  = 0;
    _avc_config->sc_cali  = 4;
    _avc_config->dac_cali = 0;
    _avc_config->flen     = 16;

}

const struct avc_function AVC_FUNC_t = {
    .avc_config_init = avc_config_init,
    .app_audio_get_volume = app_audio_get_volume,
};
struct avc_function *AVC_FUNC = (struct avc_function *)(&AVC_FUNC_t);

float spldb_lvl_thr[] = {0, 20, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80};
u8 lvl_delta_max[]    = {0,  0,  0,  1, 1,  2,  2,  3,  3, 3, 4, 4, 4};
u8 lvl_max[]          = {0,  2,  2,  3, 4,  4,  5,  5,  6, 6, 7, 7, 8};

const float tidy_scale = 0.8733;
const float tidy_offset = 25.6;

const u16 phone_call_dig_vol_tab[] = {
    0,	//0
    111,//1
    161,//2
    234,//3
    338,//4
    490,//5
    708,//6
    1024,//7
    1481,//8
    2142,//9
    3098,//10
    4479,//11
    6477,//12
    9366,//13
    14955,//14
    16384 //15
};
#endif
