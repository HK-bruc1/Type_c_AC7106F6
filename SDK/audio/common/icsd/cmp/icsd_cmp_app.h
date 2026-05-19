#ifndef _ICSD_CMP_APP_H
#define _ICSD_CMP_APP_H

#include "icsd_cmp_config.h"
#include "icsd_common_v2_app.h"

#define ANC_ADAPTIVE_CMP_ORDER					10			/*ANC自适应CMP滤波器阶数，原厂指定*/

#define ANC_ADAPTIVE_CMP_ONLY_IN_MUSIC_UPDATE	0			/*(实时CMP)仅在播歌/通话的时候更新*/

enum ANC_EAR_ADAPTIVE_CMP_CH {
    ANC_EAR_ADAPTIVE_CMP_CH_L = 0,
    ANC_EAR_ADAPTIVE_CMP_CH_R,
};

enum ANC_EAR_ADAPTIVE_CMP_DATA_FORM {
    CMP_FROM_ANC_EAR_ADAPTIVE = 0,
    CMP_FROM_RTANC,
    CMP_FROM_AFQ,
};

//ICSD CMP 状态
enum ANC_EAR_ADAPTIVE_CMP_STATE {
    ANC_EAR_ADAPTIVE_CMP_STATE_CLOSE = 0,
    ANC_EAR_ADAPTIVE_CMP_STATE_OPEN,
};


struct anc_cmp_param_output {
    u8 l_update_flag;
    u8 r_update_flag;
    float l_gain;
    double *l_coeff;

    float r_gain;
    double *r_coeff;
};

int audio_anc_ear_adaptive_cmp_open(u8 data_from);

int audio_anc_ear_adaptive_cmp_run(void *p);

int audio_anc_ear_adaptive_cmp_close(void);

//获取FGQ
float *audio_anc_ear_adaptive_cmp_output_get(enum ANC_EAR_ADAPTIVE_CMP_CH ch);

//获取IIR_COEFF
int audio_rtanc_adaptive_cmp_output_get(struct anc_cmp_param_output *output);

//获取滤波器类型
u8 *audio_anc_ear_adaptive_cmp_type_get(enum ANC_EAR_ADAPTIVE_CMP_CH ch);
void audio_rtanc_adaptive_cmp_update_flag_get(struct anc_cmp_param_output *output);

u8 audio_anc_ear_adaptive_cmp_result_get(void);

//CMP SZ挑线
void audio_anc_ear_adaptive_cmp_sz_sel(u8 sz_sel, float *fgq, u8 *iir_type, u8 ch);

float *audio_anc_ear_adaptive_cmp_sz_cmp_get(enum ANC_EAR_ADAPTIVE_CMP_CH ch);

#endif
