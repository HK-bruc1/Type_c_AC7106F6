#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_cvp_online.data.bss")
#pragma data_seg(".audio_cvp_online.data")
#pragma const_seg(".audio_cvp_online.text.const")
#pragma code_seg(".audio_cvp_online.text")
#endif
#include "audio_cvp_online.h"
#include "audio_cvp_def.h"
#include "app_online_cfg.h"
#include "config/config_target.h"
#include "system/includes.h"
#include "online_db_deal.h"
#include "timer.h"
#include "cvp_node.h"
#include "app_main.h"
#include "adc_file.h"
#include "audio_cvp.h"
#include "lib_h/jlsp_v3_ns.h"
#include "lib_h/jlsp_DualMicSystem_Hybrid.h"

extern float eq_db2mag(float x);
#if 1
extern void put_float(double fv);
#define AEC_ONLINE_LOG	y_printf
#define AEC_ONLINE_FLOG	put_float
#else
#define AEC_ONLINE_LOG(...)
#define AEC_ONLINE_FLOG(...)
#endif

#if (TCFG_CFG_TOOL_ENABLE || TCFG_AEC_TOOL_ONLINE_ENABLE)
const int const_audio_cvp_debug_online_enable = 1;
#else
const int const_audio_cvp_debug_online_enable = 0;
#endif

/*fb eq工具参数结构体*/
/* struct dns_coeff_data_handle {
    u16 data_len;
    u8 mic_mode : 4;    //算法类型,1:2mic,2:3mic，暂时没有使用
    u8 eq_sel : 2;      //0:eq曲线，1:参考线
    u8 bypass : 2;      //暂时没有使用
    u8 reserved1;
    u16 fft_points;     //fft点数，256/512
    u16 sample_rate;    //采样率，8000/16000
    u8 data[0];         //参考线数据/eq数据/中心点数据
}; */

/*三代算法调音工具（兼容一代2mic-hybrid和3mic）结构体*/
struct dns_coeff_data_handle {
    u16 data_len;         // 数据长度 (16位)
    u16 mic_mode : 4;     // 算法类型 (4位): 1: 2mic, 2: 3mic, 3: CVP-V3-1mic, 4: CVP-V3-2mic, 5: CVP-V3-2mic-Hybrid, 6: CVP-V3-3mic, 7: CVP-V3-3mic-ANC
    u16 eq_sel_lsb : 2;   // 目标数据低位 (2位): 0: EQ曲线, 1: 参考线
    u16 bypass : 2;       // 暂时未使用 (2位)
    u16 eq_sel_msb : 3;   // 目标数据高位 (3位)
    u16 reserved1 : 5;    // 保留字段 (5位)
#if TCFG_AUDIO_CVP_V3_MODE
    float eq_total_gain;    // eq总增益
#endif
    u16 fft_points;       // FFT点数 (256/512)
    u16 sample_rate;      // 采样率 (8000/16000)
    float data[];         // 参考线数据 / EQ数据 / 中心点数据
};

extern const u8 CONST_DMS_HYBRID_ONLINE_TYPE;

