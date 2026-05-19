
#ifndef _AUDIO_ANC_COMMON_H_
#define _AUDIO_ANC_COMMON_H_

#include "generic/typedef.h"
#include "app_config.h"
#include "audio_config_def.h"

extern const u8 anc_ext_support_scene[7][10];

enum anc_event {
    ANC_EVENT_NONE = 0,
    ANC_EVENT_ADT_INIT,
    ANC_EVENT_ADT_RESET,
    ANC_EVENT_FILTER_UPDATE_BEFORE,
    ANC_EVENT_FILTER_UPDATE_AFTER,
    ANC_EVENT_RESET_BEFORE,
    ANC_EVENT_RESET_AFTER,
    ANC_EVENT_ADT_OPEN,
    ANC_EVENT_ADT_CLOSE,
    ANC_EVENT_ADT_SCENE_CHECK,
};

struct anc_tws_sync_info {
    u8 anc_mode; 			//当前ANC模式
    u8 user_anc_mode;		//用户ANC模式
    u8 ear_adaptive_seq;	//耳道自适应seq序号
    u8 multi_scene_id;		//多滤波器场景ID
    u8 vdt_state;			//智能免摘状态
    u16 app_adt_mode;		//ADT APP模式
};

int audio_anc_event_notify(enum anc_event event, int arg);

int audio_anc_app_adt_mode_init(u8 enable);

//ANC进入产测模式
int audio_anc_production_enter(void);

//ANC退出产测模式
int audio_anc_production_exit(void);

int audio_anc_production_mode_get(void);

int audio_anc_switch_adt_app_open(void);

int audio_anc_switch_adt_app_close(void);

//检查通话和ANC复用MIC 模拟增益是否匹配
int audio_anc_mic_gain_check(u8 is_phone_caller);

//TWS 回连ANC信息同步发送API
void bt_tws_sync_anc(void);

//ANC基础-同步信息处理函数
int anc_mode_sync(struct anc_tws_sync_info *info);

//启动ANC 数字MIC
int audio_anc_pdm_mic_start(u8 mic_ch);

//关闭ANC 数字MIC
int audio_anc_pdm_mic_stop(void);

//==============ANC 内存管理=================

//打印当前未释放的内存
void anc_mem_unfree_dump();

//内存申请
void *anc_malloc(const char *name, size_t size);

//内存释放
void anc_free(void *pv);

//用于不在ANC管理释放的内存，清除记录，比如ADC_HW BUFF
void anc_mem_clear(void *pv);

//比较ANC资源文件和ANC_IF差异并替换
int anc_config_rsfile_read(void *p);

#if TCFG_AUDIO_ANC_ENABLE

/*------------------------------------------------------------------*/
/*                         ANC_EXT 功能限制处理                     */
/*------------------------------------------------------------------*/

#if ((TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN || TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE) && \
	(!TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN))
#error "ANC自适应CMP、实时自适应必须先开启ANC耳道自适应"
#endif/*TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN*/

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN && (TCFG_AUDIO_ANC_TRAIN_MODE != ANC_HYBRID_EN)
#error "ANC自适应，仅支持ANC HYBRID方案"
#endif/*TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN*/

#if TCFG_AUDIO_FIT_DET_ENABLE && (TCFG_AUDIO_ANC_TRAIN_MODE == ANC_FF_EN)
#error "贴合度检测，仅支持带ANC FB的方案"
#endif/*TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN*/

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE || TCFG_AUDIO_FIT_DET_ENABLE
//AUDIO(ANC)频响获取使能 - AFQ
#define TCFG_AUDIO_FREQUENCY_GET_ENABLE		1
#endif

#ifndef TCFG_AUDIO_FREQUENCY_GET_ENABLE
#define TCFG_AUDIO_FREQUENCY_GET_ENABLE		0
#endif

#ifndef TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE
#define TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE	1	//ANC调试模式 支持基础ANC调试，若flash不足，生产可关闭
#elif TCFG_ANC_BOX_ENABLE || TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN || TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
#undef TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE
#define TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE	1	// ANC调试模式 ANC扩展功能及串口调试必须打开
#endif

#ifndef TCFG_AUDIO_ANC_EXT_EN
#define TCFG_AUDIO_ANC_EXT_EN				1
#endif

#endif/*TCFG_AUDIO_ANC_ENABLE*/

#endif/*_AUDIO_ANC_COMMON_H_*/
