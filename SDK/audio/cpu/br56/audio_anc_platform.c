/*
 ****************************************************************
 *							AUDIO ANC
 * File  : audio_anc.c
 * By    :
 * Notes : 管理ANC CPU平台特性相关的逻辑及流程
 *
 *
 ****************************************************************
 */

#include "app_config.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#include "adc_file.h"
#include "asm/audio_common.h"

#if TCFG_AUDIO_ANC_CH == (ANC_L_CH | ANC_R_CH)
#if TCFG_AUDIO_ANC_TRAIN_MODE == ANC_FF_EN
const u8 CONST_ANC_WORK_MODE = ANC_STEREO_FF;
#else
const u8 CONST_ANC_WORK_MODE = ANC_STEREO_FB;
#endif

#else	/*TWS*/
const u8 CONST_ANC_WORK_MODE = ANC_MONO_HYBRID;
#endif


static void audio_anc_platform_gains_init(anc_gain_param_t *gains);
static void audio_anc_platform_adc_ch_set(void);

static audio_anc_t *anc_param = NULL;
void audio_anc_platform_param_init(audio_anc_t *param)
{
    if (param) {
        anc_param = param;
#if TCFG_AUDIO_ANC_BASE_DEBUG_ENABLE
        audio_anc_platform_gains_init(&anc_param->gains);
#endif
        //初始化ANC ADC相关参数，注意需在audio_adc_init后调用
        audio_anc_platform_adc_ch_set();
    }
}

void set_anc_gains_alogm(u32 alogm)
{
    if (anc_param) {
        anc_param->gains.ff_alogm = alogm;
        anc_param->gains.fb_alogm = alogm;
        anc_param->gains.cmp_alogm = alogm;
    }
}

u32 audio_anc_gains_alogm_get(enum ANC_IIR_TYPE type)
{
    if (anc_param) {
        switch (type) {
        case ANC_FF_TYPE:
            return anc_param->gains.ff_alogm;
        case ANC_FB_TYPE:
            return anc_param->gains.fb_alogm;
        case ANC_CMP_TYPE:
            return anc_param->gains.cmp_alogm;
        case ANC_TRANS_TYPE:
            return anc_param->gains.trans_alogm;
        }
    }
    return -1;
}

void anc_param_gain_dcc_printf(anc_core_dcc_t *dcc)
{
    printf("dcc norm_dc_par %d\n",	  dcc->norm_dc_par);
    printf("dcc pwr_dc_par  %d\n",	  dcc->pwr_dc_par);
    printf("dcc amp1_dc_par %d\n",	  dcc->amp1_dc_par);
    printf("dcc amp2_dc_par %d\n",	  dcc->amp2_dc_par);
    printf("dcc pwr_thr     %d\n",	  dcc->pwr_thr);
    printf("dcc amp_thr     %d\n",	  dcc->amp_thr);

    printf("dcc adj_dc_par1     %d\n",  dcc->adj_dc_par1);
    printf("dcc adj_dc_par2     %d\n",  dcc->adj_dc_par2);
    printf("dcc adj_incr_step   %d\n",  dcc->adj_incr_step);
    printf("dcc adj_upd_sample  %d\n",  dcc->adj_upd_sample);
}

void anc_param_gain_drc_printf(anc_core_drc_t *drc)
{
    printf("drc threshold %d\n",		  drc->threshold);
    printf("drc ratio     %d\n",		  drc->ratio);
    printf("drc mkg       %d\n",		  drc->mkg);
    printf("drc knee      %d\n",		  drc->knee);
    printf("drc att_time  %d\n",		  drc->att_time);
    printf("drc rls_time  %d\n",		  drc->rls_time);
    printf("drc bypass    %d\n",		  drc->bypass);
}

