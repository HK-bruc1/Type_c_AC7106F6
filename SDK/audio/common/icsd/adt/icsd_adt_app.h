#ifndef __ICSD_ADT_APP_H_
#define __ICSD_ADT_APP_H_

#include "typedef.h"
#include "icsd_anc_user.h"
#include "anc.h"
#include "icsd_wind_app.h"
#include "audio_anc_common.h"

#define SPEAK_TO_CHAT_TASK_NAME     "speak_to_chat"

/* 通过蓝牙spp发送风噪信息
 * 需要同时打开TCFG_BT_SUPPORT_SPP和APP_ONLINE_DEBUG*/
#define ICSD_ADT_WIND_INFO_SPP_DEBUG_EN  0

/* 通过蓝牙spp发送音量自适应得噪声大小信息
 * 需要同时打开TCFG_BT_SUPPORT_SPP和APP_ONLINE_DEBUG*/
#define ICSD_ADT_VOL_NOISE_LVL_SPP_DEBUG_EN  0

/*串口写卡导出MIC的数据，需要先打开宏AUDIO_DATA_EXPORT_VIA_UART*/
#define ICSD_ADT_MIC_DATA_EXPORT_EN  0

enum {
    ANC_WIND_NOISE_LVL0 = 0,
    ANC_WIND_NOISE_LVL1,
    ANC_WIND_NOISE_LVL2,
    ANC_WIND_NOISE_LVL3,
    ANC_WIND_NOISE_LVL4,
    ANC_WIND_NOISE_LVL5,
    ANC_WIND_NOISE_LVL_MAX,
};

enum {
    WIND_AREA_TAP_DOUBLE_CLICK = 2,     //双击
    WIND_AREA_TAP_THIRD_CLICK,      //三击
    WIND_AREA_TAP_MULTIPLE_CLICK,   //大于3次多次连击
};

//enum-type : u16
enum ICSD_ADT_MODE {
    ADT_MODE_CLOSE = 0,
    ADT_SPEAK_TO_CHAT_MODE = BIT(0), 			//智能免摘
    ADT_WIND_NOISE_DET_MODE = BIT(1),			//风噪检测
    ADT_WIDE_AREA_TAP_MODE = BIT(2), 			//广域点击
    ADT_ENVIRONMENT_DET_MODE = BIT(3), 			// 环境声检测
    ADT_REAL_TIME_ADAPTIVE_ANC_MODE = BIT(4), 	// RT_ANC
    ADT_EAR_IN_DETCET_MODE = BIT(5), 			// 入耳检测
    ADT_ENV_NOISE_DET_MODE = BIT(6), 			// 环境噪声检测
    ADT_REAL_TIME_ADAPTIVE_ANC_TIDY_MODE = BIT(7), 	// RT_ANC TIDY mode
    //ADT_ADAPTIVE_DCC_MODE = BIT(8),			//自适应DCC
    ADT_HOWLING_DET_MODE = BIT(9),				//啸叫检测
};

enum {
    /*任务消息*/
    ICSD_ADT_VDT_TIRRGER = 1,
    ICSD_ADT_VDT_RESUME,
    ICSD_ADT_VDT_KEEP_STATE,
    ICSD_ADT_WIND_LVL,
    ICSD_ADT_WAT_RESULT,
    ICSD_ADT_ENV_NOISE_LVL,
    ICSD_ADT_AVC_NOISE_LVL,
    ICSD_ADT_TONE_PLAY,
    SPEAK_TO_CHAT_TASK_KILL,
    ICSD_ANC_MODE_SWITCH,
    ICSD_ADT_STATE_SYNC,
    ICSD_ADT_EXPORT_DATA_WRITE,
    ICSD_ADT_ANC_SWITCH_OPEN,
    ICSD_ADT_ANC_SWITCH_CLOSE,

    /*对耳信息同步*/
    SYNC_ICSD_ADT_VDT_TIRRGER,
    SYNC_ICSD_ADT_VDT_RESUME,
    SYNC_ICSD_ADT_WIND_LVL_CMP,
    SYNC_ICSD_ADT_WAT_RESULT,
    SYNC_ICSD_ADT_WIND_LVL_RESULT,
    SYNC_ICSD_ADT_OPEN,
    SYNC_ICSD_ADT_CLOSE,
    SYNC_ICSD_ADT_SET_ANC_FADE_GAIN,
    SYNC_ICSD_ADT_ENV_NOISE_LVL_CMP,
    SYNC_ICSD_ADT_ENV_NOISE_LVL_RESULT,
    SYNC_ICSD_ADT_RTANC_DRC_RESULT,
    SYNC_ICSD_ADT_RTANC_TONE_RESUME,
    SYNC_ICSD_ADT_TONE_ADAPTIVE_SYNC,
};

