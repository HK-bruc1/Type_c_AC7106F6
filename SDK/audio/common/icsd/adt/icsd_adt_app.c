#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif
/*
 ****************************************************************************
 *							ICSD ADT APP API
 *
 *Description	: 智能免摘和anc风噪检测相关接口
 *Notes			:
 ****************************************************************************
 */
#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE)
#include "audio_anc_includes.h"
#include "system/includes.h"
#include "icsd_adt.h"
#include "system/task.h"
#include "audio_adc.h"
#include "audio_dac.h"
#include "timer.h"
#include "btstack/avctp_user.h"
#include "bt_tws.h"
#include "tone_player.h"
#include "anc.h"
#include "audio_config.h"
#include "sniff.h"
#include "effects/convert_data.h"

#if (ICSD_ADT_WIND_INFO_SPP_DEBUG_EN || ICSD_ADT_VOL_NOISE_LVL_SPP_DEBUG_EN)
#include "spp_user.h"
#endif

#if ICSD_ADT_MIC_DATA_EXPORT_EN
#include "aec_uart_debug.h"
#endif /*ICSD_ADT_MIC_DATA_EXPORT_EN*/

#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE && ICSD_ADT_WIND_INFO_SPP_DEBUG_EN && (!TCFG_BT_SUPPORT_SPP || !APP_ONLINE_DEBUG)
#error "please open TCFG_BT_SUPPORT_SPP and APP_ONLINE_DEBUG"
#endif
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE && ICSD_ADT_VOL_NOISE_LVL_SPP_DEBUG_EN && (!TCFG_BT_SUPPORT_SPP || !APP_ONLINE_DEBUG)
#error "please open TCFG_BT_SUPPORT_SPP and APP_ONLINE_DEBUG"
#endif
#if ICSD_ADT_MIC_DATA_EXPORT_EN && (TCFG_AUDIO_DATA_EXPORT_DEFINE != AUDIO_DATA_EXPORT_VIA_UART)
#error "please open AUDIO_DATA_EXPORT_VIA_UART"
#endif

#if 1
#define adt_log printf
#else
#define adt_log(...)
#endif

#if 1
#define adt_debug_log printf
#else
#define adt_debug_log(...)
#endif

#define ADT_MIC_GAIN_SOURCE_LFF				0		//anc_gains lff
#define ADT_MIC_GAIN_SOURCE_RFF				0		//anc_gains rff
#define ADT_MIC_GAIN_SOURCE_ESCO_TALK		1		//esco_cfg talk mic

//ADT TALK MIC增益来源
#define ICSD_ADT_TALK_GAIN_SOURCE	ADT_MIC_GAIN_SOURCE_LFF

extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;
extern const u8 const_adc_async_en;

struct speak_to_chat_t {
    volatile u8 busy;
    volatile u8 state;
    struct icsd_acoustic_detector_libfmt libfmt;//回去需要的资源
    struct icsd_acoustic_detector_infmt infmt;//转递资源
    struct icsd_adt_input_param_v3 input_param;
    void *lib_alloc_ptr;//adt申请的内存指针
    u8 adt_wind_suspend_rtanc;	//触发风噪时挂起RT_ANC 标志
    u8 adt_wat_result;//广域点击次数
    u8 adt_wat_ignore_flag;//是否忽略广域点击的响应
    u16 resume_timer;	//ADT恢复定时器
#if (ICSD_ADT_WIND_INFO_SPP_DEBUG_EN || ICSD_ADT_VOL_NOISE_LVL_SPP_DEBUG_EN)
    struct spp_operation_t *spp_opt;
    u8 spp_connected_state;
#endif
    u32 wind_cnt;
    int sample_rate;
#if ICSD_ADT_MIC_DATA_EXPORT_EN
    s16 tmpbuf0[256];
    s16 tmpbuf1[256];
    s16 tmpbuf2[256];
#endif /*ICSD_ADT_MIC_DATA_EXPORT_EN*/
    float drc_gain[4];
    u16 adt_env_adaptive_hold_id;   //环境自适应触发冷却
    int talk_mic_seq;
    int lff_mic_seq;
    int lfb_mic_seq;
    int rff_mic_seq;
    int rfb_mic_seq;
    s16 *talk_mic_buf;
    s16 *lff_mic_buf;
    s16 *lfb_mic_buf;
    s16 *rff_mic_buf;
    s16 *rfb_mic_buf;
    char *sync_cnt;		//TWS实时性消息计数
};
struct speak_to_chat_t *speak_to_chat_hdl = NULL;

typedef struct {
    u8 env_noise_det_en;
    u8 mode_switch_busy;
    u16 adt_mode;			   //已生效的adt_mode
    u16 record_adt_mode;       //预设的adt_mode(befor scene check)
    u16 app_adt_mode;		   //用户层adt_mode记录
    enum ICSD_ADT_SCENE scene;
    //spinlock_t lock;
    OS_MUTEX mutex;
} adt_info_t;
static adt_info_t adt_info = {
    .env_noise_det_en = 0,
    .mode_switch_busy = 0,
    .adt_mode = ADT_MODE_CLOSE,
    .record_adt_mode = ADT_MODE_CLOSE,    //
    .app_adt_mode = ADT_MODE_CLOSE,    //
    .scene = 0,
};

//映射ANC模式和算法恢复变量关系
const u8 adt_resume_mode[3][2] = {
    {RESUME_ANCMODE, ADT_ANC_OFF},		//ANC_OFF
    {RESUME_ANCMODE, ADT_ANC_ON},		//ANC_ON
    {RESUME_BYPASSMODE, ADT_ANC_OFF},	//ANC_TRANSPARENCY
};

//ADT TWS通讯有实时性要求的信息
const int adt_tws_rt_sync_table[] = {
    SYNC_ICSD_ADT_VDT_TIRRGER,
    SYNC_ICSD_ADT_WAT_RESULT,
};

void audio_icsd_adt_env_noise_det_set(u8 env_noise_det_en)
{
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
    adt_info.env_noise_det_en |= env_noise_det_en;
    adt_log("audio_icsd_adt_env_noise_det_set %d\n", adt_info.env_noise_det_en);
#endif
}
void audio_icsd_adt_env_noise_det_clear(u8 env_noise_det_clear)
{
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
    adt_info.env_noise_det_en &= ~env_noise_det_clear;
    adt_log("audio_icsd_adt_env_noise_det_clear %d\n", adt_info.env_noise_det_en);
#endif
}
u8 audio_icsd_adt_env_noise_det_get()
{
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
    return adt_info.env_noise_det_en;
#endif
    return 0;
}

void icsd_adt_lock_init()
{
    os_mutex_create(&adt_info.mutex);
    //spin_lock_init(&adt_info.lock);
}
void icsd_adt_lock()
{
    os_mutex_pend(&adt_info.mutex, 0);
    //spin_lock(&adt_info.lock);
}
void icsd_adt_unlock()
{
    os_mutex_post(&adt_info.mutex);
    //spin_unlock(&adt_info.lock);
}
/*tws同步char状态使用*/
static u8 globle_speak_to_chat_state = 0;
static u8 anc_env_adaptive_gain_suspend = 0;
static u16 anc_env_adaptive_suspend_id = 0;
static u16 anc_wind_det_suspend_id = 0;
u8 get_icsd_adt_mic_ch(struct icsd_acoustic_detector_libfmt *libfmt);
static void audio_rtanc_drc_flag_remote_set(u8 ouput, float gain, float fb_gain);
static int icsd_adt_debug_tool_function_get(void);

//检查TWS 消息是否多次发送未收到，避免TWS消息池堆积过多
static int icsd_adt_info_sync_add_check(int mode, int num)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        u8 cnt = ARRAY_SIZE(adt_tws_rt_sync_table);
        for (int i = 0; i < cnt; i++) {
            if (mode == adt_tws_rt_sync_table[i]) {
                hdl->sync_cnt[i] += num;
                /* adt_log("ADT SYNC_MODE %d, cnt[%d] %d, num %d\n", mode, i, hdl->sync_cnt[i], num); */
                if (hdl->sync_cnt[i] < 0) {
                    hdl->sync_cnt[i] = 0;
                    adt_log("Err: icsd_adt_info_sync cnt error %d\n", mode);
                }
                if (hdl->sync_cnt[i] > 3) {	//同一条消息TWS发送多次未收到
                    hdl->sync_cnt[i] = 3;
                    adt_log("Err: icsd_adt_info_sync full %d\n", mode);
                    return -1;
                }
                break;
            }
        }
    }
    return 0;
}

//ADT TWS 信息同步回调处理函数
void audio_icsd_adt_info_sync_cb(void *_data, u16 len, bool rx)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    u8 tmp_data[4];
    u8 *data = (u8 *)_data;
    int mode = data[0];
    int wat_result  = 0;
    int val = 0;
    int err = 0;
    int adt_mode = 0;
    int suspend = 0;
    int anc_fade_gain = 0;
    int anc_fade_mode = 0;
    int anc_fade_time = 0;
    // adt_debug_log("audio_icsd_adt_info_sync_cb rx:%d, mode:%d ", rx, mode);

#if TCFG_USER_TWS_ENABLE
    if (!rx) {
        icsd_adt_info_sync_add_check(mode, -1);
    }
#endif
    switch (mode) {
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    case SYNC_ICSD_ADT_RTANC_DRC_RESULT:
        if (!hdl || !hdl->state) {
            break;
        }
        if (rx) {
            hdl->busy = 1;
            float drc_gain;
            float drc_fb_gain;
            memcpy(&drc_gain, ((u8 *)_data) + 2, 4);
            memcpy(&drc_fb_gain, ((u8 *)_data) + 6, 4);
            audio_rtanc_drc_flag_remote_set(((u8 *)_data)[1], drc_gain, drc_fb_gain);
            if (err != OS_NO_ERR) {
                adt_log("audio_icsd_adt_info_sync_cb err %d", err);
            }
            hdl->busy = 0;
        }
        break;
#endif
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    case SYNC_ICSD_ADT_VDT_TIRRGER:
        u8 voice_state    = ((u8 *)_data)[1];
        if (adt_info.adt_mode & ADT_SPEAK_TO_CHAT_MODE) {
            /* err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_VOICE_STATE, voice_state); */
            audio_vdt_trigger_send_to_adt(voice_state);
        }
        break;

    case SYNC_ICSD_ADT_VDT_RESUME:
        audio_vdt_resume_send_to_adt();
        break;
#endif /*TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE*/
#if TCFG_AUDIO_WIDE_AREA_TAP_ENABLE
    case SYNC_ICSD_ADT_WAT_RESULT:
        if (!hdl || !hdl->state) {
            break;
        }
        hdl->busy = 1;
        wat_result    = ((u8 *)_data)[1];
#if (TCFG_USER_TWS_ENABLE && !AUDIO_ANC_WIDE_AREA_TAP_EVENT_SYNC)
        /*广域点击:是否响应另一只耳机的点击*/
        if (!rx)
#endif /*AUDIO_ANC_WIDE_AREA_TAP_EVENT_SYNC*/
        {
            if ((adt_info.adt_mode & ADT_WIDE_AREA_TAP_MODE) && wat_result && !hdl->adt_wat_ignore_flag) {
                err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_WAT_RESULT, wat_result);
                if (err != OS_NO_ERR) {
                    adt_log("audio_icsd_adt_info_sync_cb err %d", err);
                }
            }
        }
        hdl->busy = 0;
        break;
