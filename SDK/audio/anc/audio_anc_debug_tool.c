
#ifdef SUPPORT_MS_EXTENSIONS
#pragma   bss_seg(".audio_anc_debug_tool.data.bss")
#pragma  data_seg(".audio_anc_debug_tool.data")
#pragma const_seg(".audio_anc_debug_tool.text.const")
#pragma  code_seg(".audio_anc_debug_tool.text")
#endif/*SUPPORT_MS_EXTENSIONS*/
/****************************************************
				audio_anc_debug_tool.c
*	ANC debug 工具，用SPP代替有线串口打印关键debug数据

*****************************************************/

#include "app_config.h"
#include "btstack/avctp_user.h"
#include "classic/hci_lmp.h"
#include "generic/circular_buf.h"
#include "system/task.h"
#include "timer.h"
#include "crc.h"

#if TCFG_ANC_TOOL_DEBUG_ONLINE && TCFG_AUDIO_ANC_ENABLE

#include "audio_anc_includes.h"

#if TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE

#define ANC_DEBUG_TOOL_BY_TIMER			1  //定时器输出
#define ANC_DEBUG_TOOL_HEAD_MARK0		0xAA
#define ANC_DEBUG_TOOL_HEAD_MARK1		0x56

#define ANC_DEBUG_SEND_MAX_SIZE 		1024	//数据包最大缓存

#define ANC_DEBUG_TOOL_SEND_TIME 		50		//定时发送间隔		（最短40，太小可能会导致发不过来，与手机端有关）
#define ANC_DEBUG_SEND_SIZE 			350		//每次发送最大长度	（最长550，与手机端有关）

enum {
    CMD_DEFAULT = 0x0,	//默认命令标识
};

//数据包头信息
struct anc_debug_tool_packt_t {
    u8 mark0;
    u8 mark1;
    u16 id;
    u16 len;
    u8 data[0];
    /* u16 crc; */
};
#define ANC_DEBUG_TOOL_HEAD_SIZE 	sizeof(struct anc_debug_tool_packt_t) + 2 + 1	//head+crc+cmd

struct anc_debug_tool_t {
    u8 state;
    u8 log_en;
    u8 suspend;
    volatile u8 send_busy;
    volatile u8 send_id;
    OS_MUTEX mutex;

#if ANC_DEBUG_TOOL_BY_TIMER
    u32 send_len;
    cbuffer_t cbuf;
    u8 send_buf[ANC_DEBUG_SEND_MAX_SIZE + ANC_DEBUG_TOOL_HEAD_SIZE];
    u16 spp_timer;
#endif
};

enum anc_debug_tool_cmd_t {
    ANC_DEBUG_CMD_SINGLE_PACKET = 0x1,		//单包模式
    ANC_DEBUG_CMD_UNPACK_PACKET,			//拆包模式
};


static struct anc_debug_tool_t *debug_hdl = NULL;

static void audio_anc_debug_spp_send(u8 *buf, int len)
{
    int tmp_len = 512;
    int err = 0;
    //大于512循环发送
    while (len > tmp_len) {
        do {
            err = bt_cmd_prepare(USER_CTRL_SPP_SEND_DATA, tmp_len, buf);
        } while (err);
        buf += tmp_len;
        len -= tmp_len;
    }
    do {
        err = bt_cmd_prepare(USER_CTRL_SPP_SEND_DATA, len, buf);
    } while (err);
}

static void audio_anc_debug_send_timer(void *p)
{
    u8 *ptr;
    if (debug_hdl->state == ANC_DEBUG_STA_OPEN) {
        debug_hdl->send_busy = 1;

        ptr = cbuf_read_alloc(&debug_hdl->cbuf, &debug_hdl->send_len);
        if (debug_hdl->send_len) {
            if (debug_hdl->send_len > ANC_DEBUG_SEND_SIZE) {
                debug_hdl->send_len = ANC_DEBUG_SEND_SIZE;
            }
            //写不进，可以尝试加大MAX_SUPPORT_WAITINT_PKT
            bt_cmd_prepare(USER_CTRL_SPP_SEND_DATA, debug_hdl->send_len, ptr);
        }
        cbuf_read_updata(&debug_hdl->cbuf, debug_hdl->send_len);
        debug_hdl->send_len = 0;
        debug_hdl->send_busy = 0;
    }
}

