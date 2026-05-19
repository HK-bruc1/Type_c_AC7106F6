
#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE)
#include "system/includes.h"
#include "audio_anc_includes.h"
#include "tone_player.h"
#include "audio_config.h"
#include "sniff.h"
#include "btstack/avctp_user.h"
#include "bt_tws.h"

#define LOG_TAG             "[ANC_VDT]"
#define LOG_ERROR_ENABLE
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
/* #define LOG_CLI_ENABLE */
#include "debug.h"

struct vdt_trigger_sta {
    u8 a2dp_pause;			//免摘-暂停音乐
    u8 switch_flag;			//免摘-发生模式切换
    u32 next_period;		//免摘触发后下一个触发时间，避免频繁触发
};

struct audio_vdt_hdl {
    u8 state;		 		//免摘状态
    u8 keep_state; 			//算法状态保存
    u8 sensitivity;			//灵敏度
    u8 tws_trigger;			//TWS回连触发状态同步
    u16 end_time; 			//免摘定时结束的时间，单位ms
    u16 timer;				//免摘恢复定时器

    struct vdt_trigger_sta trigger;	//免摘触发APP状态记录
};

static struct audio_vdt_hdl *vdt_hdl = NULL;

/************************* start 智能免摘相关接口 ***********************/

/*智能免摘识别结果输出回调*/
void audio_speak_to_chat_output_handle(u8 voice_state)
{
    //putchar('<');
    //putchar('A');
    //putchar('0' + voice_state);
    //putchar('>');
    if (vdt_hdl->keep_state) {
        //模式切换中
        return;
    }
    u8 data[4] = {SYNC_ICSD_ADT_VDT_TIRRGER, voice_state, 0, 0};

    if (voice_state && time_after(jiffies, vdt_hdl->trigger.next_period)) {
        //间隔一秒唤醒，避免频繁触发
        vdt_hdl->trigger.next_period = jiffies + msecs_to_jiffies(1000);

#if TCFG_USER_TWS_ENABLE
        if (tws_in_sniff_state() && (tws_api_get_role() == TWS_ROLE_MASTER)) {
            /*如果在蓝牙siniff下需要退出蓝牙sniff再发送, 加快sniff下的免摘触发速度*/
            icsd_adt_tx_unsniff_req();
        }
#endif
        /*同步状态*/
        audio_icsd_adt_info_sync(data, 4);
    }
}

/*打开智能免摘*/
int audio_speak_to_chat_open()
{
    log_info("%s\n", __func__);
    u8 adt_mode = ADT_SPEAK_TO_CHAT_MODE;
    return audio_icsd_adt_sync_open(adt_mode);
}

/*关闭智能免摘*/
int audio_speak_to_chat_close()
{
    log_info("%s", __func__);
    u8 adt_mode = ADT_SPEAK_TO_CHAT_MODE;
    return audio_icsd_adt_sync_close(adt_mode, 0);
}

void audio_speak_to_chat_demo()
{
    log_info("%s", __func__);
    /*判断智能免摘是否已经打开*/
    if ((get_icsd_adt_mode() & ADT_SPEAK_TO_CHAT_MODE) == 0) {
        /*打开提示音*/
        icsd_adt_tone_play(ICSD_ADT_TONE_SPKCHAT_ON);
        audio_speak_to_chat_open();
    } else {
        /*关闭提示音*/
        icsd_adt_tone_play(ICSD_ADT_TONE_SPKCHAT_OFF);
        audio_speak_to_chat_close();
    }
}

/*获取智能免摘是否触发 return 1 触发; 0 不触发*/
u8 audio_speak_to_chat_is_trigger()
{
    return (vdt_hdl->state == AUDIO_VDT_STATE_TRIGGER) ? 1 : 0;
}

/*恢复免摘之前的状态*/
void audio_speak_to_chat_resume(void)
{
    log_info("%s, state:%d\n", __func__, vdt_hdl->state);
    u8 data[4];
    if (vdt_hdl && vdt_hdl->state) {
        /* vdt_hdl->adt_suspend = 0; */
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state()) {
            data[0] = SYNC_ICSD_ADT_VDT_RESUME;
            audio_icsd_adt_info_sync(data, 4);
        } else {
            audio_vdt_resume_send_to_adt();
        }
#else
        audio_vdt_resume_send_to_adt();
#endif/*TCFG_USER_TWS_ENABLE*/
    }
}

/*APP需求：设置免摘定时结束的时间，单位ms*/
int audio_speak_to_char_end_time_set(u16 time)
{
    if (vdt_hdl) {
        vdt_hdl->end_time = time;
    }
    return 0;
}

/*APP需求：设置智能免摘检测的灵敏度*/
int audio_speak_to_chat_sensitivity_set(u8 sensitivity)
{
    //目前暂不支持
    return 0;
}

/*----------------------SDK 内部接口--------------------*/

void audio_speak_to_chat_init(void)
{
    /* log_debug("%s \n", __func__); */
    vdt_hdl = anc_malloc("ICSD_VDT", sizeof(struct audio_vdt_hdl));
    vdt_hdl->end_time = AUDIO_SPEAK_TO_CHAT_END_TIME;
}

