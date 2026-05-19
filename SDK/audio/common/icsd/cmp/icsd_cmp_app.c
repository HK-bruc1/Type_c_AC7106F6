#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".icsd_cmp.data.bss")
#pragma data_seg(".icsd_cmp.data")
#pragma const_seg(".icsd_cmp.text.const")
#pragma code_seg(".icsd_cmp.text")
#endif

#include "app_config.h"
#include "audio_config_def.h"
#if (TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN && \
	 TCFG_AUDIO_ANC_ENABLE && \
	 TCFG_AUDIO_ANC_EAR_ADAPTIVE_VERSION == ANC_EXT_V2)

#include "audio_anc_includes.h"

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN

#include "icsd_cmp.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "icsd_cmp_config.h"

#if 1
#define cmp_log printf
#else
#define cmp_log(...)
#endif

#define ICSD_CMP_OUTPUT_PRINTF                  1       //cmp 输出滤波器打印
#define ANC_ADAPTIVE_CMP_RUN_TIME_DEBUG			1		//CMP 算法运行时间测试

#define ANC_ADAPTIVE_CMP_OUTPUT_SIZE		((1 + 3 * ANC_ADAPTIVE_CMP_ORDER) * sizeof(float))

struct icsd_cmp_param {
    u8 update_flag;
    u8 type[ANC_ADAPTIVE_CMP_ORDER];		//滤波器类型
    _cmp_output *output;					//算法输出
    float gain;								//增益
    anc_fr_t fr[ANC_ADAPTIVE_CMP_ORDER];	//输出整理
    double *coeff;							//IIR_COEFF
    float *sz_cmp;							//双FB方案, sz补偿
};

struct icsd_cmp_hdl {
    u8 state;
    u8 busy;
    u8 data_from;
    void *lib_alloc_ptr;

    struct icsd_cmp_param l_param;
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    struct icsd_cmp_param r_param;
#endif
};

int (*cmp_printf)(const char *format, ...) = _cmp_printf;

struct icsd_cmp_hdl *cmp_hdl = NULL;

static int audio_anc_ear_adaptive_cmp_close(void);

void icsd_cmp_force_exit(void)
{
    if (cmp_hdl) {
        DeAlorithm_disable();
    }
}

#if 0
static const float sz_cmp_factor_local[] = {  // 60*2
    0.963793, 0.000000, 0.986749, 0.014526, 1.003910, 0.012926, 1.026123, 0.011642, 1.033021, -0.003880, 1.030658, -0.007711, 1.040935, -0.001288, 1.052287, -0.003880, 1.082905, -0.001467, 1.081072, -0.014716,
    1.137425, -0.022067, 1.168316, -0.050960, 1.219801, -0.082931, 1.203713, -0.220755, 1.204439, -0.216236, 1.201030, -0.293226, 1.180225, -0.360809, 1.125884, -0.479319, 0.931298, -0.480807, 0.941836, -0.467514,
    0.899464, -0.472681, 0.829148, -0.447897, 0.771591, -0.411462, 0.777075, -0.374702, 0.742858, -0.386555, 0.726220, -0.346913, 0.712452, -0.353743, 0.673371, -0.351083, 0.647078, -0.331966, 0.611047, -0.287191,
    0.626269, -0.260849, 0.614002, -0.245480, 0.605257, -0.230781, 0.600403, -0.203312, 0.594758, -0.188350, 0.591445, -0.158516, 0.608727, -0.143485, 0.607351, -0.129028, 0.613200, -0.117099, 0.621101, -0.101453,
    0.633386, -0.093033, 0.639954, -0.090755, 0.638554, -0.085074, 0.646624, -0.077382, 0.648430, -0.074510, 0.652886, -0.066783, 0.659506, -0.063976, 0.663030, -0.062161, 0.664012, -0.059119, 0.667434, -0.053777,
    0.669919, -0.050635, 0.675036, -0.048427, 0.677023, -0.046558, 0.681144, -0.043662, 0.684940, -0.043610, 0.685590, -0.042749, 0.685522, -0.041096, 0.689335, -0.038630, 0.690117, -0.037390, 0.693586, -0.036743,
};
#endif


