#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".mickey.data.bss")
#pragma data_seg(".mickey.data")
#pragma const_seg(".mickey.text.const")
#pragma code_seg(".mickey.text")
#endif
#include "key_driver.h"
#include "mickey.h"
#include "gpio.h"
#include "app_config.h"
#include "asm/power_interface.h"
#include "system/init.h"
#include "power/power_manage.h"
#include "system/init.h"
#include "audio_dac.h"
#include "audio_adc.h"

#if TCFG_MICKEY_ENABLE
#define MIC_NOKEY    (diff_down + diff_down/2)
#define POWER_ON_FITER  (2000/10)//开机2s后,删除一些通道不采集

static const struct mickey_platform_data *__this = NULL;
//按键驱动扫描参数列表
struct key_driver_para mickey_scan_param;
static volatile u8 power_on_flag;
static u16 power_on_cnt;
static u16 adc_micldo;

u8 mic_get_key_value(void)
{
    u8 key = NO_KEY;
    u16 adc_mic, adc_vcmo;
    u16 diff_val, diff_pp, diff_up, diff_down, diff_mic, diff_min, mic_res;

    if (power_on_flag) {
        power_on_cnt++;
        adc_micldo = adc_get_value(AD_CH_AUDIO_MICLDO);
        if (power_on_cnt > POWER_ON_FITER) {
            power_on_flag = 0;
            power_on_cnt = 0;
            adc_delete_ch(AD_CH_AUDIO_MICLDO);
        }
    }

    if (__this == NULL || (__this->enable != 1)) {
        return NO_KEY;
    }

    if (__this->dacln_ad_channel != -1) {
        adc_vcmo =  adc_get_value(__this->dacln_ad_channel);//公共端
    } else {
        adc_vcmo = 0;
    }

    adc_mic = adc_get_value(__this->micbias_ad_channel);//mic端电压值
    mic_res = __this->pull_up_value;

    diff_val = adc_mic - adc_vcmo;//mic的电压值
    diff_mic = adc_micldo - adc_vcmo;//mic + micbias上拉电阻的电压值
    diff_pp  = __this->pp_key.res_value * diff_mic / (__this->pp_key.res_value + mic_res);
    diff_up  = __this->up_key.res_value * diff_mic / (__this->up_key.res_value + mic_res);
    diff_down  = __this->down_key.res_value * diff_mic / (__this->down_key.res_value + mic_res);

    //判断无按键电平
    if (diff_val > MIC_NOKEY) {
        return NO_KEY;
    }

    diff_mic = (diff_mic > diff_val) ? (diff_mic - diff_val) : (diff_val - diff_mic);//micbias上拉电阻的电压值
    diff_pp = (diff_pp > diff_val) ? (diff_pp - diff_val) : (diff_val - diff_pp);
    diff_up = (diff_up > diff_val) ? (diff_up - diff_val) : (diff_val - diff_up);
    diff_down = (diff_down > diff_val) ? (diff_down - diff_val) : (diff_val - diff_down);

    diff_min = diff_mic;
    if (diff_pp < diff_min) {
        diff_min = diff_pp;
        key = __this->pp_key.key_value;
    }
    if (diff_up < diff_min) {
        diff_min = diff_up;
        key = __this->up_key.key_value;
    }
    if (diff_down < diff_min) {
        diff_min = diff_down;
        key = __this->down_key.key_value;
    }

    return key;
}

__attribute__((weak))
const struct mickey_platform_data *get_mickey_platform_data()
{
    return NULL;
}

__INITCALL_BANK_CODE
int mickey_init(void)
{
    __this = get_mickey_platform_data();

    if (!__this) {
        return -EINVAL;
    }

    if (!__this->enable) {
        return KEY_NOT_SUPPORT;
    }

    if (__this->micbias_ad_channel == 0xffff) {
        return -EINVAL;
    }

    adc_add_sample_ch(__this->micbias_ad_channel);
    adc_add_sample_ch(AD_CH_AUDIO_MICLDO);
    if (__this->dacln_ad_channel != -1) {
        adc_add_sample_ch(__this->dacln_ad_channel);        //MICKEY按键扫描需要用到DAC负端的电压值，做为按键判定的参考值，所以需要将DAC负端加入ADC采样通道
    }
    power_on_flag = 1;
    return 0;
}

REGISTER_KEY_OPS(mickey) = {
    .idle_query_en    = 1,
    .key_type         = KEY_DRIVER_TYPE_MIC,
    .filter_time      = 2,                //按键消抖延时;
    .long_time        = 75,              //按键判定长按数量
    .hold_time        = (75 + 15),      //按键判定HOLD数量
    .click_delay_time = 20,                //按键被抬起后等待连击延时数量
    .scan_time        = 10,                //按键扫描频率, 单位: ms
    .param            = &mickey_scan_param,
    .get_value        = mic_get_key_value,
    .key_init         = mickey_init,
};

#endif  /* #if TCFG_MICKEY_ENABLE */