#endif /*TCFG_AUDIO_WIDE_AREA_TAP_ENABLE*/
    case SYNC_ICSD_ADT_OPEN:
    case SYNC_ICSD_ADT_CLOSE:
        memcpy(&adt_mode, ((u8 *)_data) + 1, sizeof(u16));
        adt_log("SYNC_ICSD_ADT_OPEN/CLOSE, cmd 0x%x, adt_mode 0x%x\n", mode, adt_mode);
        suspend = ((u8 *)_data)[3];
        //APP层 操作，需要修改标志
        icsd_adt_app_mode_set(adt_mode, (mode == SYNC_ICSD_ADT_OPEN));
        err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 3, mode, adt_mode, suspend);
        if (err != OS_NO_ERR) {
            adt_log("audio_icsd_adt_info_sync_cb err %d", err);
        }
        break;
    case SYNC_ICSD_ADT_SET_ANC_FADE_GAIN:
        anc_fade_gain = (((u8 *)_data)[2] << 8) | ((u8 *)_data)[1];
        anc_fade_mode = ((u8 *)_data)[3];
        anc_fade_time = ((u8 *)_data)[4];
        anc_fade_time = anc_fade_time * 100;
        err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 4, mode, anc_fade_gain, anc_fade_mode, anc_fade_time);
        if (err != OS_NO_ERR) {
            adt_log("audio_icsd_adt_info_sync_cb err %d", err);
        }
        break;
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    case SYNC_ICSD_ADT_RTANC_TONE_RESUME:
        adt_log("SYNC_ICSD_ADT_RTANC_TONE_RESUME");
        audio_anc_real_time_adaptive_resume("TONE_ADAPTIVE");
        break;
    case SYNC_ICSD_ADT_TONE_ADAPTIVE_SYNC:
        adt_log("SYNC_ICSD_ADT_TONE_ADAPTIVE_SYNC %d\n", rx);
        audio_anc_tone_adaptive_enable(rx);
        break;
#endif
    default:
        break;
    }
}

#define TWS_FUNC_ID_ICSD_ADT_M2S    TWS_FUNC_ID('I', 'A', 'M', 'S')
REGISTER_TWS_FUNC_STUB(audio_icsd_adt_m2s) = {
    .func_id = TWS_FUNC_ID_ICSD_ADT_M2S,
    .func    = audio_icsd_adt_info_sync_cb,
};

void audio_icsd_adt_info_sync(u8 *data, int len)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    // adt_log("audio_icsd_adt_info_sync , %d, %d, %d", data[0], data[1], data[2]);

#if TCFG_USER_TWS_ENABLE
    if (icsd_adt_info_sync_add_check(data[0], 1)) {
        return;
    }
    if (get_tws_sibling_connect_state()) {
        int ret = tws_api_send_data_to_sibling(data, len, TWS_FUNC_ID_ICSD_ADT_M2S);
    } else
#endif
    {
        audio_icsd_adt_info_sync_cb(data, len, 0);
    }
}

//ADC 中断处理函数
static void audio_mic_output_handle(void *priv, s16 *data, int len)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    s16 *talk_mic = NULL;
    s16 *lff_mic = NULL;
    s16 *lfb_mic = NULL;
    s16 *rff_mic = NULL;
    s16 *rfb_mic = NULL;
    u16 talk_mic_ch = icsd_get_talk_mic_ch();
    u16 ref_mic_l_ch = icsd_get_ref_mic_L_ch();
    u16 fb_mic_l_ch = icsd_get_fb_mic_L_ch();
    u16 ref_mic_r_ch = icsd_get_ref_mic_R_ch();
    u16 fb_mic_r_ch = icsd_get_fb_mic_R_ch();

    u8 mic_ch_num = audio_get_mic_num(hdl->libfmt.mic_type);
    u16 open_mic_ch = get_icsd_adt_mic_ch(&hdl->libfmt);
    if (const_adc_async_en) {
        mic_ch_num = audio_max_adc_ch_num_get();
    }
    if (hdl && hdl->state) {
        hdl->busy = 1;
#ifndef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
        if (adc_hdl.bit_width != ADC_BIT_WIDTH_16) {
            audio_convert_data_32bit_to_16bit_round((s32 *)data, (s16 *)data, (len >> 2) * mic_ch_num);
            len >>= 1;
        }
#endif


#ifdef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
        //共用adc并且使用24bit时
        if (adc_hdl.bit_width != ADC_BIT_WIDTH_16) {
            // putchar('B');
            s32 *s32_data = (s32 *)data;
            /*复用adc时，adc的数据需要额外buf存起来*/
            s32 *s32_talk_mic = (s32 *)hdl->talk_mic_buf;
            s32 *s32_lff_mic   = (s32 *)hdl->lff_mic_buf;
            s32 *s32_lfb_mic   = (s32 *)hdl->lfb_mic_buf;
            s32 *s32_rff_mic   = (s32 *)hdl->rff_mic_buf;
            s32 *s32_rfb_mic   = (s32 *)hdl->rfb_mic_buf;
            talk_mic = hdl->talk_mic_buf;
            lff_mic   = hdl->lff_mic_buf;
            lfb_mic   = hdl->lfb_mic_buf;
            rff_mic   = hdl->rff_mic_buf;
            rfb_mic   = hdl->rfb_mic_buf;

            for (int i = 0; i < len / 4; i++) {
                if (open_mic_ch & talk_mic_ch) {
                    s32_talk_mic[i] = s32_data[mic_ch_num * i + hdl->talk_mic_seq];
                    talk_mic[i] = (s32_talk_mic[1] >> 8);
                    data_sat_s16(talk_mic[i]);
                }
                if (open_mic_ch & ref_mic_l_ch) {
                    s32_lff_mic[i]   = s32_data[mic_ch_num * i + hdl->lff_mic_seq];
                    lff_mic[i] = (s32_lff_mic[1] >> 8);
                    data_sat_s16(lff_mic[i]);
                }
                if (open_mic_ch & fb_mic_l_ch) {
                    s32_lfb_mic[i]   = s32_data[mic_ch_num * i + hdl->lfb_mic_seq];
                    lfb_mic[i] = (s32_lfb_mic[1] >> 8);
                    data_sat_s16(lfb_mic[i]);
                }
                if (open_mic_ch & ref_mic_r_ch) {
                    s32_rff_mic[i]   = s32_data[mic_ch_num * i + hdl->rff_mic_seq];
                    rff_mic[i] = (s32_rff_mic[1] >> 8);
                    data_sat_s16(rff_mic[i]);
                }
                if (open_mic_ch & fb_mic_r_ch) {
                    s32_rfb_mic[i]   = s32_data[mic_ch_num * i + hdl->rfb_mic_seq];
                    rfb_mic[i] = (s32_rfb_mic[1] >> 8);
                    data_sat_s16(rfb_mic[i]);
                }
            }
            len >>= 1;
        } else
#endif
        {
            //共用adc并且使用16bit时 || 不共用adc使用16bit || 不共用adc使用24bit
            /*复用adc时，adc的数据需要额外buf存起来*/
            talk_mic = hdl->talk_mic_buf;
            lff_mic   = hdl->lff_mic_buf;
            lfb_mic   = hdl->lfb_mic_buf;
            rff_mic   = hdl->rff_mic_buf;
            rfb_mic   = hdl->rfb_mic_buf;
            for (int i = 0; i < len / 2; i++) {
                if (open_mic_ch & talk_mic_ch) {
                    talk_mic[i] = data[mic_ch_num * i + hdl->talk_mic_seq];
                }
                if (open_mic_ch & ref_mic_l_ch) {
                    lff_mic[i]   = data[mic_ch_num * i + hdl->lff_mic_seq];
                }
                if (open_mic_ch & fb_mic_l_ch) {
                    lfb_mic[i]   = data[mic_ch_num * i + hdl->lfb_mic_seq];
                }
                if (open_mic_ch & ref_mic_r_ch) {
                    rff_mic[i]   = data[mic_ch_num * i + hdl->rff_mic_seq];
                }
                if (open_mic_ch & fb_mic_r_ch) {
                    rfb_mic[i]   = data[mic_ch_num * i + hdl->rfb_mic_seq];
                }
            }
        }
        hdl->input_param.priv = priv;
        hdl->input_param.lff = lff_mic;
        hdl->input_param.rff = rff_mic;
        hdl->input_param.lfb = lfb_mic;
        hdl->input_param.rfb = rfb_mic;
        hdl->input_param.ltalk = talk_mic;
        hdl->input_param.rtalk = NULL;
        hdl->input_param.len = len;
        icsd_acoustic_detector_mic_input_hdl_v3(&hdl->input_param);
#if ICSD_ADT_MIC_DATA_EXPORT_EN
        if (open_mic_ch & talk_mic_ch) {
            aec_uart_fill(0, talk_mic, len);
        }
        if (open_mic_ch & ref_mic_l_ch) {
            aec_uart_fill(1, lff_mic,   len);
        }
        if (open_mic_ch & fb_mic_l_ch) {
            aec_uart_fill(2, lfb_mic,   len);
        }
        /* aec_uart_write(); */
        /*write里面可能会触发pend的动作，所有放到任务去跑*/
        int err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_EXPORT_DATA_WRITE, 1);
        if (err != OS_NO_ERR) {
            adt_log("%s err %d", __func__, err);
        }
#endif /*ICSD_ADT_MIC_DATA_EXPORT_EN*/
        hdl->busy = 0;
    }
}

//ANC task dma 抄数回调接口
void audio_acoustic_detector_anc_dma_post_msg(void)
{

}
//ANC DMA下采样数据输出回调，用于写卡分析
void audio_acoustic_detector_anc_dma_output_callback(void)
{

}

void audio_icsd_adt_start()
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    audio_icsd_adt_algom_resume(anc_mode_get());
}

//将coeff/gain 等信息更新给免摘 : 执行
int audio_acoustic_detector_updata()
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        hdl->busy = 1;
        /*获取anc参数和增益*/
        /*获取anc参数和增益*/
        hdl->infmt.gains_l_fbgain = get_anc_gains_l_fbgain();
        hdl->infmt.gains_l_ffgain = get_anc_gains_l_ffgain();
        hdl->infmt.gains_l_transgain = get_anc_gains_l_transgain();
        hdl->infmt.gains_l_transfbgain = get_anc_gains_lfb_transgain();
        hdl->infmt.trans_alogm    = audio_anc_gains_alogm_get(ANC_TRANS_TYPE);
        hdl->infmt.alogm          = audio_anc_gains_alogm_get(ANC_FF_TYPE);

        extern u32 get_anc_gains_sign();
        hdl->infmt.gain_sign = get_anc_gains_sign();

        icsd_acoustic_detector_infmt_updata(&hdl->infmt);
        hdl->busy = 0;
    }

    return 0;
}