void audio_anc_platform_gains_printf(u8 cmd)
{
    char *str[2] = {
        "ANC_CFG_READ",
        "ANC_CFG_WRITE"
    };
    anc_gain_param_t *gains = &anc_param->gains;
    printf("-------------anc_param anc_gain_t-%s--------------\n", str[cmd - 1]);
    printf("====================ANC COMMON PARAM=====================\n");
    printf("developer_mode %d\n",    gains->developer_mode);
    printf("dac_gain %d\n", gains->dac_gain);
    printf("l_ffmic_gain %d\n", gains->l_ffmic_gain);
    printf("l_fbmic_gain %d\n", gains->l_fbmic_gain);
    printf("r_ffmic_gain %d\n", gains->r_ffmic_gain);
    printf("r_fbmic_gain %d\n", gains->r_fbmic_gain);
    printf("cmp_en %d\n",		  gains->cmp_en);

    printf("gain_sign 0X%x\n",  gains->gain_sign);
    printf("noise_lvl 0X%x\n",  gains->noise_lvl);
    printf("ff_alogm %d\n",	  gains->ff_alogm);
    printf("fb_alogm %d\n",	  gains->fb_alogm);
    printf("cmp_alogm %d\n",	  gains->cmp_alogm);
    printf("trans_alogm %d\n",  gains->trans_alogm);

    printf("ancl_ffgain %d.%d\n", ((int)(gains->l_ffgain * 100.0)) / 100, ((int)(gains->l_ffgain * 100.0)) % 100);
    printf("ancl_fbgain %d.%d\n", ((int)(gains->l_fbgain * 100.0)) / 100, ((int)(gains->l_fbgain * 100.0)) % 100);
    printf("ancl_trans_gain %d.%d\n", ((int)(gains->l_transgain * 100.0)) / 100, ((int)(gains->l_transgain * 100.0)) % 100);
    printf("ancl_cmpgain %d.%d\n", ((int)(gains->l_cmpgain * 100.0)) / 100, ((int)(gains->l_cmpgain * 100.0)) % 100);
    printf("ancr_ffgain %d.%d\n", ((int)(gains->r_ffgain * 100.0)) / 100, ((int)(gains->r_ffgain * 100.0)) % 100);
    printf("ancr_fbgain %d.%d\n", ((int)(gains->r_fbgain * 100.0)) / 100, ((int)(gains->r_fbgain * 100.0)) % 100);
    printf("ancr_trans_gain %d.%d\n", ((int)(gains->r_transgain * 100.0)) / 100, ((int)(gains->r_transgain * 100.0)) % 100);
    printf("ancr_cmpgain %d.%d\n", ((int)(gains->r_cmpgain * 100.0)) / 100, ((int)(gains->r_cmpgain * 100.0)) % 100);

    printf("audio_drc_thr %d/10\n", (int)(gains->audio_drc_thr * 10.0));

    printf("====================ANC AHS PARAM=====================\n");
    printf("ahs_en %d\n",		 gains->ahs_en);
    printf("ahs_dly %d\n",     gains->ahs_dly);
    printf("ahs_tap %d\n",     gains->ahs_tap);
    printf("ahs_wn_shift %d\n", gains->ahs_wn_shift);
    printf("ahs_wn_sub %d\n",  gains->ahs_wn_sub);
    printf("ahs_shift %d\n",   gains->ahs_shift);
    printf("ahs_u %d\n",       gains->ahs_u);
    printf("ahs_gain %d\n",    gains->ahs_gain);
    printf("ahs_nlms_sel %d\n", gains->ahs_nlms_sel);


    printf("==================ANC ADAPTIVE PARAM==================\n");
    printf("adaptive_ref_en %d\n", gains->adaptive_ref_en);
    printf("adaptive_ref_fb_f %d/10\n", (int)(gains->adaptive_ref_fb_f * 10.0));
    printf("adaptive_ref_fb_g %d/10\n", (int)(gains->adaptive_ref_fb_g * 10.0));
    printf("adaptive_ref_fb_q %d/10\n", (int)(gains->adaptive_ref_fb_q * 10.0));

    printf("==================ANC MIC CMP PARAM==================\n");
    printf("mic_cmp_gain_en %d\n", gains->mic_cmp_gain_en);
    printf("l_ffmic_cmp_gain %d/10\n", (int)(gains->l_ffmic_cmp_gain * 10.0));
    printf("l_fbmic_cmp_gain %d/10\n", (int)(gains->l_fbmic_cmp_gain * 10.0));
    printf("r_ffmic_cmp_gain %d/10\n", (int)(gains->r_ffmic_cmp_gain * 10.0));
    printf("r_fbmic_cmp_gain %d/10\n", (int)(gains->r_fbmic_cmp_gain * 10.0));

    printf("====================ANC DRC PARAM=====================\n");
    printf("drc_en %d\n",  	  gains->drc_en);

    printf("<DRC_TOP_FF>\n");
    anc_param_gain_drc_printf(&gains->drc_top_ff);
    printf("<DRC_TOP_FB>\n");
    anc_param_gain_drc_printf(&gains->drc_top_fb);
    printf("<DRC_TOP_TRANS>\n");
    anc_param_gain_drc_printf(&gains->drc_top_trans);
    printf("<DRC_CORE_FF>\n");
    anc_param_gain_drc_printf(&gains->drc_core_ff);
    printf("<DRC_CORE_FB>\n");
    anc_param_gain_drc_printf(&gains->drc_core_fb);
    printf("<DRC_CORE_TRANS>\n");
    anc_param_gain_drc_printf(&gains->drc_core_trans);
    printf("<DRC_DAC_MUX>\n");
    anc_param_gain_drc_printf(&gains->drc_dac_mux);

    printf("====================ANC DCC PARAM=====================\n");
    printf("dcc_mode %d\n",  	  gains->dcc_mode);
    printf("<DCC_FF>\n");
    anc_param_gain_dcc_printf(&gains->dcc_ff);
    printf("<DCC_FB>\n");
    anc_param_gain_dcc_printf(&gains->dcc_fb);
}

