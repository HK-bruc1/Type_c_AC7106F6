#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".rt_anc.data.bss")
#pragma data_seg(".rt_anc.data")
#pragma const_seg(".rt_anc.text.const")
#pragma code_seg(".rt_anc.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"

#if TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
#include "audio_anc_includes.h"
#include "rt_anc.h"
#include "clock_manager/clock_manager.h"
#include "mic_effect.h"

#include "icsd_adt.h"

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif/*TCFG_USER_TWS_ENABLE*/

#if TCFG_SPEAKER_EQ_NODE_ENABLE
#include "effects/audio_spk_eq.h"
#endif

#if 1
#define rtanc_log	printf
#else
#define rtanc_log(...)
#endif/*log_en*/

#if 0
#define rtanc_debug_log	printf
#else
#define rtanc_debug_log(...)
#endif/*log_en*/


#define AUDIO_RT_ANC_SUSPEND_OBJECT_PRINTF			1		//RTANC 挂起对象打印

enum {
    RTANC_SZ_STA_INIT = BIT(0),		//SZ_SEL 初始化
    RTANC_SZ_STA_AFQ_OUT = BIT(1),	//SZ_SEL AFQ 输出SZ
    RTANC_SZ_STA_START = BIT(2),	//SZ_SEL RTANC 启动
};

enum {
    RTANC_SPP_CMD_SELF_STATE = 0,
    RTANC_SPP_CMD_SUSPEND_STATE,
    RTANC_SPP_CMD_RESUME_STATE,
};

//SE_SEL
const u8 sz_sel_h_freq_test[100] = {
    0x00, 0x1B, 0x37, 0x42, 0x00, 0x1B, 0xB7, 0x42, 0x40, 0x54, 0x09, 0x43, 0x00, 0x1B, 0x37, 0x43,
    0xC0, 0xE1, 0x64, 0x43, 0x40, 0x54, 0x89, 0x43, 0xA0, 0x37, 0xA0, 0x43, 0x00, 0x1B, 0xB7, 0x43,
    0x60, 0xFE, 0xCD, 0x43, 0xC0, 0xE1, 0xE4, 0x43, 0x20, 0xC5, 0xFB, 0x43, 0xF0, 0xC5, 0x14, 0x44,
    0x50, 0xA9, 0x2B, 0x44, 0xB0, 0x8C, 0x42, 0x44, 0x10, 0x70, 0x59, 0x44, 0x70, 0x53, 0x70, 0x44,
    0x68, 0x9B, 0x83, 0x44, 0x18, 0x0D, 0x8F, 0x44, 0xC8, 0x7E, 0x9A, 0x44, 0x28, 0x62, 0xB1, 0x44,
    0x88, 0x45, 0xC8, 0x44, 0xE8, 0x28, 0xDF, 0x44, 0x48, 0x0C, 0xF6, 0x44, 0x84, 0xE9, 0x11, 0x45,
    0xE4, 0xCC, 0x28, 0x45
};

#define AUDIO_RTANC_COEFF_SIZE				40 		//单个滤波器大小 5 * sizeof(double)

//SZ_SEL 触发计算
#define RTANC_SZ_STATE_TRIGGER		(RTANC_SZ_STA_INIT | RTANC_SZ_STA_AFQ_OUT | RTANC_SZ_STA_START)

//管理RTANC 滤波器输出:启动后RAM空间常驻
struct rt_anc_output {
    u8 ff_use_outfgq;
    float ff_gain;
    float fb_gain;
    float cmp_gain;
    float ff_fgq[31];
    double *ff_coeff;
    double *cmp_coeff;
};

//管理SZ_SEL 相关数据结构
struct audio_rt_anc_sz_sel {
    u8 state;
    u8 sel_idx;
    float *lff_out;			//[31] gain + 10 * order_size
    float *lcmp_out;	    //[31] gain + 10 * order_size
    __afq_output output;	//sz_output
};

struct rt_anc_channel {
    struct rt_anc_tmp_buffer *tmp;        //算法临时缓存
    struct rt_anc_output out;             //算法结果输出
    __rtanc_var_buffer var_cache_buff;    //缓存上传算法结果，便于重新传入
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    u8 *cmp_tool_data;					  //CMP工具数据缓存
#endif
};

struct audio_rt_anc_hdl {
    u8 seq;
    u8 state;
    u8 run_busy;
    u8 dut_mode;            //产测模式使能
    int rtanc_mode;
    int cmp_tool_data_len;
    audio_anc_t *param;
    float *debug_param;

    struct rt_anc_channel ch_l;
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    struct rt_anc_channel ch_r;
#endif

    struct rtanc_param *rtanc_p;
    struct icsd_rtanc_tool_data *rtanc_tool;	//工具临时数据
    struct rt_anc_fade_gain_ctr fade_ctr;
#if ANC_DUT_MIC_CMP_GAIN_ENABLE
    struct anc_mic_gain_cmp_cfg *mic_cmp;
#endif
    struct audio_rt_anc_sz_sel sz_sel;

    struct list_head suspend_head;
    OS_MUTEX mutex;
};

struct rtanc_suspend_bulk {
    const char *name;
    struct list_head entry;
};

static void audio_anc_real_time_adaptive_data_packet(struct icsd_rtanc_tool_data *rtanc_tool);
static void audio_rtanc_sz_pz_cmp_process(void);
static void audio_rtanc_suspend_list_query(u8 init_flag);
static void audio_rtanc_spp_send_data(u8 cmd, u8 *buf, int len);
static void audio_rtanc_cmp_tool_data_catch(u8 *data, int len, u8 ch);

static struct audio_rt_anc_hdl	*hdl = NULL;

anc_packet_data_t *rtanc_tool_data = NULL;

void audio_anc_real_time_adaptive_init(audio_anc_t *param)
{
    hdl = anc_malloc("ICSD_RTANC", sizeof(struct audio_rt_anc_hdl));
    hdl->param = param;
    hdl->fade_ctr.lff_gain = 16384;
    hdl->fade_ctr.lfb_gain = 16384;
    hdl->fade_ctr.rff_gain = 16384;
    hdl->fade_ctr.rfb_gain = 16384;

    hdl->rtanc_mode = ADT_REAL_TIME_ADAPTIVE_ANC_MODE;	//默认为正常模式

    os_mutex_create(&hdl->mutex);
    INIT_LIST_HEAD(&hdl->suspend_head);
}

/* Real Time ANC 自适应启动限制 */
int audio_rtanc_permit(u8 sync_mode)
{
    int ret = anc_ext_rtanc_param_check();
    if (ret) { //RTANC 工具参数缺失
        return ret;
    }
    if (hdl->param->lff_yorder > RT_ANC_MAX_ORDER || \
        hdl->param->lfb_yorder > RT_ANC_MAX_ORDER || \
        hdl->param->lcmp_yorder > RT_ANC_MAX_ORDER || \
        hdl->param->rff_yorder > RT_ANC_MAX_ORDER || \
        hdl->param->rfb_yorder > RT_ANC_MAX_ORDER || \
        hdl->param->rcmp_yorder > RT_ANC_MAX_ORDER) {
        return ANC_EXT_OPEN_FAIL_MAX_IIR_LIMIT;		//最大滤波器个数限制
    }
    return 0;
}

