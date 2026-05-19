#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_anc_common_plug.data.bss")
#pragma data_seg(".audio_anc_common_plug.data")
#pragma const_seg(".audio_anc_common_plug.text.const")
#pragma code_seg(".audio_anc_common_plug.text")
#endif

/*
 ****************************************************************
 *							AUDIO ANC COMMON PLUG
 * File  : audio_anc_comon_plug.c
 * By    :
 * Notes : 存放ANC共用插件
 *
 *
 ****************************************************************
 */

#include "app_config.h"

#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc_includes.h"
#endif
#include "dac_node.h"
#include "jlstream.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#include "sniff.h"
#endif/*TCFG_USER_TWS_ENABLE*/

#if 0
#define anc_plug_log	printf
#else
#define anc_plug_log(...)
#endif/*log_en*/

/********************************************************
	   				ANC 防破音
********************************************************/
#if ANC_MUSIC_DYNAMIC_GAIN_EN

#include "amplitude_statistic.h"

enum {
    ANC_MUSIC_DYN_STATE_STOP = 0,
    ANC_MUSIC_DYN_STATE_START,
    ANC_MUSIC_DYN_STATE_SUSPEND,
};

struct anc_music_dyn_gain_t {
    u8  state;
    char loud_dB;					/*音乐动态增益-当前音乐幅值*/
    s16 loud_nowgain;				/*音乐动态增益-当前增益*/
    s16 loud_targetgain;			/*音乐动态增益-目标增益*/
    u16 loud_timeid;				/*音乐动态增益-定时器ID*/
    s16 loud_trigge_thr;			/*音乐动态增益-触发阈值*/
    LOUDNESS_M_STRUCT loud_hdl;		/*音乐动态增益-操作句柄*/
};

#define ANC_MUSIC_DYNAMIC_GAIN_NORGAIN				1024	/*正常默认增益*/
#define ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL		6		/*音乐响度取样间隔*/

extern struct audio_dac_hdl dac_hdl;
static struct anc_music_dyn_gain_t *music_dyn_hdl = NULL;

static void audio_anc_music_dynamic_gain_dac_node_state_cb(struct dac_node_cb_hdl *hdl);
static void audio_anc_music_dynamic_gain_dac_node_write_cb(void *buf, int len);

void audio_anc_music_dynamic_gain_init(void)
{
    music_dyn_hdl = zalloc(sizeof(struct anc_music_dyn_gain_t));
    music_dyn_hdl->loud_nowgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
    music_dyn_hdl->loud_targetgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
    music_dyn_hdl->loud_dB = 1;
    dac_node_callback_add("ANC_MUSIC_DYN", 0XFF, \
                          audio_anc_music_dynamic_gain_dac_node_state_cb, \
                          audio_anc_music_dynamic_gain_dac_node_write_cb);
}

#if ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB
static void audio_anc_music_dyn_gain_resume_timer(void *priv)
{
    if (music_dyn_hdl->loud_nowgain < music_dyn_hdl->loud_targetgain) {
        music_dyn_hdl->loud_nowgain += 5;      ///+5耗时194 * 10ms
        if (music_dyn_hdl->loud_nowgain >= music_dyn_hdl->loud_targetgain) {
            music_dyn_hdl->loud_nowgain = music_dyn_hdl->loud_targetgain;
            sys_timer_del(music_dyn_hdl->loud_timeid);
            music_dyn_hdl->loud_timeid = 0;
        }
    } else if (music_dyn_hdl->loud_nowgain > music_dyn_hdl->loud_targetgain) {
        music_dyn_hdl->loud_nowgain -= 5;      ///-5耗时194 * 10ms
        if (music_dyn_hdl->loud_nowgain <= music_dyn_hdl->loud_targetgain) {
            music_dyn_hdl->loud_nowgain = music_dyn_hdl->loud_targetgain;
            sys_timer_del(music_dyn_hdl->loud_timeid);
            music_dyn_hdl->loud_timeid = 0;
        }
    }
    audio_anc_fbgain_set(music_dyn_hdl->loud_nowgain, music_dyn_hdl->loud_nowgain);
    anc_plug_log("fb resume music_dyn_hdl->loud_nowgain %d\n", music_dyn_hdl->loud_nowgain);
}

#endif/*ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB*/

