#include "audio_dac.h"
#include "audio_adc.h"
#include "media_config.h"
#include "power/power_manage.h"
#include "audio_volume_mixer.h"
#include "app_config.h"
#include "audio_demo/audio_demo.h"


static u8 audio_dac_idle_query()
{
    return audio_dac_is_idle();
}

static enum LOW_POWER_LEVEL audio_dac_level_query(void)
{
    /*根据dac的状态选择sleep等级*/
    if (config_audio_dac_power_off_lite) {
        /*anc打开，进入轻量级低功耗*/
        return LOW_POWER_MODE_LIGHT_SLEEP;
    } else {
        /*进入最优低功耗*/
        return LOW_POWER_MODE_SLEEP;
    }
}

REGISTER_LP_TARGET(audio_dac_lp_target) = {
    .name    = "audio_dac",
    .level   = audio_dac_level_query,
    .is_idle = audio_dac_idle_query,
};

void audio_fast_mode_test()
{
    printf("audio_fast_mode_test\n");
    audio_dac_set_volume(&dac_hdl, app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
#if TCFG_DAC_NODE_ENABLE
    audio_dac_start(&dac_hdl);
    audio_adc_mic_demo_open(AUDIO_ADC_MIC_CH, 10, 16000, 1);
#endif

}

void dac_power_on(void)
{
#if TCFG_DAC_NODE_ENABLE
    audio_dac_open(&dac_hdl);
#endif
}

void dac_power_off(void)
{
#if TCFG_DAC_NODE_ENABLE
    audio_dac_close(&dac_hdl);
#endif
}

/*
 * DAC开关状态回调，需要的时候，"#if 0" 改成 "#if 1"
 */
#if 0
void audio_dac_power_state(u8 state)
{
    switch (state) {
    case DAC_ANALOG_OPEN_PREPARE:
        //DAC打开前，即准备打开
        printf("DAC power_on start\n");
        break;
    case DAC_ANALOG_OPEN_FINISH:
        //DAC打开后，即打开完成
        printf("DAC power_on end\n");
        break;
    case DAC_ANALOG_CLOSE_PREPARE:
        //DAC关闭前，即准备关闭
        printf("DAC power_off start\n");
        break;
    case DAC_ANALOG_CLOSE_FINISH:
        //DAC关闭后，即关闭完成
        printf("DAC power_off end\n");
        break;
    }
}
#endif

