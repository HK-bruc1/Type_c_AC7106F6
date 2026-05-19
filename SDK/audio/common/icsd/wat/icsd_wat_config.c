#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE)
#include "icsd_wat.h"
#include "icsd_adt.h"

const u8 wat_data_en = 1;

int (*wat_printf)(const char *format, ...) = _wat_printf;

void wat_config_init(__wat_config *_wat_config)
{
    _wat_config->wat_pn_gain = 1.1;//ANC OFF
    _wat_config->wat_bypass_gain = 6;  //通透
    _wat_config->wat_anc_gain    = 1;  //ANC ON
}


void tws_tx_unsniff_req();

const struct wat_function WAT_FUNC_t = {
    .wat_config_init = wat_config_init,
    .tws_tx_unsniff_req = tws_tx_unsniff_req,
};
struct wat_function *WAT_FUNC = (struct wat_function *)(&WAT_FUNC_t);

#endif/*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
