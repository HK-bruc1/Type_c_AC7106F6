#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_cvp_v3.data.bss")
#pragma data_seg(".audio_cvp_v3.data")
#pragma const_seg(".audio_cvp_v3.text.const")
#pragma code_seg(".audio_cvp_v3.text")
#endif
/*
 ****************************************************************
 *							AUDIO (Sigle DualMic TMS System)
 * File  : audio_cvp_v3.c
 * By    :
 * Notes : 第三代CVP算法
 *
 ****************************************************************
 */
#include "audio_cvp.h"
#include "system/includes.h"
#include "media/includes.h"
#include "effects/eq_config.h"
#include "effects/audio_pitch.h"
#include "circular_buf.h"
#include "audio_cvp_online.h"
#include "cvp_node.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/*TCFG_USER_TWS_ENABLE*/
#include "overlay_code.h"
#include "audio_dc_offset_remove.h"
#include "audio_cvp_def.h"
#include "effects/audio_gain_process.h"
#include "amplitude_statistic.h"
#include "app_main.h"
#include "lib_h/jlsp_v3_ns.h"
#include "fs/sdfile.h"
#include "online_db_deal.h"
#include "spp_user.h"
#include "audio_anc_includes.h"
#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif/*TCFG_AUDIO_DUT_ENABLE*/

#if !defined(TCFG_CVP_DEVELOP_ENABLE) || (TCFG_CVP_DEVELOP_ENABLE == 0)
#if TCFG_AUDIO_CVP_V3_MODE
#define LOG_TAG_CONST       CVP_USER
#define LOG_TAG             "[CVP_USER]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"
#include "audio_cvp_debug.c"
#include "adc_file.h"
#include "cvp_v3_config.c"

#if TCFG_AUDIO_AVC_NODE_ENABLE
#include "audio_avc.h"
#endif
//*********************************************************************************//
//                          CVP Common Configs                                     //
//*********************************************************************************//
#define CVP_V3_EQ_FILE_OFFSET   			(4)
#define CVP_V3_EQ_TOTAL_GAIN_DEFAULT_LINEAR (1.0f)
#define CVP_V3_ALGO_INDEX_INVALID  			(-1)
#define CVP_V3_ALGO_INDEX_1MIC              (0)
#define CVP_V3_ALGO_INDEX_2MIC              (1)
#define CVP_V3_ALGO_INDEX_3MIC_NO_ANC       (2)
#define CVP_V3_ALGO_INDEX_2MIC_HYBRID       (3)
#define CVP_V3_ALGO_INDEX_3MIC_WITH_ANC     (4)

#define CVP_V3_ALGO_INDEX_MIN               (CVP_V3_ALGO_INDEX_1MIC)
#define CVP_V3_ALGO_INDEX_MAX               (CVP_V3_ALGO_INDEX_3MIC_WITH_ANC)
#define CVP_V3_ALGO_INDEX_IS_VALID(idx)     (((idx) >= CVP_V3_ALGO_INDEX_MIN) && ((idx) <= CVP_V3_ALGO_INDEX_MAX))

#define CVP_V3_ALGO_TYPE_MIN				(3)
#define CVP_V3_ALGO_TYPE_MAX				(7)
#define CVP_V3_ALGO_TYPE_IS_VALID(type)     (((type) >= CVP_V3_ALGO_TYPE_MIN) && ((type) <= CVP_V3_ALGO_TYPE_MAX))

const u8 CONST_JLSP_FF_COMPEN = 1;
const u8 CONST_ACTSEQ_EN = 0;
//代码量控制
const int CVP_V3_ALGO_ENABLEBIT = TCFG_CVP_ALGO_TYPE;
const int DUAL_FLEX_MODE = TCFG_CVP_FLEXIBLE_MODE;
#ifdef TCFG_AUDIO_FLOW_PRESET_AVC_ESCO_AEC_ENABLE
const int CVP_V3_PROCESS_RESULT_DUMP_ENABLE = TCFG_AUDIO_FLOW_PRESET_AVC_ESCO_AEC_ENABLE;
#else
const int CVP_V3_PROCESS_RESULT_DUMP_ENABLE = 0;
#endif
//EQ DEBUG打印
const u8 CONST_EQ_DEBUG_ENABLE = 0;

/*CVP_TOGGLE:CVP模块(包括AEC、NLP、NS等)总开关，Disable则数据完全不经过处理，释放资源*/
#define CVP_TOGGLE				1

/*响度指示器*/
#define CVP_LOUDNESS_TRACE_ENABLE		0	//跟踪获取当前mic输入幅值

/* 通过蓝牙spp发送风噪信息
 * 需要同时打开USER_SUPPORT_PROFILE_SPP和APP_ONLINE_DEBUG*/
#define WIND_DETECT_INFO_SPP_DEBUG_ENABLE  0
#if WIND_DETECT_INFO_SPP_DEBUG_ENABLE && (!APP_ONLINE_DEBUG || !TCFG_BT_SUPPORT_SPP)
#error "蓝牙spp发数功能需要打开TCFG_BT_SUPPORT_SPP 和 APP_ONLINE_DEBUG"
#endif

/*数据输出开头丢掉的数据包数*/
#define AEC_OUT_DUMP_PACKET		15
/*数据输出开头丢掉的数据包数*/
#define AEC_IN_DUMP_PACKET		1

/*使能即可跟踪通话过程的内存情况*/
#define CVP_MEM_TRACE_ENABLE	0
//**************************CVP Common Configs End************************************//

extern void aec_code_movable_load(void);
extern void aec_code_movable_unload(void);
extern u32 usb_mic_is_running();

struct cvp_hdl {
    volatile u8 start;					//CVP模块状态
    u8 inbuf_clear_cnt;					//CVP输入数据丢掉
    u8 output_fade_in;					//CVP输出淡入使能
    u8 output_fade_in_gain;				//CVP输出淡入增益
    u8 EnableBit;						//CVP使能模块
    u8 input_clear;						//清0输入数据标志
    u16 dump_packet;					//前面如果有杂音，丢掉几包
    struct cvp_attr attr;				//v3 cvp aec模块参数属性
    struct audio_cvp_pre_param_t pre;	//预处理配置
    int algo_type;
#if TCFG_SUPPORT_MIC_CAPLESS
    void *dcc_hdl;
#endif
#if WIND_DETECT_INFO_SPP_DEBUG_ENABLE
    struct spp_operation_t *spp_opt;    //蓝牙spp发送句柄
    u8 spp_cnt;                         //发数间隔
    int wd_flag;                        //风噪状态
    float wd_val;                       //风噪强度(dB)
    char spp_tmpbuf[25];                //打印缓存buf
#endif
};

struct cvp_v3_coeff_format {
    u16 length;
    u16 mix_flag;
    float wb_eq_global_gain;
    float nb_eq_global_gain;
    u16 fft_size;
    u16 sample_rate;
};

struct cvp_hdl *cvp_v3 = NULL;

static u8 global_output_way = 0;

void audio_cvp_v3_set_output_way(u8 en)
{
    global_output_way = en;
}

int audio_cvp_v3_probe_param_update(struct audio_cvp_pre_param_t *cfg)
{
    if (cvp_v3) {
        cvp_v3->pre = *cfg;
    }
    return 0;
}

void audio_cvp_v3_set_anc_mode(u8 anc_mode)
{
    if (cvp_v3 && (cvp_v3->algo_type & CVP_ALGO_3MIC)) {
        static JLSP_set_anc_state_mode anc_state_mode;
        anc_state_mode.isAncOn = anc_mode;
        anc_state_mode.ancMode = 0;
        audio_cvp_v3_ioctl(CVP_SET_ANC_STATEMODE, TRI_SET_ANC_STATEMODE, &anc_state_mode);
    }
}