void audio_anc_platform_task_msg(int *msg)
{
}

void audio_anc_platform_clk_update(void)
{
    audio_anc_common_param_init();			//初始化Audio ANC common 参数
    if (anc_real_mode_get() != ANC_OFF) {	//实时更新填入使用
        audio_common_anc_clock_open();		//更新ANC时钟配置
        audio_common_dac_cic_update();		//更新DAC CIC配置
        audio_common_dac_drc_update();		//更新DAC DRC配置
    }
}

static void audio_anc_platform_gains_init(anc_gain_param_t *gains)
{
    gains->version = ANC_GAINS_VERSION;
    gains->l_ffmic_gain = 0;
    gains->l_fbmic_gain = 0;
    gains->r_ffmic_gain = 0;
    gains->r_fbmic_gain = 0;
    gains->cmp_en = 1;
    gains->gain_sign = 0;
    gains->ff_alogm = ANC_ALOGM6;
    gains->fb_alogm = ANC_ALOGM6;
    gains->cmp_alogm = ANC_ALOGM6;
    gains->trans_alogm = ANC_ALOGM6;

    //ANC IIR增益初始化
    gains->l_ffgain = 1.0f;
    gains->l_fbgain = 1.0f;
    gains->l_cmpgain = 1.0f;
    gains->l_transgain = 1.0f;
    gains->r_ffgain = 1.0f;
    gains->r_fbgain = 1.0f;
    gains->r_cmpgain = 1.0f;
    gains->r_transgain = 1.0f;

    //MIC CMP补偿值初始化
    gains->mic_cmp_gain_en = 0;
    gains->l_ffmic_cmp_gain = 1.0f;
    gains->l_fbmic_cmp_gain = 1.0f;
    gains->r_ffmic_cmp_gain = 1.0f;
    gains->r_fbmic_cmp_gain = 1.0f;

    //DRC配置
    gains->drc_en = 0;	//DRC总使能
    //DRC FF IIR before
    gains->drc_top_ff.threshold = 531;
    gains->drc_top_ff.ratio = 255;
    gains->drc_top_ff.mkg = 0;
    gains->drc_top_ff.knee = 64;
    gains->drc_top_ff.att_time = 3;
    gains->drc_top_ff.rls_time = 13;
    gains->drc_top_ff.bypass = 1;

    //DRC FB IIR before
    gains->drc_top_fb.threshold = 531;
    gains->drc_top_fb.ratio = 255;
    gains->drc_top_fb.mkg = 0;
    gains->drc_top_fb.knee = 64;
    gains->drc_top_fb.att_time = 3;
    gains->drc_top_fb.rls_time = 13;
    gains->drc_top_fb.bypass = 1;

    //DRC TRANS IIR befo->e
    gains->drc_top_trans.threshold = 531;
    gains->drc_top_trans.ratio = 255;
    gains->drc_top_trans.mkg = 0;
    gains->drc_top_trans.knee = 64;
    gains->drc_top_trans.att_time = 3;
    gains->drc_top_trans.rls_time = 13;
    gains->drc_top_trans.bypass = 1;

    //DRC FF IIR after
    gains->drc_core_ff.threshold = 531;
    gains->drc_core_ff.ratio = 255;
    gains->drc_core_ff.mkg = 0;
    gains->drc_core_ff.knee = 64;
    gains->drc_core_ff.att_time = 3;
    gains->drc_core_ff.rls_time = 13;
    gains->drc_core_ff.bypass = 1;

    //DRC FB IIR after
    gains->drc_core_fb.threshold = 531;
    gains->drc_core_fb.ratio = 255;
    gains->drc_core_fb.mkg = 0;
    gains->drc_core_fb.knee = 64;
    gains->drc_core_fb.att_time = 3;
    gains->drc_core_fb.rls_time = 13;
    gains->drc_core_fb.bypass = 1;

    //DRC TRANS IIR afte->
    gains->drc_core_trans.threshold = 531;
    gains->drc_core_trans.ratio = 255;
    gains->drc_core_trans.mkg = 0;
    gains->drc_core_trans.knee = 64;
    gains->drc_core_trans.att_time = 3;
    gains->drc_core_trans.rls_time = 13;
    gains->drc_core_trans.bypass = 1;

    //DRC DAC MIX
    gains->drc_dac_mux.threshold = 531;
    gains->drc_dac_mux.ratio = 255;
    gains->drc_dac_mux.mkg = 0;
    gains->drc_dac_mux.knee = 64;
    gains->drc_dac_mux.att_time = 3;
    gains->drc_dac_mux.rls_time = 13;
    gains->drc_dac_mux.bypass = 1;

    //DCC 模式控制
    gains->dcc_mode = 0;
    //DCC FF
    gains->dcc_ff.norm_dc_par = 0;
    gains->dcc_ff.pwr_dc_par = 2;
    gains->dcc_ff.amp1_dc_par = 0;
    gains->dcc_ff.amp2_dc_par = 6;
    gains->dcc_ff.pwr_thr = 12000;
    gains->dcc_ff.amp_thr = 12000;
    gains->dcc_ff.adj_dc_par1 = 0;
    gains->dcc_ff.adj_dc_par2 = 6;
    gains->dcc_ff.adj_incr_step = 0;
    gains->dcc_ff.adj_upd_sample = 0;

    //DCC FF
    gains->dcc_fb.norm_dc_par = 0;
    gains->dcc_fb.pwr_dc_par = 2;
    gains->dcc_fb.amp1_dc_par = 0;
    gains->dcc_fb.amp2_dc_par = 6;
    gains->dcc_fb.pwr_thr = 12000;
    gains->dcc_fb.amp_thr = 12000;
    gains->dcc_fb.adj_dc_par1 = 0;
    gains->dcc_fb.adj_dc_par2 = 6;
    gains->dcc_fb.adj_incr_step = 0;
    gains->dcc_fb.adj_upd_sample = 0;

    gains->ahs_en = 0;
    gains->ahs_dly = 1;
    gains->ahs_tap = 100;
    gains->ahs_wn_shift = 9;
    gains->ahs_wn_sub = 1;
    gains->ahs_shift = 210;
    gains->ahs_u = 4000;
    gains->ahs_gain = -1024;
    gains->ahs_nlms_sel = 0;

    gains->audio_drc_thr = -6.0f;

    gains->hd_en = 0;
    gains->hd_corr_thr = 232;
    gains->hd_corr_gain = 256;
    gains->hd_corr_dly = 13;
    gains->hd_pwr_rate = 2;
    gains->hd_pwr_ctl_gain_en = 0;
    gains->hd_pwr_ctl_ahsrst_en = 0;
    gains->hd_pwr_thr = 18000;
    gains->hd_pwr_ctl_gain = 1638;

    gains->hd_pwr_ref_ctl_en = 0;
    gains->hd_pwr_ref_ctl_hthr = 2000;
    gains->hd_pwr_ref_ctl_lthr1 = 1000;
    gains->hd_pwr_ref_ctl_lthr2 = 200;

    gains->hd_pwr_err_ctl_en = 0;
    gains->hd_pwr_err_ctl_hthr = 2000;
    gains->hd_pwr_err_ctl_lthr1 = 1000;
    gains->hd_pwr_err_ctl_lthr2 = 200;

    gains->adaptive_ref_en = 0;
    gains->adaptive_ref_fb_f = 120.0;
    gains->adaptive_ref_fb_g = 15.0;
    gains->adaptive_ref_fb_q = 0.4;
}

