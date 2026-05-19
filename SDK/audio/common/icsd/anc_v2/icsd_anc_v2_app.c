#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_anc.data.bss")
#pragma data_seg(".icsd_anc.data")
#pragma const_seg(".icsd_anc.text.const")
#pragma code_seg(".icsd_anc.text")
#endif
#include "app_config.h"
#include "audio_config_def.h"
#if (TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN && \
	 TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EAR_ADAPTIVE_VERSION == ANC_EXT_V2)

#include "audio_anc_includes.h"
#include "board_config.h"
#include "tone_player.h"

#if 1
#define icsd_board_log printf
#else
#define icsd_board_log(...)
#endif/*log_en*/

#if AUDIO_ANC_STEREO_ENABLE	/*头戴式*/

#if ANC_CHIP_VERSION == ANC_VERSION_BR28
const u8 ICSD_ANC_V2_MODE = HEADSET_2CH_TONE_BYPASS_PZ; //HEADSET_TONES_MODE;//训练模式
const u8 anc_dma_ch_num = 2;         //ANC DMA通道数
#else /*ANC_CHIP_VERSION == ANC_VERSION_BR50*/
const u8 ICSD_ANC_V2_MODE = HEADSET_4CH_TONE_BYPASS_PZ;//训练模式
const u8 anc_dma_ch_num = 4;         //ANC DMA通道数
#endif
const u8 ICSD_EP_TYPE_V2 = ICSD_HEADSET;//耳机类型
#else 	/*TWS*/
//const u8 ICSD_ANC_V2_MODE = TWS_HALF_INEAR_MODE;//训练模式
const u8 ICSD_ANC_V2_MODE = TWS_2CH_TONE_BYPASS_PZ;//训练模式
const u8 anc_dma_ch_num = 2;         //ANC DMA通道数
const u8 ICSD_EP_TYPE_V2 = ICSD_HALF_INEAR;//耳机类型
#endif

u8 adaptive_run_busy;

void icsd_set_adaptive_run_busy(u8 busy)
{
    adaptive_run_busy = busy;
}

//训练结果获取API，用于通知应用层当前使用自适应参数还是默认参数
u8 icsd_anc_v2_train_result_get(struct icsd_anc_v2_tool_data *TOOL_DATA)
{
    u8 result = 0;
    if (TOOL_DATA) {
        if ((TOOL_DATA->result_l == FF_ANC_FAIL || TOOL_DATA->result_r == FF_ANC_FAIL) && \
            anc_ext_ear_adaptive_result_from() == EAR_ADAPTIVE_FAIL_RESULT_FROM_NORMAL) {
            return result;
        }
        if (TOOL_DATA->anc_combination & ANC_TFF) {
            result |= ANC_ADAPTIVE_RESULT_LFF | ANC_ADAPTIVE_RESULT_RFF;
        }
        if (TOOL_DATA->anc_combination & ANC_TFB) {
            result |= ANC_ADAPTIVE_RESULT_LFB | ANC_ADAPTIVE_RESULT_RFB;
        }
    }
    return result;
}

static void icsd_anc_v2_time_data_exit()
{
    if (ICSD_REG->anc_time_data) {
        ICSD_REG->anc_time_data = NULL;
    }
}

static void icsd_anc_v2_exit()
{
    if (ICSD_REG->anc_v2_ram_addr) {
        anc_free(ICSD_REG->anc_v2_ram_addr);
        ICSD_REG->anc_v2_ram_addr = 0;
    }
    printf("icsd_anc_v2_exit");
    icsd_anc_v2_time_data_exit();
    mem_stats();
}

const char *anc_adaptive_result_str[] = {
    "FF_ANC_SUCCESS",
    "FF_ANC_FAIL",
    "FF_ANC_JITTER2",
    "FF_ANC_JITTER1",
    "FF_ANC_RECORD",
    "FF_ANC_ERROR",
};

void icsd_anc_v2_output(struct icsd_anc_v2_tool_data *_TOOL_DATA, u8 OUTPUT_STATE)
{
    if (_TOOL_DATA) {
        switch (OUTPUT_STATE) {
        case ANC_V2_OUTPUT_NORMAL://正常输出
            printf("\n\nANC_V2_OUTPUT_NORMAL\n");

#if TCFG_USER_TWS_ENABLE
            printf("ANC_ADAPTIVE result :%s\n", anc_adaptive_result_str[_TOOL_DATA->result_l]);
#else
            printf("ANC_ADAPTIVE result L:%s, result:R %s\n", anc_adaptive_result_str[_TOOL_DATA->result_l], \
                   anc_adaptive_result_str[_TOOL_DATA->result_r]);
#endif
            printf("ICSD combination %d\n", _TOOL_DATA->anc_combination);
            audio_anc_adaptive_data_packet((struct icsd_anc_v2_tool_data *)_TOOL_DATA);
            icsd_set_adaptive_run_busy(0);
            audio_anc_adaptive_result_callback(_TOOL_DATA->result_l, _TOOL_DATA->result_r);
            icsd_board_log("train time tool data end:%d\n", (int)((jiffies - ICSD_REG->train_time_v2) * 10));
            icsd_anc_v2_exit();
            ICSD_REG->icsd_anc_v2_contral = 0;
            ANC_FUNC.icsd_anc_v2_cmd(ANC_V2_CMD_USER_TRAIN_END);
            break;
        case ANC_V2_OUTPUT_TIMEOUT://超时输出
            printf("\n\nANC_V2_OUTPUT_TIMEOUT\n");
            icsd_anc_v2_exit();
            ICSD_REG->icsd_anc_v2_contral = 0;
            ANC_FUNC.icsd_anc_v2_cmd(ANC_V2_CMD_USER_TRAIN_END);
            break;
        case ANC_V2_OUTPUT_FORCED://强行退出
            printf("\n\nANC_V2_OUTPUT_FORCED\n");
            if (ICSD_REG) {
                printf("icsd_anc_forced_exit_end cb\n");
                icsd_anc_v2_exit();
                ICSD_REG->icsd_anc_v2_contral = 0;
                anc_user_train_cb(ANC_ON, 1);
            } else {
                anc_user_train_cb(ANC_ON, 1);
            }
            break;
        default:
            ICSD_REG->icsd_anc_v2_contral = 0;
            icsd_anc_v2_exit();
            ANC_FUNC.icsd_anc_v2_cmd(ANC_V2_CMD_USER_TRAIN_END);
            break;
        }
    }
}
/*=========================================*/
//启动ancdma
/*=========================================*/
void icsd_anc_v2_dma_2ch_on(u8 out_sel, int *buf, int len)
{
    ICSD_REG->time_data_wptr = 0;
    anc_dma_on_double(out_sel, buf, len);
}


