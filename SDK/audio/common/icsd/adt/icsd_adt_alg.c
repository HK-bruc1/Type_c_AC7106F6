#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_adt.data.bss")
#pragma data_seg(".icsd_adt.data")
#pragma const_seg(".icsd_adt.text.const")
#pragma code_seg(".icsd_adt.text")
#endif

#include "app_config.h"
#include "audio_anc_includes.h"
#if (((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && TCFG_AUDIO_ANC_ENABLE) || \
    (TCFG_AUDIO_AVC_NODE_ENABLE && (TCFG_AVC_ALGO_SELECT == 1)))

//用于管理与ADT_LIB 相关函数编译控制
#define ICSD_VDT_LIB          TCFG_AUDIO_SPEAK_TO_CHAT_ENABLE
#define ICSD_WAT_LIB          TCFG_AUDIO_WIDE_AREA_TAP_ENABLE
#define ICSD_EIN_LIB          0	//入耳检测
#define ICSD_WIND_LIB         TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
#define ICSD_RTANC_LIB        TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
#define ICSD_AVC_LIB		  (TCFG_AUDIO_ANC_ENV_NOISE_DET_ENABLE || (TCFG_AUDIO_AVC_NODE_ENABLE && (TCFG_AVC_ALGO_SELECT == 1)))	//自适应音量
#define ICSD_HOWL_LIB		  TCFG_AUDIO_ANC_HOWLING_DET_ENABLE
#define ICSD_ADJDCC_LIB		  TCFG_AUDIO_ADAPTIVE_DCC_ENABLE
// #define ICSD_RTAEQ_LIB        1

#include "icsd_adt.h"

#if ICSD_VDT_LIB
#include "icsd_vdt.h"
#endif

#if ICSD_WAT_LIB
#include "icsd_wat.h"
#endif

#if ICSD_EIN_LIB
#include "icsd_ein.h"
#endif

#if ICSD_RTANC_LIB
#include "rt_anc.h"
#include "rt_anc_app.h"
#endif

#if ICSD_AVC_LIB
#include "icsd_avc.h"
#endif

#if ICSD_WIND_LIB
#include "icsd_wind.h"
#include "icsd_wind_app.h"
#endif

#if ICSD_HOWL_LIB
#include "icsd_howl.h"
#endif

#if ICSD_ADJDCC_LIB
#include "icsd_adjdcc.h"
#endif

u16 adt_debug_ramsize = 0;
//===========ADJDCC============================================
int icsd_adt_adjdcc_get_libfmt(u8 type)
{
    int add_size = 0;
#if ICSD_ADJDCC_LIB
    struct icsd_adjdcc_libfmt libfmt;
    icsd_adjdcc_get_libfmt(&libfmt, type);
    add_size = libfmt.lib_alloc_size;
#else
    printf("ADT need to include ADJDCC LIB ! ! !\n");
    while (1);
#endif
    return add_size;
}

int icsd_adt_adjdcc_set_infmt(int _ram_addr, int TOOL_FUNCTION, u8 type)
{
    int set_size = 0;
#if ICSD_ADJDCC_LIB
    struct icsd_adjdcc_infmt  fmt = {0};
    fmt.alloc_ptr = (void *)_ram_addr;
    fmt.TOOL_FUNCTION = TOOL_FUNCTION;
    if (type != 2) {
        fmt.adjdcc_config = adjdcc_config_init();
    }
    icsd_adjdcc_set_infmt(&fmt, type);
    set_size = fmt.lib_alloc_size;
#endif
    return set_size;
}

u8 icsd_adt_adjdcc_run(__adt_adjdcc_run_parm *_run_parm, __adt_adjdcc_output *_output)
{
#if ICSD_ADJDCC_LIB
    __icsd_adjdcc_run_parm run_parm;
    __icsd_adjdcc_output output;
    run_parm.refmic = _run_parm->refmic;
    //run_parm.errmic = _run_parm->errmic;
    run_parm.len = _run_parm->len;
    icsd_alg_adjdcc_run(&run_parm, &output);
    ADT_FUNC->icsd_ADJDCC_output(output.result);
    return output.de_task;
#else
    return 0;
#endif
}
//===========HOWL============================================
int icsd_adt_howl_get_libfmt()
{
    int add_size = 0;
#if ICSD_HOWL_LIB
    struct icsd_howl_libfmt libfmt;
    icsd_howl_get_libfmt(&libfmt);
    add_size = libfmt.lib_alloc_size;
#else
    printf("ADT need to include HOWL LIB ! ! !\n");
    while (1);
#endif
    return add_size;
}

