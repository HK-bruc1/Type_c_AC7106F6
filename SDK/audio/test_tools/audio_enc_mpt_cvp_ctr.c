#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_enc_mpt_cvp_ctr.data.bss")
#pragma data_seg(".audio_enc_mpt_cvp_ctr.data")
#pragma const_seg(".audio_enc_mpt_cvp_ctr.text.const")
#pragma code_seg(".audio_enc_mpt_cvp_ctr.text")
#endif
/*
 ****************************************************************
 *			Audio Enc Mass production test self CVP control
 * File  : audio_enc_mpt_cvp_crl.c
 * note  : 自研ENC产测 CVP通话算法/MIC 控制层
 ****************************************************************
 */
#include "cpu/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "cvp_user.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_enc_mpt_self.h"
#include "overlay_code.h"
#include "adc_file.h"
#include "cvp_node.h"
#include "cvp_develop_node.h"
#include "esco_recoder.h"
#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif

#if AUDIO_ENC_MPT_SELF_ENABLE

#define DUT_CVP_LOG_ENABLE
#ifdef DUT_CVP_LOG_ENABLE
#define dut_cvp_log(x, ...)  printf("[enc_mpt_cvp]" x " ", ## __VA_ARGS__)
#else
#define dut_cvp_log(...)
#endif/*DUT_CVP_LOG_ENABLE*/

/*
   暂不支持开ANC模式测试
  1没有与ANC复用ADC BUFF
  2没有开动态MIC增益
*/


extern struct adc_platform_data adc_data;
extern struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;
extern const u16 audio_cvp_uuid_table[12];
extern void clock_refurbish(void);

#define MIC_BUF_NUM        	3
#define MIC_IRQ_POINTS     	256

#define CVP_MIC_SR			16000	//采样率

#define CVP_MONITOR_DIS		0		//监听关闭
#define CVP_MONITOR_PROBE	1		//监听处理前数据
#define CVP_MONITOR_POST	2		//监听处理后数据
//默认监听选择
#define CVP_MONITOR_SEL		CVP_MONITOR_DIS

#define AUDIO_ENC_MPT_MIC_NUM			AUDIO_ADC_MAX_NUM		//最大ADC个数


typedef struct {
    u8 dump_cnt;
    u8 monitor;
    u8 mic_num;	//当前MIC个数
#if TCFG_AUDIO_CVP_DEVELOP_ENABLE
    u8 dev_mic_id[CVP_DEV_MAX_MIC_NUM];
#endif
    u16 mic_id[AUDIO_ENC_MPT_MIC_NUM];
    u16 dut_mic_ch;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 *tmp_buf[AUDIO_ENC_MPT_MIC_NUM - 1];
    s16 *mic_buf;
} audio_cvp_t;
static audio_cvp_t *cvp_hdl = NULL;

/*	写卡使能控制, 需注意一下几点
	1、需打开 AUDIO_DATA_EXPORT_VIA_UART
	2、AEC原本的写卡流程需关闭
	3、测试流程需包含CVP_OUT测试, 因为写卡只能在线程写
*/
#define	AUDIO_ENC_MPT_UART_DEBUG_EN		0
#define AUDIO_ENC_MPT_UART_DEBUG_CH		3						//写卡通道数
//写卡长度
#define AUDIO_ENC_MPT_UART_DEBUG_LEN	(MIC_IRQ_POINTS * 2 + 20) * AUDIO_ENC_MPT_UART_DEBUG_CH

#if ((defined TCFG_AUDIO_DATA_EXPORT_DEFINE) && (TCFG_AUDIO_DATA_EXPORT_DEFINE == AUDIO_DATA_EXPORT_VIA_UART))
#include "aec_uart_debug.h"
#else
#undef AUDIO_ENC_MPT_UART_DEBUG_EN
#define AUDIO_ENC_MPT_UART_DEBUG_EN     0
#endif

static u16 audio_enc_mpt_cvp_uuid_get(void);

/*监听输出：默认输出到dac*/
static int cvp_monitor_output(s16 *data, int len)
{
    int wlen = audio_dac_write(&dac_hdl, data, len);
    if (wlen != len) {
        dut_cvp_log("monitor output full\n");
    }
    return wlen;
}

/*监听使能*/
static int cvp_monitor_en(u8 en, int sr)
{
    dut_cvp_log("cvp_monitor_en:%d,sr:%d\n", en, sr);
    if (en) {
        app_audio_state_switch(APP_AUDIO_STATE_MUSIC, app_audio_volume_max_query(AppVol_BT_MUSIC), NULL);	// 音量状态设置
        audio_dac_set_volume(&dac_hdl, app_audio_volume_max_query(AppVol_BT_MUSIC));					// dac 音量设置
        audio_dac_set_sample_rate(&dac_hdl, sr);
        audio_dac_start(&dac_hdl);
        audio_dac_channel_start(NULL);
    } else {
        audio_dac_stop(&dac_hdl);
        audio_dac_close(&dac_hdl);
    }
    return 0;
}

