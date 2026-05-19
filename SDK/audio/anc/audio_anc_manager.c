
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_anc_common.data.bss")
#pragma data_seg(".audio_anc_common.data")
#pragma const_seg(".audio_anc_common.text.const")
#pragma code_seg(".audio_anc_common.text")
#endif

/*
 ****************************************************************
 *							AUDIO ANC MANAGER
 * File  : audio_anc_manager.c
 * By    :
 * Notes : 存放ANC策略管理
 *
 ****************************************************************
 */
#include "app_config.h"

#if TCFG_AUDIO_ANC_ENABLE

#include "audio_anc_includes.h"

#define LOG_TAG             "[ANC_MANA]"
#define LOG_ERROR_ENABLE
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
/* #define LOG_CLI_ENABLE */
#include "debug.h"

/*
   配置不同业务场景下支持ANC_EXT 算法功能
   1、ANC模式、IDLE场景默认全支持
   2、多场景共存时，以不支持的场景优先
   3、数组修改注意
   		1）scene 需与枚举 enum ICSD_ADT_SCENE 的 USER SCENE 对齐
   		2）算法  需与枚举 enum ICSD_ADT_MODE  对齐
*/
const u8 anc_ext_support_scene[7][10] = {
    //0-智能免摘 1-风噪检测 2-广域点击 3-NULL 4-RTANC(RTEQ) 5-入耳检测 6-环境自适应 7-NULL 8-NULL 9-啸叫检测
    { 1,         1,         1,         0,     1,      		1,         1,           0,     0,     1         }, //A2DP场景
    { 0,         0,         0,         0,     1,      		0,         0,           0,     0,     1         }, //ESCO场景
    { 0,         0,         0,         0,     1,      		0,         0,           0,     0,     1         }, //MIC混响场景
    { 0,         0,         0,         0,     0,      		0,         0,           0,     0,     0         }, //PC(USB) MIC场景
    { 0,         0,         0,         0,     0,      		0,         0,           0,     0,     0         }, //提示音场景
    { 0,         0,         0,         0,     0,      		0,         0,           0,     0,     0         }, //ANC_OFF场景
#if ANC_MULT_TRANS_FB_ENABLE /*通透+FB 才能支持RTANC*/
    { 1,         1,         1,         0,     1,      		1,         1,           0,     0,     1         }, //通透场景
#else
    { 1,         1,         1,         0,     0,/*NO*/		1,         1,           0,     0,     1         }, //通透场景
#endif
};


//ANC模式切换时，启动算法功能 - 后续可被模块单独开关替代
int audio_anc_app_adt_mode_init(u8 enable)
{
    u16 adt_mode = 0;
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    adt_mode |= ADT_WIND_NOISE_DET_MODE;
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_WIND_DET, ANC_EXT_ALGO_STA_OPEN, 0);
#endif
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    adt_mode |= ADT_REAL_TIME_ADAPTIVE_ANC_MODE;
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_RTANC, ANC_EXT_ALGO_STA_OPEN, 0);
#endif
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
    adt_mode |= ADT_ENV_NOISE_DET_MODE;
    audio_icsd_adt_env_noise_det_set(AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE | AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE);
#endif
#if TCFG_AUDIO_ANC_HOWLING_DET_ENABLE
    adt_mode |= ADT_HOWLING_DET_MODE;
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_SOFT_HOWL_DET, ANC_EXT_ALGO_STA_OPEN, 0);
#endif
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    anc_ext_adaptive_cmp_tool_en_set(1);
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_ADAPTIVE_CMP, ANC_EXT_ALGO_STA_OPEN, 0);
#endif
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    anc_ext_adaptive_eq_tool_en_set(1);
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_ADAPTIVE_EQ, ANC_EXT_ALGO_STA_OPEN, 0);
#endif
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    adt_mode |= ADT_SPEAK_TO_CHAT_MODE;
#endif
    log_info("audio_anc_app_adt_mode_init 0x%x, en %d\n", adt_mode, enable);

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    icsd_adt_app_mode_set(adt_mode, enable);
#endif
    return 0;
}

static int audio_anc_event_adt_reset(int arg)
{
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    enum ICSD_ADT_SCENE scene = (enum ICSD_ADT_SCENE)arg;
    //通话/播歌场景复位ADT，需保留RTANC算法临时参数
    if ((scene & (ADT_SCENE_ESCO | ADT_SCENE_A2DP)) && audio_anc_real_time_adaptive_state_get()) {
        audio_rtanc_var_cache_set(1);
    }
#endif
    return 0;
}

