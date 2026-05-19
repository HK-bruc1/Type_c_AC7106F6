/*
 ******************************************************************************************
 *							Audio Config
 *
 * Discription: 音频模块与芯片系列相关配置
 *
 * Notes:
 ******************************************************************************************
 */
#include "cpu/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "audio_config.h"
#include "clock_manager/clock_manager.h"

const u16 dac_digital_gain_tab_version_c[7] = {
    16384,  // 20mW
    16384,  // 30mW
    16766,  // 50mW
    17156,  // 80mW
    17156,  // 100mW
    16766,  // 2Vrms
    17156   // 2Vrms
};
const int config_audio_dac_mute_timeout = 70;   //单位：ms

/*
 *******************************************************************
 *						Audio Dac Config
 *******************************************************************
 */
const int config_audio_dac_trim_fade_enable = AUD_DAC_TRIM_FADE_ENABLE;
const int config_audio_dac_mute_nf_level = 0;   // DAC mute后底噪等级

/*
 *******************************************************************
 *						Audio Codec Config
 *******************************************************************
 */
const int config_audio_dac_mix_enable = 1;

//***********************
//*		AAC Codec       *
//***********************
const int AAC_DEC_MP4A_LATM_ANALYSIS = 1;
const int AAC_DEC_LIB_SUPPORT_24BIT_OUTPUT = 1;
const int WTS_DEC_LIB_SUPPORT_24BIT_OUTPUT = 1;
//***********************
//*		MP3 Codec       *
//***********************
#ifdef CONFIG_SOUNDBOX_CASE_ENABLE
const int MP3_DEC_LIB_SUPPORT_24BIT_OUTPUT = 1;
#else
const int MP3_DEC_LIB_SUPPORT_24BIT_OUTPUT = 0;
#endif

//***********************
//*		Audio Params    *
//***********************
void audio_adc_param_fill(struct mic_open_param *mic_param, struct adc_platform_cfg *platform_cfg)
{
    mic_param->mic_mode      = platform_cfg->mic_mode;
    mic_param->mic_ain_sel   = platform_cfg->mic_ain_sel;
    mic_param->mic_bias_sel  = platform_cfg->mic_bias_sel;
    mic_param->mic_bias_rsel = platform_cfg->mic_bias_rsel;
    mic_param->mic_dcc       = platform_cfg->mic_dcc;
    mic_param->mic_dcc_en    = platform_cfg->mic_dcc_en;
}
void audio_linein_param_fill(struct linein_open_param *linein_param, const struct adc_platform_cfg *platform_cfg)
{
    linein_param->linein_mode    = platform_cfg->mic_mode;
    linein_param->linein_ain_sel = platform_cfg->mic_ain_sel;
    linein_param->linein_dcc     = platform_cfg->mic_dcc;
}

/*param:dvol*/
u16 dac_dvol_max_query(void)
{
    u16 DAC_0dB = 16100;
    if ((JL_SYSTEM->CHIP_VER >= 0xA2) && (JL_SYSTEM->CHIP_VER < 0xAC)) { //C版以后才做DAC TRIM
        DAC_0dB = dac_digital_gain_tab_version_c[TCFG_DAC_POWER_MODE];
    }
    //printf("dac_dvol_max_query:%d",DAC_0dB);
    return DAC_0dB;
}

//***********************
//*		Audio Clock     *
//***********************
int aud_clock_alloc(const char *name, u32 clk)
{
    y_printf("aud_clock_alloc:%s(%d)\n", name, clk);
    return clock_alloc(name, clk);
}

int aud_clock_free(char *name)
{
    y_printf("aud_clock_free:%s\n", name);
    return clock_free(name);

}

u16 audio_ana_vol_dynamic_switch_init(u16 *gain_table, int table_size)
{
    if (JL_SYSTEM->CHIP_VER <= 0xA2) {
        //@BR56 ver.A/B/C
        return AVOL_SET_LVL;
    }
    //@BR56 ver.D
    if (!gain_table) {
        return 0;
    }
    u16 ana_vol_save = 0;
    u16 avol_set_lvl = 0;
    for (u8 i = 1; i <= table_size; i++) {
        //检索需要切换为模拟音量的档位
        if (ana_vol_save && (ana_vol_save == gain_table[i])) {
            avol_set_lvl = i;
            printf("avol_dinamic_switch_init: avol_set_lvl %d\n", avol_set_lvl);
            break;
        }
        ana_vol_save = gain_table[i];
    }
    if (!ana_vol_save) {
        printf("avol_dinamic_switch_init_fail\n");
    }
    return avol_set_lvl;
}

/*
 * cur_vol:当前设置的音量
 * avol_set_lvl:达到该档位时需要切换模拟音量
 */
void audio_ana_vol_dynamic_switch_set(u16 cur_vol, u16 avol_set_lvl)
{
#if defined(ANA_VOL_DYNAMIC_SWITCH) && ANA_VOL_DYNAMIC_SWITCH
    if (avol_set_lvl) {
        float dB_value = DEFAULT_DIGITAL_VOLUME;
#if ((TCFG_AUDIO_ANC_ENABLE) && (defined ANC_MODE_DIG_VOL_LIMIT))
        dB_value = (dB_value > ANC_MODE_DIG_VOL_LIMIT) ? ANC_MODE_DIG_VOL_LIMIT : dB_value;
#endif/*TCFG_AUDIO_ANC_ENABLE*/

        u16 dvol_full_max = dac_dvol_max_query();
        u16 dvol_max = (u16)(dvol_full_max * dB_Convert_Mag(dB_value));

        //以下为 @BR56 系列模拟/数字增益配置
        u16 dvol_set = dvol_max;                          //0dB
        u16 avol_set = 3;                                 //0dB
        if (cur_vol <= avol_set_lvl) {
            //需要切换模拟音量
            dvol_set = dvol_max * dB_Convert_Mag(6.0f);   //+6dB
            dvol_set = (dvol_set > ((1 << 15) - 1)) ? ((1 << 15) - 1) : dvol_set;  //防止超过寄存器16bit有符号数表达范围
            if (JL_SYSTEM->CHIP_VER <= 0xA2) {
                //ver.A/B/C
                avol_set = 2;                             //-6dB
            } else {
                //ver.D
                avol_set = 1;                             //-9dB
            }
        }

        printf("avol_dinamic_sw avol_set %d, dvol_set %d\n", avol_set, dvol_set);
        if ((dvol_set != audio_dac_ch_digital_gain_get(&dac_hdl, BIT(0))) || \
            (dvol_set != audio_dac_ch_digital_gain_get(&dac_hdl, BIT(1)))) {
            //防止重复设置
            printf("avol_dinamic_sw dvol_set suss\n");
            audio_dac_set_digital_vol(&dac_hdl, dvol_set);
            app_audio_volume_dvol_set(dvol_set);//更新app_volume_mixer中保存的值
        }
        if ((avol_set != audio_dac_ch_analog_gain_get(BIT(0))) || \
            (avol_set != audio_dac_ch_analog_gain_get(BIT(1)))) {
            printf("avol_dinamic_sw avol_set suss\n");
            audio_dac_set_analog_vol(&dac_hdl, avol_set);
            app_audio_volume_avol_set(avol_set, avol_set); //更新app_volume_mixer中保存的值
        }
    }
#endif
}
