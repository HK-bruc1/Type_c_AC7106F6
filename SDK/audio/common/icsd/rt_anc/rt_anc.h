#ifndef _ICSD_RT_ANC_LIB_H
#define _ICSD_RT_ANC_LIB_H

#include "icsd_common_v2.h"
#include "anc_DeAlg_v2.h"
#include "anc_sDe.h"
#include "icsd_anc_v2_config.h"
#include "math.h"
#include "asm/math_fast_function.h"
#include "../tool/anc_ext_tool_cfg.h"

#if RTANC_RTPRINTF_EN
#define _rt_printf printf               //打开智能免摘库打印信息
#else
extern int rt_printf_off(const char *format, ...);
#define _rt_printf rt_printf_off
#endif
extern int (*rt_printf)(const char *format, ...);

#if RTANC_HZPRINTF_EN
#define _hz_printf printf               //打开智能免摘库打印信息
#else
extern int rt_printf_off(const char *format, ...);
#define _hz_printf rt_printf_off
#endif
extern int (*hz_printf)(const char *format, ...);

extern const u8 rtanc_log_en;
#define sd_rtanc_log(format, ...)  if(rtanc_log_en){{if(config_ulog_enable){printf(format, ##__VA_ARGS__);}if(config_dlog_enable){dlog_printf((-1 & ~BIT(31)), format, ##__VA_ARGS__);}}}


#define RT_DMA_BELONG_TO_SZL    		1
#define RT_DMA_BELONG_TO_SZR    		2
#define RT_DMA_BELONG_TO_PZL    		3
#define RT_DMA_BELONG_TO_PZR    		4
#define RT_DMA_BELONG_TO_TONEL  		5
#define RT_DMA_BELONG_TO_BYPASSL 		6
#define RT_DMA_BELONG_TO_TONER  		7
#define RT_DMA_BELONG_TO_BYPASSR 	    8
#define RT_DMA_BELONG_TO_CMP 	   		9

#define RT_ANC_TEST_DATA			    0
#define RT_ANC_DSF8_DATA_DEBUG	        0
#define RT_DSF8_DATA_DEBUG_TYPE			RT_DMA_BELONG_TO_CMP

#define RT_ANC_ON						BIT(1)
#define RT_ANC_NEED_UPDATA              BIT(2)
#define RT_ANC_SUSPEND              	BIT(3)

#define RT_ANC_USER_TRAIN_DMA_LEN 		16384
#define RT_ANC_DMA_DOUBLE_LEN       	(512*4) //(512*8)
#define RT_ANC_DOUBLE_TRAIN_LEN         512
#define RT_ANC_DMA_DOUBLE_CNT       	32

#define RT_ANC_DMA_INTERVAL_MAX    		(1.8*RT_ANC_DMA_DOUBLE_LEN*1000/375) //us

#define RT_ANC_MAX_ORDER				10		//rt_anc最大支持滤波器个数

typedef struct {

} __rt_anc_config;

struct rt_anc_function {
    u8(*anc_dma_done_ppflag)();
    void (*icsd_adt_rtanc_suspend)();
    void (*anc_dma_on)(u8 out_sel, int *buf, int len);
    void (*anc_dma_on_double)(u8 out_sel, int *buf, int len);
    void (*anc_core_dma_ie)(u8 en);
    void (*anc_core_dma_stop)(void);
    void (*rt_anc_post_rttask_cmd)(u8 cmd);
    void (*rt_anc_post_anctask_cmd)(u8 cmd);
    void (*icsd_post_detask_msg)(u8 cmd);
    void (*rt_anc_dma_2ch_on)(u8 out_sel, int *buf, int len);
    void (*rt_anc_dma_4ch_on)(u8 out_sel_ch01, u8 out_sel_ch23, int *buf, int len);
    void (*rt_anc_alg_output)(void *rt_param_l, void *rt_param_r);
    int (*jiffies_usec2offset)(unsigned long begin, unsigned long end);
    unsigned long (*jiffies_usec)(void);
    int (*audio_anc_debug_send_data)(u8 *buf, int len);

    void (*rt_anc_config_init)(__rt_anc_config *_rt_anc_config);
    void (*rt_anc_param_updata_cmd)(void *param_l, void *param_r);

    void (*clock_refurbish)();

    void (*icsd_self_talk_output)(u8 flag);
    u8(*get_wind_lvl)();
    u8(*get_adjdcc_result)();
    u8(*get_wind_angle)();
    float(*get_avc_spldb_iir)();

    void (*rtanc_drc_output)(u8 flag, float gain, float fb_gain);
    u8(*rtanc_drc_read)(float **gain);