int icsd_adt_howl_set_infmt(int _ram_addr)
{
    int set_size = 0;
#if ICSD_HOWL_LIB
    struct icsd_howl_infmt  fmt;
    fmt.alloc_ptr = (void *)_ram_addr;
    icsd_howl_set_infmt(&fmt);
    set_size = fmt.lib_alloc_size;
    adt_debug_ramsize += fmt.debug_ram_size;
#endif
    return set_size;
}

void icsd_adt_howl_run(__adt_howl_run_parm *_run_parm, __adt_howl_output *_output)
{
#if ICSD_HOWL_LIB
    __icsd_howl_run_parm run_parm;
    __icsd_howl_output output;
    run_parm.ref = _run_parm->ref;
    run_parm.anco = _run_parm->anco;
    icsd_alg_howl_run(&run_parm, &output);
    _output->howl_output = output.howl_output;
    ADT_FUNC->icsd_HOWL_output(output.howl_output);
#endif
}

u8 icsd_adt_howl_mic_sel()
{
    u8 ret = 0;
#if ICSD_HOWL_LIB
    ret = icsd_howl_mic_sel();
#endif
    return ret;
}
void icsd_adt_howl_debug_run()
{
#if ICSD_HOWL_LIB
    icsd_howl_debug_run();
#endif
}
void icsd_adt_howl_ft_debug_run()
{
#if ICSD_HOWL_LIB
    icsd_howl_ft_debug_run();
#endif
}
u8 icsd_adt_howl_debug_start(u8 howl_output)
{
    u8 ret = 0;
#if ICSD_HOWL_LIB
    ret = icsd_howl_debug_start(howl_output);
#endif
    return ret;
}

u8 icsd_adt_howl_ftrigger_debug_start(u8 howl_output)
{
    u8 ret = 0;
#if ICSD_HOWL_LIB
    ret = icsd_howl_ft_debug_start(howl_output);
#endif
    return ret;
}

//===========AVC============================================
int icsd_adt_avc_get_libfmt(u8 type)
{
    int add_size = 0;
#if ICSD_AVC_LIB
    struct icsd_avc_libfmt libfmt;
    libfmt.type = type;
    icsd_avc_get_libfmt(&libfmt);
    add_size = libfmt.lib_alloc_size;
#else
    printf("ADT need to include AVC LIB ! ! !\n");
    while (1);
#endif
    return add_size;
}

int icsd_adt_avc_set_infmt(int _ram_addr, u8 type)
{
    int set_size = 0;
#if ICSD_AVC_LIB
    struct icsd_avc_infmt  fmt;
    fmt.alloc_ptr = (void *)_ram_addr;
    fmt.type = type;
    icsd_avc_set_infmt(&fmt);
    set_size = fmt.lib_alloc_size;
#endif
    return set_size;
}

void icsd_adt_avc_run(__adt_avc_run_parm *_run_parm, __adt_avc_output *_output)
{
#if ICSD_AVC_LIB
    __icsd_avc_run_parm run_parm;
    __icsd_avc_output output;
    run_parm.dac_data = _run_parm->dac_data;
    run_parm.refmic = _run_parm->refmic;
    run_parm.type = _run_parm->type;
    /*
    for(int i=0;i<10;i++){
    	printf("avc DAC/REF:%d                 %d\n",run_parm.dac_data[20+i],run_parm.refmic[20+i]);
    }
    */
    icsd_avc_run(&run_parm, &output);
    _output->ctl_lvl = output.ctl_lvl;
    _output->spldb_iir = output.spldb_iir;
#if (!TCFG_AUDIO_AVC_NODE_ENABLE)
    ADT_FUNC->icsd_AVC_output(_output);
#endif
#endif
}

void icsd_adt_avc_config_update_run(void *_config)
{
#if ICSD_AVC_LIB
    __avc_config *config = (__avc_config *)_config;
    avc_config_update(config);
#endif
}

//===========RTANC============================================

/* rtanc 参数更新输出:可用于挂载AEQ/CMP */
void icsd_adt_rtanc_alg_output(void *rt_param_l, void *rt_param_r)
{
#if ICSD_RTANC_LIB
    ADT_FUNC->icsd_RTANC_output(rt_param_l, rt_param_r);
#endif
}

