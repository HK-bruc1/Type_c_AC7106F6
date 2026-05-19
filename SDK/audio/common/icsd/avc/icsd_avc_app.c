#include "app_config.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc_includes.h"
#if ((TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE) || \
        (ANC_ADAPTIVE_EN && TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE))

#include "system/includes.h"
#include "anc.h"
#include "tone_player.h"
#include "audio_config.h"
#include "sniff.h"
#include "icsd_adt.h"

struct audio_avc_hdl {
    void *avc_thr_hdl;  //噪声阈值检测模块句柄
    struct audio_anc_lvl_sync *lvl_sync_hdl;
    u16 last_vol_dB;    //记录上次设置的音量，防止重复设置
};
static struct audio_avc_hdl *avc_hdl = NULL;

/* 注：噪声阈值表成员数，需要与音量偏移表第二维的成员数（即音量偏移表的列数）相同 */

/* 1、噪声阈值表 */
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
//标准模式阈值表
const int avc_thr_table[] = {0, 80, 160};
#else
//轻量模式阈值表
const int avc_thr_table[] = {0, 60, 120};
#endif

/* 2、音量偏移表 */
#define VOLUME_LEVEL    5 //将音量划分5个区间
const u16 anc_avc_offset_table[][3] = { //不同音量区间下，同一噪声等级偏移的音量offset不同，使用该表控制
    /* noise_lvl0  noise_lvl1  noise_lvl2 */
    {0,         20,         35}, /*第1区间的音量: cur_vol < max_vol * 1/VOLUME_LEVEL*/
#if (VOLUME_LEVEL >= 2)
    {0,         10,         22}, /*第2区间的音量: cur_vol < max_vol * 2/VOLUME_LEVEL , cur_vol >= max_vol * 1/VOLUME_LEVEL*/
#endif
#if (VOLUME_LEVEL >= 3)
    {0,         8,          15}, /*第3区间的音量: cur_vol < max_vol * 3/VOLUME_LEVEL, cur_vol >= max_vol * 2/VOLUME_LEVEL*/
#endif
#if (VOLUME_LEVEL >= 4)
    {0,         4,          12}, /*第4区间的音量: cur_vol < max_vol * 4/VOLUME_LEVEL, cur_vol >= max_vol * 3/VOLUME_LEVEL*/
#endif
#if (VOLUME_LEVEL >= 5)
    {0,         6,          10}, /*第5区间的音量: cur_vol < max_vol * 5/VOLUME_LEVELi, cur_vol >= max_vol * 4/VOLUME_LEVEL*/
#endif
};

#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
/*打开音量自适应*/
int audio_icsd_adaptive_vol_open()
{
    printf("%s", __func__);
    u8 adt_mode = ADT_ENV_NOISE_DET_MODE;
    audio_icsd_adt_env_noise_det_set(AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE);
    return audio_icsd_adt_sync_open(adt_mode);
}

/*关闭音量自适应*/
int audio_icsd_adaptive_vol_close()
{
    printf("%s", __func__);
    u8 adt_mode = ADT_ENV_NOISE_DET_MODE;
    audio_icsd_adt_env_noise_det_clear(AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE);
    int ret = audio_icsd_adt_sync_close(adt_mode, 0);
    return ret;
}

/*音量自适应使用demo*/
void audio_icsd_adaptive_vol_demo()
{
    printf("%s", __func__);
    if (audio_icsd_adt_open_permit(ADT_ENV_NOISE_DET_MODE) == 0) {
        return;
    }

    if ((get_icsd_adt_mode() & ADT_ENV_NOISE_DET_MODE) == 0) {
        /*打开提示音*/
        /* icsd_adt_tone_play(ICSD_ADT_TONE_WINDDET_ON); */
        audio_icsd_adaptive_vol_open();
    } else {
        /*关闭提示音*/
        /* icsd_adt_tone_play(ICSD_ADT_TONE_WINDDET_OFF); */
        audio_icsd_adaptive_vol_close();
    }
}
#endif

static int audio_anc_avc_enable_check()
{
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
    return (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE);
#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE
    return 1;
#endif
    return 1;
}

int audio_anc_avc_thr_to_lvl(int avc_thr)
{
    if (!avc_hdl) {
        return -1;
    }
    if (!audio_anc_avc_enable_check()) {
        return -1;
    }
    /*anc模式下才改音量*/
    if (anc_mode_get() != ANC_OFF) {
        /*划分环境噪声等级*/
        int avc_lvl = audio_threshold_det_run(avc_hdl->avc_thr_hdl, avc_thr);
#if ICSD_AVC_LVL_PRINTF
        printf("avc_det-avc_thr: %d, avc_lvl: %d\n", avc_thr, avc_lvl);
#endif
        return avc_lvl;
    }
    return -1;
}

void audio_anc_avc_lvl_sync_info(u8 *data, int len)
{
    if (!avc_hdl) {
        return;
    }
    audio_anc_lvl_sync_info(avc_hdl->lvl_sync_hdl, data, len);
}