    void (*ff_adjdcc_par_set)(u8 dc_par);
};
extern struct rt_anc_function *RT_ANC_FUNC;
enum {
    RT_ANC_PART2 = 0,
    RT_ANC_RTTASK_SUSPEND,
    RT_ANC_MASTER_UPDATA,
    RT_ANC_SLAVER_UPDATA,
    RT_ANC_WAIT_UPDATA,
    RT_ANC_UPDATA_END,
    RT_CMP_PART2,
    RTANC_HEADSET_UPDATE = 17,
};

enum {
    RT_ANC_CMD_FORCED_EXIT = 0,
    RT_ANC_CMD_PART1,
    RT_ANC_CMD_EXIT,
    RT_ANC_CMD_UPDATA,
    RTANC_CMD_UPDATA,
    RT_ANC_CMD_DMA_ON,
    RT_ANC_CMD_ANCTASK_SUSPEND,
};

typedef struct {
    u8  anc_mode;			// 0 ANC, 1 通透(只会在DAC出声的时候更新)
    u8  test_mode;          // 0:normal 1:产测模式
    u8	ff_yorder;
    u8  fb_yorder;
    u8  cmp_yorder;
    u8  cmp_eq_updat;       // 置1表示下需要更新cmp与eq
    u8  cmp_eq_szidx;
    u8  ff_updat;
    u8  fb_updat;
    u8  ff_use_outfgq;
    u8  updat_busy;
    u8  ff_fade_slow;
    float ffgain;
    float fbgain;
    float ff_fade_gain;
    float fb_fade_gain;
    float cmpgain;
    float ff_fgq[31];
    double ff_coeff[5 * RT_ANC_MAX_ORDER]; //max
    double fb_coeff[5 * RT_ANC_MAX_ORDER]; //max
    double cmp_coeff[5 * RT_ANC_MAX_ORDER]; //max
    void *param;
} __rt_anc_param;

typedef struct {
    u8 flag;
    u8 ind_last;
    u8 ls_gain_flag;
    u8 sdrc_flag_fix_ls;
    u8 sdrc_flag_rls_ls;
    u8 update_cnt;
    u8 music2norm_cnt;
    u8 cmp_ind_last;
    u8 low_spl_cnt;
    u8 low_spl_env;
    u8 rewear_ind;
    u8 st_update_cnt;

    float sz_ind_iir;
    float part1_ref_lf_dB;
    float part1_err_lf_dB;
    float fb_aim_gain;
    float tg_db[MEMLEN / 2];
    float tg_db_buf[MEMLEN / 2];
    float sz_target_iir[MSELEN];
    float sz_iir[MSELEN];

} __rtanc_var_buffer;

struct rt_anc_infmt {
    void *alloc_ptr;     //外部申请的ram地址
    u16	tone_delay;      //提示音延长多长时间开始获取sz数据
    u8  type;            //RT_ANC_TWS / RT_ANC_HEADSET
    void *param;
    void *ext_cfg;
    __rt_anc_param *anc_param_l;
    __rt_anc_param *anc_param_r;
    __rtanc_var_buffer *var_buf;
    __rtanc_var_buffer *var_buf_r;
    u8  id;
    struct icsd_rtanc_tool_data *rtanc_tool;
};

struct rt_anc_libfmt {
    //输出参数
    int lib_alloc_size;  //算法ram需求大小
    u8  alogm;           //anc采样率
    //输入参数
    u8  type;            //RT_ANC_TWS / RT_ANC_HEADSET
};

struct rt_anc_tws_packet {
    s8  *data;
    u16 len;
};

enum {
    M_CMD_TEST = 0,
    S_CMD_TEST,
};

typedef struct {
    u8  cmd;
    u8  id;
    u8  data;
} __rtanc_sync_cmd;

extern const u8 rt_anc_dma_ch_num;
extern const u8 rt_cmp_en;
extern const u8 rt_anc_dac_en;
extern const u8 rt_anc_tool_data;
extern  u16	RT_ANC_STATE;
//RT_ANC调用
int os_taskq_post_msg(const char *name, int argc, ...);
//SDK 调用
void rt_anc_anctask_cmd_handle(void *param, u8 cmd);
void rt_anc_master_packet(struct rt_anc_tws_packet *packet, u8 cmd);
void rt_anc_slave_packet(struct rt_anc_tws_packet *packet, u8 cmd);
void icsd_rtanc_m2s_cb(void *_data, u16 len, bool rx);
void icsd_rtanc_s2m_cb(void *_data, u16 len, bool rx);
//库调用
void rt_anc_config_init();
void rt_anc_dsf8_data_debug(u8 belong, s16 *dsf_out_ch0, s16 *dsf_out_ch1, s16 *dsf_out_ch2, s16 *dsf_out_ch3);


//=========RTANC============
struct __rtanc_bt_inf {
    u8 debug_function;
    u16 debug_id;
    u8  rtanc_id;

    u8  state;
    u8  cmp_eq_updat;
    u8  ff_updat;
    u8  fb_updat;
    u8  cmp_eq_szidx;

    s16 log_buf[40];

    u16 mode_sel;
    float ff_fgq[31];
};