void icsd_adt_drc_output(u8 flag, float gain, float fb_gain)
{
#if ICSD_RTANC_LIB
    ADT_FUNC->icsd_DRC_output(flag, gain, fb_gain);
#endif
}

u8 icsd_adt_drc_read(float **gain)
{
#if ICSD_RTANC_LIB
    ADT_FUNC->icsd_DRC_read(gain);
    //printf("tws_gain:local_gain:%d, remote_gain:%d\n", (int)(gain[0]), (int)(gain[1]));
    return 1;
#else
    return 0;
#endif
}

int icsd_adt_rtanc_get_libfmt(u8 rtanc_type)
{
    int add_size = 0;
#if ICSD_RTANC_LIB
    struct icsd_rtanc_libfmt rtanc_libfmt;
    rtanc_libfmt.ch_num = rt_anc_dma_ch_num;
    rtanc_libfmt.rtanc_type = rtanc_type;
    icsd_rtanc_get_libfmt(&rtanc_libfmt);
    add_size = rtanc_libfmt.lib_alloc_size;
#else
    printf("ADT need to include RTANC LIB ! ! !\n");
    while (1);
#endif

    return add_size;
}

int icsd_adt_rtanc_set_infmt(int _ram_addr, void *rtanc_tool, u8 rtanc_type, int TOOL_FUNCTION, u8 part1_times)
{
    int set_size = 0;
#if ICSD_RTANC_LIB
    DeAlorithm_enable();
    struct icsd_rtanc_infmt rtanc_fmt;
    rtanc_fmt.ep_type = ICSD_ADT_EP_TYPE;
    rtanc_fmt.ch_num = rt_anc_dma_ch_num;
    rtanc_fmt.alloc_ptr = (void *)_ram_addr;
    rtanc_fmt.rtanc_tool = rtanc_tool;
    rtanc_fmt.rtanc_type = rtanc_type;
    rtanc_fmt.TOOL_FUNCTION = TOOL_FUNCTION;
    rtanc_fmt.part1_times = part1_times;
    icsd_rtanc_set_infmt(&rtanc_fmt);
    set_size = rtanc_fmt.lib_alloc_size;
    extern void icsd_adt_rtanc_set_ch_num(u8 ch_num);
    icsd_adt_rtanc_set_ch_num(rt_anc_dma_ch_num);

    audio_adt_rtanc_set_infmt(rtanc_tool);
#endif
    return set_size;
}

void icsd_adt_alg_rtanc_run_part1(__adt_anc_part1_parm *_part1_parm)
{
#if ICSD_RTANC_LIB
    __icsd_rtanc_part1_parm part1_parm;
    part1_parm.part1_ch   = _part1_parm->part1_ch;
    part1_parm.dma_ch   = _part1_parm->dma_ch;
    part1_parm.part1_cnt = _part1_parm->part1_cnt;
    part1_parm.inptr_h  = _part1_parm->inptr_h;
    part1_parm.inptr_l  = _part1_parm->inptr_l;
    part1_parm.inptr_m  = _part1_parm->inptr_m;
    part1_parm.out0_sum = _part1_parm->out0_sum;
    part1_parm.out1_sum = _part1_parm->out1_sum;
    part1_parm.out2_sum = _part1_parm->out2_sum;
    part1_parm.fft_ram  = _part1_parm->fft_ram;
    part1_parm.hpest_temp = _part1_parm->hpest_temp;
    part1_parm.dac_flag_iszero = _part1_parm->dac_flag_iszero;
    part1_parm.p1dac_max_vld = _part1_parm->p1dac_max_vld;
    icsd_alg_rtanc_run_part1(&part1_parm);
#endif
}

u8 icsd_adt_alg_rtanc_run_part2(__adt_rtanc_part2_parm *_part2_parm)
{
#if ICSD_RTANC_LIB
    __icsd_rtanc_part2_parm part2_parm;
    part2_parm.dma_ch   = _part2_parm->dma_ch;
    part2_parm.out0_sum = _part2_parm->out0_sum;
    part2_parm.out1_sum = _part2_parm->out1_sum;
    part2_parm.out2_sum = _part2_parm->out2_sum;
    part2_parm.sz_out0_sum = _part2_parm->sz_out0_sum;
    part2_parm.sz_out1_sum = _part2_parm->sz_out1_sum;
    part2_parm.sz_out2_sum = _part2_parm->sz_out2_sum;
    part2_parm.szpz_out = _part2_parm->szpz_out;
    part2_parm.dac_flag_iszero  = _part2_parm->dac_flag_iszero;
    part2_parm.LR_FLAG = _part2_parm->LR_FLAG;
    u8 de_task = icsd_alg_rtanc_run_part2_merge(&part2_parm);
#if ICSD_ADT_RTANC_OFFLINE_PRINTF
    icsd_alg_rtanc_offline_printf(&part2_parm);
#endif
    return de_task;
#else
    return 0;
#endif
}