#if (TCFG_CFG_TOOL_ENABLE)
/* CVP_ANALYSIS工具针对CVP_V3节点EQ类型定义 */
#define MAX_FFT_POINTS 						(256)
#define CVP_V3_ALGO_WB_EQ_INDEX				(2)
#define CVP_V3_ALGO_WB_REF_EQ_INDEX			(3)
#define CVP_V3_ALGO_NB_EQ_INDEX				(4)
#define CVP_V3_ALGO_NB_REF_EQ_INDEX			(5)
#define CVP_V3_ALGO_ANC_ON_EQ_INDEX			(6)
#define CVP_V3_ALGO_ANC_ON_REF_EQ_INDEX		(7)
#define CVP_V3_ALGO_ANC_OFF_EQ_INDEX		(8)
#define CVP_V3_ALGO_ANC_OFF_REF_EQ_INDEX	(9)
#define CVP_V3_ALGO_ANC_TRANS_EQ_INDEX		(10)
#define CVP_V3_ALGO_ANC_TRANS_REF_EQ_INDEX	(11)
#define CVP_V3_ALGO_WN_WB_EQ_INDEX			(12)
#define CVP_V3_ALGO_WN_WB_REF_EQ_INDEX		(13)
#define CVP_V3_ALGO_WN_NB_EQ_INDEX			(14)
#define CVP_V3_ALGO_WN_NB_REF_EQ_INDEX		(15)
#define CVP_V3_ALGO_SET_NORMAL_EQ			(1)
#define CVP_V3_ALGO_SET_WN_EQ				(2)
/*
*********************************************************************
*                  dns_coeff_param_updata
* Description: fb eq 参数在线更新
* Arguments  : coeff_file   文件路径
*			   data	        数据地址
*			   len		    数据长度
* Return	 : 0 成功 其他 失败
* Note(s)    : null
*********************************************************************
*/
int dns_coeff_param_updata(const char *coeff_file, void *data, int len)
{
    int coeff_len;
    if (!data) {
        printf("[error] online data is NULL !!!");
        return -1;
    }
    struct dns_coeff_data_handle *data_hdl = (struct dns_coeff_data_handle *)data;//zalloc(param_len);
    printf("dns_coeff_param_updata data->len : %d and len : %d", data_hdl->data_len, len);
    /*在线调试的时候直接更新eq参数*/
    extern u8 get_cvp_context_status();
    if (!get_cvp_context_status()) {
        printf("[error] aec is close !!!");
        return 0;
    }
    if (data_hdl->data_len != len) {
        printf("[error] data_len[%d != %d] err !!!", data_hdl->data_len, len);
        return -1;
    }

    u16 eq_sel = (u16)((data_hdl->eq_sel_msb << 2) | data_hdl->eq_sel_lsb);
    printf("data_len=%d, mic_mode=%d, eq_sel_lsb=%d, eq_sel_msb=%d, eq_sel=%d, "
           "bypass=%d, reserved1=%d, fft_points=%d, sample_rate=%d\n",
           data_hdl->data_len,
           data_hdl->mic_mode,
           data_hdl->eq_sel_lsb,
           data_hdl->eq_sel_msb,
           eq_sel,
           data_hdl->bypass,
           data_hdl->reserved1,
           data_hdl->fft_points,
           data_hdl->sample_rate);
    coeff_len = ((data_hdl->fft_points >> 1) + 1) * sizeof(float);
    printf("coeff_len %d", coeff_len);
#if TCFG_AUDIO_CVP_V3_MODE
    float total_gain;
    if (eq_sel == CVP_V3_ALGO_WB_EQ_INDEX || eq_sel == CVP_V3_ALGO_NB_EQ_INDEX) {
        /* printf("current eq_total_gain :\n");
        put_float(data_hdl->eq_total_gain); */
        total_gain = eq_db2mag(data_hdl->eq_total_gain);
    }

    static float eq_temp[MAX_FFT_POINTS + 1];
    memcpy(eq_temp, data_hdl->data, coeff_len);
    for (int i = 0; i < (data_hdl->fft_points >> 1) + 1; i++) {
        eq_temp[i] = eq_db2mag(eq_temp[i]);
    }

    if (eq_sel >= CVP_V3_ALGO_WB_EQ_INDEX && eq_sel <= CVP_V3_ALGO_NB_REF_EQ_INDEX) {
        static JLSP_set_wbornb_eq eq_cfg;
        if (eq_sel == CVP_V3_ALGO_WB_EQ_INDEX || eq_sel == CVP_V3_ALGO_WB_REF_EQ_INDEX) {
            eq_cfg.is_wb = 1;
        } else {
            eq_cfg.is_wb = 0;
        }
        eq_cfg.eqCoeffs = eq_temp;
        eq_cfg.type = CVP_V3_ALGO_SET_NORMAL_EQ;
        if (eq_sel == CVP_V3_ALGO_WB_EQ_INDEX || eq_sel == CVP_V3_ALGO_NB_EQ_INDEX) {
            for (int i = 0; i < (data_hdl->fft_points >> 1) + 1; i++) {
                eq_cfg.eqCoeffs[i] = eq_cfg.eqCoeffs[i] * total_gain;
                /* put_float(eq_cfg.eqCoeffs[i]);
                printf(","); */
            }

        }
        audio_cvp_v3_ioctl(CVP_SET_EQ, SET_WBORNB_EQ, &eq_cfg);
    } else if (eq_sel >= CVP_V3_ALGO_ANC_ON_EQ_INDEX && eq_sel <= CVP_V3_ALGO_ANC_TRANS_REF_EQ_INDEX) {
#if CONFIG_ANC_ENABLE
        extern u8 anc_mode_get(void);
        u8 anc_mode = anc_mode_get();
        printf("current anc mode: %d\n", anc_mode);
        /*下发曲线的类型与当前ANC 模式不匹配时,需set算法内部为当前下发的模式*/
        int update_anc_mode;
        switch (eq_sel) {
        case CVP_V3_ALGO_ANC_ON_EQ_INDEX:
        case CVP_V3_ALGO_ANC_ON_REF_EQ_INDEX:
            update_anc_mode = 2;
            break;
        case CVP_V3_ALGO_ANC_OFF_EQ_INDEX:
        case CVP_V3_ALGO_ANC_OFF_REF_EQ_INDEX:
            update_anc_mode = 1;
            break;
        case CVP_V3_ALGO_ANC_TRANS_EQ_INDEX:
        case CVP_V3_ALGO_ANC_TRANS_REF_EQ_INDEX:
            update_anc_mode = 3;
            break;
        default:
            update_anc_mode = 0;
        }
        /*通透暂时用不到*/
        if (update_anc_mode != anc_mode) {
            static JLSP_set_anc_state_mode anc_state_mode;
            anc_state_mode.isAncOn = update_anc_mode;
            anc_state_mode.ancMode = 0;
            audio_cvp_v3_ioctl(CVP_SET_ANC_STATEMODE, TRI_SET_ANC_STATEMODE, &anc_state_mode);
            static JLSP_set_fb2main_eq fb2main_eq;
            fb2main_eq.isAncon = update_anc_mode;
            fb2main_eq.fb2main_eq = eq_temp;
            audio_cvp_v3_ioctl(CVP_SET_EQ, TRI_SET_FB2MAIN_EQ, &fb2main_eq);
        } else {
            static JLSP_set_fb2main_eq fb2main_eq;
            fb2main_eq.isAncon 	  = update_anc_mode;
            fb2main_eq.fb2main_eq = eq_temp;
            audio_cvp_v3_ioctl(CVP_SET_EQ, TRI_SET_FB2MAIN_EQ, &fb2main_eq);
        }
#endif
    } else if (eq_sel >= CVP_V3_ALGO_WN_WB_EQ_INDEX && eq_sel <= CVP_V3_ALGO_WN_NB_REF_EQ_INDEX) {
        static JLSP_set_wbornb_eq eq_cfg;
        if (eq_sel == CVP_V3_ALGO_WN_WB_EQ_INDEX || eq_sel == CVP_V3_ALGO_WN_WB_REF_EQ_INDEX) {
            eq_cfg.is_wb = 1;
        } else {
            eq_cfg.is_wb = 0;
        }
        eq_cfg.eqCoeffs = eq_temp;
        eq_cfg.type = CVP_V3_ALGO_SET_WN_EQ;
        audio_cvp_v3_ioctl(CVP_SET_EQ, SET_WBORNB_EQ, &eq_cfg);
    }
#endif

#if TCFG_AUDIO_CVP_DMS_HYBRID_DNS_MODE
    //在线更新fb eq曲线参数
    static float eq_temp[MAX_FFT_POINTS + 1];
    memcpy(eq_temp, data_hdl->data, coeff_len);

    for (int i = 0; i < (data_hdl->fft_points >> 1) + 1; i++) {
        eq_temp[i] = eq_db2mag(eq_temp[i]);
    }

    //在线更新fb eq曲线参数
    if (CONST_DMS_HYBRID_ONLINE_TYPE == 0) {
        audio_cvp_ioctl(CVP_SET_EQ, 0, eq_temp);
    } else if (CONST_DMS_HYBRID_ONLINE_TYPE == 1) {
        //在线更新talk eq曲线参数
        audio_cvp_ioctl(CVP_SET_EQ, 0, eq_temp);
    }
#elif TCFG_AUDIO_CVP_3MIC_MODE
    /* jlsp_tms_set_fbeq((float *)data_hdl->data, coeff_len); */
    /* jlsp_tms_set_talkeq((float *)data_hdl->data, coeff_len); */
#endif

    return 0;
}
#endif