void audio_anc_debug_tool_open(void)
{
    if (debug_hdl) {
        printf("ERR:ANC_DEBUG OPEN NOW!");
        return;
    }
    debug_hdl = anc_malloc("DEBUG", sizeof(struct anc_debug_tool_t));
    debug_hdl->state = ANC_DEBUG_STA_OPEN;
    debug_hdl->send_id = 1;
    os_mutex_create(&debug_hdl->mutex);
#if ANC_DEBUG_TOOL_BY_TIMER
    cbuf_init(&debug_hdl->cbuf, debug_hdl->send_buf, sizeof(debug_hdl->send_buf));
    debug_hdl->spp_timer = sys_timer_add(NULL, audio_anc_debug_send_timer, ANC_DEBUG_TOOL_SEND_TIME);
#endif
}

void audio_anc_debug_tool_close(void)
{
    if (debug_hdl) {
        debug_hdl->state = ANC_DEBUG_STA_CLOSE;
        while (debug_hdl->send_busy) {
            os_time_dly(1);
        }
#if ANC_DEBUG_TOOL_BY_TIMER
        sys_timer_del(debug_hdl->spp_timer);
#endif
        anc_free(debug_hdl);
        debug_hdl = NULL;
    }
}

void audio_anc_debug_spp_log_en(u8 en)
{
    if (debug_hdl) {
        debug_hdl->log_en = en;
    }
}

void audio_anc_debug_spp_suspend(u8 en)
{
    if (debug_hdl) {
        debug_hdl->suspend = en;
    }
}

int audio_anc_debug_send_data_packet(u8 cmd, u8 *buf, int len)
{
    os_mutex_pend(&debug_hdl->mutex, 0);
    int err = 0;
    u16 calc_crc;
    int packet_len = len + ANC_DEBUG_TOOL_HEAD_SIZE;
    struct anc_debug_tool_packt_t *packet = anc_malloc("ANC_DEBUG", packet_len);	//len+ head + crc

    if (debug_hdl->send_id < 255) {
        debug_hdl->send_id++;
    } else {
        debug_hdl->send_id = 1;
    }

    packet->mark0 = ANC_DEBUG_TOOL_HEAD_MARK0;
    packet->mark1 = ANC_DEBUG_TOOL_HEAD_MARK1;
    packet->id = debug_hdl->send_id;
    packet->len = len + 1;
    packet->data[0] = cmd;
    memcpy(packet->data + 1, buf, len);
    calc_crc = CRC16(packet, packet_len - 2);
    memcpy(packet->data + 1 + len, (u8 *)&calc_crc, 2);

    if (debug_hdl) {
#if ANC_DEBUG_TOOL_BY_TIMER
        //异步发送
        if (cbuf_is_write_able(&debug_hdl->cbuf, len)) {
            cbuf_write(&debug_hdl->cbuf, packet, packet_len);
        } else {
            printf("anc debug send full!\n");
            len = 0;
            goto __exit;
        }
#else
        //阻塞式发送
        audio_anc_debug_spp_send(packet, len);
#endif
    }

__exit:
    os_mutex_post(&debug_hdl->mutex);
    anc_free(packet);
    return len;
}

int audio_anc_debug_send_data(u8 *buf, int len)
{
    if (debug_hdl) {
        if ((!debug_hdl->log_en) || debug_hdl->suspend) {
            //log 关闭时，打印类信息不允许上传
            switch (buf[0]) {
            case ANC_DEBUG_CMD_ICSD:
            case ANC_DEBUG_CMD_APP:
            case ANC_DEBUG_CMD_PASS:
                return 0;
            }
        }
        if (len <= ANC_DEBUG_SEND_MAX_SIZE) {
            //单包模式
            return audio_anc_debug_send_data_packet(ANC_DEBUG_CMD_SINGLE_PACKET, buf, len);
        } else {
            //拆包模式
            printf("ERR:ANC_DEBUG buff size full!");
            return 0;
        }
    }
    return 0;
}


u8 audio_anc_debug_busy_get(void)
{
    if (debug_hdl) {
        return debug_hdl->send_busy;
        /* return bt_cmd_prepare(USER_CTRL_SPP_SEND_DATA, 0, NULL); */
    }
    return 0;
}