enum ICSD_ADT_IOC {
    ICSD_ADT_IOC_INIT = 1,
    ICSD_ADT_IOC_EXIT,
    ICSD_ADT_IOC_SUSPEND,
    ICSD_ADT_IOC_RESUME,
};


//ANC扩展功能场景支持定义
enum ICSD_ADT_SCENE {
    //-------USER SCENE------------
    ADT_SCENE_IDEL          = 0,
    ADT_SCENE_A2DP          = BIT(0),	//A2DP场景
    ADT_SCENE_ESCO          = BIT(1),	//ESCO场景
    ADT_SCENE_MIC_EFFECT    = BIT(2),	//MIC混响场景
    ADT_SCENE_PC_MIC    	= BIT(3),	//PC(USB) MIC场景
    ADT_SCENE_TONE          = BIT(4),	//提示音场景(未启用)
    ADT_SCENE_ANC_OFF       = BIT(5),	//ANC_OFF场景
    ADT_SCENE_ANC_TRANS     = BIT(6),	//通透场景
    //用户定义场景在此添加

    //------SDK FIX SCENE 默认不支持ADT---------
    //序号位置修改需配合audio_icsd_adt_scene_check
    ADT_SCENE_AFQ           = BIT(16),	//AFQ
    ADT_SCENE_PRODUCTION    = BIT(17),	//产测
    ADT_SCENE_EAR_ADAPTIVE  = BIT(18),	//耳道自适应
};
#define ADT_FIX_MUTEX_SCENE_OFFSET	16	//ADT固定互斥场景偏移，ADT_SCENE_AFQ = BIT(16)


/*打开声音检测*/
int audio_acoustic_detector_open();

/*关闭声音检测*/
int audio_acoustic_detector_close();


void audio_icsd_adt_resume();
void audio_icsd_adt_suspend();
int audio_icsd_adt_param_update(void);

void audio_icsd_adt_algom_resume(u8 anc_mode);
void audio_icsd_adt_algom_suspend(void);

/*获取adt的模式*/
u16 get_icsd_adt_mode();

/*同步tws配对时，同步adt的状态*/
void audio_anc_icsd_adt_state_sync(struct anc_tws_sync_info *info);

int anc_adt_init();

int audio_icsd_adt_open(u16 adt_mode);
int audio_icsd_adt_sync_open(u16 adt_mode);

int audio_icsd_adt_sync_close(u16 adt_mode, u8 suspend);
int audio_icsd_adt_close(u32 scene, u8 run_sync, u16 adt_mode, u8 suspend);
int audio_icsd_adt_res_close(u16 adt_mode, u8 suspend);

u8 audio_icsd_adt_is_running();
void audio_icsd_adt_info_sync(u8 *data, int len);;
int audio_icsd_adt_open_permit(u16 adt_mode);

/*参数更新接口*/
int audio_acoustic_detector_updata();

/*获取免摘需要多少个mic*/
u8 get_icsd_adt_mic_num();

/*获取ADT模式切换是否繁忙*/
u8 get_icsd_adt_mode_switch_busy();

/*设置ADT 通透模式下的FB/CMP 参数*/
void audio_icsd_adt_trans_fb_param_set(audio_anc_t *param);

//获取当前ADT是否运行
u8 icsd_adt_is_running(void);

/************************* 广域点击相关接口 ***********************/
/*打开广域点击*/
int audio_wat_click_open();

/*关闭广域点击*/
int audio_wat_click_close();

/*设置是否忽略广域点击
 * ingore: 1 忽略点击，0 响应点击
 * 忽略点击的时间，单位ms*/
int audio_wide_area_tap_ignore_flag_set(u8 ignore, u16 time);

/*广域点击开关demo*/
void audio_wat_click_demo();

/*广域点击事件处理*/
void audio_wat_area_tap_event_handle(u8 wat_result);

/*广域点击识别结果输出回调*/
void audio_wat_click_output_handle(u8 wat_result);

