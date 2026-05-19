#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_dot.data.bss")
#pragma data_seg(".icsd_dot.data")
#pragma const_seg(".icsd_dot.text.const")
#pragma code_seg(".icsd_dot.text")
#endif

#include "app_config.h"
//#if ((defined TCFG_AUDIO_FIT_DET_ENABLE) && TCFG_AUDIO_FIT_DET_ENABLE && \
TCFG_AUDIO_ANC_ENABLE)
#include "icsd_dot.h"
#include "icsd_dot_app.h"
#include "audio_anc.h"
struct icsd_dot_parm *DOT_PARM;
int (*dot_printf)(const char *format, ...) = _dot_printf;
/* #define dot_printf       _dot_printf; */

//lib_icsd_dot.a使用，临时接口
void icsd_dot_parm_init()
{
    DOT_PARM->end_point = 14;
    DOT_PARM->start_point = 4;
    DOT_PARM->norm_thr = ICSD_DOT_NORM_THR;
    DOT_PARM->loose_thr = ICSD_DOT_LOOSE_THR;
    DOT_PARM->msc_mean_thr = 1;
}
//#endif/*TCFG_AUDIO_FIT_DET_ENABLE*/