/*
	测试前的预处理,
   	如方案使用模拟开关复用ADC通道，可以在此切换模拟开关
 */
static void audio_enc_mpt_fre_pre(u16 mic_ch)
{
#if 0
    //测试算法或者TALK 主MIC
    if (mic_ch & (AUDIO_ENC_MPT_CVP_OUT | AUDIO_ENC_MPT_TALK_MIC)) {
    }
    //测试算法或者TALK 副MIC
    if (mic_ch & (AUDIO_ENC_MPT_CVP_OUT | AUDIO_ENC_MPT_TALK_FF_MIC)) {
    }
    //测试算法或者TALK FBMIC
    if (mic_ch & (AUDIO_ENC_MPT_CVP_OUT | AUDIO_ENC_MPT_TALK_FB_MIC)) {
    }
    //测试算法或者VPU
    if (mic_ch & (AUDIO_ENC_MPT_CVP_OUT | AUDIO_ENC_MPT_TALK_VPU)) {
    }
    //测试TWS FFMIC or 头戴式LFF MIC
    if (mic_ch & AUDIO_ENC_MPT_FF_MIC) {
    }
    //测试TWS FBMIC or 头戴式LFB MIC
    if (mic_ch & AUDIO_ENC_MPT_FB_MIC) {
    }
    //测试头戴式RFF MIC
    if (mic_ch & AUDIO_ENC_MPT_RFF_MIC) {
    }
    //测试头戴式RFB MIC
    if (mic_ch & AUDIO_ENC_MPT_RFB_MIC) {
    }
#endif
}

/*mic adc原始数据输出*/
static void mic_output(void *priv, s16 *data, int len)
{
    /* putchar('.'); */
    s16 *mic_data[cvp_hdl->mic_num];
    int i, index;
    u8 talk_id_cnt = 0xff;
    //ADC启动不稳定，实测开头大约有50ms 杂音数据, 这里丢弃掉16*8 = 128ms
#if (defined CONFIG_CPU_BR50)
    if (cvp_hdl->dump_cnt < 8 + 20) { 	//BR50 ADC稳定需要28*16 = 448ms  后续可以优化
#else
    if (cvp_hdl->dump_cnt < 8) {
#endif
        cvp_hdl->dump_cnt++;
        return;
    }

    //printf("[%5d],[%5d],[%5d],[%5d]", data[0], data[1], data[2], data[3]);
    mic_data[0] = data;
    //printf("mic_data:%x,%x,%d\n",data,mic1_data_pos,len);
    if (cvp_hdl->mic_num > 1) {
        for (index = 1; index < cvp_hdl->mic_num; index++) {
            mic_data[index] =  cvp_hdl->tmp_buf[index - 1];
        }
        for (i = 0; i < (len >> 1); i++) {
            for (index = 0; index < cvp_hdl->mic_num; index++) {
                mic_data[index][i] =  data[i * cvp_hdl->mic_num + index];
            }
        }
    }


#if TCFG_AUDIO_CVP_V3_MODE	//CVP_V3算法
    u32 algo_type = cvp_get_algo_type();
    audio_cvp_lock();
    for (i = 0; i < cvp_hdl->mic_num; i++) {
#if AUDIO_ENC_MPT_UART_DEBUG_EN
        if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
            aec_uart_fill_v2(i, mic_data[i], 512);
        }
#endif/*AUDIO_ENC_MPT_UART_DEBUG_EN*/
        audio_enc_mpt_fre_response_inbuf(cvp_hdl->mic_id[i], mic_data[i], len);
        if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
#if CVP_V3_3MIC_ALGO_ENABLE
            if (algo_type & CVP_V3_3MIC_MASK) {
                if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_FB_MIC) {
                    audio_cvp_v3_fb_mic_push(mic_data[i], len);
                    continue;
                }
            }
#endif
#if CVP_V3_2MIC_ALGO_ENABLE || CVP_V3_3MIC_ALGO_ENABLE
            if (algo_type & (CVP_V3_2MIC_MASK | CVP_V3_3MIC_MASK)) {
                if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_FF_MIC) {
                    audio_cvp_v3_ff_mic_push(mic_data[i], len);
                    continue;
                }
            }