/*
*********************************************************************
*                  Audio CVP Process_Probe
* Description: CVP模块数据前处理回调
* Arguments  : data 数据地址
*			   len	数据长度
* Return	 : 0 成功 其他 失败
* Note(s)    : 在源数据经过AEC模块前，可以增加自定义处理
*********************************************************************
*/
static LOUDNESS_M_STRUCT mic_loudness;
static int audio_cvp_v3_probe(short *talk_mic, short *ff_mic, short *fb_mic, short *ref, u16 len)
{
#if TCFG_AUDIO_MIC_ARRAY_TRIM_ENABLE
    //表示使用主副麦差值计算，且仅减小增益
    if (app_var.audio_mic_array_trim_en) {
        if (app_var.audio_mic_array_diff_cmp != 1.0f) {
            if (app_var.audio_mic_array_diff_cmp > 1.0f) {
                float mic0_gain = 1.0 / app_var.audio_mic_array_diff_cmp;
                GainProcess_16Bit(talk_mic, talk_mic, mic0_gain, 1, 1, 1, len >> 1);
            } else {
                float mic1_gain = app_var.audio_mic_array_diff_cmp;
                GainProcess_16Bit(ff_mic, ff_mic, mic1_gain, 1, 1, 1, len >> 1);
            }
        } else {       //表示使用每个MIC与金机曲线的差值
            GainProcess_16Bit(talk_mic, talk_mic, app_var.audio_mic_cmp.talk, 1, 1, 1, len >> 1);
            GainProcess_16Bit(ff_mic, ff_mic, app_var.audio_mic_cmp.ff, 1, 1, 1, len >> 1);
        }
    }
#endif
    if (cvp_v3->pre.pre_gain_en) {
        if ((cvp_v3->algo_type & CVP_ALGO_1MIC) || (cvp_v3->algo_type & CVP_ALGO_1MIC_AECNLP)) {
            GainProcess_16Bit(talk_mic, talk_mic, cvp_v3->pre.talk_mic_gain, 1, 1, 1, len >> 1);
        } else if (cvp_v3->algo_type & CVP_TYPE_2MIC) {
            GainProcess_16Bit(talk_mic, talk_mic, cvp_v3->pre.talk_mic_gain, 1, 1, 1, len >> 1);
            GainProcess_16Bit(ff_mic, ff_mic, cvp_v3->pre.talk_ref_mic_gain, 1, 1, 1, len >> 1);
        } else if (cvp_v3->algo_type & CVP_ALGO_3MIC) {
            GainProcess_16Bit(talk_mic, talk_mic, cvp_v3->pre.talk_mic_gain, 1, 1, 1, len >> 1);
            GainProcess_16Bit(ff_mic, ff_mic, cvp_v3->pre.talk_ref_mic_gain, 1, 1, 1, len >> 1);
            GainProcess_16Bit(fb_mic, fb_mic, cvp_v3->pre.talk_fb_mic_gain, 1, 1, 1, len >> 1);
        }
    }
#if TCFG_SUPPORT_MIC_CAPLESS
    if (cvp_v3->dcc_hdl) {
        audio_dc_offset_remove_run(cvp_v3->dcc_hdl, (void *)talk_mic, len);
    }
#endif

#if CVP_LOUDNESS_TRACE_ENABLE
    loudness_meter_short(&mic_loudness, talk_mic, len >> 1);
#endif/*CVP_LOUDNESS_TRACE_ENABLE*/

    if (cvp_v3->inbuf_clear_cnt) {
        cvp_v3->inbuf_clear_cnt--;
        if ((cvp_v3->algo_type & CVP_ALGO_1MIC) || (cvp_v3->algo_type & CVP_ALGO_1MIC_AECNLP)) {
            memset(talk_mic, 0, len);
        } else if (cvp_v3->algo_type & CVP_TYPE_2MIC) {
            memset(talk_mic, 0, len);
            memset(ff_mic, 0, len);
        } else if (cvp_v3->algo_type & CVP_ALGO_3MIC) {
            memset(talk_mic, 0, len);
            memset(ff_mic, 0, len);
            memset(fb_mic, 0, len);
        }
    }
    return 0;
}

/*
*********************************************************************
*                  Audio CVP Get AEC Data
* Description: 获取CVP模块aec算法后数据
* Arguments  : data 回音消除处理后的数据
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
int audio_cvp_v3_aec_cllback(s16 *data, u16 len)
{
    /* printf(">>>%d %d %d\n", data[0], data[1], data[2]); */
#if (TCFG_AUDIO_AVC_NODE_ENABLE && \
        (defined TCFG_AUDIO_FLOW_PRESET_AVC_ESCO_AEC_ENABLE) && TCFG_AUDIO_FLOW_PRESET_AVC_ESCO_AEC_ENABLE)
    audio_avc_esco_aec_run(data, len);
#endif
    return 0;
}


/*
*********************************************************************
*                  Audio CVP Process_Post
* Description: CVP模块数据后处理回调
* Arguments  : data 数据地址
*			   len	数据长度
* Return	 : 0 成功 其他 失败
* Note(s)    : 在数据处理完毕，可以增加自定义后处理
*********************************************************************
*/
static int audio_cvp_v3_post(s16 *data, u16 len)
{
    //后处理获取风噪信息
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
        TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE)
    if (get_cvp_icsd_wind_lvl_det_state()) {
        int wd_flag = 0;
        float wind_lvl = 0.f;
        audio_cvp_v3_get_wind_detect_info(&wd_flag, &wind_lvl);
        audio_cvp_wind_lvl_output_handle(wind_lvl);
    }
#endif

#if WIND_DETECT_INFO_SPP_DEBUG_ENABLE
#if TCFG_USER_TWS_ENABLE
    if ((tws_api_get_role() == TWS_ROLE_MASTER))
#endif
    {
        cvp_v3->spp_cnt ++;
        if ((cvp_v3->attr.EnableBit & WNC_EN) && (cvp_v3->spp_cnt > 20) && cvp_v3->spp_opt && cvp_v3->spp_opt->send_data) {
            cvp_v3->spp_cnt = 0;
            memset(cvp_v3->spp_tmpbuf, 0x20, sizeof(cvp_v3->spp_tmpbuf));
            jlsp_cvp_v3_get_wind_detect_info(&cvp_v3->wd_flag, &cvp_v3->wd_val);
            sprintf(cvp_v3->spp_tmpbuf, "flag:%d, val:%d", cvp_v3->wd_flag, (int)cvp_v3->wd_val);
            cvp_v3->spp_opt->send_data(NULL, cvp_v3->spp_tmpbuf, sizeof(cvp_v3->spp_tmpbuf));
            printf("wd_flag:%d, wd_val:%d", cvp_v3->wd_flag, (int)cvp_v3->wd_val);
        }
    }
#endif
    return 0;
}


/*跟踪系统内存使用情况:physics memory size xxxx bytes*/
static void sys_memory_trace(void)
{
    static int cnt = 0;
    if (cnt++ > 200) {
        cnt = 0;
        mem_stats();
    }
}

//跟踪系统风噪信息->宽窄带信息->麦克风状态信息->ANC状态信息
void audio_cvp_v3_info_trace(void *priv)
{
    int is_wb_state = 0;
    int mic_state = 0;
    int wd_flag = 0;
    float wd_val = 0.f;
    int mode = 0;
    mic_state = audio_cvp_v3_get_mic_state_info(is_wb_state);
    is_wb_state =  audio_cvp_v3_get_bandwidth_info(mic_state);
    audio_cvp_v3_get_wind_detect_info(&wd_flag, &wd_val);
    audio_cvp_v3_get_anc_mode(&mode);
    printf("is_wb_state %d mic_state %d wd_flag %d anc_mode %d\n", is_wb_state, mic_state, wd_flag, mode);
    printf("cvp_v3 wd_val(dB):\n");
    put_float(wd_val);
}

/*
 *********************************************************************
 *                  Audio CVP Output Handle
 * Description: CVP模块数据输出回调
 * Arguments  : data 输出数据地址
 *			   len	输出数据长度
 * Return	 : 数据输出消耗长度
 * Note(s)    : None.
 *********************************************************************
 */