//将滤波器等相关信息，整合成库需要的格式
static void rt_anc_get_param(__rt_anc_param *rt_param_l, __rt_anc_param *rt_param_r)
{
    rtanc_debug_log("%s, %d\n", __func__, __LINE__);

    int yorder_size = 5 * sizeof(double);
    audio_anc_t *param = hdl->param;
    //ANC 0, 通透 1(通透只在DAC出声的时候更新)
    int anc_mode = (anc_mode_get() == ANC_ON) ? 0 : 1;

    if (rt_param_l) {
        if (anc_mode_get() == ANC_TRANSPARENCY) {
            rt_param_l->ff_use_outfgq = 0;
            // memcpy(rt_param_l->ff_fgq, hdl->ch_l.out.ff_fgq, 31*4);
            rt_param_l->ff_yorder = param->ltrans_yorder;
            rt_param_l->fb_yorder = param->ltrans_fb_yorder;
            rt_param_l->cmp_yorder = param->ltrans_cmp_yorder;
            rt_param_l->ffgain = param->gains.l_transgain;
            rt_param_l->fbgain = param->ltrans_fbgain;
            rt_param_l->cmpgain = param->ltrans_cmpgain;

            memcpy(rt_param_l->ff_coeff, param->ltrans_coeff, rt_param_l->ff_yorder * yorder_size);
            memcpy(rt_param_l->fb_coeff, param->ltrans_fb_coeff, rt_param_l->fb_yorder * yorder_size);
            memcpy(rt_param_l->cmp_coeff, param->ltrans_cmp_coeff, rt_param_l->cmp_yorder * yorder_size);
        } else {
            rt_param_l->ff_use_outfgq = hdl->ch_l.out.ff_use_outfgq;
            memcpy(rt_param_l->ff_fgq, hdl->ch_l.out.ff_fgq, 31 * 4);
            rt_param_l->ff_yorder = param->lff_yorder;
            rt_param_l->fb_yorder = param->lfb_yorder;
            rt_param_l->cmp_yorder = param->lcmp_yorder;
            rt_param_l->ffgain = param->gains.l_ffgain;
            rt_param_l->fbgain = param->gains.l_fbgain;
            rt_param_l->cmpgain = param->gains.l_cmpgain;

            memcpy(rt_param_l->ff_coeff, param->lff_coeff, rt_param_l->ff_yorder * yorder_size);
            memcpy(rt_param_l->fb_coeff, param->lfb_coeff, rt_param_l->fb_yorder * yorder_size);
            memcpy(rt_param_l->cmp_coeff, param->lcmp_coeff, rt_param_l->cmp_yorder * yorder_size);
        }
        rt_param_l->ff_fade_gain = ((float)hdl->fade_ctr.lff_gain) / 16384.0f;
        rt_param_l->fb_fade_gain = ((float)hdl->fade_ctr.lfb_gain) / 16384.0f;
        rt_param_l->anc_mode = anc_mode;
        rt_param_l->param = param;
        //printf(" ================== hdl->dut_mode %d\n",  hdl->dut_mode);
        rt_param_l->test_mode = hdl->dut_mode;

        rtanc_debug_log("rtanc l order:ff %d, fb %d, cmp %d\n", rt_param_l->ff_yorder, \
                        rt_param_l->fb_yorder, rt_param_l->cmp_yorder);
        rtanc_debug_log("rtanc l gain:ff %d/100, fb %d/100, cmp %d/100\n", (int)(rt_param_l->ffgain * 100.0f), \
                        (int)(rt_param_l->fbgain * 100.0f), (int)(rt_param_l->cmpgain * 100.0f));
        rtanc_debug_log("rtanc l fade_gain:ff %d/100, fb %d/100\n", (int)(rt_param_l->ff_fade_gain * 100.0f), \
                        (int)(rt_param_l->fb_fade_gain * 100.0f));
        /* put_buf((u8*)rt_param_l->ff_coeff,  rt_param_l->ff_yorder * yorder_size); */
        /* put_buf((u8*)rt_param_l->fb_coeff,  rt_param_l->fb_yorder * yorder_size); */
        /* put_buf((u8*)rt_param_l->cmp_coeff, rt_param_l->cmp_yorder * yorder_size); */
    }

#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    if (rt_param_r) { // 右声道完善逻辑
        if (anc_mode_get() == ANC_TRANSPARENCY) {
            rt_param_r->ff_use_outfgq = 0; // 与左声道透传模式保持一致（禁用outfgq）
            // memcpy(rt_param_r->ff_fgq, hdl->ch_r.out.ff_fgq, 31*4);

            rt_param_r->ff_yorder = param->rtrans_yorder;
            rt_param_r->fb_yorder = param->rtrans_fb_yorder;
            rt_param_r->cmp_yorder = param->rtrans_cmp_yorder;
            rt_param_r->ffgain = param->gains.r_transgain;
            rt_param_r->fbgain = param->rtrans_fbgain;
            rt_param_r->cmpgain = param->rtrans_cmpgain;

            memcpy(rt_param_r->ff_coeff, param->rtrans_coeff, rt_param_r->ff_yorder * yorder_size);
            memcpy(rt_param_r->fb_coeff, param->rtrans_fb_coeff, rt_param_r->fb_yorder * yorder_size);
            memcpy(rt_param_r->cmp_coeff, param->rtrans_cmp_coeff, rt_param_r->cmp_yorder * yorder_size);
        } else {
            rt_param_r->ff_use_outfgq = hdl->ch_r.out.ff_use_outfgq; // 复用左声道的outfgq开关（与左声道保持一致）
            memcpy(rt_param_r->ff_fgq, hdl->ch_r.out.ff_fgq, 31 * 4); // 复制右声道专属fgq数据

            rt_param_r->ff_yorder = param->rff_yorder;
            rt_param_r->fb_yorder = param->rfb_yorder;
            rt_param_r->cmp_yorder = param->rcmp_yorder;
            rt_param_r->ffgain = param->gains.r_ffgain;
            rt_param_r->fbgain = param->gains.r_fbgain;
            rt_param_r->cmpgain = param->gains.r_cmpgain;

            memcpy(rt_param_r->ff_coeff, param->rff_coeff, rt_param_r->ff_yorder * yorder_size);
            memcpy(rt_param_r->fb_coeff, param->rfb_coeff, rt_param_r->fb_yorder * yorder_size);
            memcpy(rt_param_r->cmp_coeff, param->rcmp_coeff, rt_param_r->cmp_yorder * yorder_size);
        }

        rt_param_r->ff_fade_gain = ((float)hdl->fade_ctr.rff_gain) / 16384.0f; // 右声道fade增益
        rt_param_r->fb_fade_gain = ((float)hdl->fade_ctr.rfb_gain) / 16384.0f;
        rt_param_r->anc_mode = anc_mode; // 降噪模式（与左声道一致）
        rt_param_r->test_mode = hdl->dut_mode; // 测试模式（与左声道一致）
        rt_param_r->param = param; // 绑定参数指针

        rtanc_debug_log("rtanc r order:ff %d, fb %d, cmp %d\n", rt_param_r->ff_yorder, \
                        rt_param_r->fb_yorder, rt_param_r->cmp_yorder);
        rtanc_debug_log("rtanc r gain:ff %d/100, fb %d/100, cmp %d/100\n", (int)(rt_param_r->ffgain * 100.0f), \
                        (int)(rt_param_r->fbgain * 100.0f), (int)(rt_param_r->cmpgain * 100.0f));
        rtanc_debug_log("rtanc r fade_gain:ff %d/100, fb %d/100\n", (int)(rt_param_r->ff_fade_gain * 100.0f), \
                        (int)(rt_param_r->fb_fade_gain * 100.0f));
        /* 可选：右声道系数调试输出（与左声道保持一致的注释风格） */
        /* put_buf((u8*)rt_param_r->ff_coeff,  rt_param_r->ff_yorder * yorder_size); */
        /* put_buf((u8*)rt_param_r->fb_coeff,  rt_param_r->fb_yorder * yorder_size); */
        /* put_buf((u8*)rt_param_r->cmp_coeff, rt_param_r->cmp_yorder * yorder_size); */
    }
#endif

}

int audio_rtanc_var_cache_set(u8 flag)
{
    if (hdl) {
        rtanc_log("audio_rtanc:var_cache_flag %d\n", flag);
        hdl->ch_l.var_cache_buff.flag = flag;
        if (!flag) {
            hdl->ch_l.out.ff_use_outfgq = 0;
        }
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        hdl->ch_r.var_cache_buff.flag = flag;
        if (!flag) {
            hdl->ch_r.out.ff_use_outfgq = 0;
        }
#endif
    }
    return 0;
}

#if 0
struct __anc_ext_dynamic_cfg *dynamic_buffer;
struct __anc_ext_dynamic_cfg *anc_ext_dynamic_def_cfg_get()
{
    if (dynamic_buffer == NULL) {
        dynamic_buffer = (struct __anc_ext_dynamic_cfg *)zalloc(sizeof(struct __anc_ext_dynamic_cfg));
    }
    struct __anc_ext_dynamic_cfg *cfg = dynamic_buffer;

    cfg->sdcc_det_en = 1;
    cfg->sdcc_par1 = 0;
    cfg->sdcc_par2 = 4;
    cfg->sdcc_flag_thr = 1;
    cfg->sdrc_det_en = 1;
    cfg->sdrc_flag_thr = 1;
    cfg->sdrc_cnt_thr = 8;
    cfg->sdrc_ls_rls_lidx = 4;
    cfg->sdrc_ls_rls_hidx = 9;

    cfg->ref_iir_coef = 0.0078;
    cfg->err_iir_coef = 0.0078;
    cfg->sdcc_ref_thr = -24;
    cfg->sdcc_err_thr = -12;
    cfg->sdcc_thr_cmp = -24;
    cfg->sdrc_ref_thr = -24;
    cfg->sdrc_err_thr = -12;
    cfg->sdrc_ref_margin = 30;
    cfg->sdrc_err_margin = 35;
    cfg->sdrc_fb_att_thr = 0;
    cfg->sdrc_fb_rls_thr = -35;
    cfg->sdrc_fb_att_step = 2;
    cfg->sdrc_fb_rls_step = 2;
    cfg->sdrc_fb_set_thr = 3;
    cfg->sdrc_ff_att_thr = 0;
    cfg->sdrc_ff_rls_thr = -24;
    cfg->sdrc_ls_min_gain = 0;
    cfg->sdrc_ls_att_step = 6;
    cfg->sdrc_ls_rls_step = 2;
    cfg->sdrc_rls_ls_cmp = 15;
    return dynamic_buffer;
}

struct __anc_ext_rtanc_adaptive_cfg *rtanc_adpt_buffer;
struct __anc_ext_rtanc_adaptive_cfg *anc_ext_rtanc_adaptive_def_cfg_get()
{
    if (rtanc_adpt_buffer == NULL) {
        rtanc_adpt_buffer = (struct __anc_ext_rtanc_adaptive_cfg *)zalloc(sizeof(struct __anc_ext_rtanc_adaptive_cfg));
    }
    struct __anc_ext_rtanc_adaptive_cfg *cfg = rtanc_adpt_buffer;

    cfg->wind_det_en = 1;
    cfg->wind_lvl_thr = 30;
    cfg->wind_lock_cnt = 4;
    cfg->idx_use_same = 1;
    cfg->msc_dem_thr0 = 8;
    cfg->msc_dem_thr1 = 10;
    cfg->msc_mat_lidx = 5;
    cfg->msc_mat_hidx = 12;
    cfg->msc_idx_thr = 6;
    cfg->msc_scnt_thr = 2;
    cfg->msc2norm_updat_cnt = 5;
    cfg->msc_lock_cnt = 1;
    cfg->msc_tg_diff_lidx = 5;
    cfg->msc_tg_diff_hidx = 12;
    cfg->cmp_diff_lidx = 5;
    cfg->cmp_diff_hidx = 12;
    cfg->cmp_idx_thr = 4;
    cfg->cmp_adpt_en = 0;
    cfg->spl2norm_cnt = 5;
    cfg->talk_lock_cnt = 3;
    cfg->noise_idx_thr = 6;
    cfg->noise_updat_thr = 20;
    cfg->noise_ffdb_thr = 20;
    cfg->mse_lidx = 5;
    cfg->mse_hidx = 12;
    cfg->fast_cnt = 2;
    cfg->fast_ind_thr = 20;
    cfg->fast_ind_div = 2;
    cfg->norm_mat_lidx = 5;
    cfg->norm_mat_hidx = 12;
    cfg->tg_lidx = 5;
    cfg->tg_hidx = 12;
    cfg->rewear_idx_thr = 4;
    cfg->norm_updat_cnt0 = 10;
    cfg->norm_updat_cnt1 = 0;
    cfg->norm_cmp_cnt0 = 8;
    cfg->norm_cmp_cnt1 = 3;
    cfg->norm_cmp_lidx = 5;
    cfg->norm_cmp_hidx = 12;
    cfg->updat_lock_cnt = 2;
    cfg->ls_fix_mode = 1;
    cfg->ls_fix_lidx = 5;
    cfg->ls_fix_hidx = 10;
    cfg->iter_max0 = 20;
    cfg->iter_max1 = 20;
    cfg->m1_dem_cnt0 = 5;
    cfg->m1_dem_cnt1 = 4;
    cfg->m1_dem_cnt2 = 10;
    cfg->m1_scnt0 = 3;
    cfg->m1_scnt1 = 3;
    cfg->m1_dem_off_cnt0 = 10;
    cfg->m1_dem_off_cnt1 = 10;
    cfg->m1_dem_off_cnt2 = 10;
    cfg->m1_scnt0_off = 5;
    cfg->m1_scnt1_off = 5;
    cfg->m1_scnt2_off = 10;
    cfg->m1_idx_thr = 7;
    cfg->msc_iir_idx_thr = 5;
    cfg->msc_atg_sm_lidx = 9;
    cfg->msc_atg_sm_hidx = 24;
    cfg->msc_atg_diff_lidx = 10;
    cfg->msc_atg_diff_hidx = 25;
    cfg->msc_spl_idx_cut = 20;
    cfg->tight_divide = 0;
    cfg->tight_idx_thr = 40;
    cfg->tight_idx_diff = 4;

