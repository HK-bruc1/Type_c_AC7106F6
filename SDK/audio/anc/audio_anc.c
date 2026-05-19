/*
 ****************************************************************
 *							AUDIO ANC
 * File  : audio_anc.c
 * By    :
 * Notes : ref_mic = 参考mic
 *		   err_mic = 误差mic
 *
 ****************************************************************
 */
#include "system/includes.h"
#include "audio_anc_includes.h"
#include "system/task.h"
#include "timer.h"
#include "online_db_deal.h"
#include "app_config.h"
#include "app_tone.h"
#include "audio_adc.h"
#include "asm/audio_common.h"
#include "btstack/avctp_user.h"
#include "app_main.h"
#include "audio_manager.h"
#include "audio_config.h"
#include "esco_player.h"
#include "a2dp_player.h"
#include "adc_file.h"
#include "clock.h"
#include "jlstream.h"
#include "dac_node.h"
#include "clock_manager/clock_manager.h"
#include "mic_effect.h"

#if RCSP_ADV_EN
#include "adv_anc_voice.h"
#endif
#if ANC_EAR_ADAPTIVE_EN
#include "adv_adaptive_noise_reduction.h"
#endif/*ANC_EAR_ADAPTIVE_EN*/

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/*TCFG_USER_TWS_ENABLE*/
#if THIRD_PARTY_PROTOCOLS_SEL&LL_SYNC_EN
#include "ble_iot_anc_manager.h"
#endif

#ifdef SUPPORT_MS_EXTENSIONS
#pragma   bss_seg(".anc_user.data.bss")
#pragma  data_seg(".anc_user.data")
#pragma const_seg(".anc_user.text.const")
#pragma  code_seg(".anc_user.text")
#endif/*SUPPORT_MS_EXTENSIONS*/

#define AT_VOLATILE_RAM_CODE_ANC        AT(.anc_user.text.cache.L1)

#if INEAR_ANC_UI
extern u8 inear_tws_ancmode;
#endif/*INEAR_ANC_UI*/

#if TCFG_AUDIO_ANC_ENABLE

#if 1
#define user_anc_log	printf
#else
#define user_anc_log(...)
#endif/*log_en*/

#if TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE
const u8 CONST_ANC_BASE_DEBUG_EN = 1;
#else
const u8 CONST_ANC_BASE_DEBUG_EN = 0;
#endif

#if ANC_EAR_ADAPTIVE_EN
const u8 CONST_ANC_EAR_ADAPTIVE_EN = 1;
#else
const u8 CONST_ANC_EAR_ADAPTIVE_EN = 0;
#endif/*ANC_EAR_ADAPTIVE_EN*/

#if TCFG_ANC_SELF_DUT_GET_SZ
const u8 CONST_ANC_DUT_SZ_EN = 1;
#else
const u8 CONST_ANC_DUT_SZ_EN = 0;
#endif/*TCFG_ANC_SELF_DUT_GET_SZ*/

#define TWS_ANC_SYNC_TIMEOUT	400 //ms

static void anc_fade_in_timeout(void *arg);
static void anc_timer_deal(void *priv);
static anc_coeff_t *anc_cfg_test(u8 coeff_en, u8 gains_en);
static void anc_fade_in_timer_add(audio_anc_t *param);
static int audio_anc_adt_prepare(u8 mode);
static int audio_anc_adt_reset(u8 mode);
static void anc_tone_play_and_mode_switch(u8 mode, u8 preemption, u8 cb_sel);
void anc_mode_enable_set(u8 mode_enable);
void audio_anc_post_msg_drc(void);
void audio_anc_post_msg_debug(void);
void audio_anc_post_msg_user_train_run(void);
void audio_anc_mic_management(audio_anc_t *param);

static void audio_anc_sz_fft_dac_check_slience_cb(void *buf, int len);
void audio_anc_post_msg_sz_fft_run(void);

void audio_anc_howldet_fade_set(u16 gain);
static void audio_anc_stereo_mix_ctr(void);

static void audio_anc_switch_rtanc_resume_timeout(void *priv);
static void anc_mode_switch_sem_post();

#define ESCO_MIC_DUMP_CNT	10
extern void esco_mic_dump_set(u16 dump_cnt);
extern void audio_cvp_v3_set_anc_mode(u8 anc_mode);

typedef struct {
    u8 mode_num;					/*模式总数*/
    u8 mode_enable;					/*使能的模式*/
    u8 anc_parse_seq;
    u8 suspend;						/*挂起*/
    u8 ui_mode_sel; 				/*ui菜单降噪模式选择*/
    u8 new_mode;					/*记录模式切换最新的目标模式*/
    u8 last_mode;					/*记录模式切换的上一个模式*/
    u8 tone_mode;					/*记录当前提示音播放的模式*/
    u8 user_mode;					/*用户层模式*/
    u8 mic_dy_state;        		/*动态MIC增益状态*/
    char mic_diff_gain;				/*动态MIC增益差值*/
#if ANC_MULT_ORDER_ENABLE
    u16 scene_id;					/*多场景滤波器-场景ID*/
    u8 scene_max;					/*多场景滤波器-最大场景个数*/
#endif/*ANC_MULT_ORDER_ENABLE*/
    u8 howldet_suspend_rtanc;		/*啸叫检测挂起RTANC的标识*/
    u16 ui_mode_sel_timer;
    u16 fade_in_timer;
    u16 fade_gain;
    u16 mic_resume_timer;			/*动态MIC增益恢复定时器id*/
    u8 switch_latch_flag;           /*切模式 锁存标志*/
    u8 switch_latch_mode;           /*切模式 锁存模式*/
    u8 adt_open;
    u8 stereo_to_mono_mix;			//ANC 双->单声道控制
#if AUDIO_ANC_PDM_MIC_ENABLE
    u8 pdm_mic_ch;
#endif
    float drc_ratio;				/*动态MIC增益，对应DRC增益比例*/
    volatile u8 state;				/*ANC状态*/
    volatile u8 mode_switch_lock;	/*切模式锁存，1表示正在切ANC模式*/
    volatile u8 sync_busy;
    audio_anc_t param;				/*ANC lib句柄*/
    audio_adc_mic_mana_t dy_mic_param[4];	/*MIC动态增益参数管理*/
    OS_SEM *sem;
#if ANC_EAR_ADAPTIVE_EN
    u8 ear_adaptive;				/*耳道自适应-模式标志*/
#endif/*ANC_EAR_ADAPTIVE_EN*/

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    u16 switch_rtanc_resume_id;		/*模式切换 RTANC恢复定时器*/

    u16 tone_adaptive_sync_id;		/*入耳自适应 RTANC恢复定时器*/
    u8 tone_adaptive_suspend;		/*入耳自适应 挂起ANC标志*/
    u8 tone_adaptive_device;		/*入耳自适应 挂起设备*/
#endif
} anc_t;
static anc_t *anc_hdl = NULL;
extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;

__attribute__((weak))
void audio_anc_event_to_user(int mode)
{

}
u32 get_anc_gains_sign()
{
    if (anc_hdl) {
        return anc_hdl->param.gains.gain_sign;
    } else {
        return 0;
    }
}

audio_anc_t *audio_anc_param_get(void)
{
    if (anc_hdl) {
        return &anc_hdl->param;
    } else {
        return NULL;
    }
}

void *get_anc_lfb_coeff()
{
    if (anc_hdl) {
        return anc_hdl->param.lfb_coeff;
    } else {
        return NULL;
    }
}

float get_anc_gains_l_fbgain()
{
    if (anc_hdl) {
        return anc_hdl->param.gains.l_fbgain;
    } else {
        return 0;
    }
}

u8 get_anc_l_fbyorder()
{
    if (anc_hdl) {
        return anc_hdl->param.lfb_yorder;
    } else {
        return 0;
    }
}

void *get_anc_rfb_coeff()
{
    if (anc_hdl) {
        return anc_hdl->param.rfb_coeff;
    } else {
        return NULL;
    }
}

float get_anc_gains_r_fbgain()
{
    if (anc_hdl) {
        return anc_hdl->param.gains.r_fbgain;
    } else {
        return 0;
    }
}

u8 get_anc_r_fbyorder()
{
    if (anc_hdl) {
        return anc_hdl->param.rfb_yorder;
    } else {
        return 0;
    }
}

void *get_anc_lff_coeff()
{
    if (anc_hdl) {
        printf("anc hdl lff coeff:%x\n", (int)anc_hdl->param.lff_coeff);
        return anc_hdl->param.lff_coeff;
    } else {
        printf("anc hdl NULL~~~~~~~~\n");
        return NULL;
    }
}

float get_anc_gains_l_ffgain()
{
    if (anc_hdl) {
        return anc_hdl->param.gains.l_ffgain;
    } else {
        return 0;
    }
}

u8 get_anc_l_ffyorder()
{
    if (anc_hdl) {
        return anc_hdl->param.lff_yorder;
    } else {
        return 0;
    }
}

void *get_anc_ltrans_coeff()
{
    if (anc_hdl) {
        return anc_hdl->param.ltrans_coeff;
    } else {
        return NULL;
    }
}

float get_anc_gains_l_transgain()
{
    if (anc_hdl) {
        return anc_hdl->param.gains.l_transgain;
    } else {
        return 0;
    }
}

u8 get_anc_l_transyorder()
{
    if (anc_hdl) {
        return anc_hdl->param.ltrans_yorder;
    } else {
        return 0;
    }
}

void *get_anc_ltrans_fb_coeff()
{
    if (anc_hdl) {
        return anc_hdl->param.ltrans_fb_coeff;
    } else {
        return NULL;
    }
}

float get_anc_gains_lfb_transgain()
{
    if (anc_hdl) {
        return anc_hdl->param.ltrans_fbgain;
    } else {
        return 0;
    }
}

u8 get_anc_lfb_transyorder()
{
    if (anc_hdl) {
        return anc_hdl->param.ltrans_fb_yorder;
    } else {
        return 0;
    }
}