void audio_anc_music_dynamic_gain_process(void)
{
    if (music_dyn_hdl->loud_nowgain != music_dyn_hdl->loud_targetgain) {
        anc_plug_log("low_nowgain %d, targergain %d\n", music_dyn_hdl->loud_nowgain, music_dyn_hdl->loud_targetgain);
#if ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB
        if (!music_dyn_hdl->loud_timeid) {
            music_dyn_hdl->loud_timeid = sys_timer_add(NULL, audio_anc_music_dyn_gain_resume_timer, 10);
        }
#else
        music_dyn_hdl->loud_nowgain = music_dyn_hdl->loud_targetgain;
        audio_anc_fade_ctr_set(ANC_FADE_MODE_MUSIC_DYNAMIC, AUDIO_ANC_FDAE_CH_FB, music_dyn_hdl->loud_targetgain << 4);
#endif/*ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB*/
    }
}

void audio_anc_music_dynamic_gain_reset(void)
{
    if (music_dyn_hdl->loud_targetgain != ANC_MUSIC_DYNAMIC_GAIN_NORGAIN) {
        anc_plug_log("audio_anc_music_dynamic_gain_reset\n");
        music_dyn_hdl->loud_targetgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
        if (music_dyn_hdl->loud_timeid) {
            sys_timer_del(music_dyn_hdl->loud_timeid);
            music_dyn_hdl->loud_timeid = 0;
        }
        music_dyn_hdl->loud_dB = 1;	//清除上次保留的dB值, 避免切模式后不触发的情况
        if (anc_mode_get() == ANC_ON) {
            audio_anc_post_msg_music_dyn_gain();
        } else {
            music_dyn_hdl->loud_nowgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
            music_dyn_hdl->loud_hdl.peak_val = music_dyn_hdl->loud_trigge_thr;
#if !ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB
            audio_anc_fade_ctr_set(ANC_FADE_MODE_MUSIC_DYNAMIC, AUDIO_ANC_FDAE_CH_FB, music_dyn_hdl->loud_targetgain << 4);
#endif
        }
    }
}

void audio_anc_music_dynamic_fb_gain_reset(void)
{
    audio_anc_fbgain_set(music_dyn_hdl->loud_nowgain, music_dyn_hdl->loud_nowgain);
}

static void audio_anc_music_dynamic_gain_dB_get(int dB)
{
    float mult = 1.122f;
    float target_mult = 1.0f;
    int cnt, i, gain;
#if ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL > 1 //区间变化，避免频繁操作
    if (dB > music_dyn_hdl->loud_trigge_thr) {
        int temp_dB = dB / ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL;
        dB = temp_dB * ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL;
    }
#endif/*ANC_MUSIC_DYNAMIC_GAIN_TRIGGER_INTERVAL*/
    if (music_dyn_hdl->loud_dB != dB) {
        music_dyn_hdl->loud_dB = dB;
        if (dB > music_dyn_hdl->loud_trigge_thr) {
            cnt = dB - music_dyn_hdl->loud_trigge_thr;
            for (i = 0; i < cnt; i++) {
                target_mult *= mult;
            }
            gain = (int)((float)ANC_MUSIC_DYNAMIC_GAIN_NORGAIN / target_mult);
//            anc_plug_log("cnt %d, dB %d, gain %d\n", cnt, dB, gain);
            music_dyn_hdl->loud_targetgain = gain;
            audio_anc_post_msg_music_dyn_gain();
        } else {
            audio_anc_music_dynamic_gain_reset();
        }
    }
}

void audio_anc_music_dynamic_gain_det(void *data, int len)
{
    /* putchar('l'); */
    if (anc_mode_get() == ANC_ON) {
        if (dac_hdl.pd->bit_width) {
            loudness_meter_24bit(&music_dyn_hdl->loud_hdl, (int *)data, len >> 2);
        } else {
            loudness_meter_short(&music_dyn_hdl->loud_hdl, (s16 *)data, len >> 1);
        }
        audio_anc_music_dynamic_gain_dB_get(music_dyn_hdl->loud_hdl.peak_val);
    }
}

void audio_anc_music_dynamic_gain_open(int sr, s16 thr)
{
    anc_plug_log("audio_anc_music_dynamic_gain_open %d\n", sr);
    music_dyn_hdl->loud_nowgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
    music_dyn_hdl->loud_targetgain = ANC_MUSIC_DYNAMIC_GAIN_NORGAIN;
    music_dyn_hdl->loud_dB = 1;     //清除上次保留的dB值
    music_dyn_hdl->loud_trigge_thr = thr;

    loudness_meter_init(&music_dyn_hdl->loud_hdl, sr, 50, 0);
    music_dyn_hdl->loud_hdl.peak_val = thr;
}