    cfg->wind_ref_thr = 100;
    cfg->wind_ref_max = 110;
    cfg->wind_ref_min = 12;
    cfg->wind_miu_div = 100;
    cfg->msc_err_thr0 = 4;
    cfg->msc_err_thr1 = 2;
    cfg->msc_tg_thr = 3;
    cfg->cmp_updat_thr = 1.5;
    cfg->cmp_updat_fast_thr = 2.5;
    cfg->cmp_mul_factor = 2;
    cfg->cmp_div_factor = 60;
    cfg->low_spl_thr = 0;
    cfg->splcnt_add_thr = 0;
    cfg->noise_mse_thr = 8;
    cfg->lms_err = 59;
    cfg->mse_thr1 = 5;
    cfg->mse_thr2 = 8;
    cfg->uscale = 0.8;
    cfg->uoffset = 0.1;
    cfg->u_pre_thr = 0.5;
    cfg->u_cur_thr = 0.15;
    cfg->u_first_thr = 0.4;
    cfg->norm_tg_thr = 1.5;
    cfg->norm_cmp_thr = 2;
    cfg->ls_fix_range0[0] = -10;
    cfg->ls_fix_range0[1] = 10;
    cfg->ls_fix_range1[0] = 0;
    cfg->ls_fix_range1[1] = 16;
    cfg->ls_fix_range2[0] = 12;
    cfg->ls_fix_range2[1] = 40;
    cfg->msc_tar_coef0 = 0.8;
    cfg->msc_ind_coef0 = 0.6;
    cfg->msc_tar_coef1 = 0.2;
    cfg->msc_ind_coef1 = 0.3;
    cfg->msc_atg_diff_thr0 = 4.0;
    cfg->msc_atg_diff_thr1 = 1.0;
    cfg->msc_tg_spl_thr = 4;
    cfg->msc_sz_sm_thr = 4;
    cfg->msc_atg_sm_thr = 3;

    return rtanc_adpt_buffer;
}
#endif

//设置RTANC初始参数
int audio_adt_rtanc_set_infmt(void *rtanc_tool)
{

    struct __anc_ext_rtanc_adaptive_cfg *rtanc_tool_cfg = NULL;
    struct __anc_ext_dynamic_cfg *dynamic_cfg = NULL;
    struct __anc_ext_rtanc_adaptive_cfg *rtanc_tool_cfg_r = NULL;
    struct __anc_ext_dynamic_cfg *dynamic_cfg_r = NULL;

    rtanc_debug_log("=================audio_adt_rtanc_set_infmt==================\n");
    if (++hdl->seq == 0xff) {
        hdl->seq = 0;
    }
    struct rt_anc_infmt infmt;
    if (anc_ext_tool_online_get()) {
        hdl->rtanc_tool = rtanc_tool;
    } else {
        hdl->rtanc_tool = NULL;
    }
    //查询是否有人挂起RTANC
    audio_rtanc_suspend_list_query(1);

#if AUDIO_RT_ANC_SZ_PZ_CMP_EN
    audio_rtanc_sz_pz_cmp_process();
#endif

    infmt.var_buf = &hdl->ch_l.var_cache_buff;
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    infmt.var_buf_r = &hdl->ch_r.var_cache_buff;
#endif
    if (hdl->sz_sel.state & RTANC_SZ_STA_INIT) {
        hdl->sz_sel.state |= RTANC_SZ_STA_START;
        rtanc_log("RTANC_STATE_SZ:START\n");
        if (hdl->sz_sel.state == RTANC_SZ_STATE_TRIGGER) {
            os_taskq_post_msg("anc", 1, ANC_MSG_RTANC_SZ_OUTPUT);
            infmt.var_buf->cmp_ind_last = hdl->sz_sel.sel_idx - 1;
        }
    }
    infmt.id = hdl->seq;
    infmt.param = hdl->param;
    infmt.ext_cfg = (void *)anc_ext_ear_adaptive_cfg_get();
    infmt.anc_param_l = anc_malloc("ICSD_RTANC", sizeof(__rt_anc_param));
    //gali 可优化
    infmt.anc_param_r = anc_malloc("ICSD_RTANC", sizeof(__rt_anc_param));

    //rtanc_log("rtanc_init flag %d, ind_last %d, ff_use_outfgq %d\n", hdl->ch_l.var_cache_buff.flag, hdl->ch_l.var_cache_buff.ind_last, hdl->ch_l.out.ff_use_outfgq);
    rtanc_log("L:rtanc_init flag %d, %d, %d, %d, %d, %d\n", infmt.var_buf->flag, infmt.var_buf->ind_last, infmt.var_buf->ls_gain_flag, \
              infmt.var_buf->sdrc_flag_fix_ls, infmt.var_buf->sdrc_flag_rls_ls, infmt.var_buf->cmp_ind_last);
    rtanc_log("L:part1 %d, %d, fb_gain %d\n", (int)(infmt.var_buf->part1_ref_lf_dB * 1000.0f), (int)(infmt.var_buf->part1_err_lf_dB * 1000.0f), \
              (int)(infmt.var_buf->fb_aim_gain * 1000.0f));
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    rtanc_log("R:rtanc_init flag %d, %d, %d, %d, %d, %d\n", infmt.var_buf_r->flag, infmt.var_buf_r->ind_last, infmt.var_buf_r->ls_gain_flag, \
              infmt.var_buf_r->sdrc_flag_fix_ls, infmt.var_buf_r->sdrc_flag_rls_ls, infmt.var_buf_r->cmp_ind_last);
    rtanc_log("R:part1 %d, %d, fb_gain %d\n", (int)(infmt.var_buf_r->part1_ref_lf_dB * 1000.0f), (int)(infmt.var_buf_r->part1_err_lf_dB * 1000.0f), \
              (int)(infmt.var_buf_r->fb_aim_gain * 1000.0f));
#endif
    rt_anc_get_param(infmt.anc_param_l, infmt.anc_param_r);

    rtanc_tool_cfg = anc_ext_rtanc_adaptive_cfg_get(0);
    dynamic_cfg = anc_ext_dynamic_cfg_get(0);
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    rtanc_tool_cfg_r = anc_ext_rtanc_adaptive_cfg_get(1);
    dynamic_cfg_r = anc_ext_dynamic_cfg_get(1);
#endif

    rt_anc_set_init(&infmt, rtanc_tool_cfg, rtanc_tool_cfg_r, dynamic_cfg, dynamic_cfg_r);

    if (infmt.anc_param_l) {
        anc_free(infmt.anc_param_l);
    }
    if (infmt.anc_param_r) {
        anc_free(infmt.anc_param_r);
    }
    return 0;
}

static void audio_rt_anc_param_updata(__rt_anc_param *l_param, __rt_anc_param *r_param)
{
    if (anc_mode_get() != ANC_ON) {
        return;
    }
    rtanc_debug_log("audio_rt_anc_param_updata\n");

    audio_anc_t *param = hdl->param;
    u8 ff_updat = 0;
    u8 fb_updat = 0;
    u8 cmp_eq_updat = 0;

    //sz_sel 模式已经存有FF滤波器, 不需要拷贝
    if (hdl->sz_sel.state != RTANC_SZ_STATE_TRIGGER) {
        if (hdl->ch_l.out.ff_coeff) {
            anc_free(hdl->ch_l.out.ff_coeff);
        }
        hdl->ch_l.out.ff_coeff = anc_malloc("ICSD_RTANC", param->lff_yorder * AUDIO_RTANC_COEFF_SIZE);
        hdl->ch_l.out.ff_use_outfgq = 1;
        memcpy(hdl->ch_l.out.ff_fgq, l_param->ff_fgq, 31 * 4);
        memcpy(hdl->ch_l.out.ff_coeff, l_param->ff_coeff, param->lff_yorder * AUDIO_RTANC_COEFF_SIZE);
    }
    //put_buf((u8 *)hdl->ch_l.out.ff_coeff, param->lff_yorder * AUDIO_RTANC_COEFF_SIZE);
    hdl->ch_l.out.ff_gain = l_param->ffgain;
    hdl->ch_l.out.fb_gain = l_param->fbgain;
    param->gains.l_ffgain = l_param->ffgain;
    param->lff_coeff = hdl->ch_l.out.ff_coeff;
    param->gains.l_fbgain = l_param->fbgain;
    ff_updat |= l_param->ff_updat;
    fb_updat |= l_param->fb_updat;
    if (l_param->ff_updat) {
        l_param->ff_updat = 0;
    }
    if (l_param->fb_updat) {
        l_param->fb_updat = 0;
    }
    cmp_eq_updat |= l_param->cmp_eq_updat;

    //param->lfb_coeff = &l_param->lfb_coeff[0];
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    if (r_param) {
        if (hdl->sz_sel.state != RTANC_SZ_STATE_TRIGGER) {
            if (hdl->ch_r.out.ff_coeff) {
                anc_free(hdl->ch_r.out.ff_coeff);
            }
            hdl->ch_r.out.ff_coeff = anc_malloc("ICSD_RTANC", param->rff_yorder * AUDIO_RTANC_COEFF_SIZE);
            hdl->ch_r.out.ff_use_outfgq = 1; // 与左声道一致，启用outfgq
            memcpy(hdl->ch_r.out.ff_fgq, r_param->ff_fgq, 31 * 4);
            memcpy(hdl->ch_r.out.ff_coeff, r_param->ff_coeff, param->rff_yorder * AUDIO_RTANC_COEFF_SIZE);
        }
        // put_buf((u8 *)hdl->ch_r.out.ff_coeff, param->rff_yorder * AUDIO_RTANC_COEFF_SIZE);
        hdl->ch_r.out.ff_gain = r_param->ffgain;  // 补充右声道FF增益到输出结构体
        hdl->ch_r.out.fb_gain = r_param->fbgain;  // 补充右声道FB增益到输出结构体
        param->gains.r_ffgain = r_param->ffgain;
        param->rff_coeff = hdl->ch_r.out.ff_coeff;
        param->gains.r_fbgain = r_param->fbgain;
        ff_updat |= r_param->ff_updat;
        fb_updat |= r_param->fb_updat;
        cmp_eq_updat |= r_param->cmp_eq_updat;
        if (r_param->ff_updat) {
            r_param->ff_updat = 0;
        }
        if (r_param->fb_updat) {
            r_param->fb_updat = 0;
        }
    }
#endif

    //工具没有开CMP则不输出
    if (!anc_ext_adaptive_cmp_tool_en_get()) {
        cmp_eq_updat &= ~AUDIO_ADAPTIVE_CMP_UPDATE_FLAG;
    }
    if (ff_updat) {
        param->ff_filter_fade_slow = l_param->ff_fade_slow;
        audio_anc_coeff_check_crc(ANC_CHECK_RTANC_UPDATE);
        anc_coeff_ff_online_update(hdl->param);		//更新ANC FF滤波器
    }
    //有CMP更新时，统一在CMP输出后更新FB
    if (fb_updat && (!(cmp_eq_updat & AUDIO_ADAPTIVE_CMP_UPDATE_FLAG))) {
        audio_anc_coeff_check_crc(ANC_CHECK_RTANC_UPDATE);
        anc_coeff_fb_online_update(hdl->param);		//更新ANC FB滤波器
    }
}