static int audio_cvp_v3_output(s16 *data, u16 len)
{
#if (((defined TCFG_KWS_VOICE_RECOGNITION_ENABLE) && TCFG_KWS_VOICE_RECOGNITION_ENABLE) || \
     ((defined TCFG_CALL_KWS_SWITCH_ENABLE) && TCFG_CALL_KWS_SWITCH_ENABLE))
    //Voice Recognition get mic data here
    extern void kws_aec_data_output(void *priv, s16 * data, int len);
    kws_aec_data_output(NULL, data, len);

#endif/*TCFG_KWS_VOICE_RECOGNITION_ENABLE*/

#if CVP_MEM_TRACE_ENABLE
    sys_memory_trace();
#endif/*CVP_MEM_TRACE_ENABLE*/

    if (cvp_v3->dump_packet) {
        cvp_v3->dump_packet--;
        memset(data, 0, len);
    } else  {
        if (cvp_v3->output_fade_in) {
            s32 tmp_data;
            //printf("fade:%d\n",cvp_v3->output_fade_in_gain);
            for (int i = 0; i < len / 2; i++) {
                tmp_data = data[i];
                data[i] = tmp_data * cvp_v3->output_fade_in_gain >> 7;
            }
            cvp_v3->output_fade_in_gain += 12;
            if (cvp_v3->output_fade_in_gain >= 128) {
                cvp_v3->output_fade_in = 0;
            }
        }
    }
    return cvp_v3_node_output_handle(data, len);
}

#define AUDIO_CVP_V3_COEFF_FILE 			(FLASH_RES_PATH"CVP_V3_CFG.bin")
/*
*********************************************************************
*                  cvp_v3_coeff_file_read
* Description: cvp_v3  eq 参数文件读取
* Arguments  : coeff_file   文件路径
* Return	 : 成功返回数据地址,失败返回null
* Note(s)    : null
*********************************************************************
*/
enum cvp_coeff_type {
    WB_EQ,
    WB_REF_EQ,
    NB_EQ,
    NB_REF_EQ,
    FB_ANC_ON_EQ,
    FB_ANC_ON_REF_EQ,
    FB_ANC_OFF_EQ,
    FB_ANC_OFF_REF_EQ,
    FB_ANC_TRANS_EQ,
    FB_ANC_TRANS_REF_EQ,
    WN_WB_EQ,
    WN_WB_REF_EQ,
    WN_NB_EQ,
    WN_NB_REF_EQ,
    CVP_TYPE_COUNT
    /* DMS_PHASE,
    TMD_PHASE, */
};
const char *cvp_v3_coeff_name[] = {"WB_EQ", "WB_REF_EQ", 	\
                                   "NB_EQ", "NB_REF_EQ", 	\
                                   "FB_ANC_ON_EQ", "FB_ANC_ON_REF_EQ",	\
                                   "FB_ANC_OFF_EQ", "FB_ANC_OFF_REF_EQ", \
                                   "FB_ANC_TRANS_EQ", "FB_ANC_TRANS_REF_EQ", \
                                   "WN_WB_EQ", "WN_WB_REF_EQ", \
                                   "WN_NB_EQ", "WN_NB_REF_EQ",
                                  };

/* algo_index: 0=1MIC, 1=2MIC, 2=3MIC, 3=2MIC-HYBRID, 4=3MIC-ANC */
static const int curve_layout_table[5][CVP_TYPE_COUNT] = {
    /* 0: 1MIC → WB_EQ(0), NB_EQ(1)，其余没有 */
    { 0,  -1,  1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1 },
    /* 1: 2MIC → 同上 */
    { 0,  -1,  1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  2, -1,  3, -1 },
    /* 2: 3MIC → 同上 */
    { 0,  -1,  1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  2,  -1, 3, -1 },
    /* 3: 2MIC-HYBRID → WB_EQ,WB_REF,NB_EQ,NB_REF = 0,1,2,3 */
    { 0,   1,  2,   3,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1 },
    /* 4: 3MIC-ANC → 按排列顺序依次填 0..7 */
    { 0,  -1,  1,  -1,   2,   3,   4,   5,   6,   7,  8, -1,  9, -1 }
};

int get_algo_index(int algo_type)
{
    int index = CVP_V3_ALGO_INDEX_INVALID;

    if ((algo_type & CVP_ALGO_3MIC) && CONFIG_ANC_ENABLE) {
        index = CVP_V3_ALGO_INDEX_3MIC_WITH_ANC;
    } else if (algo_type & CVP_ALGO_2MIC_HYBRID) {
        index = CVP_V3_ALGO_INDEX_2MIC_HYBRID;
    } else if ((algo_type & CVP_ALGO_3MIC) && !CONFIG_ANC_ENABLE) {
        index = CVP_V3_ALGO_INDEX_3MIC_NO_ANC;
    } else if (algo_type & CVP_TYPE_2MIC_WB_NB_EQ) {
        index = CVP_V3_ALGO_INDEX_2MIC;
    } else if (algo_type & CVP_ALGO_1MIC) {
        index = CVP_V3_ALGO_INDEX_1MIC;
    }

    log_info("algo_type=%d -> index=%d\n", algo_type, index);
    return index;
}

/* 读取bin文件 */
void *cvp_v3_coeff_file_read()
{
    RESFILE *fp = NULL;
    float *tmp_coeff_file = NULL;
    fp = resfile_open(AUDIO_CVP_V3_COEFF_FILE);
    if (!fp) {
        log_error("CVP_V3_CFG.bin open fail !!!");
        return NULL;
    }
    u32 tmp_coeff_len = resfile_get_len(fp);
    if (tmp_coeff_len) {
        tmp_coeff_file = zalloc(tmp_coeff_len);
    }
    if (tmp_coeff_file == NULL) {
        log_error("CVP_V3 coeff file zalloc err !!!");
        resfile_close(fp);
        return NULL;
    }
    /* resfile_seek(fp, ptr, RESFILE_SEEK_SET); */
    int rlen = resfile_read(fp, tmp_coeff_file, tmp_coeff_len);
    if (rlen != tmp_coeff_len) {
        log_error("CVP_V3_CFG.bin read err !!! %d =! %d", rlen, tmp_coeff_len);
        resfile_close(fp);
        return NULL;
    }
    resfile_close(fp);
    log_info("cvp_v3_coeff_file_read succ");
    return tmp_coeff_file;
}


void *cvp_v3_coeff_info_parse(void *coeff_file, enum cvp_coeff_type type, int algo_type)
{
    if (!coeff_file) {
        return NULL;
    }
    struct cvp_v3_coeff_format *coeff_format = (struct cvp_v3_coeff_format *)coeff_file;
    u8 coeff_algo_type = coeff_format->mix_flag & 0xF;          // 算法类型
    int coeff_offset   = CVP_V3_EQ_FILE_OFFSET;
    int eq_points      = (coeff_format->fft_size >> 1) + 1;  // (fft>>1) + 1
    int algo_index     = get_algo_index(algo_type);

    if (!CVP_V3_ALGO_INDEX_IS_VALID(algo_index)) {
        log_error("invalid algo_index=%d (min=%d max=%d)\n",
                  algo_index,
                  CVP_V3_ALGO_INDEX_MIN,
                  CVP_V3_ALGO_INDEX_MAX);
        return NULL;
    }
    int block = curve_layout_table[algo_index][type];

    if (block < CVP_V3_ALGO_INDEX_1MIC) {
        log_error("curve no exist=%d\n", block);
        return NULL;
    }

    int offset = coeff_offset + block * eq_points;
    float *coeff_info = (float *)zalloc(eq_points * sizeof(float));
    if (!coeff_info) {
        log_error("zalloc coeff_info failed\n");
        return NULL;
    }
    float *coeff_file_offset = (float *)coeff_file + offset;

    float eq_global_gain = CVP_V3_EQ_TOTAL_GAIN_DEFAULT_LINEAR;
    if (CVP_V3_ALGO_TYPE_IS_VALID(coeff_algo_type)) {
        if (type == WB_EQ) {
            eq_global_gain =  eq_db2mag(coeff_format->wb_eq_global_gain);
        } else if (type == NB_EQ) {
            eq_global_gain =  eq_db2mag(coeff_format->nb_eq_global_gain);
        }
    } else {
        log_error("invalid coeff_algo_type=%d\n", coeff_algo_type);
    }

    for (int i = 0; i < eq_points; i++) {
        coeff_info[i] = eq_db2mag(coeff_file_offset[i]) * eq_global_gain;
    }

    log_info("[%s] coeff_info_parse succ", cvp_v3_coeff_name[type]);
    return coeff_info;
}