#endif
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_MIC) {
                talk_id_cnt = i;
                /* audio_aec_inbuf(mic_data[i], len); */
                continue;
            }
        }
    }
    //主麦数据输入触发AEC线程，最后处理
    if (talk_id_cnt != 0xff) {	//表示有算法主麦输入
        //cvp 参考数据
        audio_cvp_phase_align();

        audio_cvp_v3_talk_mic_push(mic_data[talk_id_cnt], len);
    }
    audio_cvp_unlock();

#elif TCFG_AUDIO_CVP_DEVELOP_ENABLE	/*第三方算法通道映射*/
    u8 *dev_mic_id = cvp_hdl->dev_mic_id;
    for (i = 0; i < cvp_hdl->mic_num; i++) {
#if AUDIO_ENC_MPT_UART_DEBUG_EN
        if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
            aec_uart_fill_v2(i, mic_data[i], 512);
        }
#endif/*AUDIO_ENC_MPT_UART_DEBUG_EN*/
        audio_enc_mpt_fre_response_inbuf(cvp_hdl->mic_id[i], mic_data[i], len);
    }

    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
        if (dev_mic_id[1] != 0xff) {
            audio_aec_inbuf_ref(mic_data[dev_mic_id[1]], len);
        }
        if (dev_mic_id[2] != 0xff) {
            audio_aec_inbuf_ref_1(mic_data[dev_mic_id[2]], len);
        }
        if (dev_mic_id[3] != 0xff) {
            audio_aec_inbuf_ref_2(mic_data[dev_mic_id[3]], len);
        }
        if (dev_mic_id[0] != 0xff) {
            audio_cvp_phase_align();
            audio_aec_inbuf(mic_data[dev_mic_id[0]], len);
        }
    }

#else	//普通单/双/三MIC算法

    for (i = 0; i < cvp_hdl->mic_num; i++) {
#if AUDIO_ENC_MPT_UART_DEBUG_EN
        if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
            aec_uart_fill_v2(i, mic_data[i], 512);
        }
#endif/*AUDIO_ENC_MPT_UART_DEBUG_EN*/
        audio_enc_mpt_fre_response_inbuf(cvp_hdl->mic_id[i], mic_data[i], len);
        if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
#if TCFG_AUDIO_TRIPLE_MIC_ENABLE
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_FB_MIC) {
                audio_aec_inbuf_ref_1(mic_data[i], len);
                continue;
            }
#endif/*TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
#if TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_FF_MIC) {
                audio_aec_inbuf_ref(mic_data[i], len);
                continue;
            }
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE || TCFG_AUDIO_TRIPLE_MIC_ENABLE*/
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_MIC) {
                talk_id_cnt = i;
                /* audio_aec_inbuf(mic_data[i], len); */
                continue;
            }
        }
    }

    //主麦数据输入触发AEC线程，最后处理
    if (talk_id_cnt != 0xff) {	//表示有算法主麦输入
        //cvp 参考数据
        audio_cvp_phase_align();

        audio_aec_inbuf(mic_data[talk_id_cnt], len);
    }
#endif

#if CVP_MONITOR_SEL != CVP_MONITOR_DIS
    if (cvp_hdl->monitor == CVP_MONITOR_PROBE) {
        static u8 num = 0;
        static int cnt = 0;
        if (++cnt > 300) {
            cnt = 0;
            if (++num == cvp_hdl->mic_num) {
                num = 0;
            }
            r_printf("num %d", num);
        }
        cvp_monitor_output(mic_data[num], len);
    }
#endif/*CVP_MONITOR_SEL != CVP_MONITOR_DIS*/
}

static u16 audio_enc_mpt_anc_mic_query(u8 mic_type, u16 mic_id, u8 *mic_en, u16 *mic_id_tmp, u8 *gain)
{
#if TCFG_AUDIO_ANC_ENABLE
    u8 i;
    //遍历最大模拟ADC个数
    for (i = A_MIC0; i < AUDIO_ENC_MPT_MIC_NUM; i++) {
        if (mic_type == i) {
            if (!mic_en[i]) {
                //此MIC没有开过，则使用ANC的MIC增益
                gain[i] = audio_anc_mic_gain_get(i);
            }
            mic_en[i] = 1;
            mic_id_tmp[i] |= mic_id;
            dut_cvp_log("ANC mic type %d, id %x, en %x", mic_type, mic_id, mic_en[i]);
            return 0;
        }
    }
#endif/*TCFG_AUDIO_ANC_ENABLE*/
    return 1;
}

static u8 bit_convert_to_num(int bit)
{
    u8 num = 0;
    if (bit) {
        while (bit != 1) {
            bit >>= 1;
            num++;
        }
    }
    return num;
}

