#include "audio_pdm.h"
#include "includes.h"
#include "cpu/includes.h"
#include "audio_dac.h"
#include "audio_adc.h"
#include "media/includes.h"
#include "asm/audio_common.h"

/* #define PLNK_DEBUG_ENABLE */

#ifdef PLNK_DEBUG_ENABLE
#define plnk_printf 		y_printf
#else
#define plnk_printf(...)
#endif

extern struct audio_adc_hdl adc_hdl;
struct pdm_mic_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
};
static struct pdm_mic_hdl *pdm_hdl = NULL;

static PLNK_PARM *__this = NULL;
#define PLNK_CLK	24000000

static void plnk_set_shift_scale(u8 order, u8 m)
{
#if 0
    u32 cic_r = __this->sclk_fre / __this->sr;
    cic_r = cic_r * m;
    u64 shift_sum = 1;
    u32	scale_sum;
    u8 i, j;
    u8 shift_dir = 0;
    u8 shift_sel = 0;
    u8 scale_sel = 0;

    /* calculate shift_dir */
    for (u8 cnt = 0; cnt < order; cnt++) {
        shift_sum *= cic_r;
    }

    plnk_printf("shift_sum = %d", shift_sum);
    if (shift_sum < 4096) {
        shift_dir = 1;
    } else {
        shift_dir = 0;
    }

    /* calculate shift_sel */
    shift_sum = shift_sum * 15;
    for (shift_sel = 0; shift_sel <= 31; shift_sel++) {
        plnk_printf("shift_sum = %d, shift_sel = %d", shift_sum, shift_sel);
        if (shift_sum < 32768 && shift_sum > 16384) {
            break;
        }
        if (shift_dir == 1) {
            shift_sum = shift_sum * 2;
        } else if (shift_dir == 0) {
            shift_sum = shift_sum / 2;
        }
    }

    /* calculate scale_sel */
    scale_sum = shift_sum;
    for (scale_sel = 0; scale_sel <= 7; scale_sel++) {
        scale_sum = shift_sum * (8 + scale_sel) / 8;
        plnk_printf("scale_sum = %d, scale_sel = %d", scale_sum, scale_sel);
        if (scale_sum > 32768) {
            break;
        }
    }
    scale_sel = scale_sel - 1;

    plnk_printf("order = %d, m = %d, shift_dir = %d, shift_sel = %d, scale_sel = %d\n", order, m, shift_dir, shift_sel, scale_sel);

    PLNK_CH0_SHDIR(shift_dir);
    PLNK_CH1_SHDIR(shift_dir);

    PLNK_CH0_SHIFT(shift_sel);
    PLNK_CH1_SHIFT(shift_sel);

    PLNK_CH0_SCALE(scale_sel);
    PLNK_CH1_SCALE(scale_sel);
#endif
}


static void plnk_isr(void *priv, void *data, u32 len)
{
    if (__this && __this->isr_cb) {
        /* u32 data_len = len * __this->ch_num; */
        __this->isr_cb(__this->private_data, data, len);
    }
}

static void plnk_info_dump()
{
    /* plnk_printf("PLNK_CON = 0x%x", JL_PLNK->CON); */
    /* plnk_printf("PLNK_CON1 = 0x%x", JL_PLNK->CON1); */
    /* plnk_printf("PLNK_LEN = 0x%x", JL_PLNK->LEN); */
    /* plnk_printf("PLNK_ADR = 0x%x", JL_PLNK->ADR); */
    /* plnk_printf("PLNK_DOR = 0x%x", JL_PLNK->DOR); */
    /* plnk_printf("ASS_CLK_CON = 0x%x", JL_ASS->CLK_CON); */
    /* plnk_printf("CLK_CON4 = 0x%x", JL_CLOCK->CLK_CON4); */
}

static void plnk_sr_set(u32 sr)
{
    /* PLNK_CLK_EN(0); */
    /* u16 plnk_dor = __this->sclk_fre / sr - 1; */
    /* u16 sclk_div = PLNK_CLK / __this->sclk_fre - 1; */
    /* plnk_printf("PLNK_SR = %d, dor = %d, sclk_dov = %d\n", sr, plnk_dor, sclk_div); */
    /* PLNK_DOR_SET(plnk_dor); */
    /* PLNK_SCLK_DIV(sclk_div); */
    /* PLNK_CLK_EN(1); */
}

static void plnk_sclk_io_init(u8 port)
{
    gpio_set_mode(IO_PORT_SPILT(port), PORT_OUTPUT_HIGH);
    gpio_set_function(IO_PORT_SPILT(port), PORT_FUNC_PLNK_SCLK);
}

static void plnk_data_io_init(u8 ch_index, u8 port)
{
    gpio_set_mode(IO_PORT_SPILT(port), PORT_INPUT_FLOATING);
    if (ch_index) {
        gpio_set_function(IO_PORT_SPILT(port), PORT_FUNC_PLNK_DAT1);
    } else {
        gpio_set_function(IO_PORT_SPILT(port), PORT_FUNC_PLNK_DAT0);
    }
}

