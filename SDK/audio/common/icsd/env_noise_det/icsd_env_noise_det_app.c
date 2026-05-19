#include "app_config.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc_includes.h"
#if ((TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE) || \
        (ANC_ADAPTIVE_EN && TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE))
#include "system/includes.h"
#include "tone_player.h"
#include "audio_config.h"
#include "sniff.h"
#include "icsd_adt.h"
#include "hw_eq.h"

#if 1
#define env_log	printf
#else
#define env_log(...)
#endif/*log_en*/

struct audio_env_hdl {
    void *env_thr_hdl;
    struct audio_anc_lvl_sync *lvl_sync_hdl;
};
struct audio_env_hdl *env_hdl = NULL;

/* 噪声阈值表 */
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
//标准模式阈值表
const int env_thr_table[] = {0, 40, 80};
#else
//轻量模式阈值表
const int env_thr_table[] = {0, 60, 120};
#endif

#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
/*打开anc增益自适应*/
int audio_anc_env_adaptive_gain_open()
{
    env_log("%s", __func__);
    u8 adt_mode = ADT_ENV_NOISE_DET_MODE;
    audio_icsd_adt_env_noise_det_set(AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE);
    return audio_icsd_adt_sync_open(adt_mode);
}

/*关闭anc增益自适应*/
int audio_anc_env_adaptive_gain_close()
{
    env_log("%s", __func__);
    u8 adt_mode = ADT_ENV_NOISE_DET_MODE;
    audio_icsd_adt_env_noise_det_clear(AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE);
    int ret = audio_icsd_adt_sync_close(adt_mode, 0);
    return ret;
}

/*anc增益自适应使用demo*/
void audio_anc_env_adaptive_gain_demo()
{
    env_log("%s", __func__);
    if (audio_icsd_adt_open_permit(ADT_ENV_NOISE_DET_MODE) == 0) {
        return;
    }

    if ((get_icsd_adt_mode() & ADT_ENV_NOISE_DET_MODE) == 0) {
        /*打开提示音*/
        /* icsd_adt_tone_play(ICSD_ADT_TONE_WINDDET_ON); */
        audio_anc_env_adaptive_gain_open();
    } else {
        /*关闭提示音*/
        /* icsd_adt_tone_play(ICSD_ADT_TONE_WINDDET_OFF); */
        audio_anc_env_adaptive_gain_close();
    }
}
#endif

static struct anc_fade_handle anc_env_gain_fade = {
    .timer_ms = 100, //配置定时器周期
    .timer_id = 0, //记录定时器id
    .cur_gain = 16384, //记录当前增益
    .fade_setp = 0,//记录淡入淡出步进
    .target_gain = 16384, //记录目标增益
    .fade_gain_mode = 0,//记录当前设置fade gain的模式
};

/*重置环境自适应增益的初始参数*/
void audio_anc_env_adaptive_fade_param_reset(void)
{
    if (env_hdl && env_hdl->env_thr_hdl) {
        audio_threshold_det_lvl_reset(env_hdl->env_thr_hdl, ARRAY_SIZE(env_thr_table) - 1); //还原最高档位
    }
    if (anc_env_gain_fade.timer_id) {
        sys_timer_del(anc_env_gain_fade.timer_id);
        // sys_s_hi_timer_del(anc_env_gain_fade.timer_id);
        anc_env_gain_fade.timer_id = 0;
    }
    anc_env_gain_fade.cur_gain = 16384;
    anc_env_gain_fade.target_gain = 16384;
}

/*设置环境自适应增益，带淡入淡出功能*/
void audio_anc_env_adaptive_fade_gain_set(int fade_gain, int fade_time)
{
    audio_anc_gain_fade_process(&anc_env_gain_fade, ANC_FADE_MODE_ENV_ADAPTIVE_GAIN, fade_gain, fade_time);
}

static int audio_anc_env_enable_check()
{
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
    return (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE);
#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE
    return 1;
#endif
    return 1;
}

int audio_anc_env_thr_to_lvl(int env_thr)
{
    if (!env_hdl) {
        return -1;
    }
    if (!audio_anc_env_enable_check()) {
        return -1;
    }
    if (anc_mode_get() != ANC_OFF) {
        int env_lvl = audio_threshold_det_run(env_hdl->env_thr_hdl, env_thr);
#if ICSD_ENV_LVL_PRINTF
        printf("env_det-env_thr: %d, env_lvl: %d\n", env_thr, env_lvl);
#endif
        return env_lvl;
    }
    return -1;
}

void audio_anc_env_lvl_sync_info(u8 *data, int len)
{
    if (!env_hdl) {
        return;
    }
    audio_anc_lvl_sync_info(env_hdl->lvl_sync_hdl, data, len);
}