static void audio_cvp_v3_set_aec_dump_enable(int algo_type, JLSP_params_v3_cfg *cfg)
{
    if (algo_type & CVP_ALGO_1MIC) {
        cfg->single_cfg.aec_dump_enable = 1;
    } else if (algo_type & CVP_ALGO_2MIC_BF) {
        cfg->dual_cfg.aec_dump_enable = 1;
    } else if (algo_type & CVP_ALGO_2MIC_HYBRID) {
        cfg->hybrid_cfg.aec_dump_enable = 1;
    } else if (algo_type & CVP_ALGO_2MIC_FLEXIBLE) {
        cfg->dual_flex_cfg.aec_dump_enable = 1;
    } else if (algo_type & CVP_ALGO_2MIC_CLIP) {
        cfg->dual_clip_cfg.aec_dump_enable = 1;
    } else if (algo_type & CVP_ALGO_2MIC_CLIP_TYPE2) {
        cfg->dual_clip_v2_cfg.aec_dump_enable = 1;
    } else if (algo_type & CVP_ALGO_3MIC) {
        cfg->tri_cfg.aec_dump_enable = 1;
    } else {

    }
}

static u8 audio_cvp_v3_get_aec_dump_enable(int algo_type, JLSP_params_v3_cfg *cfg)
{
    if (algo_type & CVP_ALGO_1MIC) {
        return cfg->single_cfg.aec_dump_enable;
    } else if (algo_type & CVP_ALGO_2MIC_BF) {
        return cfg->dual_cfg.aec_dump_enable;
    } else if (algo_type & CVP_ALGO_2MIC_HYBRID) {
        return cfg->hybrid_cfg.aec_dump_enable;
    } else if (algo_type & CVP_ALGO_2MIC_FLEXIBLE) {
        return cfg->dual_flex_cfg.aec_dump_enable;
    } else if (algo_type & CVP_ALGO_2MIC_CLIP) {
        return cfg->dual_clip_cfg.aec_dump_enable;
    } else if (algo_type & CVP_ALGO_2MIC_CLIP_TYPE2) {
        return cfg->dual_clip_v2_cfg.aec_dump_enable;
    } else if (algo_type & CVP_ALGO_3MIC) {
        return cfg->tri_cfg.aec_dump_enable;
    } else {
        return 0;
    }
}
/*
*********************************************************************
*                  Audio CVP Parameters
* Description: CVP模块配置参数
* Arguments  : p	参数指针
* Return	 : None.
* Note(s)    : 读取配置文件成功，则使用配置文件的参数配置，否则使用默
*			   认参数配置
*********************************************************************
*/
__CVP_BANK_CODE
static void audio_cvp_v3_param_init(struct cvp_attr *p, u16 node_uuid)
{
    JLSP_params_v3_cfg *cvp_cfg = &p->cvp_cfg;
    cvp_cfg->mic_cfg        	= mic_init_cfg;
    cvp_cfg->aec1_cfg       	= aec_cfg_default;
    cvp_cfg->aec2_cfg       	= aec_cfg_default;
    cvp_cfg->aec3_cfg       	= aec_fb_cfg_default;
    cvp_cfg->nlp1_cfg       	= nlp_cfg_default;
    cvp_cfg->nlp2_cfg       	= nlp_cfg_default;
    cvp_cfg->nlp3_cfg       	= nlp_fb_cfg_default;
    cvp_cfg->wind_cfg       	= wn_init_cfg;
    cvp_cfg->bf_cfg         	= bf_init_cfg;
    cvp_cfg->fusion_cfg     	= fusion_init_cfg;
    cvp_cfg->drc_cfg        	= drc_init_cfg;
    cvp_cfg->micSel_cfg     	= micsel_init_cfg;
    cvp_cfg->single_cfg     	= single_init_cfg;
    cvp_cfg->dual_cfg       	= dual_bf_init_cfg;
    cvp_cfg->tri_cfg        	= tri_init_cfg;
    cvp_cfg->hybrid_cfg 	  	= hybrid_init_cfg;
    cvp_cfg->single_aecnlp_cfg 	= aecnlp_init_cfg;
    cvp_cfg->dual_flex_adf_cfg  = dual_flex_adf_cfg;
    cvp_cfg->dual_flex_cfg      = dual_flex_cfg;
    cvp_cfg->dual_clip_cfg		= dual_clip_init_cfg;
    cvp_cfg->dual_clip_v2_cfg	= dual_clip_v2_init_cfg;
    cvp_cfg->doa_cfg			= doa_init_cfg;

    //读取工具配置参数+预处理参数
    CVP_CONFIG cfg;
    int ret = cvp_v3_node_param_cfg_read(&cfg, 0, node_uuid);
#if TCFG_AEC_TOOL_ONLINE_ENABLE
    //APP在线调试，APP参数覆盖工具配置参数(不覆盖预处理参数)
    ret = aec_cfg_online_update_fill(&cfg, sizeof(CVP_CONFIG));
#endif/*TCFG_AEC_TOOL_ONLINE_ENABLE*/
    cvp_v3->algo_type = cvp_get_algo_type();
    log_info("CVP-V3 ALGO_TYPE = %d\n", cvp_v3->algo_type);
    if (ret == sizeof(CVP_CONFIG)) {
        log_info("CVP_V3 read cfg ok\n");
        p->EnableBit = cfg.enable_module;

        cvp_cfg->single_cfg.enableBit 		= cfg.enable_module;
        cvp_cfg->dual_cfg.enableBit 		= cfg.enable_module;
        cvp_cfg->tri_cfg.enableBit 			= cfg.enable_module;
        cvp_cfg->single_aecnlp_cfg.enableBit = cfg.enable_module;
        cvp_cfg->dual_flex_cfg.enableBit    = cfg.enable_module;
        cvp_cfg->dual_clip_cfg.enableBit    = cfg.enable_module;
        cvp_cfg->dual_clip_v2_cfg.enableBit = cfg.enable_module;

        p->output_sel = cfg.output_sel;
        p->adc_ref_en = cfg.adc_ref_en;

#ifdef TCFG_AUDIO_FLOW_PRESET_AVC_ESCO_AEC_ENABLE
        audio_cvp_v3_set_aec_dump_enable(cvp_v3->algo_type, cvp_cfg);
#endif

        if (!(cvp_v3->algo_type & CVP_ALGO_1MIC_AECNLP)) {
            cvp_cfg->single_cfg.singleCompenDb = cfg.CompenDb;
            cvp_cfg->dual_cfg.dualCompenDb     = cfg.CompenDb;
            cvp_cfg->tri_cfg.triCompenDb       = cfg.CompenDb;
            cvp_cfg->dual_flex_cfg.dualCompenDb = cfg.CompenDb;
            cvp_cfg->dual_clip_cfg.dualCompenDb = cfg.CompenDb;
            cvp_cfg->dual_clip_v2_cfg.dualCompenDb = cfg.CompenDb;
        }

        cvp_cfg->aec1_cfg.aecProcessMaxFrequency = cfg.aec_process_maxfrequency;
        cvp_cfg->aec1_cfg.aecProcessMinFrequency = cfg.aec_process_minfrequency;

        if (!(cvp_v3->algo_type & CVP_ALGO_1MIC_AECNLP)) {
            cvp_cfg->aec2_cfg.aecProcessMaxFrequency = cfg.aec_process_maxfrequency;
            cvp_cfg->aec2_cfg.aecProcessMinFrequency = cfg.aec_process_minfrequency;
            cvp_cfg->aec3_cfg.aecProcessMaxFrequency = cfg.aec_process_maxfrequency;
            cvp_cfg->aec3_cfg.aecProcessMinFrequency = cfg.aec_process_minfrequency;
        }

        cvp_cfg->nlp1_cfg.nlpProcessMaxFrequency = cfg.nlp_process_maxfrequency;
        cvp_cfg->nlp1_cfg.nlpProcessMinFrequency = cfg.nlp_process_minfrequency;
        cvp_cfg->nlp1_cfg.overDrive = cfg.overdrive;

        if (!(cvp_v3->algo_type & CVP_ALGO_1MIC_AECNLP)) {
            cvp_cfg->nlp2_cfg.nlpProcessMaxFrequency = cfg.nlp_process_maxfrequency;
            cvp_cfg->nlp2_cfg.nlpProcessMinFrequency = cfg.nlp_process_minfrequency;
            cvp_cfg->nlp2_cfg.overDrive = cfg.overdrive;
            cvp_cfg->nlp3_cfg.nlpProcessMaxFrequency = cfg.nlp_process_maxfrequency;
            cvp_cfg->nlp3_cfg.nlpProcessMinFrequency = cfg.nlp_process_minfrequency;
            cvp_cfg->nlp3_cfg.overDrive = cfg.overdrive;
        }

        if (!(cvp_v3->algo_type & CVP_ALGO_1MIC_AECNLP)) {
            cvp_cfg->single_cfg.aggressFactor = cfg.aggressfactor;
            cvp_cfg->single_cfg.minSupress = cfg.minsuppress;
            cvp_cfg->dual_cfg.aggressFactor = cfg.aggressfactor;
            cvp_cfg->dual_cfg.minSupress = cfg.minsuppress;
            cvp_cfg->tri_cfg.aggressFactor = cfg.aggressfactor;
            cvp_cfg->tri_cfg.minSupress = cfg.minsuppress;
            cvp_cfg->dual_flex_cfg.aggressFactor = cfg.aggressfactor;
            cvp_cfg->dual_flex_cfg.minSupress = cfg.minsuppress;
            cvp_cfg->dual_clip_cfg.aggressFactor = cfg.aggressfactor;
            cvp_cfg->dual_clip_cfg.minSupress = cfg.minsuppress;
            cvp_cfg->dual_clip_v2_cfg.aggressFactor = cfg.aggressfactor;
            cvp_cfg->dual_clip_v2_cfg.minSupress = cfg.minsuppress;

            cvp_cfg->drc_cfg.noiseGateThresholdDb = cfg.noisegatethresholdDb;
            cvp_cfg->drc_cfg.makeUpGain = cfg.makeupGain;
            cvp_cfg->drc_cfg.kneeThresholdDb = cfg. kneethresholdDb;
        }
#if (CVP_V3_2MIC_ALGO_ENABLE || CVP_V3_3MIC_ALGO_ENABLE)
        if (!(cvp_v3->algo_type & CVP_ALGO_2MIC_FLEXIBLE)) {
            if ((cvp_v3->algo_type & CVP_TYPE_2MIC) || (cvp_v3->algo_type & CVP_ALGO_3MIC)) {
                cvp_cfg->bf_cfg.encProcessMaxFrequency = cfg.enc_process_maxfreq;
                cvp_cfg->bf_cfg.encProcessMinFrequency = cfg.enc_process_minfreq;
                cvp_cfg->bf_cfg.micDistance = cfg.mic_distance;
                cvp_cfg->bf_cfg.sirMaxFreq = cfg.sir_maxfreq;
                cvp_cfg->bf_cfg.targetSignalDegradation = cfg.target_signal_degradation;
                cvp_cfg->bf_cfg.aggressFactor = cfg.enc_aggressfactor;
                cvp_cfg->bf_cfg.minSuppress = cfg.enc_minsuppress;
                //V1 + post_en = 1<==>1代bf    V2 + post_en<==> 3代算法且无需配置ENCMIC补偿值
#if (CVP_BF_VERSION == JLSP_BF_V100)
                cvp_cfg->bf_cfg.type = BF_TYPE_V1;
                cvp_cfg->bf_cfg.bfPost_en = 1;
#else
                cvp_cfg->bf_cfg.type = BF_TYPE_V2;
                cvp_cfg->bf_cfg.bfPost_en = 0;
#endif
                if (cvp_v3->algo_type & CVP_ALGO_2MIC_CLIP_TYPE2) {
                    cvp_cfg->bf_cfg.type = BF_TYPE_V1_1;
                    cvp_cfg->bf_cfg.bfPost_en = 1;
                }

            }
            if ((cvp_v3->algo_type & CVP_TYPE_2MIC) || (cvp_v3->algo_type & CVP_ALGO_3MIC)) {
                cvp_cfg->wind_cfg.windProbHighTh = cfg.windProbHighTh;
                cvp_cfg->wind_cfg.windProbLowTh = cfg.windProbLowTh;
                cvp_cfg->wind_cfg.windEngDbTh = cfg.windEngDbTh;
                cvp_cfg->micSel_cfg.detectTime = cfg.detect_time;            // in second
                /*0~-90 dB 两个mic能量差异持续大于此阈值超过检测时间则会检测为故障*/
                cvp_cfg->micSel_cfg.detectEngDiffTh = cfg.detect_eng_diff_thr;     //  dB
                /*0~-90 dB 当处于故障状态时，正常的mic能量大于此阈值才会检测能量差异，避免安静环境下误判切回正常状态*/
                cvp_cfg->micSel_cfg.detectEngLowerBound = cfg.detect_eng_lowerbound; // 0~-90 dB start detect when mic energy lower than this
                cvp_cfg->micSel_cfg.detMaxFrequency = cfg.MalfuncDet_MaxFrequency;  //检测频率上限
                cvp_cfg->micSel_cfg.detMinFrequency = cfg.MalfuncDet_MinFrequency;   //检测频率下限
                cvp_cfg->micSel_cfg.onlyDetect = cfg.OnlyDetect;// 0 -> 故障切换到单mic模式， 1-> 只检测不切换
            }
        }
#if (CVP_V3_2MIC_FLEXIBLE_ENABLE)
        //双麦话务adaptive
        if (cvp_v3->algo_type & CVP_ALGO_2MIC_FLEXIBLE) {
            cvp_cfg->dual_flex_cfg.dualFlexType	= cfg.flexible_mode;
            cvp_cfg->dual_flex_adf_cfg.processMaxFrequency = cfg.adaptive_processMaxFrequency;
            cvp_cfg->dual_flex_adf_cfg.processMinFrequency = cfg.adaptive_processMinFrequency;
            cvp_cfg->dual_flex_adf_cfg.cohefMaxGamma = cfg.cohefMaxGamma;
            cvp_cfg->dual_flex_adf_cfg.varmuMaxGamma = cfg.varmuMaxGamma;
            cvp_cfg->dual_flex_adf_cfg.sirMaxGamma	 = cfg.sirMaxGamma;
            cvp_cfg->dual_flex_adf_cfg.useSirGain	 = cfg.useSirGain;
        }
#endif

#if (CVP_V3_3MIC_FUSION_ENABLE)
        if (cvp_v3->algo_type & CVP_ALGO_3MIC) {
            cvp_cfg->doa_cfg.use_au = cfg.use_au;
            cvp_cfg->doa_cfg.degree_start = cfg.degree_start;
            cvp_cfg->doa_cfg.degree_end = cfg.degree_end;
        }
#endif
#endif
        //flow
        cvp_cfg->single_cfg.preGainDb = cfg.preGainDb;
        cvp_cfg->single_aecnlp_cfg.preGainDb = cfg.preGainDb;
        cvp_cfg->dual_cfg.dualPreGainDb = cfg.preGainDb;
        cvp_cfg->dual_flex_cfg.dualPreGainDb = cfg.preGainDb;
        cvp_cfg->dual_clip_cfg.dualPreGainDb = cfg.preGainDb;
        cvp_cfg->dual_clip_v2_cfg.dualPreGainDb = cfg.preGainDb;
        cvp_cfg->tri_cfg.triPreGainDb = cfg.preGainDb;
    } else {
        log_error("CVP-V3 read cfg error,use default param\n");
        p->EnableBit = AEC_EN | NLP_EN; //读取cfg配置文件失败，默认使能AEC和NLP避免选择当前模式时传EnableBit错误
        p->ul_eq_en = 1;
        p->output_sel = CVP_OUTPUT_SEL_DEFAULT;
    }
    void *coeff_bin = cvp_v3_coeff_file_read();
    if (!(cvp_v3->algo_type & CVP_ALGO_1MIC_AECNLP)) {
        cvp_cfg->single_cfg.singleWbEq  = cvp_v3_coeff_info_parse(coeff_bin, WB_EQ, cvp_v3->algo_type);
        cvp_cfg->single_cfg.singleNbEq  = cvp_v3_coeff_info_parse(coeff_bin, NB_EQ, cvp_v3->algo_type);
        cvp_cfg->dual_cfg.dualWnWbEqVec = cvp_v3_coeff_info_parse(coeff_bin, WN_WB_EQ, cvp_v3->algo_type);
        cvp_cfg->dual_cfg.dualWnNbEqVec = cvp_v3_coeff_info_parse(coeff_bin, WN_NB_EQ, cvp_v3->algo_type);
        cvp_cfg->dual_cfg.dualWbEqVec   = cvp_cfg->single_cfg.singleWbEq;
        cvp_cfg->dual_cfg.dualNbEqVec   = cvp_cfg->single_cfg.singleNbEq;
        cvp_cfg->dual_flex_cfg.dualFlexWbEqVec	= cvp_cfg->single_cfg.singleWbEq;
        cvp_cfg->dual_flex_cfg.dualFlexNbEqVec 	= cvp_cfg->single_cfg.singleNbEq;
        cvp_cfg->dual_clip_cfg.dualWbEqVec  	= cvp_cfg->single_cfg.singleWbEq;
        cvp_cfg->dual_clip_cfg.dualNbEqVec  	= cvp_cfg->single_cfg.singleNbEq;
        cvp_cfg->dual_clip_v2_cfg.dualWbEqVec  	= cvp_cfg->single_cfg.singleWbEq;
        cvp_cfg->dual_clip_v2_cfg.dualNbEqVec  	= cvp_cfg->single_cfg.singleNbEq;
        cvp_cfg->tri_cfg.triWbEqVec  	= cvp_cfg->single_cfg.singleWbEq;
        cvp_cfg->tri_cfg.triNbEqVec  	= cvp_cfg->single_cfg.singleNbEq;
        cvp_cfg->dual_clip_cfg.dualWnWbEqVec = cvp_cfg->dual_cfg.dualWnWbEqVec;
        cvp_cfg->dual_clip_cfg.dualWnNbEqVec = cvp_cfg->dual_cfg.dualWnNbEqVec;
        cvp_cfg->dual_clip_v2_cfg.dualWnWbEqVec = cvp_cfg->dual_cfg.dualWnWbEqVec;
        cvp_cfg->dual_clip_v2_cfg.dualWnNbEqVec = cvp_cfg->dual_cfg.dualWnNbEqVec;
        cvp_cfg->dual_flex_cfg.dualWnFlexWbEqVec = cvp_cfg->dual_cfg.dualWnWbEqVec;
        cvp_cfg->dual_flex_cfg.dualWnFlexNbEqVec = cvp_cfg->dual_cfg.dualWnNbEqVec;
        cvp_cfg->tri_cfg.triWnWbEqVec = cvp_cfg->dual_cfg.dualWnWbEqVec;
        cvp_cfg->tri_cfg.triWnNbEqVec = cvp_cfg->dual_cfg.dualWnNbEqVec;
    }

    //对于有参考线的算法模式,如需传入参考线对应的EQ曲线,需手动修改传入get_mag函数的类型为对应的
    if (cvp_v3->algo_type & CVP_ALGO_3MIC) {
        cvp_cfg->tri_cfg.triFbTransferFuncOn  = cvp_v3_coeff_info_parse(coeff_bin, FB_ANC_ON_REF_EQ, cvp_v3->algo_type);
        cvp_cfg->tri_cfg.triFbTransferFuncOff = cvp_v3_coeff_info_parse(coeff_bin, FB_ANC_OFF_REF_EQ, cvp_v3->algo_type);
    }

    if ((cvp_v3->algo_type & CVP_ALGO_3MIC) || (cvp_v3->algo_type & CVP_ALGO_2MIC_HYBRID)) {
        cvp_cfg->nlp2_cfg.preEnhance = 1; 	// 控制双麦hybrid和三麦时fb那一路回声强先做一次
    }
    free(coeff_bin);
    p->algo_type = cvp_v3->algo_type;

    log_info("CVP_V3:AEC[%d] NLP[%d] NS[%d] ENC[%d] DRC[%d] MFDT[%d] WNC[%d] EQ[%d] DOA[%d]", !!(p->EnableBit & AEC_EN), !!(p->EnableBit & NLP_EN), !!(p->EnableBit & ANS_EN), !!(p->EnableBit & ENC_EN), !!(p->EnableBit & AGC_EN), !!(p->EnableBit & MFDT_EN), !!(p->EnableBit & WNC_EN), !!(p->EnableBit & THR_EQ_EN), !!(p->EnableBit & DOA_EN));

#if TCFG_AUDIO_MIC_ARRAY_TRIM_ENABLE
    app_var.enc_degradation = p->target_signal_degradation;
#endif
}