static int anc_ear_adaptive_cmp_run_ch(struct icsd_cmp_param *p, float *sz, int sz_len, u8 ch)
{
    float *sz_tmp;
    _cmp_output *output;
    s8 sz_sign = anc_ext_ear_adaptive_sz_calr_sign_get();
    /* cmp_log("sz_sign %d\n", sz_sign); */
    if (sz && sz_len) {
        struct anc_ext_ear_adaptive_param *ext_cfg = anc_ext_ear_adaptive_cfg_get();
        __adpt_cmp_cfg *cmp_cfg = anc_malloc("ICSD_CMP", sizeof(__adpt_cmp_cfg));
        icsd_cmp_cfg_set(cmp_cfg, ext_cfg, ch, ANC_ADAPTIVE_CMP_ORDER);
        memcpy(p->type, cmp_cfg->type, ANC_ADAPTIVE_CMP_ORDER);

#if ANC_ADAPTIVE_CMP_RUN_TIME_DEBUG
        u32 last = jiffies_usec();
#endif
        //icsd_cmp_run 会修改SZ数据，需要备份
        sz_tmp = (float *)anc_malloc("ICSD_CMP", sz_len * sizeof(float));

#if AUDIO_ANC_ADAPTIVE_CMP_SZ_FACTOR
        //双FB 方案需做SZ补偿
        if (audio_anc_stereo_mix_get()) {
            cmp_log("ICSD_CMP_STATE: SZ_CMP RUN\n");
            u8 l_num = get_anc_l_fbyorder();
            u8 r_num = get_anc_r_fbyorder();
            float l_gain = get_anc_gains_l_fbgain();
            float r_gain = get_anc_gains_r_fbgain();
            double *l_coeff = (double *)get_anc_lfb_coeff();
            double *r_coeff = (double *)get_anc_rfb_coeff();
            fbx2_szcmp_cal(l_coeff, l_gain, l_num, r_coeff, r_gain, r_num, sz, (float *)ext_cfg->cmp_sz_factor, sz_tmp);
            //fbx2_szcmp_cal(l_coeff, l_gain, l_num, r_coeff, r_gain, r_num, sz, (float *)sz_cmp_factor_local, sz_tmp);

            if (anc_ext_tool_online_get()) {
                if (p->sz_cmp) {
                    anc_free(p->sz_cmp);
                }
                p->sz_cmp = anc_malloc("ICSD_CMP_APP", sz_len * sizeof(float));
                memcpy((u8 *)p->sz_cmp, (u8 *)sz_tmp, sz_len * sizeof(float));
            }
        } else
#endif
        {
            memcpy((u8 *)sz_tmp, (u8 *)sz, sz_len * sizeof(float));
        }

        //CMP来自耳道/实时自适应时，SZ符号一定为正
        output = icsd_cmp_run(sz_tmp, cmp_cfg);

        cmp_log("ANC_ADAPTIVE_CMP_OUTPUT_SIZE %lu, ch %d\n", ANC_ADAPTIVE_CMP_OUTPUT_SIZE, ch);

        p->output->state = output->state;
        memcpy((u8 *)p->output->fgq, (u8 *)output->fgq, ANC_ADAPTIVE_CMP_OUTPUT_SIZE);

        //CMP增益符号与SZ符号 负相关
        p->output->fgq[0] = (abs(p->output->fgq[0])) * (sz_sign) * (-1);

#if ANC_ADAPTIVE_CMP_RUN_TIME_DEBUG
        cmp_log("ANC ADAPTIVE CMP RUN time: %d us\n", (int)(jiffies_usec2offset(last, jiffies_usec())));
#endif

#if 0	//CMP结果输出打印
        float *fgq = p->output->fgq;
        cmp_log("Global %d/10000\n", (int)(fgq[0] * 10000));
        for (int i = ; i < ANC_ADAPTIVE_CMP_ORDER; i++) {
            cmp_log("SEG[%d]:Type %d, FGQ/10000:%d %d %d\n", i, p->type[i], \
                    (int)(fgq[i * 3 + 1] * 10000), (int)(fgq[i * 3 + 2] * 10000), \
                    (int)(fgq[i * 3 + 3] * 10000));
        }
#elif ICSD_CMP_OUTPUT_PRINTF
        float *fgq = p->output->fgq;
        for (int i = cmp_cfg->iir_num_fix; i < ANC_ADAPTIVE_CMP_ORDER; i++) {
            cmp_log("CMP[%d] %d, %d, %d\n", i, (int)(fgq[i * 3 + 1] * 10000), \
                    (int)(fgq[i * 3 + 2] * 10000), (int)(fgq[i * 3 + 3] * 10000));
        }
#endif

        anc_free(sz_tmp);
        anc_free(cmp_cfg);
    }
    return p->output->state;
}

