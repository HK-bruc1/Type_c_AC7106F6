#ifndef _ICSD_ADT_H
#define _ICSD_ADT_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "math.h"
#include "asm/math_fast_function.h"
#include "stdlib.h"
#include "icsd_common_v2.h"
#include "icsd_adt_alg.h"


extern const u8 adt_log_en;
#define sd_adt_log(format, ...)  if(adt_log_en){{if(config_ulog_enable){printf(format, ##__VA_ARGS__);}if(config_dlog_enable){dlog_printf((-1 & ~BIT(31)), format, ##__VA_ARGS__);}}}

#if ADT_PRINTF_EN
#define _adt_printf printf                  //打开智能免摘库打印信息
#else
#define _adt_printf icsd_printf_off
#endif

#define ADT_VDT_EN					BIT(0) //智能免摘
#define ADT_WDT_EN					BIT(1) //风噪检测
#define ADT_WAT_EN					BIT(2) //广域点击
#define ADT_ENV_EN                  BIT(3) //环境声检测
#define ADT_RTANC_EN				BIT(4) //实时自适应ANC
#define ADT_EIN_EN                  BIT(5) //入耳检测
#define ADT_AVC_EN                  BIT(6) //自适应音量
//#define ADT_RTANC_TIDY_EN                  BIT(7) //RTANC TIDY mode 占用
#define ADT_ADJDCC_EN               BIT(8) //自适应DCC
#define ADT_HOWL_EN                 BIT(9) //防啸叫

#define ADT_PATH_3M_EN         		BIT(0)
extern u8 ADT_PATH_CONFIG;
#define ADT_INF_1					BIT(0)//state
#define ADT_INF_2					BIT(1)//talk+ref
#define ADT_INF_3					BIT(2)//data
#define ADT_INF_4                   BIT(3)//err+ref
#define ADT_INF_5                   BIT(4)//ANC状态
#define ADT_INF_6                   BIT(6)//power
#define ADT_INF_7                   BIT(7)//ref power
#define ADT_INF_8                   BIT(8)//dither
#define ADT_INF_9					BIT(9)//PNC 原始数据
#define ADT_INF_10					BIT(10)//ANC 原始数据
#define ADT_INF_11					BIT(11)//TRANS 原始数据
#define ADT_INF_12					BIT(12)//open talk mic
extern const u16 ADT_DEBUG_INF;

//u16 mic_type
#define ADT_REFMIC_L				BIT(0)
#define ADT_REFMIC_R				BIT(1)
#define ADT_ERRMIC_L				BIT(2)
#define ADT_ERRMIC_R				BIT(3)
#define ADT_TLKMIC_L				BIT(4)
#define ADT_TLKMIC_R				BIT(5)