/*
*********************************************************************
*                  Audio CVP Open
* Description: 初始化CVP模块
* Arguments  : sr 			采样率(8000/16000)
*			   enablebit	使能模块(AEC/NLP/AGC/ANS...)
*			   out_hdl		自定义回调函数，NULL则用默认的回调
* Return	 : 0 成功 其他 失败
* Note(s)    : 该接口是对audio_cvp_v3_init的扩展，支持自定义使能模块以及
*			   数据输出回调函数
*********************************************************************
*/
__CVP_BANK_CODE
int audio_cvp_v3_open(struct audio_aec_init_param_t *init_param, s16 enablebit, int (*out_hdl)(s16 *data, u16 len))
{
    u8 aec_dump_enable = 0;
    u32 mic_sr = init_param->sample_rate;
    u32 ref_sr = init_param->ref_sr;
    struct cvp_attr *cvp_param;
    log_info("audio_cvp_v3_open\n");
    mem_stats();
    cvp_v3 = zalloc(sizeof(struct cvp_hdl));
    if (cvp_v3 == NULL) {
        log_error("cvp_v3 malloc failed");
        return -ENOMEM;
    }

    overlay_load_code(OVERLAY_AEC);
    aec_code_movable_load();

    /*初始化dac read的资源*/
    audio_dac_read_init();

    cvp_v3->dump_packet = AEC_OUT_DUMP_PACKET;
    cvp_v3->inbuf_clear_cnt = AEC_IN_DUMP_PACKET;
    cvp_v3->output_fade_in = 1;
    cvp_v3->output_fade_in_gain = 0;
    cvp_param = &cvp_v3->attr;
    cvp_param->cvp_probe = audio_cvp_v3_probe;
    cvp_param->cvp_output = audio_cvp_v3_output;
    cvp_param->wn_en = 0;
    cvp_param->ref_channel = init_param->ref_channel;
    if ((cvp_param->ref_channel == 0) || (cvp_param->ref_channel > 2)) {
        if (cvp_param->ref_channel > 2) {
            printf("[warning] ref_channel is greater than 2, not adapted.\n");
        }
        cvp_param->ref_channel = 1;
    }

    if (ref_sr) {
        if (cvp_param->adc_ref_en) {
            cvp_param->ref_sr  = mic_sr; // 硬回采参考数据采样率ref_sr和ADC采样率sample_rate保持一致
        } else {
            cvp_param->ref_sr  = ref_sr;	  // 软回采
        }
    } else {
        cvp_param->ref_sr  = usb_mic_is_running();
    }

    if (cvp_param->ref_sr == 0) {
        if (TCFG_ESCO_DL_CVSD_SR_USE_16K && (mic_sr == 8000)) {
            cvp_param->ref_sr = 16000;	//CVSD 下行为16K
        } else {
            cvp_param->ref_sr = mic_sr;
        }
    }

    audio_cvp_v3_param_init(cvp_param, init_param->node_uuid);
    aec_dump_enable = audio_cvp_v3_get_aec_dump_enable(cvp_v3->algo_type, &cvp_param->cvp_cfg);
    if (aec_dump_enable) {
        if (cvp_param->cvp_aec_ouput == NULL) {
            cvp_param->cvp_aec_ouput = audio_cvp_v3_aec_cllback;
        }
    } else {
        cvp_param->cvp_aec_ouput = NULL;
    }
    if (enablebit >= 0) {
        cvp_param->EnableBit = enablebit;
    }
    if (out_hdl) {
        cvp_param->cvp_output = out_hdl;
    }
    cvp_param->output_way = global_output_way;

    if (cvp_param->output_way == 0 && cvp_param->adc_ref_en == 0) {
        /*内部读取DAC数据做参考数据才需要做24bit转16bit*/
        extern struct dac_platform_data dac_data;
        cvp_param->ref_bit_width = dac_data.bit_width;
    } else {
        cvp_param->ref_bit_width = DATA_BIT_WIDE_16BIT;
    }

    cvp_param->mic_sr = mic_sr;
    if (mic_sr == 16000) { //WideBand宽带
        cvp_param->hw_delay_offset = 60;
    } else {//NarrowBand窄带
        cvp_param->hw_delay_offset = 55;
    }

    /*获取当前跑的esco type*/
    int type = lmp_private_get_esco_packet_type();
    int frame_time = (lmp_private_get_esco_packet_type() >> 8) & 0xff;
    int media_type = type & 0xff;
    if (media_type == 0) {
        cvp_param->bandwidth = CVP_NB_EN;
    } else  {
        cvp_param->bandwidth = CVP_WB_EN;
    }

#if TCFG_SUPPORT_MIC_CAPLESS
    if (audio_adc_file_get_mic_mode(0) == AUDIO_MIC_CAPLESS_MODE) {
        cvp_v3->dcc_hdl = audio_dc_offset_remove_open(mic_sr, 1);
    }
#endif

    //cvp_param_dump(cvp_param);
    cvp_v3->EnableBit = cvp_param->EnableBit;
    cvp_param->aptfilt_only = 0;
#if (((defined TCFG_KWS_VOICE_RECOGNITION_ENABLE) && TCFG_KWS_VOICE_RECOGNITION_ENABLE) || \
     ((defined TCFG_CALL_KWS_SWITCH_ENABLE) && TCFG_CALL_KWS_SWITCH_ENABLE))
    extern u8 kws_get_state(void);
    if (kws_get_state()) {
        cvp_param->EnableBit = AEC_EN;
        cvp_param->aptfilt_only = 1;
        printf("kws open,aec_enablebit=%x", cvp_param->EnableBit);
        //临时关闭aec, 对比测试
        //cvp_param->toggle = 0;
    }