/*检测到讲话状态执行*/
void audio_vdt_trigger_send_to_adt(u8 state)
{
    int err = 0;
    if (icsd_adt_is_running() && state) {
        log_debug("%s", __func__);
        err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_VDT_TIRRGER, (int)vdt_hdl);
        if (err != OS_NO_ERR) {
            log_error("%s err %d", __func__, err);
        }
    }
}

//触发免摘之后恢复至默认状态的定时器
static void audio_speak_to_chat_resume_timer(void *p)
{
    log_debug("%s\n", __func__);
    vdt_hdl->timer = 0;
    audio_speak_to_chat_resume();
}

static void audio_vdt_a2dp_pause(u8 pause)
{
    if (pause) {
        if (bt_a2dp_get_status() == BT_MUSIC_STATUS_STARTING) {
            log_info("send PAUSE cmd");
            vdt_hdl->trigger.a2dp_pause = 1;
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
        }
    } else {
        if (vdt_hdl->trigger.a2dp_pause && (bt_a2dp_get_status() != BT_MUSIC_STATUS_STARTING)) {
            log_info("send PLAY cmd");
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
        vdt_hdl->trigger.a2dp_pause = 0;
    }
}

//算法输出状态改变接口
int audio_vdt_trigger_process(u8 tone)
{
    //in speak_no_chat task
    log_debug("%s\n", __func__);
    switch (vdt_hdl->state) {
    case AUDIO_VDT_STATE_OPEN:

        vdt_hdl->state = AUDIO_VDT_STATE_TRIGGER;
        log_info("VDT_STATE:TRIGGER");

        /*检测到语音切换到通透*/
        if (anc_mode_get() != ANC_TRANSPARENCY) {
            audio_icsd_adt_algom_suspend();	//切换ANC之后，算法会重启，无需恢复
            vdt_hdl->keep_state = AUDIO_VDT_KEEP_STATE_PREPARE;
            vdt_hdl->trigger.switch_flag = 1;
            log_info("VDT_KEEP_STATE:PREPARE");
            anc_mode_switch_base(ANC_TRANSPARENCY, 0);
        }

        /*如果在播歌暂停播歌*/
        audio_vdt_a2dp_pause(1);

#if SPEAK_TO_CHAT_PLAY_TONE_EN
        /*结束免摘后检测播放提示音*/
        if (tone) {
            icsd_adt_tone_play(ICSD_ADT_TONE_NORMAL);
        }
#endif /*SPEAK_TO_CHAT_PLAY_TONE_EN*/

        if (vdt_hdl->timer) {
            sys_s_hi_timeout_modify(vdt_hdl->timer, vdt_hdl->end_time);
        } else {
            vdt_hdl->timer = sys_s_hi_timerout_add(NULL, audio_speak_to_chat_resume_timer, vdt_hdl->end_time);
        }

        break;

    case AUDIO_VDT_STATE_TRIGGER:
        log_info("VDT_STATE:REPEATED TRIGGER");
#if SPEAK_TO_CHAT_TEST_TONE_EN
        /*每次检测到说话都播放提示音*/
        icsd_adt_tone_play(ICSD_ADT_TONE_NORMAL);
#endif /*SPEAK_TO_CHAT_TEST_TONE_EN*/

        /*重新定时*/
        if (vdt_hdl->timer) {
            sys_s_hi_timeout_modify(vdt_hdl->timer, vdt_hdl->end_time);
        } else {
            vdt_hdl->timer = sys_s_hi_timerout_add(NULL, audio_speak_to_chat_resume_timer, vdt_hdl->end_time);
        }
        break;
    default:
        break;
    }
    return 0;
}

static void audio_vdt_resume_timer_del(void)
{
    /* log_debug("%s, %d, state %d\n", __func__, __LINE__, vdt_hdl->state); */
    if (vdt_hdl->timer) {
        sys_s_hi_timeout_del(vdt_hdl->timer);
        vdt_hdl->timer = 0;
    }
}

void audio_vdt_resume_send_to_adt(void)
{
    if (icsd_adt_is_running()) {
        log_debug("%s", __func__);
        int err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_VDT_RESUME, (int)vdt_hdl);
        if (err != OS_NO_ERR) {
            log_error("%s err %d", __func__, err);
        }
    }
}