struct adt_function {
    //sys
    void (*os_time_dly)(int tick);
    int (*os_sem_pend)(OS_SEM *sem, int timeout);
    int (*os_sem_del)(OS_SEM  *p_sem, int block);
    int (*os_sem_create)(OS_SEM *p_sem, int cnt);
    int (*os_sem_set)(OS_SEM *p_sem, u16 cnt);
    int (*os_sem_post)(OS_SEM *p_sem);
    void (*local_irq_disable)();
    void (*local_irq_enable)();
    void (*icsd_post_adttask_msg)(u8 cmd, u8 id);
    void (*icsd_post_srctask_msg)(u8 cmd);
    void (*icsd_post_anctask_msg)(u8 cmd);
    void (*icsd_post_rtanctask_msg)(u8 cmd);
    void (*icsd_post_detask_msg)(u8 cmd);
    int (*jiffies_usec2offset)(unsigned long begin, unsigned long end);
    unsigned long (*jiffies_usec)(void);
    int (*audio_anc_debug_send_data)(u8 *buf, int len);
    u8(*audio_anc_debug_busy_get)(void);
    u8(*audio_adt_talk_mic_analog_close)();
    u8(*audio_adt_talk_mic_analog_open)();
    u8(*audio_anc_mic_gain_get)(u8 mic_sel);
    //tws
    int (*tws_api_get_role)(void);
    int (*tws_api_get_tws_state)();
    u8(*icsd_adt_tws_state)();
    //anc
    u8(*anc_dma_done_ppflag)();
    void (*anc_core_dma_ie)(u8 en);
    void (*anc_core_dma_stop)(void);
    void (*anc_dma_on)(u8 out_sel, int *buf, int len);
    void (*anc_dma_on_double)(u8 out_sel, int *buf, int len);
    void (*anc_dma_on_double_4ch)(u8 ch1_out_sel, u8 ch2_out_sel, int *buf, int irq_point);
    void (*anc_dma_off)();
    void (*icsd_adt_hw_suspend)();
    void (*icsd_adt_hw_resume)();
    void (*icsd_adt_anc46k_out_reset)();
    void (*icsd_adt_anc46k_out_isr)();
    //dac
    int (*audio_dac_read_anc_reset)();
    int (*audio_dac_read_mic_reset)();
    int (*audio_dac_read_anc)(s16 points_offset, void *data, int len, u8 read_channel);
    int (*audio_dac_read_mic)(s16 points_offset, void *data, int len, u8 read_channel);
    //src
    void (*icsd_adt_src_write)(void *data, int len, void *resample);
    void (*icsd_adt_src_push)(void *resample);
    void (*icsd_adt_src_close)(void *resample);
    void *(*icsd_adt_src_init)(int in_rate, int out_rate, int (*handler)(void *, void *, int));
    //OUTPUT
    void (*icsd_VDT_output)(u8 vdt_result);
    void (*icsd_WAT_output)(u8 wat_result);
    void (*icsd_WDT_output)(u8 wind_lvl);
    void (*icsd_EIN_output)(u8 ein_state);
    void (*icsd_AVC_output)(__adt_avc_output *_output);
    void (*icsd_RTANC_output)(void *rt_param_l, void *rt_param_r);
    void (*icsd_ADJDCC_output)(u8 result);
    void (*icsd_HOWL_output)(u8 result);
    void (*icsd_DRC_output)(u8 output, float gain, float fb_gain);
    u8(*icsd_DRC_read)(float **gain);   //bit0:本地结果  bit1:TWS发送过来的结果
};
extern struct adt_function	*ADT_FUNC;

struct icsd_acoustic_detector_infmt {
    int TOOL_FUNCTION;
    void *param;
    u16 sample_rate;     //当前播放采样率
    u16 adc_sr;           //MIC数据采样率
    u8 ein_state;
    s8 lff_gain;         //dB值
    s8 lfb_gain;
    s8 rff_gain;
    s8 rfb_gain;
    s8 talk_gain;

    u8 adt_mode;         // TWS: 0 HEADSET: 1
    u8 dac_mode;		 //低压0 高压1
    u8 mic0_type;        // MIC0 类型
    u8 mic1_type;        // MIC1 类型
    u8 mic2_type;        // MIC2 类型
    u8 mic3_type;        // MIC3 类型
    u8 sensitivity;      //智能免摘灵敏度设置
    float gains_l_fbgain;
    float gains_l_ffgain;
    float gains_l_transgain;
    float gains_l_transfbgain;
    u8    gain_sign;
    u32   trans_alogm;
    u32   alogm;
    void *alloc_ptr;    //外部申请的ram地址
    struct icsd_rtanc_tool_data *rtanc_tool;
    /*
        算法结果输出
        voice_state: 0 无讲话； 1 有讲话
        wind_lvl: 0-70 风噪等级
    */
    void (*output_hdl)(u8 voice_state, u8 wind_lvl);    //算法结果输出
    void (*anc_dma_post_msg)(void);     //ANC task dma 抄数回调接口
    void (*anc_dma_output_callback);    //ANC DMA下采样数据输出回调，用于写卡分析
    u16 debug_ram_size;
};