static void anc_task(void *p)
{
    int res;
    int anc_ret = 0;
    int msg[16];
    u8 cur_anc_mode;
    user_anc_log(">>>ANC TASK<<<\n");
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            switch (msg[1]) {
#if TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE
            case ANC_MSG_TRAIN_OPEN:/*启动训练模式*/
                audio_mic_pwr_ctl(MIC_PWR_ON);
#if AUDIO_ANC_PDM_MIC_ENABLE
                audio_anc_pdm_mic_start(anc_hdl->pdm_mic_ch);
#endif
                user_anc_log("ANC_MSG_TRAIN_OPEN");
                audio_anc_dma_sel_map(&anc_hdl->param, &anc_hdl->param.debug_sel);
                audio_anc_train(&anc_hdl->param, 1);
                os_time_dly(1);
                anc_train_close();
                audio_mic_pwr_ctl(MIC_PWR_OFF);
                break;
            case ANC_MSG_TRAIN_CLOSE:/*训练关闭模式*/
                user_anc_log("ANC_MSG_TRAIN_CLOSE");
                anc_train_close();
                break;
#endif
            case ANC_MSG_RUN:
                anc_hdl->param.mode = msg[2];

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
                //ADT 预处理，ANC_OFF(ADT)会替换成ANC_EXT模式
                audio_anc_adt_prepare(msg[2]);
#endif
                cur_anc_mode = anc_hdl->param.mode;

                //检查通话和ANC MIC增益是否一致，不一致则报错
                audio_anc_mic_gain_check(0);
#if AUDIO_ANC_MIC_ARRAY_ENABLE
                audio_anc_stereo_mix_ctr();
#endif

                if (cur_anc_mode == ANC_EXT) {
                    audio_anc_fade_ctr_set(ANC_FADE_MODE_EXT, AUDIO_ANC_FDAE_CH_ALL, 0);
                } else {
                    audio_anc_fade_ctr_set(ANC_FADE_MODE_EXT, AUDIO_ANC_FDAE_CH_ALL, AUDIO_ANC_FADE_GAIN_DEFAULT);
                }

                audio_anc_event_to_user(cur_anc_mode);

                if (cur_anc_mode != ANC_OFF) {
#if ANC_MULT_ORDER_ENABLE
#if ANC_EAR_ADAPTIVE_EN
                    if ((anc_hdl->param.anc_coeff_mode == ANC_COEFF_MODE_ADAPTIVE) && (cur_anc_mode == ANC_ON)) {
                        audio_anc_coeff_adaptive_set(ANC_COEFF_MODE_ADAPTIVE, 0);
                    } else {
                        anc_mult_scene_set(anc_hdl->scene_id);
                    }
                    audio_anc_coeff_check_crc(ANC_CHECK_SWITCH_DEF);
#else
                    anc_mult_scene_set(anc_hdl->scene_id);
#endif/*ANC_EAR_ADAPTIVE_EN*/
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
                    audio_rtanc_coeff_set();
#endif
#elif TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
                    audio_anc_db_cfg_read();
                    audio_rtanc_coeff_set();
#endif/*ANC_MULT_ORDER_ENABLE*/
                }

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	(!(ANC_MULT_ORDER_ENABLE && ANC_MULT_TRANS_FB_ENABLE))
                //多滤波器 通透+FB 没有开启时，ADT 通透模式则使用固定FB参数, CMP参数复用ANC
                audio_icsd_adt_trans_fb_param_set(&anc_hdl->param);
#endif

                user_anc_log("ANC_MSG_RUN:%s \n", anc_mode_str[cur_anc_mode]);
#if (RCSP_ADV_EN && RCSP_ADV_ANC_VOICE)
                /*APP增益设置*/
                anc_hdl->param.anc_fade_gain = rcsp_adv_anc_voice_value_get(cur_anc_mode);
#endif/*RCSP_ADV_EN && RCSP_ADV_ANC_VOICE*/
#if THIRD_PARTY_PROTOCOLS_SEL&LL_SYNC_EN
                /*APP增益设置*/
                anc_hdl->param.anc_fade_gain = iot_anc_effect_value_get(cur_anc_mode);
#endif

#if ANC_HOWLING_DETECT_EN
                //复位啸叫检测相关变量
                anc_howling_detect_reset();
#endif

#if (TCFG_AUDIO_ADC_MIC_CHA == LADC_CH_PLNK)
                /* esco_mic_dump_set(ESCO_MIC_DUMP_CNT); */
#endif/*LADC_CH_PLNK*/
                if (anc_hdl->state == ANC_STA_INIT) {
                    audio_mic_pwr_ctl(MIC_PWR_ON);
#if AUDIO_ANC_PDM_MIC_ENABLE
                    audio_anc_pdm_mic_start(anc_hdl->pdm_mic_ch);
#endif
#if ANC_MODE_SYSVDD_EN
                    clock_set_lowest_voltage(SYSVDD_VOL_SEL_105V);	//进入ANC时提高SYSVDD电压
#endif/*ANC_MODE_SYSVDD_EN*/
                    anc_ret = audio_anc_run(&anc_hdl->param);
                    if (anc_ret == 0) {
                        anc_hdl->state = ANC_STA_OPEN;
                    } else {
                        /*
                         *-EPERM(-1):不支持ANC(非差分MIC，或混合馈使用4M的flash)
                         *-EINVAL(-22):参数错误(anc_coeff.bin参数为空, 或不匹配)
                         */
                        user_anc_log("audio_anc open Failed:%d\n", anc_ret);
                        anc_hdl->mode_switch_lock = 0;
                        anc_hdl->state = ANC_STA_INIT;
                        anc_hdl->param.mode = ANC_OFF;
                        anc_hdl->new_mode = anc_hdl->param.mode;
                        audio_mic_pwr_ctl(MIC_PWR_OFF);
                        anc_mode_switch_sem_post();
                        break;
                    }
                } else {
                    audio_anc_run(&anc_hdl->param);
                }
                if (cur_anc_mode == ANC_OFF) {
                    anc_hdl->state = ANC_STA_INIT;
#if AUDIO_ANC_PDM_MIC_ENABLE
                    audio_anc_pdm_mic_stop();
#endif
                    audio_mic_pwr_ctl(MIC_PWR_OFF);
#if ANC_MODE_SYSVDD_EN
                    clock_set_lowest_voltage(SYSVDD_VOL_SEL_084V);	//退出ANC恢复普通模式
#endif/*ANC_MODE_SYSVDD_EN*/
                    /*anc关闭，如果没有连接蓝牙，倒计时进入自动关机*/
                    /* extern u8 bt_get_total_connect_dev(void); */
                    /* if (bt_get_total_connect_dev() == 0) {    //已经没有设备连接 */
                    /* sys_auto_shut_down_enable(); */
                    /* } */
                }

#if ANC_MUSIC_DYNAMIC_GAIN_EN
                audio_anc_music_dynamic_gain_reset();	/*音乐动态增益，切模式复位状态*/
#if ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB
                if (anc_hdl->param.mode == ANC_ON) {
                    //ANC初始化会复位FB GAIN，导致动态修改无效，在ANCmute前重新赋值
                    audio_anc_music_dynamic_fb_gain_reset();
                }
#endif
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/
                if (anc_hdl->param.anc_fade_en) {
                    if (anc_hdl->param.mode == ANC_EXT) {   //ANC_EXT没有增益输出，不需要太久的fade
                        os_time_dly(60 / 10);
                    } else if ((anc_hdl->param.mode != ANC_OFF) && (anc_hdl->last_mode == ANC_OFF)) {
                        //若上一个模式为ANC_EXT，已经开过ADC, 不需要400ms 延时等待ADC稳定
                        os_time_dly(ANC_MODE_SWITCH_DELAY_MS / 10); //延时避免切模式反馈有哒哒声
                    }
                }
#if ANC_ADAPTIVE_EN
                audio_anc_power_adaptive_reset();
#endif/*ANC_ADAPTIVE_EN*/
                anc_fade_in_timer_add(&anc_hdl->param);

#if ANC_EAR_ADAPTIVE_EN
                if (anc_ear_adaptive_busy_get()) {
                    anc_ear_adaptive_mode_end();	//耳道自适应训练结束
                }
#endif

#if ANC_MULT_ORDER_ENABLE
                audio_anc_mult_scene_coeff_free();
#endif/*ANC_MULT_ORDER_ENABLE*/

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
                audio_anc_adt_reset(cur_anc_mode); //模式切换后，ADT复位
#endif

#if TCFG_AUDIO_ANC_DCC_TRIM_ENABLE
                if (anc_hdl->param.dcc_trim.mode == ANC_DCC_TRIM_START) {
                    audio_anc_dcc_trim_process();
                }
#endif
#if TCFG_AUDIO_CVP_V3_MODE
                audio_cvp_v3_set_anc_mode(cur_anc_mode);
#endif
                anc_hdl->last_mode = cur_anc_mode;
                anc_hdl->mode_switch_lock = 0;
                anc_mode_switch_sem_post();
                //1、当前模式处于ANC_EXT，但用户模式非ANC_OFF，非AFQ状态，切换用户的模式
                //2、存在切模式锁存，切换锁存的模式

#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
                if (((cur_anc_mode == ANC_EXT) && (anc_hdl->user_mode != ANC_OFF) && (!audio_icsd_afq_is_running())) ||
                    anc_hdl->switch_latch_flag) {
#else
                if (((cur_anc_mode == ANC_EXT) && (anc_hdl->user_mode != ANC_OFF)) ||
                    anc_hdl->switch_latch_flag) {
#endif
                    anc_hdl->switch_latch_flag = 0;
                    user_anc_log("anc_switch_latch: trigger %d, cur_mode %d\n", anc_hdl->user_mode, cur_anc_mode);
                    anc_mode_switch(anc_hdl->user_mode, 0);
                }
                break;
            case ANC_MSG_MODE_SYNC:
                user_anc_log("anc_mode_sync:%d", msg[2]);
                anc_mode_switch_base(msg[2], 0);
                break;
#if TCFG_USER_TWS_ENABLE
            case ANC_MSG_TONE_SYNC:
                user_anc_log("anc_tone_sync_play:%d", msg[2]);
#if !ANC_MODE_EN_MODE_NEXT_SW
                if (anc_hdl->mode_switch_lock) {
                    user_anc_log("anc mode switch lock\n");
                    break;
                }
                anc_hdl->mode_switch_lock = 1;
#endif/*ANC_MODE_EN_MODE_NEXT_SW*/
                if (msg[2] == SYNC_TONE_ANC_OFF) {
                    anc_hdl->new_mode = ANC_OFF;
                } else if (msg[2] == SYNC_TONE_ANC_ON) {
                    anc_hdl->new_mode = ANC_ON;
                } else {
                    anc_hdl->new_mode = ANC_TRANSPARENCY;
                }
                if (anc_hdl->suspend) {
                    anc_hdl->param.tool_enablebit = 0;
                }
                anc_tone_play_and_mode_switch(anc_hdl->new_mode, ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
                //anc_mode_switch_deal(anc_hdl->new_mode);
                anc_hdl->sync_busy = 0;
                break;
#endif/*TCFG_USER_TWS_ENABLE*/
            case ANC_MSG_FADE_END:
                break;
            case ANC_MSG_DEBUG_OUTPUT:
                audio_anc_debug_data_output();
                break;
#if ANC_ADAPTIVE_EN
            case ANC_MSG_ADAPTIVE_SYNC:
                audio_anc_power_adaptive_tone_play((u8)msg[2], (u8)msg[3]);
                break;
#endif/*ANC_ADAPTIVE_EN*/

#if ANC_MUSIC_DYNAMIC_GAIN_EN
            case ANC_MSG_MUSIC_DYN_GAIN:
                audio_anc_music_dynamic_gain_process();
                break;
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/
#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
            case ANC_MSG_AFQ_CMD:
                icsd_afq_anctask_handler(&anc_hdl->param, msg);
                break;
#endif/*TCFG_AUDIO_FREQUENCY_GET_ENABLE*/
#if ANC_EAR_ADAPTIVE_EN
            case ANC_MSG_ICSD_ANC_V2_CMD:
            case ANC_MSG_ICSD_ANC_V2_INIT:
            case ANC_MSG_ICSD_ANC_CMD:
            case ANC_MSG_USER_TRAIN_INIT:
            case ANC_MSG_USER_TRAIN_RUN:
            case ANC_MSG_USER_TRAIN_SETPARAM:
            case ANC_MSG_USER_TRAIN_TIMEOUT:
            case ANC_MSG_USER_TRAIN_END:
                anc_ear_adaptive_cmd_handler(&anc_hdl->param, msg);
                break;
#endif/*ANC_EAR_ADAPTIVE_EN*/

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
            case ANC_MSG_RT_ANC_CMD:
                extern void rt_anc_anctask_cmd_handle(void *param, u8 cmd);
                rt_anc_anctask_cmd_handle((void *)&anc_hdl->param, (u8)msg[2]);
                break;
            case ANC_MSG_RTANC_SZ_OUTPUT:
#if RTANC_SZ_SEL_FF_OUTPUT
                audio_rtanc_sz_select_process();
#endif
                break;
#endif
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
            case ANC_MSG_46KOUT_DEMO:
                extern void icsd_anc_46kout_demo();
                icsd_anc_46kout_demo();
                break;
#endif
            case ANC_MSG_MIC_DATA_GET:
                /* extern void anc_spp_mic_data_get(audio_anc_t *param); */
                /* anc_spp_mic_data_get(&anc_hdl->param); */
                break;
#if AUDIO_ANC_COEFF_SMOOTH_ENABLE
            case ANC_MSG_COEFF_UPDATE:
                audio_anc_coeff_check_crc(ANC_CHECK_UPDATE);
                audio_anc_event_notify(ANC_EVENT_FILTER_UPDATE_BEFORE, 0);
                //更新ANC滤波器
                if (msg[2] == ANC_COEFF_TYPE_FF) {
                    anc_coeff_ff_online_update(&anc_hdl->param);
                } else if (msg[2] == ANC_COEFF_TYPE_FB) {
                    anc_coeff_fb_online_update(&anc_hdl->param);
                } else {
                    anc_coeff_online_update(&anc_hdl->param, 0);			//更新ANC滤波器
                }
                audio_anc_event_notify(ANC_EVENT_FILTER_UPDATE_AFTER, 0);
                break;
#endif
            case ANC_MSG_RESET:
                audio_anc_event_notify(ANC_EVENT_RESET_BEFORE, 0);
                if (msg[2] && anc_hdl->param.anc_fade_en) {
                    int alogm = audio_anc_gains_alogm_get((anc_hdl->param.mode == ANC_TRANSPARENCY) ? ANC_TRANS_TYPE : ANC_FF_TYPE);
                    int dly = audio_anc_fade_dly_get(anc_hdl->param.anc_fade_gain, alogm);
                    audio_anc_fade_ctr_set(ANC_FADE_MODE_RESET, AUDIO_ANC_FDAE_CH_ALL, 0);
                    os_time_dly(dly / 10 + 1);
                    audio_anc_reset(&anc_hdl->param, 1);
                    //避免param.anc_fade_gain 被修改，这里使用固定值
                    audio_anc_fade_ctr_set(ANC_FADE_MODE_RESET, AUDIO_ANC_FDAE_CH_ALL, AUDIO_ANC_FADE_GAIN_DEFAULT);
                } else {
                    audio_anc_reset(&anc_hdl->param, 0);
                }
                audio_anc_event_notify(ANC_EVENT_RESET_AFTER, 0);
                break;
#if TCFG_ANC_SELF_DUT_GET_SZ
            case ANC_MSG_SZ_FFT_OPEN:
                clock_refurbish();	//模块时钟消耗96M
                audio_mic_pwr_ctl(MIC_PWR_ON);
                anc_hdl->param.mode = ANC_TRAIN;
                anc_hdl->new_mode = anc_hdl->param.mode;
                anc_hdl->param.test_type = ANC_TEST_TYPE_FFT;	//设置当前的测试类型
                //设置当前测试模式采样率 BR50 FB只支持ANC_ALOGM5~7
                anc_hdl->param.sz_fft.alogm = ANC_ALOGM3;
                anc_hdl->param.sz_fft.data_sel = msg[2];		//传入目标数据通道
                anc_hdl->param.sz_fft.check_flag = 1;			//检查DAC有效数据段
                audio_anc_dma_sel_map(&anc_hdl->param, &anc_hdl->param.sz_fft.data_sel);
                audio_anc_train(&anc_hdl->param, 1);			//打开训练模式
                dac_node_write_callback_add("ANC_SZ_FFT", 0XFF, audio_anc_sz_fft_dac_check_slience_cb);	//注册DAC回调接口-静音检测
                break;
            case ANC_MSG_SZ_FFT_RUN:
                audio_anc_sz_fft_run(&anc_hdl->param);			//SZ_FFT 频响计算主体
                break;
            case ANC_MSG_SZ_FFT_CLOSE:
                audio_anc_sz_fft_stop(&anc_hdl->param);			//SZ_FFT 停止计算
                anc_train_close();								//关闭ANC，模式设置为ANC_OFF
                break;
#endif/*TCFG_ANC_SELF_DUT_GET_SZ*/
            case ANC_MSG_MODE_SWITCH_IN_ANCTASK:
                user_anc_log("anc_mode_in_anctask:%d, %d", msg[2], msg[3]);
                anc_mode_switch(msg[2], msg[3]);
                break;
            case ANC_MSG_DRC_TIMER:
                audio_anc_platform_task_msg(msg);
                break;
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE
            case ANC_MSG_ENV_NOISE_LVL:
                audio_env_noise_event_process((u8)msg[2]);
                break;
#endif
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE
            case ANC_MSG_AVC_NOISE_LVL:
                audio_icsd_adptive_vol_event_process((u8)msg[2]);
                break;
#endif

            }
        } else {
            user_anc_log("res:%d,%d", res, msg[1]);
        }
    }
}

