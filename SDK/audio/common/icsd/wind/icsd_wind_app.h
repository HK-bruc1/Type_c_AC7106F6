
#ifndef _ICSD_WIND_APP_H
#define _ICSD_WIND_APP_H

#include "generic/typedef.h"
#include "icsd_common_v2.h"
#include "app_config.h"


/*
   	SDK_WIND_PHONE_TYPE SDK 方案类型
	TWS   : ICSD_WIND_TWS
	头戴式：ICSD_WIND_HEADSET

	SDK_WIND_MIC_TYPE 风噪算法类型
	ICSD_WIND_LFF_TALK: TALK+FF, TWS 3 MIC 推荐使用
	ICSD_WIND_LFF_RFF : LFF+RFF  TWS HYBRID 2MIC / 头戴式 推荐使用
*/
#if TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_LR
/*头戴式方案*/
#define SDK_WIND_PHONE_TYPE  ICSD_WIND_HEADSET
#define SDK_WIND_MIC_TYPE    ICSD_WIND_LFF_RFF

#else
/*TWS 方案*/
#define SDK_WIND_PHONE_TYPE  ICSD_WIND_TWS
#define SDK_WIND_MIC_TYPE    ICSD_WIND_LFF_TALK		//TWS 2MIC/3MIC (TALK+FF) 方案使用
// #define SDK_WIND_MIC_TYPE    ICSD_WIND_LFF_RFF	//TWS 2MIC HYBRID (FF+FB) 方案使用

#endif


/*打开风噪检测*/
int audio_icsd_wind_detect_open();

/*关闭风噪检测*/
int audio_icsd_wind_detect_close();

/*风噪检测开关demo*/
void audio_icsd_wind_detect_demo();

/*重置风噪检测的初始参数*/
void audio_anc_wind_noise_fade_param_reset(void);

/*设置风噪检测设置增益，带淡入淡出功能*/
void audio_anc_wind_noise_fade_gain_set(int fade_gain, int fade_time);

/*风噪检测处理*/
int audio_anc_wind_noise_process(u8 wind_lvl);

/*风噪检测ioctl*/
int audio_adt_wind_ioctl(int cmd, int arg);

/*风噪检测噪声值转换为档位*/
int audio_anc_wind_thr_to_lvl(int wind_thr);

/*风噪检测档位同步接口*/
void audio_anc_wind_lvl_sync_info(u8 *data, int len);


#endif
