

#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
        TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_HOWLING_DET_ENABLE)
#include "system/includes.h"
#include "audio_anc_includes.h"
#include "tone_player.h"
#include "audio_config.h"
#include "sniff.h"

extern int (*howl_printf)(const char *format, ...);

#define ANC_SOFT_HOWLING_TARGET_GAIN				0		/*啸叫时的目标增益, range [0 - 16384]; default 0 */
#define ANC_SOFT_HOWLING_HOLD_TIME				1000	/*啸叫目标增益的持续时间(单位ms), range [0 - 10000]; default 1000 */
#define ANC_SOFT_HOWLING_RESUME_TIME				4000	/*恢复到正常增益的时间(单位ms), range [200 - 10000]; default 4000 */
#define ANC_SOFT_HOWLING_DETECT_FADE_GAIN_DEFAULT 	16384

enum anc_howling_soft_det_state {
    ANC_SOFT_HOWLING_DET_STATE_CLOSE = 0,
    ANC_SOFT_HOWLING_DET_STATE_OPEN,
    ANC_SOFT_HOWLING_DET_STATE_HOLD_TIME,
    ANC_SOFT_HOWLING_DET_STATE_SUSPEND,
};

struct anc_hd_soft_cfg {
    u16 hold_time;		//目标增益维持时间(单位ms)
    u16 resume_time;	//默认增益恢复时间(单位ms)
    int target_gain;	//啸叫时的目标增益
    int default_gain;	//没有啸叫默认增益
};

struct anc_hd_soft_handle {
    enum anc_howling_soft_det_state state;
    u8 busy;
    u16 resume_time_id;
    u16 hold_time_id;
    u16 clear_time_id;
    u16 now_gain;
    struct anc_hd_soft_cfg cfg;
    OS_SEM sem;
};

static struct anc_hd_soft_handle *anc_hd_hdl = NULL;
static spinlock_t anc_hd_lock;

/*打开啸叫检测*/
int audio_anc_howling_detect_open()
{
    howl_printf("audio_anc_howling_detect_open");
    u16 adt_mode = ADT_HOWLING_DET_MODE;
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_SOFT_HOWL_DET, ANC_EXT_ALGO_STA_OPEN, 0);
    return audio_icsd_adt_sync_open(adt_mode);
}

/*关闭啸叫检测*/
int audio_anc_howling_detect_close()
{
    howl_printf("audio_anc_howling_detect_open");
    u16 adt_mode = ADT_HOWLING_DET_MODE;
    anc_ext_algorithm_state_update(ANC_EXT_ALGO_SOFT_HOWL_DET, ANC_EXT_ALGO_STA_CLOSE, 0);
    return audio_icsd_adt_sync_close(adt_mode, 0);
}

/*啸叫检测使用demo*/
void audio_anc_howling_detect_demo()
{
    howl_printf("audio_anc_howling_detect_demo");
    if (audio_icsd_adt_open_permit(ADT_HOWLING_DET_MODE) == 0) {
        return;
    }

    if ((get_icsd_adt_mode() & ADT_HOWLING_DET_MODE) == 0) {
        audio_anc_howling_detect_open();
    } else {
        audio_anc_howling_detect_close();
    }
}

/*ANC啸叫抑制 相关性中断后 恢复API*/
static void anc_soft_howling_det_resume(void *priv)
{
    struct anc_hd_soft_handle *hdl = anc_hd_hdl;
    if (hdl) {
        hdl->now_gain += hdl->cfg.default_gain / (hdl->cfg.resume_time / 200);
        if (hdl->now_gain > hdl->cfg.default_gain) {
            hdl->now_gain = hdl->cfg.default_gain;
            audio_anc_howldet_fade_set(hdl->now_gain);
            howl_printf("[anc_hd]resume time end\n");
            sys_hi_timer_del(hdl->resume_time_id);
            hdl->resume_time_id = 0;
            return;
        }
        audio_anc_howldet_fade_set(hdl->now_gain);
    }
}