/*ANC配置参数填充*/
void anc_param_fill(u8 cmd, anc_gain_t *cfg)
{
    if (cmd == ANC_CFG_READ) {
        anc_gain_t *db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, &anc_hdl->param.gains_size);
        if (db_gain) {
            cfg->gains = db_gain->gains;	//直接读flash区域
        } else {
            cfg->gains = anc_hdl->param.gains;	//flash区域为空，则使用默认参数
        }
    } else if (cmd == ANC_CFG_WRITE) {
        anc_hdl->param.gains = cfg->gains;
        anc_hdl->param.gains.hd_en = ANC_HOWLING_DETECT_EN;
        anc_hdl->param.gains.hd_corr_thr = ANC_HOWLING_DETECT_CORR_THR;
        anc_hdl->param.gains.hd_corr_dly = 13;
        anc_hdl->param.gains.hd_corr_gain = ANC_HOWLING_TARGET_GAIN;
        anc_hdl->param.gains.hd_pwr_rate = 2;
        anc_hdl->param.gains.hd_pwr_ctl_gain_en = 0;
        anc_hdl->param.gains.hd_pwr_ctl_ahsrst_en = 0;
        anc_hdl->param.gains.hd_pwr_thr = ANC_HOWLING_DETECT_PWR_THR;
        anc_hdl->param.gains.hd_pwr_ctl_gain = 1638;

        anc_hdl->param.gains.hd_pwr_ref_ctl_en = 0;
        anc_hdl->param.gains.hd_pwr_ref_ctl_hthr = 2000;
        anc_hdl->param.gains.hd_pwr_ref_ctl_lthr1 = 1000;
        anc_hdl->param.gains.hd_pwr_ref_ctl_lthr2 = 200;

        anc_hdl->param.gains.hd_pwr_err_ctl_en = 0;
        anc_hdl->param.gains.hd_pwr_err_ctl_hthr = 2000;
        anc_hdl->param.gains.hd_pwr_err_ctl_lthr1 = 1000;
        anc_hdl->param.gains.hd_pwr_err_ctl_lthr2 = 200;
        anc_hdl->param.gains.developer_mode = ANC_DEVELOPER_MODE_EN;

        audio_anc_param_normalize(&anc_hdl->param);

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
        if (audio_anc_gains_alogm_get(ANC_TRANS_TYPE) != audio_anc_gains_alogm_get(ANC_FF_TYPE)) {
            user_anc_log("===========================\n\n");
            // anc_hdl->param.gains.trans_alogm = anc_hdl->param.gains.alogm;
            user_anc_log("err : anc_hdl->param.gains.trans_alogm != anc_hdl->param.gains.alogm !!!");
            user_anc_log("\n\n===========================");
        }
#endif

#if ANC_EAR_ADAPTIVE_EN
        //左右参数映射，测试使用
        audio_anc_param_map(0, 1);
        if (anc_hdl->param.anc_coeff_mode == ANC_COEFF_MODE_ADAPTIVE) {
            audio_anc_ear_adaptive_param_init(&anc_hdl->param);
        }

#endif/*ANC_EAR_ADAPTIVE_EN*/
    }
    /* audio_anc_platform_gains_printf(cmd); */
}

//ANC配置文件读取并更新
int audio_anc_db_cfg_read(void)
{
    /*读取ANC增益配置*/
    anc_gain_t *db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, &anc_hdl->param.gains_size);
    if (db_gain) {
        user_anc_log("anc_gain_db get succ,len:%d\n", anc_hdl->param.gains_size);
        if (audio_anc_gains_version_verify(&anc_hdl->param, db_gain)) {	//检查版本差异
            user_anc_log("The anc gains version is older, new = 0X%x\n", db_gain->gains.version);
            db_gain = (anc_gain_t *)anc_db_get(ANC_DB_GAIN, &anc_hdl->param.gains_size);
        }
        anc_param_fill(ANC_CFG_WRITE, db_gain);
    } else {
        user_anc_log("anc_gain_db get failed");
        /* audio_anc_platform_gains_printf(ANC_CFG_READ); */
        return 1;
    }
    /*读取ANC滤波器系数*/
#if ANC_MULT_ORDER_ENABLE
    return audio_anc_mult_coeff_file_read();
#endif/*ANC_MULT_ORDER_ENABLE*/

    anc_coeff_t *db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
    if (!anc_coeff_fill(db_coeff)) {
        user_anc_log("anc_coeff_db get succ,len:%d", anc_hdl->param.coeff_size);
    } else {
        user_anc_log("anc_coeff_db get failed");
        return 1;
#if 0	//测试数据写入是否正常，或写入自己想要的滤波器数据
        extern anc_coeff_t *anc_cfg_test(audio_anc_t *param, u8 coeff_en, u8 gains_en);
        anc_coeff_t *test_coeff = anc_cfg_test(&anc_hdl->param, 1, 1);
        if (test_coeff) {
            int ret = anc_db_put(&anc_hdl->param, NULL, test_coeff);
            if (ret == 0) {
                user_anc_log("anc_db put test succ\n");
            } else {
                user_anc_log("anc_db put test err\n");
            }
        }
        anc_coeff_t *test_coeff1 = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
        anc_coeff_fill(test_coeff1);
#endif
    }
#if ANC_EAR_ADAPTIVE_EN
    //左右参数映射，测试使用
    audio_anc_param_map(1, 0);
#endif/*ANC_EAR_ADAPTIVE_EN*/
    return 0;

}

/*ANC初始化*/
void anc_init(void)
{
    anc_hdl = anc_malloc("ANC_BASE", sizeof(anc_t));
    user_anc_log("anc_hdl size:%d\n", (int)sizeof(anc_t));
    ASSERT(anc_hdl);
    audio_anc_param_init(&anc_hdl->param);
    audio_anc_platform_param_init(&anc_hdl->param);
    anc_debug_init(&anc_hdl->param);
    audio_anc_fade_ctr_init();
#if AUDIO_ANC_DATA_EXPORT_VIA_UART
    audio_anc_develop_init();
#endif

#if (TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2)
    audio_afq_common_init();
#endif
#if ANC_EAR_ADAPTIVE_EN
    anc_ear_adaptive_init(&anc_hdl->param);
#endif/*ANC_EAR_ADAPTIVE_EN*/
#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
    audio_icsd_afq_init();
#endif/*TCFG_AUDIO_FREQUENCY_GET_ENABLE*/
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    audio_adaptive_eq_init();
#endif/*TCFG_AUDIO_ADAPTIVE_EQ_ENABLE*/
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    audio_speak_to_chat_init();
#endif
#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
    anc_ext_tool_init();
#endif
#if ANC_MULT_ORDER_ENABLE
    anc_mult_init(&anc_hdl->param);
    anc_hdl->scene_id = ANC_MULT_ORDER_NORMAL_ID;	//默认使用场景1滤波器
    anc_hdl->param.mult_gain_set = audio_anc_mult_gains_id_set;
#endif/*ANC_MULT_ORDER_ENABLE*/

#if ANC_MUSIC_DYNAMIC_GAIN_EN
    audio_anc_music_dynamic_gain_init();
#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    audio_anc_real_time_adaptive_init(&anc_hdl->param);
#endif
#if ANC_DUT_MIC_CMP_GAIN_ENABLE
    audio_anc_mic_gain_cmp_init(&anc_hdl->param.mic_cmp);
#endif
    anc_hdl->param.post_msg_drc = audio_anc_post_msg_drc;
    anc_hdl->param.post_msg_debug = audio_anc_post_msg_debug;
#if TCFG_ANC_MODE_ANC_EN
    anc_hdl->mode_enable |= ANC_ON_BIT;
#endif/*TCFG_ANC_MODE_ANC_EN*/
#if TCFG_ANC_MODE_TRANS_EN
    anc_hdl->mode_enable |= ANC_TRANS_BIT;
#endif/*TCFG_ANC_MODE_TRANS_EN*/
#if TCFG_ANC_MODE_OFF_EN
    anc_hdl->mode_enable |= ANC_OFF_BIT;
#endif/*TCFG_ANC_MODE_OFF_EN*/
    anc_mode_enable_set(anc_hdl->mode_enable);
    anc_hdl->param.cfg_online_deal_cb = anc_cfg_online_deal;
    anc_hdl->param.mode = ANC_OFF;
    anc_hdl->new_mode = ANC_OFF;
    anc_hdl->last_mode = ANC_OFF;
    anc_hdl->user_mode = ANC_OFF;
    anc_hdl->param.production_mode = 0;
    anc_hdl->param.developer_mode = ANC_DEVELOPER_MODE_EN;
    anc_hdl->param.anc_fade_en = ANC_FADE_EN;/*ANC淡入淡出，默认开*/
    anc_hdl->param.anc_fade_gain = 16384;/*ANC淡入淡出增益,16384 (0dB) max:32767*/
    anc_hdl->param.tool_enablebit = TCFG_AUDIO_ANC_TRAIN_MODE;
    anc_hdl->param.online_busy = 0;		//在线调试繁忙标志位
    //36
    anc_hdl->param.dut_audio_enablebit = ANC_DUT_CLOSE;
    anc_hdl->param.fade_time_lvl = ANC_MODE_FADE_LVL;
    anc_hdl->param.enablebit = TCFG_AUDIO_ANC_TRAIN_MODE;
    anc_hdl->param.ch = TCFG_AUDIO_ANC_CH;
    anc_hdl->param.mic_type[0] = (ANC_CONFIG_LFF_EN) ? TCFG_AUDIO_ANCL_FF_MIC : MIC_NULL;
    anc_hdl->param.mic_type[1] = (ANC_CONFIG_LFB_EN) ? TCFG_AUDIO_ANCL_FB_MIC : MIC_NULL;
    anc_hdl->param.mic_type[2] = (ANC_CONFIG_RFF_EN) ? TCFG_AUDIO_ANCR_FF_MIC : MIC_NULL;
    anc_hdl->param.mic_type[3] = (ANC_CONFIG_RFB_EN) ? TCFG_AUDIO_ANCR_FB_MIC : MIC_NULL;

#if AUDIO_ANC_MIC_ARRAY_ENABLE
    anc_hdl->stereo_to_mono_mix = 1;
    anc_hdl->param.lff_en = ANC_CONFIG_LFF_EN;
    anc_hdl->param.lfb_en = ANC_CONFIG_LFB_EN;
    anc_hdl->param.rff_en = (AUDIO_ANC_MIC_ARRAY_FF_NUM == 2) ? ANC_CONFIG_RFF_EN : 0;
    anc_hdl->param.rfb_en = (AUDIO_ANC_MIC_ARRAY_FB_NUM == 2) ? ANC_CONFIG_RFB_EN : 0;
#else
    anc_hdl->param.lff_en = ANC_CONFIG_LFF_EN;
    anc_hdl->param.lfb_en = ANC_CONFIG_LFB_EN;
    anc_hdl->param.rff_en = ANC_CONFIG_RFF_EN;
    anc_hdl->param.rfb_en = ANC_CONFIG_RFB_EN;
#endif
    anc_hdl->param.debug_sel = 0;

    anc_hdl->param.trans_fb_en = (ANC_MULT_TRANS_FB_ENABLE | TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN);

    //对相关控制变量做初始化
    anc_hdl->param.howling_detect_toggle = ANC_HOWLING_DETECT_EN;

    //初始化滤波器淡入步进
    anc_hdl->param.ff_filter_fade_fast = 0;
    anc_hdl->param.ff_filter_fade_slow = 3;
    anc_hdl->param.fb_filter_fade_fast = 0;
    anc_hdl->param.fb_filter_fade_slow = 3;

    anc_hdl->param.drc_toggle = 1;
    anc_hdl->param.dcc_adaptive_toggle = 1;
    anc_hdl->param.lr_lowpower_en = ANC_LR_LOWPOWER_EN;
#if ANC_COEFF_SAVE_ENABLE
    anc_db_init();
    audio_anc_db_cfg_read();
#endif/*ANC_COEFF_SAVE_ENABLE*/

#if TCFG_AUDIO_ANC_MULT_ORDER_ENABLE || TCFG_AUDIO_ANC_EXT_EN
    anc_hdl->param.biquad2ab = icsd_biquad2ab_out_v2;
#endif

#if ANC_EAR_ADAPTIVE_EN && (TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE == 0)
    //2.再读取ANC自适应参数, 若存在则覆盖默认参数, RTANC流程问题，不读取耳道参数
    audio_anc_adaptive_data_read();
#endif/*ANC_EAR_ADAPTIVE_EN*/

#if ANC_HOWLING_DETECT_EN
#if ANC_CHIP_VERSION == ANC_VERSION_BR56
    anc_hdl->param.howling_detect_ch = 0;	//br56固定写0
#else
    anc_hdl->param.howling_detect_ch = ANC_HOWLING_DETECT_CHANNEL;
#endif
    struct anc_howling_detect_cfg howling_detect_cfg;
    howling_detect_cfg.detect_time = ANC_HOWLING_DETECT_TIME;
    howling_detect_cfg.hold_time = ANC_HOWLING_HOLD_TIME;
    howling_detect_cfg.resume_time = ANC_HOWLING_RESUME_TIME;
    howling_detect_cfg.param = &anc_hdl->param;
    howling_detect_cfg.fade_gain_set = audio_anc_howldet_fade_set;
    anc_howling_detect_init(&howling_detect_cfg);
#endif

    /* sys_timer_add(NULL, anc_timer_deal, 5000);*/

    audio_anc_mic_management(&anc_hdl->param);

#if ANC_ADAPTIVE_EN
    audio_anc_power_adaptive_init(&anc_hdl->param);
#endif/*ANC_ADAPTIVE_EN*/

#if TCFG_ANC_SELF_DUT_GET_SZ
    anc_hdl->param.sz_fft.dma_run_hdl = audio_anc_post_msg_sz_fft_run;
#endif/*TCFG_ANC_SELF_DUT_GET_SZ*/

    audio_anc_max_yorder_verify(&anc_hdl->param);
    task_create(anc_task, NULL, "anc");
    anc_hdl->state = ANC_STA_INIT;

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    anc_adt_init();
    audio_icsd_adt_scene_set(ADT_SCENE_ANC_OFF, 1);	//默认为ANC_OFF场景
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/

    //初始化 ANC_EXT相关算法使能
    audio_anc_app_adt_mode_init(1);

    user_anc_log("anc_init ok");
}

#if TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE
void anc_train_open(u8 mode, u8 debug_sel)
{
    user_anc_log("ANC_Train_Open\n");
    local_irq_disable();
    if (anc_hdl && (anc_hdl->state == ANC_STA_INIT)) {
        /*防止重复打开训练模式*/
        if (anc_hdl->param.mode == ANC_TRAIN) {
            local_irq_enable();
            return;
        }
        /*anc工作，退出sniff*/
        bt_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
        /*anc工作，关闭自动关机*/
        /* sys_auto_shut_down_disable(); */

        anc_hdl->param.debug_sel = debug_sel;
        anc_hdl->param.mode = mode;
        anc_hdl->new_mode = anc_hdl->param.mode;
        anc_hdl->param.test_type = ANC_TEST_TYPE_PCM;
        /*训练的时候，申请buf用来保存训练参数*/
        local_irq_enable();
        os_taskq_post_msg("anc", 1, ANC_MSG_TRAIN_OPEN);
        user_anc_log("%s ok\n", __FUNCTION__);
        return;
    }
    local_irq_enable();
}

