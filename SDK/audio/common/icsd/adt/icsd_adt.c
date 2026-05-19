#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif
#include "app_config.h"
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
	 TCFG_AUDIO_ANC_ENABLE)

#include "audio_anc_includes.h"
#include "os/os_api.h"
#include "system/task.h"
#include "classic/tws_api.h"
#include "tone_player.h"
#include "icsd_adt.h"
#include "audio_src.h"
#include "effects/convert_data.h"
#include "asm/dac.h"

/*
#define ICSD_ANC_TASK_NAME  "icsd_anc"
#define ICSD_ADT_TASK_NAME  "icsd_adt"
#define ICSD_RTANC_TASK_NAME "rt_anc"
*/
char ICSD_ANC_TASK_NAME[] = {"icsd_anc_1"};
char ICSD_ADT_TASK_NAME[] = {"icsd_adt_1"};
char ICSD_RTANC_TASK_NAME[] = {"rt_anc_1"};

#define ICSD_SRC_TASK_NAME  "icsd_src"
#define ICSD_DE_TASK_NAME   "rt_de"

//0:在cpu0，1:在cpu1
static u8 icsd_task_cpu_sel = 1;
void audio_anc_icsd_task_cpu_sel(u8 cpu_sel)
{
    icsd_task_cpu_sel = cpu_sel;
    r_printf("audio_anc_icsd_task_cpu_sel : %d\n", icsd_task_cpu_sel);
}

__adt_anc46k_ctl *ANC46K_CTL = NULL;
int (*adt_printf)(const char *format, ...) = _adt_printf;
void *anc_debug_malloc(const char *name, int size)
{
    printf("anc debug malloc:%s %d\n", name, size);
    return anc_malloc(name, size);
}

void anc_debug_free(void *pv)
{
    printf("anc debug free\n");
    anc_free(pv);
}

#define TWS_FUNC_ID_SDADT_M2S    TWS_FUNC_ID('A', 'D', 'T', 'M')
REGISTER_TWS_FUNC_STUB(icsd_adt_m2s) = {
    .func_id = TWS_FUNC_ID_SDADT_M2S,
    .func    = icsd_adt_m2s_cb,
};
#define TWS_FUNC_ID_SDADT_S2M    TWS_FUNC_ID('A', 'D', 'T', 'S')
REGISTER_TWS_FUNC_STUB(icsd_adt_s2m) = {
    .func_id = TWS_FUNC_ID_SDADT_S2M,
    .func    = icsd_adt_s2m_cb,
};

u8 icsd_adt_tws_state()
{
    u32 tws_state = ADT_FUNC->tws_api_get_tws_state();
    if (tws_state & TWS_STA_SIBLING_CONNECTED) {
        return 1;
    }
    return 0;
}

void icsd_adt_tws_m2s(u8 cmd)
{
    ADT_FUNC->local_irq_disable();
    s8 data[8];
    icsd_adt_m2s_packet(data, cmd);
    int ret = tws_api_send_data_to_sibling(data, 8, TWS_FUNC_ID_SDADT_M2S);
    ADT_FUNC->local_irq_enable();
}

void icsd_adt_tws_s2m(u8 cmd)
{
    ADT_FUNC->local_irq_disable();
    s8 data[8];
    icsd_adt_s2m_packet(data, cmd);
    int ret = tws_api_send_data_to_sibling(data, 8, TWS_FUNC_ID_SDADT_S2M);
    ADT_FUNC->local_irq_enable();
}

int icsd_adt_tws_msync(u8 *data, s16 len)
{
    int err = 0;
    if (ADT_FUNC->icsd_adt_tws_state()) {
        icsd_adt_tx_unsniff_req();
        u32 role = ADT_FUNC->tws_api_get_role();
        if (role == 0) { //master
            int ret = tws_api_send_data_to_sibling(data, len, TWS_FUNC_ID_SDADT_S2M);
            if (ret) {
                err = 2;//发送失败
            }
        } else {
            err = 1;//不需要发送
        }
    } else {
        err = 3;//蓝牙断开
    }
    return err;
}