#endif/*TCFG_KWS_VOICE_RECOGNITION_ENABLE*/

#if WIND_DETECT_INFO_SPP_DEBUG_ENABLE
    if (cvp_v3->attr.EnableBit & WNC_EN) {
        spp_get_operation_table(&cvp_v3->spp_opt);
    }
#endif

    log_info("cvp_v3_open\n");
#if CVP_TOGGLE
    int ret = cvp_init(cvp_param);
    ASSERT(ret == 0, "cvp_v3 open err %d!!", ret);
#endif
    cvp_v3->start = 1;
#if TCFG_AUDIO_DUT_ENABLE
    audio_dut_cvp_control_check();    //产测命令控制检查
#endif
    mem_stats();
    log_info("audio_cvp_v3_open succ\n");
    return 0;
}

/*
*********************************************************************
*                  Audio CVP Init
* Description: 初始化CVP模块
* Arguments  : sample_rate 采样率(8000/16000)
*              ref_sr 参考采样率
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
__CVP_BANK_CODE
int audio_cvp_v3_init(struct audio_aec_init_param_t *init_param)
{
    return audio_cvp_v3_open(init_param, -1, NULL);
}

/*
*********************************************************************
*                  Audio CVP Reboot
* Description: CVP模块复位接口
* Arguments  : reduce 复位/恢复标志
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_cvp_v3_reboot(u8 reduce)
{
    if (cvp_v3) {
        printf("audio_cvp_v3_reboot:%x,%x,start:%d", cvp_v3->EnableBit, cvp_v3->attr.EnableBit, cvp_v3->start);
        if (cvp_v3->start) {
            if (reduce) {
                cvp_v3->attr.EnableBit = AEC_EN;
                cvp_v3->attr.aptfilt_only = 1;
                cvp_reboot(cvp_v3->attr.EnableBit);
            } else {
                if (cvp_v3->EnableBit != cvp_v3->attr.EnableBit) {
                    cvp_v3->attr.aptfilt_only = 0;
                    cvp_reboot(cvp_v3->EnableBit);
                }
            }
        }
    } else {
        printf("audio_aec close now\n");
    }
}

/*
*********************************************************************
*                  Audio CVP Output Select
* Description: 输出选择
* Arguments  : sel = CVP_OUTPUT_SEL_DEFAULT 默认输出算法处理结果
*				   = CVP_OUTPUT_SEL_TALK_MIC	输出主mic(通话mic)的原始数据
*				   = CVP_OUTPUT_SEL_FF_MIC	输出副mic(降噪mic)的原始数据
*			   agc 输出数据要不要经过agc自动增益控制模块
* Return	 : None.
* Note(s)    : 可以通过选择不同的输出，来测试mic的频响和ENC指标
*********************************************************************
*/
void audio_cvp_v3_output_sel(CVP_OUTPUT_ENUM sel, u8 agc)
{
    if (cvp_v3)	{
        printf("cvp_output_sel:%d\n", sel);
        if (agc) {
            cvp_v3->attr.EnableBit |= AGC_EN;
        } else {
            cvp_v3->attr.EnableBit &= ~AGC_EN;
        }
        cvp_v3->attr.output_sel = sel;
    }
}