void anc_train_close(void)
{
    if (anc_hdl == NULL) {
        return;
    }
    if (anc_hdl->param.mode == ANC_TRAIN) {
        anc_hdl->param.train_para.train_busy = 0;
        anc_hdl->param.mode = ANC_OFF;
        anc_hdl->new_mode = anc_hdl->param.mode;
        anc_hdl->state = ANC_STA_INIT;
        audio_anc_train(&anc_hdl->param, 0);
        audio_mic_pwr_ctl(MIC_PWR_OFF);
        user_anc_log("anc_train_close ok\n");
    }
}
#endif

/*查询当前ANC是否处于训练状态*/
int anc_train_open_query(void)
{
    if (anc_hdl) {
#if ANC_EAR_ADAPTIVE_EN
        if (anc_ear_adaptive_busy_get()) {
            return 1;
        }
#endif/*ANC_EAR_ADAPTIVE_EN*/
        if (anc_hdl->param.mode == ANC_TRAIN) {
            return 1;
        }
    }
    return 0;
}

extern void audio_dump();
static void anc_timer_deal(void *priv)
{
    audio_config_dump();

#if ANC_MULT_ORDER_ENABLE
    printf("anc scene:%d\n", audio_anc_mult_scene_get());
#endif
    audio_anc_fade_ctr_get_min();
    /* mem_stats(); */
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        printf("%c, master\n", bt_tws_get_local_channel());
    } else {
        printf("%c, slave\n", bt_tws_get_local_channel());
    }
#endif/*TCFG_USER_TWS_ENABLE*/
    //anc_mem_unfree_dump();
    /* audio_dump(); */
}

static int anc_tone_play_cb(void *priv, enum stream_event event)
{
    u8 next_mode = anc_hdl->tone_mode;

#if ANC_EAR_ADAPTIVE_EN
    if ((event != STREAM_EVENT_START) && (event != STREAM_EVENT_STOP) && (event != STREAM_EVENT_INIT)) {
        if (anc_hdl->new_mode == next_mode && next_mode == ANC_ON && anc_hdl->ear_adaptive) {
            /*提示音被打断，退出自适应*/
            anc_ear_adaptive_tone_play_err_cb();
        }
    }
#endif /*ANC_EAR_ADAPTIVE_EN*/

    if (event != STREAM_EVENT_STOP) {
        return 0;
    }
    printf("anc_tone_play_cb,anc_mode:%d,%d,new_mode:%d\n", next_mode, anc_hdl->param.mode, anc_hdl->new_mode);
    /*
     *当最新的目标ANC模式和即将切过去的模式一致，才进行模式切换实现。
     *否则表示，模式切换操作（比如快速按键切模式）new_mode领先于ANC模式切换实现next_mode
     *则直接在切模式的时候，实现最新的目标模式
     */
    if (anc_hdl->new_mode == next_mode) {
#if ANC_EAR_ADAPTIVE_EN
        if (next_mode == ANC_ON && anc_hdl->ear_adaptive) {
            anc_hdl->ear_adaptive = 0;
            anc_ear_adaptive_tone_play_cb();
        } else
#endif/*ANC_EAR_ADAPTIVE_EN*/
        {
            anc_mode_switch_deal(next_mode);
        }
    } else {
        anc_hdl->mode_switch_lock = 0;
    }
    return 0;
}


static void tws_anc_tone_callback(int priv, enum stream_event event)
{
    r_printf("tws_anc_tone_callback: %d\n",  event);
    anc_tone_play_cb(NULL, event);
}

#define TWS_ANC_TONE_STUB_FUNC_UUID   0x1FE4698C
REGISTER_TWS_TONE_CALLBACK(tws_anc_tone_stub) = {
    .func_uuid  = TWS_ANC_TONE_STUB_FUNC_UUID,
    .callback   = tws_anc_tone_callback,
};

/*
*********************************************************************
*                  anc_play_tone
* Description: ANC提示音播放和模式切换
* Arguments  : file_name	提示音名字
*			   cb_sel	 	切模式方式选择
* Return	 : None.
* Note(s)    : 通过cb_sel选择是在播放提示音切模式，还是播提示音同时切
*			   模式
*********************************************************************
*/
static int anc_play_tone(const char *file_name, u8 cb_sel, u8 tws_sync)
{
    int ret = 0;
    if (cb_sel) {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state() && tws_sync) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                ret = tws_play_tone_file_callback(file_name, TWS_ANC_SYNC_TIMEOUT, TWS_ANC_TONE_STUB_FUNC_UUID);
            }
        } else {
            ret = play_tone_file_callback(file_name, NULL, anc_tone_play_cb);
            /* ret = tws_play_tone_file_callback(file_name, TWS_ANC_SYNC_TIMEOUT, TWS_ANC_TONE_STUB_FUNC_UUID); */
        }
#else
        ret = play_tone_file_callback(file_name, NULL, anc_tone_play_cb);
#endif
    } else {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state() && tws_sync) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                ret = tws_play_tone_file(file_name, TWS_ANC_SYNC_TIMEOUT);
            }
        } else {
            /* ret = tws_play_tone_file(file_name, TWS_ANC_SYNC_TIMEOUT); */
            ret = play_tone_file(file_name);
        }
#else
        ret = play_tone_file(file_name);
#endif
    }
    return ret;
}

/*
*********************************************************************
*                  Audio ANC Tone Play And Mode Switch
* Description: ANC提示音播放和模式切换
* Arguments  : index	    当前模式
*			   preemption	提示音抢断播放标识
*			   cb_sel	 	切模式方式选择
* Return	 : None.
* Note(s)    : 通过cb_sel选择是在播放提示音切模式，还是播提示音同时切
*			   模式
*********************************************************************
*/
static void anc_tone_play_and_mode_switch(u8 mode, u8 preemption, u8 cb_sel)
{
    /* if (!esco_player_runing() && !a2dp_player_runing()) {	//后台没有音频，提示音默认打断播放
        preemption = 1;
    } */
    u8 tws_sync = 1;
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_algom_suspend();
#endif
    if (cb_sel) {
        /*ANC打开情况下，播提示音的同时，anc效果淡出*/
        if (anc_hdl->state == ANC_STA_OPEN) {
            audio_anc_fade_ctr_set(ANC_FADE_MODE_SWITCH, AUDIO_ANC_FDAE_CH_ALL, 0);
        }
        if (mode == ANC_ON) {
            char *tone = (char *)get_tone_files()->anc_on;
#if ANC_EAR_ADAPTIVE_EN
            if (anc_hdl->ear_adaptive) {   //非ANC模式下切自适应，需要提前开MIC，ADC需要稳定时间
                anc_ear_adaptive_tone_play_start();
                tone = (char *)get_tone_files()->anc_adaptive;
                tws_sync = anc_ear_adaptive_tws_sync_en_get();   //自适应状态关闭左右平衡，则不需要同步切模式
            }
#endif/*ANC_EAR_ADAPTIVE_EN*/
            anc_play_tone(tone, cb_sel, tws_sync);
        } else if (mode == ANC_TRANSPARENCY) {
            anc_play_tone(get_tone_files()->anc_trans, cb_sel, tws_sync);
        } else if (mode == ANC_OFF) {
            anc_play_tone(get_tone_files()->anc_off, cb_sel, tws_sync);
        }
        anc_hdl->tone_mode = anc_hdl->new_mode;

    } else {
        if (mode == ANC_ON) {
            anc_play_tone(get_tone_files()->anc_on, cb_sel, tws_sync);
        } else if (mode == ANC_TRANSPARENCY) {
            anc_play_tone(get_tone_files()->anc_trans, cb_sel, tws_sync);
        } else if (mode == ANC_OFF) {
            anc_play_tone(get_tone_files()->anc_off, cb_sel, tws_sync);
        }
        anc_mode_switch_deal(mode);
    }
}

static void anc_tone_stop(void)
{
    if (anc_hdl && anc_hdl->mode_switch_lock) {
        tone_player_stop();
    }
}

/*
*********************************************************************
*                  anc_fade_in_timer
* Description: ANC增益淡入函数
* Arguments  : None.
* Return	 : None.
* Note(s)    :通过定时器的控制，使ANC的淡入增益一点一点叠加
*********************************************************************
*/
static void anc_fade_in_timer(void *arg)
{
    anc_hdl->fade_gain += (anc_hdl->param.anc_fade_gain / anc_hdl->param.fade_time_lvl);
    if (anc_hdl->fade_gain > anc_hdl->param.anc_fade_gain) {
        anc_hdl->fade_gain = anc_hdl->param.anc_fade_gain;
        usr_timer_del(anc_hdl->fade_in_timer);
    }
    audio_anc_fade_ctr_set(ANC_FADE_MODE_SWITCH, AUDIO_ANC_FDAE_CH_ALL, anc_hdl->fade_gain);
}

static void anc_fade_in_timer_add(audio_anc_t *param)
{
    u32 alogm, dly;
    if (param->anc_fade_en) {
        alogm = audio_anc_gains_alogm_get((param->mode == ANC_TRANSPARENCY) ? ANC_TRANS_TYPE : ANC_FF_TYPE);
        dly = audio_anc_fade_dly_get(param->anc_fade_gain, alogm);
        anc_hdl->fade_gain = 0;
        if (param->mode == ANC_ON) {		//可自定义模式，当前仅在ANC模式下使用
            user_anc_log("anc_fade_time_lvl  %d\n", param->fade_time_lvl);
            anc_hdl->fade_gain = (param->anc_fade_gain / param->fade_time_lvl);
            audio_anc_fade_ctr_set(ANC_FADE_MODE_SWITCH, AUDIO_ANC_FDAE_CH_ALL, anc_hdl->fade_gain);
            if (param->fade_time_lvl > 1) {
                anc_hdl->fade_in_timer = usr_timer_add((void *)0, anc_fade_in_timer, dly, 1);
            }
        } else if ((param->mode == ANC_TRANSPARENCY) || (param->mode == ANC_BYPASS)) {
            audio_anc_fade_ctr_set(ANC_FADE_MODE_SWITCH, AUDIO_ANC_FDAE_CH_ALL, param->anc_fade_gain);
        }
    }
}
static void anc_fade_out_timeout(void *arg)
{
    os_taskq_post_msg("anc", 2, ANC_MSG_RUN, (int)arg);
}

/*ANC淡入淡出增益设置接口*/
void audio_anc_fade_gain_set(int gain)
{
    //anc_hdl->param.anc_fade_gain = gain;
    /* if (anc_hdl->param.mode != ANC_OFF) { */
    //用户层操作 ANC_FADE_MODE_USER 的模式
    audio_anc_fade_ctr_set(ANC_FADE_MODE_USER, AUDIO_ANC_FDAE_CH_ALL, gain);
    /* } */
}
/*ANC淡入淡出增益接口*/
int audio_anc_fade_gain_get(void)
{
    return anc_hdl->param.anc_fade_gain;
}

static void anc_fade(u32 gain, u8 mode)
{
    u32	alogm, dly;
    if (anc_hdl) {
        alogm = audio_anc_gains_alogm_get((anc_hdl->param.mode == ANC_TRANSPARENCY) ? ANC_TRANS_TYPE : ANC_FF_TYPE);
        dly = audio_anc_fade_dly_get(anc_hdl->param.anc_fade_gain, alogm);
        user_anc_log("anc_fade:%d,dly:%d, mode %d", gain, dly, mode);
        audio_anc_fade_ctr_set(ANC_FADE_MODE_SWITCH, AUDIO_ANC_FDAE_CH_ALL, gain);
        if (anc_hdl->param.anc_fade_en) {
            usr_timeout_del(anc_hdl->fade_in_timer);
            usr_timeout_add((void *)(intptr_t)mode, anc_fade_out_timeout, dly, 1);
        } else {/*不淡入淡出，则直接切模式*/
            os_taskq_post_msg("anc", 2, ANC_MSG_RUN, mode);
        }
    }
}

/*
 *mode:降噪/通透/关闭
 *tone_play:切换模式的时候，是否播放提示音
 */
static void anc_mode_switch_deal(u8 mode)
{
    user_anc_log("anc switch,state:%s, mode %d", anc_state_str[anc_hdl->state], mode);
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (anc_hdl->switch_rtanc_resume_id) {
        sys_timer_del(anc_hdl->switch_rtanc_resume_id);
        anc_hdl->switch_rtanc_resume_id = 0;
    }
    audio_anc_real_time_adaptive_suspend("MODE_SWITCH");
#endif
    anc_hdl->mode_switch_lock = 1;
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_algom_suspend();
#endif
    if (anc_hdl->state == ANC_STA_OPEN) {
        user_anc_log("anc open now,switch mode:%d", mode);
        anc_fade(0, mode);//切模式，先fade_out
    } else if (anc_hdl->state == ANC_STA_INIT) {
        audio_anc_fade_ctr_set(ANC_FADE_MODE_SWITCH, AUDIO_ANC_FDAE_CH_ALL, 0);
#if 1
        os_taskq_post_msg("anc", 2, ANC_MSG_RUN, mode);
#else

        if (anc_hdl->new_mode != ANC_OFF) {
            /*anc工作，关闭自动关机*/
            /* sys_auto_shut_down_disable(); */
            os_taskq_post_msg("anc", 2, ANC_MSG_RUN, mode);
        } else {
            user_anc_log("anc close now,new mode is ANC_OFF\n");
            anc_hdl->mode_switch_lock = 0;
        }
#endif
    } else {
        user_anc_log("anc state err:%d\n", anc_hdl->state);
        anc_hdl->mode_switch_lock = 0;
    }
}

void anc_gain_app_value_set(int app_value)
{
    static int anc_gain_app_value = -1;
    if (-1 == app_value && -1 != anc_gain_app_value) {
        /* anc_hdl->param.anc_ff_gain = anc_gain_app_value; */
    }
    anc_gain_app_value = app_value;
}

#if TCFG_USER_TWS_ENABLE
/*tws同步播放模式提示音，并且同步进入anc模式*/
void anc_tone_sync_play(int tone_name)
{
    if (anc_hdl) {
        user_anc_log("anc_tone_sync_play:%d", tone_name);
        os_taskq_post_msg("anc", 2, ANC_MSG_TONE_SYNC, tone_name);
    }
}