//ANC音乐动态增益支持的场景
const u8 support_scene[] = {
    STREAM_SCENE_A2DP,
    STREAM_SCENE_LINEIN,
    STREAM_SCENE_PC_SPK,
};

static void audio_anc_music_dynamic_gain_dac_node_state_cb(struct dac_node_cb_hdl *hdl)
{
    u8 support = 0;
    //筛选支持的场景
    for (int i = 0; i < sizeof(support_scene); i++) {
        if (hdl->scene == support_scene[i]) {
            anc_plug_log("anc music dyn support scene %x, state %x\n", hdl->scene, hdl->state);
            support = 1;
        }
    }
    if (hdl->state == DAC_NODE_STATE_START) {
        if ((music_dyn_hdl->state == ANC_MUSIC_DYN_STATE_STOP) && support) {
            audio_anc_music_dynamic_gain_open(hdl->sample_rate, TCFG_ANC_MUSIC_DYNAMIC_GAIN_THR);
            music_dyn_hdl->state = ANC_MUSIC_DYN_STATE_START;
        }
    } else if (hdl->state == DAC_NODE_STATE_STOP) {
        if ((music_dyn_hdl->state == ANC_MUSIC_DYN_STATE_START) && support) {
            audio_anc_music_dynamic_gain_reset();
            music_dyn_hdl->state = ANC_MUSIC_DYN_STATE_STOP;
        }
    }
}

static void audio_anc_music_dynamic_gain_dac_node_write_cb(void *buf, int len)
{
    if (music_dyn_hdl->state == ANC_MUSIC_DYN_STATE_START) {
        audio_anc_music_dynamic_gain_det(buf, len);
    }
}

void audio_anc_music_dynamic_gain_suspend(void)
{
    if (music_dyn_hdl) {
        music_dyn_hdl->state = ANC_MUSIC_DYN_STATE_SUSPEND;
        audio_anc_music_dynamic_gain_reset();
    }
}


#endif/*ANC_MUSIC_DYNAMIC_GAIN_EN*/


/********************************************************
	   				场景自适应
********************************************************/
#if ANC_ADAPTIVE_EN

struct anc_power_adaptive_hdl {
    u8 now_ref_lvl;			/*场景自适应FF当前等级*/
    u8 now_err_lvl;			/*场景自适应FB当前等级*/
    u8 suspend;				/*挂起场景自适应标志*/
    u8 suspend_state;		/*场景自适应挂起状态*/
    audio_anc_t *param;		/*ANC操作句柄*/
};
struct anc_power_adaptive_hdl *power_adaptive_hdl = NULL;

/*
	自适应增益等级系数表
   1、更多等级需求，则继续添加该结构体数组成员
   2、注意阈值需从大到小排列
   3、前等级的pow_lthr与后等级的pow_thr之间需要有隔离带，
      否则处于临界范围，会频繁切换等级
 */
anc_adap_param_t anc_power_adaptive_param[] = {
    //等级0
    {
        .pow_thr = 0XFFFFFFFF,		//当前等级能量高阈值
        .default_flag = 1,			//默认等级标志
        .gain = 1024,               //当前等级对应增益 range [0-1024]
        .gain_decrease_fade_time = 10,    //(高->低)高增益等级 向当前等级淡入时间*100ms, range [1 - 200]*/
        .gain_increase_fade_time = 10,    //(低->高)低增益等级 向当前等级淡入时间*100ms, range [1 - 200]*/
        .pow_lthr = 700,			//当前等级能量低阈值，与MIC GAIN相关, 此默认值 MIC_GAIN = 4;
    },
    //等级1
    {
        .pow_thr = 200,
        .gain = 350,
        .gain_decrease_fade_time = 10,
        .gain_increase_fade_time = 10,
        .pow_lthr = 0,
    },
};

static void audio_anc_power_adaptive_fade(u16 gain);
static void audio_anc_power_adaptive_event_cb(u8 lvl, enum ANC_ADAPTIVE_FADE_EVENT event);