void audio_rtanc_cmp_update(void)
{
    audio_anc_t *param = hdl->param;

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    struct anc_cmp_param_output cmp_p;

    audio_rtanc_adaptive_cmp_update_flag_get(&cmp_p);

    if (cmp_p.l_update_flag) {
        if (hdl->ch_l.out.cmp_coeff) {
            anc_free(hdl->ch_l.out.cmp_coeff);
        }
        hdl->ch_l.out.cmp_coeff = anc_malloc("ICSD_RTANC", ANC_ADAPTIVE_CMP_ORDER * AUDIO_RTANC_COEFF_SIZE);
        cmp_p.l_coeff = hdl->ch_l.out.cmp_coeff;
    }

#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    if (cmp_p.r_update_flag) {
        if (hdl->ch_r.out.cmp_coeff) {
            anc_free(hdl->ch_r.out.cmp_coeff);
        }
        hdl->ch_r.out.cmp_coeff = anc_malloc("ICSD_RTANC", ANC_ADAPTIVE_CMP_ORDER * AUDIO_RTANC_COEFF_SIZE);
        cmp_p.r_coeff = hdl->ch_r.out.cmp_coeff;
    }
#endif

    audio_rtanc_adaptive_cmp_output_get(&cmp_p);

    rtanc_debug_log("cmp_p.l_update_flag %d , cmp_p.r_update_flag %d \n", cmp_p.l_update_flag, cmp_p.r_update_flag);
    if (cmp_p.l_update_flag) {
        hdl->ch_l.out.cmp_gain = cmp_p.l_gain;
        if (anc_mode_get() == ANC_TRANSPARENCY) {
            u8 sign = param->gains.gain_sign;
            param->ltrans_cmpgain = (sign & ANCL_CMP_SIGN) ? (0 - cmp_p.l_gain) : cmp_p.l_gain;
            param->ltrans_cmp_coeff = cmp_p.l_coeff;
        } else {
            param->gains.l_cmpgain = cmp_p.l_gain;
            param->lcmp_coeff = cmp_p.l_coeff;
        }
    }

#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    if (cmp_p.r_update_flag) {
        hdl->ch_r.out.cmp_gain = cmp_p.r_gain;
        if (anc_mode_get() == ANC_TRANSPARENCY) {
            u8 sign = param->gains.gain_sign; // 复用左声道的增益符号参数（通常左右声道符号逻辑一致）
            param->rtrans_cmpgain = (sign & ANCR_CMP_SIGN) ? (0 - cmp_p.r_gain) : cmp_p.r_gain;
            param->rtrans_cmp_coeff = cmp_p.r_coeff;
        } else {
            param->gains.r_cmpgain = cmp_p.r_gain;
            param->rcmp_coeff = cmp_p.r_coeff;
        }
    }
#endif
    //rtanc_debug_log("cmp updata gain = %d, coef = %d, ", (int)(cmp_p.l_gain * 100), (int)(cmp_p.l_coeff[0] * 100));
    rtanc_log("ANC_CHECK:CMP_UPDATE, crc %x-%d/100\n", CRC16(cmp_p.l_coeff, ANC_ADAPTIVE_CMP_ORDER * AUDIO_RTANC_COEFF_SIZE), (int)(cmp_p.l_gain * 100));
#endif
    if (anc_mode_get() != ANC_OFF) {
        audio_anc_coeff_fb_smooth_update();
    }
}

//RTANC 输出SZ 预处理 0:L  1:R
static void audio_rtanc_output_sz_ch_process(__rt_anc_param *rt_param, __afq_data **sz_p, u8 ch)
{
    __afq_data *sz = NULL;
    if (!rt_param) {
        return;
    }
    rtanc_log("%s, ch:%d, update_flag %x\n", __func__, ch, rt_param->cmp_eq_updat);
    if (rt_param->cmp_eq_updat) {
        if (hdl->sz_sel.state != RTANC_SZ_STATE_TRIGGER) {
            sz = anc_malloc("ICSD_RTANC", sizeof(__afq_data));
            sz->len = 120;
            sz->out = anc_malloc("ICSD_RTANC", sizeof(float) * 120);
            icsd_rtanc_alg_get_sz(sz->out, ch); //0:L  1:R
            if (rt_param->cmp_eq_szidx != 100) {
                //printf("sz idx set %d\n", rt_param->cmp_eq_szidx);
                audio_afq_common_sz_sel_idx_set(rt_param->cmp_eq_szidx + 1, ch);
            } else {
                //printf("sz idx set 0\n");
                audio_afq_common_sz_sel_idx_set(0, ch);
            }
        }
        if (!CRC16(sz->out, 120 * 4)) {
            rt_param->cmp_eq_updat = 0;
            printf("sz_output err\n");
        }
    }
    audio_afq_common_cmp_eq_update_set(rt_param->cmp_eq_updat, ch);

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN && AUDIO_RT_ANC_EXPORT_TOOL_DATA_DEBUG
    //当RTANC没有输出CMP时，使用上一次的CMP结果输出给工具
    if (!(rt_param->cmp_eq_updat & AUDIO_ADAPTIVE_CMP_UPDATE_FLAG)) {
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        u8 *tool_data = ch ? hdl->ch_r.cmp_tool_data : hdl->ch_l.cmp_tool_data;
        audio_rtanc_cmp_tool_data_catch(tool_data, hdl->cmp_tool_data_len, ch);
#else
        audio_rtanc_cmp_tool_data_catch(hdl->ch_l.cmp_tool_data, hdl->cmp_tool_data_len, ch);
#endif
    }
#endif


    if (sz) {
        *sz_p = sz;
    }
    //RTANC LIB 内部busy清0
    rt_param->updat_busy = 0;
    rt_param->cmp_eq_updat = 0;
}

//实时自适应算法输出接口
void audio_adt_rtanc_output_handle(void *rt_param_l, void *rt_param_r)
{
    //RTANC de 算法强制退出时，将不再输出output
    //gali 需考虑打断的逻辑
    if ((!icsd_adt_is_running()) && (hdl->sz_sel.state != RTANC_SZ_STATE_TRIGGER)) {
        return;
    }
    u8 cmp_eq_update = 0;
    __rt_anc_param *l_param = (__rt_anc_param *)rt_param_l;
    __rt_anc_param *r_param = (__rt_anc_param *)rt_param_r;
    rtanc_log("RTANC_STATE:OUTPUT_RUN\n");
    hdl->run_busy = 1;
    //挂起RT_ANC
    audio_anc_real_time_adaptive_suspend("RTANC_OUTPUT");

    //gali 更新滤波器 L+R
    if (hdl->sz_sel.state != RTANC_SZ_STATE_TRIGGER || RTANC_SZ_SEL_FF_OUTPUT) {
        audio_rt_anc_param_updata(l_param, r_param);
    }

    //整理SZ数据结构
    __afq_output output = {0};// = anc_malloc("ICSD_RTANC", sizeof(__afq_output));// = &temp;

    if (hdl->sz_sel.state == RTANC_SZ_STATE_TRIGGER) {
        //sz_sel 模式已经存有SZ
        printf("sz sel mode exist sz\n");
        output = hdl->sz_sel.output;
        audio_afq_common_sz_sel_idx_set(hdl->sz_sel.sel_idx, 0);
        //g_printf("rtanc_output sz\n");
        //put_buf((u8 *)output.sz_l->out, 120 * 4);
    }

#if AUDIO_RT_ANC_EXPORT_TOOL_DATA_DEBUG
    audio_anc_real_time_adaptive_data_packet(hdl->rtanc_tool);
#endif
    cmp_eq_update |= l_param->cmp_eq_updat;
    audio_rtanc_output_sz_ch_process(l_param, &output.sz_l, 0);
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    cmp_eq_update |= r_param->cmp_eq_updat;
    audio_rtanc_output_sz_ch_process(r_param, &output.sz_r, 1);
#endif

    //AFQ触发
    if (cmp_eq_update) {
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
        if (anc_ext_adaptive_cmp_tool_en_get()) {
            audio_anc_real_time_adaptive_suspend("RTCMP");
        }
#endif

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
        //如果AEQ在运行就挂起RTANC
        if (audio_adaptive_eq_state_get() && anc_ext_adaptive_eq_tool_en_get()) {
            audio_anc_real_time_adaptive_suspend("AEQ");
        }
#endif
        //执行挂在到AFQ 的算法，如AEQ, 内部执行完会恢复RTANC
        if (hdl->sz_sel.state == RTANC_SZ_STATE_TRIGGER) {
            audio_afq_common_output_post_msg(&output, AUDIO_AFQ_PRIORITY_HIGH);
        } else {
            audio_afq_common_output_post_msg(&output, AUDIO_AFQ_PRIORITY_NORMAL);
        }
    }

    if (hdl->sz_sel.state != RTANC_SZ_STATE_TRIGGER) {
        if (output.sz_l) {
            anc_free(output.sz_l->out);
            anc_free(output.sz_l);
        }
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        if (output.sz_r) {
            anc_free(output.sz_r->out);
            anc_free(output.sz_r);
        }
#endif
    }

    //恢复RT_ANC
    audio_anc_real_time_adaptive_resume("RTANC_OUTPUT");


    rtanc_log("RTANC_STATE:OUTPUT_RUN END\n");
    hdl->run_busy = 0;
}