#if (ICSD_ADT_WIND_INFO_SPP_DEBUG_EN || ICSD_ADT_VOL_NOISE_LVL_SPP_DEBUG_EN)
/*记录spp连接状态*/
void icsd_adt_spp_connect_state_cbk(u8 state)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        hdl->spp_connected_state = state;
    }
}
#endif

/*设置talk mic gain和ff mic gain一样*/
static void audio_icsd_adt_set_talk_mic_gain(audio_mic_param_t *mic_param)
{
    u8 talk_mic_ch = icsd_get_talk_mic_ch();
    u8 target_ch;
#if ICSD_ADT_TALK_GAIN_SOURCE == ADT_MIC_GAIN_SOURCE_LFF
    target_ch = icsd_get_ref_mic_L_ch();
#elif ICSD_ADT_TALK_GAIN_SOURCE == ADT_MIC_GAIN_SOURCE_RFF
    target_ch = icsd_get_ref_mic_R_ch();
#else
    target_ch = talk_mic_ch;
#endif

    //BIT 转 NUM
    for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        if (talk_mic_ch == AUDIO_ADC_MIC(i)) {
            talk_mic_ch = i;
        }
        if (target_ch == AUDIO_ADC_MIC(i)) {
            target_ch = i;
        }
    }
#if ICSD_ADT_TALK_GAIN_SOURCE == ADT_MIC_GAIN_SOURCE_ESCO_TALK
    mic_param->mic_gain[talk_mic_ch] = audio_adc_file_get_gain(target_ch);
#else
    mic_param->mic_gain[talk_mic_ch] = audio_anc_mic_gain_get(target_ch);
#endif

    adt_debug_log("talk_mic_ch %d, gain %d", talk_mic_ch, mic_param->mic_gain[talk_mic_ch]);
}

//注册给DAC，用于当DAC 设置采样率时通知ADT算法做对应处理
void audio_icsd_adt_set_sample(int sample_rate)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    //DAC设置采样率时会重新申请buffer，DAC读指针需重新复位
    audio_dac_read_anc_reset();
    if (hdl && hdl->state) {
        if (hdl->sample_rate != sample_rate) {
            adt_debug_log("%s:%d", __func__, __LINE__);
            icsd_adt_set_audio_sample_rate(sample_rate);
        }
        hdl->sample_rate = sample_rate;
    }
}

/*获取免摘需要多少个mic*/
//gali 根据使能的接口获取算法需要开多少个MIC，目前没有独立风噪，这里需要修改
u8 get_icsd_adt_mic_num()
{
    return 3;
#if 0
    //需要根据对应功能获取这里需要多少个MIC
    if ((ICSD_WIND_EN) || ICSD_ENVNL_EN) {
        //独立风噪模式,没有免摘，只需2个 MIC
        return 2;
    } else {
        return 3;
    }
#endif
}

u8 get_icsd_adt_mic_ch(struct icsd_acoustic_detector_libfmt *libfmt)
{
    if (!libfmt) {
        return 0;
    }
    u16 mic_type = libfmt->mic_type;
    u16 mic_ch = 0;
    if (ADT_REFMIC_L & mic_type) {
        /* adt_debug_log("ADT_REFMIC_L\n"); */
        mic_ch |= icsd_get_ref_mic_L_ch();
    }
    if (ADT_REFMIC_R & mic_type) {
        /* adt_debug_log("ADT_REFMIC_R\n"); */
        mic_ch |= icsd_get_ref_mic_R_ch();
    }
    if (ADT_ERRMIC_L & mic_type) {
        /* adt_debug_log("ADT_ERRMIC_L\n"); */
        mic_ch |= icsd_get_fb_mic_L_ch();
    }
    if (ADT_ERRMIC_R & mic_type) {
        /* adt_debug_log("ADT_ERRMIC_R\n"); */
        mic_ch |= icsd_get_fb_mic_R_ch();
    }
    if (ADT_TLKMIC_L & mic_type) {
        /* adt_debug_log("ADT_TLKMIC_L\n"); */
        mic_ch |= icsd_get_talk_mic_ch();
    }
    if (ADT_TLKMIC_R & mic_type) {
        /* adt_debug_log("ADT_TLKMIC_R\n"); */
        mic_ch |= icsd_get_talk_mic_ch();
    }
    return mic_ch;
}

static void adt_env_adaptive_hold_timeout(void *p)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl == NULL) {
        return;
    }
    hdl->adt_env_adaptive_hold_id = 0;
    adt_debug_log("[ENV_ADAPTIVE]:hold time stop\n");
}

/*打开智能免摘检测*/
int audio_acoustic_detector_open()
{
    adt_log("audio_acoustic_detector_open 0x%x\n", adt_info.adt_mode);
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl == NULL) {
        return -1;
    }

    /*修改了sniff的唤醒间隔*/
    icsd_set_tws_t_sniff(160);
    int adt_function = adt_info.adt_mode;

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    if (adt_function & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
        //检查到RTANC打开限制，清除RTANC相关模式
        int ret = audio_rtanc_adaptive_init(1);
        if (ret && ret != ANC_EXT_OPEN_FAIL_REENTRY) {	//非重入异常则退出
            adt_info.adt_mode &= ~ADT_REAL_TIME_ADAPTIVE_ANC_MODE;
            if (adt_function == ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
                return -1;
            } else {
                adt_function &= ~(ADT_REAL_TIME_ADAPTIVE_ANC_MODE);
            }
        } else {
            //正常启动流程
            if ((adt_info.scene & ADT_SCENE_ESCO)) {
                hdl->libfmt.rtanc_type = mini_rtanc;
            } else {
                hdl->libfmt.rtanc_type = norm_rtanc;
            }
        }
    }
#endif

    if (adt_function & ADT_ENV_NOISE_DET_MODE) {
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
        if (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE) {
            audio_adt_avc_ioctl(ANC_EXT_IOC_INIT, 0);
        }
#endif
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
        if (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE) {
            audio_adt_env_ioctl(ANC_EXT_IOC_INIT, 0);
        }
#endif
    }

#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    audio_adt_vdt_ioctl(ICSD_ADT_IOC_INIT, 0);
#endif

    if (adt_info.adt_mode & ADT_ENV_NOISE_DET_MODE) {
        hdl->adt_env_adaptive_hold_id = sys_timeout_add(NULL, adt_env_adaptive_hold_timeout, 5000);
        adt_debug_log("[ENV_ADAPTIVE]:hold time start\n");
    }
    //启动需要初始化DAC READ相关变量，避免使用上次遗留参数
    audio_dac_read_anc_reset();
    audio_dac_set_sample_rate_callback(&dac_hdl, audio_icsd_adt_set_sample);
    /*初始化dac read的资源*/
    audio_dac_read_anc_init();

    int debug_adc_sr = 16000; //固定16k采样率

#if TCFG_AUDIO_ADAPTIVE_DCC_ENABLE  //RTANC & ADJDCC
    if (adt_function & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
        adt_function |= ADT_ADJDCC_EN;
    }
#endif

    if (adt_function & ADT_WIND_NOISE_DET_MODE) {
        if (adt_function & ADT_ADJDCC_EN) {
            hdl->libfmt.wdt_type = 3;   //ADJDCC+WIND+SELF_TALK 复用RAM
        } else {
            hdl->libfmt.wdt_type = 2;   //WIND+SELF_TALK 复用RAM
        }
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
        audio_adt_wind_ioctl(ANC_EXT_IOC_INIT, 0);
#endif
    }

    hdl->libfmt.adc_sr = debug_adc_sr;//Raymond MIC的采样率由外部决定，通过set函数通知ADT
    icsd_acoustic_detector_get_libfmt(&hdl->libfmt, adt_function);
#if (ICSD_ADT_WIND_INFO_SPP_DEBUG_EN || ICSD_ADT_VOL_NOISE_LVL_SPP_DEBUG_EN)
    /*获取spp发送句柄*/
    if (adt_info.adt_mode & (ADT_WIND_NOISE_DET_MODE | ADT_ENV_NOISE_DET_MODE)) {
        spp_get_operation_table(&hdl->spp_opt);
        if (hdl->spp_opt) {
            /*设置记录spp连接状态回调*/
            hdl->spp_opt->regist_state_cbk(hdl, icsd_adt_spp_connect_state_cbk);
            adt_debug_log("icsd adt spp debug open \n");
        }
    }
#endif

    /*获取需要打开的mic通道*/
    u16 mic_ch = get_icsd_adt_mic_ch(&hdl->libfmt);
    u16 talk_mic_ch = icsd_get_talk_mic_ch();
    u16 ref_mic_l_ch = icsd_get_ref_mic_L_ch();
    u16 fb_mic_l_ch = icsd_get_fb_mic_L_ch();
    u16 ref_mic_r_ch = icsd_get_ref_mic_R_ch();
    u16 fb_mic_r_ch = icsd_get_fb_mic_R_ch();
    adt_log("mic_ch 0x%x, talk %d, lff %d, lfb %d, rff %d, rfb %d\n",
            mic_ch, talk_mic_ch, ref_mic_l_ch, fb_mic_l_ch, ref_mic_r_ch, fb_mic_r_ch);

#ifndef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
    if (((adt_info.scene & ADT_SCENE_ESCO) || adc_file_is_runing()) && mic_ch) {
        adt_log("ERR:ICSD_ADT Without enabling the ASYNC ADC, the call doesn't support ADT.");
        return -1;
    }
#endif

#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
    hdl->infmt.TOOL_FUNCTION = icsd_adt_debug_tool_function_get();
#endif
    g_printf("mode : 0x%x, mic_type : 0x%x, mic_ch : 0x%x, adc_len : %d, sr : %d, size : %d, rtanc_type %d, w_type %d, tool 0x%x", \
             adt_function, hdl->libfmt.mic_type, mic_ch, hdl->libfmt.adc_isr_len, hdl->libfmt.adc_sr, \
             hdl->libfmt.lib_alloc_size, hdl->libfmt.rtanc_type, hdl->libfmt.wdt_type, hdl->infmt.TOOL_FUNCTION);
    if (hdl->libfmt.lib_alloc_size) {
        hdl->lib_alloc_ptr = anc_malloc("ICSD_ADT_LIB", hdl->libfmt.lib_alloc_size);
    } else {
        return -1;;
    }
    if (hdl->lib_alloc_ptr == NULL) {
        adt_log("hdl->lib_alloc_ptr malloc fail !!!");
        ASSERT(0);
        return -1;
    }

#if TCFG_AUDIO_ANC_HOWLING_DET_ENABLE
    if (adt_function & ADT_HOWLING_DET_MODE) {
        /*初始化啸叫检测资源*/
        icsd_anc_soft_howling_det_init();
    }
