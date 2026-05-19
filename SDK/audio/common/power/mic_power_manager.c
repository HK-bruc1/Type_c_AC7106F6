
#include "mic_power_manager.h"
#include "gpio_config.h"
#include "audio_adc.h"
#include "adc_file.h"
#include "audio_config_def.h"

/*
 *MIC电源管理
 *设置mic vdd对应port的状态
 */
void audio_mic_pwr_ctl(audio_mic_pwr_t state)
{
    int i;
    struct adc_platform_cfg *platform_cfg = audio_adc_platform_get_cfg();
    switch (state) {
    case MIC_PWR_OFF:
        if (audio_adc_is_active()) {
            printf("MIC_IO_PWR AUDIO_ADC is useing !\n");
            return;
        }
        printf("MIC_IO_PWR close\n");
    case MIC_PWR_INIT:
#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
#if !ANC_ADC0_BIAS_SEL
        gpio_set_mode(IO_PORT_SPILT(ANC_ADC0_POWER_IO), PORT_OUTPUT_LOW);
#endif
#if !ANC_ADC1_BIAS_SEL
        gpio_set_mode(IO_PORT_SPILT(ANC_ADC1_POWER_IO), PORT_OUTPUT_LOW);
#endif
        break;
#endif
        /*mic供电IO配置：输出0*/
        for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (platform_cfg[i].mic_bias_sel == 0) {
                /* u32 gpio = uuid2gpio(platform_cfg[i].power_io); */
                u32 gpio = platform_cfg[i].power_io;
                gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_LOW);
            }
        }

        /*
        //测试发现 少数MIC在开机的时候，需要将MIC_IN拉低，否则工作异常
        gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_LOW);	//MIC0
        gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_LOW);	//MIC1
        gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_LOW);	//MIC2
        gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_LOW);	//MIC3
         */
        break;

    case MIC_PWR_ON:
#if defined(ANC_MIC_REUSE_ENABLE) && ANC_MIC_REUSE_ENABLE
#if !ANC_ADC0_BIAS_SEL
        gpio_set_mode(IO_PORT_SPILT(ANC_ADC0_POWER_IO), PORT_OUTPUT_HIGH);
#endif
#if !ANC_ADC1_BIAS_SEL
        gpio_set_mode(IO_PORT_SPILT(ANC_ADC1_POWER_IO), PORT_OUTPUT_HIGH);
#endif
        break;
#endif
        /*mic供电IO配置：输出1*/
        for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if (platform_cfg[i].mic_bias_sel == 0) {
                /* u32 gpio = uuid2gpio(platform_cfg[i].power_io); */
                u32 gpio = platform_cfg[i].power_io;
                gpio_set_mode(IO_PORT_SPILT(gpio), PORT_OUTPUT_HIGH);
            }
        }
        break;

    case MIC_PWR_DOWN:
        break;
    }
}