void icsd_adt_alg_rtanc_part2_parm_init(__adt_rtanc_part2_parm *_part2_parm)
{
#if ICSD_RTANC_LIB
    __icsd_rtanc_part2_parm part2_parm;
    part2_parm.LR_FLAG = _part2_parm->LR_FLAG;
    icsd_alg_rtanc_part2_parm_init(&part2_parm);
#endif
}
//BIT0: CHL
//BIT1: CHR
u8 icsd_adt_get_rtanc_part1_ch()
{
    u8 part1_ch = 0;
#if ICSD_RTANC_LIB
    part1_ch = icsd_alg_rtanc_get_part1_ch();
    printf("part1_ch= %d \n", part1_ch);
#endif
    return part1_ch;
}
void icsd_adt_set_rtanc_part1_ch()
{
#if ICSD_RTANC_LIB
    icsd_alg_rtanc_set_part1_ch();
#endif
}
void icsd_adt_rtanc_set_update_flag(u8 flag)
{
#if ICSD_RTANC_LIB
    icsd_alg_rtanc_set_update_flag(flag);
#endif
}
void icsd_adt_rtanc_set_part1_all0()
{
#if ICSD_RTANC_LIB
    icsd_alg_rtanc_set_part1_ch_all0();
#endif
}
void icsd_adt_alg_rtanc_part1_reset()
{
#if ICSD_RTANC_LIB
    rt_anc_part1_reset();
#endif
}

u8 icsd_adt_alg_rtanc_get_wind_lvl()
{
#if ICSD_WIND_LIB
    return icsd_adt_get_wind_lvl();
#else
    return 0;
#endif
}
float icsd_adt_alg_rtanc_get_avc_spldb_iir()
{
#if ICSD_AVC_LIB
    return icsd_adt_get_avc_spldb_iir();
#else
    return 100;
#endif
}
u8 icsd_adt_alg_rtanc_get_wind_angle()
{
#if ICSD_WIND_LIB
    return icsd_adt_get_wind_angle();
#else
    return 0;
#endif
}

u8 icsd_adt_alg_rtanc_get_adjdcc_result()
{
#if ICSD_RTANC_LIB
    return icsd_adt_get_adjdcc_result();
#else
    return 0;
#endif
}

void icsd_adt_alg_rtanc_de_run_l()
{
#if ICSD_RTANC_LIB
    rtanc_cal_and_update_filter_task(0);
#endif
}
void icsd_adt_alg_rtanc_de_run_r()
{
#if ICSD_RTANC_LIB
    rtanc_cal_and_update_filter_task(1);
#endif
}
void icsd_adt_rtanc_fadegain_update_run(void *_param)
{
#if ICSD_RTANC_LIB
    struct rtanc_param *param = (struct rtanc_param *)_param;
    icsd_alg_rtanc_fadegain_update(param);
#endif
}

//===========EIN============================================
int icsd_adt_ein_get_libfmt()
{
    int add_size = 0;
#if ICSD_EIN_LIB
    struct icsd_ein_libfmt libfmt;
    icsd_ein_get_libfmt(&libfmt);
    add_size = libfmt.lib_alloc_size;
#else
    printf("ADT need to include EIN LIB ! ! !\n");
    while (1);
#endif
    return add_size;
}

int icsd_adt_ein_set_infmt(int _ram_addr, u8 ein_state)
{
    int set_size = 0;
#if ICSD_EIN_LIB
    struct icsd_ein_infmt  fmt;
    fmt.ein_state = ein_state;
    fmt.alloc_ptr = (void *)_ram_addr;
    icsd_ein_set_infmt(&fmt);
    set_size = fmt.lib_alloc_size;
#endif
    return set_size;
}