static void audio_rtanc_suspend_list_query(u8 init_flag)
{
    //ADT切模式过程并且非初始化时，禁止切换
    if (get_icsd_adt_mode_switch_busy() && !init_flag) {
        if (hdl->state != RT_ANC_STATE_SUSPEND) {	//如挂起RTANC，则表示RTANC活动中，允许释放
            return;
        }
    }
    if (!audio_anc_real_time_adaptive_state_get()) {
        return;
    }

    os_mutex_pend(&hdl->mutex, 0);
    if (!list_empty(&hdl->suspend_head)) {
#if AUDIO_RT_ANC_SUSPEND_OBJECT_PRINTF
        if (init_flag) {
            rtanc_log("RTANC suspend object = {\n");
            struct rtanc_suspend_bulk *bulk;
            list_for_each_entry(bulk, &hdl->suspend_head, entry) {
                rtanc_log("    %s\n", bulk->name);
                audio_rtanc_spp_send_data(RTANC_SPP_CMD_SUSPEND_STATE, (u8 *)(bulk->name), strlen(bulk->name) + 1);
            }
            rtanc_log("}\n");
        }
#endif
        if (hdl->state == RT_ANC_STATE_OPEN) {
            rtanc_log("RTANC_STATE:SUSPEND\n");
            hdl->state = RT_ANC_STATE_SUSPEND;
            anc_ext_algorithm_state_update(ANC_EXT_ALGO_RTANC, ANC_EXT_ALGO_STA_SUSPEND, 0);
            icsd_adt_rtanc_suspend();
        }
    } else {
        if (hdl->state == RT_ANC_STATE_SUSPEND) {
            rtanc_log("RTANC_STATE:SUSPEND->OPEN\n");
            audio_rtanc_spp_send_data(RTANC_SPP_CMD_RESUME_STATE, NULL, 0);
            anc_ext_algorithm_state_update(ANC_EXT_ALGO_RTANC, ANC_EXT_ALGO_STA_OPEN, 0);
            hdl->state = RT_ANC_STATE_OPEN;
            icsd_adt_rtanc_resume();
        }
    }
    os_mutex_post(&hdl->mutex);
}

//RT ANC挂起接口, suspend/resume 的name 必须一致
void audio_anc_real_time_adaptive_suspend(const char *name)
{
    struct rtanc_suspend_bulk *bulk, *new_bulk;
    if (!name) {
        return;
    }

    if (cpu_in_irq()) {
        int msg[3] = {(int) audio_anc_real_time_adaptive_suspend, 1, (int)name};
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
        return;
    }

    /* rtanc_log("%s, %s\n", __func__, name); */
    audio_rtanc_spp_send_data(RTANC_SPP_CMD_SUSPEND_STATE, (u8 *)name, strlen(name) + 1);
    os_mutex_pend(&hdl->mutex, 0);

    if (!list_empty(&hdl->suspend_head)) {
        list_for_each_entry(bulk, &hdl->suspend_head, entry) {
            if (!strcmp(name, bulk->name)) {
                //rtanc_log("same name %s\n", name);
                os_mutex_post(&hdl->mutex);
                return;	//同名称则返回
            }
        }
    }
    new_bulk = anc_malloc("ICSD_RTANC", sizeof(struct rtanc_suspend_bulk));
    new_bulk->name = name;

    list_add(&new_bulk->entry, &hdl->suspend_head);
    os_mutex_post(&hdl->mutex);
    audio_rtanc_suspend_list_query(0);
}

//RT ANC恢复接口
void audio_anc_real_time_adaptive_resume(const char *name)
{
    struct rtanc_suspend_bulk *bulk;
    struct rtanc_suspend_bulk *temp;
    if (!name) {
        return;
    }

    if (cpu_in_irq()) {
        int msg[3] = {(int) audio_anc_real_time_adaptive_resume, 1, (int)name};
        int ret = os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
        return;
    }

    /* rtanc_log("%s, %s\n", __func__, name); */
    os_mutex_pend(&hdl->mutex, 0);

    if (list_empty(&hdl->suspend_head)) {
        os_mutex_post(&hdl->mutex);
        return;
    }
    list_for_each_entry_safe(bulk, temp, &hdl->suspend_head, entry) {
        if (!strcmp(bulk->name, name)) {
            list_del(&bulk->entry);
            anc_free(bulk);
            break;
        }
    }
    os_mutex_post(&hdl->mutex);
    audio_rtanc_suspend_list_query(0);
}

int audio_anc_real_time_adaptive_suspend_get(void)
{
    if (hdl) {
        if (!list_empty(&hdl->suspend_head)) {
            return 1;
        }
    }
    return 0;
}

//打开RTANC 前准备，sync_mode 是否TWS同步模式
int audio_rtanc_adaptive_init(u8 sync_mode)
{
    int ret;

    rtanc_debug_log("=================rt_anc_open==================\n");
    rtanc_debug_log("rtanc state %d\n", audio_anc_real_time_adaptive_state_get());
    ret = audio_rtanc_permit(sync_mode);
    if (ret) {
        rtanc_log("Err:rt_anc_open permit sync:%d, ret:%d\n", sync_mode, ret);
        audio_anc_debug_err_send_data(ANC_DEBUG_APP_RTANC, &ret, sizeof(int));
        return ret;
    }

    hdl->run_busy = 0;
    rtanc_log("RTANC_STATE:OPEN\n");
    hdl->state = RT_ANC_STATE_OPEN;
    clock_alloc("ANC_RT_ADAPTIVE", AUDIO_RT_ANC_CLOCK_ALLOC);

    hdl->ch_l.tmp = anc_malloc("ICSD_RTANC", sizeof(struct rt_anc_tmp_buffer));

#if AUDIO_ANC_STEREO_ENABLE	//头戴式
    hdl->ch_r.tmp = anc_malloc("ICSD_RTANC", sizeof(struct rt_anc_tmp_buffer));
#endif

    hdl->rtanc_p = anc_malloc("ICSD_RTANC", sizeof(struct rtanc_param));
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    audio_anc_ear_adaptive_cmp_open(CMP_FROM_RTANC);
#endif
    return 0;
}

int audio_rtanc_adaptive_exit(void)
{
    rtanc_debug_log("================rt_anc_close==================\n");
    if (audio_anc_real_time_adaptive_state_get() == 0) {
        return 1;
    }

    //恢复工具显示状态
    if (anc_ext_algorithm_state_get(ANC_EXT_ALGO_RTANC) == ANC_EXT_ALGO_STA_SUSPEND) {
        anc_ext_algorithm_state_update(ANC_EXT_ALGO_RTANC, ANC_EXT_ALGO_STA_OPEN, 0);
    }

    rtanc_log("RTANC_STATE:CLOSE\n");
    hdl->state = RT_ANC_STATE_CLOSE;
    clock_free("ANC_RT_ADAPTIVE");
    anc_free(hdl->rtanc_p);
    hdl->rtanc_p = NULL;

    anc_free(hdl->ch_l.tmp);
    hdl->ch_l.tmp = NULL;

#if AUDIO_ANC_STEREO_ENABLE	//头戴式
    anc_free(hdl->ch_r.tmp);
    hdl->ch_r.tmp = NULL;
#endif

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    audio_anc_ear_adaptive_cmp_close();
#endif
    rtanc_log("rt_anc_close ok\n");
    return 0;
}

static int audio_rtanc_adaptive_en(u8 en)
{
    int ret;
    rtanc_log("audio_rtanc_adaptive_en: %d\n", en);
    if (en) {
#if AUDIO_ANC_DATA_EXPORT_VIA_UART
        if (audio_anc_develop_is_running()) {
            printf("RTANC_STATE: ERR ANC_DEV OPEN NOW\n");
            return -1;
        }
#endif
        ret = audio_rtanc_permit(0);
        if (ret) {
            return ret;
        }
        audio_rtanc_var_cache_set(0);

        return audio_icsd_adt_sync_open(ADT_REAL_TIME_ADAPTIVE_ANC_MODE);
    } else {
        audio_icsd_adt_sync_close(ADT_REAL_TIME_ADAPTIVE_ANC_MODE, 0);
    }
    return 0;
}

int audio_anc_real_time_adaptive_open(void)
{
    int ret;
    ret = audio_rtanc_adaptive_en(1);
    if (!ret) {
        anc_ext_algorithm_state_update(ANC_EXT_ALGO_RTANC, ANC_EXT_ALGO_STA_OPEN, 0);
    }
    return ret;
}

int audio_anc_real_time_adaptive_close(void)
{
    int ret;
    ret = audio_rtanc_adaptive_en(0);

    anc_ext_algorithm_state_update(ANC_EXT_ALGO_RTANC, ANC_EXT_ALGO_STA_CLOSE, 0);

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    //app层关闭RTANC，清掉CMP工具上报参数
    audio_rtanc_cmp_data_clear();
#endif

#if AUDIO_RT_ANC_KEEP_LAST_PARAM == 1
    if (!(audio_anc_production_mode_get() || get_app_anctool_spp_connected_flag())) {
        return ret;
    }
#elif AUDIO_RT_ANC_KEEP_LAST_PARAM == 2
    return ret;
#endif
    //恢复默认ANC参数
    if ((!ret) && (anc_mode_get() == ANC_ON)) {
#if ANC_MULT_ORDER_ENABLE
        /* audio_anc_mult_scene_set(audio_anc_mult_scene_get()); */
        audio_anc_mult_scene_update(audio_anc_mult_scene_get());
#else
        audio_anc_db_cfg_read();
        audio_anc_coeff_smooth_update();
#endif
    }
    return ret;
}