struct icsd_acoustic_detector_libfmt {
    int adc_isr_len;     //ADC中断长度
    int adc_sr;          //ADC 采样率
    int lib_alloc_size;  //算法ram需求大小
    u8 mic_num;			 //需要打开的mic个数
    u16 mic_type;
    u8 rtanc_type;      //RTANC类型(传入参数)
    u8 wdt_type;		//0:单独风噪 1:风噪+ADJDCC 2:风噪+自讲+AVC 3：风噪+ADJDCC+自讲
};

typedef struct {
    u8  out_en;
    int loop_len;    //循环BUF长度
    u16 loop_remain; //循环BUF内可用数据长度
    s16 *ch0_dptr;   //通道1数据地址
    s16 *ch1_dptr;	 //通道2数据地址
    s16 *ch2_dptr;	 //通道3数据地址
    s16 *ch3_dptr;   //通道4数据地址
    u16 rptr;    //通道1数据读指针
} __adt_anc46k_ctl;
extern __adt_anc46k_ctl *ANC46K_CTL;

struct icsd_adt_input_param_v3 {
    void *priv;
    s16 *lff;
    s16 *rff;
    s16 *lfb;
    s16 *rfb;
    s16 *ltalk;
    s16 *rtalk;
    int len;
};

enum {
    ICSD_ANC_LFF_MIC  = 0,
    ICSD_ANC_LFB_MIC  = 1,
    ICSD_ANC_RFF_MIC  = 2,
    ICSD_ANC_RFB_MIC  = 3,
    ICSD_ANC_TALK_MIC = 4,
    ICSD_ANC_MIC_NULL = 0XFF,   //对应的MIC没有数值,则表示这个MIC没有开启
};

enum {
    ADT_DACMODE_LOW = 0,
    ADT_DACMODE_HIGH,
};


extern const u8 BT_ADT_INF_EN;
extern const u8 BT_ADT_DP_STATE_EN;
extern const u8 VDT_DATA_CHECK;
extern const u8 WIND_DATA_CHECK;
extern const u8 ICSD_WIND_3MIC;
extern const u8 ICSD_HOWL_EN;
extern const u8 ICSD_ADT_WIND_MIC_TYPE;
extern const u8 ICSD_ADT_WIND_PHONE_TYPE;
extern const u8 ADT_VDT_USE_ANCDMAL;
extern const u8 ICSD_WIND_EN;
extern const u8 ICSD_WAT_EN;
extern const u8 ICSD_VDT_EN;
extern const u8 ICSD_ENVNL_EN;
extern const u8 ICSD_EIN_EN;
extern const u8 ICSD_AVC_EN;
extern const u8 ICSD_RTANC_EN;
extern const u8 ICSD_RTAEQ_EN;
extern const u8 ICSD_46KOUT_EN;
extern const u8 ICSD_ADJDCC_EN;

