#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_afq.data.bss")
#pragma data_seg(".icsd_afq.data")
#pragma const_seg(".icsd_afq.text.const")
#pragma code_seg(".icsd_afq.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc_common.h"
#endif

#if (TCFG_AUDIO_FREQUENCY_GET_ENABLE && \
	 TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2)
#include "audio_anc_includes.h"
#include "tone_player.h"
#include "adv_adaptive_noise_reduction.h"
#include "audio_src.h"
#include "clock_manager/clock_manager.h"
#include "icsd_afq.h"

#if 0
#define afq_log printf
#else
#define afq_log(...)
#endif


u8 icsd_afq_set_pnc(u8 ff_yorder, float bypass_vol, s8 bypass_sign)
{
    return 0;
}

int (*afq_printf)(const char *format, ...);

u16 icsd_afq_timeout_add(void *priv, void (*func)(void *priv), u32 msec)
{
    return sys_hi_timeout_add(priv, func, msec);
}

void icsd_afq_timeout_del(u32 timeout_handler)
{
    sys_hi_timeout_del(timeout_handler);
}

void icsd_afq_cmd(u8 cmd)
{
    os_taskq_post_msg("anc", 2, ANC_MSG_AFQ_CMD, cmd);
}

const struct afq_function AFQ_FUNC_t = {
    .anc_dma_done_ppflag = anc_dma_done_ppflag,
    .local_irq_disable = local_irq_disable,
    .local_irq_enable = local_irq_enable,
    .os_time_dly = os_time_dly,
    .jiffies_usec = jiffies_usec,
    .jiffies_usec2offset = jiffies_usec2offset,
    .icsd_afq_cmd = icsd_afq_cmd,
    .icsd_afq_timeout_add = icsd_afq_timeout_add,
    .icsd_afq_timeout_del = icsd_afq_timeout_del,
    .icsd_afq_dma_2ch_on = anc_dma_on_double,
#if ANC_CHIP_VERSION != ANC_VERSION_BR28
    .icsd_afq_dma_4ch_on = anc_dma_on_double_4ch,
#else
    .icsd_afq_dma_4ch_on = 0,
#endif
    .icsd_afq_dma_stop = anc_dma_off,
    .icsd_afq_set_pnc = icsd_afq_set_pnc,
};
struct afq_function *AFQ_FUNC = (struct afq_function *)(&AFQ_FUNC_t);

static void icsd_afq_out_debug(__afq_data *_AFQ_DATA)
{
    int len = _AFQ_DATA->len;
    afq_log("icsd_afq_out_debug:%d\n", len);
    local_irq_disable();
    for (int i = 0; i < len; i++) {
        afq_log("tsz:%d\n", (int)(_AFQ_DATA->out[i] * 1000));
    }
    local_irq_enable();
}

//sz输出
void icsd_afq_output(__afq_output *_AFQ_OUT)
{
    if (_AFQ_OUT->state) {
        printf("AFQ SZ err~~~~~~~~~\n");
    } else {
        afq_log("AFQ_OUTPUT_NORMAL\n");
        if (_AFQ_OUT->sz_l) {
            afq_log("AFQ_OUT_L\n");
            icsd_afq_out_debug(_AFQ_OUT->sz_l);
        }
        if (_AFQ_OUT->sz_r) {
            afq_log("AFQ_OUT_R\n");
            icsd_afq_out_debug(_AFQ_OUT->sz_r);
        }
    }
    audio_icsd_afq_output(_AFQ_OUT);
}

#if ICSD_AFQ_DSF8_DATA_DEBUG
s16 DSF8_DEBUG_H[AFQ_DMA_DOUBLE_LEN / 8 * AFQ_DMA_DOUBLE_CNT * 2];
s16 DSF8_DEBUG_L[AFQ_DMA_DOUBLE_LEN / 8 * AFQ_DMA_DOUBLE_CNT * 2];
s16 DSF8_R_DEBUG_H[AFQ_DMA_DOUBLE_LEN / 8 * AFQ_DMA_DOUBLE_CNT * 2];
s16 DSF8_R_DEBUG_L[AFQ_DMA_DOUBLE_LEN / 8 * AFQ_DMA_DOUBLE_CNT * 2];