/*
*********************************************************************
*                  Audio CVP Close
* Description: 关闭CVP模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
__CVP_BANK_CODE
void audio_cvp_v3_close(void)
{
    log_info("audio_cvp_v3_close:%x", (u32)cvp_v3);
    if (cvp_v3) {
        cvp_v3->start = 0;

#if CVP_TOGGLE
        //cppcheck-suppress knownConditionTrueFalse
        cvp_exit();
#if ((defined TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN) && TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN && \
        TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE)
        audio_cvp_icsd_wind_det_close();
#endif
#endif /*CVP_TOGGLE*/

        /*释放dac read的资源*/
        audio_dac_read_exit();

#if TCFG_SUPPORT_MIC_CAPLESS
        if (cvp_v3->dcc_hdl) {
            audio_dc_offset_remove_close(cvp_v3->dcc_hdl);
            cvp_v3->dcc_hdl = NULL;
        }
#endif

        local_irq_disable();

        if (cvp_v3->attr.cvp_cfg.single_cfg.singleWbEq) {
            free(cvp_v3->attr.cvp_cfg.single_cfg.singleWbEq);
            cvp_v3->attr.cvp_cfg.single_cfg.singleWbEq = NULL;
        }

        if (cvp_v3->attr.cvp_cfg.single_cfg.singleNbEq) {
            free(cvp_v3->attr.cvp_cfg.single_cfg.singleNbEq);
            cvp_v3->attr.cvp_cfg.single_cfg.singleNbEq = NULL;
        }

        if (cvp_v3->attr.cvp_cfg.tri_cfg.triFbTransferFuncOn) {
            free(cvp_v3->attr.cvp_cfg.tri_cfg.triFbTransferFuncOn);
            cvp_v3->attr.cvp_cfg.tri_cfg.triFbTransferFuncOn = NULL;
        }

        if (cvp_v3->attr.cvp_cfg.tri_cfg.triFbTransferFuncOff) {
            free(cvp_v3->attr.cvp_cfg.tri_cfg.triFbTransferFuncOff);
            cvp_v3->attr.cvp_cfg.tri_cfg.triFbTransferFuncOff = NULL;
        }

        if (cvp_v3->attr.cvp_cfg.dual_cfg.dualWnWbEqVec) {
            free(cvp_v3->attr.cvp_cfg.dual_cfg.dualWnWbEqVec);
            cvp_v3->attr.cvp_cfg.dual_cfg.dualWnWbEqVec = NULL;
        }

        if (cvp_v3->attr.cvp_cfg.dual_cfg.dualWnNbEqVec) {
            free(cvp_v3->attr.cvp_cfg.dual_cfg.dualWnNbEqVec);
            cvp_v3->attr.cvp_cfg.dual_cfg.dualWnNbEqVec = NULL;
        }

        free(cvp_v3);
        cvp_v3 = NULL;
        local_irq_enable();

        aec_code_movable_unload();
    }
}