#endif

    if (mic_ch) {
        u8 bit_width_offset = 1;
#ifdef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
        if (adc_hdl.bit_width != ADC_BIT_WIDTH_16) {
            bit_width_offset = 2;
        }
#endif
        /*复用adc时，adc的数据需要额外申请buf存起来*/
        if (mic_ch & talk_mic_ch) {
            hdl->talk_mic_buf = anc_malloc("ICSD_ADC", hdl->libfmt.adc_isr_len * sizeof(short) * bit_width_offset);
        }
        if (mic_ch & ref_mic_l_ch) {
            hdl->lff_mic_buf = anc_malloc("ICSD_ADC", hdl->libfmt.adc_isr_len * sizeof(short) * bit_width_offset);
        }
        if (mic_ch & fb_mic_l_ch) {
            hdl->lfb_mic_buf = anc_malloc("ICSD_ADC", hdl->libfmt.adc_isr_len * sizeof(short) * bit_width_offset);
        }
        if (mic_ch & ref_mic_r_ch) {
            hdl->rff_mic_buf = anc_malloc("ICSD_ADC", hdl->libfmt.adc_isr_len * sizeof(short) * bit_width_offset);
        }
        if (mic_ch & fb_mic_r_ch) {
            hdl->rfb_mic_buf = anc_malloc("ICSD_ADC", hdl->libfmt.adc_isr_len * sizeof(short) * bit_width_offset);
        }
    }

    /*配置TWS还是头戴式耳机*/
#if TCFG_USER_TWS_ENABLE
    hdl->infmt.adt_mode = ADT_TWS;
#else
    hdl->infmt.adt_mode = ADT_HEADSET;
#endif /*TCFG_USER_TWS_ENABLE*/

    /*检测灵敏度
     * 0 : 正常灵敏度
     * 1 : 智能灵敏度*/
    hdl->infmt.sensitivity = 0;
    /*配置低压/高压DAC*/
#if ((TCFG_AUDIO_DAC_MODE == DAC_MODE_L_DIFF) || (TCFG_AUDIO_DAC_MODE == DAC_MODE_L_SINGLE))
    hdl->infmt.dac_mode = ADT_DACMODE_LOW;
#else
    hdl->infmt.dac_mode = ADT_DACMODE_HIGH;
#endif /*TCFG_AUDIO_DAC_MODE*/
    /*获取anc参数和增益*/
    hdl->infmt.gains_l_fbgain = get_anc_gains_l_fbgain();
    hdl->infmt.gains_l_ffgain = get_anc_gains_l_ffgain();
    hdl->infmt.gains_l_transgain = get_anc_gains_l_transgain();
    hdl->infmt.gains_l_transfbgain = get_anc_gains_lfb_transgain();
    hdl->infmt.trans_alogm    = audio_anc_gains_alogm_get(ANC_TRANS_TYPE);
    hdl->infmt.alogm          = audio_anc_gains_alogm_get(ANC_FF_TYPE);

    extern u32 get_anc_gains_sign();
    hdl->infmt.gain_sign = get_anc_gains_sign();

    hdl->infmt.alloc_ptr = hdl->lib_alloc_ptr;
    /*数据输出回调*/
    /* hdl->infmt.output_hdl = audio_acoustic_detector_output_hdl; */
    hdl->infmt.anc_dma_post_msg = audio_acoustic_detector_anc_dma_post_msg;
    hdl->infmt.anc_dma_output_callback = audio_acoustic_detector_anc_dma_output_callback;

    hdl->infmt.lff_gain = audio_anc_mic_gain_get_dB(icsd_get_ref_mic_L_ch(), 0);
    hdl->infmt.lfb_gain = audio_anc_mic_gain_get_dB(icsd_get_fb_mic_L_ch(), 0);
    hdl->infmt.rff_gain = audio_anc_mic_gain_get_dB(icsd_get_ref_mic_R_ch(), 0);
    hdl->infmt.rfb_gain = audio_anc_mic_gain_get_dB(icsd_get_fb_mic_R_ch(), 0);
#if ICSD_ADT_TALK_GAIN_SOURCE == ADT_MIC_GAIN_SOURCE_LFF
    hdl->infmt.talk_gain = hdl->infmt.lff_gain;
#elif ICSD_ADT_TALK_GAIN_SOURCE == ADT_MIC_GAIN_SOURCE_RFF
    hdl->infmt.talk_gain = hdl->infmt.rff_gain;
#else
    hdl->infmt.talk_gain = audio_anc_mic_gain_get_dB(icsd_get_talk_mic_ch(), 1);
#endif
    adt_log("icsd adt mic gain set: lff %d, lfb %d, rff %d, rfb %d, talk %d dB\n",
            hdl->infmt.lff_gain, hdl->infmt.lfb_gain, hdl->infmt.rff_gain,
            hdl->infmt.rfb_gain, hdl->infmt.talk_gain);
    //gain检查
    if ((hdl->libfmt.mic_type & ADT_REFMIC_L) && (hdl->libfmt.mic_type & ADT_REFMIC_R)) {
        ASSERT(hdl->infmt.lff_gain == hdl->infmt.rff_gain);
    }
    if ((hdl->libfmt.mic_type & ADT_ERRMIC_L) && (hdl->libfmt.mic_type & ADT_ERRMIC_R)) {
        ASSERT(hdl->infmt.lfb_gain == hdl->infmt.rfb_gain);
    }
    if ((hdl->libfmt.mic_type & ADT_REFMIC_L) && \
        ((hdl->libfmt.mic_type & ADT_TLKMIC_L) || (hdl->libfmt.mic_type & ADT_TLKMIC_R))) {
        ASSERT(hdl->infmt.lff_gain == hdl->infmt.talk_gain);
        if (adt_info.scene & ADT_SCENE_ESCO) {//通话状态下，校验ANC和通话TALK MIC增益是否一致
            ASSERT(hdl->infmt.lff_gain == audio_anc_mic_gain_get_dB(icsd_get_talk_mic_ch(), 1));
        }
    }

    hdl->infmt.sample_rate = 44100;
    hdl->infmt.ein_state = 0;
    hdl->infmt.adc_sr = debug_adc_sr;

#if AUDIO_RT_ANC_EXPORT_TOOL_DATA_DEBUG
    if (anc_ext_tool_online_get()) {
        hdl->infmt.rtanc_tool = anc_malloc("ICSD_DEBUG", sizeof(struct icsd_rtanc_tool_data));;
        adt_debug_log("rtanc tool demo:0x%x==================================\n", (int)hdl->infmt.rtanc_tool);
    }
#endif
    extern void icsd_task_create();
    icsd_task_create();

    icsd_acoustic_detector_set_infmt(&hdl->infmt);
    //set_icsd_adt_dma_done_flag(1);


    icsd_acoustic_detector_open();

    //算法需要的MIC和当前样机配置的MIC不匹配: 需要检测MIC配置，或可能此方案不支持部分算法
    ASSERT(audio_get_mic_num(mic_ch) == audio_get_mic_num(hdl->libfmt.mic_type), "adt need %d mic num, but now only open %d mic num, please check your anc mic and esco mic cfg !!!", audio_get_mic_num(hdl->libfmt.mic_type), audio_get_mic_num(mic_ch));
    audio_mic_param_t mic_param = {
        .mic_ch_sel        = mic_ch,
        .sample_rate       = hdl->libfmt.adc_sr,//采样率
        .adc_irq_points    = hdl->libfmt.adc_isr_len,//一次处理数据的数据单元， 单位点 4对齐(要配合mic起中断点数修改)
#ifdef TCFG_AUDIO_ADC_ENABLE_ALL_DIGITAL_CH
        //共用mic的时候，才是要保持和adc节点的一样
        .adc_buf_num       = 3,
#else
        .adc_buf_num       = 2,
#endif
    };
    for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        //需使用ANC MIC 增益，避免调试流程需要依赖通话MIC增益配置
        mic_param.mic_gain[i] = audio_anc_mic_gain_get(i);
        //mic_param.mic_gain[i] = audio_adc_file_get_gain(i);
    }

    if (mic_ch & talk_mic_ch) {
        /*ADT使用3mic时，设置talk mic gain和ff mic gain一样*/
        audio_icsd_adt_set_talk_mic_gain(&mic_param);
    }

    if (mic_ch) {
        int err = audio_mic_en(1, &mic_param, audio_mic_output_handle);
        if (err != 0) {
            adt_log("open mic fail !!!");
            goto err0;
        }
    }
    if (mic_ch & talk_mic_ch) {
        hdl->talk_mic_seq = get_adc_seq(&adc_hdl, talk_mic_ch); //查询模拟mic对应的ADC通道,要求通道连续
    }
    if (mic_ch & ref_mic_l_ch) {
        hdl->lff_mic_seq = get_adc_seq(&adc_hdl, ref_mic_l_ch);
    }
    if (mic_ch & fb_mic_l_ch) {
        hdl->lfb_mic_seq = get_adc_seq(&adc_hdl, fb_mic_l_ch);
    }
    if (mic_ch & ref_mic_r_ch) {
        hdl->rff_mic_seq = get_adc_seq(&adc_hdl, ref_mic_r_ch);
    }
    if (mic_ch & fb_mic_r_ch) {
        hdl->rfb_mic_seq = get_adc_seq(&adc_hdl, fb_mic_r_ch);
    }
    adt_log("adc seq, talk : %d, lff : %d, lfb : %d, rff : %d, rfb : %d\n", hdl->talk_mic_seq, hdl->lff_mic_seq, hdl->lfb_mic_seq, hdl->rff_mic_seq, hdl->rfb_mic_seq);
    audio_icsd_adt_start();

    hdl->state = 1;
    return 0;
err0:
    audio_acoustic_detector_close();
    return -1;
}

/*关闭智能免摘检测*/
int audio_acoustic_detector_close()
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl) {
        if (hdl->adt_env_adaptive_hold_id) {
            sys_timeout_del(hdl->adt_env_adaptive_hold_id);
            hdl->adt_env_adaptive_hold_id = 0;
        }
        audio_dac_del_sample_rate_callback(&dac_hdl, audio_icsd_adt_set_sample);
        audio_mic_en(0, NULL, NULL);
        //set_icsd_adt_dma_done_flag(0);
        if (hdl->lib_alloc_ptr) {
            /*需要先挂起再关闭*/
            audio_icsd_adt_algom_suspend();
            adt_debug_log("icsd_acoustic_detector_close, 0\n");
            icsd_acoustic_detector_close();
            adt_debug_log("icsd_acoustic_detector_close, 1\n");
            extern void icsd_task_kill();
            icsd_task_kill();

            anc_free(hdl->lib_alloc_ptr);
            hdl->lib_alloc_ptr = NULL;
        }
        if (hdl->talk_mic_buf) {
            anc_free(hdl->talk_mic_buf);
            hdl->talk_mic_buf = NULL;
        }
        if (hdl->lff_mic_buf) {
            anc_free(hdl->lff_mic_buf);
            hdl->lff_mic_buf = NULL;
        }
        if (hdl->lfb_mic_buf) {
            anc_free(hdl->lfb_mic_buf);
            hdl->lfb_mic_buf = NULL;
        }
        if (hdl->rff_mic_buf) {
            anc_free(hdl->rff_mic_buf);
            hdl->rff_mic_buf = NULL;
        }
        if (hdl->rfb_mic_buf) {
            anc_free(hdl->rfb_mic_buf);
            hdl->rfb_mic_buf = NULL;
        }
#if AUDIO_RT_ANC_EXPORT_TOOL_DATA_DEBUG
        if (hdl->infmt.rtanc_tool) {
            anc_free(hdl->infmt.rtanc_tool);
            hdl->infmt.rtanc_tool = NULL;
        }
#endif
        /*释放dac read的资源*/
        audio_dac_read_anc_exit();
    }
    adt_log("audio_acoustic_detector_close, %d end \n", __LINE__);
    return 0;
}