int dsf8_wptr = 0;
void icsd_afq_dsf8_data_2ch(s16 *dsf_out_h, s16 *dsf_out_l)
{
    printf("afq dsf8 save:%d\n", dsf8_wptr);
    if ((dsf8_wptr + (AFQ_DMA_DOUBLE_LEN / 8)) > ((AFQ_DMA_DOUBLE_LEN / 8) * AFQ_DMA_DOUBLE_CNT)) {
        return;
    }
    s16 *wptr_h = &DSF8_DEBUG_H[dsf8_wptr];
    s16 *wptr_l = &DSF8_DEBUG_L[dsf8_wptr];
    memcpy(wptr_h, dsf_out_h, (AFQ_DMA_DOUBLE_LEN / 8) * 2);
    memcpy(wptr_l, dsf_out_l, (AFQ_DMA_DOUBLE_LEN / 8) * 2);
    dsf8_wptr += AFQ_DMA_DOUBLE_LEN / 8;
#if 0
    wdt_close();
    if (dsf8_wptr >= ((AFQ_DMA_DOUBLE_LEN / 8) * AFQ_DMA_DOUBLE_CNT)) {
        AFQ_FUNC->local_irq_disable();
        for (int i = 0; i < (AFQ_DMA_DOUBLE_LEN / 8) * AFQ_DMA_DOUBLE_CNT; i++) {
            printf("DSF8:H/L:%d                            %d\n", DSF8_DEBUG_H[i], DSF8_DEBUG_L[i]);
        }
    }
#endif
}

void icsd_afq_dsf8_printf(u8 mode)
{
    mem_stats();
    if (mode == 1) {
        wdt_close();
        AFQ_FUNC->local_irq_disable();
        for (int i = 0; i < (AFQ_DMA_DOUBLE_LEN / 8) * AFQ_DMA_DOUBLE_CNT; i++) {
            printf("DSF8:H/L:%d                            %d\n", DSF8_DEBUG_H[i], DSF8_DEBUG_L[i]);
        }
        AFQ_FUNC->local_irq_enable();
    } else {
        dsf8_wptr = 0;
    }

}

void icsd_afq_dsf8_data_4ch(s16 *dsf_out_h, s16 *dsf_out_l, s16 *dsf_out_h_r, s16 *dsf_out_l_r)
{
    printf("afq dsf8 save\n");
    s16 *wptr_h = &DSF8_DEBUG_H[dsf8_wptr];
    s16 *wptr_l = &DSF8_DEBUG_L[dsf8_wptr];
    s16 *wptr_h_r = &DSF8_R_DEBUG_H[dsf8_wptr];
    s16 *wptr_l_r = &DSF8_R_DEBUG_L[dsf8_wptr];
    memcpy(wptr_h, dsf_out_h, AFQ_DMA_DOUBLE_LEN / 8 * 2);
    memcpy(wptr_l, dsf_out_l, AFQ_DMA_DOUBLE_LEN / 8 * 2);
    memcpy(wptr_h_r, dsf_out_h_r, AFQ_DMA_DOUBLE_LEN / 8 * 2);
    memcpy(wptr_l_r, dsf_out_l_r, AFQ_DMA_DOUBLE_LEN / 8 * 2);
    dsf8_wptr += AFQ_DMA_DOUBLE_LEN / 8;
    if (dsf8_wptr >= (AFQ_DMA_DOUBLE_LEN / 8 * AFQ_DMA_DOUBLE_CNT)) {
        AFQ_FUNC->local_irq_disable();
        for (int i = 0; i < AFQ_DMA_DOUBLE_LEN / 8 * AFQ_DMA_DOUBLE_CNT; i++) {
            printf("4CH DSF8:%d                      %d                       %d                     %d\n", DSF8_DEBUG_H[i], DSF8_DEBUG_L[i], DSF8_R_DEBUG_H[i], DSF8_R_DEBUG_L[i]);
        }
    }
}
#endif

void icsd_afq_dsf8_data_debug(u8 belong, s16 *dsf_out_ch0, s16 *dsf_out_ch1, s16 *dsf_out_ch2, s16 *dsf_out_ch3)
{
#if ICSD_AFQ_DSF8_DATA_DEBUG
    if (belong == AFQ_DSF8_DATA_DEBUG_TYPE) {
        if (afq_dma_ch_num == 2) {
            icsd_afq_dsf8_data_2ch(dsf_out_ch0, dsf_out_ch1);
        } else {
            icsd_afq_dsf8_data_4ch(dsf_out_ch0, dsf_out_ch1, dsf_out_ch2, dsf_out_ch3);
        }
    }
#endif
}

#endif