int icsd_adt_tws_ssync(u8 *data, s16 len)
{
    int err = 0;
    if (ADT_FUNC->icsd_adt_tws_state()) {
        icsd_adt_tx_unsniff_req();
        u32 role = ADT_FUNC->tws_api_get_role();
        if (role == 0) { //master
            err = 1;
        } else {
            int ret = tws_api_send_data_to_sibling(data, len, TWS_FUNC_ID_SDADT_S2M);
            if (ret) {
                err = 2;
            }
        }
    } else {
        err = 3;
    }
    return err;
}

//ICSD算法库内部调用
void icsd_adt_dac_loopbuf_malloc(u16 points)
{
    if (adt_dac_loopbuf) {
        anc_free(adt_dac_loopbuf);
        adt_dac_loopbuf = NULL;
    }
    adt_dac_loopbuf = anc_malloc("ICSD_DAC", points * 2);
    //printf("dac loopbuf ram size:%d\n", points * 2);
}

void icsd_adt_dac_loopbuf_free()
{
    if (adt_dac_loopbuf) {
        anc_free(adt_dac_loopbuf);
        adt_dac_loopbuf = NULL;
    }
}

void icsd_adt_dma_done()
{
    icsd_acoustic_detector_ancdma_done();
}

void icsd_adt_hw_suspend()
{
    anc_dma_off();
}

void icsd_adt_hw_resume()
{
    audio_acoustic_detector_updata();
}

void icsd_adt_rtanc_fadegain_update(void *param)
{
    local_irq_disable();
    icsd_adt_rtanc_fadegain_updaterun(param);
    local_irq_enable();
}

/*********************** icsd task api ***********************/
static void adt_post_msg(const char *name, u8 cmd, u8 id)
{
    int err = os_taskq_post_msg(name, 2, cmd, id);
    if (err != OS_NO_ERR) {
        printf("%s err %d", __func__, err);
    }
}

void icsd_post_anctask_msg(u8 cmd)
{
    icsd_anctask_cmd_check(cmd);
    adt_post_msg(ICSD_ANC_TASK_NAME, cmd, 0);
}

void icsd_post_adttask_msg(u8 cmd, u8 id)
{
    adt_post_msg(ICSD_ADT_TASK_NAME, cmd, id);
}

void icsd_post_srctask_msg(u8 cmd)
{
    icsd_srctask_cmd_check(cmd);
    adt_post_msg(ICSD_SRC_TASK_NAME, cmd, 0);
}

void icsd_post_rtanctask_msg(u8 cmd)
{
    adt_post_msg(ICSD_RTANC_TASK_NAME, cmd, 0);
}

void icsd_post_detask_msg(u8 cmd)
{
    adt_post_msg(ICSD_DE_TASK_NAME, cmd, 0);
}

static void icsd_anc_process_task(void *priv)
{
    int res = 0;
    int msg[30];
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            icsd_adt_anctask_handle(msg[1]);
        }
    }
}

static void icsd_adt_task(void *priv)
{
    int res = 0;
    int msg[30];
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            icsd_adt_task_handler(msg[1], msg[2]);
        }
    }
}

static void icsd_src_task(void *priv)
{
    int res = 0;
    int msg[30];
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            icsd_src_task_handler(msg[1]);
        }
    }
}

static void icsd_rtanc_task(void *priv)
{
    int res = 0;
    int msg[30];
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            icsd_rtanc_task_handler(msg[1]);
        }
    }
}

static void icsd_de_task(void *priv)
{
    int res = 0;
    int msg[30];
    while (1) {
        res = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (res == OS_TASKQ) {
            icsd_de_task_handler(msg[1]);
        }
    }
}

static u8 icsd_anc_task = 0;
static u8 adt_task = 0;
static u8 src_task = 0;
static u8 rtanc_task = 0;
static u8 de_task = 0;
static void adt_task_create(void (*task)(void *p), const char *name, u8 *task_flag)
{
    if (*task_flag == 0) {
        task_create(task, NULL, name);
        *task_flag = 1;
    }
}

static void adt_task_kill(const char *name, u8 *task_flag)
{
    if (*task_flag) {
        task_kill(name);
        *task_flag = 0;
    }
}

