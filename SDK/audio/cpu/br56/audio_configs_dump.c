#include "generic/typedef.h"
#include "audio_dac.h"
#include "audio_adc.h"

/*音频模块寄存器跟踪*/
void audio_adda_dump(void) //打印所有的dac,adc寄存器
{
    printf("JL_WL_AUD CON0:%x", JL_WL_AUD->CON0);
    printf("DAC_CON0:%x", JL_AUDDAC->DAC_CON0);
    printf("DAC_CON1:%x", JL_AUDDAC->DAC_CON1);
    printf("DAC_CON2:%x", JL_AUDDAC->DAC_CON2);
    printf("DAC_CON3:%x", JL_AUDDAC->DAC_CON3);
    printf("DAC_CON4:%x", JL_AUDDAC->DAC_CON4);
    printf("DAC_CON5:%x", JL_AUDDAC->DAC_CON5);
    printf("DAC_CON6:%x", JL_AUDDAC->DAC_CON6);
    printf("DAC_CON7:%x", JL_AUDDAC->DAC_CON7);
    printf("DAC_CON8:%x", JL_AUDDAC->DAC_CON8);
    printf("DAC_CON9:%x", JL_AUDDAC->DAC_CON9);
    printf("DAC_CON10:%x", JL_AUDDAC->DAC_CON10);
    printf("DAC_CON11:%x", JL_AUDDAC->DAC_CON11);
    printf("DAC_CON12:%x", JL_AUDDAC->DAC_CON12);
    printf("DAC_CON13:%x", JL_AUDDAC->DAC_CON13);
    printf("DAC_CON14:%x", JL_AUDDAC->DAC_CON14);
    printf("DAC_CON15:%x", JL_AUDDAC->DAC_CON15);
    printf("ADC_CON0:%x", JL_AUDADC->ADC_CON0);
    printf("ADC_CON1:%x", JL_AUDADC->ADC_CON1);
    printf("ADC_CON2:%x", JL_AUDADC->ADC_CON2);
    printf("DAC_VL0:%x", JL_AUDDAC->DAC_VL0);
    printf("DAC_TM0:%x", JL_AUDDAC->DAC_TM0);
    printf("ADDA_CON0:%x", JL_ADDA->ADDA_CON0);
    printf("ADDA_CON1:%x", JL_ADDA->ADDA_CON1);
    printf("ADDA_CON0:%x    1:%x\n", JL_ADDA->ADDA_CON0, JL_ADDA->ADDA_CON1);
    printf("DAA_CON 0:%x 	1:%x,	2:%x,	7:%x\n", JL_ADDA->DAA_CON0, JL_ADDA->DAA_CON1, JL_ADDA->DAA_CON2, JL_ADDA->DAA_CON7);
    printf("ADA_CON 0:%x	1:%x	2:%x	3:%x	4:%x	5:%x	6:%x\n", JL_ADDA->ADA_CON0, JL_ADDA->ADA_CON1, JL_ADDA->ADA_CON2, JL_ADDA->ADA_CON3, JL_ADDA->ADA_CON4, JL_ADDA->ADA_CON5, JL_ADDA->ADA_CON6);
    printf("AUD_CON 0:%x	1:%x	2:%x	3:%x	4:%x	5:%x	6:%x\n", JL_AUD->AUD_CON0, JL_AUD->AUD_CON1, JL_AUD->AUD_CON2, JL_AUD->AUD_CON3, JL_AUD->AUD_CON4, JL_AUD->AUD_CON5, JL_AUD->AUD_CON6);
}

/*音频模块配置跟踪*/
void audio_config_dump()
{
    u8 dac_bit_width = ((JL_AUDDAC->DAC_CON0 & BIT(20)) ? 24 : 16);
    u8 adc_bit_width = ((JL_AUDADC->ADC_CON0 & BIT(20)) ? 24 : 16);
    int dac_dgain_max = 16384;
    int dac_again_max = 7;
    int mic_gain_max = 5;
    u8 dac_dcc = (JL_AUDDAC->DAC_CON0 >> 12) & 0xF;
    u8 mic0_dcc = (JL_AUDADC->ADC_CON1 >> 12) & 0xF;
    u8 mic1_dcc = (JL_AUDADC->ADC_CON1 >> 16) & 0xF;

    u8 dac_again_l = (JL_ADDA->DAA_CON1 >> 1) & 0x7;
    u8 dac_again_r = (JL_ADDA->DAA_CON2 >> 1) & 0x7;
    u32 dac_dgain_l = JL_AUDDAC->DAC_VL0 & 0xFFFF;
    u32 dac_dgain_r = (JL_AUDDAC->DAC_VL0 >> 16) & 0xFFFF;
    u8 mic0_0_6 = (JL_ADDA->ADA_CON2 >> 14) & 0x1;
    u8 mic1_0_6 = (JL_ADDA->ADA_CON3 >> 14) & 0x1;
    u8 mic0_0_3 = (JL_ADDA->ADA_CON2 >> 13) & 0x1;
    u8 mic1_0_3 = (JL_ADDA->ADA_CON3 >> 13) & 0x1;
    u8 mic0_gain = (JL_ADDA->ADA_CON2 >> 10) & 0x7;
    u8 mic1_gain = (JL_ADDA->ADA_CON3 >> 10) & 0x7;
    int dac_sr = audio_dac_get_sample_rate_base_reg();
    int adc_sr = audio_adc_mic_get_sample_rate();

    printf("[ADC]BitWidth:%d,DCC:%d,%d,SR:%d\n", adc_bit_width, mic0_dcc, mic1_dcc, adc_sr);
    printf("[ADC]Gain(Max:%d):%d,%d,N3dB:%d,%d,N6dB:%d,%d\n", mic_gain_max, mic0_gain, mic1_gain, \
           mic0_0_3, mic1_0_3, mic0_0_6, mic1_0_6);

    printf("[DAC]BitWidth:%d,DCC:%d,SR:%d\n", dac_bit_width, dac_dcc, dac_sr);
    printf("[DAC]AGain(Max:%d):%d,%d,DGain(Max:%d):%d,%d\n", dac_again_max, dac_again_l, dac_again_r, \
           dac_dgain_max, dac_dgain_l, dac_dgain_r);
}