static void audio_anc_debug_user_cmd_ack(u8 cmd2pc, u8 ret, u8 err_num)
{
    u8 cmd[5];
    //透传命令标识
    cmd[0] = 0xFD;
    cmd[1] = 0x90;
    //自定义命令标识
    cmd[2] = 0xB1;
    if (ret == TRUE) {
        cmd[3] = cmd2pc;
        anctool_api_write(cmd, 4);
    } else {
        cmd[3] = 0xFE;
        cmd[4] = err_num;
        anctool_api_write(cmd, 5);
    }
}

int audio_anc_debug_user_cmd_process(u8 *data, int len)
{
    //data[0] = 0xB1
    u8 cmd = data[0];
    int data_len = len - 1;	//目标数据长度
    u8 *data_p = data + 1;	//目标数据地址
    float f_param = 0.0f;
#if AUDIO_ANC_MIC_ARRAY_ENABLE
    audio_anc_t *anc_param;
#endif

    if (data_len == 4) {
        memcpy((u8 *)&f_param, data_p, 4);
    }

    printf("ANC DEBUG USER CMD:0x%x\n", cmd);
    put_buf(data_p, data_len);

#if 1
    switch (cmd) {
    case CMD_DEFAULT:
        /* put_buf(data_p, data_len); */
        break;

#if 0 //入耳提示音功能验证
    case 1:
        int audio_rtanc_in_ear_demo(u8 tone_en, ANC_mode_t exit_mode);
        audio_rtanc_in_ear_demo(1, ANC_ON);
        break;
#endif
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    case 2:
        audio_anc_app_adt_mode_init(data[1]);
        break;
#endif

#if AUDIO_ANC_DATA_EXPORT_VIA_UART
    case 3:
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        if (audio_anc_real_time_adaptive_state_get()) {
            g_printf("ANC_DEV: rtanc open now\n");
        }
#endif
        if (data[1]) {
            audio_anc_develop_open(ANC_DEV_DATA_SEL_ANC);
        } else {
            audio_anc_develop_close();
        }
#endif

#if TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE
    case 10:
        extern float avc_alpha_db;
        avc_alpha_db = f_param;
        break;
#endif

#if 0// DCC TRIM 功能验证
    case 4: //DCC 模式切换
        int audio_anc_dcc_trim_switch(u8 en);
        audio_anc_dcc_trim_switch(data[1]);
        break;

    case 5: //DCC open
        int audio_anc_dcc_trim_open(void);
        audio_anc_dcc_trim_open();
        break;
#endif


#if ANC_HOWLING_DETECT_EN
    case 11:
        //开关啸叫检测
        void audio_anc_howl_det_toggle_demo();
        audio_anc_howl_det_toggle_demo();
        break;
#endif

#if TCFG_AUDIO_ANC_ENV_ADAPTIVE_GAIN_ENABLE
    case 12:
        //开关环境自适应
        /* void audio_anc_env_det_toggle_demo(); */
        /* audio_anc_env_det_toggle_demo(); */
        void audio_anc_env_adaptive_gain_demo();
        audio_anc_env_adaptive_gain_demo();
        break;
#endif
#if TCFG_AUDIO_ANC_ENABLE && (!(defined CONFIG_CPU_BR36))
    case 13:
        if (data_len == 4) {
            audio_anc_fade_ctr_set(ANC_FADE_MODE_USER, AUDIO_ANC_FDAE_CH_FF, (u16)f_param);
        }
        break;
#endif
#if AUDIO_ANC_MIC_ARRAY_ENABLE
    case 14:
        //对调主副的FF mic
        printf("ANC_STEREO_MIX: swap ff mic ch\n");
        anc_param = audio_anc_param_get();
        if (data[1]) {
            anc_param->mic_type[0] = TCFG_AUDIO_ANCR_FF_MIC;
            anc_param->mic_type[2] = TCFG_AUDIO_ANCL_FF_MIC;
        } else {
            anc_param->mic_type[0] = TCFG_AUDIO_ANCL_FF_MIC;
            anc_param->mic_type[2] = TCFG_AUDIO_ANCR_FF_MIC;
        }
        put_buf(anc_param->mic_type, 4);
        break;
    case 15:
        //对调主副的FB mic
        printf("ANC_STEREO_MIX: swap fb mic ch\n");
        anc_param = audio_anc_param_get();
        if (data[1]) {
            anc_param->mic_type[1] = TCFG_AUDIO_ANCR_FB_MIC;
            anc_param->mic_type[3] = TCFG_AUDIO_ANCL_FB_MIC;
        } else {
            anc_param->mic_type[1] = TCFG_AUDIO_ANCL_FB_MIC;
            anc_param->mic_type[3] = TCFG_AUDIO_ANCR_FB_MIC;
        }
        put_buf(anc_param->mic_type, 4);
        break;
    case 16:
        /*
           0：关闭多MIC阵列，FF/FB_NUM = 1
           1：打开多MIC阵列，FF/FB NUM > 1
         */
        printf("ANC_STEREO_MIX: en %d\n", data[1]);
        audio_anc_stereo_mix_set(data[1]);
        break;
#endif
    default:
        break;
    }
#endif

    audio_anc_debug_user_cmd_ack(cmd, TRUE, 0);
    return 0;
}