extern s16 *adt_dac_loopbuf;
extern int (*adt_printf)(const char *format, ...);
//tws
void icsd_adt_tx_unsniff_req();
void icsd_adt_s2m_packet(s8 *data, u8 cmd);
void icsd_adt_m2s_packet(s8 *data, u8 cmd);
void icsd_adt_m2s_cb(void *_data, u16 len, bool rx);
void icsd_adt_s2m_cb(void *_data, u16 len, bool rx);
void icsd_adt_tws_m2s(u8 cmd);
void icsd_adt_tws_s2m(u8 cmd);
int  icsd_adt_tws_msync(u8 *data, s16 len);
int  icsd_adt_tws_ssync(u8 *data, s16 len);
//task
void icsd_srctask_cmd_check(u8 cmd);
void icsd_anctask_cmd_check(u8 cmd);
void icsd_adt_task_handler(int msg, int msg2);
void icsd_src_task_handler(int msg);
void icsd_adt_anctask_handle(u8 cmd);
void icsd_rtanc_task_handler(int msg);
void icsd_de_task_handler(int msg);
void icsd_task_create();
//anc
void icsd_adt_dma_done();
//dac
void icsd_adt_set_audio_sample_rate(u16 sample_rate);
void icsd_adt_dac_loopbuf_malloc(u16 points);
void icsd_adt_dac_loopbuf_free();
//APP调用的LIB函数
void icsd_acoustic_detector_get_libfmt(struct icsd_acoustic_detector_libfmt *libfmt, int function);
void icsd_acoustic_detector_set_infmt(struct icsd_acoustic_detector_infmt *fmt);
void icsd_acoustic_detector_infmt_updata(struct icsd_acoustic_detector_infmt *fmt);
void icsd_acoustic_detector_open(void);
void icsd_acoustic_detector_close();
void icsd_acoustic_detector_resume(u8 mode, u8 anc_onoff);
void icsd_acoustic_detector_suspend();
void icsd_acoustic_detector_ancdma_done();//ancdma done回调
void icsd_acoustic_detector_mic_input_hdl(void *priv, s16 *buf, int len);
void icsd_acoustic_detector_mic_input_hdl_v2(void *priv, s16 *talk_mic, s16 *ff_mic, s16 *fb_mic, int len);
void icsd_acoustic_detector_mic_input_hdl_v3(struct icsd_adt_input_param_v3 *param);
void icsd_adt_rtanc_suspend();
void icsd_adt_rtanc_resume();
u8 	 icsd_adt_current_mic_num();//获取当前ADT使用的mic数量
//OUTPUT
void icsd_envnl_output(int result);
void icsd_WDT_output_demo(u8 wind_lvl);
void icsd_WAT_output_demo(u8 wat_result);
void icsd_VDT_output_demo(u8 vdt_result);
void icsd_EIN_output_demo(u8 ein_state);
void icsd_AVC_output_demo(__adt_avc_output *_output);
void icsd_RTANC_output_demo(void *rt_param_l, void *rt_param_r);
void icsd_ADJDCC_output_demo(u8 result);
//RTANC
void icsd_adt_rtanc_fadegain_updaterun(void *param);
void icsd_adt_rtanc_fadegain_update(void *param);//使用该函数过程中需要锁住，确保调用过程中ADT空间不会被释放
//AVC
void icsd_adt_avc_config_update(void *_config);
void icsd_HOWL_output_demo(u8 result);

void icsd_adt_tone_play_handler(u8 idx);
u8 	 icsd_adt_get_wind_lvl();
u8 icsd_adt_get_adjdcc_result();
/*初始化dac read的资源*/
int audio_dac_read_anc_init(void);
/*释放dac read的资源*/
int audio_dac_read_anc_exit(void);
/*重置当前dac read读取的参数*/
int audio_dac_read_anc_reset(void);
int audio_dac_read_mic_reset(void);

int *icsd_minirt_reuse_ram();
void icsd_adt_de_suspend();
void anc_debug_free(void *pv);
void *anc_debug_malloc(const char *name, int size);

extern const u8 rt_anc_dac_en;
extern const u8 mic_input_v2;
extern const u8 RTANC_ALG_DEBUG;
extern const u8 ICSD_WDT_V2;
extern const u8 ICSD_HOWL_REF_EN;
extern const u8 avc_run_interval;
extern const u8 tidy_avc_run_interval;
extern const u8 icsd_ancdma_dac_debug;
extern u16 adt_debug_ramsize;
extern const u8 ADT_MIC_VERSION;
extern const u8 ICSD_AVC_DATAPATH;
extern const u16 ANC_DMA_POINTS;
extern char lib_adt_version[];
extern const u8 icsd_dac_micread;
extern const u8 rtanc_ancout_2ch_en;

#endif