static void anc_play_tone_at_same_time_handler(int tone_name, int err)
{
    switch (tone_name) {
    case SYNC_TONE_ANC_ON:
    case SYNC_TONE_ANC_OFF:
    case SYNC_TONE_ANC_TRANS:
        anc_tone_sync_play(tone_name);
        break;
    }
}

TWS_SYNC_CALL_REGISTER(anc_tws_tone_play) = {
    .uuid = 0x123A9E53,
    .task_name = "app_core",
    .func = anc_play_tone_at_same_time_handler,
};

void anc_play_tone_at_same_time(int tone_name, int msec)
{
    tws_api_sync_call_by_uuid(0x123A9E53, tone_name, msec);
}
#endif/*TCFG_USER_TWS_ENABLE*/

//用于某些需要等anc完全关闭才能开启的场景，如通话与anc的mic复用
static void anc_mode_switch_sem_create()
{
    if (!anc_hdl) {
        return;
    }
    if (anc_hdl->sem) {
        return;
    }
    anc_hdl->sem = zalloc(sizeof(OS_SEM));
    os_sem_create(anc_hdl->sem, 0);
}
static void anc_mode_switch_sem_post()
{
    if (!anc_hdl || !anc_hdl->sem) {
        return;
    }
    os_sem_post(anc_hdl->sem);
}
static void anc_mode_switch_sem_pend()
{
    if (!anc_hdl || !anc_hdl->sem) {
        return;
    }
    os_sem_pend(anc_hdl->sem, 0);
}
static void anc_mode_switch_sem_del()
{
    if (!anc_hdl) {
        return;
    }
    if (!anc_hdl->sem) {
        return;
    }
    os_sem_del(anc_hdl->sem, 0);
    free(anc_hdl->sem);
    anc_hdl->sem = NULL;
}

//阻塞方式等待anc模式切换完成
void anc_mode_switch_pend(u8 mode, u8 tone_play)
{
    anc_mode_switch_sem_create();
    if (!anc_mode_switch(mode, tone_play)) {
        anc_mode_switch_sem_pend();
    }
    anc_mode_switch_sem_del();
}

int anc_mode_switch(u8 mode, u8 tone_play)
{
    anc_hdl->user_mode = mode;
    return anc_mode_switch_base(mode, tone_play);
}

//SDK内部使用，禁止用户层直接调用
int anc_mode_switch_base(u8 mode, u8 tone_play)
{
    if (anc_hdl == NULL) {
        return 1;
    }
    u8 ignore_same_mode = 0;
    u8 tws_sync_en = 1;
    /*模式切换超出范围*/
    if ((mode > (ANC_MODE_NULL - 1)) || (mode < ANC_OFF)) {
        user_anc_log("anc mode switch err:%d", mode);
        return 1;
    }
#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
    //获取频响时 不允许其他模式切换
    if (audio_icsd_afq_is_running()) {
        user_anc_log("Error :audio_icsd_afq_is_runnin\n");
        return 1;
    }
#endif
#if TCFG_AUDIO_ANC_DCC_TRIM_ENABLE
    if (audio_anc_dcc_trim_state_get()) {
        user_anc_log("Error :audio anc dcc trim by now\n");
        return 1;
    }
    if (anc_hdl->param.dcc_trim.mode == ANC_DCC_TRIM_START) {
        ignore_same_mode = 1;
    }
#endif
    /*模式切换同一个*/
#if ANC_EAR_ADAPTIVE_EN
    if (anc_hdl->ear_adaptive) {
        ignore_same_mode = 1;
        tws_sync_en = anc_ear_adaptive_tws_sync_en_get();   //自适应状态关闭左右平衡，则不需要同步切模式
    }
#endif/*ANC_EAR_ADAPTIVE_EN*/
    if ((mode == ANC_EXT) && tone_play) {
        user_anc_log("anc mode switch err: ANC_EXT don't play tone\n");
        return 1;
    }
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    if (anc_hdl->new_mode == ANC_EXT && mode == ANC_OFF && audio_icsd_adt_is_running()) {
        user_anc_log("anc mode switch err: in ANC_OFF(ANC_EXT)\n");
        return 1;
    }
#endif/*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
    if ((anc_hdl->new_mode == mode) && (!ignore_same_mode)) {

        user_anc_log("anc mode switch err:same mode");
        return 1;
    }
#if ANC_MODE_EN_MODE_NEXT_SW
    if (anc_hdl->mode_switch_lock && (mode != ANC_EXT)) {
        user_anc_log("anc mode switch lock : %d\n", mode);
        anc_hdl->switch_latch_flag = 1; //若切模式锁存，则记住用户层最后一次模式记录
        return 1;
    }
    anc_hdl->mode_switch_lock = 1;
#endif/*ANC_MODE_EN_MODE_NEXT_SW*/

    anc_hdl->new_mode = mode;/*记录最新的目标ANC模式*/
#if ANC_TONE_END_MODE_SW && (!ANC_MODE_EN_MODE_NEXT_SW)
    anc_tone_stop();
#endif/*ANC_TONE_END_MODE_SW*/

    if (anc_hdl->suspend) {
        anc_hdl->param.tool_enablebit = 0;
    }

    printf("anc_mode_switch,tws_sync_en = %d, mode %d\n", tws_sync_en, mode);
    /* anc_gain_app_value_set(-1); */		//没有处理好bypass与ANC_ON的关系
    /*
     *ANC模式提示音播放规则
     *(1)根据应用选择是否播放提示音：tone_play
     *(2)tws连接的时候，主机发起模式提示音同步播放
     *(3)单机的时候，直接播放模式提示音
     */
    if (tone_play) {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state() && tws_sync_en) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                user_anc_log("[tws_master]anc_tone_sync_play");
                anc_hdl->sync_busy = 1;
                if (anc_hdl->new_mode == ANC_ON) {
                    anc_play_tone_at_same_time(SYNC_TONE_ANC_ON, TWS_ANC_SYNC_TIMEOUT);
                } else if (anc_hdl->new_mode == ANC_OFF) {
                    anc_play_tone_at_same_time(SYNC_TONE_ANC_OFF, TWS_ANC_SYNC_TIMEOUT);
                } else {
                    anc_play_tone_at_same_time(SYNC_TONE_ANC_TRANS, TWS_ANC_SYNC_TIMEOUT);
                }
            }
            return 0;
        } else {
            user_anc_log("anc_tone_play");
            anc_tone_play_and_mode_switch(mode, ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
        }
#else
        anc_tone_play_and_mode_switch(mode, ANC_TONE_PREEMPTION, ANC_TONE_END_MODE_SW);
#endif/*TCFG_USER_TWS_ENABLE*/
    } else {
        anc_mode_switch_deal(mode);
    }
    return 0;
}

static void anc_ui_mode_sel_timer(void *priv)
{
    if (anc_hdl->ui_mode_sel && (anc_hdl->ui_mode_sel != anc_mode_get())) {
        /*
         *提示音不打断
         *tws提示音同步播放完成
         */
        /* if ((tone_get_status() == 0) && (anc_hdl->sync_busy == 0)) { */
        if (anc_hdl->sync_busy == 0) {
            user_anc_log("anc_ui_mode_sel_timer:%d,sync_busy:%d", anc_hdl->ui_mode_sel, anc_hdl->sync_busy);
            anc_mode_switch(anc_hdl->ui_mode_sel, 1);
            sys_timer_del(anc_hdl->ui_mode_sel_timer);
            anc_hdl->ui_mode_sel_timer = 0;
        }
    }
}

/*ANC通过ui菜单选择anc模式,处理快速切换的情景*/
void anc_ui_mode_sel(u8 mode, u8 tone_play)
{
    /*
     *timer存在表示上个模式还没有完成切换
     *提示音不打断
     *tws提示音同步播放完成
     */
    /* if ((anc_hdl->ui_mode_sel_timer == 0) && (tone_get_status() == 0) && (anc_hdl->sync_busy == 0)) { */
    if ((anc_hdl->ui_mode_sel_timer == 0) && (anc_hdl->sync_busy == 0)) {
        user_anc_log("anc_ui_mode_sel[ok]:%d", mode);
        anc_mode_switch(mode, tone_play);
    } else {
        user_anc_log("anc_ui_mode_sel[dly]:%d,timer:%d,sync_busy:%d", mode, anc_hdl->ui_mode_sel_timer, anc_hdl->sync_busy);
        anc_hdl->ui_mode_sel = mode;
        if (anc_hdl->ui_mode_sel_timer == 0) {
            anc_hdl->ui_mode_sel_timer = sys_timer_add(NULL, anc_ui_mode_sel_timer, 50);
        }
    }
}

/*在anc任务里面切换anc模式，避免上一次切换没有完成，这次切换被忽略的情况*/
void anc_mode_switch_in_anctask(u8 mode, u8 tone_play)
{
    if (anc_hdl) {
        os_taskq_post_msg("anc", 3, ANC_MSG_MODE_SWITCH_IN_ANCTASK, mode, tone_play);
    }
}

/*ANC挂起*/
void anc_suspend(void)
{
    if (anc_hdl) {
        user_anc_log("anc_suspend\n");
        anc_hdl->suspend = 1;
        anc_hdl->param.tool_enablebit = 0;	//使能设置为0
        if (anc_hdl->param.anc_fade_en) {	  //挂起ANC增益淡出
            audio_anc_fade_ctr_set(ANC_FADE_MODE_SUSPEND, AUDIO_ANC_FDAE_CH_ALL, 0);
        }
        audio_anc_en_set(0);
    }
}

/*ANC恢复*/
void anc_resume(void)
{
    if (anc_hdl) {
        user_anc_log("anc_resume\n");
        anc_hdl->suspend = 0;
        anc_hdl->param.tool_enablebit = anc_hdl->param.enablebit;	//使能恢复
        audio_anc_en_set(1);
        if (anc_hdl->param.anc_fade_en) {	  //恢复ANC增益淡入
            //避免param.anc_fade_gain 被修改，这里使用固定值
            audio_anc_fade_ctr_set(ANC_FADE_MODE_SUSPEND, AUDIO_ANC_FDAE_CH_ALL, AUDIO_ANC_FADE_GAIN_DEFAULT);
        }
    }
}