void icsd_adt_alg_ein_run(__adt_ein_run_parm *_run_parm, __adt_ein_output *_output)
{
#if ICSD_EIN_LIB
    __icsd_ein_run_parm run_parm;
    __icsd_ein_output output;
    run_parm.adc_ref_buf = _run_parm->adc_ref_buf;
    run_parm.adc_err_buf = _run_parm->adc_err_buf;
    run_parm.inear_sz_spko_buf = _run_parm->inear_sz_spko_buf;
    run_parm.inear_sz_emic_buf = _run_parm->inear_sz_emic_buf;
    run_parm.inear_sz_dac_buf = _run_parm->inear_sz_dac_buf;
    run_parm.resume_mode = _run_parm->resume_mode;
    run_parm.anc_onoff = _run_parm->anc_onoff;
    run_parm.run_mode = _run_parm->run_mode;

    extern void icsd_alg_ein_run(__icsd_ein_run_parm * run_parm, __icsd_ein_output * output);
    icsd_alg_ein_run(&run_parm, &output);
    _output->ein_output = output.ein_output;
    _output->ein_alg_bt_inf = output.ein_alg_bt_inf;
    _output->ein_alg_bt_len = output.ein_alg_bt_len;
#endif
}
//===========WAT============================================
int icsd_adt_wat_get_libfmt()
{
    int add_size = 0;
#if ICSD_WAT_LIB
    struct icsd_wat_libfmt libfmt;
    icsd_wat_get_libfmt(&libfmt);
    add_size = libfmt.lib_alloc_size;
#else
    printf("ADT need to include WAT LIB ! ! !\n");
    while (1);
#endif
    return add_size;
}

int icsd_adt_wat_set_infmt(int _ram_addr)
{
    int set_size = 0;
#if ICSD_WAT_LIB
    struct icsd_wat_infmt  fmt;
    fmt.alloc_ptr = (void *)_ram_addr;
    icsd_wat_set_infmt(&fmt);
    set_size = fmt.lib_alloc_size;
#endif
    return set_size;
}

void icsd_adt_alg_wat_run(__adt_wat_run_parm *_run_parm, __adt_wat_output *_output)
{
#if ICSD_WAT_LIB
    __icsd_wat_run_parm run_parm;
    __icsd_wat_output output;
    run_parm.data_1_ptr = _run_parm->data_1_ptr;
    run_parm.err_gain = _run_parm->err_gain;
    run_parm.resume_mode = _run_parm->resume_mode;
    run_parm.anc_onoff = _run_parm->anc_onoff;
    icsd_alg_wat_run(&run_parm, &output);
    _output->wat_output = output.wat_output;
    _output->get_max_range = output.get_max_range;
    _output->max_range = output.max_range;
#endif
}

void icsd_adt_alg_wat_ram_clean()
{
#if ICSD_WAT_LIB
    icsd_wat_ram_clean();
#endif
}

u8 icsd_adt_alg_adjdcc_trigger_update(u8 env_level, float *table)
{
#if ICSD_RTANC_LIB
    return adjdcc_trigger_update(env_level, table);
#else
    return 0;
#endif

}

void icsd_adt_alg_rtanc_adjdcc_flag_set(u8 flag)
{
#if ICSD_RTANC_LIB
    rtanc_adjdcc_flag_set(flag);
#endif
}

u8 icsd_adt_alg_rtanc_adjdcc_flag_get()
{
#if ICSD_RTANC_LIB
    return rtanc_adjdcc_flag_get();
#else
    return 0;
#endif
}


//===========WIND===========================================
#if ICSD_WIND_LIB
const u8 ICSD_ADT_WIND_MIC_TYPE   = SDK_WIND_MIC_TYPE;
const u8 ICSD_ADT_WIND_PHONE_TYPE = SDK_WIND_PHONE_TYPE;
#else
const u8 ICSD_ADT_WIND_MIC_TYPE   = 0;
const u8 ICSD_ADT_WIND_PHONE_TYPE = 0;
#endif

u8 icsd_adt_win_get_tlkmic_en()
{
    u8 need_tlk_mic = 0;
    switch (ICSD_ADT_WIND_MIC_TYPE) {
    case ICSD_WIND_LFF_TALK:
    case ICSD_WIND_RFF_TALK:
    case ICSD_WIND_LFF_LFB_TALK:
        need_tlk_mic = 1;
        break;
    default:
        break;
    }
    return need_tlk_mic;
}

