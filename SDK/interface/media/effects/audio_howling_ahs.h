#ifndef _AUDIO_HOWLING_AHS_H_
#define _AUDIO_HOWLING_AHS_H_
#include "generic/typedef.h"
#include "jlsp_ahs_api.h"

struct audio_ahs_param_tool_set {
    int is_bypass;
    ahs_param_t param;
};
struct audio_ahs_post_param_tool_set {
    int is_bypass;
    ahs_param_t param;
};

struct audio_ahs_init_param_t {
    s32 sample_rate;
    u32 ref_sr;
    u16 frame_points;
    u8 iis_in_dac_out;
};

int audio_ahs_init(struct audio_ahs_init_param_t *init_param);
void audio_ahs_close(void);
u8 audio_ahs_status(void);
int howling_ahs_read_ref_data(void);
void audio_ahs_inbuf(s16 *buf, u16 len);
void audio_ahs_refbuf(s16 *data0, u16 len);
void audio_ahs_far_refbuf(s16 *buf, u16 len);
void audio_jlsp_ahs_run_1(s16 *frame_out, u16 points);
u8 audio_ahs_status(void);
void audio_ahs_sem_post();
void audio_jlsp_ahs_run_single_core(s16 *in, u16 points);
void audio_ahs_param_update(ahs_param_t *param);
#endif