/*ANC信息保存*/
void anc_info_save()
{
    if (anc_hdl) {
        anc_info_t anc_info;
        int ret = syscfg_read(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret == sizeof(anc_info)) {
#if INEAR_ANC_UI
            if (anc_info.mode == anc_mode_get() && anc_info.inear_tws_mode == inear_tws_ancmode) {
#else
            if (anc_info.mode == anc_mode_get()) {
#endif/*INEAR_ANC_UI*/
                user_anc_log("anc info.mode == cur_anc_mode");
                return;
            }
        } else {
            user_anc_log("read anc_info err");
        }

        user_anc_log("save anc_info");
        anc_info.mode = anc_mode_get();
#if INEAR_ANC_UI
        anc_info.inear_tws_mode = inear_tws_ancmode;
#endif/*INEAR_ANC_UI*/
        ret = syscfg_write(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret != sizeof(anc_info)) {
            user_anc_log("anc info save err!\n");
        }

    }
}

/*系统上电的时候，根据配置决定是否进入上次的模式*/
void anc_poweron(void)
{
#if TCFG_AUDIO_ANC_DCC_TRIM_ENABLE
    audio_anc_dcc_trim_check();
#endif
    if (anc_hdl) {
#if ANC_INFO_SAVE_ENABLE
        anc_info_t anc_info;
        int ret = syscfg_read(CFG_ANC_INFO, &anc_info, sizeof(anc_info));
        if (ret == sizeof(anc_info)) {
            user_anc_log("read anc_info succ,state:%s,mode:%s", anc_state_str[anc_hdl->state], anc_mode_str[anc_info.mode]);
#if INEAR_ANC_UI
            inear_tws_ancmode = anc_info.inear_tws_mode;
#endif/*INEAR_ANC_UI*/
            if ((anc_hdl->state == ANC_STA_INIT) && (anc_info.mode != ANC_OFF)) {
                anc_mode_switch(anc_info.mode, 0);
            }
        } else {
            user_anc_log("read anc_info err");
        }
#endif/*ANC_INFO_SAVE_ENABLE*/
    }
}

/*ANC poweroff*/
void anc_poweroff(void)
{
    if (anc_hdl) {
#if ANC_EAR_ADAPTIVE_EN && (!ANC_DEVELOPER_MODE_EN)
        anc_ear_adaptive_power_off_save_data();
#endif/*ANC_POWEOFF_SAVE_ADAPTIVE_DATA*/
        user_anc_log("anc_cur_state:%s\n", anc_state_str[anc_hdl->state]);
#if ANC_INFO_SAVE_ENABLE
        anc_info_save();
#endif/*ANC_INFO_SAVE_ENABLE*/
        if (anc_hdl->state == ANC_STA_OPEN) {
            user_anc_log("anc_poweroff\n");
            /*close anc module when fade_out timeout*/
            anc_hdl->param.mode = ANC_OFF;
            anc_hdl->new_mode = anc_hdl->param.mode;
            anc_fade(0, ANC_OFF);
        }
    }
}

/*模式切换测试demo*/
#define ANC_MODE_NUM	3 /*ANC模式循环切换*/
static const u8 anc_mode_switch_tab[ANC_MODE_NUM] = {
    ANC_OFF,
    ANC_TRANSPARENCY,
    ANC_ON,
};
void anc_mode_next(void)
{
    if (anc_hdl) {
        if (anc_train_open_query()) {
            return;
        }
        u8 next_mode = 0;
        local_irq_disable();
        anc_hdl->param.anc_fade_en = ANC_FADE_EN;	//防止被其他地方清0
        for (u8 i = 0; i < ANC_MODE_NUM; i++) {
            if (anc_mode_switch_tab[i] == anc_mode_get()) {
                next_mode = i + 1;
                if (next_mode >= ANC_MODE_NUM) {
                    next_mode = 0;
                }
                if ((anc_hdl->mode_enable & BIT(anc_mode_switch_tab[next_mode])) == 0) {
                    user_anc_log("anc_mode_filt,next:%d,en:%d", next_mode, anc_hdl->mode_enable);
                    next_mode++;
                    if (next_mode >= ANC_MODE_NUM) {
                        next_mode = 0;
                    }
                }
                //g_printf("fine out anc mode:%d,next:%d,i:%d",anc_hdl->param.mode,next_mode,i);
                break;
            }
        }
        local_irq_enable();
        //user_anc_log("anc_next_mode:%d old:%d,new:%d", next_mode, anc_hdl->param.mode, anc_mode_switch_tab[next_mode]);
        u8 new_mode = anc_mode_switch_tab[next_mode];
        user_anc_log("new_mode:%s", anc_mode_str[new_mode]);

#if ANC_EAR_ADAPTIVE_EN && ANC_EAR_ADAPTIVE_EVERY_TIME		//非通话状态下，每次切模式都自适应
        if ((anc_mode_switch_tab[next_mode] == ANC_ON) && (esco_player_runing() == 0)) {
            audio_anc_mode_ear_adaptive(1);
        } else
#endif/*ANC_EAR_ADAPTIVE_EVERY_TIME*/
        {
            anc_mode_switch(anc_mode_switch_tab[next_mode], 1);
        }
    }
}

/*设置ANC支持切换的模式*/
void anc_mode_enable_set(u8 mode_enable)
{
    if (anc_hdl) {
        anc_hdl->mode_enable = mode_enable;
        u8 mode_cnt = 0;
        for (u8 i = 1; i < 4; i++) {
            if (mode_enable & BIT(i)) {
                mode_cnt++;
                user_anc_log("%s Select", anc_mode_str[i]);
            }
        }
        user_anc_log("anc_mode_enable_set:%d", mode_cnt);
        anc_hdl->mode_num = mode_cnt;
    }
}

/*获取anc状态，0:空闲，l:忙*/
u8 anc_status_get(void)
{
    u8 status = 0;
    if (anc_hdl) {
        if (anc_hdl->state == ANC_STA_OPEN) {
            status = 1;
        }
    }
    return status;
}

/*获取真实anc模式，包括ANC_EXT*/
u8 anc_real_mode_get(void)
{
    if (anc_hdl) {
        return anc_hdl->param.mode;
    }
    return 0;
}

/*获取anc当前模式*/
u8 anc_mode_get(void)
{
    if (anc_hdl) {
        //user_anc_log("anc_mode_get:%s", anc_mode_str[anc_hdl->param.mode]);
        if (anc_hdl->param.mode == ANC_EXT) {
            return ANC_OFF;		//ANC_EXT只是启动数据流，ANC处于关闭状态
        } else {
            return anc_hdl->param.mode;
        }
    }
    return 0;
}

/*设置anc用户层模式*/
void anc_user_mode_set(u8 mode)
{
    if (anc_hdl) {
        user_anc_log("anc_user_mode_set:%s", anc_mode_str[mode]);
        anc_hdl->user_mode = mode;
    }
}

/*获取anc用户层模式*/
u8 anc_user_mode_get(void)
{
    if (anc_hdl) {
        //user_anc_log("anc_mode_get:%s", anc_mode_str[anc_hdl->param.mode]);
        return anc_hdl->user_mode;
    }
    return 0;
}

/*获取anc是否处于切模式状态*/
u8 anc_mode_switch_lock_get(void)
{
    if (anc_hdl) {
        return anc_hdl->mode_switch_lock;
    }
    return 0;
}

/*设置ANC切模式状态*/
void anc_mode_switch_lock_clean(void)
{
    if (anc_hdl) {
        anc_hdl->mode_switch_lock = 0;
    }
}

/*获取anc记录的最新的目标ANC模式*/
u8 anc_new_target_mode_get(void)
{
    if (anc_hdl) {
        //user_anc_log("anc_mode_get:%s", anc_mode_str[anc_hdl->new_mode]);
        return anc_hdl->new_mode;
    }
    return 0;
}

/*获取anc模式，ff_mic的增益*/
u8 audio_anc_ffmic_gain_get(void)
{
    u8 gain = 0;
    if (anc_hdl) {
        if (anc_hdl->param.ch & ANC_L_CH) {
            gain = anc_hdl->param.gains.l_ffmic_gain;
        } else {
            gain = anc_hdl->param.gains.r_ffmic_gain;
        }
    }
    return gain;
}

/*获取anc模式，fb_mic的增益*/
u8 audio_anc_fbmic_gain_get(void)
{
    u8 gain = 0;
    if (anc_hdl) {
        if (anc_hdl->param.ch & ANC_L_CH) {
            gain = anc_hdl->param.gains.l_fbmic_gain;
        } else {
            gain = anc_hdl->param.gains.r_fbmic_gain;
        }
    }
    return gain;
}

/*获取anc模式，指定mic的增益, mic_sel:目标MIC通道*/
u8 audio_anc_mic_gain_get(u8 mic_sel)
{
    if (anc_hdl) {
        if (mic_sel < AUDIO_ADC_MAX_NUM)  {
            return anc_hdl->param.mic_param[mic_sel].gain;
        }
    }
    return 0;
}

/*获取anc mic是否使能*/
u8 audio_anc_mic_en_get(u8 mic_sel)
{
    if (anc_hdl) {
        if (mic_sel < AUDIO_ADC_MAX_NUM)  {
            return anc_hdl->param.mic_param[mic_sel].en;
        }
    }
    return 0;
}

s8 audio_anc_mic_gain_get_dB(u8 mic_ch, u8 is_talk_mic)
{
    u8 i;
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (BIT(i) == mic_ch) {
            break;
        }
    }
    u8 index = 0;
    if (is_talk_mic) {
        index = audio_adc_file_get_gain(i);
    } else {
        index = audio_anc_mic_gain_get(i);
    }
    user_anc_log("mic_ch 0x%x, is_talk_mic %d, index %d, i %d\n", mic_ch, is_talk_mic, index, i);
    return audio_adc_gain_dB_table[index];
}

void audio_anc_drc_toggle_set(u8 toggle)
{
    if (anc_hdl) {
        anc_hdl->param.drc_toggle = toggle;
    }
}

void audio_anc_dcc_adaptive_toggle_set(u8 toggle)
{
    if (anc_hdl) {
        anc_hdl->param.dcc_adaptive_toggle = toggle;
    }
}

/*anc coeff读接口*/
int *anc_coeff_read(void)
{
#if ANC_BOX_READ_COEFF
    int *coeff = anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
    if (coeff) {
        coeff = (int *)((u8 *)coeff);
    }
    user_anc_log("anc_coeff_read:0x%x, size %d", (u32)coeff, anc_hdl->param.coeff_size);
    return coeff;
#else
    return NULL;
#endif/*ANC_BOX_READ_COEFF*/
}


/*anc coeff写接口*/
int anc_coeff_write(int *coeff, u16 len)
{
#if ANC_MULT_ORDER_ENABLE
    return -1;
#endif/*ANC_MULT_ORDER_ENABLE*/
    anc_coeff_t *db_coeff = (anc_coeff_t *)coeff;
    anc_coeff_t *tmp_coeff = NULL;

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN && TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    audio_rtanc_cmp_data_clear();
#endif

    user_anc_log("anc_coeff_write:0x%x, len:%d", (u32)coeff, len);
    int ret = anc_coeff_check(db_coeff, len);
    if (ret) {
        return ret;
    }
    if (db_coeff->cnt != 1) {
        ret = anc_db_put(&anc_hdl->param, NULL, (anc_coeff_t *)coeff);
    } else {	//保存对应的单项滤波器
        ret = anc_coeff_single_fill(&anc_hdl->param, (anc_coeff_t *)coeff);
    }
    if (ret) {
        user_anc_log("anc_coeff_write err:%d", ret);
        return -1;
    }
    db_coeff = (anc_coeff_t *)anc_db_get(ANC_DB_COEFF, &anc_hdl->param.coeff_size);
    user_anc_log("anc_db coeff_size %d\n", anc_hdl->param.coeff_size);
    anc_coeff_fill(db_coeff);
#if ANC_EAR_ADAPTIVE_EN
    //左右参数映射，测试使用
    audio_anc_param_map(1, 0);
#endif/*ANC_EAR_ADAPTIVE_EN*/

    if (anc_hdl->param.mode != ANC_OFF) {		//实时更新填入使用
        audio_anc_reset(&anc_hdl->param, 0);
    }
    return 0;
}

static u8 ANC_idle_query(void)
{
    if (anc_hdl) {
        /*ANC训练模式，不进入低功耗*/
        if (anc_train_open_query()) {
            return 0;
        }
    }
    return 1;
}

static enum LOW_POWER_LEVEL ANC_level_query(void)
{
    if (anc_hdl == NULL) {
        return LOW_POWER_MODE_SLEEP;
    }
    /*根据anc的状态选择sleep等级*/
    if (anc_status_get() || anc_hdl->mode_switch_lock) {
        /*anc打开，进入轻量级低功耗*/
        return LOW_POWER_MODE_LIGHT_SLEEP;
    }
    /*anc关闭，进入最优低功耗*/
    return LOW_POWER_MODE_DEEP_SLEEP;
}

AT_VOLATILE_RAM_CODE_ANC
u8 is_lowpower_anc_active()
{
    if (anc_hdl) {
        if (anc_hdl->state == ANC_STA_OPEN || anc_hdl->mode_switch_lock) {
            /* 若anc 开启，则进入轻量级低功耗时，保持住ANC对应时钟 */
            if ((anc_hdl->param.ch == (ANC_L_CH | ANC_R_CH)) && !anc_hdl->param.lr_lowpower_en) {
                return ANC_CLOCK_USE_PLL;
            } else {
                return ANC_CLOCK_USE_BTOSC;
            }
        }
    }
    return ANC_CLOCK_USE_CLOSE;
}

REGISTER_LP_TARGET(ANC_lp_target) = {
    .name       = "ANC",
    .level      = ANC_level_query,
    .is_idle    = ANC_idle_query,
};

void chargestore_uart_data_deal(u8 *data, u8 len)
{
    /* anc_uart_process(data, len); */
}

#if 0//TCFG_ANC_TOOL_DEBUG_ONLINE
//回复包
void anc_ci_send_packet(u32 id, u8 *packet, int size)
{
    if (DB_PKT_TYPE_ANC == id) {
        app_online_db_ack(anc_hdl->anc_parse_seq, packet, size);
        return;
    }
    /* y_printf("anc_app_spp_tx\n"); */
    ci_send_packet(id, packet, size);
}

//接收函数
static int anc_app_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
    /* y_printf("anc_app_spp_rx\n"); */
    anc_hdl->anc_parse_seq  = ext_data[1];
    anc_spp_rx_packet(packet, size);
    return 0;
}
//发送函数
void anc_btspp_packet_tx(u8 *packet, int size)
{
    app_online_db_send(DB_PKT_TYPE_ANC, packet, size);
}
#endif/*TCFG_ANC_TOOL_DEBUG_ONLINE*/

#if TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE
u8 anc_btspp_train_again(u8 mode, u32 dat)
{
    /* tws_api_detach(TWS_DETACH_BY_POWEROFF); */
    //ANC模式下可直接获取REF+ERRMIC的数据
    /* if((dat == 3) || (dat == 5)) { */
    /* anc_hdl->param.debug_sel = dat; */
    /* os_taskq_post_msg("anc", 1, ANC_MSG_MIC_DATA_GET); */
    /* return 0; */
    /* } */
    if (anc_hdl->param.train_para.train_busy) {
        return 0;
    }
#if ANC_EAR_ADAPTIVE_EN
    if (anc_ear_adaptive_busy_get()) {	//ANC自适应切换时，不支持获取DMA
        return 0;
    }
//仅TWS耳机支持工具右声道映射到左声道
#if TCFG_USER_TWS_ENABLE && !AUDIO_ANC_STEREO_ENABLE
    switch (dat) {
    case 2:
        dat = 4;
        break;
    case 3:
        dat = 5;
        break;
    case 6:
        dat = 7;
        break;
    }
#endif/*TCFG_USER_TWS_ENABLE*/
#endif/*ANC_EAR_ADAPTIVE_EN*/
    user_anc_log("anc_btspp_train_again\n");
    audio_anc_close();
    anc_hdl->state = ANC_STA_INIT;
    anc_train_open(mode, (u8)dat);
    anc_hdl->param.train_para.train_busy = 1;
    return 1;
}
#endif

u8 audio_anc_develop_get(void)
{
    return anc_hdl->param.developer_mode;
}

void audio_anc_develop_set(u8 mode)
{
#if (!ANC_DEVELOPER_MODE_EN) && ANC_EAR_ADAPTIVE_EN	//非强制开发者模式跟随工具设置
    if (mode != anc_hdl->param.developer_mode) {
        anc_hdl->param.developer_mode = mode;
        anc_ear_adaptive_develop_set(mode);
    }
#endif/*ANC_DEVELOPER_MODE_EN*/
}

/*ANC配置在线读取接口*/
int anc_cfg_online_deal(u8 cmd, anc_gain_t *cfg)
{
    /*同步在线更新配置*/
    anc_gain_param_t *gains = &anc_hdl->param.gains;
    anc_param_fill(cmd, cfg);
    if (cmd == ANC_CFG_WRITE) {
        /*实时更新ANC配置*/
        audio_anc_mic_management(&anc_hdl->param);
        audio_anc_mic_gain(anc_hdl->param.mic_param, 0);
        audio_anc_platform_clk_update();
        if (anc_hdl->param.mode != ANC_OFF) {	//实时更新填入使用
#if ANC_MULT_ORDER_ENABLE
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN && TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
            audio_rtanc_cmp_data_clear();
#endif
            anc_mult_scene_set(anc_hdl->scene_id);	//覆盖增益以及增益符号
#endif/*ANC_MULT_ORDER_ENABLE*/
            audio_anc_reset(&anc_hdl->param, 0);//ANC初始化，不可异步，因为后面会更新ANCIF区域内容
#if ANC_MULT_ORDER_ENABLE
            audio_anc_mult_scene_coeff_free();		//释放空间
#endif/*ANC_MULT_ORDER_ENABLE*/
        }
        /*仅修改增益配置*/
        int ret = anc_db_put(&anc_hdl->param, cfg, NULL);
        if (ret) {
            user_anc_log("ret %d\n", ret);
            anc_hdl->param.lff_coeff = NULL;
            anc_hdl->param.rff_coeff = NULL;
            anc_hdl->param.lfb_coeff = NULL;
            anc_hdl->param.rfb_coeff = NULL;
        };
    }
    return 0;
}

void audio_anc_post_msg_drc(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_DRC_TIMER);
}

void audio_anc_post_msg_debug(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_DEBUG_OUTPUT);
}

void audio_anc_post_msg_icsd_anc_cmd(u8 cmd)
{
    os_taskq_post_msg("anc", 2, ANC_MSG_ICSD_ANC_CMD, cmd);
}