void audio_anc_power_adaptive_init(audio_anc_t *param)
{
    anc_plug_log("%s\n", __func__);
    struct anc_power_adaptive_cfg cfg;
    power_adaptive_hdl = zalloc(sizeof(struct anc_power_adaptive_hdl));

    //流程参数初始化
    power_adaptive_hdl->param = param;
    power_adaptive_hdl->param->adaptive_mode = ANC_ADAPTIVE_MODE;
    power_adaptive_hdl->now_ref_lvl = 0xff;
    power_adaptive_hdl->now_err_lvl = 0xff;

#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE
    audio_adt_env_ioctl(ANC_EXT_IOC_INIT, 0);
#endif
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE
    audio_adt_avc_ioctl(ANC_EXT_IOC_INIT, 0);
#endif

    //算法参数初始化
    cfg.det_time = ANC_ADAPTIVE_POWER_DET_TIME;
    cfg.fade_gain_set = audio_anc_power_adaptive_fade;
    cfg.event_cb = audio_anc_power_adaptive_event_cb;
    cfg.ref_param = anc_power_adaptive_param;
    cfg.ref_max_lvl = sizeof(anc_power_adaptive_param) / sizeof(anc_adap_param_t);
    cfg.err_param = anc_power_adaptive_param;
    cfg.err_max_lvl = sizeof(anc_power_adaptive_param) / sizeof(anc_adap_param_t);
    cfg.param = param;
#if (TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_LITE_ENABLE || TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_LITE_ENABLE)
    cfg.pow_output_cb = audio_anc_env_avc_thr_to_lvl_sync;
#else
    cfg.pow_output_cb = NULL;
#endif
    anc_pow_adap_init(&cfg);
}

#if TCFG_USER_TWS_ENABLE
#define TWS_FUNC_ID_ANC_POWER_ADAPTIVE_SYNC    		 TWS_FUNC_ID('A', 'N', 'C', 'A')
static void bt_tws_anc_power_adaptive_sync(int dat, int err)
{
    /* printf("%s,%d, lvl 0X%x\n", __func__, __LINE__, dat); */
    u8 ref_lvl = (dat >> 8);
    u8 err_lvl = (dat & 0XFF);
    u8 sync_flag = 0;
    //避免相同等级重复切换
    if (power_adaptive_hdl->now_ref_lvl != ref_lvl) {
        power_adaptive_hdl->now_ref_lvl = ref_lvl;
        sync_flag = 1;
    }
    if (sync_flag) {
        os_taskq_post_msg("anc", 3, ANC_MSG_ADAPTIVE_SYNC, ref_lvl, err_lvl);
    }
}

TWS_SYNC_CALL_REGISTER(tws_anc_adaptive_sync) = {
    .uuid = TWS_FUNC_ID_ANC_POWER_ADAPTIVE_SYNC,
    .task_name = "anc",
    .func = bt_tws_anc_power_adaptive_sync,
};

#define TWS_FUNC_ID_ANC_POWER_ADAPTIVE_COMPARE_SYNC    TWS_FUNC_ID('A', 'N', 'C', 'B')
static void bt_tws_anc_power_adaptive_compare(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        /* r_printf("tws_anc_power_adaptive_compare: %d, %d\n", data[0], data[1]); */
        u8 ref_lvl = data[0];
        u8 err_lvl = data[1];
        int ref_lvl_temp;
        ref_lvl_temp = anc_pow_adap_lvl_get(ANC_FF_EN);
        g_printf("compare ref_lvl_temp %d, lvl %d \n", ref_lvl_temp, ref_lvl);
        if (ref_lvl_temp >= ref_lvl) {
            tws_api_sync_call_by_uuid(TWS_FUNC_ID_ANC_POWER_ADAPTIVE_SYNC, ((ref_lvl << 8) | err_lvl), 150);
        }
    }
}

REGISTER_TWS_FUNC_STUB(tws_anc_power_adaptive_compare) = {
    .func_id = TWS_FUNC_ID_ANC_POWER_ADAPTIVE_COMPARE_SYNC,
    .func    = bt_tws_anc_power_adaptive_compare,
};

static int bt_tws_anc_power_adaptive_compare_send(int ref_lvl, int err_lvl)
{
    u8 data[2];
    int ret;
    data[0] = ref_lvl;
    data[1] = err_lvl;
    /* r_printf("tws_anc_power_adaptive_sync: %d, %d\n", data[0], data[1]); */
    local_irq_disable();
    ret = tws_api_send_data_to_sibling(data, 2, TWS_FUNC_ID_ANC_POWER_ADAPTIVE_COMPARE_SYNC);
    local_irq_enable();
    return ret;
}
#endif