int icsd_adt_wind_get_libfmt()
{
    int add_size = 0;
#if ICSD_WIND_LIB
    struct icsd_win_libfmt libfmt;
    icsd_wind_get_libfmt(&libfmt);
    add_size = libfmt.lib_alloc_size;
#else
    printf("ADT need to include WIND LIB ! ! !\n");
    while (1);
#endif
    return add_size;
}

int icsd_adt_wind_set_infmt(int _ram_addr, int TOOL_FUNCTION)
{
    int set_size = 0;
#if ICSD_WIND_LIB
    struct icsd_win_infmt  fmt;
    fmt.alloc_ptr = (void *)_ram_addr;
    fmt.TOOL_FUNCTION = TOOL_FUNCTION;
    printf("WIND TOOL FUNCTION:%d\n", TOOL_FUNCTION);
    icsd_wind_set_infmt(&fmt);
    set_size = fmt.lib_alloc_size;
    adt_debug_ramsize += fmt.debug_ram_size;
#endif
    return set_size;
}

void icsd_adt_alg_wind_run(__adt_win_run_parm *_run_parm, __adt_win_output *_output)
{
#if ICSD_WIND_LIB
    __icsd_win_run_parm run_parm;
    __icsd_win_output output;
    run_parm.data_1_ptr = _run_parm->data_1_ptr;
    run_parm.data_2_ptr = _run_parm->data_2_ptr;
    run_parm.wdt_type = _run_parm->wdt_type;
    run_parm.anc_mode = _run_parm->anc_mode;
    run_parm.lff_gain = _run_parm->lff_gain;
    run_parm.lfb_gain = _run_parm->lfb_gain;
    run_parm.rff_gain = _run_parm->rff_gain;
    run_parm.rfb_gain = _run_parm->rfb_gain;
    icsd_alg_wind_run(&run_parm, &output);
    _output->wind_lvl = output.wind_lvl;
    _output->wind_alg_bt_inf = output.wind_alg_bt_inf;
    _output->wind_alg_bt_len = output.wind_alg_bt_len;
#endif
}

void icsd_adt_alg_wind_run_part1(__adt_win_run_parm *_run_parm, __adt_win_output *_output)
{
#if ICSD_WIND_LIB
    __icsd_win_run_parm run_parm;
    __icsd_win_output output;
    run_parm.data_1_ptr = _run_parm->data_1_ptr;
    run_parm.part1_out = (__wind_part1_out *)_run_parm->part1_out;
    run_parm.part1_out->idx = _run_parm->part1_out->idx;
    run_parm.part1_out->wind_id = _run_parm->part1_out->wind_id;
    icsd_alg_wind_run_part1(&run_parm, &output);
#endif
}

void icsd_adt_alg_wind_run_part2(__adt_win_run_parm *_run_parm, __adt_win_output *_output)
{
#if ICSD_WIND_LIB
    __icsd_win_run_parm run_parm;
    __icsd_win_output output;
    run_parm.part1_out = (__wind_part1_out *)_run_parm->part1_out;
    run_parm.part1_out_rx = (__wind_part1_out_rx *)_run_parm->part1_out_rx;
    icsd_alg_wind_run_part2(&run_parm, &output);
    _output->wind_lvl = output.wind_lvl;
    _output->wind_alg_bt_inf = output.wind_alg_bt_inf;
    _output->wind_alg_bt_len = output.wind_alg_bt_len;
#endif
}

int icsd_adt_alg_wind_ssync(__win_part1_out *_part1_out_tx)
{
    int ret = 1;
#if ICSD_WIND_LIB
    ret = alg_wind_ssync((__wind_part1_out *)_part1_out_tx);
#endif
    return ret;
}

void icsd_adt_wind_master_tx_data_sucess(void *_data)
{
#if ICSD_WIND_LIB
    icsd_wind_master_tx_data_sucess(_data);
#endif
}

void icsd_adt_wind_master_rx_data(void *_data)
{
#if ICSD_WIND_LIB
    icsd_wind_master_rx_data(_data);
#endif
}

void icsd_adt_wind_slave_tx_data_sucess(void *_data)
{
#if ICSD_WIND_LIB
    icsd_wind_slave_tx_data_sucess(_data);
#endif
}

void icsd_adt_wind_slave_rx_data(void *_data)
{
#if ICSD_WIND_LIB
    icsd_wind_slave_rx_data(_data);
#endif
}