#if TCFG_AUDIO_CVP_DEVELOP_ENABLE
//第三方通话算法MIC排序, 严格按照TALK / FF / FB / VPU 排序对应MIC，方便中断执行对应inbuf()
static void cvp_dev_mic_id_sort(u8 *mic_en)
{
    int i = 0;
    u8 mic_cnt = 0;
    //赋值0xff, 用于检查MIC是否启动
    memset(cvp_hdl->dev_mic_id, 0xff, sizeof(cvp_hdl->dev_mic_id));
    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
        for (i = 0; i < cvp_hdl->mic_num; i++) {
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_MIC) {
                cvp_hdl->dev_mic_id[mic_cnt++] = i;
            }
        }
        for (i = 0; i < cvp_hdl->mic_num; i++) {
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_FF_MIC) {
                cvp_hdl->dev_mic_id[mic_cnt++] = i;
                break;
            }
        }
        for (i = 0; i < cvp_hdl->mic_num; i++) {
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_FB_MIC) {
                cvp_hdl->dev_mic_id[mic_cnt++] = i;
                break;
            }
        }
        for (i = 0; i < cvp_hdl->mic_num; i++) {
            if (cvp_hdl->mic_id[i] & AUDIO_ENC_MPT_TALK_VPU) {
                cvp_hdl->dev_mic_id[mic_cnt++] = i;
                break;
            }
        }
    }
    printf("cvp_dev_mic_id_sort %d\n", (int)sizeof(cvp_hdl->dev_mic_id));
    put_buf(cvp_hdl->dev_mic_id, sizeof(cvp_hdl->dev_mic_id));
}
#endif