#if AUDIO_RT_ANC_EXPORT_TOOL_DATA_DEBUG
static void audio_anc_real_time_adaptive_data_packet(struct icsd_rtanc_tool_data *rtanc_tool)
{
    if (!rtanc_tool) {
        return;
    }
    int i;
    int len = rtanc_tool->h_len;
    int ff_yorder = hdl->param->lff_yorder;

    int ff_dat_len =  sizeof(anc_fr_t) * ff_yorder + 4;

    struct anc_ext_ear_adaptive_param *ext_cfg = anc_ext_ear_adaptive_cfg_get();
    u8 ff_iir_type[ANC_ADAPTIVE_FF_ORDER];

    u8 *ff_dat, *rff_dat;

    if (hdl->sz_sel.state == RTANC_SZ_STATE_TRIGGER) {//SZ_SEL时，输出SZ相关滤波器
        memcpy(rtanc_tool->ff_fgq_l, hdl->sz_sel.lff_out, ((1 + 3 * RT_ANC_MAX_ORDER) * sizeof(float)));
        rtanc_tool->h_len = len = 25;
        memcpy(rtanc_tool->h_freq,  sz_sel_h_freq_test, 4 * len);
        for (int i = 0; i < len; i++) {
            rtanc_tool->sz_out_l[2 * i] = hdl->sz_sel.output.sz_l->out[2 * mem_list[i]];
            rtanc_tool->sz_out_l[2 * i + 1] = hdl->sz_sel.output.sz_l->out[2 * mem_list[i] + 1];
        }
    }

#if ANC_CONFIG_LFF_EN
    for (i = 0; i < ANC_ADAPTIVE_FF_ORDER; i++) {
        ff_iir_type[i] = ext_cfg->ff_iir_high[i].type;
        /* rtanc_log("get lff_type %d\n", iir_type[i]); */
    }

    ff_dat = anc_malloc("ICSD_RTANC", ff_dat_len);
    //RTANC 增益没有输出符号，需要对齐工具符号
    rtanc_tool->ff_fgq_l[0] = (hdl->param->gains.gain_sign & ANCL_FF_SIGN) ? \
                              (0 - rtanc_tool->ff_fgq_l[0]) : rtanc_tool->ff_fgq_l[0];
    audio_anc_fr_format(ff_dat, rtanc_tool->ff_fgq_l, ff_yorder, ff_iir_type);
#endif/*ANC_CONFIG_LFF_EN*/
#if ANC_CONFIG_RFF_EN && AUDIO_ANC_STEREO_ENABLE
    for (i = 0; i < ANC_ADAPTIVE_FF_ORDER; i++) {
        ff_iir_type[i] = ext_cfg->rff_iir_high[i].type;
        /* rtanc_log("get rff_type %d\n", iir_type[i]); */
    }
    rff_dat = anc_malloc("ICSD_RTANC", ff_dat_len);
    //RTANC 增益没有输出符号，需要对齐工具符号
    rtanc_tool->ff_fgq_r[0] = (hdl->param->gains.gain_sign & ANCR_FF_SIGN) ? \
                              (0 - rtanc_tool->ff_fgq_r[0]) : rtanc_tool->ff_fgq_r[0];
    audio_anc_fr_format(rff_dat, rtanc_tool->ff_fgq_r, ff_yorder, ff_iir_type);
#endif/*ANC_CONFIG_RFF_EN*/

    rtanc_debug_log("-- len1: %d, len2: %d\n", rtanc_tool->h_len, rtanc_tool->h2_len);
    rtanc_debug_log("-- ff_yorder = %d\n", ff_yorder);
    /* 先统一申请空间，因为下面不知道什么情况下调用函数 anc_data_catch 时令参数 init_flag 为1 */
    rtanc_tool_data = anc_data_catch(rtanc_tool_data, NULL, 0, 0, 1);

#if AUDIO_ANC_STEREO_ENABLE	//头戴式

    if (hdl->param->developer_mode) {
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h_freq, len * 4, ANC_L_ADAP_FRE, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->sz_out_l, len * 8, ANC_L_ADAP_SZPZ, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->pz_out_l, len * 8, ANC_L_ADAP_PZ, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_l, len * 8, ANC_L_ADAP_TARGET, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_cmp_l, len * 8, ANC_L_ADAP_TARGET_CMP, 0);
    }
    rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)ff_dat, ff_dat_len, ANC_L_FF_IIR, 0);  //R_ff

    rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h2_freq, rtanc_tool->h2_len * 4, ANC_L_ADAP_FRE_2, 0);
    rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->mse_l, rtanc_tool->h2_len * 8, ANC_L_ADAP_MSE, 0);

    /* put_buf((u8 *)rtanc_tool->h_freq, len * 4); */
    /* put_buf((u8 *)rtanc_tool->sz_out_l, len * 8); */
    /* put_buf((u8 *)rtanc_tool->pz_out_l, len * 8); */
    /* put_buf((u8 *)rtanc_tool->target_out_l, len * 8); */
    /* put_buf((u8 *)rtanc_tool->target_out_cmp_l, len * 8); */
    /* put_buf((u8 *)rtanc_tool->h2_freq, rtanc_tool->h2_len * 4); */
    /* put_buf((u8 *)rtanc_tool->mse_l, rtanc_tool->h2_len * 8); */

    /* put_buf((u8 *)ff_dat, ff_dat_len);  //R_ff */
    /* put_buf((u8 *)cmp_dat, cmp_dat_len);  //R_cmp */

    if (hdl->param->developer_mode) {
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h_freq, len * 4, ANC_R_ADAP_FRE, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->sz_out_r, len * 8, ANC_R_ADAP_SZPZ, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->pz_out_r, len * 8, ANC_R_ADAP_PZ, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_r, len * 8, ANC_R_ADAP_TARGET, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_cmp_r, len * 8, ANC_R_ADAP_TARGET_CMP, 0);
    }
    rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rff_dat, ff_dat_len, ANC_R_FF_IIR, 0);  //R_ff

    rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h2_freq, rtanc_tool->h2_len * 4, ANC_R_ADAP_FRE_2, 0);
    rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->mse_r, rtanc_tool->h2_len * 8, ANC_R_ADAP_MSE, 0);
#else
#if TCFG_USER_TWS_ENABLE
    if (bt_tws_get_local_channel() == 'R') {
        rtanc_debug_log("RTANC export send tool_data, ch:R\n");
        if (hdl->param->developer_mode) {
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h_freq, len * 4, ANC_R_ADAP_FRE, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->sz_out_l, len * 8, ANC_R_ADAP_SZPZ, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->pz_out_l, len * 8, ANC_R_ADAP_PZ, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_l, len * 8, ANC_R_ADAP_TARGET, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_cmp_l, len * 8, ANC_R_ADAP_TARGET_CMP, 0);
        }
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)ff_dat, ff_dat_len, ANC_R_FF_IIR, 0);  //R_ff

        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h2_freq, rtanc_tool->h2_len * 4, ANC_R_ADAP_FRE_2, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->mse_l, rtanc_tool->h2_len * 8, ANC_R_ADAP_MSE, 0);
    } else
#endif/*TCFG_USER_TWS_ENABLE*/
    {
        rtanc_debug_log("RTANC export send tool_data, ch:L\n");

        /* put_buf((u8 *)rtanc_tool->h_freq, len * 4); */
        /* put_buf((u8 *)rtanc_tool->sz_out_l, len * 8); */
        /* put_buf((u8 *)rtanc_tool->pz_out_l, len * 8); */
        /* put_buf((u8 *)rtanc_tool->target_out_l, len * 8); */
        /* put_buf((u8 *)rtanc_tool->target_out_cmp_l, len * 8); */
        /* put_buf((u8 *)rtanc_tool->h2_freq, rtanc_tool->h2_len * 4); */
        /* put_buf((u8 *)rtanc_tool->mse_l, rtanc_tool->h2_len * 8); */

        /* put_buf((u8 *)ff_dat, ff_dat_len);  //R_ff */
        /* put_buf((u8 *)cmp_dat, cmp_dat_len);  //R_cmp */

        if (hdl->param->developer_mode) {
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h_freq, len * 4, ANC_L_ADAP_FRE, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->sz_out_l, len * 8, ANC_L_ADAP_SZPZ, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->pz_out_l, len * 8, ANC_L_ADAP_PZ, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_l, len * 8, ANC_L_ADAP_TARGET, 0);
            rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->target_out_cmp_l, len * 8, ANC_L_ADAP_TARGET_CMP, 0);
        }
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)ff_dat, ff_dat_len, ANC_L_FF_IIR, 0);  //R_ff

        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->h2_freq, rtanc_tool->h2_len * 4, ANC_L_ADAP_FRE_2, 0);
        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)rtanc_tool->mse_l, rtanc_tool->h2_len * 8, ANC_L_ADAP_MSE, 0);
    }
#endif

#if ANC_CONFIG_LFF_EN
    anc_free(ff_dat);
#endif/*ANC_CONFIG_LFF_EN*/
#if ANC_CONFIG_RFF_EN && AUDIO_ANC_STEREO_ENABLE
    anc_free(rff_dat);
#endif/*ANC_CONFIG_RFF_EN*/

}

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
//将CMP 工具数据拼接到RTANC
static void audio_rtanc_cmp_tool_data_catch(u8 *data, int len, u8 ch)
{
    if (!data) {
        return;
    }
    float *sz_cmp, *sz_temp;
    u8 sz_cmp_id, iir_id;
#if AUDIO_ANC_STEREO_ENABLE	//头戴式
    iir_id = ch ? ANC_R_CMP_IIR : ANC_L_CMP_IIR;
#else
#if TCFG_USER_TWS_ENABLE
    if (bt_tws_get_local_channel() == 'R') {
        rtanc_log("cmp export send tool_data, ch:R\n");
        sz_cmp_id = ANC_R_ADAP_SZ_CMP;
        iir_id = ANC_R_CMP_IIR;
    } else
#endif/*TCFG_USER_TWS_ENABLE*/
    {
        rtanc_log("cmp export send tool_data, ch:L\n");
        sz_cmp_id = ANC_L_ADAP_SZ_CMP;
        iir_id = ANC_L_CMP_IIR;
    }
#endif
#if AUDIO_ANC_ADAPTIVE_CMP_SZ_FACTOR
    sz_cmp = audio_anc_ear_adaptive_cmp_sz_cmp_get(ANC_EAR_ADAPTIVE_CMP_CH_L);
    if (sz_cmp) {
        //60点->25点
        /* sz_temp = anc_malloc("ICSD_CMP", test_num); */
        sz_temp = anc_malloc("ICSD_CMP", 25 * 2 * sizeof(float));
        for (int i = 0; i < 25; i++) {
            sz_temp[2 * i] = sz_cmp[2 * mem_list[i]];
            sz_temp[2 * i + 1] = sz_cmp[2 * mem_list[i] + 1];
        }

        rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)sz_temp, 25 * 8, sz_cmp_id, 0);
        anc_free(sz_temp);
    }
#endif
    rtanc_tool_data = anc_data_catch(rtanc_tool_data, (u8 *)data, len, iir_id, 0);  //R_cmp
}

void audio_rtanc_cmp_data_clear(void)
{
    if (hdl) {
        if (hdl->ch_l.cmp_tool_data) {
            anc_free(hdl->ch_l.cmp_tool_data);
            hdl->ch_l.cmp_tool_data = NULL;
        }
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        if (hdl->ch_r.cmp_tool_data) {
            anc_free(hdl->ch_r.cmp_tool_data);
            hdl->ch_r.cmp_tool_data = NULL;
        }
#endif
        hdl->cmp_tool_data_len = 0;
    }
}