struct __rtanc_bt_cfg {
    u8  debug_function;
    u16 debug_id;
    u8  rtanc_id;

    struct __anc_ext_rtanc_adaptive_cfg rtanc_tool_cfg;
    struct __anc_ext_dynamic_cfg dynamic_cfg;
};

struct icsd_rtanc_infmt {
    void *alloc_ptr;     //外部申请的ram地址
    int lib_alloc_size;  //算法ram需求大小
    __rt_anc_param *anc_param_l;
    __rt_anc_param *anc_param_r;
    u8  ch_num;
    u8  ep_type;
    u8  rtanc_type;
    u8  part1_times;
    struct icsd_rtanc_tool_data *rtanc_tool;
    int  TOOL_FUNCTION;
};

struct icsd_rtanc_libfmt {
    //输出参数
    int lib_alloc_size;  //算法ram需求大小
    u8  ch_num;
    u8  rtanc_type;
};

typedef struct {
    u8  part1_ch;
    u8  dma_ch;
    u8  part1_cnt;
    u8  dac_flag_iszero;
    s16 *inptr_h;
    s16 *inptr_m;
    s16 *inptr_l;
    int p1dac_max_vld;

    float *out0_sum;
    float *out1_sum;
    float *out2_sum;
    int   *fft_ram;
    float *hpest_temp;
} __icsd_rtanc_part1_parm;

typedef struct {
    u8  dma_ch;
    u8  dac_flag_iszero;
    u8  LR_FLAG;
    float *out0_sum;
    float *out1_sum;
    float *out2_sum;
    float *sz_out0_sum;
    float *sz_out1_sum;
    float *sz_out2_sum;
    float *szpz_out;
} __icsd_rtanc_part2_parm;


typedef struct {
    u8     yorder;
    double *coeff;
    float  gain;
    float  fade_gain;
} __rtanc_filter;

struct rtanc_param {
    __rtanc_filter ffl_filter;
    __rtanc_filter fbl_filter;
    __rtanc_filter ffr_filter;
    __rtanc_filter fbr_filter;
};

void icsd_rtanc_get_libfmt(struct icsd_rtanc_libfmt *libfmt);
void icsd_rtanc_set_infmt(struct icsd_rtanc_infmt *fmt);
void icsd_alg_rtanc_run_part1(__icsd_rtanc_part1_parm *part1_parm);
u8   icsd_alg_rtanc_run_part2_merge(__icsd_rtanc_part2_parm *part2_parm);
void icsd_alg_rtanc_offline_printf(__icsd_rtanc_part2_parm *part2_parm);
// void icsd_alg_rtanc_offline_printf_r();
//void icsd_alg_rtanc_part2_parm_init();
void icsd_alg_rtanc_part2_parm_init(__icsd_rtanc_part2_parm *part2_parm);
//void icsd_alg_rtanc_part2_parm_init_r();
void icsd_alg_rtanc_set_part1_ch_lr(u8 ch, u8 part2_stop);
u8   icsd_alg_rtanc_get_part1_ch();
void icsd_alg_rtanc_set_part1_ch();
void icsd_alg_rtanc_set_part1_ch_all0();
void icsd_alg_rtanc_set_update_flag(u8 flag);
u8 icsd_alg_rtanc_get_update_flag();
void rt_anc_time_out_del();
void rt_anc_set_init(struct rt_anc_infmt *fmt, struct __anc_ext_rtanc_adaptive_cfg *rtanc_tool_cfg, struct __anc_ext_rtanc_adaptive_cfg *rtanc_tool_cfg_r, struct __anc_ext_dynamic_cfg *dynamic_cfg, struct __anc_ext_dynamic_cfg *dynamic_cfg_r);
void rt_anc_init(struct rt_anc_infmt *fmt, struct __anc_ext_rtanc_adaptive_cfg *rtanc_tool_cfg, struct __anc_ext_rtanc_adaptive_cfg *rtanc_tool_cfg_r, struct __anc_ext_dynamic_cfg *dynamic_cfg, struct __anc_ext_dynamic_cfg *dynamic_cfg_r);
void rt_anc_param_updata_cmd(void *param_l, void *param_r);
void rt_anc_part1_reset();
void icsd_rtanc_alg_get_sz(float *sz_out, u8 ch);
void icsd_alg_rtanc_fadegain_update(struct rtanc_param *param);
void icsd_alg_rtanc_filter_update(struct rtanc_param *param);
void icsd_post_rtanctask_msg(u8 cmd);
void icsd_post_detask_msg(u8 cmd);
void rtanc_adjdcc_flag_set(u8 flag);
void rtanc_cal_and_update_filter_task(u8 ch);
u8 adjdcc_trigger_update(u8 env_level, float *table);
u8 rtanc_adjdcc_flag_get();

extern const u8 RTANC_ALG_DEBUG;

extern char lib_rtanc_version[];
#endif