//ADT挂起, 可用于避免滤波器增益突变导致ADT异常
void audio_icsd_adt_suspend()
{
    //其他线程调用
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        hdl->busy = 1;
        adt_debug_log("%s : %d", __func__, __LINE__);
        if (hdl->resume_timer) {
            sys_s_hi_timeout_del(hdl->resume_timer);
        }
        audio_icsd_adt_algom_suspend();
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
        audio_adt_vdt_ioctl(ICSD_ADT_IOC_SUSPEND, 0);
#endif
        hdl->busy = 0;
    }
}

//ANC更新滤波器、增益时需要通知免摘更新算法参数
int audio_icsd_adt_param_update(void)
{
    //其他线程调用
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        hdl->busy = 1;
        adt_debug_log("%s : %d", __func__, __LINE__);
        audio_acoustic_detector_updata();
        hdl->busy = 0;
    }
    return 0;
}

static void audio_icsd_adt_resume_timer(void *p)
{
    //中断调用
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    adt_debug_log("%s\n", __func__);
    if (hdl && hdl->state) {
        hdl->busy = 1;
        //挂起后恢复ADT
        audio_icsd_adt_algom_resume(anc_mode_get());

        hdl->resume_timer = 0;
        hdl->busy = 0;
    }
}

//ADT恢复, 与挂起成对使用
void audio_icsd_adt_resume()
{
    //其他线程调用
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        hdl->busy = 1;
        adt_debug_log("%s : %d", __func__, __LINE__);

        if (hdl->resume_timer == 0) {
            hdl->resume_timer = sys_s_hi_timerout_add(NULL, audio_icsd_adt_resume_timer, 500);
        }
        hdl->busy = 0;
    }
}

void audio_adt_rtanc_sync_tone_resume(void)
{
    u8 data[4];
    data[0] = SYNC_ICSD_ADT_RTANC_TONE_RESUME;
    audio_icsd_adt_info_sync(data, 4);
}

void audio_adt_tone_adaptive_sync(void)
{
    u8 data[4];
    data[0] = SYNC_ICSD_ADT_TONE_ADAPTIVE_SYNC;
    audio_icsd_adt_info_sync(data, 4);
}

void audio_icsd_adt_algom_suspend(void)
{
    //可能其他线程调用
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        icsd_acoustic_detector_suspend();
    }
}

void audio_icsd_adt_algom_resume(u8 anc_mode)
{
    u8 mode = anc_mode - 1;	//ANC_OFF = 1
    if (anc_mode >= ANC_OFF && anc_mode <= ANC_TRANSPARENCY) {
        /* adt_log("%s, %d, %d\n", __func__, adt_resume_mode[mode][0], adt_resume_mode[mode][1]); */
        icsd_acoustic_detector_resume(adt_resume_mode[mode][0], adt_resume_mode[mode][1]);
    }
}

/*adt是否在跑*/
u8 audio_icsd_adt_is_running()
{
    return (speak_to_chat_hdl != NULL);
}

/*获取adt的模式*/
u16 get_icsd_adt_mode()
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    return adt_info.adt_mode;
}

/*ADT TWS同步执行接口 - 关闭ADT*/
static void audio_icsd_adt_state_sync_done(struct anc_tws_sync_info *info)
{

    adt_log("%s, adt_mode 0x%x, chat %d", __func__, info->app_adt_mode, info->vdt_state);
    put_buf((u8 *)info, sizeof(struct anc_tws_sync_info));

#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    audio_vdt_tws_trigger_set(info->vdt_state);
#endif

    adt_info.app_adt_mode = info->app_adt_mode;

    if (anc_mode_sync(info)) {
        //ANC相同模式无法切换，则在此处复位ADT
        audio_icsd_adt_res_close(0, 0);
    }
}

/*获取ADT模式切换是否繁忙*/
u8 get_icsd_adt_mode_switch_busy()
{
    return adt_info.mode_switch_busy;
}

#if TCFG_USER_TWS_ENABLE
/*同步tws配对时，同步adt的状态*/
void audio_anc_icsd_adt_state_sync(struct anc_tws_sync_info *info)
{
    adt_debug_log("audio_anc_icsd_adt_state_sync");
    int len = sizeof(struct anc_tws_sync_info);
    struct anc_tws_sync_info *sync_data = anc_malloc("ICSD_ADT", len);
    if (!sync_data) {
        return;
    }
    memcpy(sync_data, info, len);
    //put_buf((u8*)sync_data, len);
    int err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_STATE_SYNC, (int)sync_data);
    if (err != OS_NO_ERR) {
        anc_free(sync_data);
        adt_log("audio_anc_icsd_adt_state_sync err %d", err);
    }
}
#endif

//ADT 播放完提示音回调接口
void icsd_adt_task_play_tone_cb(void *priv)
{
    //To do
}

static void audio_icsd_adt_task(void *p)
{
    adt_debug_log("%s", __func__);
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    int msg[16];
    char tmpbuf[25];
    int res;
    int ret;
    u16 anc_fade_gain;

    while (1) {
        int msg[16] = {0};
        res = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            switch (msg[1]) {
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
            case ICSD_ADT_VDT_TIRRGER:
                hdl = speak_to_chat_hdl;
                if (hdl && hdl->state) {
                    audio_vdt_trigger_process(1);
                }
                break;
            case ICSD_ADT_VDT_RESUME:
                hdl = speak_to_chat_hdl;
                if (hdl && hdl->state) {
                    audio_vdt_resume_process();
                }
                break;
            case ICSD_ADT_VDT_KEEP_STATE:
                adt_log("ICSD_ADT_VDT_KEEP_STATE: %d\n", msg[2]);
                audio_vdt_keep_state_set(msg[2]);
                break;
#endif /*TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE*/
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
            case ICSD_ADT_WIND_LVL:
                hdl = speak_to_chat_hdl;
                if (hdl && hdl->state) {
                    hdl->busy = 1;
                    u8 wind_lvl = (u8)msg[2];
                    static u32 next_period = 0;
                    /*间隔200ms以上发送一次数据*/
                    if (time_after(jiffies, next_period)) {
                        next_period = jiffies + msecs_to_jiffies(200);
                        /* adt_debug_log("[task]wind_lvl : %d", wind_lvl); */
                        /*蓝牙spp发送风噪值*/
#if ICSD_ADT_WIND_INFO_SPP_DEBUG_EN
                        /*spp是否连接 && spp是否初始化 && spp发送是否初始化*/
                        if (hdl->spp_opt && hdl->spp_opt->send_data) {
                            memset(tmpbuf, 0x20, sizeof(tmpbuf));//填充空格
                            sprintf(tmpbuf, "wind lvl:%d\r\n", wind_lvl);
                            /*关闭蓝牙sniff，再发送数据*/
                            bt_sniff_disable();
#if TCFG_USER_TWS_ENABLE
                            if (get_tws_sibling_connect_state() && (tws_api_get_role() == TWS_ROLE_MASTER)) {
                                /*主机发送*/
                                hdl->spp_opt->send_data(NULL, tmpbuf, sizeof(tmpbuf));
                            } else
#endif /*TCFG_USER_TWS_ENABLE*/
                            {
                                hdl->spp_opt->send_data(NULL, tmpbuf, sizeof(tmpbuf));

                            }
                        }
#endif /*ICSD_ADT_WIND_INFO_SPP_DEBUG_EN*/
                    }
                    /*风噪处理*/
                    ret = audio_anc_wind_noise_process(wind_lvl);
                    if (ret != -1) {
                        anc_fade_gain = ret;
                    }

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
                    //触发风噪检测之后需要挂起RTANC
                    if (audio_anc_real_time_adaptive_state_get() && (ret != -1)) {
                        if (anc_fade_gain != 16384) {
                            if (anc_wind_det_suspend_id) {
                                sys_timeout_del(anc_wind_det_suspend_id);
                            }
                            audio_anc_real_time_adaptive_suspend("ANC_WIND_DET");
                        }
                    }
#endif

                    hdl->busy = 0;
                }
                break;
#endif /*TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE*/
#if TCFG_AUDIO_WIDE_AREA_TAP_ENABLE
            case ICSD_ADT_WAT_RESULT:
                hdl = speak_to_chat_hdl;
                if (hdl && hdl->state) {
                    hdl->busy = 1;
                    u8 wat_result = (u8)msg[2];
                    adt_debug_log("[task]wat_result : %d", wat_result);
                    hdl->adt_wat_result = wat_result;
                    audio_wat_area_tap_event_handle(hdl->adt_wat_result);
                    hdl->busy = 0;
                }
                break;
#endif /*TCFG_AUDIO_WIDE_AREA_TAP_ENABLE*/
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
            case ICSD_ADT_ENV_NOISE_LVL:
            case ICSD_ADT_AVC_NOISE_LVL:
                //env avc共用流程
                hdl = speak_to_chat_hdl;
                if (hdl && hdl->state) {
                    hdl->busy = 1;
                    u8 noise_lvl = (u8)msg[2];
                    static u32 next_period = 0;
                    /*间隔200ms以上发送一次数据*/
                    if (time_after(jiffies, next_period)) {
                        next_period = jiffies + msecs_to_jiffies(200);
                        /* adt_debug_log("[task]vol noise_lvl : %d", noise_lvl); */
                        /*蓝牙spp发送风噪值*/
#if ICSD_ADT_VOL_NOISE_LVL_SPP_DEBUG_EN
                        /*spp是否连接 && spp是否初始化 && spp发送是否初始化*/
                        if (hdl->spp_opt && hdl->spp_opt->send_data) {
                            memset(tmpbuf, 0x20, sizeof(tmpbuf));//填充空格
                            sprintf(tmpbuf, "noise lvl:%d\r\n", noise_lvl);
                            /*关闭蓝牙sniff，再发送数据*/
                            bt_sniff_disable();
#if TCFG_USER_TWS_ENABLE
                            if (get_tws_sibling_connect_state() && (tws_api_get_role() == TWS_ROLE_MASTER)) {
                                /*主机发送*/
                                hdl->spp_opt->send_data(NULL, tmpbuf, sizeof(tmpbuf));
                            } else
#endif /*TCFG_USER_TWS_ENABLE*/
                            {
                                hdl->spp_opt->send_data(NULL, tmpbuf, sizeof(tmpbuf));

                            }
                        }
#endif /*ICSD_ADT_WIND_INFO_SPP_DEBUG_EN*/
                    }
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
                    if (msg[1] == ICSD_ADT_AVC_NOISE_LVL) {
                        audio_icsd_adptive_vol_event_process(noise_lvl);
                    }
#endif
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
                    if (msg[1] == ICSD_ADT_ENV_NOISE_LVL) {
                        ret = audio_env_noise_event_process(noise_lvl);
                        if (ret != -1) {
                            anc_fade_gain = ret;
                        }
                    }
#endif

                    hdl->busy = 0;
                }
                break;
#endif /*TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE*/
            case ICSD_ADT_TONE_PLAY:
                /*播放提示音,定时器里面需要播放提示音时，可发消息到这里播放*/
                u8 index = (u8)msg[2];
                int err = icsd_adt_tone_play_callback(index, icsd_adt_task_play_tone_cb, NULL);
                break;
            case SPEAK_TO_CHAT_TASK_KILL:
                /*清掉队列消息*/
                os_taskq_flush();
                os_sem_post((OS_SEM *)msg[2]);
                break;
            case ICSD_ANC_MODE_SWITCH:
                adt_log("ICSD_ANC_MODE_SWITCH");
                anc_mode_switch_base((u8)msg[2], (u8)msg[3]);
                break;
            case SYNC_ICSD_ADT_OPEN:
                adt_log("SYNC_ICSD_ADT_OPEN");
                adt_info.mode_switch_busy = 1;
                audio_icsd_adt_open(msg[2]);
                adt_info.mode_switch_busy = 0;
                break;
            case SYNC_ICSD_ADT_CLOSE:
                adt_log("SYNC_ICSD_ADT_CLOSE");
                adt_info.mode_switch_busy = 1;
                audio_icsd_adt_res_close(msg[2], msg[3]);
                adt_info.mode_switch_busy = 0;
                if (msg[5]) {
                    adt_log("close sem 0x%x\n", (int)(msg[5]));
                    os_sem_post((OS_SEM *)(msg[5]));
                }
                //还原当前用户模式
                if (anc_real_mode_get() == ANC_EXT && (!audio_icsd_adt_scene_check(adt_info.adt_mode, 0))) {
                    adt_log("ADT: anc mode change: ANC_EXT-> %d\n", anc_user_mode_get());
                    anc_mode_switch_base(anc_user_mode_get(), 0);
                }
                break;
            case ICSD_ADT_ANC_SWITCH_OPEN:
                adt_log("ICSD_ADT_ANC_SWITCH_OPEN: %x\n", msg[2]);
                adt_info.mode_switch_busy = 1;
                audio_icsd_adt_open(msg[2]);
                adt_info.mode_switch_busy = 0;
                break;
            case ICSD_ADT_ANC_SWITCH_CLOSE:
                adt_log("ICSD_ADT_ANC_SWITCH_CLOSE: %x\n", msg[2]);
                adt_info.mode_switch_busy = 1;
                audio_icsd_adt_res_close(msg[2], msg[3]);
                adt_info.mode_switch_busy = 0;
                break;
            case SYNC_ICSD_ADT_SET_ANC_FADE_GAIN:
                adt_log("adt_sync_mode %d sync_set  [anc_fade]: %d, fade_time : %d\n", msg[3], msg[2], msg[4]);
                if (msg[3] == ANC_FADE_MODE_ENV_ADAPTIVE_GAIN) {
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
                    audio_anc_env_adaptive_fade_gain_set(msg[2], msg[4]);
#endif
                } else if (msg[3] == ANC_FADE_MODE_WIND_NOISE) {
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
                    audio_anc_wind_noise_fade_gain_set(msg[2], msg[4]);
#endif
                } else {
                    icsd_anc_fade_set(msg[3], msg[2]);
                }
                break;
#if TCFG_USER_TWS_ENABLE
            case ICSD_ADT_STATE_SYNC:
                adt_log("ICSD_ADT_STATE_SYNC");
                struct anc_tws_sync_info *info = (struct anc_tws_sync_info *)msg[2];
                /*先关闭adt，同步adt状态，然后同步anc，在anc里面做判断开adt*/
                audio_icsd_adt_state_sync_done(info);
                anc_free(info);
                break;
#endif
#if ICSD_ADT_MIC_DATA_EXPORT_EN
            case ICSD_ADT_EXPORT_DATA_WRITE:
                aec_uart_write();
                break;
#endif /*ICSD_ADT_MIC_DATA_EXPORT_EN*/
            default:
                break;
            }
        }
    }
}