int audio_anc_ear_adaptive_cmp_run(void *output)
{
    int ret = 0;
    u8 ignore_mode = 0;
    u8 l_state = 0, r_state = 0;
    struct icsd_cmp_libfmt libfmt;
    struct icsd_cmp_infmt  fmt;

    if (!cmp_hdl) {
        return -1;
    }
    if (cmp_hdl->data_from == CMP_FROM_RTANC) {
        if (((struct audio_afq_output *)output)->priority == AUDIO_AFQ_PRIORITY_HIGH) {
            ignore_mode = 1;
        }
    }
#if (ANC_ADAPTIVE_CMP_ONLY_IN_MUSIC_UPDATE && TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE)
    //非通话/播歌状态 不更新
    if (!(a2dp_player_runing() || mic_effect_player_runing()) && \
        (!ignore_mode)) {
        cmp_log("icsd_cmp:audio idle, return\n");
        return -1;
    }
#endif
    if (esco_player_runing()) { //通话不更新cmp
        cmp_log("icsd_cmp:in phone, return\n");
        return -1;
    }
    if ((cmp_hdl->state == ANC_EAR_ADAPTIVE_CMP_STATE_CLOSE) || cmp_hdl->busy) {
        return -1;
    }
    cmp_hdl->busy = 1;
    wdt_clear();
    cmp_log("ICSD_CMP_STATE:RUN");

    //申请算法资源
    icsd_cmp_get_libfmt(&libfmt);
    //cmp_log("CMP RAM SIZE:%d %d %d\n", libfmt.lib_alloc_size, cmp_IIR_NUM_FIX, cmp_IIR_NUM_FLEX);
    cmp_hdl->lib_alloc_ptr = fmt.alloc_ptr = anc_malloc("ICSD_CMP", libfmt.lib_alloc_size);
    cmp_log("%p--%x \n", cmp_hdl->lib_alloc_ptr, ((u32)cmp_hdl->lib_alloc_ptr) + libfmt.lib_alloc_size);
    icsd_cmp_set_infmt(&fmt);

    struct anc_ext_ear_adaptive_param *ext_cfg = anc_ext_ear_adaptive_cfg_get();

    if (cmp_hdl->data_from == CMP_FROM_RTANC) {
        struct audio_afq_output *p = (struct audio_afq_output *)output;
        if ((!p->sz_l_sel_idx) && (p->l_cmp_eq_update & AUDIO_ADAPTIVE_CMP_UPDATE_FLAG)) {
            l_state = anc_ear_adaptive_cmp_run_ch(&cmp_hdl->l_param, p->sz_l.out, p->sz_l.len, 0);
        }

#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        if ((!p->sz_r_sel_idx) && (p->r_cmp_eq_update & AUDIO_ADAPTIVE_CMP_UPDATE_FLAG)) {
            r_state = anc_ear_adaptive_cmp_run_ch(&cmp_hdl->r_param, p->sz_r.out, p->sz_r.len, 1);
        }
#endif
    } else {
        __afq_output *p2 = (__afq_output *)output;
        if (p2->sz_l) {
            l_state = anc_ear_adaptive_cmp_run_ch(&cmp_hdl->l_param, p2->sz_l->out, p2->sz_l->len, 0);
        }

#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        if (p2->sz_r) {
            r_state = anc_ear_adaptive_cmp_run_ch(&cmp_hdl->r_param, p2->sz_r->out, p2->sz_r->len, 1);
        }
#endif
    }
    if (l_state || r_state) {
        printf("CMP OUTPUT ERR!! STATE : L %d, R %d\n", l_state, r_state);
        ret = -1;
    }
    cmp_hdl->busy = 0;

    //释放算法资源
    anc_free(cmp_hdl->lib_alloc_ptr);

    cmp_log("ICSD_CMP_STATE:RUN FINISH");
    return ret;
}