/*
*********************************************************************
*                  read_dns_coeff_param
* Description: fb eq 参数文件读取
* Arguments  : coeff_file   文件路径
* Return	 : 成功返回数据地址,失败返回null
* Note(s)    : null
*********************************************************************
*/
void *read_dns_coeff_param(const char *coeff_file)
{
    RESFILE *fp = NULL;
    int coeff_len, len;
    struct dns_coeff_data_handle *data_hdl = NULL;
    printf("read_dns_fb_coeff_param");
    //===============================//
    //          打开参数文件         //
    //===============================//
    fp = resfile_open(coeff_file);
    if (!fp) {
        printf("[error] open %s fail !!!", coeff_file);
        return NULL;
    }
    len = resfile_get_len(fp);
    if (len) {
        data_hdl = NULL;
        data_hdl = zalloc(len);
    }
    if (data_hdl == NULL) {
        printf("[error] zalloc err !!!");
        resfile_close(fp);
        return NULL;
    }

    int rlen = resfile_read(fp, data_hdl, len);
    if (rlen != len) {
        printf("[error] read DNSFB_Coeff.bin err !!! %d =! %d", rlen, len);
        resfile_close(fp);
        return NULL;
    }
    resfile_close(fp);
    coeff_len = ((data_hdl->fft_points >> 1) + 1) * sizeof(float);
    printf("coeff_len %d", coeff_len);
    void *TransferFunc = zalloc(coeff_len);
    /*适配三代算法调音*/
    u16 eq_sel = (u16)((data_hdl->eq_sel_msb << 2) | data_hdl->eq_sel_lsb);
    printf("data_len=%d, mic_mode=%d, eq_sel_lsb=%d, eq_sel_msb=%d, eq_sel=%d, "
           "bypass=%d, reserved1=%d, fft_points=%d, sample_rate=%d\n",
           data_hdl->data_len,
           data_hdl->mic_mode,
           data_hdl->eq_sel_lsb,
           data_hdl->eq_sel_msb,
           eq_sel,
           data_hdl->bypass,
           data_hdl->reserved1,
           data_hdl->fft_points,
           data_hdl->sample_rate);

    if (eq_sel == 1) {
        /*参考线数据*/
        memcpy(TransferFunc, data_hdl->data, coeff_len);
    } else if (eq_sel == 0) {
        /*eq曲线数据*/
        memcpy(TransferFunc, data_hdl->data + coeff_len, coeff_len);
    }
    /* printf("\n\n\n ref data ==============================="); */
    /* put_buf(data_hdl->data, coeff_len); */
    /* printf("\n\n\n eq data ==============================="); */
    /* put_buf(data_hdl->data + coeff_len, coeff_len); */
    free(data_hdl);
    return TransferFunc;

}

#if TCFG_AEC_TOOL_ONLINE_ENABLE

extern int esco_dec_dac_gain_set(u8 gain);
extern int esco_dec_dac_gain_get(void);
enum {
    AEC_UPDATE_CLOSE,
    AEC_UPDATE_INIT,
    AEC_UPDATE_ONLINE,
};

typedef struct {
    u8 update;
    u8 reserved;
    u16 timer;
#if TCFG_AUDIO_TRIPLE_MIC_ENABLE
    AEC_TMS_CONFIG cfg;
#elif TCFG_AUDIO_DUAL_MIC_ENABLE
#if (TCFG_AUDIO_DMS_SEL == DMS_NORMAL)
    AEC_DMS_CONFIG cfg;
#else/*TCFG_AUDIO_DMS_SEL == DMS_FLEXIBLE*/
    DMS_FLEXIBLE_CONFIG cfg;
#endif/*TCFG_AUDIO_DMS_SEL*/
#else/*SINGLE MIC*/
    AEC_CONFIG cfg;
    u8 agc_en;
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
} aec_update_t;
aec_update_t *aec_update = NULL;

static void aec_update_timer_deal(void *priv)
{
    AEC_ONLINE_LOG("aec_update_timer_deal");
#if TCFG_AUDIO_TRIPLE_MIC_ENABLE
    aec_tms_cfg_update(&aec_update->cfg);
#elif TCFG_AUDIO_DUAL_MIC_ENABLE
#if (TCFG_AUDIO_DMS_SEL == DMS_NORMAL)
    aec_dms_cfg_update(&aec_update->cfg);
#else
    aec_dms_flexible_cfg_update(&aec_update->cfg);
#endif /*TCFG_AUDIO_DMS_SEL*/
#else
    aec_cfg_update(&aec_update->cfg);
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
    sys_timer_del(aec_update->timer);
    aec_update->timer = 0;
}