void audio_anc_power_adaptive_tone_play(int ref_lvl, int err_lvl)
{
    anc_plug_log("anc_pow_power_adaptive_sync:%d %d", ref_lvl, err_lvl);
#if ANC_ADAPTIVE_TONE_EN
    if (ref_lvl == 1) {
        play_tone_file(get_tone_files()->num[1]);
    } else if (ref_lvl == 0) {
        play_tone_file(get_tone_files()->num[2]);
    }
#endif/*ANC_ADAPTIVE_TONE_EN*/
    anc_pow_adap_fade_timer_add(ref_lvl, err_lvl);
}

int audio_anc_pow_adap_deal(int ref_lvl, int err_lvl)
{
#if TCFG_USER_TWS_ENABLE
    if (get_tws_sibling_connect_state()) {
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            anc_plug_log("[master]anc_pow_adap send %d %d\n", ref_lvl, err_lvl);
        } else {
            anc_plug_log("[slave]anc_pow_adap send %d %d\n", ref_lvl, err_lvl);
        }
        bt_tws_anc_power_adaptive_compare_send(ref_lvl, err_lvl);
    } else {
        anc_plug_log("[no tws]anc_pow_adap send %d %d\n", ref_lvl, err_lvl);
        audio_anc_power_adaptive_tone_play(ref_lvl, err_lvl);
    }
#else
    anc_plug_log("[no tws]anc_pow_adap send %d %d\n", ref_lvl, err_lvl);
    audio_anc_power_adaptive_tone_play(ref_lvl, err_lvl);
#endif/*TCFG_USER_TWS_ENABLE*/
    return 0;
}

/*切换增益模式*/
void audio_anc_power_adaptive_mode_set(u8 mode, u8 lvl)
{
    if (power_adaptive_hdl) {
        power_adaptive_hdl->param->adaptive_mode = mode;
        audio_anc_pow_export_en((mode == ANC_ADAPTIVE_GAIN_MODE), 0);
        if (anc_mode_get() == ANC_ON) {
            switch (mode) {
            case ANC_ADAPTIVE_MANUAL_MODE:
                anc_plug_log("ANC_ADAPTIVE_MANUAL_MODE ,lvl%d\n", lvl);
                audio_anc_pow_adap_stop();
                power_adaptive_hdl->param->anc_lvl = lvl;
                audio_anc_lvl_set(lvl, 1);
                break;
            case ANC_ADAPTIVE_GAIN_MODE:
                anc_plug_log("ANC_ADAPTIVE_GAIN_MODE\n");
                audio_anc_pow_adap_start();
                break;
            }
        }
    }
}

static void audio_anc_power_adaptive_fade(u16 gain)
{
    if (power_adaptive_hdl) {
        audio_anc_fade_ctr_set(ANC_FADE_MODE_SCENE_ADAPTIVE, AUDIO_ANC_FDAE_CH_ALL, gain);
    }
}
static void audio_anc_power_adaptive_event_cb(u8 lvl, enum ANC_ADAPTIVE_FADE_EVENT event)
{
    //lvl 当前触发等级
    //event 当前事件
    /* anc_plug_log("adap lvl %d, event %d\n",lvl, event); */
}

//获取当前场景自适应模式
u8 audio_anc_power_adaptive_mode_get(void)
{
    return power_adaptive_hdl->param->adaptive_mode;
}

//用于功能互斥需要挂起场景自适应
void audio_anc_power_adaptive_suspend(void)
{
    if (power_adaptive_hdl->suspend == 1) {
        return;
    }
    power_adaptive_hdl->suspend = 1;
    power_adaptive_hdl->suspend_state = audio_anc_power_adaptive_mode_get();
    if (power_adaptive_hdl->suspend_state == ANC_ADAPTIVE_GAIN_MODE) {
        audio_anc_power_adaptive_mode_set(ANC_ADAPTIVE_MANUAL_MODE, 0);
    }
}

//用于功能互斥结束后恢复场景自适应
void audio_anc_power_adaptive_resume(void)
{
    if (power_adaptive_hdl->suspend == 0) {
        return;
    }
    power_adaptive_hdl->suspend = 0;
    if (power_adaptive_hdl->suspend_state == ANC_ADAPTIVE_GAIN_MODE) {
        audio_anc_power_adaptive_mode_set(ANC_ADAPTIVE_GAIN_MODE, 0);
    }
}

#endif