void icsd_task_create()
{
    if (icsd_task_cpu_sel) {
        r_printf("icsd task create in cpu1");
        strcpy(ICSD_ANC_TASK_NAME, "icsd_anc_1");
        strcpy(ICSD_ADT_TASK_NAME, "icsd_adt_1");
        strcpy(ICSD_RTANC_TASK_NAME, "rt_anc_1");
    } else {
        r_printf("icsd task create in cpu0");
        strcpy(ICSD_ANC_TASK_NAME, "icsd_anc_0");
        strcpy(ICSD_ADT_TASK_NAME, "icsd_adt_0");
        strcpy(ICSD_RTANC_TASK_NAME, "rt_anc_0");
    }
    r_printf("icsd task create \r\nICSD_ANC_TASK_NAME:%s ,\r\nICSD_ADT_TASK_NAME:%s ,\r\nICSD_RTANC_TASK_NAME:%s\r\n\r\n", ICSD_ANC_TASK_NAME, ICSD_ADT_TASK_NAME, ICSD_RTANC_TASK_NAME)
    adt_task_create(icsd_anc_process_task, ICSD_ANC_TASK_NAME, &icsd_anc_task);
    adt_task_create(icsd_adt_task, ICSD_ADT_TASK_NAME, &adt_task);
    adt_task_create(icsd_src_task, ICSD_SRC_TASK_NAME, &src_task);
    if (ICSD_RTANC_EN) {
        adt_task_create(icsd_rtanc_task, ICSD_RTANC_TASK_NAME, &rtanc_task);
        adt_task_create(icsd_de_task, ICSD_DE_TASK_NAME, &de_task);
    }
}

void icsd_task_kill()
{
    r_printf("icsd task kill \r\nICSD_ANC_TASK_NAME:%s ,\r\nICSD_ADT_TASK_NAME:%s ,\r\nICSD_RTANC_TASK_NAME:%s\r\n\r\n", ICSD_ANC_TASK_NAME, ICSD_ADT_TASK_NAME, ICSD_RTANC_TASK_NAME)
    adt_task_kill(ICSD_ANC_TASK_NAME, &icsd_anc_task);
    adt_task_kill(ICSD_ADT_TASK_NAME, &adt_task);
    adt_task_kill(ICSD_SRC_TASK_NAME, &src_task);
    if (ICSD_RTANC_EN) {
        adt_task_kill(ICSD_RTANC_TASK_NAME, &rtanc_task);
        adt_task_kill(ICSD_DE_TASK_NAME, &de_task);
    }
}

void icsd_adt_anc46k_out_reset()
{
    ANC46K_CTL->rptr = 0;
}

void icsd_adt_anc46k_out_isr()
{
    //该函数为中断内部函数只能做简单的流程处理，不能占用太多时间
    if (ANC46K_CTL->loop_remain >= 256) {
        //printf("post 46k:%d\n",ANC46K_CTL->loop_remain);
        ANC46K_CTL->loop_remain -= 256;
        os_taskq_post_msg("anc", 1, ANC_MSG_46KOUT_DEMO);
    }
}

void icsd_adt_tone_play_handler(u8 idx)
{
    switch (idx) {
    case 0:
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM0);
        break;
    case 1:
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM1);
        break;
    case 2:
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM2);
        break;
    case 3:
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM3);
        break;
    }
}

static struct dac_read_handle icsd_dac_read_hdl = {
    .read_pos = DAC_READ_MAGIC,
    .cur_dac_hrp = 0,
    .last_dac_hrp = 0,
    .dac_hrp_diff = 0,
};

static struct dac_read_handle icsd_dac_read_michdl = {
    .read_pos = DAC_READ_MAGIC,
    .cur_dac_hrp = 0,
    .last_dac_hrp = 0,
    .dac_hrp_diff = 0,
};