static void plnk_sclk_io_uninit(u8 port)
{
    /* gpio_set_mode(IO_PORT_SPILT(port), PORT_OUTPUT_HIGH); */
    gpio_disable_function(IO_PORT_SPILT(port), PORT_FUNC_PLNK_SCLK);
}

static void plnk_data_io_uninit(u8 ch_index, u8 port)
{
    /* gpio_set_mode(IO_PORT_SPILT(port), PORT_INPUT_FLOATING); */
    if (ch_index) {
        gpio_disable_function(IO_PORT_SPILT(port), PORT_FUNC_PLNK_DAT1);
    } else {
        gpio_disable_function(IO_PORT_SPILT(port), PORT_FUNC_PLNK_DAT0);
    }
}

void *plnk_init(void *hw_plink)
{
    if (hw_plink == NULL) {
        return NULL;
    }

    __this = (PLNK_PARM *)hw_plink;

    audio_adc_dmic_clock_open(1, 23);
    u8 ch_num = 0;
    /* adc_core_digital_open_t param = {0}; */
    plnk_sclk_io_init(__this->sclk_io);
    pdm_hdl = zalloc(sizeof(struct pdm_mic_hdl));
    audio_adc_mic_set_sample_rate(&pdm_hdl->mic_ch, __this->sr);
    struct mic_open_param mic_param[4] = {0};
    if (__this->ch_cfg[0].en) {
        plnk_printf("MIC0");
        mic_param[0].mic_dcc_en    = 1;
        mic_param[0].mic_dcc       = 1;
        audio_adc_mic_open(&pdm_hdl->mic_ch, AUDIO_ADC_DMIC_0, &adc_hdl, &mic_param[0]);
        plnk_data_io_init(0, __this->data_cfg[0].io);
        ch_num++;
    }
    if (__this->ch_cfg[1].en) {
        plnk_printf("MIC1");
        mic_param[1].mic_dcc_en    = 1;
        mic_param[1].mic_dcc       = 1;
        audio_adc_mic_open(&pdm_hdl->mic_ch, AUDIO_ADC_DMIC_1, &adc_hdl, &mic_param[1]);
        plnk_data_io_init(0, __this->data_cfg[1].io);
        ch_num++;
    }
    if (__this->ch_cfg[2].en) {
        plnk_printf("MIC2");
        mic_param[2].mic_dcc_en    = 1;
        mic_param[2].mic_dcc       = 1;
        audio_adc_mic_open(&pdm_hdl->mic_ch, AUDIO_ADC_DMIC_2, &adc_hdl, &mic_param[2]);
        plnk_data_io_init(1, __this->data_cfg[2].io);
        ch_num++;
    }
    if (__this->ch_cfg[3].en) {
        plnk_printf("MIC3");
        mic_param[3].mic_dcc_en    = 1;
        mic_param[3].mic_dcc       = 1;
        audio_adc_mic_open(&pdm_hdl->mic_ch, AUDIO_ADC_DMIC_3, &adc_hdl, &mic_param[3]);
        plnk_data_io_init(1, __this->data_cfg[3].io);
        ch_num++;
    }
    __this->ch_num = ch_num;
    plnk_printf("CH: %d %d\n", __this->dma_len, __this->ch_num);
    __this->buf = malloc(__this->dma_len  * 2 * __this->ch_num);
    ASSERT(__this->buf);
    memset(__this->buf, 0x00, __this->dma_len  * 2 * __this->ch_num);
    audio_adc_mic_set_buffs(&pdm_hdl->mic_ch, __this->buf, __this->dma_len, 2);
    //step3:设置mic采样输出回调函数
    pdm_hdl->adc_output.handler = (void(*)(void *, s16 *, int))plnk_isr;
    pdm_hdl->adc_output.priv = __this->private_data;
    audio_adc_add_output_handler(&adc_hdl, &pdm_hdl->adc_output);
    return __this;
}

void plnk_start(void *hw_plink)
{
    if (!hw_plink) {
        return;
    }
    audio_adc_mic_start(&pdm_hdl->mic_ch);
}

void plnk_stop(void *hw_plink)
{
    if (!hw_plink) {
        return;
    }
    audio_adc_mic_close(&pdm_hdl->mic_ch);
}

void plnk_uninit(void *hw_plink)
{
    if (!hw_plink) {
        return;
    }
    audio_adc_del_output_handler(&adc_hdl, &pdm_hdl->adc_output);
    audio_adc_mic_close(&pdm_hdl->mic_ch);
    plnk_sclk_io_uninit(__this->sclk_io);
    plnk_data_io_uninit(0, __this->data_cfg[0].io);
    plnk_data_io_uninit(1, __this->data_cfg[2].io);
    free(pdm_hdl);
    pdm_hdl = NULL;

    /* if (__this->buf) {
        dma_free(__this->buf);
        __this->buf = NULL;
    } */
    __this = NULL;
}