/*char 定时结束后从通透同步恢复anc on /anc off*/
//1、a2dp player 点击播歌,挂起免摘，恢复到ANC ON/OFF
void audio_vdt_resume_process(void)
{
    //in speak_no_chat task
    log_debug("%s, state:%d\n", __func__, vdt_hdl->state);
    int err = 0;
    u8 user_anc_mode;
    if (icsd_adt_is_running()) {
        if (vdt_hdl->state != AUDIO_VDT_STATE_TRIGGER) {
            return;	//重入判断
        }
        user_anc_mode = anc_user_mode_get();
        log_info("%s : anc_mode = %d", __func__, user_anc_mode);
        /*tws同步后，删除定时器*/
        audio_vdt_resume_timer_del();
        vdt_hdl->state = AUDIO_VDT_STATE_RESUME;
        log_info("VDT_STATE:RESUME");
        audio_icsd_adt_algom_suspend();

#if SPEAK_TO_CHAT_PLAY_TONE_EN
        /*免摘结束退出通透提示音*/
        err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_TONE_PLAY, (int)ICSD_ADT_TONE_NUM0);
#endif /*SPEAK_TO_CHAT_PLAY_TONE_EN*/

        if (user_anc_mode == anc_mode_get()) {
            audio_icsd_adt_algom_resume(user_anc_mode);
            vdt_hdl->state = AUDIO_VDT_STATE_OPEN;
            log_info("VDT_STATE:RESUME->OPEN");
        } else {
            vdt_hdl->keep_state = AUDIO_VDT_KEEP_STATE_PREPARE;
            log_info("VDT_KEPP_STATE:PREPARE");
            anc_mode_switch_base(user_anc_mode, 0);
        }
        vdt_hdl->trigger.switch_flag = 0;
        audio_vdt_a2dp_pause(0);
    }
}

u8 audio_vdt_state_get(void)
{
    log_debug("%s, state:%d\n", __func__, vdt_hdl->state);
    return vdt_hdl->state;
}

u8 audio_vdt_keep_state_set(u8 state)
{
    //in speak_no_chat task
    vdt_hdl->keep_state = state;
    log_info("VDT_KEPP_STATE:SET = %d\n", state);
    return 0;
}

u8 audio_vdt_keep_state_get(void)
{
    return vdt_hdl->keep_state;
}

u8 audio_vdt_keep_state_send_to_adt(u8 state)
{
    log_debug("%s, %d, state:%d\n", __func__, state);
    int err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_VDT_KEEP_STATE, state);
    if (err != OS_NO_ERR) {
        log_error("%s err %d", __func__, err);
    }
    return 0;
}

u8 audio_vdt_tws_trigger_set(u8 en)
{
    //in speak_no_chat task
    vdt_hdl->tws_trigger = en;
    log_info("TWS_TRIGGER: SET = %d\n", en);
    return 0;
}


static void audio_vdt_ioc_init(void)
{
    //in speak_no_chat task
    if (vdt_hdl->keep_state == AUDIO_VDT_KEEP_STATE_START) {
        log_info("%s, keep:%d, state:%d\n", __func__, vdt_hdl->keep_state, vdt_hdl->state);
        //过渡状态执行完成, ANC_MSG_RUN 内部清除keep_state状态
        if (vdt_hdl->state == AUDIO_VDT_STATE_RESUME) {
            vdt_hdl->state = AUDIO_VDT_STATE_OPEN;
            log_info("VDT_STATE:RESUME->OPEN");
        }
        return;
    }
    if (vdt_hdl->state != AUDIO_VDT_STATE_CLOSE) {
        return;
    }
    vdt_hdl->state = AUDIO_VDT_STATE_OPEN;
    if (vdt_hdl->tws_trigger) {
        vdt_hdl->tws_trigger = 0;
        log_info("VDT_STATE:TWS_TRIGGER");
        audio_vdt_trigger_process(0);
    }
    log_info("VDT_STATE:OPEN");
}

//免摘退出
static void audio_vdt_ioc_exit(void)
{
    //in speak_no_chat task
    if (vdt_hdl->keep_state == AUDIO_VDT_KEEP_STATE_START) {
        log_info("%s, keep:%d, state:%d\n", __func__, vdt_hdl->keep_state, vdt_hdl->state);
        return;
    }
    audio_vdt_resume_timer_del();

    if (vdt_hdl->trigger.switch_flag) {
        vdt_hdl->trigger.switch_flag = 0;
        log_info("exit: user_mode %d, mode %d", anc_user_mode_get(), anc_mode_get());
        //触发免摘过程中被关闭，恢复user_mode
        anc_mode_switch_base(anc_user_mode_get(), 0);
    }

    /*恢复播歌状态*/
    audio_vdt_a2dp_pause(0);
    vdt_hdl->keep_state = 0;
    vdt_hdl->trigger.next_period = 0;
    vdt_hdl->state = AUDIO_VDT_STATE_CLOSE;
    log_info("VDT_STATE:CLOSE");
}

static void audio_vdt_ioc_suspend(void)
{
    //恢复免摘的状态
    if (vdt_hdl->state == AUDIO_VDT_STATE_TRIGGER) {
        audio_vdt_resume_send_to_adt();
    }
}

int audio_adt_vdt_ioctl(int cmd, int arg)
{
    switch (cmd) {
    case ICSD_ADT_IOC_INIT:
        audio_vdt_ioc_init();
        break;
    case ICSD_ADT_IOC_EXIT:
        audio_vdt_ioc_exit();
        break;
    case ICSD_ADT_IOC_SUSPEND:
        audio_vdt_ioc_suspend();
        break;
    case ICSD_ADT_IOC_RESUME:
        break;
    }
    return 0;
}


#endif /*(defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