#if ANC_DUT_MIC_CMP_GAIN_ENABLE

static struct anc_mic_gain_cmp_cfg *anc_dut_mic_cmp = NULL;

enum {
    CMD_MIC_CMP_GAIN_EN = 0X5F,		//FF/FB 增益补偿使能
    CMD_MIC_CMP_GAIN_SET = 0X60,	//FF/FB 增益补偿设置
    CMD_MIC_CMP_GAIN_GET = 0X61,	//FF/FB 增益补偿读取
    CMD_MIC_CMP_GAIN_CLEAN = 0X62,	//FF/FB 增益补偿清0
    CMD_MIC_CMP_GAIN_SAVE = 0X63,	//FF/FB 增益补偿保存
    CMD_MIC_CMP_GAIN_ALL_GET = 0X64, //FF/FB 增益补偿结构体获取
    CMD_MIC_CMP_GAIN_ALL_SET = 0X65, //FF/FB 增益补偿结构体设置
};

//ANC MIC补偿值初始化
void audio_anc_mic_gain_cmp_init(void *mic_cmp)
{
    if (mic_cmp) {
        anc_dut_mic_cmp = (struct anc_mic_gain_cmp_cfg *)mic_cmp;
        int ret = syscfg_read(CFG_ANC_MIC_CMP_GAIN, mic_cmp, sizeof(struct anc_mic_gain_cmp_cfg));
        if (ret != sizeof(struct anc_mic_gain_cmp_cfg)) {
            printf("anc_mic_cmp cap vm read fail !!!");
            anc_dut_mic_cmp->en = 1;
            anc_dut_mic_cmp->lff_gain = 1.0f;
            anc_dut_mic_cmp->lfb_gain = 1.0f;
            anc_dut_mic_cmp->rff_gain = 1.0f;
            anc_dut_mic_cmp->rfb_gain = 1.0f;
        } else {
            printf("anc_mic_cmp cap vm read succ: en %d lff_gain %d/1000 lfb_gain %d/1000 rff_gain %d/1000 rfb_gain %d/1000\n",
                   anc_dut_mic_cmp->en,
                   (int)(anc_dut_mic_cmp->lff_gain * 1000.0f),
                   (int)(anc_dut_mic_cmp->lfb_gain * 1000.0f),
                   (int)(anc_dut_mic_cmp->rff_gain * 1000.0f),
                   (int)(anc_dut_mic_cmp->rfb_gain * 1000.0f));
        }
    }
}

float audio_anc_mic_gain_cmp_get(u8 id)
{
    if (!anc_dut_mic_cmp) {
        return 1.0f;
    }
    switch (id) {
    case ANC_L_FFMIC_CMP_GAIN_SET:
        return anc_dut_mic_cmp->lff_gain;
    case ANC_L_FBMIC_CMP_GAIN_SET:
        return anc_dut_mic_cmp->lfb_gain;
    case ANC_R_FFMIC_CMP_GAIN_SET:
        return anc_dut_mic_cmp->rff_gain;
    case ANC_R_FBMIC_CMP_GAIN_SET:
        return anc_dut_mic_cmp->rfb_gain;
    }
    return 1.0f;
}