static void audio_anc_platform_adc_ch_set(void)
{
    struct adc_file_cfg *cfg = audio_adc_file_get_cfg();
    struct adc_platform_cfg *platform_cfg = audio_adc_platform_get_cfg();

#if ANC_MIC_REUSE_ENABLE
    anc_param->mic_param[0].mic_p.mic_mode      = ANC_ADC0_MODE;
    anc_param->mic_param[0].mic_p.mic_ain_sel   = ANC_ADC0_AIN_SEL;
    anc_param->mic_param[0].mic_p.mic_bias_sel  = ANC_ADC0_BIAS_SEL;
    anc_param->mic_param[0].mic_p.mic_bias_rsel = ANC_ADC0_BIAS_RSEL;
    anc_param->mic_param[0].mic_p.mic_dcc       = ANC_ADC0_DCC_EN;
    anc_param->mic_param[0].mic_p.mic_dcc_en    = ANC_ADC0_DCC_LEVEL;
    anc_param->mic_param[0].pre_gain    		= 0;
    anc_param->mic_param[1].mic_p.mic_mode      = ANC_ADC1_MODE;
    anc_param->mic_param[1].mic_p.mic_ain_sel   = ANC_ADC1_AIN_SEL;
    anc_param->mic_param[1].mic_p.mic_bias_sel  = ANC_ADC1_BIAS_SEL;
    anc_param->mic_param[1].mic_p.mic_bias_rsel = ANC_ADC1_BIAS_RSEL;
    anc_param->mic_param[1].mic_p.mic_dcc       = ANC_ADC1_DCC_EN;
    anc_param->mic_param[1].mic_p.mic_dcc_en    = ANC_ADC1_DCC_LEVEL;
    anc_param->mic_param[1].pre_gain    		= 0;
#else
    for (int i = 0; i < AUDIO_ADC_MAX_NUM; i++) {
        anc_param->mic_param[i].mic_p.mic_mode      = platform_cfg[i].mic_mode;
        anc_param->mic_param[i].mic_p.mic_ain_sel   = platform_cfg[i].mic_ain_sel;
        anc_param->mic_param[i].mic_p.mic_bias_sel  = platform_cfg[i].mic_bias_sel;
        anc_param->mic_param[i].mic_p.mic_bias_rsel = platform_cfg[i].mic_bias_rsel;
        anc_param->mic_param[i].mic_p.mic_dcc       = platform_cfg[i].mic_dcc;
        anc_param->mic_param[i].mic_p.mic_dcc_en    = platform_cfg[i].mic_dcc_en;
        anc_param->mic_param[i].pre_gain    		= cfg->param[i].mic_pre_gain;
    }
#endif
}


#endif