int aec_cfg_online_update(int root_cmd, void *packet)
{
    if ((root_cmd != AEC_CFG_SMS)          && (root_cmd != AEC_CFG_DMS) &&
        (root_cmd != AEC_CFG_SMS_DNS)      && (root_cmd != AEC_CFG_DMS_DNS) &&
        (root_cmd != AEC_CFG_DMS_FLEXIBLE) && (root_cmd != AEC_CFG_DMS_FLEXIBLE_DNS) &&
        (root_cmd != AEC_CFG_TMS_DNS)) {
        return -1;
    }
    if (aec_update->update == AEC_UPDATE_CLOSE) {
        printf("AEC_UPDATE_CLOSE");
        return 0;
    }
    u8 update = 1;
    u8 mic_index = 0;
    aec_online_t *cfg = packet;
    int id = cfg->id;
    //AEC_ONLINE_LOG("AEC_TYPE[0x30xx:SMS 0x31xx:DMS]:%x",root_cmd);
    //AEC_ONLINE_LOG("aec_cfg_id:%x,val:%d", cfg->id, (int)cfg->val_int);
    //AEC_ONLINE_FLOG(cfg->val_float);
    if (id >= WN_MSC_TH) {
        AEC_ONLINE_LOG("WNC cfg update\n");
#if TCFG_AUDIO_TRIPLE_MIC_ENABLE
        switch (id) {
        case WN_MSC_TH:
            AEC_ONLINE_LOG("wn_msc_th:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ms_th = cfg->val_float;
            break;
        case MS_TH:
            AEC_ONLINE_LOG("ms_th:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.wn_msc_th = cfg->val_float;
            break;
        case WN_GAIN_OFFSET:
            AEC_ONLINE_LOG("wn_gain_offset:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.wn_gain_offset = cfg->val_float;
            break;
        default:
            AEC_ONLINE_LOG("wnc param default:%x\n", id, cfg->val_int);
            AEC_ONLINE_FLOG(cfg->val_float);
            break;
        }
#endif /*TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
    } else if (id >= ENC_Process_MaxFreq) {
        AEC_ONLINE_LOG("ENC cfg update\n");
        switch (id) {
#if (TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
#if (TCFG_AUDIO_DMS_SEL == DMS_NORMAL) || TCFG_AUDIO_TRIPLE_MIC_ENABLE
        case ENC_Process_MaxFreq:
            AEC_ONLINE_LOG("ENC_Process_MaxFreq:%d\n", cfg->val_int);
            aec_update->cfg.enc_process_maxfreq = cfg->val_int;
            break;
        case ENC_Process_MinFreq:
            AEC_ONLINE_LOG("ENC_Process_MinFreq:%d\n", cfg->val_int);
            aec_update->cfg.enc_process_minfreq = cfg->val_int;
            break;
        case ENC_SIR_MaxFreq:
            AEC_ONLINE_LOG("ENC_SIR_MaxFreq:%d\n", cfg->val_int);
            aec_update->cfg.sir_maxfreq = cfg->val_int;
            break;
        case ENC_MIC_Distance:
            AEC_ONLINE_LOG("ENC_MIC_Distance:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.mic_distance = cfg->val_float;
            break;
        case ENC_Target_Signal_Degradation:
            AEC_ONLINE_LOG("ENC_Target_Signal_Degradation:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.target_signal_degradation = cfg->val_float;
            break;
        case ENC_AggressFactor:
            AEC_ONLINE_LOG("ENC_AggressFactor:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.enc_aggressfactor = cfg->val_float;
            break;
        case ENC_MinSuppress:
            AEC_ONLINE_LOG("ENC_MinSuppress:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.enc_minsuppress = cfg->val_float;
            break;
#else/*TCFG_AUDIO_DMS_SEL == DMS_FLEXIBLE*/
        case ENC_Suppress_Pre:
            AEC_ONLINE_LOG("ENC_Suppress_Pre:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.enc_suppress_pre = cfg->val_float;
            break;
        case ENC_Suppress_Post:
            AEC_ONLINE_LOG("ENC_Suppress_Post:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.enc_suppress_post = cfg->val_float;
            break;
        case ENC_MinSuppress:
            AEC_ONLINE_LOG("ENC_MinSuppress:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.enc_minsuppress = cfg->val_float;
            break;
        case ENC_Disconverge_Thr:
            AEC_ONLINE_LOG("ENC_Disconverge_Thr:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.enc_disconverge_erle_thr = cfg->val_float;
            break;

#endif/*TCFG_AUDIO_DMS_SEL*/
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
#if TCFG_AUDIO_TRIPLE_MIC_ENABLE
        case Tri_SnrThreshold0:
            AEC_ONLINE_LOG("Tri_SnrThreshold0:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.Tri_SnrThreshold0 = cfg->val_float;
            break;
        case Tri_SnrThreshold1:
            AEC_ONLINE_LOG("Tri_SnrThreshold1:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.Tri_SnrThreshold1 = cfg->val_float;
            break;
        case Tri_CompenDb:
            AEC_ONLINE_LOG("Tri_CompenDb:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.Tri_CompenDb = cfg->val_float;
            break;
#endif /*TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
        default:
            AEC_ONLINE_LOG("enc param default:%x\n", id, cfg->val_int);
            AEC_ONLINE_FLOG(cfg->val_float);
            break;
        }
    } else if (id >= AGC_NDT_FADE_IN) {
        AEC_ONLINE_LOG("AGC cfg update\n");
        switch (id) {
#if (TCFG_AUDIO_TRIPLE_MIC_ENABLE)
        case AGC_NDT_FADE_IN:
            AEC_ONLINE_LOG("AGC_NDT_FADE_IN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.ndt_fade_in = cfg->val_float;
            break;
        case AGC_NDT_FADE_OUT:
            AEC_ONLINE_LOG("AGC_NDT_FADE_OUT:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.ndt_fade_out = cfg->val_float;
            break;
        case AGC_DT_FADE_IN:
            AEC_ONLINE_LOG("AGC_DT_FADE_IN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.dt_fade_in = cfg->val_float;
            break;
        case AGC_DT_FADE_OUT:
            AEC_ONLINE_LOG("AGC_DT_FADE_OUT:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.dt_fade_out = cfg->val_float;
            break;
        case AGC_NDT_MAX_GAIN:
            AEC_ONLINE_LOG("AGC_NDT_MAX_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.ndt_max_gain = cfg->val_float;
            break;
        case AGC_NDT_MIN_GAIN:
            AEC_ONLINE_LOG("AGC_NDT_MIN_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.ndt_min_gain = cfg->val_float;
            break;
        case AGC_NDT_SPEECH_THR:
            AEC_ONLINE_LOG("AGC_NDT_SPEECH_THR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.ndt_speech_thr = cfg->val_float;
            break;
        case AGC_DT_MAX_GAIN:
            AEC_ONLINE_LOG("AGC_DT_MAX_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.dt_max_gain = cfg->val_float;
            break;
        case AGC_DT_MIN_GAIN:
            AEC_ONLINE_LOG("AGC_DT_MIN_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.dt_min_gain = cfg->val_float;
            break;
        case AGC_DT_SPEECH_THR:
            AEC_ONLINE_LOG("AGC_DT_SPEECH_THR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.dt_speech_thr = cfg->val_float;
            break;
        case AGC_ECHO_PRESENT_THR:
            AEC_ONLINE_LOG("AGC_ECHO_PRESENT_THR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_ext.echo_present_thr = cfg->val_float;
            break;

        case AGC_TYPE:
            AEC_ONLINE_LOG("AGC_TYPE:%d\n", cfg->val_int);
            aec_update->cfg.agc_type = cfg->val_int;
            break;
        case AGC_MIN_MAG_LEVEL:
            AEC_ONLINE_LOG("AGC_MIN_MAG_LEVEL:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_int.min_mag_db_level = cfg->val_float;
            break;
        case AGC_MAX_MAG_LEVEL:
            AEC_ONLINE_LOG("AGC_MAX_MAG_LEVEL:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_int.max_mag_db_level = cfg->val_float;
            break;
        case AGC_ADDITION_MAG_LEVEL:
            AEC_ONLINE_LOG("AGC_ADDITION_MAG_LEVEL:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_int.addition_mag_db_level = cfg->val_float;
            break;
        case AGC_CLIP_MAG_LEVEL:
            AEC_ONLINE_LOG("AGC_CLIP_MAG_LEVEL:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_int.clip_mag_db_level = cfg->val_float;
            break;
        case AGC_FLOOR_MAG_LEVEL:
            AEC_ONLINE_LOG("AGC_FLOOR_MAG_LEVEL:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.agc.agc_int.floor_mag_db_level = cfg->val_float;
            break;

#else /*(TCFG_AUDIO_TRIPLE_MIC_ENABLE == 0)*/
        case AGC_NDT_FADE_IN:
            AEC_ONLINE_LOG("AGC_NDT_FADE_IN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ndt_fade_in = cfg->val_float;
            break;
        case AGC_NDT_FADE_OUT:
            AEC_ONLINE_LOG("AGC_NDT_FADE_OUT:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ndt_fade_out = cfg->val_float;
            break;
        case AGC_DT_FADE_IN:
            AEC_ONLINE_LOG("AGC_DT_FADE_IN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.dt_fade_in = cfg->val_float;
            break;
        case AGC_DT_FADE_OUT:
            AEC_ONLINE_LOG("AGC_DT_FADE_OUT:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.dt_fade_out = cfg->val_float;
            break;
        case AGC_NDT_MAX_GAIN:
            AEC_ONLINE_LOG("AGC_NDT_MAX_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ndt_max_gain = cfg->val_float;
            break;
        case AGC_NDT_MIN_GAIN:
            AEC_ONLINE_LOG("AGC_NDT_MIN_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ndt_min_gain = cfg->val_float;
            break;
        case AGC_NDT_SPEECH_THR:
            AEC_ONLINE_LOG("AGC_NDT_SPEECH_THR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ndt_speech_thr = cfg->val_float;
            break;
        case AGC_DT_MAX_GAIN:
            AEC_ONLINE_LOG("AGC_DT_MAX_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.dt_max_gain = cfg->val_float;
            break;
        case AGC_DT_MIN_GAIN:
            AEC_ONLINE_LOG("AGC_DT_MIN_GAIN:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.dt_min_gain = cfg->val_float;
            break;
        case AGC_DT_SPEECH_THR:
            AEC_ONLINE_LOG("AGC_DT_SPEECH_THR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.dt_speech_thr = cfg->val_float;
            break;
        case AGC_ECHO_PRESENT_THR:
            AEC_ONLINE_LOG("AGC_ECHO_PRESENT_THR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.echo_present_thr = cfg->val_float;
            break;
#endif /*TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
        }
    } else if (id >= ANS_AggressFactor) {
        AEC_ONLINE_LOG("ANS cfg update\n");
        switch (id) {
#if (TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
        case ANS_AggressFactor:
            AEC_ONLINE_LOG("ANS_AggressFactor:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.aggressfactor = cfg->val_float;
            break;
        case ANS_MinSuppress:
            AEC_ONLINE_LOG("ANS_MinSuppress:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.minsuppress = cfg->val_float;
            break;
        case ANS_MicNoiseLevel:
            AEC_ONLINE_LOG("ANS_MicNoiseLevel:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.init_noise_lvl = cfg->val_float;
            break;
#else
        case ANS_AGGRESS:
            AEC_ONLINE_LOG("ANS_AGGRESS:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ans_aggress = cfg->val_float;
            break;
        case ANS_SUPPRESS:
            AEC_ONLINE_LOG("ANS_SUPPRESS:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ans_suppress = cfg->val_float;
            break;
        case DNS_GainFloor:
            AEC_ONLINE_LOG("DNS_GainFloor:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ans_suppress = cfg->val_float;
            break;
        case DNS_OverDrive:
            AEC_ONLINE_LOG("DNS_OverDrive:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.ans_aggress = cfg->val_float;
            break;
#endif /*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
        }
    } else if (id >= NLP_ProcessMaxFreq) {
        AEC_ONLINE_LOG("NLP cfg update\n");
        switch (id) {
#if (TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
        case NLP_ProcessMaxFreq:
            AEC_ONLINE_LOG("NLP_ProcessMaxFreq:%d\n", cfg->val_int);
            aec_update->cfg.nlp_process_maxfrequency = cfg->val_int;
            break;
        case NLP_ProcessMinFreq:
            AEC_ONLINE_LOG("NLP_ProcessMinFreq:%d\n", cfg->val_int);
            aec_update->cfg.nlp_process_minfrequency = cfg->val_int;
            break;
        case NLP_OverDrive:
            AEC_ONLINE_LOG("NLP_OverDrive:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.overdrive = cfg->val_float;
            break;
#else/*SINGLE MIC*/
        case NLP_AGGRESS_FACTOR:
            AEC_ONLINE_LOG("NLP_AGGRESS_FACTOR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.es_aggress_factor = cfg->val_float;
            break;
        case NLP_MIN_SUPPRESS:
            AEC_ONLINE_LOG("NLP_MIN_SUPPRESS:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.es_min_suppress = cfg->val_float;
            break;
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
        }
    } else if (id >= AEC_ProcessMaxFreq) {
        switch (id) {
#if (TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
        case AEC_ProcessMaxFreq:
            AEC_ONLINE_LOG("AEC_ProcessMaxFreq:%d\n", cfg->val_int);
            aec_update->cfg.aec_process_maxfrequency = cfg->val_int;
            break;
        case AEC_ProcessMinFreq:
            AEC_ONLINE_LOG("AEC_ProcessMinFreq:%d\n", cfg->val_int);
            aec_update->cfg.aec_process_minfrequency = cfg->val_int;
            break;
        case AEC_AF_Lenght:
            AEC_ONLINE_LOG("AEC_AF_Lenght:%d\n", cfg->val_int);
            aec_update->cfg.af_length = cfg->val_int;
            break;
#else/*SINGLE MIC*/
        case AEC_DT_AGGRESS:
            AEC_ONLINE_LOG("AEC_DT_AGGRESS:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.aec_dt_aggress = cfg->val_float;
            break;
        case AEC_REFENGTHR:
            AEC_ONLINE_LOG("AEC_REFENGTHR:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.aec_refengthr = cfg->val_float;
            break;
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
        }
    } else {
        AEC_ONLINE_LOG("General cfg update\n");
        switch (id) {
        case GENERAL_DAC:
            AEC_ONLINE_LOG("DAC_Gain:%d\n", cfg->val_int);
            aec_update->cfg.dac_again =  cfg->val_int;
            esco_dec_dac_gain_set(cfg->val_int);
            update = 0;
            break;
        case GENERAL_TALK_MIC:
            AEC_ONLINE_LOG("TALK_Mic_Gain:%d\n", cfg->val_int);
            aec_update->cfg.mic_again =  cfg->val_int;
            mic_index = audio_get_mic_index(cvp_get_talk_mic_ch());
            audio_adc_file_set_gain(mic_index, cfg->val_int);
#if (TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
#if ((TCFG_AUDIO_DMS_SEL == DMS_NORMAL) || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
            mic_index = audio_get_mic_index(cvp_get_talk_ref_mic_ch());
            audio_adc_file_set_gain(mic_index, cfg->val_int);
#endif/*TCFG_AUDIO_DMS_SEL*/
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
            update = 0;
            break;
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#if (TCFG_AUDIO_DMS_SEL == DMS_FLEXIBLE)
        case GENERAL_REF_MIC:
            AEC_ONLINE_LOG("REF_Mic_Gain:%d\n", cfg->val_int);
            aec_update->cfg.mic1_again =  cfg->val_int;
            mic_index = audio_get_mic_index(cvp_get_talk_ref_mic_ch());
            audio_adc_file_set_gain(mic_index, cfg->val_int);
            update = 0;
            break;
#endif/*TCFG_AUDIO_DMS_SEL*/
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
#if (TCFG_AUDIO_TRIPLE_MIC_ENABLE)
        case GENERAL_FB_MIC:
            AEC_ONLINE_LOG("FB_Mic_Gain:%d\n", cfg->val_int);
            aec_update->cfg.fb_mic_again =  cfg->val_int;
            mic_index = audio_get_mic_index(cvp_get_talk_fb_mic_ch());
            audio_adc_file_set_gain(mic_index, cfg->val_int);
            update = 0;
            break;
        case GENERAL_OUTPUT_SEL:
            AEC_ONLINE_LOG("GENERAL_OUTPUT_SEL:%d\n", cfg->val_int);
            aec_update->cfg.output_sel = cfg->val_int;
            break;
#endif/*TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
        case GENERAL_ModuleEnable:
#if (TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
            AEC_ONLINE_LOG("DMS EnableBit:%x", cfg->val_int);
            aec_update->cfg.enable_module = cfg->val_int;
#else
            AEC_ONLINE_LOG("SMS EnableBit:%x", cfg->val_int);
            aec_update->cfg.aec_mode = cfg->val_int;

            /*使用新配置工具后，兼容app在线调试*/
            if (aec_update->cfg.aec_mode == 2) {
                aec_update->cfg.aec_mode &= ~AEC_MODE_ADVANCE;
                aec_update->cfg.aec_mode |= AEC_MODE_ADVANCE;
            } else if (aec_update->cfg.aec_mode == 1) {
                aec_update->cfg.aec_mode &= ~AEC_MODE_ADVANCE;
                aec_update->cfg.aec_mode |= AEC_MODE_REDUCE;
            } else {
                aec_update->cfg.aec_mode &= ~AEC_MODE_ADVANCE;
            }
            if (aec_update->agc_en) {
                aec_update->cfg.aec_mode |= AGC_EN;
            }
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
            break;
        case GENERAL_PC_ModuleEnable:
#if (TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE)
            AEC_ONLINE_LOG("DMS EnableBit:%x", cfg->val_int);
            aec_update->cfg.enable_module = cfg->val_int;
#else
            AEC_ONLINE_LOG("SMS EnableBit:%x", cfg->val_int);
            aec_update->cfg.aec_mode = cfg->val_int;
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
            break;
        case GENERAL_UL_EQ:
            AEC_ONLINE_LOG("UL_EQ_EN:%d\n", cfg->val_int);
            aec_update->cfg.ul_eq_en = cfg->val_int;
            break;
#if TCFG_AUDIO_DUAL_MIC_ENABLE
#if (TCFG_AUDIO_DMS_SEL == DMS_NORMAL)
        case GENERAL_Global_MinSuppress:
            AEC_ONLINE_LOG("GlobalMinSuppress:");
            AEC_ONLINE_FLOG(cfg->val_float);
            aec_update->cfg.global_minsuppress = cfg->val_float;
            break;
#endif/*TCFG_AUDIO_DMS_SEL*/
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
        }
    }
    aec_update->update = AEC_UPDATE_ONLINE;
    if (update && audio_aec_status()) {
        if (aec_update->timer) {
            sys_timer_modify(aec_update->timer, 500);
        } else {
            aec_update->timer = sys_timer_add(NULL, aec_update_timer_deal, 500);
        }
    }
    return 0;
}

int aec_cfg_online_init()
{
    int ret = 0;
    u8 mic_index = 0;
    aec_update = zalloc(sizeof(aec_update_t));
    //读工具中的配置
    ret = cvp_node_param_cfg_read(&aec_update->cfg, 1);
    cvp_param_cfg_read();
    mic_index = audio_get_mic_index(cvp_get_talk_mic_ch());
    aec_update->cfg.mic_again = audio_adc_file_get_gain(mic_index);

    aec_update->cfg.dac_again = (u8)esco_dec_dac_gain_get();//dac增益获取接口

#if TCFG_AUDIO_TRIPLE_MIC_ENABLE
    mic_index = audio_get_mic_index(cvp_get_talk_fb_mic_ch());
    aec_update->cfg.fb_mic_again = audio_adc_file_get_gain(mic_index);
    if (ret == sizeof(AEC_TMS_CONFIG)) {
        aec_update->update = AEC_UPDATE_INIT;
    }
#elif TCFG_AUDIO_DUAL_MIC_ENABLE
#if (TCFG_AUDIO_DMS_SEL == DMS_NORMAL)

    if (ret == sizeof(AEC_DMS_CONFIG)) {
        aec_update->update = AEC_UPDATE_INIT;
    }
#else/*TCFG_AUDIO_DMS_SEL == DMS_FLEXIBLE*/
    mic_index = audio_get_mic_index(cvp_get_talk_ref_mic_ch());
    aec_update->cfg.mic1_again = audio_adc_file_get_gain(mic_index);

    if (ret == sizeof(DMS_FLEXIBLE_CONFIG)) {
        aec_update->update = AEC_UPDATE_INIT;
    }
#endif/*TCFG_AUDIO_DMS_SEL*/
#else/*SINGLE MIC*/
    if (ret == sizeof(AEC_CONFIG)) {
        aec_update->update = AEC_UPDATE_INIT;
    }
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/
    return 0;
}

/*return 1：有在线更新数据*/
/*return 0：没有在线更新数据*/
int aec_cfg_online_update_fill(void *cfg, u16 len)
{
    if (aec_update && aec_update->update) {
        memcpy(cfg, &aec_update->cfg, len);
        return len;
    }
    return 0;
}

int aec_cfg_online_exit()
{
    if (aec_update) {
        free(aec_update);
        aec_update = NULL;
    }
    return 0;
}

int get_aec_config(u8 *buf, int version)
{
#if 1/*每次获取update的配置*/
    if (aec_update) {
        printf("cfg_size:%d\n", (u32)sizeof(aec_update->cfg));
        memcpy(buf, &aec_update->cfg, sizeof(aec_update->cfg));

#if ((TCFG_AUDIO_DUAL_MIC_ENABLE == 0) && (TCFG_AUDIO_TRIPLE_MIC_ENABLE == 0))
        AEC_CONFIG *cfg = (AEC_CONFIG *)buf;
        /*单麦aec_mode: app(旧)和pc端(兼容) */
        if (version == 0x01) {
            printf("APP version %d", version);
            if (aec_update->cfg.aec_mode & AGC_EN) {
                aec_update->agc_en = 1;
            } else {
                aec_update->agc_en = 0;
            }
            if ((aec_update->cfg.aec_mode & AEC_MODE_ADVANCE) == AEC_MODE_ADVANCE) {
                cfg->aec_mode = 2;
            } else if ((aec_update->cfg.aec_mode & AEC_MODE_ADVANCE) == AEC_MODE_REDUCE) {
                cfg->aec_mode = 1;
            } else {
                cfg->aec_mode = 0;
            }
        } else if (version == 0x02) {
            printf("PC version %d", version);
        }
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/

        return sizeof(aec_update->cfg);
    }
    return 0;
#endif
}

#endif /*TCFG_AEC_TOOL_ONLINE_ENABLE*/

/*
 ***********************************************************************
 *					导出通话过程的数据
 *
 *
 ***********************************************************************
 */
#if (TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SPP)
enum {
    AEC_RECORD_COUNT = 0x200,
    AEC_RECORD_START,
    AEC_RECORD_STOP,
    ONLINE_OP_QUERY_RECORD_PACKAGE_LENGTH,
};

enum {
    AEC_ST_INIT,
    AEC_ST_START,
    AEC_ST_STOP,
};

#define AEC_RECORD_CH		2
#define AEC_RECORD_MTU		200
#define RECORD_CH0_LENGTH		644
#define RECORD_CH1_LENGTH		644
#define RECORD_CH2_LENGTH		644

typedef struct {
    u8 state;
    u8 ch;	/*export data ch num*/
    u16 send_timer;
    u8 packet[256];
} aec_record_t;
aec_record_t *aec_rec = NULL;

typedef struct {
    int cmd;
    int data;
} rec_cmd_t;

extern int audio_capture_start(void);
extern void audio_capture_stop(void);
static int aec_online_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
    int res_data = 0;
    rec_cmd_t rec_cmd;
    int err = 0;
    u8 parse_seq = ext_data[1];
    //AEC_ONLINE_LOG("aec_spp_rx,seq:%d,size:%d\n", parse_seq, size);
    //put_buf(packet, size);
    memcpy(&rec_cmd, packet, sizeof(rec_cmd_t));
    switch (rec_cmd.cmd) {
    case AEC_RECORD_COUNT:
        res_data = aec_rec->ch;
        err = app_online_db_ack(parse_seq, (u8 *)&res_data, 4);
        AEC_ONLINE_LOG("query record_ch num:%d\n", res_data);
        break;
    case AEC_RECORD_START:
        err = app_online_db_ack(parse_seq, (u8 *)&res_data, 1); //该命令随便ack一个byte即可
        aec_rec->state = AEC_ST_START;
        audio_capture_start();
        AEC_ONLINE_LOG("record_start\n");
        break;
    case AEC_RECORD_STOP:
        AEC_ONLINE_LOG("record_stop\n");
        audio_capture_stop();
        aec_rec->state = AEC_ST_STOP;
        app_online_db_ack(parse_seq, (u8 *)&res_data, 1); //该命令随便ack一个byte即可
        break;
    case ONLINE_OP_QUERY_RECORD_PACKAGE_LENGTH:
        if (rec_cmd.data == 0) {
            res_data = RECORD_CH0_LENGTH;
        } else if (rec_cmd.data == 1) {
            res_data = RECORD_CH1_LENGTH;
        } else {
            res_data = RECORD_CH2_LENGTH;
        }
        AEC_ONLINE_LOG("query record ch%d packet length:%d\n", rec_cmd.data, res_data);
        err = app_online_db_ack(parse_seq, (u8 *)&res_data, 4); //回复对应的通道数据长度
        break;
    }
    return err;
}

static void aec_export_timer(void *priv)
{
    int err;
    if (aec_rec->state) {
        putchar('.');
#if 1
        static u8 data = 0;
        memset(aec_rec->packet, data, 128);
        data++;
#endif
        err = app_online_db_send(DB_PKT_TYPE_DAT_CH0, aec_rec->packet, AEC_RECORD_MTU);
        if (err) {
            printf("w0_err:%d", err);
        }
        err = app_online_db_send(DB_PKT_TYPE_DAT_CH1, aec_rec->packet, AEC_RECORD_MTU);
        if (err) {
            printf("w1_err:%d", err);
        }
    } else {
        //putchar('S');
    }
}

int spp_data_export(u8 ch, u8 *buf, u16 len)
{
    u8 data_ch;
    if (aec_rec->state == AEC_ST_START) {
        putchar('.');
        if (ch == 0) {
            data_ch = DB_PKT_TYPE_DAT_CH0;
        } else if (ch == 1) {
            data_ch = DB_PKT_TYPE_DAT_CH1;
        } else {
            data_ch = DB_PKT_TYPE_DAT_CH2;
        }
        int err = app_online_db_send_more(data_ch, buf, len);
        if (err) {
            r_printf("tx_err:%d", err);
            //return -1;
        }
        return len;
    } else {
        //putchar('x');
        return 0;
    }
}

int aec_data_export_init(u8 ch)
{
    aec_rec = zalloc(sizeof(aec_record_t));
    //aec_rec->send_timer = sys_timer_add(NULL, aec_export_timer, 16);
    aec_rec->ch = ch;
    app_online_db_register_handle(DB_PKT_TYPE_EXPORT, aec_online_parse);
    return 0;
}

int aec_data_export_exit()
{
    if (aec_rec) {
        if (aec_rec->send_timer) {
            sys_timer_del(aec_rec->send_timer);
            aec_rec->send_timer = 0;
        }
        free(aec_rec);
        aec_rec = NULL;
    }
    return 0;
}
#endif /*TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_SPP*/

#if 0
static u8 aec_online_idle_query(void)
{
    return ((aec_rec == NULL) ? 1 : 0);
}

REGISTER_LP_TARGET(aec_online_lp_target) = {
    .name = "aec_online",
    .is_idle = aec_online_idle_query,
};
#endif