//补偿命令处理函数
int audio_anc_mic_gain_cmp_cmd_process(u8 cmd, u8 *buf, int len)
{
    put_buf(buf, len);
    float gain;
    switch (cmd) {
    case CMD_MIC_CMP_GAIN_EN:
        anc_plug_log("CMD_MIC_CMP_GAIN_EN\n");
        anc_dut_mic_cmp->en = buf[0];
#if ANC_MULT_ORDER_ENABLE
        audio_anc_mult_scene_update(audio_anc_mult_scene_get());
#endif
        break;
    case CMD_MIC_CMP_GAIN_SET:
        if (len != 5) {	//cmd + sizeof(float)
            return 1;
        }
        memcpy((u8 *)&gain, buf + 1, 4);
        anc_plug_log("CMD_MIC_CMP_GAIN_SET %d/100\n", (int)(gain * 100.0f));
        switch (buf[0]) {
        case ANC_L_FFMIC_CMP_GAIN_SET:
            anc_dut_mic_cmp->lff_gain = gain;
            break;
        case ANC_L_FBMIC_CMP_GAIN_SET:
            anc_dut_mic_cmp->lfb_gain = gain;
            break;
        case ANC_R_FFMIC_CMP_GAIN_SET:
            anc_dut_mic_cmp->rff_gain = gain;
            break;
        case ANC_R_FBMIC_CMP_GAIN_SET:
            anc_dut_mic_cmp->rfb_gain = gain;
            break;
        }
#if ANC_MULT_ORDER_ENABLE
        audio_anc_mult_scene_update(audio_anc_mult_scene_get());
#endif
        break;
    case CMD_MIC_CMP_GAIN_CLEAN:
        anc_plug_log("CMD_MIC_CMP_GAIN_CLEAN\n");
        anc_dut_mic_cmp->lff_gain = 1.0f;
        anc_dut_mic_cmp->lfb_gain = 1.0f;
        anc_dut_mic_cmp->rff_gain = 1.0f;
        anc_dut_mic_cmp->rfb_gain = 1.0f;
#if ANC_MULT_ORDER_ENABLE
        audio_anc_mult_scene_update(audio_anc_mult_scene_get());
#endif
        break;
    case CMD_MIC_CMP_GAIN_SAVE:
        anc_plug_log("CMD_MIC_CMP_GAIN_SAVE\n");
        int wlen = syscfg_write(CFG_ANC_MIC_CMP_GAIN, (u8 *)anc_dut_mic_cmp, sizeof(struct anc_mic_gain_cmp_cfg));
        if (wlen != sizeof(struct anc_mic_gain_cmp_cfg)) {
            printf("anc_mic_cmp cap vm write fail !!!");
            return 1;
        }
        break;
    case CMD_MIC_CMP_GAIN_ALL_SET:
        anc_plug_log("CMD_MIC_CMP_GAIN_ALL_SET\n");
        if (len != sizeof(struct anc_mic_gain_cmp_cfg)) {
            return 1;
        }
        memcpy(anc_dut_mic_cmp, buf, len);
#if ANC_MULT_ORDER_ENABLE
        audio_anc_mult_scene_update(audio_anc_mult_scene_get());
#endif
        break;

    }
    return 0;
}

u8 *audio_anc_mic_gain_cmp_cfg_get(int *len)
{
    if (anc_dut_mic_cmp) {
        *len = sizeof(struct anc_mic_gain_cmp_cfg);
        return (u8 *)anc_dut_mic_cmp;
    }
    return NULL;
}


#endif

#if TCFG_AUDIO_ANC_DCC_TRIM_ENABLE
/*
    Trim的方式，
        0 开机不做动作
        1 每次开机TRIM；
        2 只TRIM 1次，并存到VM;
*/
#define AUDIO_ANC_DCC_TRIM_MODE         0

#define AUDIO_ANC_DCC_TRIM_ONCE_TIME	20		//定时时间 ms
#define AUDIO_ANC_DCC_TRIM_CNT			50		//定时次数
#define AUDIO_ANC_DCC_TRIM_HOLD_TIME	1500 	//冷却时间 ms


struct anc_dcc_trim_param {
    u8 state;
    u16 time_id;
    u16 cnt;
    u16 hold_time_id;
};

struct anc_dcc_trim_param dcc_trim_hdl;
//dcc trim 流程-待测试
int audio_anc_dcc_trim_switch(u8 en)
{
    audio_anc_t *param = audio_anc_param_get();
    if (!param) {
        return 1;
    }
    if (en) {
        anc_plug_log("DCC_TRIM_SWITCH:USE");
        param->dcc_trim.mode = ANC_DCC_TRIM_USE;
    } else {	//已工具配置的DCC为准
        anc_plug_log("DCC_TRIM_SWITCH:CLOSE");
        param->dcc_trim.mode = ANC_DCC_TRIM_CLOSE;
    }
    return 0;
}

int audio_anc_dcc_trim_check(void)
{
#if AUDIO_ANC_DCC_TRIM_MODE == 2
    struct anc_dcc_trim_cfg *dcc_trim =  &(audio_anc_param_get()->dcc_trim);
    int ret = syscfg_read(PSKEY_ANC_DCC_TRIM_CFG, dcc_trim, sizeof(struct anc_dcc_trim_cfg));
    if (ret != sizeof(struct anc_mic_gain_cmp_cfg)) {
        printf("DCC_TRIM_STATE: DCC vm read fail !!!");
        audio_anc_dcc_trim_open();
    } else {
        printf("DCC_TRIM_STATE: DCC vm read succ !!!");
        printf("mode %d, ff_dcc %d, fb_dcc %d\n", dcc_trim->mode, dcc_trim->lff_dcc, dcc_trim->lfb_dcc);
    }
#elif AUDIO_ANC_DCC_TRIM_MODE == 1
    audio_anc_dcc_trim_open();
#endif
    return 0;
}