void audio_anc_post_msg_user_train_init(u8 mode)
{
    if (mode == ANC_ON) {
        os_taskq_post_msg("anc", 1, ANC_MSG_USER_TRAIN_INIT);
    }
}

void audio_anc_post_msg_user_train_run(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_USER_TRAIN_RUN);
}

void audio_anc_post_msg_user_train_setparam(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_USER_TRAIN_SETPARAM);
}

void audio_anc_post_msg_user_train_timeout(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_USER_TRAIN_TIMEOUT);
}

void audio_anc_post_msg_user_train_end(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_USER_TRAIN_END);
}

void audio_anc_post_msg_music_dyn_gain(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_MUSIC_DYN_GAIN);
}

#if TCFG_ANC_SELF_DUT_GET_SZ
void audio_anc_post_msg_sz_fft_open(u8 sel)
{
    os_taskq_post_msg("anc", 2, ANC_MSG_SZ_FFT_OPEN, sel);
}

void audio_anc_post_msg_sz_fft_run(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_SZ_FFT_RUN);
}

void audio_anc_post_msg_sz_fft_close(void)
{
    os_taskq_post_msg("anc", 1, ANC_MSG_SZ_FFT_CLOSE);
}

static void audio_anc_sz_fft_dac_check_slience_cb(void *buf, int len)
{
    if (anc_hdl->param.sz_fft.check_flag) {
        s16 *check_buf = (s16 *)buf;
        for (int i = 0; i < len / 2; i++) {
            if (check_buf[i]) {
                anc_hdl->param.sz_fft.check_flag = 0;
                audio_anc_sz_fft_trigger();
                dac_node_write_callback_del("ANC_SZ_FFT");
                break;
            }
        }
    }
}

//SZ_FFT测试函数
void audio_anc_sz_fft_test(void)
{
#if 0
    static u8 cnt = 0;
    g_printf("cnt %d\n", cnt);
    switch (cnt) {
    case 0:
        audio_anc_post_msg_sz_fft_open(4);
        break;
    case 1:
        audio_anc_sz_fft_trigger();
        /* play_tone_file(get_tone_files()->normal); */
        break;
    case 2:
        audio_anc_post_msg_sz_fft_close();
        break;
    case 3:
        audio_anc_sz_fft_outbuf_release();
        break;
    }
    if (++cnt > 3) {
        cnt = 0;
    }
#endif
}

#endif/* TCFG_ANC_SELF_DUT_GET_SZ*/


void audio_anc_dut_enable_set(u8 enablebit)
{
    anc_hdl->param.dut_audio_enablebit = enablebit;
}

void audio_anc_mic_mana_set_gain(audio_anc_t *param, u8 num, u8 type)
{
    switch (type) {
    case ANC_MIC_TYPE_LFF:
        param->mic_param[num].gain = param->gains.l_ffmic_gain;
        break;
    case ANC_MIC_TYPE_LFB:
        param->mic_param[num].gain = param->gains.l_fbmic_gain;
        break;
    case ANC_MIC_TYPE_RFF:
        param->mic_param[num].gain = param->gains.r_ffmic_gain;
        break;
    case ANC_MIC_TYPE_RFB:
        param->mic_param[num].gain = param->gains.r_fbmic_gain;
        break;
    }
}

/*获取对应的mic是否为anc 复用mic, mic_ch ff:0 ; fb:1*/
u8 audio_anc_mic_mult_flag_get(u32 mic_ch)
{
    if (anc_hdl) {
        audio_anc_t *param = &anc_hdl->param;
        for (int i = 0; i <  AUDIO_ADC_MAX_NUM; i++) {
            if ((mic_ch == 0) && param->mic_param[i].en && \
                ((param->mic_param[i].type == ANC_MIC_TYPE_LFF) || \
                 (param->mic_param[i].type == ANC_MIC_TYPE_RFF))) {
                return param->mic_param[i].mult_flag;
            }
            if ((mic_ch == 1) && param->mic_param[i].en && \
                ((param->mic_param[i].type == ANC_MIC_TYPE_LFB) || \
                 (param->mic_param[i].type == ANC_MIC_TYPE_RFB))) {
                return param->mic_param[i].mult_flag;
            }
        }
    }
    return 0;
}

/*设置对应的mic为anc 复用mic, mic_ch ff:0 ; fb:1*/
void audio_anc_mic_mult_flag_set(u32 mic_ch, u8 mult_flag)
{
    int i;
    if (anc_hdl) {
        audio_anc_t *param = &anc_hdl->param;
        for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
            if ((mic_ch == 0) && param->mic_param[i].en && \
                ((param->mic_param[i].type == ANC_MIC_TYPE_LFF) || \
                 (param->mic_param[i].type == ANC_MIC_TYPE_RFF))) {
                param->mic_param[i].mult_flag = mult_flag;
                break;
            }
            if ((mic_ch == 1) && param->mic_param[i].en && \
                ((param->mic_param[i].type == ANC_MIC_TYPE_LFB) || \
                 (param->mic_param[i].type == ANC_MIC_TYPE_RFB))) {
                param->mic_param[i].mult_flag = mult_flag;
                break;
            }
        }
        r_printf("mic_ch %d, mic %d, mult_flag %d", mic_ch, i, param->mic_param[i].mult_flag);
    }
}

void audio_anc_mic_management(audio_anc_t *param)
{
    int i;
    struct adc_file_cfg *cfg = audio_adc_file_get_cfg();
    for (i = 0; i < 4; i++) {
        for (int mic_num = 0; mic_num < AUDIO_ADC_MAX_NUM; mic_num++) {
            if (param->mic_type[i] == mic_num) {
                param->mic_param[mic_num].en = 1;
                param->mic_param[mic_num].type = i;
                if (cfg->mic_en_map & BIT(mic_num)) {
                    param->mic_param[mic_num].mult_flag = 1;
                }
                audio_anc_mic_mana_set_gain(param, mic_num, i);
                break;
            }
        }
#if AUDIO_ANC_PDM_MIC_ENABLE
        if (param->mic_type[i] >= D_MIC0) {
            anc_hdl->pdm_mic_ch |= BIT(param->mic_type[i] - D_MIC0);
        }
#endif
    }
#if AUDIO_ANC_PDM_MIC_ENABLE
    printf("anc dmic_num 0x%x\n", anc_hdl->pdm_mic_ch);
#endif
    for (i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        printf("mic%d en %d, gain %d, type %d, mult_flag %d\n", i, param->mic_param[i].en, \
               param->mic_param[i].gain, param->mic_param[i].type, param->mic_param[i].mult_flag);
    }
}

#if AUDIO_ANC_COEFF_SMOOTH_ENABLE
/*
   ANC 滤波器无缝平滑更新，
   前置条件:1、更新前后的滤波器个数一致
   			2、需在 "非ANC_OFF" 模式下调用
 */
void audio_anc_coeff_smooth_update(void)
{
    /* uint32_t rets_addr; */
    /* __asm__ volatile("%0 = rets ;" : "=r"(rets_addr)); */
    if ((anc_hdl->param.mode != ANC_OFF) && !anc_hdl->mode_switch_lock) {
        /* user_anc_log("audio_anc_coeff_smooth_update %x\n", rets_addr); */
        os_taskq_post_msg("anc", 2, ANC_MSG_COEFF_UPDATE, ANC_COEFF_TYPE_FF | ANC_COEFF_TYPE_FB);	//无缝切换滤波器
    }
}

/*无缝更新FF滤波器*/
void audio_anc_coeff_ff_smooth_update(void)
{
    /* uint32_t rets_addr; */
    /* __asm__ volatile("%0 = rets ;" : "=r"(rets_addr)); */
    if ((anc_hdl->param.mode != ANC_OFF) && !anc_hdl->mode_switch_lock) {
        /* user_anc_log("audio_anc_coeff_smooth_update %x\n", rets_addr); */
        os_taskq_post_msg("anc", 2, ANC_MSG_COEFF_UPDATE, ANC_COEFF_TYPE_FF);	//无缝切换滤波器
    }
}

/*无缝更新FB/CMP滤波器*/
void audio_anc_coeff_fb_smooth_update(void)
{
    /* uint32_t rets_addr; */
    /* __asm__ volatile("%0 = rets ;" : "=r"(rets_addr)); */
    if ((anc_hdl->param.mode != ANC_OFF) && !anc_hdl->mode_switch_lock) {
        /* user_anc_log("audio_anc_coeff_smooth_update %x\n", rets_addr); */
        os_taskq_post_msg("anc", 2, ANC_MSG_COEFF_UPDATE, ANC_COEFF_TYPE_FB);	//无缝切换滤波器
    }
}
#endif

void audio_anc_coeff_fade_set(u8 coeff_type, u8 fade_fast, u8 fade_slow)
{
    if (anc_hdl) {
        switch (coeff_type) {
        case ANC_COEFF_TYPE_FF:
            anc_hdl->param.ff_filter_fade_fast = fade_fast;
            anc_hdl->param.ff_filter_fade_slow = fade_slow;
            break;
        case ANC_COEFF_TYPE_FB:
            anc_hdl->param.fb_filter_fade_fast = fade_fast;
            anc_hdl->param.fb_filter_fade_slow = fade_slow;
            break;
        }
    }
}

/*
   ANC 驱动复位（包括滤波器），支持淡入淡出
   前置条件：需在 "非ANC_OFF" 模式下调用;
   param: fade_en 1 开启淡入淡出，会有一定执行时间；
   				  0 关闭淡入淡出，会有po声；
 */
void audio_anc_param_reset(u8 fade_en)
{
    if ((anc_hdl->param.mode != ANC_OFF) && !anc_hdl->mode_switch_lock) {
        os_taskq_post_msg("anc", 2, ANC_MSG_RESET, fade_en);
    }
}

/*
 	设置ANC模式变量，仅在耳道自适应启动使用，使用ANC_ON的模式调用对应的库接口
 */
void audio_anc_mode_set(u8 mode)
{
    if (anc_hdl && (strcmp(os_current_task(), "anc") == 0)) {
        anc_hdl->param.mode = mode;
        anc_hdl->new_mode = mode;
    } else {
        user_anc_log("anc_mode_set err ,task %s\n", os_current_task());
    }
}

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
static int audio_anc_adt_prepare(u8 mode)
{
    u8 trans_alogm = audio_anc_gains_alogm_get(ANC_TRANS_TYPE);
    if (trans_alogm != 5) {
        ASSERT(0, "ERR!!ANC ADT trans_alogm %d != 5\n", trans_alogm);
    }

    audio_icsd_adt_scene_set(ADT_SCENE_ANC_OFF, (mode == ANC_OFF) || (mode == ANC_EXT));
    audio_icsd_adt_scene_set(ADT_SCENE_ANC_TRANS, (mode == ANC_TRANSPARENCY));

    if (mode == ANC_OFF) {
        //存在ADT 功能支持ANC_OFF场景，修改为ANC_EXT模式
        if (audio_icsd_adt_scene_check(icsd_adt_app_mode_get(), 0)) {
            user_anc_log("ANC: anc mode change : ANC_OFF->ANC_EXT\n");
            anc_hdl->param.mode = ANC_EXT;
            anc_hdl->new_mode = ANC_EXT;
        }
    }
    audio_icsd_adt_algom_suspend();
    return 0;
}

static int audio_anc_adt_reset(u8 mode)
{
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    //保护免摘现场
    u8 keep_state = 0;
    if (audio_vdt_keep_state_get() == AUDIO_VDT_KEEP_STATE_PREPARE) {
        keep_state = 1;
        audio_vdt_keep_state_send_to_adt(AUDIO_VDT_KEEP_STATE_START);
    }
#endif

    if (anc_hdl->adt_open) {
        anc_hdl->adt_open = 0;
        audio_anc_switch_adt_app_close();
    }
    /* if (rtanc_in_anc_off || (cur_anc_mode == ANC_ON) || (cur_anc_mode == ANC_TRANSPARENCY)) { */
    if (mode != ANC_OFF) {
        anc_hdl->adt_open = 1;
        audio_anc_switch_adt_app_open();
    }
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    if (keep_state) {
        audio_vdt_keep_state_send_to_adt(AUDIO_VDT_KEEP_STATE_STOP);
    }
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (anc_hdl->param.mode == ANC_OFF) {
        audio_anc_real_time_adaptive_resume("MODE_SWITCH");
    } else {
        if (anc_hdl->switch_rtanc_resume_id) {
            sys_timer_del(anc_hdl->switch_rtanc_resume_id);
            anc_hdl->switch_rtanc_resume_id = 0;
        }
        anc_hdl->switch_rtanc_resume_id = sys_timeout_add(NULL, audio_anc_switch_rtanc_resume_timeout, 200);
    }
#endif
    return 0;
}
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/

#if ANC_EAR_ADAPTIVE_EN
void audio_ear_adaptive_en_set(u8 en)
{
    if (anc_hdl) {
        anc_hdl->ear_adaptive = en;
    }
}

/*耳道自适应互斥功能挂起*/
void audio_ear_adaptive_train_app_suspend(void)
{
    anc_mode_change_tool(ANC_FF_EN);					//设置仅FF通路
    audio_anc_drc_toggle_set(0);						//关闭DRC
    audio_anc_dcc_adaptive_toggle_set(0);				//关闭DCC
#if ANC_HOWLING_DETECT_EN
    anc_howling_detect_app_suspend();
#endif
#if ANC_ADAPTIVE_EN
    //关闭场景自适应功能
    audio_anc_power_adaptive_suspend();
#endif
}

/*耳道自适应互斥功能恢复*/
void audio_ear_adaptive_train_app_resume(void)
{
    anc_mode_change_tool(TCFG_AUDIO_ANC_TRAIN_MODE);
    audio_anc_drc_toggle_set(1);
    audio_anc_dcc_adaptive_toggle_set(1);
#if ANC_HOWLING_DETECT_EN
    anc_howling_detect_app_resume();
#endif
#if ANC_ADAPTIVE_EN
    //恢复场景自适应功能
    audio_anc_power_adaptive_resume();
#endif
}
#endif/*ANC_EAR_ADAPTIVE_EN*/

#if ANC_MULT_ORDER_ENABLE

