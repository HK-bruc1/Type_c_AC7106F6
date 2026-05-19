#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_aeq.data.bss")
#pragma data_seg(".icsd_aeq.data")
#pragma const_seg(".icsd_aeq.text.const")
#pragma code_seg(".icsd_aeq.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if (TCFG_AUDIO_ADAPTIVE_EQ_ENABLE && TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2) && \
	(defined TCFG_EQ_ENABLE)

#include "icsd_aeq_app.h"
#include "effects/audio_eq.h"

struct aeq_default_seg_tab *AEQ_DEF;
int (*aeq_printf)(const char *format, ...) = _aeq_printf;

void aeq_seg_design(void *seg_tmp, int sample_rate, void *coef)
{
    eq_seg_design((struct eq_seg_info *)seg_tmp, sample_rate, coef);
}

const struct eq_function EQ_FUNC_t = {
    .aeq_seg_design = aeq_seg_design,
};
struct eq_function *EQ_FUNC = (struct eq_function *)(&EQ_FUNC_t);

void aeq_get_par(struct aeq_default_seg_tab *aeq_def, void *eq_par)
{

    struct eq_default_seg_tab *eq_def = (struct eq_default_seg_tab *)eq_par;

    //
    aeq_def->seg_num        = eq_def->seg_num;
    aeq_def->global_gain    = eq_def->global_gain;
    aeq_def->seg            = (struct aeq_seg_info *)eq_def->seg;

    //struct aeq_seg_info *seg;

    //for(int i=0;i<aeq_def->seg_num; i++ )
    //{
    //
    //	struct eq_seg_info *seg = eq_def->seg++;
    //    aeq_def->seg[i].iir_type  = seg->iir_type;
    //    aeq_def->seg[i].freq      = seg->freq    ;
    //    aeq_def->seg[i].gain      = seg->gain    ;
    //    aeq_def->seg[i].q         = seg->q       ;
    //}
    //
    //aeq_def->seg.iir_type  = eq_def->seg->iir_type;
    //aeq_def->seg.freq      = eq_def->seg->freq    ;
    //aeq_def->seg.gain      = eq_def->seg->gain    ;
    //aeq_def->seg.q         = eq_def->seg->q       ;

    /* printf("finish get par\n"); */

}

void icsd_aeq_force_exit()
{
    DeAlorithm_disable();
}
#endif






