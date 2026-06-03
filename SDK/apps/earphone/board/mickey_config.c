#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".mickey_config.data.bss")
#pragma data_seg(".mickey_config.data")
#pragma const_seg(".mickey_config.text.const")
#pragma code_seg(".mickey_config.text")
#endif
#include "app_config.h"
#include "key/mickey.h"
#include "key_driver.h"
#include "adkey_config.h"
#include "mickey_config.h"
#include "audio_adc.h"
#include "audio_dac.h"

#if TCFG_MICKEY_ENABLE

static struct mickey_platform_data g_mickey_platform_data;

static inline u16 mickey_calc_pull_up_value(u8 bias_rsel)
{
    if (bias_rsel < 9) {
        return 500 * bias_rsel + 200;
    } else {
        /* 4500 + 200 = 4700 */
        return 1000 * (bias_rsel - 9) + 4700;
    }
}

const struct mickey_platform_data *get_mickey_platform_data()
{

    if (g_mickey_platform_data.enable) {
        return &g_mickey_platform_data;
    }

    g_mickey_platform_data.pp_key.res_value = mickey_res_table.pp_res_value;
    g_mickey_platform_data.up_key.res_value = mickey_res_table.up_res_value;
    g_mickey_platform_data.down_key.res_value = mickey_res_table.down_res_value;

    g_mickey_platform_data.pp_key.key_value = KEY_MIC_NUM0;
    g_mickey_platform_data.up_key.key_value = KEY_MIC_NUM1;
    g_mickey_platform_data.down_key.key_value = KEY_MIC_NUM2;

    extern int audio_mic_ldo_en(u8 index_map, u8 en, u8 mic_bias_rsel);
#if TCFG_ADC0_ENABLE
    audio_mic_ldo_en(TCFG_ADC0_BIAS_SEL, 1, TCFG_ADC0_BIAS_RSEL);
#if (TCFG_ADC0_BIAS_SEL & AUDIO_MIC_LDO_PWR)
    //使用MICLDO供电时,ad通道需要选择其他IO
    gpio_set_mode(IO_PORT_SPILT(mickey_res_table.mickey_pin), PORT_INPUT_FLOATING);
    gpio_set_function(IO_PORT_SPILT(mickey_res_table.mickey_pin), PORT_FUNC_GPADC);
    g_mickey_platform_data.micbias_ad_channel = adc_io2ch(mickey_res_table.mickey_pin);
    g_mickey_platform_data.pull_up_value = mickey_res_table.extern_bias_res;
#else
    //使用MICBIAS供电时,ad通道选择micbias
    g_mickey_platform_data.micbias_ad_channel = AD_CH_AUDIO_MICBIAS0;
    g_mickey_platform_data.pull_up_value = mickey_calc_pull_up_value(TCFG_ADC0_BIAS_RSEL);
#endif

#elif TCFG_ADC1_ENABLE
    audio_mic_ldo_en(TCFG_ADC1_BIAS_SEL, 1, TCFG_ADC1_BIAS_RSEL);
#if (TCFG_ADC1_BIAS_SEL & AUDIO_MIC_LDO_PWR)
    //使用MICLDO供电时,ad通道需要选择其他IO
    gpio_set_mode(IO_PORT_SPILT(mickey_res_table.mickey_pin), PORT_INPUT_FLOATING);
    gpio_set_function(IO_PORT_SPILT(mickey_res_table.mickey_pin), PORT_FUNC_GPADC);
    g_mickey_platform_data.micbias_ad_channel = adc_io2ch(mickey_res_table.mickey_pin);
    g_mickey_platform_data.pull_up_value = mickey_res_table.extern_bias_res;
#else
    //使用MICBIAS供电时,ad通道选择micbias
    g_mickey_platform_data.micbias_ad_channel = AD_CH_AUDIO_MICBIAS1;
    g_mickey_platform_data.pull_up_value = mickey_calc_pull_up_value(TCFG_ADC1_BIAS_RSEL);
#endif

#else
#error "please open the ADC0 or ADC1!!!"
    g_mickey_platform_data.micbias_ad_channel = 0xffff;
#endif

#if (TCFG_EARPHONE_TYPE == 0)//四线模式
    g_mickey_platform_data.dacln_ad_channel = AD_CH_AUDIO_DACLN;
#else
    g_mickey_platform_data.dacln_ad_channel = -1;
#endif

    g_mickey_platform_data.enable    = 1;
    return &g_mickey_platform_data;
}

#endif
