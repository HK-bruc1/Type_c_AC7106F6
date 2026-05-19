/*
 ****************************************************************************
 *							Audio Mic_Capless Module
 *								省电容mic模块
 *
 *Notes:省电容MIC表示直接将mic的信号输出接到芯片的MICIN引脚
 ****************************************************************************
 */
#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "app_main.h"
#include "audio_config.h"
#include "gpadc.h"
#include "adc_file.h"

#define LOG_TAG             "[APP_AUDIO]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define TCFG_AUDIO_MC_ENABLE	1	//mic_capless_enable

#define MC_LOG_ENABLE
#ifdef MC_LOG_ENABLE
#define MC_LOG_I	g_printf
#define MC_LOG_E	r_printf
#else
#define MC_LOG_E(...)
#define MC_LOG_I(...)
#endif/*MC_LOG_ENABLE*/

#if TCFG_AUDIO_MC_ENABLE

__INITCALL_BANK_CODE
void mic_capless_trim_init(int update)
{
    struct adc_platform_cfg *platform_cfg = audio_adc_platform_get_cfg();
    u8 por_flag = 0;
    u8 cur_por_flag = 0;
    if (platform_cfg[0].mic_mode != AUDIO_MIC_CAPLESS_MODE) {
        return;
    }
    /*
     *1.update
     *2.power_on_reset
     *3.pin reset
     */
    if (update || is_reset_source(P33_VDDIO_POR_RST) || (is_reset_source(P33_PPINR_RST))) {
        cur_por_flag = 0xA5;
    }
    syscfg_read(CFG_POR_FLAG, &por_flag, 1);
    log_info("POR flag:%x-%x", cur_por_flag, por_flag);
    if ((cur_por_flag == 0xA5) && (por_flag != cur_por_flag)) {
        //log_info("update POR flag");
        /*需要保存上电状态时因为开机有可能进入soft poweroff，下次开机的时候需要知道上次的开机状态*/
        syscfg_write(CFG_POR_FLAG, &cur_por_flag, 1);
    }
}

#endif/*TCFG_AUDIO_MC_ENABLE*/

