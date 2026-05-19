
#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE)
#include "audio_anc.h"
#include "icsd_adt.h"
#include "icsd_adt_app.h"
#include "rt_anc.h"
#include "icsd_avc.h"


/*==================算法输出===========================*/
void icsd_HOWL_output_demo(u8 result)
{
    printf("icsd_adt_alg_howl_output:%d\n", result);
}

void icsd_ADJDCC_output_demo(u8 result)
{
    /* printf("icsd_adt_alg_adjdcc_output:%d\n", result); */
}

void icsd_EIN_output_demo(u8 ein_state)
{
    printf("icsd_adt_alg_ein_output:%d\n", ein_state); //1:入耳	0:出耳
    extern void icsd_adt_ein_demo(u8 ein_state);
    icsd_adt_ein_demo(ein_state);

}

void icsd_VDT_output_demo(u8 vdt_result)
{
    if (vdt_result) {
        printf("--------------------------------VDT OUTPUT:%d---------------------\n", vdt_result);
    }
}

void icsd_WAT_output_demo(u8 wat_result)
{
    if ((wat_result == 2) || (wat_result == 3)) {
        printf("--------------------------------WAT OUTPUT:%d---------------------\n", wat_result);
    }
}

void icsd_WDT_output_demo(u8 wind_lvl)
{
    if (wind_lvl) {
        printf("--------------------------------WDT OUTPUT:%d---------------------\n", wind_lvl);
    }
}

void icsd_AVC_output_demo(__adt_avc_output *_output)
{
    if (_output->ctl_lvl) {
        printf("--------------------------------icsd_adt_avc_output:%d %d------------\n", _output->ctl_lvl, (int)(100 * _output->spldb_iir));
    }
}

void icsd_adt_avc_config_update_demo()
{
    __avc_config config;
    config.alpha_db = 0.990;
    config.db_cali = 13;
    config.sc_cali = 1.3;
    config.flen    = 16;
    icsd_adt_avc_config_update(&config);
}

void icsd_envnl_output(int result)
{
    printf("icsd_envnl_output:>>>>>>>>>>>>>>>>>>%d\n", result);
}

#define	ICSD_ANCDMA_4CH_46K_DEBUG_EN		0
#if ICSD_ANCDMA_4CH_46K_DEBUG_EN
#define anc46k_DEBUG_LEN		(1024*3)
s16 wptr_dma1_h_debug[anc46k_DEBUG_LEN];
s16 wptr_dma1_l_debug[anc46k_DEBUG_LEN];
s16 wptr_dma2_h_debug[anc46k_DEBUG_LEN];
s16 wptr_dma2_l_debug[anc46k_DEBUG_LEN];
u16 anc46k_wptr = 0;
u8 anc46k_debug_end = 0;
void icsd_ancdma_4ch_46k_debug()
{
    if (anc46k_debug_end) {
        ANC46K_CTL->loop_remain -= 512;
        return;
    }
    static u8 cnt = 0;
    if (cnt < 30) {
        cnt++;
        ANC46K_CTL->loop_remain -= 512;
        return;
    }
    memcpy(&wptr_dma1_h_debug[anc46k_wptr], &ANC46K_CTL->ch0_dptr[ANC46K_CTL->rptr], 2 * 512);
    memcpy(&wptr_dma1_l_debug[anc46k_wptr], &ANC46K_CTL->ch1_dptr[ANC46K_CTL->rptr], 2 * 512);
    memcpy(&wptr_dma2_h_debug[anc46k_wptr], &ANC46K_CTL->ch2_dptr[ANC46K_CTL->rptr], 2 * 512);
    memcpy(&wptr_dma2_l_debug[anc46k_wptr], &ANC46K_CTL->ch3_dptr[ANC46K_CTL->rptr], 2 * 512);
    //4通道数据都拿了后再修改rptr，并且消耗loop_remain
    ANC46K_CTL->rptr += 512;
    if (ANC46K_CTL->rptr >= ANC46K_CTL->loop_len) {
        ANC46K_CTL->rptr = 0;
    }
    ANC46K_CTL->loop_remain -= 512;

    anc46k_wptr += 512;
    if (anc46k_wptr >= anc46k_DEBUG_LEN) {
        anc46k_debug_end = 1;
        local_irq_disable();
        for (int i = 0; i < anc46k_DEBUG_LEN; i++) {
            printf("DMA1H,DMA1L,DMA2H,DMA2L:%d                         %d                           %d                                %d\n",
                   wptr_dma1_h_debug[i], wptr_dma1_l_debug[i], wptr_dma2_h_debug[i], wptr_dma2_l_debug[i]);
        }
        local_irq_enable();
    }
}
#endif

void icsd_anc_46kout_demo()
{
    if (ANC46K_CTL) {
#if ICSD_ANCDMA_4CH_46K_DEBUG_EN
        icsd_ancdma_4ch_46k_debug();
#else

#if 0
        static int cnt = 300;
        static int cnt2 = 1000;
        if (cnt2) {
            cnt2--;
        } else {
            if (cnt) {
                cnt--;
                printf("46kout remain:%d  %d\n", ANC46K_CTL->loop_remain, cnt);
                icsd_adt_anc46k_outen_set(1);
            } else {
                icsd_adt_anc46k_outen_set(0);
            }
        }
#endif

        int export_points = 256;
        int export_len = export_points * 2;
        extern int aec_uart_fill_v2(u8 ch, void *buf, u16 size);
        extern void aec_uart_write_v2(void);
        if (ANC46K_CTL->out_en) {
            aec_uart_fill_v2(0, &ANC46K_CTL->ch0_dptr[ANC46K_CTL->rptr], export_len);
            aec_uart_fill_v2(1, &ANC46K_CTL->ch1_dptr[ANC46K_CTL->rptr], export_len);
            aec_uart_fill_v2(2, &ANC46K_CTL->ch3_dptr[ANC46K_CTL->rptr], export_len);
            aec_uart_write_v2();
        }
        ANC46K_CTL->rptr += export_points;
        if (ANC46K_CTL->rptr >= ANC46K_CTL->loop_len) {
            ANC46K_CTL->rptr = 0;
        }
#endif
    }
}

#if 0
void icsd_adt_rtanc_demo(void *param)
{
    printf("icsd_rtanc_demo>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    icsd_task_create();
    struct icsd_acoustic_detector_libfmt libfmt;
    icsd_acoustic_detector_get_libfmt(&libfmt, 0);
    struct icsd_acoustic_detector_infmt fmt;
    fmt.alloc_ptr = zalloc(libfmt.lib_alloc_size);
    fmt.param = param;
    icsd_acoustic_detector_set_infmt(&fmt);
    icsd_acoustic_detector_open();
    extern void icsd_adt_anc_part1_start();
    icsd_adt_anc_part1_start();
    audio_icsd_adt_algom_resume(ANC_ON);
}
#endif

void icsd_tws_tone_play_send(u8 idx);
void icsd_adt_ein_demo(u8 ein_state)
{
    static u8 pre_state;
    if (ein_state != pre_state) {
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>:  %d\n", ein_state);
        icsd_tws_tone_play_send(ein_state);
    }
    pre_state = ein_state;
}

#endif