void audio_anc_ear_adaptive_cmp_sz_sel(u8 sz_sel, float *fgq, u8 *iir_type, u8 ch)
{
    u8 sz_sel_tmp = sz_sel;
    s16 *fgq_table;
    sz_sel = (sz_sel >= 101) ? 60 : sz_sel;
    cmp_log("cmp:ch %d sz_sel %d -> %d\n", ch, sz_sel_tmp, sz_sel);

    struct anc_ext_ear_adaptive_param *ext_cfg = anc_ext_ear_adaptive_cfg_get();
    __adpt_cmp_cfg *cmp_cfg = anc_malloc("ICSD_CMP", sizeof(__adpt_cmp_cfg));
    icsd_cmp_cfg_set(cmp_cfg, ext_cfg, ch, ANC_ADAPTIVE_CMP_ORDER);
    memcpy(iir_type, cmp_cfg->type, ANC_ADAPTIVE_CMP_ORDER);

    s8 sz_sign = anc_ext_ear_adaptive_sz_calr_sign_get();

    /* fgq_getfrom_table(cmp_fgq_table, sz_sel - 1, fgq); */
    fgq_table = ch ? ext_cfg->rcmp_mem_iir->mem_iir : ext_cfg->cmp_mem_iir->mem_iir;

    fgq_getfrom_table(fgq_table, sz_sel - 1, fgq);
    //CMP增益符号与SZ符号 负相关
    fgq[0] = (abs(fgq[0])) * (sz_sign) * (-1);


#if 0	//CMP结果输出打印
    g_printf("Global %d/10000\n", (int)(fgq[0] * 10000));
    for (int i = 0; i < ANC_ADAPTIVE_CMP_ORDER; i++) {
        g_printf("SEG[%d]:Type %d, FGQ/10000:%d %d %d\n", i, iir_type[i], \
                 (int)(fgq[i * 3 + 1] * 10000), (int)(fgq[i * 3 + 2] * 10000), \
                 (int)(fgq[i * 3 + 3] * 10000));
    }
#elif ICSD_CMP_OUTPUT_PRINTF
    for (int i = cmp_cfg->iir_num_fix; i < ANC_ADAPTIVE_CMP_ORDER; i++) {
        cmp_log("cmp[%d] %d, %d, %d\n", i, (int)(fgq[i * 3 + 1] * 10000), \
                (int)(fgq[i * 3 + 2] * 10000), (int)(fgq[i * 3 + 3] * 10000));
    }
#endif
    anc_free(cmp_cfg);
}

//CMP sz处理回调，仅有RTANC使用，耳道自适应不适用
static void audio_anc_ear_adaptive_cmp_afq_output_hdl(struct audio_afq_output *p)
{
    u8 ret = 0;
    u8 update_flag = 0;
    if (cmp_hdl) {
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        cmp_log("cmp flag:state %d, l:%d, r:%d\n", audio_anc_real_time_adaptive_state_get(), p->l_cmp_eq_update, p->r_cmp_eq_update);
        cmp_hdl->l_param.update_flag = p->l_cmp_eq_update & AUDIO_ADAPTIVE_CMP_UPDATE_FLAG;
        update_flag |= cmp_hdl->l_param.update_flag;
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        cmp_hdl->r_param.update_flag = p->r_cmp_eq_update & AUDIO_ADAPTIVE_CMP_UPDATE_FLAG;
        update_flag |= cmp_hdl->r_param.update_flag;
#endif
        if ((!audio_anc_real_time_adaptive_state_get()) || (!update_flag)) {
            printf("ICSD_CMP: RTANC_CLOSE by now, exit!\n");
            audio_anc_real_time_adaptive_resume("RTCMP");
            return;
        }
        //优先sz_sel
        if (p->sz_l_sel_idx && cmp_hdl->l_param.update_flag) {
            audio_anc_ear_adaptive_cmp_sz_sel(p->sz_l_sel_idx, cmp_hdl->l_param.output->fgq, cmp_hdl->l_param.type, 0);
        }
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        if (p->sz_r_sel_idx && cmp_hdl->r_param.update_flag) {
            audio_anc_ear_adaptive_cmp_sz_sel(p->sz_r_sel_idx, cmp_hdl->r_param.output->fgq, cmp_hdl->r_param.type, 1);
        }
#endif
        //无sz_sel，滤波器拟合
        if (!(p->sz_l_sel_idx || p->sz_r_sel_idx)) {
            ret = audio_anc_ear_adaptive_cmp_run(p);
        }
        if (!ret) {
            //更新参数，更新工具信息
#if AUDIO_RT_ANC_EXPORT_TOOL_DATA_DEBUG
            audio_rtanc_cmp_data_packet();
#endif
            audio_rtanc_cmp_update();
        } else {
            //异常处理
            cmp_hdl->l_param.output->state = -1;
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
            cmp_hdl->r_param.output->state = -1;
#endif
        }
        audio_anc_real_time_adaptive_resume("RTCMP");
#endif
    }
}