/************************* 风噪检测相关接口 ***********************/

/*风噪检测识别结果输出回调*/
void audio_icsd_wind_detect_output_handle(u8 wind_thr);


int audio_cvp_icsd_wind_det_open();
int audio_cvp_icsd_wind_det_close();
u8 get_cvp_icsd_wind_lvl_det_state();
void audio_cvp_icsd_wind_det_demo();
void audio_cvp_wind_lvl_output_handle(u8 wind_lvl);


/************************* 音量自适应相关接口 ***********************/
/*打开音量自适应*/
int audio_icsd_adaptive_vol_open();

/*关闭音量自适应*/
int audio_icsd_adaptive_vol_close();

/*音量自适应使用demo*/
void audio_icsd_adaptive_vol_demo();

/*音量偏移处理*/
void audio_icsd_adptive_vol_event_process(u8 spldb_iir);

/************************* 环境增益自适应相关接口 ***********************/
/*打开anc增益自适应*/
int audio_anc_env_adaptive_gain_open();

/*关闭anc增益自适应*/
int audio_anc_env_adaptive_gain_close();

/*anc增益自适应使用demo*/
void audio_anc_env_adaptive_gain_demo();

/*环境噪声处理*/
int audio_env_noise_event_process(u8 spldb_iir);

/*设置环境自适应增益，带淡入淡出功能*/
void audio_anc_env_adaptive_fade_gain_set(int fade_gain, int fade_time);

/*重置环境自适应增益的初始参数*/
void audio_anc_env_adaptive_fade_param_reset(void);


/************************* 啸叫检测相关接口 ***********************/
/*打开啸叫检测*/
int audio_anc_howling_detect_open();

/*关闭啸叫检测*/
int audio_anc_howling_detect_close();

/*啸叫检测使用demo*/
void audio_anc_howling_detect_demo();

/*啸叫检测结果回调*/
void audio_anc_howling_detect_output_handle(u8 result);

/*初始化啸叫检测资源*/
void icsd_anc_soft_howling_det_init();

/*退出啸叫检测资源*/
void icsd_anc_soft_howling_det_exit(void);

//DRC处理结果回调
void audio_rtanc_drc_output(u8 output, float gain, float fb_gain);

//DRC TWS标志获取
u8 audio_rtanc_drc_flag_get(float **gain);

//非对耳同步打开
int audio_icsd_adt_no_sync_open(u16 adt_mode);

/*阻塞性 重启ADT算法*/
int audio_icsd_adt_reset(u32 scene);

/*异步 重启ADT算法*/
int audio_icsd_adt_async_reset(u32 scene);

int audio_icsd_anc_switch_open(u16 adt_mode);

int audio_icsd_anc_switch_close(u16 adt_mode);

int audio_icsd_adt_scene_check(u16 adt_mode, u8 skip_check);

int icsd_adt_a2dp_scene_set(u8 en);

int audio_icsd_adt_scene_set(enum ICSD_ADT_SCENE scene, u8 enable);

enum ICSD_ADT_SCENE audio_icsd_adt_scene_get(void);

void icsd_adt_lock_init();
void icsd_adt_lock();
void icsd_adt_unlock();

extern void *get_anc_lfb_coeff();
extern void *get_anc_lff_coeff();
extern void *get_anc_ltrans_coeff();
extern void *get_anc_ltrans_fb_coeff();
extern float get_anc_gains_l_fbgain();
extern float get_anc_gains_l_ffgain();
extern float get_anc_gains_l_transgain();
extern float get_anc_gains_lfb_transgain();
extern u8 get_anc_l_fbyorder();
extern u8 get_anc_l_ffyorder();
extern u8 get_anc_l_transyorder();
extern u8 get_anc_lfb_transyorder();
extern void icsd_adt_drc_output(u8 flag, float gain, float fb_gain);
extern u8 icsd_adt_drc_read(float **gain);

void audio_adt_rtanc_sync_tone_resume(void);

void audio_adt_tone_adaptive_sync(void);

int icsd_adt_app_mode_set(u16 adt_mode, u8 en);

u32 icsd_adt_app_mode_get(void);
void audio_icsd_adt_env_noise_det_set(u8 env_noise_det_en);
void audio_icsd_adt_env_noise_det_clear(u8 env_noise_det_clear);
u8 audio_icsd_adt_env_noise_det_get();
#endif