u8 icsd_adt_wind_data_sync_en()
{
    u8 en = 0;
#if ICSD_WIND_LIB
    if ((ICSD_WIND_PHONE_TYPE == ICSD_WIND_TWS) && ((ICSD_WIND_MIC_TYPE == ICSD_WIND_LFF_RFF) || (ICSD_WIND_MIC_TYPE == ICSD_WIND_LFF_LFB_RFF))) {
        en = 1;
    }
#endif
    return en;
}

void *icsd_adt_wind_reuse_ram()
{
#if ICSD_WIND_LIB
    return icsd_wind_reuse_ram();
#endif
    return 0;
}

void icsd_adt_wind_angle_run_data(s16 *talk_mic, s16 *ffl_mic)
{
#if ICSD_WIND_LIB
    icsd_wind_angle_run_data(talk_mic, ffl_mic);
#endif
}

void icsd_adt_wind_angle_alg_run()
{
#if ICSD_WIND_LIB
    icsd_wind_angle_run();
#endif
}

void icsd_adt_wind_angle_clean()
{
#if ICSD_WIND_LIB
    icsd_wind_angle_clean();
#endif
}

void icsd_adt_wdt_debug_run()
{
#if ICSD_WIND_LIB
    icsd_wdt_debug_run();
#endif
}

u8 icsd_adt_wdt_debug_start(u8 wind_lvl)
{
    u8 ret = 0;
#if ICSD_WIND_LIB
    ret = icsd_wdt_debug_start(wind_lvl);
#endif
    return ret;
}
//===========VDT============================================
#if ICSD_VDT_LIB
const u8 ADT_VDT_USE_ANCDMAL = VDT_USE_ANCDMAL_DATA;
#else
const u8 ADT_VDT_USE_ANCDMAL = 0;
#endif
int icsd_adt_vdt_get_libfmt()
{
    int add_size = 0;
#if ICSD_VDT_LIB
    struct icsd_vdt_libfmt libfmt;
    icsd_vdt_get_libfmt(&libfmt);
    add_size = libfmt.lib_alloc_size;
#else
    printf("ADT need to include VDT LIB ! ! !\n");
    while (1);
#endif
    return add_size;
}

int icsd_adt_vdt_set_infmt(int _ram_addr)
{
    int set_size = 0;
#if ICSD_VDT_LIB
    struct icsd_vdt_infmt  fmt;
    fmt.alloc_ptr = (void *)_ram_addr;
    icsd_vdt_set_infmt(&fmt);
    set_size = fmt.lib_alloc_size;
#endif
    return set_size;
}

void icsd_adt_alg_vdt_run(__adt_vdt_run_parm *_run_parm, __adt_vdt_output *_output)
{
#if ICSD_VDT_LIB
    __icsd_vdt_run_parm run_parm;
    __icsd_vdt_output output;
    run_parm.refmic   = _run_parm->refmic;
    run_parm.errmic   = _run_parm->errmic;
    run_parm.tlkmic   = _run_parm->tlkmic;
    run_parm.dmah     = _run_parm->dmah;
    run_parm.dmal     = _run_parm->dmal;
    run_parm.dac_data = _run_parm->dac_data;
    run_parm.fbgain   = _run_parm->fbgain;
    run_parm.tfbgain  = _run_parm->tfbgain;
    run_parm.mic_num  = _run_parm->mic_num;
    run_parm.anc_mode_ind = _run_parm->anc_mode_ind;
    icsd_alg_vdt_run(&run_parm, &output);
    _output->voice_state = output.voice_state;
#endif
}

void icsd_adt_vdt_data_init(u8 _anc_mode_ind, float refl_mgain, float errl_mgain, float refr_mgain, float errr_mgain, float tlk_mgain)
{
#if ICSD_VDT_LIB
    icsd_vdt_data_init(_anc_mode_ind, refl_mgain, errl_mgain, refr_mgain, errr_mgain, tlk_mgain, ADT_MIC_VERSION);
#endif
}

void icsd_adt_alg_debug_free()
{
#if ICSD_HOWL_LIB
    icsd_alg_howl_ftdebug_free();
#endif

#if ICSD_WIND_LIB
    icsd_alg_wdt_debug_free();
#endif

#if ICSD_AVC_LIB
    icsd_alg_avc_debug_free();
#endif
}

void icsd_adt_uart2sd_open()
{
#if AUDIO_ANC_DATA_EXPORT_VIA_UART
    audio_anc_develop_open(2);
#endif
}

void icsd_adt_anc46k_outen_set(u8 en)
{
    ANC46K_CTL->out_en = en;
}

#endif/*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