static u8 *audio_rtanc_cmp_data_packet_ch(u8 *tool_data, u8 ch)
{
    int cmp_yorder = ANC_ADAPTIVE_CMP_ORDER;
    float *cmp_output = audio_anc_ear_adaptive_cmp_output_get(ch);
    u8 *cmp_type = audio_anc_ear_adaptive_cmp_type_get(ch);
    int cmp_dat_len = hdl->cmp_tool_data_len;
    u8 *cmp_dat = anc_malloc("ICSD_RTANC", cmp_dat_len);

    audio_anc_fr_format(cmp_dat, cmp_output, cmp_yorder, cmp_type);

    if (tool_data) {
        anc_free(tool_data);
    }
    tool_data = anc_malloc("ICSD_CMP", cmp_dat_len);
    memcpy(tool_data, cmp_dat, cmp_dat_len);
    anc_free(cmp_dat);

    rtanc_debug_log("cmp_data_packet yorder = %d, ch %d\n", cmp_yorder, ch);

    audio_rtanc_cmp_tool_data_catch(tool_data, cmp_dat_len, ch);

    return tool_data;
}

//格式化CMP工具数据
void audio_rtanc_cmp_data_packet(void)
{
    if (!hdl->rtanc_tool) {
        return;
    }
    hdl->cmp_tool_data_len = sizeof(anc_fr_t) * ANC_ADAPTIVE_CMP_ORDER + 4;
    hdl->ch_l.cmp_tool_data = audio_rtanc_cmp_data_packet_ch(hdl->ch_l.cmp_tool_data, ANC_EAR_ADAPTIVE_CMP_CH_L);
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    hdl->ch_r.cmp_tool_data = audio_rtanc_cmp_data_packet_ch(hdl->ch_r.cmp_tool_data, ANC_EAR_ADAPTIVE_CMP_CH_R);
#endif
}
#endif/*TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN*/

#endif

int audio_anc_real_time_adaptive_tool_data_get(u8 **buf, u32 *len)
{
    if ((!audio_anc_real_time_adaptive_suspend_get()) && audio_anc_real_time_adaptive_state_get()) {
        rtanc_log("rtanc tool_data err: running! need suspend/close, return!\n");
        return -1;
    }
    if (rtanc_tool_data == NULL) {
        rtanc_log("rtanc tool_data err: packet is NULL, return!\n");
        return -1;
    }
    if (rtanc_tool_data->dat_len == 0) {
        rtanc_log("rtanc tool_data err: dat_len == 0\n");
        return -1;
    }
    rtanc_log("rtanc_tool_data->dat_len %d\n", rtanc_tool_data->dat_len);
    *buf = rtanc_tool_data->dat;
    *len = rtanc_tool_data->dat_len;
    return 0;
}

int audio_rtanc_fade_gain_suspend(struct rt_anc_fade_gain_ctr *fade_ctr)
{
    return 0; // gain no support
    rtanc_debug_log("%s, state %d\n", __func__, audio_anc_real_time_adaptive_state_get());
    if ((!audio_anc_real_time_adaptive_state_get()) || \
        get_icsd_adt_mode_switch_busy() || anc_mode_switch_lock_get()) {
        hdl->fade_ctr = *fade_ctr;
        return 0;
    }
    if (hdl) {
        if (memcmp(&hdl->fade_ctr, fade_ctr, sizeof(struct rt_anc_fade_gain_ctr))) {
            audio_anc_real_time_adaptive_suspend("ANC_FADE");

            if (hdl->rtanc_p) {
                hdl->rtanc_p->ffl_filter.fade_gain = ((float)fade_ctr->lff_gain) / 16384.0f;
                hdl->rtanc_p->fbl_filter.fade_gain = ((float)fade_ctr->lfb_gain) / 16384.0f;
                hdl->rtanc_p->ffr_filter.fade_gain = ((float)fade_ctr->rff_gain) / 16384.0f;
                hdl->rtanc_p->fbr_filter.fade_gain = ((float)fade_ctr->rfb_gain) / 16384.0f;

                icsd_adt_rtanc_fadegain_update(hdl->rtanc_p);
            }
            hdl->fade_ctr = *fade_ctr;
        }
    }
    return 0;
}

int audio_rtanc_fade_gain_resume(void)
{
    audio_anc_real_time_adaptive_resume("ANC_FADE");
    return 0;
}

static u8 RTANC_idle_query(void)
{
    if (hdl) {
        return (hdl->state) ? 0 : 1;
    }
    return 1;
}

u8 audio_anc_real_time_adaptive_state_get(void)
{
    return (!RTANC_idle_query());
}

u8 audio_anc_real_time_adaptive_run_busy_get(void)
{
    if (hdl) {
        return hdl->run_busy;
    }
    return 0;
}

REGISTER_LP_TARGET(RTANC_lp_target) = {
    .name       = "RTANC",
    .is_idle    = RTANC_idle_query,
};

#if AUDIO_RT_ANC_SZ_PZ_CMP_EN
static void audio_rtanc_sz_pz_cmp_process(void)
{
    struct sz_pz_cmp_cal_param p;
    p.spk_eq_tab = NULL;
    p.ff_gain = 1.0f;
    p.fb_gain = 1.0f;

#if ANC_DUT_MIC_CMP_GAIN_ENABLE
    hdl->mic_cmp = &hdl->param->mic_cmp;
#endif

    if (hdl->ch_l.tmp) {
        p.pz_cmp_out = hdl->ch_l.tmp->pz_cmp;
        p.sz_cmp_out = hdl->ch_l.tmp->sz_cmp;
    }
#if ANC_DUT_MIC_CMP_GAIN_ENABLE
    p.ff_gain = hdl->mic_cmp->lff_gain;
    p.fb_gain = hdl->mic_cmp->lfb_gain;
#endif
    icsd_anc_v2_sz_pz_cmp_calculate(&p);

#if AUDIO_ANC_STEREO_ENABLE
    if (hdl->ch_r.tmp) {
        p.pz_cmp_out = hdl->ch_r.tmp->pz_cmp;
        p.sz_cmp_out = hdl->ch_r.tmp->sz_cmp;
    }
#if ANC_DUT_MIC_CMP_GAIN_ENABLE
    p.ff_gain = hdl->mic_cmp->rff_gain;
    p.fb_gain = hdl->mic_cmp->rfb_gain;
#endif

    icsd_anc_v2_sz_pz_cmp_calculate(&p);
#endif
}
#endif

void audio_rtanc_self_talk_output(u8 flag)
{
    // printf("self talk flag: %d\n", flag);
    /* audio_rtanc_spp_send_data(RTANC_SPP_CMD_SELF_STATE, &flag, 1); */
}

static void audio_rtanc_spp_send_data(u8 cmd, u8 *buf, int len)
{
    audio_anc_debug_app_send_data(ANC_DEBUG_APP_RTANC, cmd, buf, len);
}

void audio_rtanc_dut_mode_set(u8 mode)
{
    if (hdl) {
        hdl->dut_mode = mode;
    }
}

struct rt_anc_tmp_buffer *audio_rtanc_tmp_buff_get(u8 ch)
{
    if (hdl) {
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        return ch ? hdl->ch_r.tmp : hdl->ch_l.tmp;
#else
        return hdl->ch_l.tmp;
#endif
    }
    return NULL;
}

#if RTANC_SZ_SEL_FF_OUTPUT
//SZ_SEL 模式: AFQ输出回调，根据SZ筛选一条稳定的SZ，和FF滤波器，提供给CMP/AEQ计算
static void audio_rtanc_sz_select_afq_output_hdl(struct audio_afq_output *p)
{
    //g_printf("afq_output sz\n");
    //put_buf((u8 *)(p->sz_l.out), p->sz_l.len * 4);
    u8 sz_alloc = 0, pz_alloc = 0;

#if AUDIO_RT_ANC_SZ_PZ_CMP_EN
    if (!hdl->pz_cmp) {
        pz_alloc = 1;
        hdl->pz_cmp = anc_malloc("ICSD_RTANC", 50 * sizeof(float));
    }
    if (!hdl->sz_cmp) {
        sz_alloc = 1;
        hdl->sz_cmp = anc_malloc("ICSD_RTANC", 50 * sizeof(float));
    }
    audio_rtanc_sz_pz_cmp_process();
#endif
    __afq_output *output = &hdl->sz_sel.output;
    if (output->sz_l) {
        anc_free(output->sz_l->out);
        anc_free(output->sz_l);
    }
    //输出完毕后释放
    output->sz_l = anc_malloc("ICSD_RTANC", sizeof(__afq_data));
    output->sz_l->len = p->sz_l.len;
    output->sz_l->out = anc_malloc("ICSD_RTANC", sizeof(float) * output->sz_l->len);

    memcpy((u8 *)output->sz_l->out, (u8 *)p->sz_l.out, sizeof(float) * output->sz_l->len);

    hdl->sz_sel.sel_idx = icsd_anc_sz_select_from_memory(hdl->sz_sel.lff_out, output->sz_l->out, p->sz_l.msc, output->sz_l->len);

    //g_printf("sz_alogm_output sz\n");
    //put_buf((u8 *)(output->sz_l->out), p->sz_l.len * 4);

    if (pz_alloc) {
        anc_free(hdl->pz_cmp);
        hdl->pz_cmp = NULL;
    }
    if (sz_alloc) {
        anc_free(hdl->sz_cmp);
        hdl->sz_cmp = NULL;
    }

#if RTANC_SZ_SEL_FF_OUTPUT
    //FGQ 输出为IIR:start
    int lff_yorder = hdl->param->lff_yorder;

    if (hdl->ch_l.out.ff_coeff) {
        anc_free(hdl->ch_l.out.ff_coeff);
    }
    hdl->ch_l.out.ff_coeff = anc_malloc("ICSD_RTANC", lff_yorder * AUDIO_RTANC_COEFF_SIZE);

    int ff_dat_len =  sizeof(anc_fr_t) * lff_yorder + 4;
    u8 *ff_dat = anc_malloc("ICSD_RTANC", ff_dat_len);
    int alogm = audio_anc_gains_alogm_get(ANC_FF_TYPE);
    audio_anc_fr_format(ff_dat, hdl->sz_sel.lff_out, lff_yorder, hdl->lff_iir_type);

    anc_fr_t *fr = (anc_fr_t *)(ff_dat + 4);
    /*
    g_printf("lff_out\n");
    printf("gain %d/1000\n", (int)((*((float *)ff_dat)) * 1000.0f));
    for (int i = 0; i < lff_yorder; i++) {
        printf("type %d, f %d, g %d, q %d\n", fr[i].type, \
               (int)(fr[i].a[0] * 1000.f), (int)(fr[i].a[1] * 1000.f), \
               (int)(fr[i].a[2] * 1000.f));
    }
    */
    audio_anc_biquad2ab_double((anc_fr_t *)(ff_dat + 4), hdl->ch_l.out.ff_coeff, lff_yorder, alogm);

    hdl->ch_l.out.ff_gain = hdl->sz_sel.lff_out[0] * RTANC_SZ_SEL_FF_OUTPUT_GAIN_RATIO;
    hdl->param->gains.l_ffgain = hdl->sz_sel.lff_out[0] * RTANC_SZ_SEL_FF_OUTPUT_GAIN_RATIO;
    hdl->param->lff_coeff = hdl->ch_l.out.ff_coeff;
    rtanc_debug_log("sz_alogm_ff_coeff output\n");
    // put_buf((u8 *)hdl->ch_l.out.ff_coeff, 10 * AUDIO_RTANC_COEFF_SIZE);
    anc_free(ff_dat);
    //FGQ 输出为AABB:stop
#endif

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    //挑线输出CMP滤波器并输出为IIR
    //FGQ 输出为IIR:start
    int lcmp_yorder = hdl->param->lcmp_yorder;
    u8 cmp_iir_type[ANC_ADAPTIVE_CMP_ORDER];
    hdl->sz_sel.lcmp_out = anc_malloc("ICSD_RTANC", (1 + 3 * RT_ANC_MAX_ORDER) * sizeof(float));

    audio_anc_ear_adaptive_cmp_sz_sel(hdl->sz_sel.sel_idx, hdl->sz_sel.lcmp_out, cmp_iir_type, 0);
    if (hdl->ch_l.out.cmp_coeff) {
        anc_free(hdl->ch_l.out.cmp_coeff);
    }
    hdl->ch_l.out.cmp_coeff = anc_malloc("ICSD_RTANC", lcmp_yorder * AUDIO_RTANC_COEFF_SIZE);

    int cmp_dat_len =  sizeof(anc_fr_t) * lcmp_yorder + 4;
    u8 *cmp_dat = anc_malloc("ICSD_RTANC", cmp_dat_len);
    int cmp_alogm = audio_anc_gains_alogm_get(ANC_CMP_TYPE);
    audio_anc_fr_format(cmp_dat, hdl->sz_sel.lcmp_out, lcmp_yorder, cmp_iir_type);

    anc_fr_t *fr = (anc_fr_t *)(cmp_dat + 4);

    audio_anc_biquad2ab_double((anc_fr_t *)(cmp_dat + 4), hdl->ch_l.out.cmp_coeff, lcmp_yorder, cmp_alogm);

    hdl->ch_l.out.cmp_gain = hdl->sz_sel.lcmp_out[0];
    hdl->param->gains.l_cmpgain = hdl->sz_sel.lcmp_out[0];
    hdl->param->lcmp_coeff = hdl->ch_l.out.cmp_coeff;
    rtanc_debug_log("sz_alogm_cmp_coeff output\n");
    // put_buf((u8 *)hdl->ch_l.out.cmp_coeff, 10 * AUDIO_RTANC_COEFF_SIZE);
    anc_free(cmp_dat);
    anc_free(hdl->sz_sel.lcmp_out);
    //FGQ 输出为AABB:stop
#endif

    hdl->sz_sel.state |= RTANC_SZ_STA_AFQ_OUT;
    rtanc_log("RTANC_STATE_SZ:AFQ_OUT\n");
    audio_afq_common_del_output_handler("RTANC");
}