/*音量偏移处理*/
void audio_icsd_adptive_vol_event_process(u8 avc_lvl)
{
    if (!avc_hdl) {
        return;
    }
    if (!audio_anc_avc_enable_check()) {
        return;
    }
    if (anc_mode_get() == ANC_OFF) {
        return;
    }
    /*获取最大音量等级*/
    s16 max_vol = app_audio_get_max_volume();
    s16 cur_vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
#if ICSD_AVC_LVL_PRINTF
    printf("avc_det-cur_vol %d, max_vol %d", cur_vol, max_vol);
#endif
    u8 vol_lvl = 0;

    /*
     * 查找当前音量在哪个音量区间，确定vol_lvl档位
     * 该逻辑也可根据实际样机的音量表以及调试情况自行修改
     */
    for (u8 i = 0; i < VOLUME_LEVEL; i++) {
        if (cur_vol <= (max_vol * (i + 1) / VOLUME_LEVEL)) {
            vol_lvl = i;
            break;
        }
    }

#if ICSD_AVC_LVL_PRINTF
    printf("avc_det-vol_lvl %d, noise_lvl %d, offset_dB %d\n", vol_lvl, avc_lvl, anc_avc_offset_table[vol_lvl][avc_lvl]);
#endif
    if (avc_hdl->last_vol_dB == anc_avc_offset_table[vol_lvl][avc_lvl]) {
        //与上次计算结果得到的音量一致，不设置
        return;
    }
    audio_app_set_vol_offset_dB((float)anc_avc_offset_table[vol_lvl][avc_lvl]);
    avc_hdl->last_vol_dB = anc_avc_offset_table[vol_lvl][avc_lvl];
}

static void audio_anc_avc_lvl_sync_cb(void *_hdl)
{
    struct audio_anc_lvl_sync *hdl = (struct audio_anc_lvl_sync *)_hdl;
    int err = 0;
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
    err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_AVC_NOISE_LVL, hdl->cur_lvl);

#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE
    err = os_taskq_post_msg("anc", 2, ANC_MSG_AVC_NOISE_LVL, hdl->cur_lvl);

#endif
    if (err != OS_NO_ERR) {
        printf("audio_anc_avc_lvl_sync_cb err %d\n", err);
    }

}

static void audio_avc_ioc_init()
{
    if (avc_hdl) {
        return;
    }
    avc_hdl = anc_malloc("ANC_AVC", sizeof(struct audio_avc_hdl));
    if (!avc_hdl) {
        return;
    }
    //阈值检测
    struct threshold_det_param param = {0};
    param.thr_table = avc_thr_table;
    param.thr_lvl_num = ARRAY_SIZE(avc_thr_table);
    param.thr_debounce = 10;
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
    param.run_interval = avc_run_interval * 11;
    param.lvl_up_hold_time = 1000;
    param.lvl_down_hold_time = 1000;
#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE
    param.run_interval = ANC_ADAPTIVE_POWER_DET_TIME / 2;
    param.lvl_up_hold_time = ANC_ADAPTIVE_POWER_DET_TIME / 2;
    param.lvl_down_hold_time = ANC_ADAPTIVE_POWER_DET_TIME / 2;
#endif
    param.default_lvl = 0; //默认最低档位
    avc_hdl->avc_thr_hdl = audio_threshold_det_open(&param);

    //档位同步
    struct audio_anc_lvl_sync_param lvl_sync_param = {0};
    lvl_sync_param.sync_result_cb = audio_anc_avc_lvl_sync_cb;
    lvl_sync_param.sync_check = 1;
    lvl_sync_param.high_lvl_sync = 1;
    lvl_sync_param.default_lvl = 0; //默认最低档位
    lvl_sync_param.name = ANC_LVL_SYNC_AVC;
    avc_hdl->lvl_sync_hdl = audio_anc_lvl_sync_open(&lvl_sync_param);
}

static void audio_avc_ioc_exit()
{
    if (avc_hdl) {
        if (avc_hdl->avc_thr_hdl) {
            audio_threshold_det_close(avc_hdl->avc_thr_hdl);
            avc_hdl->avc_thr_hdl = NULL;
        }
        if (avc_hdl->lvl_sync_hdl) {
            audio_anc_lvl_sync_close(avc_hdl->lvl_sync_hdl);
            avc_hdl->lvl_sync_hdl = NULL;
        }
        anc_free(avc_hdl);
        avc_hdl = NULL;
    }
    /*关闭音量自适应后恢复音量*/
    audio_app_set_vol_offset_dB(0);
}

int audio_adt_avc_ioctl(int cmd, int arg)
{
    switch (cmd) {
    case ANC_EXT_IOC_INIT:
        audio_avc_ioc_init();
        break;
    case ANC_EXT_IOC_EXIT:
        audio_avc_ioc_exit();
        break;
    }
    return 0;
}


#endif
#endif
