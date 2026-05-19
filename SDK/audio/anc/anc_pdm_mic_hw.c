#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".anc_pdm_mic_hw.data.bss")
#pragma data_seg(".anc_pdm_mic_hw.data")
#pragma const_seg(".anc_pdm_mic_hw.text.const")
#pragma code_seg(".anc_pdm_mic_hw.text")
#endif
#include "audio_dai/audio_pdm.h"
#include "audio_adc.h"
#include "app_config.h"
#include "audio_config.h"
#include "gpio_config.h"
#include "audio_anc_includes.h"

#if TCFG_AUDIO_ANC_ENABLE && AUDIO_ANC_PDM_MIC_ENABLE

#define ANC_PLNK_CH_NUM				BIT(0)|BIT(1)|BIT(2)|BIT(3)
#define ANC_PLNK_SCLK_PIN			IO_PORTB_02
#define ANC_PLNK_DAT0_PIN			IO_PORTB_03
#define ANC_PLNK_DAT1_PIN			IO_PORTC_03

#define ANC_PDM_IRQ_POINTS    256 /*采样中断点数*/
#define ANC_PDM_SR            16000 /*采样率*/

/* 保存配置文件信息的结构体 */
struct pdm_mic_cfg {
    u16 plnk_ch_num;
    u16 io_sclk_uuid;
    u16 io_ch0_uuid;
    u16 io_ch1_uuid;
};

struct anc_pdm_mic_hdl {
    u8 dump_cnt;
    u16 sample_rate;
    u16 irq_points;
    s16 *buf;
    PLNK_PARM *pdm_mic;
};
static int audio_anc_pdm_mic_param_init(PLNK_PARM *pdm_mic, u8 mic_ch);

static struct anc_pdm_mic_hdl *hdl = NULL;

static void pdm_mic_output_handler(void *priv, void *data,  u32 len)
{
    /* struct anc_pdm_mic_hdl *hdl = (struct anc_pdm_mic_hdl *)priv; */
    /* if (hdl->dump_cnt < 10) { */
    /* hdl->dump_cnt++; */
    /* return; */
    /* } */
}

int audio_anc_pdm_mic_start(u8 mic_ch)
{
    if (hdl || !mic_ch) {
        return -1;
    }
    hdl = zalloc(sizeof(struct anc_pdm_mic_hdl));
    hdl->pdm_mic = zalloc(sizeof(PLNK_PARM));
    audio_mic_pwr_ctl(MIC_PWR_ON);
    if (hdl->pdm_mic) {

        hdl->pdm_mic->sr = ANC_PDM_SR;
        hdl->pdm_mic->dma_len = (ANC_PDM_IRQ_POINTS << 1);
        hdl->pdm_mic->isr_cb = pdm_mic_output_handler;
        hdl->pdm_mic->private_data = hdl;	//保存私有指针

        audio_anc_pdm_mic_param_init(hdl->pdm_mic, mic_ch);

        hdl->pdm_mic = plnk_init(hdl->pdm_mic);
        plnk_start(hdl->pdm_mic);
    }
    return 0;
}

int audio_anc_pdm_mic_stop(void)
{
    if (hdl) {
        if (hdl->pdm_mic) {
            plnk_uninit(hdl->pdm_mic);
            audio_mic_pwr_ctl(MIC_PWR_OFF);
            free(hdl->pdm_mic);
            hdl->pdm_mic = NULL;
        }
        free(hdl);
        hdl = NULL;
    }
    return 0;
}