int audio_anc_ear_adaptive_cmp_open(u8 data_from)
{
    if (cmp_hdl) {
        return -1;
    }
    if (!anc_ext_adaptive_cmp_tool_en_get()) {
        return 0;
    }
    cmp_hdl = anc_malloc("ICSD_CMP", sizeof(struct icsd_cmp_hdl));
    enum audio_adaptive_fre_sel fre_sel = AUDIO_ADAPTIVE_FRE_SEL_ANC;

    cmp_log("ICSD_CMP_STATE:OPEN");
    cmp_hdl->state = ANC_EAR_ADAPTIVE_CMP_STATE_OPEN;
    cmp_hdl->data_from = data_from;
    cmp_hdl->l_param.output = anc_malloc("ICSD_CMP_APP", sizeof(_cmp_output));
    cmp_hdl->l_param.output->fgq = anc_malloc("ICSD_CMP_APP", ANC_ADAPTIVE_CMP_OUTPUT_SIZE);
    cmp_log("%p %p\n", cmp_hdl->l_param.output, cmp_hdl->l_param.output->fgq);

#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    cmp_hdl->r_param.output = anc_malloc("ICSD_CMP_APP", sizeof(_cmp_output));
    cmp_hdl->r_param.output->fgq = anc_malloc("ICSD_CMP_APP", ANC_ADAPTIVE_CMP_OUTPUT_SIZE);
#endif

    if (data_from == CMP_FROM_RTANC) {
        audio_afq_common_add_output_handler("ANC_CMP", fre_sel, audio_anc_ear_adaptive_cmp_afq_output_hdl);
    }
    return 0;
}

static void anc_ear_adaptive_cmp_param_free(struct icsd_cmp_param *p)
{
    if (p->coeff) {
        anc_free(p->coeff);
    }
    if (p->output) {
        anc_free(p->output->fgq);
        anc_free(p->output);
    }
    if (p->sz_cmp) {
        anc_free(p->sz_cmp);
    }
}

int audio_anc_ear_adaptive_cmp_close(void)
{
    if (cmp_hdl) {
        cmp_log("ICSD_CMP_STATE:CLOSE");
        cmp_hdl->state = ANC_EAR_ADAPTIVE_CMP_STATE_CLOSE;
        icsd_cmp_force_exit();
        u8 wait_cnt = 10;
        while (cmp_hdl->busy && wait_cnt) {
            os_time_dly(1);
            wait_cnt--;
        }

        if (cmp_hdl->data_from == CMP_FROM_RTANC) {
            audio_afq_common_del_output_handler("ANC_CMP");
        }
        anc_ear_adaptive_cmp_param_free(&cmp_hdl->l_param);
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        anc_ear_adaptive_cmp_param_free(&cmp_hdl->r_param);
#endif
        anc_free(cmp_hdl);
        cmp_hdl = NULL;
        cmp_log("ICSD_CMP_STATE:CLOSE FINISH");
    }
    return 0;
}

struct icsd_cmp_hdl *audio_anc_ear_adaptive_cmp_hdl_get(void)
{
    return cmp_hdl;
}