//SZ_SEL 模式:启动
int audio_rtanc_sz_select_open(void)
{
    //选择数据来源AFQ
    if (hdl->sz_sel.state) { //重入判定
        return 1;
    }
    int fre_sel = AUDIO_ADAPTIVE_FRE_SEL_AFQ;
    hdl->sz_sel.state = RTANC_SZ_STA_INIT;
    if (hdl->sz_sel.lff_out == NULL) {
        hdl->sz_sel.lff_out = anc_malloc("ICSD_RTANC", (1 + 3 * RT_ANC_MAX_ORDER) * sizeof(float));
    }
    /* audio_anc_fade_ctr_set(ANC_FADE_MODE_RTANC_SZ_HOLD, AUDIO_ANC_FDAE_CH_ALL, 0); */
    rtanc_log("RTANC_STATE_SZ:INIT %lu \n", (1 + 3 * RT_ANC_MAX_ORDER) * sizeof(float));
    audio_anc_real_time_adaptive_suspend("SZ_SEL");
    audio_afq_common_add_output_handler("RTANC", fre_sel, audio_rtanc_sz_select_afq_output_hdl);
    return 0;
}

/*
   SZ_SEL 模式: ANC线程处理, 更新RTANC效果，以及计算AEQ/CMP
   复位相关变量和资源
*/
int audio_rtanc_sz_select_process(void)
{
    __afq_output *output = &hdl->sz_sel.output;
    __rt_anc_param rt_param_l;

    rt_param_l.ffgain = hdl->ch_l.out.ff_gain;
    rt_param_l.fbgain = hdl->param->gains.l_fbgain;
    rt_param_l.ff_updat = rt_param_l.fb_updat = rt_param_l.cmp_eq_updat = \
                          (AUDIO_ADAPTIVE_AEQ_UPDATE_FLAG | AUDIO_ADAPTIVE_CMP_UPDATE_FLAG);


    audio_adt_rtanc_output_handle(&rt_param_l, NULL);

    rtanc_log("RTANC_STATE_SZ:STOP\n");
    hdl->sz_sel.state = 0;

    if (output->sz_l) {
        anc_free(output->sz_l->out);
        anc_free(output->sz_l);
        output->sz_l = NULL;
    }
    if (hdl->sz_sel.lff_out) {
        anc_free(hdl->sz_sel.lff_out);
        hdl->sz_sel.lff_out = NULL;
    }
    /* audio_anc_fade_ctr_set(ANC_FADE_MODE_RTANC_SZ_HOLD, AUDIO_ANC_FDAE_CH_ALL, 16384); */
    audio_anc_real_time_adaptive_resume("SZ_SEL");

    return 0;
}

//入耳提示音自适应结束callback
int audio_rtanc_sz_sel_callback(void)
{
    //若用户模式是ANC_OFF，则需要关闭RTANC
    if (anc_user_mode_get() == ANC_OFF) {
        anc_mode_switch_base(ANC_OFF, 0);
    }
    return 0;
}

int audio_rtanc_sz_sel_state_get(void)
{
    if (hdl) {
        return (hdl->sz_sel.state & RTANC_SZ_STA_AFQ_OUT);
    }
    return 0;
}
#endif

int audio_rtanc_coeff_set(void)
{

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    //工具连接时，切ANC默认清掉CMP工具上报参数
    audio_rtanc_cmp_data_clear();
#endif

    struct rt_anc_output *out;

#if AUDIO_RT_ANC_KEEP_LAST_PARAM
    if ((audio_anc_production_mode_get() || get_app_anctool_spp_connected_flag()) && \
        (AUDIO_RT_ANC_KEEP_LAST_PARAM == 1)) {
        return 0;
    }
    if (hdl) {
        audio_anc_t *param = hdl->param;
        out = &hdl->ch_l.out;
#if AUDIO_RT_ANC_KEEP_FF_COEFF   //是否保留上次RTANC FF结果
        if (out->ff_coeff) {
            param->gains.l_ffgain = out->ff_gain;
            param->lff_coeff = out->ff_coeff;
        }
        if (out->fb_gain != 0.0f) {	//初始化为0
            param->gains.l_fbgain = out->fb_gain;
        }
#elif RTANC_SZ_SEL_FF_OUTPUT
        if (hdl->sz_sel.state & RTANC_SZ_STA_INIT) {
            if (out->ff_coeff) {
                param->gains.l_ffgain = out->ff_gain;
                param->lff_coeff = out->ff_coeff;
            }
            if (out->fb_gain != 0.0f) {	//初始化为0
                param->gains.l_fbgain = out->fb_gain;
            }
        }
#endif
        if (out->cmp_coeff) {
            if (anc_mode_get() == ANC_ON) {
                param->gains.l_cmpgain = out->cmp_gain;
                param->lcmp_coeff = out->cmp_coeff;
            } else { //通透
                u8 sign = param->gains.gain_sign;
                param->ltrans_cmpgain = (sign & ANCL_CMP_SIGN) ? (0 - out->cmp_gain) : out->cmp_gain;
                param->ltrans_cmp_coeff = out->cmp_coeff;
            }
        }

        //-------- 右声道 参数处理 ---------
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        out = &hdl->ch_r.out;
#if AUDIO_RT_ANC_KEEP_FF_COEFF   // 保留上次RTANC FF结果（右声道）
        if (out->ff_coeff) {
            param->gains.r_ffgain = out->ff_gain;
            param->rff_coeff = out->ff_coeff;
        }
        if (out->fb_gain != 0.0f) {	// 初始化为0时不更新
            param->gains.r_fbgain = out->fb_gain;
        }
#elif RTANC_SZ_SEL_FF_OUTPUT
        if (hdl->sz_sel.state & RTANC_SZ_STA_INIT) {
            if (out->ff_coeff) {
                param->gains.r_ffgain = out->ff_gain;
                param->rff_coeff = out->ff_coeff;
            }
            if (out->fb_gain != 0.0f) {	// 初始化为0时不更新
                param->gains.r_fbgain = out->fb_gain;
            }
        }
#endif

        if (out->cmp_coeff) {
            if (anc_mode_get() == ANC_ON) {
                param->gains.r_cmpgain = out->cmp_gain;
                param->rcmp_coeff = out->cmp_coeff;
            } else { // 通透模式
                u8 sign = param->gains.gain_sign;
                param->rtrans_cmpgain = (sign & ANCR_CMP_SIGN) ? (0 - out->cmp_gain) : out->cmp_gain;
                param->rtrans_cmp_coeff = out->cmp_coeff;
            }
        }
#endif
    }
#endif
    audio_anc_coeff_check_crc(ANC_CHECK_RTANC_SAVE);
    return 0;
}

//测试demo
int audio_anc_real_time_adaptive_demo(void)
{
#if 1//tone
    if (hdl->state == RT_ANC_STATE_CLOSE) {
        if (audio_anc_real_time_adaptive_open()) {
            icsd_adt_tone_play(ICSD_ADT_TONE_NUM0);
        } else {
            icsd_adt_tone_play(ICSD_ADT_TONE_NUM1);
        }
    } else {
        icsd_adt_tone_play(ICSD_ADT_TONE_NUM0);
        audio_anc_real_time_adaptive_close();
    }
#else
    if (hdl->state == RT_ANC_STATE_CLOSE) {
        audio_anc_real_time_adaptive_open();
    } else {
        audio_anc_real_time_adaptive_close();
    }
#endif
    return 0;
}

#endif