static const u16 call_mic_id[2] = {AUDIO_ENC_MPT_TALK_MIC, AUDIO_ENC_MPT_TALK_FF_MIC};
static int mic_open(u8 *mic_gain, int sr)
{
    int i, j, index;
    u8 mic_en[AUDIO_ENC_MPT_MIC_NUM] = {0};
    u16 mic_id_tmp[AUDIO_ENC_MPT_MIC_NUM] = {0};

    /*读取adc配置*/
    audio_adc_file_init();
    /*获取通话adc通道*/
    u8 mic_ch = audio_adc_file_get_mic_en_map();
    /*获取通话mic增益*/
    dut_cvp_log("mic ch %x, mic open sr: %d\n", mic_ch, sr);
    for (index = 0; index < AUDIO_ENC_MPT_MIC_NUM; index++) {
        mic_gain[index] = audio_adc_file_get_gain(index);
        dut_cvp_log("mic%d_gain:%d \n", index, mic_gain[index]);
    }

    cvp_hdl->dump_cnt = 0;
    //1.测试CVP的输出 or 通话MIC相关 MIC输出
    if (cvp_hdl->dut_mic_ch & (AUDIO_ENC_MPT_CVP_OUT   | AUDIO_ENC_MPT_TALK_MIC    | \
                               AUDIO_ENC_MPT_TALK_FF_MIC | AUDIO_ENC_MPT_TALK_FB_MIC | \
                               AUDIO_ENC_MPT_TALK_VPU)) {

        for (index = 0; index < AUDIO_ENC_MPT_MIC_NUM; index++) {
            if (mic_ch & BIT(index)) {
                mic_en[index] = 1;
                mic_id_tmp[index] = 1;
            }
        }

        //1.1 确定主MIC/副MIC/FB MIC
#if TCFG_AUDIO_CVP_V3_MODE	        /*CVP_V3算法*/
        u32 algo_type = cvp_get_algo_type();
        dut_cvp_log("algo_type 0x%x\n", algo_type);

        u8 talk_mic = cvp_v3_get_talk_mic_ch();
        mic_id_tmp[bit_convert_to_num(talk_mic)] = AUDIO_ENC_MPT_TALK_MIC;
        dut_cvp_log("talk_mic %d", talk_mic);

        if (algo_type & (CVP_V3_2MIC_MASK | CVP_V3_3MIC_MASK)) {
            u8 talk_ref_mic = cvp_v3_get_talk_ref_mic_ch();
            mic_id_tmp[bit_convert_to_num(talk_ref_mic)] = AUDIO_ENC_MPT_TALK_FF_MIC;
            dut_cvp_log("talk_ref_mic %d", talk_ref_mic);
        }
        if (algo_type & CVP_V3_3MIC_MASK) {
            u8 talk_fb_mic = cvp_v3_get_talk_fb_mic_ch();
            mic_id_tmp[bit_convert_to_num(talk_fb_mic)] = AUDIO_ENC_MPT_TALK_FB_MIC;
            dut_cvp_log("talk_fb_mic %d", talk_fb_mic);
        }

#elif TCFG_AUDIO_DUAL_MIC_ENABLE	/*双麦通道映射*/
        u8 talk_mic = cvp_get_talk_mic_ch();
        u8 talk_ref_mic = cvp_get_talk_ref_mic_ch();
        dut_cvp_log("talk_mic %d, talk_ref_mic %d", talk_mic, talk_ref_mic);
        mic_id_tmp[bit_convert_to_num(talk_mic)] = AUDIO_ENC_MPT_TALK_MIC;
        mic_id_tmp[bit_convert_to_num(talk_ref_mic)] = AUDIO_ENC_MPT_TALK_FF_MIC;

#elif TCFG_AUDIO_TRIPLE_MIC_ENABLE	/*3麦通道映射*/
        u8 talk_mic = cvp_get_talk_mic_ch();
        u8 talk_ref_mic = cvp_get_talk_ref_mic_ch();
        u8 talk_fb_mic = cvp_get_talk_fb_mic_ch();
        dut_cvp_log("talk_mic %d, talk_ref_mic %d, talk_fb_mic %d", talk_mic, talk_ref_mic, talk_fb_mic);
        mic_id_tmp[bit_convert_to_num(talk_mic)] = AUDIO_ENC_MPT_TALK_MIC;
        mic_id_tmp[bit_convert_to_num(talk_ref_mic)] = AUDIO_ENC_MPT_TALK_FF_MIC;
        mic_id_tmp[bit_convert_to_num(talk_fb_mic)] = AUDIO_ENC_MPT_TALK_FB_MIC;

#elif TCFG_AUDIO_CVP_DEVELOP_ENABLE	/*第三方算法通道映射*/
        struct cvp_dev_cfg_t *cfg = cvp_dev_cfg_get();
        mic_id_tmp[bit_convert_to_num(cfg->talk_mic)] = AUDIO_ENC_MPT_TALK_MIC;
        if (audio_adc_file_get_esco_mic_num() != cfg->mic_num) {
            ASSERT(0, "CVP_develop ESCO MIC num is %d != %d\n", audio_adc_file_get_esco_mic_num(), cfg->mic_num);
        }
        if (cfg->vpu_en) {
            ASSERT(CVP_DEV_IS_VPU_ADC_MODE(cfg->vpu_ch_sel), " Self-test only supports ADC mode, %d", cfg->vpu_ch_sel);
            mic_id_tmp[bit_convert_to_num(cfg->vpu_ch_sel)] = AUDIO_ENC_MPT_TALK_VPU;
        }
        if (cfg->mic_type & CVP_DEV_TALK_MIC_CH) {
            mic_id_tmp[bit_convert_to_num(cfg->talk_mic)] = AUDIO_ENC_MPT_TALK_MIC;
        }
        if (cfg->mic_type & CVP_DEV_FF_MIC_CH) {
            mic_id_tmp[bit_convert_to_num(cfg->talk_ff_mic)] = AUDIO_ENC_MPT_TALK_FF_MIC;
        }
        if (cfg->mic_type & CVP_DEV_FB_MIC_CH) {
            mic_id_tmp[bit_convert_to_num(cfg->talk_fb_mic)] = AUDIO_ENC_MPT_TALK_FB_MIC;
        }
#elif TCFG_AUDIO_SINGLE_MIC_ENABLE
        for (i = 0; i < AUDIO_ENC_MPT_MIC_NUM; i++) {
            if ((mic_ch >> i) & 0x01) {
                mic_id_tmp[i] = AUDIO_ENC_MPT_TALK_MIC;
            }
        }
#endif/*TCFG_AUDIO_DUAL_MIC_ENABLE*/

    }
    //2.只测通话MIC，不测试算法输出
    if (!(cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT)) {
        for (i = 0; i < AUDIO_ENC_MPT_MIC_NUM; i++) {
            //清除非目标MIC的使能
            if (!(cvp_hdl->dut_mic_ch & mic_id_tmp[i])) {
                mic_id_tmp[i] = 0;
                mic_en[i] = 0;
            }
        }
    }

    //3.ANC MIC映射
#if TCFG_AUDIO_ANC_ENABLE
    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_FF_MIC) {
#if TCFG_USER_TWS_ENABLE && (TCFG_AUDIO_ANC_CH == ANC_R_CH)
        audio_enc_mpt_anc_mic_query(TCFG_AUDIO_ANCR_FF_MIC, AUDIO_ENC_MPT_FF_MIC, mic_en, mic_id_tmp, mic_gain);
#else
        audio_enc_mpt_anc_mic_query(TCFG_AUDIO_ANCL_FF_MIC, AUDIO_ENC_MPT_FF_MIC, mic_en, mic_id_tmp, mic_gain);
#endif/*TCFG_USER_TWS_ENABLE && (TCFG_AUDIO_ANC_CH == ANC_R_CH)*/
    }
    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_FB_MIC) {
#if TCFG_USER_TWS_ENABLE && (TCFG_AUDIO_ANC_CH == ANC_R_CH)
        audio_enc_mpt_anc_mic_query(TCFG_AUDIO_ANCR_FB_MIC, AUDIO_ENC_MPT_FB_MIC, mic_en, mic_id_tmp, mic_gain);
#else
        audio_enc_mpt_anc_mic_query(TCFG_AUDIO_ANCL_FB_MIC, AUDIO_ENC_MPT_FB_MIC, mic_en, mic_id_tmp, mic_gain);
#endif/*TCFG_USER_TWS_ENABLE && (TCFG_AUDIO_ANC_CH == ANC_R_CH)*/
    }
    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_RFF_MIC) {
        audio_enc_mpt_anc_mic_query(TCFG_AUDIO_ANCR_FF_MIC, AUDIO_ENC_MPT_RFF_MIC, mic_en, mic_id_tmp, mic_gain);
    }
    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_RFB_MIC) {
        audio_enc_mpt_anc_mic_query(TCFG_AUDIO_ANCR_FB_MIC, AUDIO_ENC_MPT_RFB_MIC, mic_en, mic_id_tmp, mic_gain);
    }