void icsd_anc_v2_dma_4ch_on(u8 out_sel_ch01, u8 out_sel_ch23, int *buf, int len)
{
#if ANC_CHIP_VERSION != ANC_VERSION_BR28
    ICSD_REG->time_data_wptr = 0;
    anc_dma_on_double_4ch(out_sel_ch01, out_sel_ch23, buf, len);
#endif
}
/*=========================================*/
//关闭ancdma中断
/*=========================================*/
void icsd_anc_v2_dma_stop()
{
    anc_dma_off();
}
/*=========================================*/
//ancdma中断处理
/*=========================================*/
void icsd_anc_v2_dma_done()
{
    //printf("dma done\n");
    if (adaptive_run_busy) {
        if (ICSD_REG->v2_user_train_state & ANC_USER_TRAIN_DMA_EN) {
            icsd_anc_v2_dma_handler();
        }

    }
}
/*=========================================*/
//将anc设置为pnc状态
//该函数退出后会马上启动ANCDMA获取PNC数据
//要求
//1.切换过程无杂音
//2.切换效果稳定后再退出
//3.需要做强退处理
//验证
//配置以下宏定义，串口可获取到数据打印，外部播放正弦波检测录音效果
//#define ICSD_ANC_DSF8_DATA_DEBUG	    1
//#define DSF8_DATA_DEBUG_TYPE			DMA_BELONG_TO_PZL
//
//注意
//*ICSD_ANC.gains_drc_en = 0;
//*ICSD_ANC.anc_fade_gain = 0;
//数据淡出完成再退出
/*=========================================*/
u8 icsd_anc_v2_set_pnc(u8 ff_yorder, float bypass_vol, s8 bypass_sign)
{
    anc_ear_adaptive_train_set_pnc(ff_yorder, bypass_vol, bypass_sign);
    if (ICSD_REG) {
        if (ICSD_REG->icsd_anc_v2_contral & ICSD_ANC_FORCED_EXIT) {
            return 1;
        }
        return 0;
    } else {
        return 1;
    }
}
/*=========================================*/
//将anc设置为bypass状态
//该函数退出后会马上启动ANCDMA获取bypass数据
//要求
//1.切换过程无杂音
//2.切换效果稳定后再退出
//3.需要做强退处理
//验证
//配置以下宏定义，串口可获取到数据打印，外部播放正弦波检测录音效果
//#define ICSD_ANC_DSF8_DATA_DEBUG	    1
//#define DSF8_DATA_DEBUG_TYPE			DMA_BELONG_TO_BYPASSL
/*=========================================*/
void anc_v2_use_train_open_ancout(u8 ff_yorder, float bypass_vol, s8 bypass_sign)
{
    struct anc_ear_adaptive_train_cfg *cfg = anc_ear_adaptive_train_cfg_get();
    cfg->ff_gain = 0.5 * bypass_vol * bypass_sign;
    cfg->ff_yorder = ff_yorder;
    double *ptr1 = cfg->ff_coeff;

    icsd_anc_v2_board_config();		//将测试 Bypass FGQ 转成IIR

    for (int i = 0; i < tap_bypass; i++) {
        *ptr1++ = bbbaa_bypass[5 * i + 0];
        *ptr1++ = bbbaa_bypass[5 * i + 1];
        *ptr1++ = bbbaa_bypass[5 * i + 2];
        *ptr1++ = bbbaa_bypass[5 * i + 3];
        *ptr1++ = bbbaa_bypass[5 * i + 4];
    }

    for (int i = tap_bypass; i < ff_yorder; i++) {
        *ptr1++ = 1;
        *ptr1++ = 0;
        *ptr1++ = 0;
        *ptr1++ = 0;
        *ptr1++ = 0;
    }
}

u8 icsd_anc_v2_set_bypass(u8 ff_yorder, float bypass_vol, s8 bypass_sign)
{
    anc_ear_adaptive_train_set_bypass(ff_yorder, bypass_vol, bypass_sign);
    if (ICSD_REG) {
        if (ICSD_REG->icsd_anc_v2_contral & ICSD_ANC_FORCED_EXIT) {
            return 1;
        }
        return 0;
    } else {
        return 1;
    }
}

float icsd_anc_v2_readfbgain()
{
    anc_gain_t *fb_gain = anc_malloc("ICSD_ANC", sizeof(anc_gain_t));
    anc_param_fill(ANC_CFG_READ, fb_gain);
    float l_fbgain = fb_gain->gains.l_fbgain;
    anc_free(fb_gain);
    return l_fbgain;
}

#endif/*TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN*/