/*
   ADT 关联模块启动限制
   return 0 不允许打开，return 1 允许打开
 */
int audio_icsd_adt_open_permit(u16 adt_mode)
{
#if TCFG_ANC_TOOL_DEBUG_ONLINE
    /*连接anc spp 工具的时候不允许打开*/
    if (get_app_anctool_spp_connected_flag()) {
        adt_debug_log("anctool spp connected !!!");
        //由于免摘需要无线调试，所以支持打开在线调试
        //return 0;
    }
#endif
#if ((defined TCFG_AUDIO_FIT_DET_ENABLE) && TCFG_AUDIO_FIT_DET_ENABLE)
    //贴合度检测状态下不允许其他模式切换
    if (audio_icsd_dot_is_running()) {
        adt_debug_log("audio icsd_dot is running!!\n");
        return 0;
    }
#endif
    return 1;
}

//adt early init
int anc_adt_init()
{
    /*先创建任务，开关adt的操作在任务里面处理*/
    icsd_adt_lock_init();
    task_create(audio_icsd_adt_task, NULL, SPEAK_TO_CHAT_TASK_NAME);
    /*注册DMA 中断回调*/
    audio_anc_event_notify(ANC_EVENT_ADT_INIT, 0);
    audio_anc_dma_add_output_handler("ANC_ADT", icsd_adt_dma_done);
    return 0;
}

int audio_icsd_adt_init()
{
    adt_debug_log("%s", __func__);
    struct speak_to_chat_t *hdl = NULL;

    if (speak_to_chat_hdl) {
        adt_debug_log("speak_to_chat_hdl  is areadly open !!");
        return 0;
    }
    speak_to_chat_hdl = anc_malloc("ICSD_ADT", sizeof(struct speak_to_chat_t));
    if (speak_to_chat_hdl == NULL) {
        adt_debug_log("speak_to_chat_hdl malloc fail !!!");
        adt_info.adt_mode = ADT_MODE_CLOSE;
        ASSERT(0);
        return -1;
    }
    hdl = speak_to_chat_hdl;
    hdl->sync_cnt = anc_malloc("ICSD_ADT", ARRAY_SIZE(adt_tws_rt_sync_table));
    hdl->drc_gain[0] = 10000.0f;
    hdl->drc_gain[1] = 10000.0f;
    hdl->drc_gain[2] = 10000.0f;
    hdl->drc_gain[3] = 10000.0f;

#if ICSD_ADT_MIC_DATA_EXPORT_EN
    aec_uart_open(3, 512);
#endif /*ICSD_ADT_MIC_DATA_EXPORT_EN*/

    /* task_create(audio_icsd_adt_task, hdl, SPEAK_TO_CHAT_TASK_NAME); */
    audio_acoustic_detector_open();
    icsd_adt_clock_add();
    /*关闭蓝牙sniff*/
#if TCFG_USER_TWS_ENABLE
    icsd_bt_sniff_set_enable(0);
#endif
    return 0;
}

int audio_icsd_adt_scene_check(u16 adt_mode, u8 skip_check)
{
    int x, y;
    int scene_size = sizeof(anc_ext_support_scene) / sizeof(anc_ext_support_scene[0]);
    int alogm_size = sizeof(anc_ext_support_scene[0]) / sizeof(anc_ext_support_scene[0][0]);
    u16 tmp_mode, out_mode = 0;

    adt_info.record_adt_mode |= adt_mode;

    //处理 SDK FIX SCENE, 与SDK强制互斥的场景
    if (adt_info.scene >> ADT_FIX_MUTEX_SCENE_OFFSET) {
        goto __exit;
    }
    /* printf("scene_size %d, alogm_size %d\n", scene_size, alogm_size); */
    for (y = 0; y < alogm_size; y++) {
        tmp_mode = (adt_info.record_adt_mode & BIT(y)) ? 1 : 0;
        for (x = 0; x < scene_size; x++) {
            if (adt_info.scene & BIT(x)) {
                tmp_mode &= anc_ext_support_scene[x][y];
            }
        }
        out_mode |= (tmp_mode << y);
    }

    //用于特殊场景 事件处理
    int arg[2] = {out_mode, skip_check};
    out_mode = audio_anc_event_notify(ANC_EVENT_ADT_SCENE_CHECK, (int)(&arg));

    if ((out_mode & (ADT_REAL_TIME_ADAPTIVE_ANC_MODE | ADT_REAL_TIME_ADAPTIVE_ANC_TIDY_MODE)) &&
        (out_mode &	ADT_SPEAK_TO_CHAT_MODE)) {
        //RTANC 与 智能免摘不共存
        printf("adt_scene_check:ERR SPEAK_TO_CHAT & RTANC Non-coexistence\n");
        return 0;
    }

__exit:
    printf("adt_scene_check: scene 0x%x, in 0x%x, re 0x%x; out 0x%x\n", adt_info.scene, adt_mode, adt_info.record_adt_mode, out_mode);

    return out_mode;
}

//启动ADT
int audio_icsd_adt_open(u16 adt_mode)
{
    //场景筛选出模式
    int adt_check_mode = audio_icsd_adt_scene_check(adt_mode | adt_info.adt_mode, 0);

    //若符合场景的模式为0，直接退出
    if (!adt_check_mode) {
        return 0;
    }
    adt_debug_log("%s:0x%x\n", __func__, adt_check_mode);

    //算法限制检查
    if (audio_icsd_adt_open_permit(adt_mode) == 0) {
        return -1;
    }

    if (anc_real_mode_get() == ANC_OFF) {
        adt_log("ADT_OPEN: anc mode change: ANC_OFF->ANC_EXT\n");
        anc_mode_switch_base(ANC_EXT, 0);
        return 0;
    }

    //重入判断, 当adt_mode = 0, 认为需要重启算法
    if (!(~adt_info.adt_mode & adt_check_mode) && adt_mode && speak_to_chat_hdl) {
        adt_log("adt_open: 0x%x, now_mode = 0x%x, adt has been opened\n", adt_check_mode, adt_info.adt_mode);
        return 0;
    }

    /*如果算法已经打开时，需要关闭在重新打开*/
    if (adt_check_mode && speak_to_chat_hdl) {
        //gali 需要修改 考虑能否一个独立挂起的接口，与挂起流程独立，减少交叉
        adt_log("adt_open: 0x%x, do reset\n", adt_check_mode);
        audio_icsd_adt_res_close(0, 0);
        return 0;
    }

    audio_anc_event_notify(ANC_EVENT_ADT_OPEN, adt_mode);

    adt_info.adt_mode = adt_check_mode;

    audio_icsd_adt_init();

    return 0;
}

//非对耳同步打开
int audio_icsd_adt_no_sync_open(u16 adt_mode)
{
    int err = 0;
    err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, SYNC_ICSD_ADT_OPEN, adt_mode);
    if (err != OS_NO_ERR) {
        adt_log("audio_icsd_adt_no_sync_open err %d", err);
    }
    return err;
}


/*同步打开，
 *ag: audio_icsd_adt_sync_open(ADT_SPEAK_TO_CHAT_MODE | ADT_WIDE_AREA_TAP_MODE | ADT_WIND_NOISE_DET_MODE) */