#endif/*TCFG_AUDIO_ANC_ENABLE*/

    //4.重新排列数组, 兼容ADC_BUFF的缓存结构
    j = 0;
    for (i = 0; i < AUDIO_ENC_MPT_MIC_NUM; i++) {
        if (mic_id_tmp[i]) {
            cvp_hdl->mic_id[j++] = mic_id_tmp[i];
        }
    }

    //5.ADC初始化
    audio_mic_pwr_ctl(MIC_PWR_ON);
    if (cvp_hdl) {
        for (index = 0; index < AUDIO_ENC_MPT_MIC_NUM; index++) {
            if (mic_en[index]) {
                cvp_hdl->mic_num++;
                adc_file_mic_open(&cvp_hdl->mic_ch, BIT(index));
                audio_adc_mic_set_gain(&cvp_hdl->mic_ch, BIT(index), mic_gain[index]);
            }
        }
        if (!cvp_hdl->mic_num) {
            dut_cvp_log("Error valid mic_num is zero!\n");
            return 1;
        }

        //5.1 ADC临时BUFF、ISR BUFF申请
        for (i = 1; i < cvp_hdl->mic_num; i++) {
            cvp_hdl->tmp_buf[i - 1] = malloc(MIC_IRQ_POINTS * sizeof(short));
        }
        cvp_hdl->mic_buf = malloc(cvp_hdl->mic_num * MIC_BUF_NUM * MIC_IRQ_POINTS * sizeof(short));

#ifdef CONFIG_CPU_BR28
        //处理因ADC 复用buffer，导致无法更新ADC DMA地址的问题，后续整理ADC接口后再删除
        adc_hdl.hw_buf = NULL;
        adc_hdl.buf_fixed = 0;
#endif

#if TCFG_AUDIO_CVP_DEVELOP_ENABLE
        cvp_dev_mic_id_sort(mic_en);
#endif

        audio_adc_mic_set_sample_rate(&cvp_hdl->mic_ch, sr);
        audio_adc_mic_set_buffs(&cvp_hdl->mic_ch, cvp_hdl->mic_buf, MIC_IRQ_POINTS * 2, MIC_BUF_NUM);

        cvp_hdl->adc_output.handler = mic_output;
        audio_adc_add_output_handler(&adc_hdl, &cvp_hdl->adc_output);
        audio_adc_mic_start(&cvp_hdl->mic_ch);
    }
    dut_cvp_log("mic_open succ mic_num %d\n", cvp_hdl->mic_num);
    for (i = 0; i < AUDIO_ENC_MPT_MIC_NUM; i++) {
        dut_cvp_log("DIG mic%d_id 0x%3x; ANA mic%d_en %d, gain %d\n", i, cvp_hdl->mic_id[i], i, mic_en[i], mic_gain[i]);
    }
    return 0;
}

static void mic_close(void)
{
    if (cvp_hdl) {
        audio_adc_mic_close(&cvp_hdl->mic_ch);
        audio_adc_del_output_handler(&adc_hdl, &cvp_hdl->adc_output);
        for (int i = 1; i < cvp_hdl->mic_num; i++) {
            free(cvp_hdl->tmp_buf[i - 1]);
        }
        if (cvp_hdl->mic_buf) {
            //在adc 驱动中释放了这个buffer
            /* free(cvp_hdl->mic_buf); */
            cvp_hdl->mic_buf = NULL;
        }
        audio_mic_pwr_ctl(MIC_PWR_OFF);
    }
}