int audio_anc_mult_scene_set(u16 scene_id)
{
    user_anc_log(" audio_anc_mult scene_id set %d\n", scene_id);
    if (audio_anc_mult_scene_id_check(scene_id)) {
        user_anc_log("anc_mult_scene id set err id = %d", scene_id);
        return 1;
    }
    anc_hdl->scene_id = scene_id;
#if ANC_EAR_ADAPTIVE_EN
    if (anc_ear_adaptive_busy_get()) {
        anc_ear_adaptive_forced_exit(1, 0);
        return 1;
    }
    //非自适应训练状态，切场景自动切回普通参数
    anc_hdl->param.anc_coeff_mode = ANC_COEFF_MODE_NORMAL;
#endif/*ANC_EAR_ADAPTIVE_EN*/
    if ((anc_hdl->param.mode != ANC_OFF) && !anc_hdl->mode_switch_lock) {
        anc_mult_scene_set(scene_id);
    }
    return 0;
}

int audio_anc_mult_scene_update(u16 scene_id)
{
    int ret = audio_anc_mult_scene_set(scene_id);
    if (!ret) {
#if AUDIO_ANC_COEFF_SMOOTH_ENABLE
        audio_anc_coeff_smooth_update();
#else
        audio_anc_param_reset(1);
#endif
    }
    return ret;
}

/*多滤波器-场景ID TWS同步*/
void audio_anc_mult_scene_id_sync(u16 scene_id)
{
    anc_hdl->scene_id = scene_id;
}

u8 audio_anc_mult_scene_get(void)
{
    return anc_hdl->scene_id;
}

void audio_anc_mult_scene_max_set(u8 scene_cnt)
{
    anc_hdl->scene_max = scene_cnt;
}

u8 audio_anc_mult_scene_max_get(void)
{
    return anc_hdl->scene_max;
}

void audio_anc_mult_scene_switch(u8 tone_flag)
{
    u8 cnt = audio_anc_mult_scene_get();
    if (cnt < audio_anc_mult_scene_max_get()) {
        cnt++;
    } else {
        cnt = 1;
    }
    if (tone_flag) {
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() != TWS_ROLE_SLAVE) {
            tws_play_tone_file(get_tone_files()->num[cnt], 400);
        }
#else
        play_tone_file(get_tone_files()->num[cnt]);
#endif
        /* tone_play_index(IDEX_TONE_NUM_0 + cnt, 0); */
    }
    audio_anc_mult_scene_update(cnt);
}
#endif/*ANC_MULT_ORDER_ENABLE*/

void audio_anc_howldet_fade_set(u16 gain)
{
    if (anc_hdl) {
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        //触发啸叫检测之后需要挂起RTANC
        if (gain != 16384) {
            if (!anc_hdl->howldet_suspend_rtanc) {
                anc_hdl->howldet_suspend_rtanc = 1;
                user_anc_log("ANC_HD_STATE:TRIGGER, [anc_fade]\n");
                audio_anc_real_time_adaptive_suspend("ANC_HOWL_DET");
            }
        } else if (anc_hdl->howldet_suspend_rtanc) {
            anc_hdl->howldet_suspend_rtanc = 0;
            user_anc_log("ANC_HD_STATE:RESUME, [anc_fade]\n");
            audio_anc_real_time_adaptive_resume("ANC_HOWL_DET");
        }
#endif
        audio_anc_fade_ctr_set(ANC_FADE_MODE_HOWLDET, AUDIO_ANC_FDAE_CH_ALL, gain);
    }
}

//跟踪ANC滤波器修改情况
void audio_anc_coeff_check_crc(u8 from)
{
    uint32_t rets_addr;
    __asm__ volatile("%0 = rets ;" : "=r"(rets_addr));
    audio_anc_t *param = &anc_hdl->param;
    u16 crc[3] = {0};
    int gain[3] = {0};
    u8 num[3] = {0};
    u8 sign = param->gains.gain_sign;
    user_anc_log("ANC_CHECK:%s, from %d\n", anc_mode_str[anc_mode_get()], from);
    if (anc_mode_get() == ANC_TRANSPARENCY) {
#if ANC_MULT_TRANS_FB_ENABLE
        if (!param->ltrans_coeff || !param->ltrans_fb_coeff || !param->ltrans_cmp_coeff) {
            user_anc_log("ANC_CHECK:ERR! rets:%x, %p, %p, %p\n", rets_addr, param->ltrans_coeff, param->ltrans_fb_coeff, param->ltrans_cmp_coeff);
            ASSERT(0);
            return;
        }
        num[0] = param->ltrans_yorder;
        num[1] = param->ltrans_fb_yorder;
        num[2] = param->ltrans_cmp_yorder;
        crc[0] = CRC16(param->ltrans_coeff, num[0] * 40);
        crc[1] = CRC16(param->ltrans_fb_coeff, num[1] * 40);
        crc[2] = CRC16(param->ltrans_cmp_coeff, num[2] * 40);
        gain[0] = (int)(param->gains.l_transgain * 100.0f) * ((sign & ANCL_TRANS_SIGN) ? (-1) : 1);
        gain[1] = (int)(param->ltrans_fbgain * 100.0f);
        gain[2] = (int)(param->ltrans_cmpgain * 100.0f);
#else

        if (!param->ltrans_coeff) {
            user_anc_log("ANC_CHECK:ERR! rets:%x, %p\n", rets_addr, param->ltrans_coeff);
            ASSERT(0);
            return;
        }
        num[0] = param->ltrans_yorder;
        crc[0] = CRC16(param->ltrans_coeff, num[0] * 40);
        gain[0] = (int)(param->gains.l_transgain * 100.0f) * ((sign & ANCL_TRANS_SIGN) ? (-1) : 1);
#endif
    } else {
        if (!param->lff_coeff || !param->lfb_coeff || !param->lcmp_coeff) {
            user_anc_log("ANC_CHECK:ERR! rets:%x, %p, %p, %p\n", rets_addr, param->lff_coeff, param->lfb_coeff, param->lcmp_coeff);
            ASSERT(0);
            return;
        }
        num[0] = param->lff_yorder;
        num[1] = param->lfb_yorder;
        num[2] = param->lcmp_yorder;
        crc[0] = CRC16(param->lff_coeff, num[0] * 40);
        crc[1] = CRC16(param->lfb_coeff, num[1] * 40);
        crc[2] = CRC16(param->lcmp_coeff, num[2] * 40);
        gain[0] = (int)(param->gains.l_ffgain * 100.0f) * ((sign & ANCL_FF_SIGN) ? (-1) : 1);
        gain[1] = (int)(param->gains.l_fbgain * 100.0f) * ((sign & ANCL_FB_SIGN) ? (-1) : 1);
        gain[2] = (int)(param->gains.l_cmpgain * 100.0f) * ((sign & ANCL_CMP_SIGN) ? (-1) : 1);
    }
    user_anc_log("ANC_CHECK:rets:%x, FF %x-%2d-%3d/100; fb %x-%2d-%3d/100; cmp %x-%2d-%3d/100\n", \
                 rets_addr, crc[0], num[0], gain[0], crc[1], num[1], gain[1], crc[2], num[2], gain[2]);
}

//记录用户端最后一个模式, 如遇切模式锁存，会生效此模式
void audio_anc_switch_latch_mode_set(u8 mode)
{
    if (anc_hdl) {
        //printf("audio_anc_switch_latch_mode_set : %d\n", mode);
        anc_hdl->switch_latch_mode = mode;
    }
}

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
static void audio_anc_tone_adaptive_timeout(void *priv)
{
    audio_anc_real_time_adaptive_resume("TONE_ADAPTIVE_SYNC");
    anc_hdl->tone_adaptive_sync_id = 0;
}

void audio_anc_tone_adaptive_disable(u8 device)
{
    //device  0  本地触发； device 1 远端触发
    if (audio_anc_production_mode_get()) {
        return;
    }
    anc_hdl->tone_adaptive_device = device;
    user_anc_log("====audio_anc_tone_adaptive_disable device:%d\n", device);
    // anc_mode_enable_set(0);
    if (anc_hdl->tone_adaptive_sync_id) {
        sys_timer_del(anc_hdl->tone_adaptive_sync_id);
        anc_hdl->tone_adaptive_sync_id = 0;
    }
    anc_hdl->tone_adaptive_suspend = 1;
    audio_anc_fade_ctr_set(ANC_FADE_MODE_ADAPTIVE_TONE, AUDIO_ANC_FDAE_CH_ALL, 0);
    audio_anc_real_time_adaptive_suspend("TONE_ADAPTIVE_SYNC");

}

void audio_anc_tone_adaptive_enable(u8 device)
{
    if (((anc_hdl->tone_adaptive_device == device) || device == 0XFF) && anc_hdl->tone_adaptive_suspend) {
        anc_hdl->tone_adaptive_suspend = 0;
        user_anc_log("====audio_anc_tone_adaptive_enable device: 0x%x\n", device);
#if TCFG_AUDIO_FREQUENCY_GET_ENABLE
        audio_afq_inear_state_set(0);
#endif
        audio_anc_fade_ctr_set(ANC_FADE_MODE_ADAPTIVE_TONE, AUDIO_ANC_FDAE_CH_ALL, 16384);
        anc_hdl->tone_adaptive_sync_id = sys_timeout_add(NULL, audio_anc_tone_adaptive_timeout, 400);
    }
}

static void audio_anc_switch_rtanc_resume_timeout(void *priv)
{
    anc_hdl->switch_rtanc_resume_id = 0;
    audio_anc_real_time_adaptive_resume("MODE_SWITCH");
}
#endif

#if (TCFG_AUDIO_ANC_ENV_ENABLE || TCFG_AUDIO_ANC_AVC_ENABLE)
static int audio_anc_env_enable_check()
{
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
    return (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE);
#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE
    return 1;
#else
    return 0;
#endif
}

static int audio_anc_avc_enable_check()
{
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
    return (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE);
#elif TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE
    return 1;
#else
    return 0;
#endif
}

void audio_anc_env_avc_thr_to_lvl_sync(int noise_thr)
{
    int env_lvl = -1;
    int avc_lvl = -1;
    noise_thr = (noise_thr < 0) ? 0 : noise_thr;
    if (audio_anc_env_enable_check()) {
        env_lvl = audio_anc_env_thr_to_lvl(noise_thr);
        struct audio_anc_lvl_sync *env_hdl = audio_anc_lvl_sync_get_hdl_by_name(ANC_LVL_SYNC_ENV);
        if (env_hdl) {
            user_anc_log("env cur_lvl %d, env_lvl %d\n", env_hdl->cur_lvl, env_lvl);
            if (env_hdl->cur_lvl == env_lvl) {
                env_lvl = -1;
            }
        }
    }
    if (audio_anc_avc_enable_check()) {
        avc_lvl = audio_anc_avc_thr_to_lvl(noise_thr);
        struct audio_anc_lvl_sync *avc_hdl = audio_anc_lvl_sync_get_hdl_by_name(ANC_LVL_SYNC_AVC);
        if (avc_hdl) {
            user_anc_log("avc cur_lvl %d, avc_lvl %d\n", avc_hdl->cur_lvl, avc_lvl);
            if (avc_hdl->cur_lvl == avc_lvl) {
                avc_lvl = -1;
            }
        }
    }
    if ((avc_lvl == -1) && (env_lvl == -1)) {
        return;
    }
    u8 data[4] = {AUDIO_ANC_LVL_SYNC_CMP, (u8)env_lvl, 0, 0};
    static u32 next_period = 0;
    /*间隔200ms以上发送一次数据*/
    if (time_after(jiffies, next_period)) {
        next_period = jiffies + msecs_to_jiffies(200);
#if TCFG_USER_TWS_ENABLE
        if (tws_in_sniff_state()) {
            /*如果在蓝牙siniff下需要退出蓝牙sniff再发送*/
            tws_api_tx_unsniff_req();
        }
        if (get_tws_sibling_connect_state()) {
            if (audio_anc_env_enable_check() && (env_lvl != -1)) {
                //环境自适应
                /* user_anc_log("[env]SYNC_ICSD_ADT_ENV_NOISE_LVL_CMP %d\n", env_lvl); */
                data[0] = AUDIO_ANC_LVL_SYNC_CMP;
                data[1] = env_lvl;
                audio_anc_env_lvl_sync_info(data, 4);
            }
            if (audio_anc_avc_enable_check() && (avc_lvl != -1)) {
                //音量自适应
                /* user_anc_log("[avc]SYNC_ICSD_ADT_AVC_NOISE_LVL_CMP %d\n", avc_lvl); */
                data[0] = AUDIO_ANC_LVL_SYNC_CMP;
                data[1] = avc_lvl;
                audio_anc_avc_lvl_sync_info(data, 4);
            }
        } else {
            /*没有tws时直接更新状态*/
            if (audio_anc_env_enable_check() && (env_lvl != -1)) {
                data[0] = AUDIO_ANC_LVL_SYNC_RESULT;
                data[1] = env_lvl;
                audio_anc_env_lvl_sync_info(data, 4);
            }
            if (audio_anc_avc_enable_check() && (avc_lvl != -1)) {
                data[0] = AUDIO_ANC_LVL_SYNC_RESULT;
                data[1] = avc_lvl;
                audio_anc_avc_lvl_sync_info(data, 4);
            }
        }
#else
        if (audio_anc_env_enable_check() && (env_lvl != -1)) {
            data[0] = AUDIO_ANC_LVL_SYNC_RESULT;
            data[1] = env_lvl;
            audio_anc_env_lvl_sync_info(data, 4);
        }
        if (audio_anc_avc_enable_check() && (avc_lvl != -1)) {
            data[0] = AUDIO_ANC_LVL_SYNC_RESULT;
            data[1] = avc_lvl;
            audio_anc_avc_lvl_sync_info(data, 4);
        }
#endif //TCFG_USER_TWS_ENABLE
    }
}
#endif

#if AUDIO_ANC_MIC_ARRAY_ENABLE
void audio_anc_stereo_mix_set(u8 en)
{
    anc_hdl->stereo_to_mono_mix = en;
}

u8 audio_anc_stereo_mix_get(void)
{
    return anc_hdl->stereo_to_mono_mix;
}

static void audio_anc_stereo_mix_ctr(void)
{
    if (anc_hdl->stereo_to_mono_mix && (anc_mode_get() == ANC_ON)) {
        anc_hdl->param.stereo_to_mono_mix = 1;
    } else {
        anc_hdl->param.stereo_to_mono_mix = 0;
    }
}
#endif

#endif/*TCFG_AUDIO_ANC_ENABLE*/