//获取算法输出
float *audio_anc_ear_adaptive_cmp_output_get(enum ANC_EAR_ADAPTIVE_CMP_CH ch)
{
    if (cmp_hdl) {
        switch (ch) {
        case ANC_EAR_ADAPTIVE_CMP_CH_L:
            if (cmp_hdl->l_param.output) {
                return cmp_hdl->l_param.output->fgq;
            }
            break;
        case ANC_EAR_ADAPTIVE_CMP_CH_R:
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
            if (cmp_hdl->r_param.output) {
                return cmp_hdl->r_param.output->fgq;
            }
#endif
            break;
        }
    }
    return NULL;
}

//获取算法输出SZ CMP
float *audio_anc_ear_adaptive_cmp_sz_cmp_get(enum ANC_EAR_ADAPTIVE_CMP_CH ch)
{
    if (cmp_hdl) {
        switch (ch) {
        case ANC_EAR_ADAPTIVE_CMP_CH_L:
            return cmp_hdl->l_param.sz_cmp;
        case ANC_EAR_ADAPTIVE_CMP_CH_R:
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
            return cmp_hdl->r_param.sz_cmp;
#endif
            break;
        }
    }
    return NULL;
}

u8 *audio_anc_ear_adaptive_cmp_type_get(enum ANC_EAR_ADAPTIVE_CMP_CH ch)
{
    if (cmp_hdl) {
        switch (ch) {
        case ANC_EAR_ADAPTIVE_CMP_CH_L:
            return cmp_hdl->l_param.type;
        case ANC_EAR_ADAPTIVE_CMP_CH_R:
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
            return cmp_hdl->r_param.type;
#endif
            break;
        }
    }
    return NULL;
}

static void audio_rtanc_adaptive_cmp_data_format(struct icsd_cmp_param *p, double *coeff)
{
    u8 *cmp_dat = (u8 *)&p->gain;
    audio_anc_fr_format(cmp_dat, p->output->fgq, ANC_ADAPTIVE_CMP_ORDER, p->type);
    /* if (p->coeff == NULL) { */
    /* p->coeff = anc_malloc("ICSD_CMP_APP", ANC_ADAPTIVE_CMP_ORDER * sizeof(double) * 5); */
    /* } */
    int alogm = audio_anc_gains_alogm_get(ANC_CMP_TYPE);
    audio_anc_biquad2ab_double(p->fr, coeff, ANC_ADAPTIVE_CMP_ORDER, alogm);
}

void audio_rtanc_adaptive_cmp_update_flag_get(struct anc_cmp_param_output *output)
{
    output->l_update_flag = cmp_hdl->l_param.update_flag;
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
    output->r_update_flag = cmp_hdl->r_param.update_flag;
#endif
}

//获取Total gain & IIR COEFF, 外部传参准备好coeff 空间
int audio_rtanc_adaptive_cmp_output_get(struct anc_cmp_param_output *output)
{
    if (cmp_hdl) {
        output->l_update_flag = cmp_hdl->l_param.update_flag;
        if (output->l_update_flag) {
            audio_rtanc_adaptive_cmp_data_format(&cmp_hdl->l_param, output->l_coeff);
            output->l_gain = (cmp_hdl->l_param.gain < 0) ? (0 - cmp_hdl->l_param.gain) : cmp_hdl->l_param.gain;
            /* output->l_coeff = cmp_hdl->l_param.coeff; */
        }
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        output->r_update_flag = cmp_hdl->r_param.update_flag;
        if (output->r_update_flag) {
            audio_rtanc_adaptive_cmp_data_format(&cmp_hdl->r_param, output->r_coeff);
            output->r_gain = (cmp_hdl->r_param.gain < 0) ? (0 - cmp_hdl->r_param.gain) : cmp_hdl->r_param.gain;
            /* output->r_coeff = cmp_hdl->r_param.coeff; */
        }
#endif
    }
    return 0;
}

u8 audio_anc_ear_adaptive_cmp_result_get(void)
{
    u8 result = 0;
    if (cmp_hdl) {
        if (!cmp_hdl->l_param.output->state) {
            result |= ANC_ADAPTIVE_RESULT_LCMP;
        }
#if ANC_CONFIG_RFB_EN && AUDIO_ANC_STEREO_ENABLE
        if (!cmp_hdl->r_param.output->state) {
            result |= ANC_ADAPTIVE_RESULT_RCMP;
        }
#endif
    }
    return result;
}

#endif
#endif