/*清晰语音数据输出*/
static int cvp_output_hdl(s16 *data, u16 len)
{
    /* putchar('o'); */

    audio_enc_mpt_fre_response_inbuf(AUDIO_ENC_MPT_CVP_OUT, data, len);
#if AUDIO_ENC_MPT_UART_DEBUG_EN
    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
        aec_uart_fill_v2(2, data, 512);
        aec_uart_write_v2();
    }
#endif/*AUDIO_ENC_MPT_UART_DEBUG_EN*/
#if !AUDIO_ENC_MPT_FRERES_ASYNC
    audio_enc_mpt_fre_response_post_run(0);
#endif/*AUDIO_ENC_MPT_FRERES_ASYNC*/

#if CVP_MONITOR_SEL != CVP_MONITOR_DIS
    if (cvp_hdl->monitor == CVP_MONITOR_POST) {
        cvp_monitor_output(data, len);
    }
#endif/*CVP_MONITOR_SEL != CVP_MONITOR_DIS*/
    return len;
}

int audio_enc_mpt_cvp_close(void)
{
    if (cvp_hdl != NULL) {
#if AUDIO_ENC_MPT_UART_DEBUG_EN
        if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
            aec_uart_close_v2();
        }
#endif/*AUDIO_ENC_MPT_UART_DEBUG_EN*/
#if CVP_MONITOR_SEL != CVP_MONITOR_DIS
        if (cvp_hdl->monitor) {
            cvp_monitor_en(0, CVP_MIC_SR);
        }
#endif/* CVP_MONITOR_SEL != CVP_MONITOR_DIS*/
        if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
#if TCFG_AUDIO_CVP_V3_MODE	//CVP_V3算法
            audio_cvp_v3_close();
#else
            audio_aec_close();
#endif
        }
        //先关AEC再关MIC
        mic_close();
        free(cvp_hdl);
        cvp_hdl = NULL;
        dut_cvp_log("cvp_hdl close\n");
    }
    return 0;
}

int audio_enc_mpt_cvp_open(u16 mic_ch)
{
    int ret = 0;
    u8 mic_gain[AUDIO_ENC_MPT_MIC_NUM];
    if (cvp_hdl) {
        //支持重入
        audio_enc_mpt_cvp_close();
        dut_cvp_log("dut_cvp start again\n");
    }
#if AUDIO_ENC_MPT_UART_DEBUG_EN
    if (mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
        aec_uart_open_v2(AUDIO_ENC_MPT_UART_DEBUG_CH, AUDIO_ENC_MPT_UART_DEBUG_LEN);
    }
#endif/*AUDIO_ENC_MPT_UART_DEBUG_EN*/
    dut_cvp_log("cvp_hdl open, mic_ch %x\n", mic_ch);
    cvp_hdl = zalloc(sizeof(audio_cvp_t));
    ASSERT(cvp_hdl);
    audio_enc_mpt_fre_pre(mic_ch);
#if CVP_MONITOR_SEL != CVP_MONITOR_DIS
    cvp_hdl->monitor = CVP_MONITOR_SEL;
    if (cvp_hdl->monitor) {
        cvp_monitor_en(1, CVP_MIC_SR);
    }
#endif/* CVP_MONITOR_SEL != CVP_MONITOR_DIS*/
    cvp_hdl->dut_mic_ch = mic_ch;

    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
        struct audio_aec_init_param_t init_param;
        init_param.sample_rate = CVP_MIC_SR;
        init_param.ref_sr = 0;
        //获取ADC(esco_adc)节点后的算法节点UUID
        init_param.node_uuid = audio_enc_mpt_cvp_uuid_get();
        if (!init_param.node_uuid) {
            ret = -1;
            goto __exit;
        }
        dut_cvp_log("init_param.node_uuid 0x%x\n", init_param.node_uuid);
        cvp_node_context_setup(init_param.node_uuid);
        cvp_param_cfg_read();

#if TCFG_AUDIO_CVP_V3_MODE	//CVP_V3算法
        audio_cvp_v3_open(&init_param, -1, cvp_output_hdl);
#elif TCFG_AUDIO_CVP_DEVELOP_ENABLE
        struct cvp_dev_cfg_t *cfg = cvp_dev_cfg_get();
        init_param.mic_num = cfg->mic_num;
        init_param.algo_type = cfg->algo_type;
        audio_aec_open(&init_param, -1, cvp_output_hdl);
#else
        audio_aec_open(&init_param, -1, cvp_output_hdl);
#endif
        //以下算法暂不支持此API
#if !TCFG_CVP_DEVELOP_ENABLE && !TCFG_AUDIO_CVP_V3_MODE
        //默认关闭DNS
        audio_cvp_ioctl(CVP_NS_SWITCH, 0, NULL); //关闭降噪
        audio_cvp_ioctl(CVP_WNC_SWITCH, 0, NULL); //关风噪检测
#if TCFG_AUDIO_TRIPLE_MIC_ENABLE	//3麦
        /*关闭fb mic*/
        audio_tms_mode_choose(TMS_MODE_2MIC);
#endif
#endif/*TCFG_AUDIO_CVP_NS_MODE*/
    }
    ret = mic_open(mic_gain, CVP_MIC_SR);
    return ret;