int audio_icsd_adt_sync_open(u16 adt_mode)
{
    /* uint32_t rets_addr; */
    /* __asm__ volatile("%0 = rets ;" : "=r"(rets_addr)); */
    /* adt_log("audio_icsd_adt_sync_open, mode=0x%x, rets=0x%x\n", adt_mode, rets_addr); */
    adt_debug_log("%s, adt_mode 0x%x", __func__, adt_mode);
    u8 data[4] = {0};
    data[0] = SYNC_ICSD_ADT_OPEN;
    memcpy(&(data[1]), &adt_mode, sizeof(u16));

    if (audio_icsd_adt_open_permit(adt_mode) == 0) {
        return -1;
    }
#if TCFG_USER_TWS_ENABLE
    if (get_tws_sibling_connect_state()) {
        if ((tws_api_get_role() == TWS_ROLE_MASTER)) {
            /*同步主机状态打开*/
            audio_icsd_adt_info_sync(data, 4);
        }
    } else
#endif
    {
        audio_icsd_adt_info_sync(data, 4);
    }
    return 0;
}

/*同步关闭，
 *ag: audio_icsd_adt_sync_close(ADT_SPEAK_TO_CHAT_MODE | ADT_WIDE_AREA_TAP_MODE | ADT_WIND_NOISE_DET_MODE) */
int audio_icsd_adt_sync_close(u16 adt_mode, u8 suspend)
{
    adt_debug_log("%s, adt_mode 0x%x", __func__, adt_mode);
    u8 data[4] = {0};
    data[0] = SYNC_ICSD_ADT_CLOSE;
    memcpy(&(data[1]), &adt_mode, sizeof(u16));
    data[3] = suspend;
#if TCFG_USER_TWS_ENABLE
    if (get_tws_sibling_connect_state()) {
        if ((tws_api_get_role() == TWS_ROLE_MASTER)) {
            /*同步主机状态打开*/
            audio_icsd_adt_info_sync(data, 4);
        }
    } else
#endif
    {
        audio_icsd_adt_info_sync(data, 4);
    }
    return 0;
}

int audio_icsd_adt_scene_set(enum ICSD_ADT_SCENE scene, u8 enable)
{
    if (enable) {
        adt_info.scene |= scene;
    } else {
        adt_info.scene &= ~scene;
    }
    adt_log("audio_icsd_adt_scene_set 0x%x:%d, scene 0x%x\n", scene, enable, adt_info.scene);
    return 0;
}

enum ICSD_ADT_SCENE audio_icsd_adt_scene_get(void)
{
    return adt_info.scene;
}

/*
	关闭ADT
	adt_mode 对应模式；suspend 挂起标志
*/
int audio_icsd_adt_close(u32 scene, u8 run_sync, u16 adt_mode, u8 suspend)
{
    int err = 0;
    OS_SEM sem;
    u32 last_jiffies = jiffies_usec();
    adt_log("audio_icsd_adt_close start 0x%x\n", adt_mode);
    if (run_sync) {
        os_sem_create(&sem, 0);
        adt_log("send sem : %x\n", (int)(&sem));
        err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 5, SYNC_ICSD_ADT_CLOSE, adt_mode, suspend, scene, &sem);
    } else {
        err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 5, SYNC_ICSD_ADT_CLOSE, adt_mode, suspend, scene, 0);
    }
    if (err != OS_NO_ERR) {
        adt_log("%s err %d", __func__, err);
    } else if (run_sync) {
        adt_log("pend sem\n");
        os_sem_pend(&sem, 0);
    }
    adt_log("audio_icsd_adt_close end, %d\n", jiffies_usec2offset(last_jiffies, jiffies_usec()));
    return err;
}

//ANC模式切换时启动ADT
int audio_icsd_anc_switch_open(u16 adt_mode)
{
    int err = 0;
    err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME, 2, ICSD_ADT_ANC_SWITCH_OPEN, adt_mode);
    if (err != OS_NO_ERR) {
        adt_log("audio_icsd_anc_switch_open err %d", err);
    }
    return err;
}

//ANC模式切换时关闭ADT
int audio_icsd_anc_switch_close(u16 adt_mode)
{
    int err = 0;
    err = os_taskq_post_msg(SPEAK_TO_CHAT_TASK_NAME,  5, ICSD_ADT_ANC_SWITCH_CLOSE, adt_mode, 0, 0, 0);
    if (err != OS_NO_ERR) {
        adt_log("audio_icsd_anc_switch_close err %d", err);
    }
    return err;
}


//总退出接口
static int audio_icsd_adt_res_close(u16 adt_mode, u8 suspend)
{
    adt_debug_log("%s: 0x%x, 0x%x", __func__, adt_mode, adt_info.adt_mode);
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl) {
        if (hdl->state) {
            if (!(adt_info.adt_mode & adt_mode) && adt_mode) {
                adt_log("adt mode : 0x%x is alreadly closed !!!", adt_mode);
                return 0;
            }
            adt_info.adt_mode &= ~adt_mode;
            adt_info.record_adt_mode &= ~adt_mode;

            hdl->state = 0;
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
            DeAlorithm_disable();
            while (hdl->busy || audio_anc_real_time_adaptive_run_busy_get()) {
#else
            while (hdl->busy) {
#endif
                putchar('w');
                os_time_dly(1);
            }
            audio_acoustic_detector_close();

            audio_anc_event_notify(ANC_EVENT_ADT_CLOSE, adt_mode);

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
            if ((adt_info.adt_mode | adt_mode) & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
                audio_rtanc_adaptive_exit();
            }
#endif

#if TCFG_AUDIO_ANC_HOWLING_DET_ENABLE
            if ((adt_info.adt_mode | adt_mode) & ADT_HOWLING_DET_MODE) {
                /*关闭啸叫检测资源*/
                icsd_anc_soft_howling_det_exit();
            }
#endif

#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
            if ((adt_info.adt_mode | adt_mode) & ADT_SPEAK_TO_CHAT_MODE) {
                audio_adt_vdt_ioctl(ICSD_ADT_IOC_EXIT, 0);
            }
#endif

            if ((adt_info.adt_mode | adt_mode) & ADT_ENV_NOISE_DET_MODE) {
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_VOLUME_ENABLE
                if (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_VOLUME_ADAPTIVE) {
                    audio_adt_avc_ioctl(ANC_EXT_IOC_EXIT, 0);
                }
#endif
#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
                if (audio_icsd_adt_env_noise_det_get() & AUDIO_ANC_ENV_NOISE_DET_GAIN_ADAPTIVE) {
                    audio_adt_env_ioctl(ANC_EXT_IOC_EXIT, 0);
                }
#endif
            }

#if ICSD_ADT_MIC_DATA_EXPORT_EN
            aec_uart_close();
#endif /*ICSD_ADT_MIC_DATA_EXPORT_EN*/

            /*打开蓝牙sniff*/
#if TCFG_USER_TWS_ENABLE
            icsd_bt_sniff_set_enable(1);
#endif

            //关闭风噪检测时恢复现场
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
            if (adt_mode & ADT_WIND_NOISE_DET_MODE) {
                audio_adt_wind_ioctl(ANC_EXT_IOC_EXIT, 0);
                icsd_anc_fade_set(ANC_FADE_MODE_WIND_NOISE, 16384);
                audio_anc_wind_noise_fade_param_reset();
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
                //恢复RT_ANC 相关标志/状态
                audio_anc_real_time_adaptive_resume("ANC_WIND_DET");
#endif
            }
#endif

#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
            if (adt_mode & ADT_ENV_NOISE_DET_MODE) {
                icsd_anc_fade_set(ANC_FADE_MODE_ENV_ADAPTIVE_GAIN, 16384);
                audio_anc_env_adaptive_fade_param_reset();
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
                if (anc_env_adaptive_gain_suspend) {
                    anc_env_adaptive_gain_suspend = 0;
                    audio_anc_real_time_adaptive_resume("ENV_ADAPTIVE");
                }
#endif
            }
#endif
        }

        anc_free(hdl->sync_cnt);
        anc_free(hdl);
        speak_to_chat_hdl = NULL;
        icsd_adt_clock_del();

    } else {
        adt_info.adt_mode &= ~adt_mode;
        adt_info.record_adt_mode &= ~adt_mode;
    }
    /*如果adt没有全部关闭，需要重新打开
     *如果是通话时关闭的，直接关闭adt*/
    adt_debug_log("adt close: adt_mode 0x%x, adc run %d, suspend %d\n", adt_info.adt_mode, adc_file_is_runing(), suspend);
    if (!suspend) {
        /* if (adt_info.adt_mode && !(esco_player_runing() || mic_effect_player_runing()) && !suspend) { */
        audio_icsd_adt_open(0);
    }
    return 0;
}

static int audio_icsd_adt_reset_base(u32 scene, u8 sync)
{
    audio_anc_event_notify(ANC_EVENT_ADT_RESET, scene);
    u8 suspend = (scene >> ADT_FIX_MUTEX_SCENE_OFFSET) ? 1 : 0;
    //固定互斥的场景 suspend = 1, 避免adt_open重复启用
    audio_icsd_adt_close(0, sync, 0, suspend);
    return 0;
}

/*阻塞性 重启ADT算法*/
int audio_icsd_adt_reset(u32 scene)
{
    return audio_icsd_adt_reset_base(scene, 1);
}

/*异步 重启ADT算法*/
int audio_icsd_adt_async_reset(u32 scene)
{
    return audio_icsd_adt_reset_base(scene, 0);
}

/************************* start 广域点击相关接口 ***********************/
#if TCFG_AUDIO_WIDE_AREA_TAP_ENABLE
/*广域点击识别结果输出回调*/
void audio_wat_click_output_handle(u8 wat_result)
{
    /* adt_debug_log("%s, wat_result:%d", __func__, wat_result); */
    u8 data[4] = {SYNC_ICSD_ADT_WAT_RESULT, wat_result, 0, 0};
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        hdl->busy = 1;
        if (wat_result && !hdl->adt_wat_ignore_flag) {

#if TCFG_USER_TWS_ENABLE
            if (tws_in_sniff_state()) {
                /*如果在蓝牙siniff下需要退出蓝牙sniff再发送*/
                icsd_adt_tx_unsniff_req();
            }
#endif
            /*没有tws时直接更新状态*/
            audio_icsd_adt_info_sync(data, 4);
        }
        hdl->busy = 0;
    }
}

void audio_wide_area_tap_ingre_flag_timer(void *p)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && (adt_info.adt_mode & ADT_WIDE_AREA_TAP_MODE)) {
        /*恢复响应*/
        adt_debug_log("wat ingore end");
        hdl->adt_wat_ignore_flag = 0;
    }
}

/*设置是否忽略广域点击
 * ignore: 1 忽略点击，0 响应点击
 * 忽略点击的时间，单位ms*/