/*环境噪声处理*/
int audio_env_noise_event_process(u8 env_lvl)
{
    if (!env_hdl) {
        return -1;
    }
    if (!audio_anc_env_enable_check()) {
        return -1;
    }
    /*anc模式下才改anc增益*/
    if (anc_mode_get() == ANC_OFF) {
        return -1;
    }

    u16 anc_fade_gain = 16384;
    u16 anc_fade_time = 3000; //ms
    /*根据风噪等级改变anc增益*/
    switch (env_lvl) {
    case ANC_WIND_NOISE_LVL0:
        anc_fade_gain = (u16)(eq_db2mag(-20.0f) * ANC_FADE_GAIN_MAX_FLOAT);
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL1:
        anc_fade_gain = (u16)(eq_db2mag(-6.0f) * ANC_FADE_GAIN_MAX_FLOAT);;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL2:
        anc_fade_gain = 16384;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL3:
        anc_fade_gain = 16384;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL4:
        anc_fade_gain = 16384;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL5:
    case ANC_WIND_NOISE_LVL_MAX:
        anc_fade_gain = 16384;
        anc_fade_time = 3000;
        break;
    default:
        anc_fade_gain = 16384;
        anc_fade_time = 3000;
        break;
    }

    if (anc_env_gain_fade.target_gain == anc_fade_gain) {
        return anc_fade_gain;
    }
    env_log("ENV_NOISE_STATE:RUN, lvl %d, [anc_fade] %d\n", env_lvl, anc_fade_gain);
    audio_anc_env_adaptive_fade_gain_set(anc_fade_gain, anc_fade_time);
    return anc_fade_gain;
}

static void audio_anc_env_lvl_sync_cb(void *_hdl)
{
    struct audio_anc_lvl_sync *hdl = (struct audio_anc_lvl_sync *)_hdl;
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
    int err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_ENV_NOISE_LVL, hdl->cur_lvl);

#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE
    int err = os_taskq_post_msg("anc", 2, ANC_MSG_ENV_NOISE_LVL, hdl->cur_lvl);

#endif
    if (err != OS_NO_ERR) {
        printf("audio_anc_env_lvl_sync_cb err %d\n", err);
    }
}

static void audio_env_ioc_init()
{
    if (env_hdl) {
        return;
    }
    env_hdl = anc_malloc("ANC_ENV", sizeof(struct audio_env_hdl));
    if (!env_hdl) {
        return;
    }

    //阈值检测
    struct threshold_det_param param = {0};
    param.thr_table = env_thr_table;
    param.thr_lvl_num = ARRAY_SIZE(env_thr_table);
    param.thr_debounce = 10;
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
    param.run_interval = avc_run_interval * 11;
    param.lvl_up_hold_time = 1000;
    param.lvl_down_hold_time = 1000;
#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE
    param.run_interval = ANC_ADAPTIVE_POWER_DET_TIME / 2;
    param.lvl_up_hold_time = ANC_ADAPTIVE_POWER_DET_TIME / 2;
    param.lvl_down_hold_time = ANC_ADAPTIVE_POWER_DET_TIME / 2;
#endif
    param.default_lvl = ARRAY_SIZE(env_thr_table) - 1; //默认最高档位
    env_hdl->env_thr_hdl = audio_threshold_det_open(&param);

    //档位同步
    struct audio_anc_lvl_sync_param lvl_sync_param = {0};
    lvl_sync_param.sync_result_cb = audio_anc_env_lvl_sync_cb;
    lvl_sync_param.sync_check = 1;
    lvl_sync_param.high_lvl_sync = 1;
    lvl_sync_param.default_lvl = ARRAY_SIZE(env_thr_table) - 1; //默认最高档位
    lvl_sync_param.name = ANC_LVL_SYNC_ENV;
    env_hdl->lvl_sync_hdl = audio_anc_lvl_sync_open(&lvl_sync_param);
}

static void audio_env_ioc_exit()
{
    if (env_hdl) {
        if (env_hdl->env_thr_hdl) {
            audio_threshold_det_close(env_hdl->env_thr_hdl);
            env_hdl->env_thr_hdl = NULL;
        }
        if (env_hdl->lvl_sync_hdl) {
            audio_anc_lvl_sync_close(env_hdl->lvl_sync_hdl);
            env_hdl->lvl_sync_hdl = NULL;
        }
        anc_free(env_hdl);
        env_hdl = NULL;
    }
}

int audio_adt_env_ioctl(int cmd, int arg)
{
    switch (cmd) {
    case ANC_EXT_IOC_INIT:
        audio_env_ioc_init();
        break;
    case ANC_EXT_IOC_EXIT:
        audio_env_ioc_exit();
        break;
    }
    return 0;
}

#endif
#endif