__exit:
#if AUDIO_ENC_MPT_UART_DEBUG_EN
    if (cvp_hdl->dut_mic_ch & AUDIO_ENC_MPT_CVP_OUT) {
        aec_uart_close_v2();
    }
#endif
#if CVP_MONITOR_SEL != CVP_MONITOR_DIS
    if (cvp_hdl->monitor) {
        cvp_monitor_en(0, CVP_MIC_SR);
    }
#endif
    free(cvp_hdl);
    cvp_hdl = NULL;
    return ret;
}

void audio_enc_mpt_fre_response_open(u16 mic_ch)
{
    clock_refurbish();
    audio_enc_mpt_fre_response_start(mic_ch);
    if (audio_enc_mpt_cvp_open(mic_ch)) {
        audio_enc_mpt_fre_response_release();
    }
}

void audio_enc_mpt_fre_response_close(void)
{
    audio_enc_mpt_fre_response_stop();
    audio_enc_mpt_cvp_close();
}

static u16 audio_enc_mpt_cvp_uuid_get(void)
{
    u16 uuid = 0;
    u8 table_size = ARRAY_SIZE(audio_cvp_uuid_table);
    struct jlstream *stream = voice_call_stream_search(NULL, VOICE_CALL_TYPE_ESCO);//获取通话数据流
    struct stream_node *node = &stream->snode->node;
    if (stream) {
        //在通话数据流中遍历通话算法节点，找到对应uuid
        for (int i = 0; i < table_size; i++) {
            /* dut_cvp_log("uuid_get %d, 0x%x\n", i, audio_cvp_uuid_table[i]); */
            if (jlstream_is_contains_node_from(node, audio_cvp_uuid_table[i])) {
                uuid = audio_cvp_uuid_table[i];
                dut_cvp_log("%s get succ! uuid[%d]:0x%x\n", __func__, i, uuid);
                break;
            }
        }
        jlstream_release(stream);
    }
    return uuid;
}

static u8 audio_cvp_idle_query()
{
    return (cvp_hdl == NULL) ? 1 : 0;
}

REGISTER_LP_TARGET(audio_enc_mpt_lp_target) = {
    .name = "audio_cvp",
    .is_idle = audio_cvp_idle_query,
};

#if AUDIO_ENC_MPT_DEBUG_DEMO
//测试func
void audio_enc_mpt_cvp_key_test()
{
    static u8 cnt = 0;
    u16 mic_ch = 0;
    dut_cvp_log("cnt %d\n", cnt);

    mem_stats();
    switch (cnt) {
    case 0:
        mic_ch |= AUDIO_ENC_MPT_CVP_OUT;
        mic_ch |= AUDIO_ENC_MPT_TALK_MIC;
        /* mic_ch |= AUDIO_ENC_MPT_TALK_FF_MIC; */
        /* mic_ch |= AUDIO_ENC_MPT_TALK_FB_MIC; */
        /* mic_ch |= AUDIO_ENC_MPT_TALK_VPU; */
        /* mic_ch |= AUDIO_ENC_MPT_FF_MIC; */
        /* mic_ch |= AUDIO_ENC_MPT_FB_MIC; */
        /* mic_ch |= AUDIO_ENC_MPT_RFF_MIC; */
        /* mic_ch |= AUDIO_ENC_MPT_RFB_MIC; */
        audio_enc_mpt_fre_response_open(mic_ch);
        break;
    case 1:
        audio_enc_mpt_fre_response_close();
        break;
    case 2:
        u8 *buf;
        audio_enc_mpt_fre_response_file_get(&buf);
        break;
    case 3:
        audio_enc_mpt_fre_response_release();
        break;
    }
    cnt++;
    if (cnt > 3) {
        cnt = 0;
    }
}
#endif

#endif/*AUDIO_ENC_MPT_SELF_ENABLE*/