int audio_wide_area_tap_ignore_flag_set(u8 ignore, u16 time)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && (adt_info.adt_mode & ADT_WIDE_AREA_TAP_MODE)) {
        hdl->adt_wat_ignore_flag = ignore;
        if (hdl->adt_wat_ignore_flag) {
            /*如果忽略点击，定时恢复响应*/
            adt_debug_log("wat ingore start");
            sys_s_hi_timerout_add(NULL, audio_wide_area_tap_ingre_flag_timer, time);
        }
        return 0;
    } else {
        return -1;
    }
}

#endif /*TCFG_AUDIO_WIDE_AREA_TAP_ENABLE*/
/************************* end 广域点击相关接口 ***********************/


/************************* satrt 风噪检测相关接口 ***********************/
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
/*风噪检测识别结果输出回调*/
void audio_icsd_wind_detect_output_handle(u8 wind_thr)
{
    /* adt_debug_log("%s, wind_lvl:%d", __func__, wind_lvl); */
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl && hdl->state) {
        int wind_lvl = audio_anc_wind_thr_to_lvl((int)wind_thr);
        if (wind_lvl == -1) {
            return;
        }
        hdl->busy = 1;
        u8 data[4] = {AUDIO_ANC_LVL_SYNC_CMP, (u8)wind_lvl, 0, 0};

        static u32 next_period = 0;
        /*间隔200ms以上发送一次数据*/
        if (time_after(jiffies, next_period)) {
            next_period = jiffies + msecs_to_jiffies(100);
            struct audio_anc_lvl_sync *lvl_hdl = audio_anc_lvl_sync_get_hdl_by_name(ANC_LVL_SYNC_WIND);
            if (lvl_hdl) {
                adt_log("wind cur_lvl %d, wind_lvl %d\n", lvl_hdl->cur_lvl, wind_lvl);
                if (lvl_hdl->cur_lvl == wind_lvl) {
                    hdl->busy = 0;
                    return;
                }
            }
#if TCFG_USER_TWS_ENABLE
            if (tws_in_sniff_state()) {
                /*如果在蓝牙siniff下需要退出蓝牙sniff再发送*/
                icsd_adt_tx_unsniff_req();
            }
            if (get_tws_sibling_connect_state()) {
                /*记录本地的风噪强度*/
                /*同步主机风噪和从机风噪比较*/
                /* adt_log("[wind]SYNC_ICSD_ADT_WIND_LVL_CMP %d\n", wind_lvl); */
                data[0] = AUDIO_ANC_LVL_SYNC_CMP;
                data[1] = wind_lvl;
                audio_anc_wind_lvl_sync_info(data, 4);
            } else
#endif
            {
                /*没有tws时直接更新状态*/
                /* adt_log("[wind]SYNC_ICSD_ADT_WIND_LVL_RESULT single %d\n", wind_lvl); */
                data[0] = AUDIO_ANC_LVL_SYNC_RESULT;
                data[1] = wind_lvl;
                audio_anc_wind_lvl_sync_info(data, 4);
            }
        }
        hdl->busy = 0;
    }
}
#endif /*TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE*/
/************************* end 风噪检测相关接口 ***********************/

/************************* satrt 音量自适应相关接口 ***********************/
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
/*音量偏移回调*/
void audio_icsd_adptive_vol_output_handle(__adt_avc_output *_output)
{
    /* adt_debug_log("%s, spldb_iir:%d", __func__, (int)(_output->spldb_iir)); */
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl->adt_env_adaptive_hold_id) {
        //算法输出未稳定
        return;
    }
    if (hdl && hdl->state) {
        hdl->busy = 1;
        audio_anc_env_avc_thr_to_lvl_sync((int)_output->spldb_iir);
        hdl->busy = 0;
    }
}
#endif /*TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE*/
/************************* end 音量自适应相关接口 ***********************/

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
static void anc_env_rtanc_suspend_timeout(void *p)
{
    //等待增益稳定再挂起RTANC
    audio_anc_real_time_adaptive_resume("ENV_ADAPTIVE");
    anc_env_adaptive_suspend_id = 0;
}

static void anc_wind_rtanc_suspend_timeout(void *p)
{
    //等待增益稳定再挂起RTANC
    audio_anc_real_time_adaptive_resume("ANC_WIND_DET");
    anc_wind_det_suspend_id = 0;
}
#endif


#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
//rtanc drc TWS remote
static void audio_rtanc_drc_flag_remote_set(u8 output, float gain, float fb_gain)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl) {
        hdl->drc_gain[1] = gain;  //set remote gain
        hdl->drc_gain[3] = fb_gain;  //set remote gain
    }
    /*adt_log("RTANC_DRC:remote_set %x, gain %d,%d, gain %d,%d\n", \
        output, (int)(gain * 100.0f), (int)(fb_gain * 100.0f), \
        (int)(hdl->drc_gain[0] * 100.0f), (int)(hdl->drc_gain[2] * 100.0f));
    */
}

//rtanc drc TWS local
void audio_rtanc_drc_output(u8 output, float gain, float fb_gain)
{
    // BIT(0) -- 0:L  1:R
    // BIT(1) -- 0:清除对方发起FF的更新标志位  1:通知对方本地FF发生更新
    // gain --

    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    u8 send_data[10] = {0};

    if ((output & BIT(0)) == 0) {
        if (output & BIT(1)) {
            hdl->drc_gain[1] = 0;
            return;
        }

        hdl->drc_gain[0] = gain;
        hdl->drc_gain[2] = fb_gain;

        if (hdl) {
#if TCFG_USER_TWS_ENABLE
            if (get_tws_sibling_connect_state()) {
                send_data[0] = SYNC_ICSD_ADT_RTANC_DRC_RESULT;
                send_data[1] = output;
                memcpy(send_data + 2, (u8 *)(&gain), 4);
                memcpy(send_data + 6, (u8 *)(&fb_gain), 4);
                audio_icsd_adt_info_sync(send_data, 10);
            }
#endif
        }
    } else {
        if (output & BIT(1)) {
            hdl->drc_gain[0] = 0;
            return;
        }

        hdl->drc_gain[1] = gain;
        hdl->drc_gain[3] = fb_gain;
    }
}

u8 audio_rtanc_drc_flag_get(float **gain)
{
    struct speak_to_chat_t *hdl = speak_to_chat_hdl;
    if (hdl) {
        *gain = &(hdl->drc_gain[0]);
    }
    return 0;
}
#endif

/*
	通透模式 广域点击、风噪检测需打开FB功能
	FB默认滤波器，CMP使用ANC 滤波器
	V2后续可考虑优化掉
*/
float anc_trans_lfb_gain = 0.0625f;
float anc_trans_rfb_gain = 0.0625f;
const double anc_trans_lfb_coeff[] = {
    0.195234751212410628795623779296875,
    0.1007948522455990314483642578125,
    0.0427739980514161288738250732421875,
    -1.02504855208098888397216796875,
    0.36385215423069894313812255859375,
    /*
    0.0508266850956715643405914306640625,
    -0.1013386586564593017101287841796875,
    0.0505129448720254004001617431640625,
    -1.99860572628676891326904296875,
    0.9986066981218755245208740234375,
    */
};
const double anc_trans_rfb_coeff[] = {
    0.195234751212410628795623779296875,
    0.1007948522455990314483642578125,
    0.0427739980514161288738250732421875,
    -1.02504855208098888397216796875,
    0.36385215423069894313812255859375,
};

//免摘/广域需要在通透下开ANC FB，提供默认的参数，如果外部使用自然通透，则可以不用
void audio_icsd_adt_trans_fb_param_set(audio_anc_t *param)
{
    anc_gain_param_t *gains = &param->gains;

    param->ltrans_fbgain = anc_trans_lfb_gain;
    param->rtrans_fbgain = anc_trans_rfb_gain;
    param->ltrans_fb_coeff = (double *)anc_trans_lfb_coeff;
    param->rtrans_fb_coeff = (double *)anc_trans_rfb_coeff;

    param->ltrans_fb_yorder = sizeof(anc_trans_lfb_coeff) / 40;
    param->rtrans_fb_yorder = sizeof(anc_trans_rfb_coeff) / 40;
    //use anc cmp param
    param->ltrans_cmpgain = (gains->gain_sign & ANCL_CMP_SIGN) ? (0 - gains->l_cmpgain) : gains->l_cmpgain;
    param->rtrans_cmpgain = (gains->gain_sign & ANCR_CMP_SIGN) ? (0 - gains->r_cmpgain) : gains->r_cmpgain;
    param->ltrans_cmp_coeff = param->lcmp_coeff;
    param->rtrans_cmp_coeff = param->rcmp_coeff;

    param->ltrans_cmp_yorder = param->lcmp_yorder;
    param->rtrans_cmp_yorder = param->rcmp_yorder;
}

//获取当前ADT是否运行
u8 icsd_adt_is_running(void)
{
    if (speak_to_chat_hdl) {
        if (speak_to_chat_hdl->state) {
            return 1;
        }
    }
    return 0;
}

int icsd_adt_app_mode_set(u16 adt_mode, u8 en)
{
    adt_log("icsd_adt_app_mode_set 0x%x, en %d", adt_mode, en);
    if (en) {
        adt_info.app_adt_mode |= adt_mode;
    } else {
        adt_info.app_adt_mode &= ~adt_mode;
    }
    return 0;
}

static int icsd_adt_debug_tool_function_get(void)
{
    int ret = 0;
    int ext_debug = anc_ext_debug_tool_function_get();
    if (ext_debug & ANC_EXT_FUNC_EN_RTANC) {
        ret |= TFUNCTION_RTANC;
    }
    if (ext_debug & ANC_EXT_FUNC_EN_WIND_DET) {
        ret |= TFUNCTION_WDT;
    }
    return ret;
}

int icsd_adt_a2dp_scene_set(u8 en)
{
    audio_icsd_adt_scene_set(ADT_SCENE_A2DP, en);

    /* audio_icsd_adt_reset(ADT_SCENE_A2DP); */
    if (adt_info.scene & ADT_SCENE_ANC_OFF) {
        //非必要不复位
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE && TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
        if (en) {
            //用于启动ANC_OFF(RTAEQ)
            if (audio_icsd_adt_scene_check(icsd_adt_app_mode_get(), 0) & ADT_REAL_TIME_ADAPTIVE_ANC_MODE) {
                audio_icsd_adt_reset(ADT_SCENE_A2DP);
            }
        } else {
            //用于关闭ANC_OFF(RTAEQ)
            if (audio_anc_real_time_adaptive_state_get()) {
                audio_icsd_adt_reset(ADT_SCENE_A2DP);
            } else {
                //删除ANC_OFF(RTAEQ定时器)
                audio_rtaeq_in_ancoff_close();
            }
        }
#endif
    }
    if (en) {
#if (defined TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE) && TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
        if (audio_speak_to_chat_is_trigger()) {
            audio_speak_to_chat_resume();
        }
#endif
    }
    return 0;
}

u32 icsd_adt_app_mode_get(void)
{
    return adt_info.app_adt_mode;
}

static u8 audio_speak_to_chat_idle_query()
{
    return speak_to_chat_hdl ? 0 : 1;
}

REGISTER_LP_TARGET(speak_to_chat_lp_target) = {
    .name = "speak_to_chat",
    .is_idle = audio_speak_to_chat_idle_query,
};

#endif /*(defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
