
#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
        TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE)
#include "system/includes.h"
#include "audio_anc_includes.h"
#include "anc.h"
#include "tone_player.h"
#include "audio_config.h"
#include "sniff.h"

#if 1
#define wind_log printf
#else
#define wind_log(...)
#endif

static void audio_anc_wind_det_spp_send_data(u8 cmd, u8 *buf, int len);

struct audio_wind_hdl {
    void *wind_thr_hdl;
    struct audio_anc_lvl_sync *lvl_sync_hdl;
};
static struct audio_wind_hdl *wind_hdl = NULL;

/* 噪声阈值表 */
const int wind_thr_table[] = {0, 30, 60, 90, 120, 150};

/*打开风噪检测*/
int audio_icsd_wind_detect_open()
{
    wind_log("%s", __func__);
    u8 adt_mode = ADT_WIND_NOISE_DET_MODE;
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_WIND_DET, ANC_EXT_ALGO_STA_OPEN, 0);
    return audio_icsd_adt_sync_open(adt_mode);
}

/*关闭风噪检测*/
int audio_icsd_wind_detect_close()
{
    wind_log("%s", __func__);
    u8 adt_mode = ADT_WIND_NOISE_DET_MODE;
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_WIND_DET, ANC_EXT_ALGO_STA_CLOSE, 0);
    return audio_icsd_adt_sync_close(adt_mode, 0);
}

/*风噪检测使用demo*/
void audio_icsd_wind_detect_demo()
{
    wind_log("%s", __func__);
    if (audio_icsd_adt_open_permit(ADT_WIND_NOISE_DET_MODE) == 0) {
        return;
    }

    if ((get_icsd_adt_mode() & ADT_WIND_NOISE_DET_MODE) == 0) {
        /*打开提示音*/
        icsd_adt_tone_play(ICSD_ADT_TONE_WINDDET_ON);
        audio_icsd_wind_detect_open();
    } else {
        /*关闭提示音*/
        icsd_adt_tone_play(ICSD_ADT_TONE_WINDDET_OFF);
        audio_icsd_wind_detect_close();
    }
}

static struct anc_fade_handle anc_wind_gain_fade = {
    .timer_ms = 100, //配置定时器周期
    .timer_id = 0, //记录定时器id
    .cur_gain = 16384, //记录当前增益
    .fade_setp = 0,//记录淡入淡出步进
    .target_gain = 16384, //记录目标增益
    .fade_gain_mode = 0,//记录当前设置fade gain的模式
};

/*重置风噪检测的初始参数*/
void audio_anc_wind_noise_fade_param_reset(void)
{
    if (wind_hdl && wind_hdl->wind_thr_hdl) {
        audio_threshold_det_lvl_reset(wind_hdl->wind_thr_hdl, 0); //还原最低档位
    }
    if (anc_wind_gain_fade.timer_id) {
        sys_timer_del(anc_wind_gain_fade.timer_id);
        //sys_s_hi_timer_del(anc_wind_gain_fade.timer_id);
        anc_wind_gain_fade.timer_id = 0;
    }
    anc_wind_gain_fade.cur_gain = 16384;
    anc_wind_gain_fade.target_gain = 16384;
}

/*设置风噪检测设置增益，带淡入淡出功能*/
void audio_anc_wind_noise_fade_gain_set(int fade_gain, int fade_time)
{
    audio_anc_gain_fade_process(&anc_wind_gain_fade, ANC_FADE_MODE_WIND_NOISE, fade_gain, fade_time);
}

int audio_anc_wind_thr_to_lvl(int wind_thr)
{
    if (!wind_hdl) {
        return -1;
    }
#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
    struct __anc_ext_wind_trigger_cfg *trigger_cfg = anc_ext_ear_adaptive_cfg_get()->wind_trigger_cfg;
#else
    struct __anc_ext_wind_trigger_cfg *trigger_cfg = NULL;
#endif
    /*anc模式下才改anc增益*/
    if (anc_mode_get() != ANC_OFF) {
        if (trigger_cfg) { //use anc_ext_tool cfg
            //临时接入风噪检测在线调试
            int *wind_thr_table_update = zalloc(sizeof(int) * 6);
            wind_thr_table_update[0] = 0;
            wind_thr_table_update[1] = trigger_cfg->thr[0];
            wind_thr_table_update[2] = trigger_cfg->thr[1];
            wind_thr_table_update[3] = trigger_cfg->thr[2];
            wind_thr_table_update[4] = trigger_cfg->thr[3];
            wind_thr_table_update[5] = trigger_cfg->thr[4];

            //其他参数更新暂用立即数代替，后续需要使用工具下发参数
            struct threshold_det_update_param update_param = {0};
            update_param.thr_table = wind_thr_table_update;
            update_param.run_interval = 150;
            update_param.lvl_up_hold_time = 1000;
            update_param.lvl_down_hold_time = 1000;
            update_param.thr_lvl_num = 6;
            update_param.thr_debounce = 10;

            audio_threshold_det_update_param(wind_hdl->wind_thr_hdl, &update_param);
            free(wind_thr_table_update);
        }
        int wind_lvl = audio_threshold_det_run(wind_hdl->wind_thr_hdl, wind_thr);
#if ICSD_WIND_LVL_PRINTF
        printf("wind_thr: %d, wind_lvl: %d\n", wind_thr, wind_lvl);
#endif
        return wind_lvl;
    }
    return -1;

}