/*
   APP SPP打印命令
 */
void audio_anc_debug_app_send_data(u8 cmd, u8 cmd_2nd, u8 *buf, int len)
{
#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE
    if (!(anc_ext_debug_tool_function_get() & ANC_EXT_FUNC_SPP_EN_APP)) {
        return;
    }
#endif
    u8 *send_buf = anc_malloc("ANC_DEBUG", len + 3);
    send_buf[0] = ANC_DEBUG_CMD_APP;
    send_buf[1] = cmd;
    send_buf[2] = cmd_2nd;
    if (len) {
        memcpy(send_buf + 3, buf, len);
    }
    /* g_printf("%s", __func__); */
    /* put_buf(send_buf, len + 2); */
    audio_anc_debug_send_data(send_buf, len + 3);
    anc_free(send_buf);
}

/*
   透传命令
   cmd 0: put_buf
   cmd 1: 打印字符串
 */
void audio_anc_debug_pass_send_data(u8 cmd, u8 *buf, int len)
{
    u8 *send_buf = anc_malloc("ANC_DEBUG", len + 2);
    send_buf[0] = ANC_DEBUG_CMD_PASS;
    send_buf[1] = cmd;
    if (len) {
        memcpy(send_buf + 2, buf, len);
    }
    /* g_printf("%s", __func__); */
    /* put_buf(send_buf, len + 2); */
    audio_anc_debug_send_data(send_buf, len + 2);
    anc_free(send_buf);
}

/*
   小机状态主动上报
 */
void audio_anc_debug_report_send_data(u8 cmd, u8 *buf, int len)
{
    u8 *send_buf = anc_malloc("ANC_DEBUG", len + 2);
    send_buf[0] = ANC_DEBUG_ACTIVE_REPORT;
    send_buf[1] = cmd;
    if (len) {
        memcpy(send_buf + 2, buf, len);
    }
    /* g_printf("%s", __func__); */
    /* put_buf(send_buf, len + 2); */
    audio_anc_debug_send_data(send_buf, len + 2);
    anc_free(send_buf);
}

/*
   错误状态上报
 */
void audio_anc_debug_err_send_data(u8 cmd, void *buf, int len)
{
    u8 *send_buf = anc_malloc("ANC_DEBUG", len + 2);
    send_buf[0] = ANC_DEBUG_CMD_ERR;
    send_buf[1] = cmd;
    if (len) {
        memcpy(send_buf + 2, buf, len);
    }
    /* g_printf("%s", __func__); */
    /* put_buf(send_buf, len + 2); */
    audio_anc_debug_send_data(send_buf, len + 2);
    anc_free(send_buf);
}

#if 0
void audio_anc_debug_test(void)
{
    printf("audio_anc_debug_test\n");
    /* audio_anc_debug_tool_open(); */

    /* char s[50]; */
    /* for (int j = 0; j < 10; j++) { */
    /* sprintf(s, "test %d\n", j); */
    /* audio_anc_debug_send_data(s, strlen(s)); */
    /* } */
    /* return; */
    int err = 0;

    u8 *buf = malloc(510);
    for (int i = 0; i < 510; i += 51) {
        memset(buf + i, ((i / 51) << 4) | (i / 51), 51);
        do {
            err = audio_anc_debug_send_data(buf + i, 51);
            break;
            if (!err) {
                printf("a");
                os_time_dly(1);
            }
        } while (!err);
    }
    free(buf);
}
#endif

#endif

#endif/*TCFG_ANC_TOOL_DEBUG_ONLINE*/