static int audio_anc_event_adt_open(int arg)
{
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE && TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    int adt_mode = arg;
    if ((adt_mode & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) && (anc_mode_get() == ANC_OFF)) {
        audio_rtaeq_in_ancoff_open();
    } else {
        audio_rtaeq_in_ancoff_close();
    }
#endif
    return 0;
}

static int audio_anc_event_adt_close(int arg)
{
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE && TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    int adt_mode;
    //跳过检查定时器检查，避免删除ANC_OFF(RTAEQ)定时器
    adt_mode = audio_icsd_adt_scene_check(0, 1);

    if ((!(adt_mode & ADT_REAL_TIME_ADAPTIVE_ANC_MODE)) || (anc_mode_get() != ANC_OFF)) {
        audio_rtaeq_in_ancoff_close();
    }
#endif
    return 0;
}

static int audio_anc_event_adt_scene_check(int *arg)
{
    if (!arg) {
        return 0;
    }
    int out_mode = arg[0];
    int skip_check = arg[1];

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE && TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    enum ICSD_ADT_SCENE scene = audio_icsd_adt_scene_get();
    //ANC_OFF场景 特殊处理
    if (scene & ADT_SCENE_ANC_OFF) {
        if (out_mode & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
            printf("adt_scene:ANC_OFF mode %x, skip %d\n", out_mode, skip_check);
            //1、ANC_OFF(RTANC/EQ) 仅在A2DP下生效
            if (!(scene & ADT_SCENE_A2DP)) {
                out_mode &= ~ADT_REAL_TIME_ADAPTIVE_ANC_MODE;
            }
            //2、检查ANC_OFF 定时状态
            if ((!skip_check) && (!audio_rtaeq_ancoff_timer_state_get())) {
                out_mode &= ~ADT_REAL_TIME_ADAPTIVE_ANC_MODE;
            }
            printf("adt_scene:ANC_OFF out_mode %x\n", out_mode);
        }
    }
#endif
    return out_mode;
}

static int audio_anc_event_filter_update_before(void)
{
    // 滤波器更新前
    log_debug("Filter update before event handled");
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    if (audio_icsd_adt_is_running() && (get_icsd_adt_mode() & ADT_SPEAK_TO_CHAT_MODE)) {
        audio_icsd_adt_suspend();
    }
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    audio_anc_real_time_adaptive_suspend("COEFF_UPDATE");
#endif

    return 0;
}

static int audio_anc_event_filter_update_after(void)
{
    // 滤波器更新后
    log_debug("Filter update after event handled");
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    if (audio_icsd_adt_is_running() && (get_icsd_adt_mode() & ADT_SPEAK_TO_CHAT_MODE)) {
        //免摘需要更新滤波器信息
        audio_icsd_adt_param_update();
        audio_icsd_adt_resume();
    }
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    audio_anc_real_time_adaptive_resume("COEFF_UPDATE");
#endif
    return 0;
}

static int audio_anc_event_reset_before(void)
{
    // ANC复位前
    log_debug("Reset before event handled");
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    if (audio_icsd_adt_is_running()) {
        audio_icsd_adt_suspend();
    }
#endif
    return 0;
}

static int audio_anc_event_reset_after(void)
{
    // ANC复位后
    log_debug("Reset after event handled");

#if ANC_HOWLING_DETECT_EN
    //复位啸叫检测相关变量
    anc_howling_detect_reset();
#endif

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    if (audio_icsd_adt_is_running()) {
        audio_icsd_adt_param_update();
        audio_icsd_adt_resume();
    }
#endif
    return 0;
}

//ANC事件策略管理
int audio_anc_event_notify(enum anc_event event, int arg)
{
    int ret = 0;
    switch (event) {
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    case ANC_EVENT_ADT_RESET:
        audio_anc_event_adt_reset(arg);
        break;
    case ANC_EVENT_ADT_OPEN:
        audio_anc_event_adt_open(arg);
        break;
    case ANC_EVENT_ADT_CLOSE:
        audio_anc_event_adt_close(arg);
        break;
    case ANC_EVENT_ADT_SCENE_CHECK:
        ret = audio_anc_event_adt_scene_check((int *)arg);
        break;
#endif
    case ANC_EVENT_FILTER_UPDATE_BEFORE:
        audio_anc_event_filter_update_before();
        break;
    case ANC_EVENT_FILTER_UPDATE_AFTER:
        audio_anc_event_filter_update_after();
        break;
    case ANC_EVENT_RESET_BEFORE:
        audio_anc_event_reset_before();
        break;
    case ANC_EVENT_RESET_AFTER:
        audio_anc_event_reset_after();
        break;
    default:
        break;
    };
    return ret;
}


#endif