static int audio_anc_pdm_mic_param_init(PLNK_PARM *pdm_mic, u8 mic_ch)
{
    if (!pdm_mic) {
        return -1;
    }

    struct pdm_mic_cfg pdm_cfg;
    u8 plnk_ch_num = 0;
    pdm_cfg.io_sclk_uuid = ANC_PLNK_SCLK_PIN;
    pdm_cfg.io_ch0_uuid = ANC_PLNK_DAT0_PIN;
    pdm_cfg.io_ch1_uuid = ANC_PLNK_DAT1_PIN;
    plnk_ch_num = mic_ch;

#if ((defined PDM_VERSION) && (PDM_VERSION == AUDIO_PDM_V2))
    pdm_mic->sclk_io = pdm_cfg.io_sclk_uuid;
    printf("PDM_IDX: %d\n", plnk_ch_num);
    if (plnk_ch_num & AUDIO_PDM_MIC_0) {
        pdm_mic->data_cfg[0].en = 1;
        pdm_mic->data_cfg[0].io = pdm_cfg.io_ch0_uuid;
    }
    if (plnk_ch_num & AUDIO_PDM_MIC_1) {
        pdm_mic->data_cfg[1].en = 1;
        pdm_mic->data_cfg[1].io = pdm_cfg.io_ch0_uuid;
    }
    if (plnk_ch_num & AUDIO_PDM_MIC_2) {
        pdm_mic->data_cfg[2].en = 1;
        pdm_mic->data_cfg[2].io = pdm_cfg.io_ch1_uuid;
    }
    if (plnk_ch_num & AUDIO_PDM_MIC_3) {
        pdm_mic->data_cfg[3].en = 1;
        pdm_mic->data_cfg[3].io = pdm_cfg.io_ch1_uuid;
    }
#else
    pdm_mic->sclk_io = pdm_cfg.io_sclk_uuid;
    if (plnk_ch_num > 0) {
        pdm_mic->data_cfg[0].en = 1;
        pdm_mic->data_cfg[0].io = pdm_cfg.io_ch0_uuid;
    }
    if (plnk_ch_num > 1) {
        pdm_mic->data_cfg[1].en = 1;
        pdm_mic->data_cfg[1].io = pdm_cfg.io_ch1_uuid;
    }
#endif
    y_printf("plnk_ch_num %d", plnk_ch_num);
    y_printf("sclk_io %d", pdm_mic->sclk_io);
    y_printf("ch0_io %d", pdm_mic->data_cfg[0].io);
    y_printf("ch1_io %d", pdm_mic->data_cfg[1].io);

    /*SCLK:1M-4M,SCLK/SR需为整数且在1-4096范围*/
    if (!pdm_mic->sr) {
        pdm_mic->sr = 16000;
    }
#if ((defined PDM_VERSION) && (PDM_VERSION == AUDIO_PDM_V2))
    if (plnk_ch_num & AUDIO_PDM_MIC_0) {
        pdm_mic->ch_cfg[0].en = 1;
        pdm_mic->ch_cfg[0].mode = DATA0_SCLK_RISING_EDGE;
        pdm_mic->ch_cfg[0].mic_type = DIGITAL_MIC_DATA;
    }
    if (plnk_ch_num & AUDIO_PDM_MIC_1) {
        pdm_mic->ch_cfg[1].en = 1;
        pdm_mic->ch_cfg[1].mode = DATA0_SCLK_FALLING_EDGE;
        pdm_mic->ch_cfg[1].mic_type = DIGITAL_MIC_DATA;
    }
    if (plnk_ch_num & AUDIO_PDM_MIC_2) {
        pdm_mic->ch_cfg[2].en = 1;
        pdm_mic->ch_cfg[2].mode = DATA1_SCLK_RISING_EDGE;
        pdm_mic->ch_cfg[2].mic_type = DIGITAL_MIC_DATA;
    }
    if (plnk_ch_num & AUDIO_PDM_MIC_3) {
        pdm_mic->ch_cfg[3].en = 1;
        pdm_mic->ch_cfg[3].mode = DATA1_SCLK_FALLING_EDGE;
        pdm_mic->ch_cfg[3].mic_type = DIGITAL_MIC_DATA;
    }
#else
    if (plnk_ch_num > 0) {
        pdm_mic->ch_cfg[0].en = 1;
        pdm_mic->ch_cfg[0].mode = DATA0_SCLK_RISING_EDGE;
        pdm_mic->ch_cfg[0].mic_type = DIGITAL_MIC_DATA;
    }
    if (plnk_ch_num > 1) {
        pdm_mic->ch_cfg[1].en = 1;
        pdm_mic->ch_cfg[1].mode = DATA1_SCLK_FALLING_EDGE;
        pdm_mic->ch_cfg[1].mic_type = DIGITAL_MIC_DATA;
    }
#endif

    return 0;
}

#endif