int audio_anc_dcc_trim_state_get(void)
{
    return dcc_trim_hdl.state;
}

int audio_anc_dcc_trim_open(void)
{
    struct anc_dcc_trim_cfg *dcc_trim =  &(audio_anc_param_get()->dcc_trim);
    struct anc_dcc_trim_param *p = &dcc_trim_hdl;
    if (p->time_id) {
        sys_timer_del(p->time_id);
        p->time_id = 0;
    }
    if (p->hold_time_id) {
        sys_timer_del(p->hold_time_id);
        p->hold_time_id = 0;
    }
    p->cnt = 0;
    p->time_id = 0;
    p->hold_time_id = 0;
    dcc_trim->mode = ANC_DCC_TRIM_START;
    dcc_trim->lff_dcc = 0;
    dcc_trim->lfb_dcc = 0;
    anc_plug_log("DCC_TRIM_STATE:OPEN");
    audio_anc_fade_ctr_set(ANC_FADE_MODE_DCC_TRIM, AUDIO_ANC_FDAE_CH_ALL, 0);
    if (!anc_mode_switch(ANC_ON, 0)) {
        p->state = 1;
    } else {
        dcc_trim->mode = ANC_DCC_TRIM_CLOSE;
        audio_anc_fade_ctr_set(ANC_FADE_MODE_DCC_TRIM, AUDIO_ANC_FDAE_CH_ALL, 16384);
    }

    return 0;
}

void audio_anc_dcc_trim_timer(void *priv)
{
    struct anc_dcc_trim_cfg *dcc_trim = &(audio_anc_param_get()->dcc_trim);
    struct anc_dcc_trim_param *p = &dcc_trim_hdl;
    int lff_dcc_tmp, lfb_dcc_tmp;
    lff_dcc_tmp = audio_anc_dc_vld_read(ANC_MIC_TYPE_LFF);
    lfb_dcc_tmp = audio_anc_dc_vld_read(ANC_MIC_TYPE_LFB);
    dcc_trim->lff_dcc += lff_dcc_tmp;
    dcc_trim->lfb_dcc += lfb_dcc_tmp;
    p->cnt++;

    //anc_plug_log("dcc_trim ff_dcc %6d, fb_dcc %6d\n", lff_dcc_tmp, lfb_dcc_tmp);

    if (p->cnt > AUDIO_ANC_DCC_TRIM_CNT) {
        dcc_trim->lff_dcc /= AUDIO_ANC_DCC_TRIM_CNT;
        dcc_trim->lfb_dcc /= AUDIO_ANC_DCC_TRIM_CNT;
        p->state = 0;
        dcc_trim->mode = ANC_DCC_TRIM_USE;
        anc_plug_log("dcc_trim output ff_dcc %6d, fb_dcc %6d\n", dcc_trim->lff_dcc, dcc_trim->lfb_dcc);
        anc_plug_log("DCC_TRIM_STATE:FINISH");
        sys_timer_del(p->time_id);
#if AUDIO_ANC_DCC_TRIM_MODE == 2
        syscfg_write(PSKEY_ANC_DCC_TRIM_CFG, dcc_trim, sizeof(struct anc_dcc_trim_cfg));
#endif
        anc_mode_switch(ANC_OFF, 0);
        audio_anc_fade_ctr_set(ANC_FADE_MODE_DCC_TRIM, AUDIO_ANC_FDAE_CH_ALL, 16384);
    }
}

//form anc task
static void audio_anc_dcc_trim_hold_timeout(void *priv)
{
    struct anc_dcc_trim_param *p = &dcc_trim_hdl;
    anc_plug_log("DCC_TRIM_STATE:HOLD_END");
    p->time_id = sys_timer_add(NULL, audio_anc_dcc_trim_timer, AUDIO_ANC_DCC_TRIM_ONCE_TIME);
    p->hold_time_id = 0;
}

//form anc task
int audio_anc_dcc_trim_process(void)
{
    struct anc_dcc_trim_param *p = &dcc_trim_hdl;
    p->hold_time_id = sys_timeout_add(NULL, audio_anc_dcc_trim_hold_timeout, AUDIO_ANC_DCC_TRIM_HOLD_TIME);
    return 0;
}
#endif


