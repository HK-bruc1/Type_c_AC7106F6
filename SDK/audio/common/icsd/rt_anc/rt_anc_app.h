#ifndef _RT_ANC_APP_H_
#define _RT_ANC_APP_H_

#include "typedef.h"
#include "anc.h"
#include "anc_ext_tool.h"
#include "icsd_adt_app.h"

#define AUDIO_RT_ANC_PARAM_BY_TOOL_DEBUG		1		//RT ANC 参数使用工具debug 调试
#define AUDIO_RT_ANC_EXPORT_TOOL_DATA_DEBUG		1		//支持导出工具debug数据
#define AUDIO_RT_ANC_SZ_PZ_CMP_EN				1		//RTANC SZ/PZ补偿使能
#define AUDIO_RT_ANC_CLOCK_ALLOC				96 * 1000000L	//RTANC预申请时钟

/*
   RTANC 开关或模式切换时，是否保持上一次的效果参数, 工具/产测流程 固定使用默认参数
   0 使用默认参数
   1 参数保持，工具/产测使用默认参数
   2 参数保持(所有流程)，测试验证使用
*/
#define AUDIO_RT_ANC_KEEP_LAST_PARAM			1
#define AUDIO_RT_ANC_KEEP_FF_COEFF              0       //在以上宏的基础上选择是否保留RTANC FF参数

#define RTANC_SZ_SEL_FF_OUTPUT                  0       //使用自适应提示音 FF输出的滤波器结果
#define RTANC_SZ_SEL_FF_OUTPUT_GAIN_RATIO       0.5f    //入耳提示音输出的FF 增益倍率

//RT ANC 状态
enum {
    RT_ANC_STATE_CLOSE = 0,
    RT_ANC_STATE_OPEN,
    RT_ANC_STATE_SUSPEND,
};

struct rt_anc_fade_gain_ctr {
    u16 lff_gain;
    u16 lfb_gain;
    u16 rff_gain;
    u16 rfb_gain;
};

struct rt_anc_tmp_buffer {
#if AUDIO_RT_ANC_SZ_PZ_CMP_EN
    float pz_cmp[50];
    float sz_cmp[50];
#endif
    //后续考虑工具直接生成
    float sz_angle_rangel[MEMLEN / 2];
    float sz_angle_rangeh[MEMLEN / 2];
    float def_gfq[31];
};

int audio_adt_rtanc_set_infmt(void *rtanc_tool);

void audio_adt_rtanc_output_handle(void *rt_param_l, void *rt_param_r);

int audio_rtanc_fade_gain_suspend(struct rt_anc_fade_gain_ctr *ctr);

int audio_rtanc_fade_gain_resume(void);

/*=======================对外API=======================*/

void audio_anc_real_time_adaptive_suspend(const char *name);

void audio_anc_real_time_adaptive_resume(const char *name);

u8 audio_anc_real_time_adaptive_run_busy_get(void);

u8 audio_anc_real_time_adaptive_state_get(void);

void audio_anc_real_time_adaptive_init(audio_anc_t *param);

int audio_anc_real_time_adaptive_open(void);

int audio_anc_real_time_adaptive_close(void);

int audio_anc_real_time_adaptive_tool_data_get(u8 **buf, u32 *len);

int audio_anc_real_time_adaptive_suspend_get(void);

struct rt_anc_tmp_buffer *audio_rtanc_tmp_buff_get(u8 ch);

void audio_rtanc_self_talk_output(u8 flag);

int audio_rtanc_adaptive_init(u8 sync_mode);

int audio_rtanc_adaptive_exit(void);

void audio_rtanc_cmp_data_packet(void);

void audio_rtanc_cmp_update(void);

void audio_rtanc_dut_mode_set(u8 mode);

int audio_rtanc_sz_select_open(void);

int audio_rtanc_sz_select_process(void);

int audio_rtanc_sz_sel_state_get(void);

int audio_rtanc_coeff_set(void);

int audio_rtanc_var_cache_set(u8 flag);

int audio_rtanc_sz_sel_callback(void);

void audio_rtanc_cmp_data_clear(void);

#endif


