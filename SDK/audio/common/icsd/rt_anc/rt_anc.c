#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".rt_anc.data.bss")
#pragma data_seg(".rt_anc.data")
#pragma const_seg(".rt_anc.text.const")
#pragma code_seg(".rt_anc.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE

#include "audio_anc_includes.h"
#include "app_tone.h"
#include "classic/tws_api.h"
#include "rt_anc.h"
#include "icsd_adt.h"
#include "icsd_adt_alg.h"
#include "clock_manager/clock_manager.h"

void anc_core_ff_adjdcc_par_set(u8 dc_par);
const u8 rtanc_log_en = ICSD_RTANC_LOG_EN;

int (*hz_printf)(const char *format, ...) = _hz_printf;
int (*rt_printf)(const char *format, ...) = _rt_printf;
int rt_printf_off(const char *format, ...)
{
    return 0;
}

void rt_anc_post_anctask_cmd(u8 cmd)
{
    os_taskq_post_msg("anc", 2, ANC_MSG_RT_ANC_CMD, cmd);
}

void rt_anc_post_rttask_cmd(u8 cmd)
{
    os_taskq_post_msg("rt_anc", 1, cmd);
}

u8 rt_anc_task_en = 0;

void rt_anc_config_init(__rt_anc_config *_rt_anc_config)
{

}

const struct rt_anc_function RT_ANC_FUNC_t = {
    .anc_dma_on = anc_dma_on,
    .anc_dma_on_double = anc_dma_on_double,
    .anc_dma_done_ppflag = anc_dma_done_ppflag,
    .anc_core_dma_ie = anc_dma_ie,
    .anc_core_dma_stop = anc_dma_stop,
    .rt_anc_post_rttask_cmd = icsd_post_rtanctask_msg,
    .rt_anc_post_anctask_cmd = rt_anc_post_anctask_cmd,
    .icsd_post_detask_msg = icsd_post_detask_msg,
    .rt_anc_dma_2ch_on = anc_dma_on_double,
    .rt_anc_dma_4ch_on = anc_dma_on_double_4ch,
    .rt_anc_alg_output = icsd_adt_rtanc_alg_output,
    .jiffies_usec = jiffies_usec,
    .jiffies_usec2offset = jiffies_usec2offset,
    .audio_anc_debug_send_data = audio_anc_debug_send_data,
    .rt_anc_config_init = rt_anc_config_init,
    .rt_anc_param_updata_cmd = rt_anc_param_updata_cmd,
    .clock_refurbish = clock_refurbish,
    .get_wind_lvl = icsd_adt_alg_rtanc_get_wind_lvl,
    .get_adjdcc_result = icsd_adt_alg_rtanc_get_adjdcc_result,
    .get_wind_angle = icsd_adt_alg_rtanc_get_wind_angle,
    .get_avc_spldb_iir = icsd_adt_alg_rtanc_get_avc_spldb_iir,
    .icsd_self_talk_output = audio_rtanc_self_talk_output,
    .icsd_adt_rtanc_suspend = icsd_adt_rtanc_suspend,
    .ff_adjdcc_par_set = anc_core_ff_adjdcc_par_set,
    .rtanc_drc_output = icsd_adt_drc_output,
    .rtanc_drc_read = icsd_adt_drc_read,
};
struct rt_anc_function *RT_ANC_FUNC = (struct rt_anc_function *)(&RT_ANC_FUNC_t);

#if RT_ANC_DSF8_DATA_DEBUG
s16 DSF8_DEBUG_H[RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT];
s16 DSF8_DEBUG_L[RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT];
s16 DSF8_R_DEBUG_H[RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT];
s16 DSF8_R_DEBUG_L[RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT];
static int dsf8_wptr = 0;
void rt_anc_dsf8_data_2ch(s16 *dsf_out_h, s16 *dsf_out_l)
{
    printf("rt anc 2ch dsf8 save\n");
    s16 *wptr_h = &DSF8_DEBUG_H[dsf8_wptr];
    s16 *wptr_l = &DSF8_DEBUG_L[dsf8_wptr];
    memcpy(wptr_h, dsf_out_h, RT_ANC_DMA_DOUBLE_LEN / 8 * 2);
    memcpy(wptr_l, dsf_out_l, RT_ANC_DMA_DOUBLE_LEN / 8 * 2);
    dsf8_wptr += RT_ANC_DMA_DOUBLE_LEN / 8;
    if (dsf8_wptr >= (RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT)) {
        local_irq_disable();
        for (int i = 0; i < RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT; i++) {
            printf("DSF8:H/L:%d                            %d\n", DSF8_DEBUG_H[i], DSF8_DEBUG_L[i]);
        }
    }
}
void rt_anc_dsf8_data_4ch(s16 *dsf_out_h, s16 *dsf_out_l, s16 *dsf_out_h_r, s16 *dsf_out_l_r)
{
    printf("rt anc 4ch dsf8 save\n");
    s16 *wptr_h = &DSF8_DEBUG_H[dsf8_wptr];
    s16 *wptr_l = &DSF8_DEBUG_L[dsf8_wptr];
    s16 *wptr_h_r = &DSF8_R_DEBUG_H[dsf8_wptr];
    s16 *wptr_l_r = &DSF8_R_DEBUG_L[dsf8_wptr];
    memcpy(wptr_h, dsf_out_h, RT_ANC_DMA_DOUBLE_LEN / 8 * 2);
    memcpy(wptr_l, dsf_out_l, RT_ANC_DMA_DOUBLE_LEN / 8 * 2);
    memcpy(wptr_h_r, dsf_out_h_r, RT_ANC_DMA_DOUBLE_LEN / 8 * 2);
    memcpy(wptr_l_r, dsf_out_l_r, RT_ANC_DMA_DOUBLE_LEN / 8 * 2);
    dsf8_wptr += RT_ANC_DMA_DOUBLE_LEN / 8;
    if (dsf8_wptr >= (RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT)) {
        local_irq_disable();
        for (int i = 0; i < RT_ANC_DMA_DOUBLE_LEN / 8 * RT_ANC_DMA_DOUBLE_CNT; i++) {
            printf("4CH DSF8:%d                      %d                       %d                     %d\n", DSF8_DEBUG_H[i], DSF8_DEBUG_L[i], DSF8_R_DEBUG_H[i], DSF8_R_DEBUG_L[i]);
        }
    }
}
#endif

void rt_anc_dsf8_data_debug(u8 belong, s16 *dsf_out_ch0, s16 *dsf_out_ch1, s16 *dsf_out_ch2, s16 *dsf_out_ch3)
{
#if RT_ANC_DSF8_DATA_DEBUG
    if (belong == RT_DSF8_DATA_DEBUG_TYPE) {
        extern const u8 rt_anc_dma_ch_num;
        if (rt_anc_dma_ch_num == 2) {
            rt_anc_dsf8_data_2ch(dsf_out_ch0, dsf_out_ch1);
        } else {
            rt_anc_dsf8_data_4ch(dsf_out_ch0, dsf_out_ch1, dsf_out_ch2, dsf_out_ch3);
        }
    }
#endif
}

#endif