extern int audio_dac_read_base(struct dac_read_handle *hdl, s16 points_offset, void *data, int len, u8 read_channel, u8 autocorrection);
extern int audio_dac_read_base_reset(struct dac_read_handle *hdl);
extern struct dac_platform_data dac_data;
int audio_dac_read_anc(s16 points_offset, void *data, int len, u8 read_channel)
{
    int rlen;
    if (dac_data.bit_width == DAC_BIT_WIDTH_24) {
        s32 *tmp_buf = anc_malloc("ICSD_DAC", len * 2 * read_channel);
        rlen = audio_dac_read_base(&icsd_dac_read_hdl, points_offset, tmp_buf, len * 2, read_channel, 0);
        if (rlen) {
            audio_convert_data_32bit_to_16bit_round((s32 *)tmp_buf, (s16 *)data, (rlen >> 2));
            rlen >>= 1;
        }
        anc_free(tmp_buf);
    } else {
        rlen = audio_dac_read_base(&icsd_dac_read_hdl, points_offset, data, len, read_channel, 0);
    }
    return rlen;
}

int audio_dac_read_mic(s16 points_offset, void *data, int len, u8 read_channel)
{
    int rlen;
    if (dac_data.bit_width == DAC_BIT_WIDTH_24) {
        s32 *tmp_buf = anc_malloc("ICSD_DAC", len * 2 * read_channel);
        rlen = audio_dac_read_base(&icsd_dac_read_michdl, points_offset, tmp_buf, len * 2, read_channel, 0);
        if (rlen) {
            audio_convert_data_32bit_to_16bit_round((s32 *)tmp_buf, (s16 *)data, (len * 2 * read_channel) >> 2);
        }
        anc_free(tmp_buf);
    } else {
        rlen = audio_dac_read_base(&icsd_dac_read_michdl, points_offset, data, len, read_channel, 0);
    }
    return rlen;
}

/*初始化dac read的资源*/
int audio_dac_read_anc_init(void)
{
    if (icsd_dac_micread) {
        audio_dac_read_base_init(&icsd_dac_read_michdl);
    }
    return audio_dac_read_base_init(&icsd_dac_read_hdl);
}
/*释放dac read的资源*/
int audio_dac_read_anc_exit(void)
{
    if (icsd_dac_micread) {
        audio_dac_read_base_exit(&icsd_dac_read_michdl);
    }
    return audio_dac_read_base_exit(&icsd_dac_read_hdl);
}
/*重置当前dac read读取的参数*/
int audio_dac_read_anc_reset(void)
{
    audio_dac_read_base_reset(&icsd_dac_read_hdl);
    if (dac_data.bit_width == DAC_BIT_WIDTH_24) {
        audio_dac_read_base(&icsd_dac_read_hdl, 0, 0, 124 * 2, 1, 0);
    } else {
        audio_dac_read_base(&icsd_dac_read_hdl, 0, 0, 124, 1, 0);
    }
    return 0;
}

int audio_dac_read_mic_reset(void)
{
    audio_dac_read_base_reset(&icsd_dac_read_michdl);
    if (dac_data.bit_width == DAC_BIT_WIDTH_24) {
        audio_dac_read_base(&icsd_dac_read_michdl, 0, 0, 124 * 2, 1, 0);
    } else {
        audio_dac_read_base(&icsd_dac_read_michdl, 0, 0, 124, 1, 0);
    }
    return 0;
}

//临时添加
u8 audio_adt_talk_mic_analog_close()
{
    /* printf("talk_mic_analog_close\n"); */
    return 0;
}
u8 audio_adt_talk_mic_analog_open()
{
    /* printf("talk_mic_analog_open\n"); */
    return 0;
}

/*处理br28没有anc_dma_on_double_4ch函数，避免调用空指针死机问题*/
void br28_test_fun_anc_dma_on_double_4ch(u8 ch1_out_sel, u8 ch2_out_sel, int *buf, int irq_point)
{
}

