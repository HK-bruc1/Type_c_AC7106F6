
#ifndef __AUDIO_ANC_PLATFORM_INCLUDES_H_
#define __AUDIO_ANC_PLATFORM_INCLUDES_H_

#include "app_config.h"

#if TCFG_AUDIO_ANC_ENABLE

/******************************************************************************/
/*                             基础头文件                                     */
/* Note:声明宏定义等信息，被其他头文件依赖			     		              */
/******************************************************************************/
#include "audio_anc_common.h"
#include "audio_anc.h"


/******************************************************************************/
/*                           通用功能头文件                                   */
/******************************************************************************/
#include "audio_anc_common_plug.h"
#include "icsd_demo.h"
#include "audio_anc_fade_ctr.h"
#include "audio_anc_debug_tool.h"
#include "audio_anc_lvl_sync.h"
#include "media/audio_threshold_det.h"

#if TCFG_ANC_TOOL_DEBUG_ONLINE
#include "app_anctool.h"
#endif

#if TCFG_ANC_BOX_ENABLE
#include "app_ancbox.h"
#endif

#if ANC_MULT_ORDER_ENABLE
#include "audio_anc_mult_scene.h"
#endif/*ANC_MULT_ORDER_ENABLE*/


/******************************************************************************/
/*                           ANC_EXT功能头文件                                */
/******************************************************************************/
#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
#include "anc_ext_tool.h"
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN || TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
#include "icsd_anc_user.h"
#include "icsd_common_v2_app.h"
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
#include "icsd_adt_app.h"
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN || TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
#include "icsd_anc_v2_interactive.h"
#include "icsd_anc_v2_config.h"
#include "icsd_anc_v2_app.h"
#endif

#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
#include "icsd_afq_app.h"
#endif

#if ANC_EAR_ADAPTIVE_CMP_EN
#include "icsd_cmp_app.h"
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
#include "rt_anc_app.h"
#endif

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
#include "icsd_aeq_app.h"
#endif

#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
#include "icsd_vdt_app.h"
#endif

#if TCFG_AUDIO_FIT_DET_ENABLE
#include "icsd_dot_app.h"
#endif

#if AUDIO_ANC_DATA_EXPORT_VIA_UART
#include "audio_anc_develop.h"
#endif

#if ANC_HOWLING_DETECT_EN
#include "anc_howling_detect.h"
#endif

#endif

#endif/*__AUDIO_ANC_PLATFORM_INCLUDES_H_*/
