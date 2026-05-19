#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_afq.data.bss")
#pragma data_seg(".icsd_afq.data")
#pragma const_seg(".icsd_afq.text.const")
#pragma code_seg(".icsd_afq.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc_common.h"
#endif

#if (TCFG_AUDIO_FREQUENCY_GET_ENABLE && \
	 TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2)

#include "icsd_afq.h"

const u8 afq_dma_ch_num = 2;
//const u8 AFQ_EP_TYPE = ICSD_HEADSET;//耳机类型
const u8 AFQ_EP_TYPE = ICSD_FULL_INEAR;//耳机类型

#endif