extern u8  audio_anc_debug_busy_get(void);
extern int audio_dac_read_anc(s16 points_offset, void *data, int len, u8 read_channel);
extern int audio_dac_read_mic(s16 points_offset, void *data, int len, u8 read_channel);
extern void audio_icsd_adptive_vol_output_handle(__adt_avc_output *_output);
const struct adt_function ADT_FUNC_t = {
    .os_time_dly = os_time_dly,
    .os_sem_set = os_sem_set,
    .os_sem_create = os_sem_create,
    .os_sem_del = os_sem_del,
    .os_sem_pend = os_sem_pend,
    .os_sem_post = os_sem_post,
    .local_irq_disable = local_irq_disable,
    .local_irq_enable = local_irq_enable,
    .icsd_post_adttask_msg = icsd_post_adttask_msg,
    .icsd_post_srctask_msg = icsd_post_srctask_msg,
    .icsd_post_anctask_msg = icsd_post_anctask_msg,
    .icsd_post_rtanctask_msg = icsd_post_rtanctask_msg,
    .icsd_post_detask_msg = icsd_post_detask_msg,
    .jiffies_usec = jiffies_usec,
    .jiffies_usec2offset = jiffies_usec2offset,
    .audio_anc_debug_send_data = audio_anc_debug_send_data,
    .audio_anc_debug_busy_get = audio_anc_debug_busy_get,
    .audio_adt_talk_mic_analog_close = audio_adt_talk_mic_analog_close,
    .audio_adt_talk_mic_analog_open = audio_adt_talk_mic_analog_open,
    .audio_anc_mic_gain_get = audio_anc_mic_gain_get,
    .tws_api_get_role = tws_api_get_role,
    .tws_api_get_tws_state = tws_api_get_tws_state,
    .icsd_adt_tws_state = icsd_adt_tws_state,
    .anc_dma_done_ppflag = anc_dma_done_ppflag,
    .anc_dma_on_double = anc_dma_on_double,
    .anc_dma_off = anc_dma_off,
    .icsd_adt_hw_suspend = icsd_adt_hw_suspend,
    .icsd_adt_hw_resume = icsd_adt_hw_resume,
    .icsd_adt_anc46k_out_reset = icsd_adt_anc46k_out_reset,
    .icsd_adt_anc46k_out_isr = icsd_adt_anc46k_out_isr,
    .audio_dac_read_anc_reset = audio_dac_read_anc_reset,
    .audio_dac_read_mic_reset = audio_dac_read_mic_reset,
    .audio_dac_read_anc = audio_dac_read_anc,
    .audio_dac_read_mic = audio_dac_read_mic,
    .icsd_adt_src_write = icsd_adt_src_write,
    .icsd_adt_src_push = icsd_adt_src_push,
    .icsd_adt_src_close = icsd_adt_src_close,
    .icsd_adt_src_init = icsd_adt_src_init,
#if ANC_CHIP_VERSION != ANC_VERSION_BR28
    .anc_dma_on_double_4ch = anc_dma_on_double_4ch,
#else
    .anc_dma_on_double_4ch = br28_test_fun_anc_dma_on_double_4ch,
#endif
#if TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
    .icsd_VDT_output = audio_speak_to_chat_output_handle,
#else
    .icsd_VDT_output = 0,
#endif
#if TCFG_AUDIO_WIDE_AREA_TAP_ENABLE
    .icsd_WAT_output = audio_wat_click_output_handle,
#else
    .icsd_WAT_output = 0,
#endif
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    .icsd_WDT_output = audio_icsd_wind_detect_output_handle,
#else
    .icsd_WDT_output = 0,
#endif
#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
    .icsd_AVC_output = audio_icsd_adptive_vol_output_handle,
#else
    .icsd_AVC_output = 0,
#endif
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    .icsd_RTANC_output = audio_adt_rtanc_output_handle,
#else
    .icsd_RTANC_output = 0,
#endif
#if TCFG_AUDIO_ADAPTIVE_DCC_ENABLE
    .icsd_ADJDCC_output = icsd_ADJDCC_output_demo,
#else
    .icsd_ADJDCC_output = 0,
#endif
#if TCFG_AUDIO_ANC_HOWLING_DET_ENABLE
    .icsd_HOWL_output = audio_anc_howling_detect_output_handle,
#else
    .icsd_HOWL_output = 0,
#endif

    .icsd_EIN_output = icsd_EIN_output_demo,

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    .icsd_DRC_output = audio_rtanc_drc_output,
    .icsd_DRC_read   = audio_rtanc_drc_flag_get,//bit0:本地结果  bit1:TWS发送过来的结果
#endif
};
struct adt_function	*ADT_FUNC = (struct adt_function *)(&ADT_FUNC_t);

#endif /*(defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/