/*增益恢复定时器注册*/
static void anc_soft_howling_det_resume_time_add(void *priv)
{
    struct anc_hd_soft_handle *hdl = anc_hd_hdl;
    if (hdl) {
        hdl->hold_time_id = 0;
        if (hdl->state == ANC_SOFT_HOWLING_DET_STATE_HOLD_TIME) {
            hdl->state = ANC_SOFT_HOWLING_DET_STATE_OPEN;
            hdl->resume_time_id = sys_hi_timer_add(NULL, anc_soft_howling_det_resume, 200);
        }
        howl_printf("[anc_hd]hold time end\n");
    }
}

/*啸叫检测结果回调*/
void audio_anc_howling_detect_output_handle(u8 result)
{
    struct anc_hd_soft_handle *hdl = anc_hd_hdl;
    if (result) {
        if (hdl) {
            howl_printf("=======================================================howling result is %d\n", result);
            if (hdl->state == ANC_SOFT_HOWLING_DET_STATE_OPEN) {
                hdl->state = ANC_SOFT_HOWLING_DET_STATE_HOLD_TIME;
                if (hdl->hold_time_id) {
                    sys_hi_timeout_del(hdl->hold_time_id);
                    hdl->hold_time_id = 0;
                }
                if (hdl->resume_time_id) {
                    sys_hi_timer_del(hdl->resume_time_id);
                    hdl->resume_time_id = 0;
                }
                hdl->now_gain = hdl->cfg.target_gain;
                audio_anc_howldet_fade_set(hdl->now_gain);
                hdl->hold_time_id = sys_hi_timeout_add(NULL, anc_soft_howling_det_resume_time_add, hdl->cfg.hold_time);
            }
        }
    } else {
        //howl_printf("n");
    }
}

/*初始化啸叫检测资源*/
void icsd_anc_soft_howling_det_init()
{
    howl_printf("[anc_hdl] icsd_anc_soft_howling_det_init\n");
    if (anc_hd_hdl) {
        return;
    }

    struct anc_hd_soft_handle *hdl = anc_malloc("ICSD_ANC_HOWL", sizeof(struct anc_hd_soft_handle));
    hdl->cfg.target_gain = ANC_SOFT_HOWLING_TARGET_GAIN ;
    hdl->cfg.default_gain = ANC_SOFT_HOWLING_DETECT_FADE_GAIN_DEFAULT ;
    hdl->cfg.hold_time   = ANC_SOFT_HOWLING_HOLD_TIME ;
    hdl->cfg.resume_time = ANC_SOFT_HOWLING_RESUME_TIME;
    hdl->state           = ANC_SOFT_HOWLING_DET_STATE_OPEN;
    spin_lock_init(&anc_hd_lock);
    anc_hd_hdl = hdl;
}

/*退出啸叫检测资源*/
void icsd_anc_soft_howling_det_exit(void)
{
    howl_printf("[anc_hd] icsd_anc_soft_howling_det_exit\n");
    struct anc_hd_soft_handle *hdl = anc_hd_hdl;
    if (hdl) {
        if (hdl->state != ANC_SOFT_HOWLING_DET_STATE_CLOSE) {
            hdl->state = ANC_SOFT_HOWLING_DET_STATE_CLOSE;
            if (hdl->hold_time_id) {
                sys_hi_timeout_del(hdl->hold_time_id);
                hdl->hold_time_id = 0;
            }
            if (hdl->resume_time_id) {
                sys_hi_timer_del(hdl->resume_time_id);
                hdl->resume_time_id = 0;
            }

            audio_anc_howldet_fade_set(hdl->cfg.default_gain);

            spin_lock(&anc_hd_lock);
            anc_free(hdl);
            hdl = NULL;
            anc_hd_hdl = NULL;
            spin_unlock(&anc_hd_lock);
        }
    }
}

#endif