void audio_anc_wind_lvl_sync_info(u8 *data, int len)
{
    if (!wind_hdl) {
        return;
    }
    audio_anc_lvl_sync_info(wind_hdl->lvl_sync_hdl, data, len);
}


/*anc风噪检测的处理*/
int audio_anc_wind_noise_process(u8 wind_lvl)
{
    if (!wind_hdl) {
        return -1;
    }
#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
    struct __anc_ext_wind_trigger_cfg *trigger_cfg = anc_ext_ear_adaptive_cfg_get()->wind_trigger_cfg;
#else
    struct __anc_ext_wind_trigger_cfg *trigger_cfg = NULL;
#endif
    /*anc模式下才改anc增益*/
    if (anc_mode_get() == ANC_OFF) {
        return -1;
    }
    u16 anc_fade_gain = 16384;
    u16 anc_fade_time = 3000; //ms
    /*根据风噪等级改变anc增益*/
    switch (wind_lvl) {
    case ANC_WIND_NOISE_LVL0:
        anc_fade_gain = 16384;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL1:
        anc_fade_gain = 10000;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL2:
        anc_fade_gain = 8000;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL3:
        anc_fade_gain = 6000;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL4:
        anc_fade_gain = 3000;
        anc_fade_time = 3000;
        break;
    case ANC_WIND_NOISE_LVL5:
    case ANC_WIND_NOISE_LVL_MAX:
        anc_fade_gain = 0;
        anc_fade_time = 3000;
        break;
    default:
        anc_fade_gain = 0;
        anc_fade_time = 3000;
        break;
    }
    if (trigger_cfg) { //use anc_ext_tool cfg
        if (wind_lvl != ANC_WIND_NOISE_LVL0) {
            int cfg_lvl = wind_lvl - ANC_WIND_NOISE_LVL1;
            //等级计算可能会超，超过最大则用最大
            cfg_lvl = (cfg_lvl > 4) ? 4 : cfg_lvl;
            if (cfg_lvl >= 0) {
                anc_fade_gain = trigger_cfg->gain[cfg_lvl];
            } else {
                printf("ERR: wind_lvl out of range\n");
            }
        }
    }

    if (anc_wind_gain_fade.target_gain == anc_fade_gain) {
        return anc_fade_gain;
    }

    wind_log("WIND_DET_STATE:RUN, lvl %d, gain %d\n", wind_lvl, anc_fade_gain);

#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
    if (anc_ext_debug_tool_function_get() & ANC_EXT_FUNC_SPP_EN_APP) {
        u8 send_buf[3];
        send_buf[0] = wind_lvl;
        memcpy(send_buf + 1, (u8 *)&anc_fade_gain, sizeof(u16));
        audio_anc_debug_app_send_data(ANC_DEBUG_APP_WIND_DET, 0, send_buf, sizeof(send_buf));
    }
#endif
    audio_anc_wind_noise_fade_gain_set(anc_fade_gain, anc_fade_time);
    return anc_fade_gain;
}

static void audio_anc_wind_lvl_sync_cb(void *_hdl)
{
    struct audio_anc_lvl_sync *hdl = (struct audio_anc_lvl_sync *)_hdl;
    int err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_WIND_LVL, hdl->cur_lvl);

    if (err != OS_NO_ERR) {
        printf("audio_anc_wind_lvl_sync_cb err %d\n", err);
    }
}

static void audio_wind_ioc_init()
{
    if (wind_hdl) {
        return;
    }
    wind_hdl = anc_malloc("ICSD_WIND", sizeof(struct audio_wind_hdl));
    if (!wind_hdl) {
        return;
    }

    //阈值检测
    struct threshold_det_param param = {0};
    param.thr_table = wind_thr_table;
    param.thr_lvl_num = ARRAY_SIZE(wind_thr_table);
    param.thr_debounce = 10;
    param.run_interval = 150; //风噪间隔150ms
    param.lvl_up_hold_time = 1000;
    param.lvl_down_hold_time = 1000;
    param.default_lvl = 0; //默认最低档位
    wind_hdl->wind_thr_hdl = audio_threshold_det_open(&param);

    //档位同步
    struct audio_anc_lvl_sync_param lvl_sync_param = {0};
    lvl_sync_param.sync_result_cb = audio_anc_wind_lvl_sync_cb;
    lvl_sync_param.sync_check = 1;
    lvl_sync_param.high_lvl_sync = 1;
    lvl_sync_param.default_lvl = 0; //默认最低档位
    lvl_sync_param.name = ANC_LVL_SYNC_WIND;
    wind_hdl->lvl_sync_hdl = audio_anc_lvl_sync_open(&lvl_sync_param);
}

static void audio_wind_ioc_exit()
{
    if (wind_hdl) {
        if (wind_hdl->wind_thr_hdl) {
            audio_threshold_det_close(wind_hdl->wind_thr_hdl);
            wind_hdl->wind_thr_hdl = NULL;
        }
        if (wind_hdl->lvl_sync_hdl) {
            audio_anc_lvl_sync_close(wind_hdl->lvl_sync_hdl);
            wind_hdl->lvl_sync_hdl = NULL;
        }
        anc_free(wind_hdl);
        wind_hdl = NULL;
    }
}

int audio_adt_wind_ioctl(int cmd, int arg)
{
    switch (cmd) {
    case ANC_EXT_IOC_INIT:
        audio_wind_ioc_init();
        break;
    case ANC_EXT_IOC_EXIT:
        audio_wind_ioc_exit();
        break;
    }
    return 0;
}

#endif /*(defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