/*
*********************************************************************
*                  Audio CVP Status
* Description: CVP模块当前状态
* Arguments  : None.
* Return	 : 0 关闭 其他 打开
* Note(s)    : None.
*********************************************************************
*/
u8 audio_cvp_v3_status(void)
{
    if (cvp_v3) {
        return cvp_v3->start;
    }
    return 0;
}

/*
*********************************************************************
*                  Talk-Mic Data Push
* Description: TALK-MIC数据输入到CVP
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度(Byte)
* Return	 : None.
* Note(s)    : 输入一帧数据，唤醒一次运行任务处理数据，默认帧长256点
*********************************************************************
*/
void audio_cvp_v3_talk_mic_push(s16 *buf, u16 len)
{
    if (len != 512) {
        //aec一帧长度需要256 points,需修改文件(esco_recorder.c/pc_mic_recorder.c)的ADC中断点数
        ASSERT(0, "CVP frame size unsupport %d samples,only support 256 samples\n", len >> 1);

    }
    if (cvp_v3 && cvp_v3->start) {
        if (cvp_v3->input_clear) {
            memset(buf, 0, len);
        }
#if CVP_TOGGLE
        int ret = cvp_talk_mic_push(buf, len);
        if (ret == -1) {
        } else if (ret == -2) {
            log_error("talk-mic input full\n");
        }
#else
        cvp_v3->attr.cvp_output(buf, len);
#endif/*CVP_TOGGLE*/
    }
}

/*
*********************************************************************
*                  FF-Mic Data Push
* Description: FF-MIC数据输入到CVP
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    :
*********************************************************************
*/
void audio_cvp_v3_ff_mic_push(s16 *buf, u16 len)
{
    if (cvp_v3 && cvp_v3->start) {
        cvp_ff_mic_push(buf, len);
    }
}
/*
*********************************************************************
*                  FB-Mic Data Push
* Description: FB-MIC数据输入到CVP
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    :
*********************************************************************
*/

void audio_cvp_v3_fb_mic_push(s16 *buf, u16 len)
{
    if (cvp_v3 && cvp_v3->start) {
        cvp_fb_mic_push(buf, len);
    }
}
/*
*********************************************************************
*                  Audio AEC Reference
* Description: AEC模块参考数据输入
* Arguments  : buf	输入参考数据地址
*			   len	输入参考数据长度
* Return	 : None.
* Note(s)    : 声卡设备是DAC，默认不用外部提供参考数据
*********************************************************************
*/
void audio_cvp_v3_spk_data_push(s16 *data0, s16 *data1, u16 len)
{
    if (cvp_v3 && cvp_v3->start) {
#if CVP_TOGGLE
        cvp_spk_data_push(data0, data1, len);
#endif/*CVP_TOGGLE*/
    }
}


/*
 * 获取风噪检测信息
 * wd_flag: 0 没有风噪，1 有风噪
 * 风噪强度(dB)
 * */
int audio_cvp_v3_get_wind_detect_info(int *wd_flag, float *wd_val)
{
    if (cvp_v3 && cvp_v3->start) {
        return jlsp_cvp_v3_get_wind_detect_info(wd_flag, wd_val);
    }
    return -1;
}


/*
 * 获取宽带窄带信息
 * is_wb_state: 0 窄带 1 宽带
 * */
int audio_cvp_v3_get_bandwidth_info(int is_wb_state)
{
    if (cvp_v3 && cvp_v3->start) {
        return jlsp_cvp_v3_get_bandwidth_info(is_wb_state);
    }
    return -1;
}

/*
 * 麦克风状态定义：
 * 0: 正常使用双麦做Beamforming
 * 1: 正常工作（Talk 模式）
 * 2: 前馈麦克风工作（FF 模式）
 */

int audio_cvp_v3_get_mic_state_info(int mic_state)
{
    if (cvp_v3 && cvp_v3->start) {
        return jlsp_cvp_v3_get_mic_state_info(mic_state);
    }
    return -1;
}

/*
*********************************************************************
*                  			Audio CVP IOCTL
* Description: CVP功能配置
* Arguments  : cmd 		操作命令
*		       value 	操作数
*		       priv 	操作内存地址
* Return	 : 0 成功 其他 失败
* Note(s)    : (1)比如动态开关降噪NS模块:
*				audio_cvp_v3_ioctl(CVP_NS_SWITCH,1,NULL);	//降噪开
*				audio_cvp_v3_ioctl(CVP_NS_SWITCH,0,NULL);   //降噪关
*********************************************************************
*/
int audio_cvp_v3_ioctl(int cmd, int value, void *priv)
{
    if (cvp_v3 && cvp_v3->start) {
        return cvp_ioctl(cmd, value, priv);
    } else {
        return -1;
    }

}

/*
*********************************************************************
*                  Audio CVP Toggle Set
* Description: CVP模块算法开关使能
* Arguments  : toggle 0 关闭算法 1 打开算法
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
int audio_cvp_v3_toggle_set(u8 toggle)
{
    if (cvp_v3) {
        cvp_toggle(toggle);
        return 0;
    }
    return 1;
}

//获取算法中anc模式
int audio_cvp_v3_get_anc_mode(int *mode)
{
    if (!mode) {
        return -1;
    }
    if (cvp_v3 && cvp_v3->start) {
        return jlsp_cvp_v3_get_anc_mode(mode);
    }
    return -1;
}

/* 设置跑2mic/3mic降噪
 * CVP_V3_TMS_MODE_2MIC  跑双麦降噪，不使用FB MIC数据
 * CVP_V3_TMS_MODE_3MIC  跑3MIC降噪
 * */
int audio_cvp_v3_tms_mode_choose(enum cvp_v3_tms_mode mode)
{
    if (cvp_v3 && cvp_v3->start) {
        return cvp_ioctl(CVP_SET_MIC_NUM_ALGO, TRI_SET_MIC_NUM, &mode);
    } else {
        return -1;
    }
}


/**
 * 以下为用户层扩展接口
 */
//pbg profile use it,don't delete
void audio_cvp_v3_input_clear_enable(u8 enable)
{
    if (cvp_v3) {
        cvp_v3->input_clear = enable;
        log_info("aec_input_clear_enable= %d\n", enable);
    }
}
#endif/*TCFG_AUDIO_CVP_V3_ENABLE == 1*/
#endif /*TCFG_CVP_DEVELOP_ENABLE*/

