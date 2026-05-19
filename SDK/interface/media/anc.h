#ifndef _ANC_BASE_H_
#define _ANC_BASE_H_

#include "generic/typedef.h"
#include "system/task.h"
#include "media/audio_def.h"
#include "audio_adc.h"
#include "asm/anc_platform.h"

enum {
    ANCDB_ERR_CRC = 1,
    ANCDB_ERR_TAG,
    ANCDB_ERR_PARAM,
    ANCDB_ERR_STATE,
};

typedef enum {
    ANC_OFF = 1,			/*关闭模式*/
    ANC_ON = 2,				/*降噪模式*/
    ANC_TRANSPARENCY = 3,	/*通透模式*/
    ANC_BYPASS = 4,			/*bypass模式*/
    ANC_EXT = 5,			/*ANC扩展模式-针对使用ANC DMA通路做算法的场景*/
    ANC_MODE_NULL,

    ANC_TRAIN = 0XA1,	/*训练模式*/
    ANC_TRANS_TRAIN,	/*通透训练模式*/
} ANC_mode_t;

/*ANC状态*/
enum {
    ANC_STA_CLOSE = 0,
    ANC_STA_INIT,
    ANC_STA_OPEN,
};

typedef enum {
    TRANS_ERR_TIMEOUT = 1,	//训练超时
    TRANS_ERR_MICERR,		//训练中MIC异常
    TRANS_ERR_FORCE_EXIT	//强制退出训练
} ANC_trans_errmsg_t;

/*ANC MIC 类型*/
enum {
    ANC_MIC_TYPE_LFF = 0,
    ANC_MIC_TYPE_LFB,
    ANC_MIC_TYPE_RFF,
    ANC_MIC_TYPE_RFB,
};

//ANC滤波器更新类型
enum {
    ANC_COEFF_TYPE_FF = BIT(0),		//FF滤波器
    ANC_COEFF_TYPE_FB = BIT(1),		//FB滤波器
};

typedef enum {
    //OLD
    ANC_L_FF_IIR  = 0x0,		//左FF滤波器ID
    ANC_L_FB_IIR  = 0x1,		//左FB滤波器ID
    ANC_L_CMP_IIR = 0x2,		//左音乐补偿滤波器ID
    ANC_L_TRANS_IIR = 0x3,		//左通透滤波器ID
    ANC_L_FIR = 0x4,			//(保留位，暂时不用)
    // ANC_L_TRANS2_IIR = 0x5,

    //NEW
    ANC_L_FF_FGQ  = 0x6,		//左FF滤波器FGQ
    ANC_L_FB_FGQ  = 0x7,		//左FB滤波器FGQ
    ANC_L_CMP_FGQ = 0x8,		//左音乐补偿滤波器FGQ
    ANC_L_TRANS_FGQ = 0x9,		//左通透滤波器FGQ
    ANC_L_FF_PARM  = 0xA,		//左FF参数ID
    ANC_L_FB_PARM  = 0xB,		//左FB参数ID
    ANC_L_CMP_PARM = 0xC,		//左音乐补偿参数ID
    ANC_L_TRANS_PARM = 0xD,		//左通透参数ID

    //OLD
    ANC_R_FF_IIR  = 0x10,		//右FF滤波器ID
    ANC_R_FB_IIR  = 0x11,       //右FB滤波器ID
    ANC_R_CMP_IIR = 0x12,       //右音乐补偿滤波器ID
    ANC_R_TRANS_IIR = 0x13,     //右通透滤波器ID
    ANC_R_FIR = 0x14,            //(保留位，暂时不用)
    // ANC_R_TRANS2_IIR = 0x15,

    //NEW
    ANC_R_FF_FGQ  = 0x16,		//右FF滤波器FGQ
    ANC_R_FB_FGQ  = 0x17,       //右FB滤波器FGQ
    ANC_R_CMP_FGQ = 0x18,       //右音乐补偿滤波器FGQ
    ANC_R_TRANS_FGQ = 0x19,     //右通透滤波器FGQ
    ANC_R_FF_PARM  = 0x1A,		//右FF参数ID
    ANC_R_FB_PARM  = 0x1B,       //右FB参数ID
    ANC_R_CMP_PARM = 0x1C,       //右音乐补偿参数ID
    ANC_R_TRANS_PARM = 0x1D,     //右通透参数ID

    //OTHER
    ANC_L_ADAP_GOLD_CURVE = 0x24,//自适应金机曲线
    ANC_R_ADAP_GOLD_CURVE = 0x34,

} ANC_config_seg_id_t;

//新增iir滤波器 type 枚举
typedef enum {
    ANC_IIR_HIGH_PASS = 0,
    ANC_IIR_LOW_PASS,
    ANC_IIR_BAND_PASS,
    ANC_IIR_HIGH_SHELF,
    ANC_IIR_LOW_SHELF,
} ANC_iir_type;

typedef enum {
    ANC_ALOGM1  = 0,
    ANC_ALOGM2,
    ANC_ALOGM3,
    ANC_ALOGM4,
    ANC_ALOGM5,
    ANC_ALOGM6,
    ANC_ALOGM7,
    ANC_ALOGM8,
} ANC_alogm_t;		//ANC算法模式

typedef enum {
    ANC_POW_SEL_R_DAC_REF =	0x02,	//功率输出R DAC+REFMIC
    ANC_POW_SEL_R_DAC_ERR,			//功率输出R DAC+ERRMIC
    ANC_POW_SEL_L_DAC_REF,			//功率输出L DAC+REFMIC
    ANC_POW_SEL_L_DAC_ERR,			//功率输出L DAC+ERRMIC
} ANC_pow_sel_t;

typedef enum {
    ANC_COEFF_MODE_NORMAL =	0x0,		//自适应普通模式
    ANC_COEFF_MODE_ADAPTIVE = 0x1,		//自适应滤波器模式
} ANC_coeff_mode_t;

//自适应解码任务打断状态
typedef enum {
    ANC_ADAPTIVE_DEC_WAIT_STOP = 0x0, //停止
    ANC_ADAPTIVE_DEC_WAIT_START,	  //启动
    ANC_ADAPTIVE_DEC_WAIT_RUN,		  //运行中
} ANC_adaptive_dec_wait_t;

//自适应滤波器数据状态
typedef enum {
    ANC_ADAPTIVE_DATA_EMPTY = 0x0, 	//数据为空
    ANC_ADAPTIVE_DATA_FROM_VM,	  	   	//来源VM
    ANC_ADAPTIVE_DATA_FROM_ALOGM,		//来源算法
} ANC_adaptive_data_from_t;

// ANC fade gain通道
typedef enum {
    AUDIO_ANC_FADE_CH_LFF = 0x1,
    AUDIO_ANC_FADE_CH_LFB = 0x2,
    AUDIO_ANC_FADE_CH_RFF = 0x4,
    AUDIO_ANC_FADE_CH_RFB = 0x8,
} ANC_fade_ch_t;

// ANC 场景自适应事件
enum ANC_ADAPTIVE_FADE_EVENT {
    ANC_ADAPTIVE_EVENT_FADE_STATR = 0x1,	//场景自适应增益切换启动
    ANC_ADAPTIVE_EVENT_FADE_END,			//场景自适应增益切换结束
};

//ANC DMA sel ch
typedef enum {
    ANC_DMA_ROUT_LOUT = 0x1,
    ANC_DMA_ROUT_RERR = 0x2,
    ANC_DMA_ROUT_RREF = 0x3,
    ANC_DMA_LOUT_LERR = 0x4,
    ANC_DMA_LOUT_LREF = 0x5,
    ANC_DMA_RREF_RERR = 0x6,
    ANC_DMA_LREF_LERR = 0x7,
} ANC_dma_ch_t;

/*ANC 滤波器类型声明*/
enum ANC_IIR_TYPE {
    ANC_FF_TYPE = BIT(0),	//FF
    ANC_FB_TYPE	= BIT(1),	//FB
    ANC_CMP_TYPE = BIT(2),	//CMP
    ANC_TRANS_TYPE = BIT(3),	//TRANS
};

enum ANC_DCC_TRIM_MODE {
    ANC_DCC_TRIM_CLOSE = 0,	//关闭trim, 使用普通模式
    ANC_DCC_TRIM_START,		//triming
    ANC_DCC_TRIM_USE,		//已有trim值
};

/*ANC模式使能位*/
#define ANC_OFF_BIT				BIT(1)	/*降噪关闭使能*/
#define ANC_ON_BIT				BIT(2)	/*降噪模式使能*/
#define ANC_TRANS_BIT			BIT(3)	/*通透模式使能*/

//ANC Ch ---eg:双声道 ANC_L_CH|ANC_R_CH
#define ANC_L_CH 					BIT(0)	//通道左
#define ANC_R_CH 	 				BIT(1)	//通道右

//ANC各类增益对应符号位
#define ANCL_FF_SIGN				BIT(0)	//左声道FF增益符号
#define ANCL_FB_SIGN				BIT(1)	//左声道FB增益符号
#define ANCL_TRANS_SIGN				BIT(2)  //左声道通透增益符号
#define ANCL_CMP_SIGN				BIT(3)  //左声道音乐补偿增益符号
#define ANCR_FF_SIGN				BIT(4)	//右声道FF增益符号
#define ANCR_FB_SIGN				BIT(5)	//右声道FB增益符号
#define ANCR_TRANS_SIGN				BIT(6)  //右声道通透增益符号
#define ANCR_CMP_SIGN				BIT(7)  //右声道音乐补偿增益符号

//DRC中断标志
#define ANCL_REF_DRC_ISR			BIT(0)
#define ANCL_ERR_DRC_ISR			BIT(1)
#define ANCR_REF_DRC_ISR			BIT(2)
#define ANCR_ERR_DRC_ISR			BIT(3)
#define ANC_ERR_DRC_ISR				ANCL_ERR_DRC_ISR | ANCR_ERR_DRC_ISR
#define ANC_REF_DRC_ISR				ANCL_REF_DRC_ISR | ANCR_REF_DRC_ISR

//DRC模块使能标志
#define ANC_DRC_FF_EN				BIT(0)	//DRC_FF使能标志
#define ANC_DRC_FB_EN				BIT(1)	//DRC_FB使能标志
#define ANC_DRC_TRANS_EN			BIT(2)	//DRC_TRANS使能标志

//ANC DUT音频模块使能控制, 可用于功耗测试
#define ANC_DUT_ANC_ENABLE			BIT(0)	//ANC_DUT ANC使能
#define ANC_DUT_ADC_ENABLE			BIT(1)	//ANC_DUT ADC使能
#define ANC_DUT_DAC_ENABLE			BIT(2)	//ANC_DUT DAC使能
#define ANC_DUT_CLOSE				ANC_DUT_ANC_ENABLE | ANC_DUT_ADC_ENABLE | ANC_DUT_DAC_ENABLE	//ANC_DUT模块关闭,正常开启所有模块

#define FF_IIR_EN  BIT(0)
#define FB_IIR_EN  BIT(1)
#define SZ_IIR_EN  BIT(2)
//ANC CORE IIR滤波器使能控制，可用于功耗测试
#define ANC_CORE_IIR_EN (FF_IIR_EN | FB_IIR_EN | SZ_IIR_EN)

/*
   ANC DUT IIR 用于功耗测试,
   使能后会使用对应指针的coeff[0-4]，
   覆盖 ANC_DUT_IIR_ORDER_NUM 数量的滤波器
   注意：bypass 类型滤波器与有效滤波器的功耗不同
*/
#define ANC_DUT_IIR_ORDER_EN		0		//测试IIR 模块使能
#define ANC_DUT_IIR_ORDER_NUM		10		//测试IIR 使能滤波器个数

//ANC自适应模式
#define ANC_ADAPTIVE_MANUAL_MODE	0		//自适应手动模式-手动切换等级
#define ANC_ADAPTIVE_GAIN_MODE		1		//自适应增益模式-自动切换等级

/*ANC训练方式*/
#define ANC_AUTO_UART  	   			0
#define ANC_AUTO_BT_SPP    			1
#define ANC_TRAIN_WAY 				ANC_AUTO_BT_SPP

/*SZ频响获取默认算法模式*/
//BR50 FB只支持ANC_ALOGM5~7, 测试SZ时选择FF的通道，支持更小采样率
#define ANC_SZ_EXPORT_DEF_ALOGM		ANC_ALOGM3

/*ANC 测试模式类型*/
#define ANC_TEST_TYPE_PCM			0	//MIC_SPK数据以PCM导出
#define ANC_TEST_TYPE_FFT			1	//SZ数据累加FFT导出

/*ANC CFG读写标志*/
#define ANC_CFG_READ		 		0x01
#define ANC_CFG_WRITE				0x02

//ANC 模拟MIC类型判断
#define IS_ANC_ANALOG_MIC(mic)     ((mic) >= A_MIC0 && (mic) < D_MIC0)
//ANC 数字MIC类型判断
#define IS_ANC_DIGITAL_MIC(mic)    ((mic) >= D_MIC0 && (mic) <= D_MIC3)

//ANC性能自动生成结构
typedef struct {
    u8  mode;				//当前训练步骤
    u8  train_busy;			//训练繁忙位 0-空闲 1-训练中 2-训练结束测试中
    u8	train_step;			//训练系数
    u8  ret_step;	 		//自适应训练系数值
    u8  auto_ts_en; 		//自动步进微调使能位
    u8  enablebit; 			//训练模式使能标志位 前馈|混合馈|通透
    u8 tool_time_flag;		//工具修改时间标志
    u8	mic_dma_export_en;	//从ANC DAM拿数，会消耗4*4K
    u8 	only_mute_train_en;	//只跑静音训练保持sz,fz系数
    u16 timerout_id;		//训练超时定时器ID
    s16 fb0_gain;
    s16 fb0sz_dly_en;
    u16 fb0sz_dly_num;
    int sz_gain;			//静音训练sz 增益
    u32 sz_lower_thr;		//误差MIC下限能量阈值
    u32 fz_lower_thr;   	//参考MIC下限能量阈值
    u32 sz_noise_thr;		//误差MIC底噪阈值
    u32 fz_noise_thr;   	//参考MIC底噪阈值
    u32 sz_adaptive_thr;	//误差MIC自适应收敛阈值
    u32 fz_adaptive_thr;	//参考MIC自适应收敛阈值
    u32 wz_train_thr;		//噪声训练阈值
} anc_train_para_t;

typedef struct {
    u8 mic_errmsg;
    u8 status;
    u32 dat[4];  //ff_num/ff_dat/fb_num/fb_dat
    u32 pow;
    u32 temp_pow;
    u32 mute_pow;
} anc_ack_msg_t;

//ANC场景自适应结构体
typedef struct {
    u8 default_flag;				//初始状态标志
    u8 trigger_cnt;					//触发计数 trigger_cnt*100ms
    u16 gain_decrease_fade_time;	//高增益等级 向当前等级淡入时间*100ms
    u16 gain_increase_fade_time;	//低增益等级 向当前等级淡入时间*100ms
    u16 gain;						//目标增益
    u32 pow_lthr;					//低阈值
    u32 pow_thr;					//高阈值
} anc_adap_param_t;

//ANC耳道自适应相关结构体
typedef struct {
    u8 ff_yorder;				//ANC耳道自适应FF IIR num
    u8 fb_yorder;				//ANC耳道自适应FB IIR num
    u8 cmp_yorder;				//ANC耳道自适应CMP IIR num
    double *lff_coeff;			/*耳道自适应-滤波器LFF暂存指针*/
    double *lfb_coeff;			/*耳道自适应-滤波器LFB暂存指针*/
    double *lcmp_coeff;			/*耳道自适应-滤波器LCMP暂存指针*/
    double *rff_coeff;			/*耳道自适应-滤波器RFF暂存指针*/
    double *rfb_coeff;			/*耳道自适应-滤波器RFB暂存指针*/
    double *rcmp_coeff;			/*耳道自适应-滤波器RCMP暂存指针*/
    u8  *ref_ff_curve;			/*耳道自适应-FF金机参考曲线*/
    int ref_ff_curve_len;  		/*耳道自适应-FF金机参考曲线长度*/
    void (*dma_done_cb)(void);
} anc_ear_adaptive_param_t;

//ANC 产测SZ获取结构体
struct anc_sz_fft_t {
    volatile u8 state;	//状态
    u8 data_sel;		//目标数据通道
    u8 alogm;			//采样率信息
    u8 check_flag;		//检查DAC有效数据标志
    u8 packet_cnt;		//overlap拼包标志
    u8 packet_lcnt;		//long buf overlap拼包标志
    volatile u8 busy;	//run 繁忙标志
    int all_sample_cnt;	//总样本计数
    int spk_sample_cnt[5];//spk区间样本计数
    int err_sample_cnt[5];//err区间样本次数
    s16 *packet_buf;	//overlap 拼包buf
    s16 *packet_lbuf;	//long buf overlap 拼包buf
    int *inbuf;			//输入数据buf
    float *out0_buf;	//输出数据buf0
    float *out1_buf;	//输出数据buf1
    float *out0_lbuf;	//输出数据lbuf0
    float *out1_lbuf;	//输出数据lbuf1
    void (*dma_run_hdl)(void);
};

//ANC ADT 参数结构
typedef struct {
    void (*dma_done_cb)(void);
} anc_adt_param_t;

struct anc_mic_gain_cmp_cfg {
    u8 en;	  			//MIC 补偿值使能控制  range 0-1;	   default 0;
    float lff_gain;		//ANCL FFmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float lfb_gain;		//ANCL FBmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float rff_gain;		//ANCR FFmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float rfb_gain;		//ANCR FBmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
};

//DCC TRIM相关数据结构
struct anc_dcc_trim_cfg {
    u8 mode;
    int lff_dcc;
    int lfb_dcc;
    int rff_dcc;
    int rfb_dcc;
};

//ANC param主要结构
typedef struct {
    u8 start;                       //ANC状态
    u8 mode;                        //ANC模式：降噪关;降噪开;通透...
    u8 production_mode;             //量产模式
    u8 developer_mode;				//开发者模式
    u8 anc_fade_en;                 //ANC淡入淡出使能
    u8 tool_enablebit;				//ANC动态使能，使用过程中可随意控制
    u8 debug_sel;					//ANCdebug data输出源
    u8 lff_en;						//左耳FF使能
    u8 lfb_en;						//左耳FB使能
    u8 rff_en;						//右耳FF使能
    u8 rfb_en;						//右耳FB使能
    u8 online_busy;					//在线调试繁忙标志位
    u8 fade_time_lvl;				//淡入时间等级，越大时间越长
    u8 mic_type[4];					//ANC mic类型控制位
    u8 dut_audio_enablebit;			//ANC_DUT 音频使能控制,用于分离模块功耗
    u8 adc_ch;						//adc数字通道
    u8 adaptive_mode;				//ANC场景自适应模式
    u8 anc_lvl;						//ANC等级
    u8 trans_fb_en;					//通透FB使能控制

    u8 lff_yorder;					//LFF IIR NUM
    u8 lfb_yorder;					//LFB IIR NUM
    u8 lcmp_yorder;					//LTRANS IIR NUM
    u8 ltrans_yorder;               //LCMP IIR NUM
    u8 rff_yorder;					//RFF IIR NUM
    u8 rfb_yorder;                  //RFB IIR NUM
    u8 rcmp_yorder;					//RCMP IIR NUM
    u8 rtrans_yorder;               //RTRANS IIR NUM
    u8 ltrans_fb_yorder;            //LTRANS FB IIR NUM
    u8 ltrans_cmp_yorder;           //LTRANS CMP IIR NUM
    u8 rtrans_fb_yorder;			//RTRANS FB IIR NUM
    u8 rtrans_cmp_yorder;			//RTRANS CMP IIR NUM
    u8 adap_ff_yorder;				//ANC耳道自适应FF IIR num
    u8 adap_fb_yorder;				//ANC耳道自适应FB IIR num
    u8 adap_cmp_yorder;				//ANC耳道自适应CMP IIR num

    u8 lr_lowpower_en;				//ANCLR(立体声)配置省功耗使能
    u8 anc_coeff_mode;				/*ANC coeff模式-0 普通coeff; 1 自适应coeff*/
    u8 test_type;					//ANC测试模式类型，获取SZ频响 or 获取SPK_MIC PCM数据

    u8 howling_detect_toggle;		//ANC 啸叫检测使能控制，用于在线切换
    u8 howling_detect_ch;			//ANC 啸叫检测通道配置

    u8 stereo_to_mono_mix;          //ANC 双->单声道控制

    u16 anc_fade_gain;				//ANC淡入淡出增益
    u16 drc_fade_gain;				//DRC淡出增益
    u16 drc_time_id;				//DRCtimeout定时器ID
    u16	enablebit;					//ANC模式:FF,FB,HYBRID
    u16 pow_export_en;				//ANC功率输出使能
    u32 coeff_size;					//ANC 读滤波器总长度
    u32 write_coeff_size;			//ANC 写滤波器总长度
    int train_err;					//训练结果 0:成功 other:失败
    u32 gains_size;					//ANC 滤波器长度

    int *debug_buf;					//测试数据流，最大65536byte(堆)
    s32 *lfir_coeff;				//FZ补偿滤波器表
    s32 *rfir_coeff;				//FZ补偿滤波器表

    double *lff_coeff;				//左耳FF滤波器
    double *lfb_coeff;				//左耳FB滤波器
    double *lcmp_coeff;				//左耳CMP滤波器
    double *ltrans_coeff;			//左耳通透滤波器
    double *rff_coeff;				//右耳FF滤波器
    double *rfb_coeff;  	    	//右耳FB滤波器
    double *rcmp_coeff;				//右耳CMP滤波器
    double *rtrans_coeff;			//右耳通透滤波器
    double *ltrans_fb_coeff;        //左耳通透FB滤波器
    double *ltrans_cmp_coeff;       //左耳通透CMP滤波器
    double *rtrans_fb_coeff;        //右耳通透FB滤波器
    double *rtrans_cmp_coeff;       //右耳通透CMP滤波器

    float ltrans_fbgain;  			//ANCL 通透FB增益(携带符号)     range 0.0316(-30dB) - 31.62
    float ltrans_cmpgain;  			//ANCL 通透CMP增益(携带符号)     range 0.0316(-30dB) - 31.62
    float rtrans_fbgain;  			//ANCR 通透FB增益(携带符号)     range 0.0316(-30dB) - 31.62
    float rtrans_cmpgain;  			//ANCR 通透CMP增益(携带符号)     range 0.0316(-30dB) - 31.62

    volatile u8 ch;					//ANC通道选择 ： ANC_L_CH | ANC_R_CH

    anc_gain_param_t gains;

    audio_adc_mic_mana_t mic_param[AUDIO_ADC_MAX_NUM];	//ANC MIC 控制参数

    anc_train_para_t train_para;//训练参数结构体
    anc_ear_adaptive_param_t *adaptive;
    anc_adt_param_t *adt;
    struct anc_sz_fft_t sz_fft;
    struct anc_mic_gain_cmp_cfg mic_cmp;

    void (*train_callback)(u8, u8);
    void (*pow_callback)(anc_ack_msg_t *msg_t, u8 setp);
    int (*cfg_online_deal_cb)(u8, anc_gain_t *);
    void (*post_msg_drc)(void);
    void (*post_msg_debug)(void);
    void (*biquad2ab)(float gain, float f, float fs, float q, double *a0, double *a1, double *a2, double *b0, double *b1, double *b2, u8 type);
    void (*mult_gain_set)(u8 gain_id, void *buf, int len);

    u8 adt_state;//智能免摘开启状态


    //gali br52/br50/br56
    u8 drc_toggle;					//drc 开关
    u8 dcc_adaptive_toggle;			//自适应dcc 开关

    u8 ff_cic_shift;				//FF CIC输出粗调增益
    u8 fb_cic_shift;				//FB CIC输出粗调增益
    u8 train_fb_to_ff;				//ANC TRAIN FBMIC复用FF的通道测试

    u8 ff_use_alogm;				//FF当前使用的算法模式
    u8 fb_use_alogm;				//FB当前使用的算法模式
    u8 sz_use_alogm;				//SZ当前使用的算法模式

    // fade_time = (1/fs) * (slow+1）* [16384 / (2^fast)]
    u8 ff_filter_fade_fast;		    //滤波器切换快步进, def 0
    u8 ff_filter_fade_slow;			//滤波器切换慢步进, def 3

    u8 fb_filter_fade_fast;		    //滤波器切换快步进, def 0
    u8 fb_filter_fade_slow;			//滤波器切换慢步进, def 3

    struct anc_dcc_trim_cfg dcc_trim;

    //gali br28
    u8 drc_dcc_en;					//drc动态调整DCC使能
    void (*adc_set_buffs_cb)(void);
} audio_anc_t;

//ANC场景自适应相关结构体
struct anc_power_adaptive_cfg {
    u8 ref_max_lvl;
    u8 err_max_lvl;
    int det_time;					//ANC 场景自适应检测时间
    anc_adap_param_t *ref_param;	//配置前馈自适应参数
    anc_adap_param_t *err_param;	//配置后馈自适应参数
    audio_anc_t *param;				//ANC句柄
    void (*fade_gain_set)(u16 gain);
    void (*event_cb)(u8 lvl, enum ANC_ADAPTIVE_FADE_EVENT event);
    void (*pow_output_cb)(int pow);
};

#define ANC_DB_HEAD_LEN		20/*ANC配置区数据头长度*/
#define ANCIF_GAIN_TAG_01	"ANCGAIN01"
#define ANCIF_COEFF_TAG_01	"ANCCOEF01"
#define ANCIF_GAIN_TAG		ANCIF_GAIN_TAG_01	//当前增益配置版本
#define ANCIF_COEFF_TAG		ANCIF_COEFF_TAG_01	//当前系数配置版本
#define ANCIF_HEADER_LEN	10
typedef struct {
    u32 total_len;	//4 后面所有数据加起来长度
    u16 group_crc;	//6 group_type开始做的CRC，更新数据后需要对应更新CRC
    u16 group_type;	//8
    u16 group_len;  //10 header开始到末尾的长度
    char header[ANCIF_HEADER_LEN];//20
    int coeff[0];
} anc_db_head_t;

#define _PACKED __attribute__((packed))
struct anc_param_head_t {
    u16 id;
    u16 offset;
    u16 len;
} _PACKED;

/*ANCIF配置区滤波器系数anc_coeff.bin*/
typedef struct {
    u16 version;
    u16 cnt;
    u8 dat[0];	//小心double访问非对齐异常
} anc_coeff_t;

/* ANC 数据拼接结构体 */
typedef struct {
    u16 version;
    u16 cnt;
    u16 last_len;
    u16 last_total_len;
    u16 dat_len;	//记录数据包dat的长度
    u8 *dat;		//指向数据包的地址
} anc_packet_data_t;

typedef struct {
    u8 type;
    float a[3];
} _PACKED anc_fr_t;

typedef struct {
    float iir_gain;
} _PACKED anc_mult_gain_t;

typedef struct {
    anc_fr_t *lff;
    anc_fr_t *lfb;
    anc_fr_t *lcmp;
    anc_fr_t *rff;
    anc_fr_t *rfb;
    anc_fr_t *rcmp;
} anc_iir_t;


int audio_anc_train(audio_anc_t *param, u8 en);

/*
 *Description 	: audio_anc_run
 *Arguements  	: param is audio anc param
 *Returns	 	: 0  open success
 *		   		: -EPERM 不支持ANC
 *		   		: -EINVAL 参数错误
 *Notes			:
 */
int audio_anc_run(audio_anc_t *param);

int audio_anc_close();

/*
*********************************************************************
*                  	audio_anc_fade_cfg_set
* Description: ANC fade 基础配置
* Arguments  : en 淡入淡出使能;
			   step 默认1，淡入步进，越大越快越容易有杂音
			   slow 默认0，缓慢步进，越大越慢
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_anc_fade_cfg_set(u8 en, u8 step, u8 slow);

/*
*********************************************************************
*                  	audio_anc_fade
* Description: ANC fade 增益配置
* Arguments  : ch 目标通道(enum ANC_fade_ch_t);
			   gain 目标fade增益
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_anc_fade(u8 ch, u16 gain);

/*
*********************************************************************
*                  	audio_anc_fade_dly_get
* Description: ANC根据采样率计算，淡出淡入时间
* Arguments  : target_value 淡入/出目标值, alogm 当前采样率
* Return	 : dly 延时时间(ms)
* Note(s)    : None.
*********************************************************************
*/
int audio_anc_fade_dly_get(int target_value, int alogm);

/*
*********************************************************************
*                  	anc_fade_ctr_ch_check
* Description: ANC fade 通道校验
* Arguments  : ch 目标通道
* Return	 : 当前系列支持的通道
* Note(s)    : None.
*********************************************************************
*/
u8 anc_fade_ctr_ch_check(u8 ch);

u8 anc_api_get_fade_en(void);

//更新所有滤波器
void anc_coeff_online_update(audio_anc_t *param, u8 hd_reset_en);

//更新FF滤波器
void anc_coeff_ff_online_update(audio_anc_t *param);

//更新FB滤波器
void anc_coeff_fb_online_update(audio_anc_t *param);


int anc_coeff_size_count(audio_anc_t *param);

/*ANC配置id*/
enum {
    ANC_DB_COEFF,	//ANC系数
    ANC_DB_GAIN,	//ANC增益
    ANC_DB_MAX,
};

typedef struct {
    u8 state;		//ANC配置区状态
    u16 total_size;	//ANC配置区总大小
} anc_db_t;

/*anc配置区初始化*/
int anc_db_init(void);
/*根据配置id获取不同的anc配置*/
int *anc_db_get(u8 id, u32 *len);
/*
 *gain:增益配置,没有的话传NULL
 *coeff:系数配置,没有的话传NULL
 */
int anc_db_put(audio_anc_t *param, anc_gain_t *gain, anc_coeff_t *coeff);
/*anc 音乐补偿模块在线开关*/
void audio_anc_cmp_en(audio_anc_t *param, u8 en);

int anc_coeff_single_fill(audio_anc_t *param, anc_coeff_t *target_coeff);

void audio_anc_dcc_set(u8 ref_1_dcc, u8 err_1_dcc, u8 ref_2_dcc, u8 err_2_dcc);

void audio_anc_gain_sign(audio_anc_t *param);

void audio_anc_drc_ctrl(audio_anc_t *param, float drc_ratio);

void audio_anc_en_set(u8 en);

void audio_anc_norgain(s16 ffgain, s16 fbgain);
/*anc ff增益设置API*/
void audio_anc_ffgain_set(s16 l_gain, s16 r_gain);
/*anc fb增益设置API*/
void audio_anc_fbgain_set(s16 l_gain, s16 r_gain);

void audio_anc_tool_en_ctrl(audio_anc_t *param, u8 enablebit);

int audio_anc_gains_version_verify(audio_anc_t *param, anc_gain_t *cfg_target);

void audio_anc_drc_process(void);

void audio_anc_max_yorder_verify(audio_anc_t *param);

int audio_anc_fade_dly_get(int target_value, int alogm);

int audio_anc_param_normalize(audio_anc_t *param);

void audio_anc_param_init(audio_anc_t *param);

int anc_cfg_online_deal(u8 cmd, anc_gain_t *cfg);

void audio_anc_drc_process(void);

void audio_anc_debug_data_output(void);

void audio_anc_debug_cbuf_sel_set(u8 sel);

void audio_anc_debug_extern_trigger(u8 flag);

/*ANC模块复位*/
void audio_anc_reset(audio_anc_t *param, u8 fade_en);

/********************工具交互API*************************/

/*设置结果回调函数*/
void anc_api_set_callback(void (*callback)(u8, u8), void (*pow_callback)(anc_ack_msg_t *, u8));

/*设置淡入淡出使能*/
void anc_api_set_fade_en(u8 en);

/*获取参数结构体*/
anc_train_para_t *anc_api_get_train_param(void);

/*获取步进*/
u8 anc_api_get_train_step(void);

/*设置通透目标能量值*/
void anc_api_set_trans_aim_pow(u32 pow);

/*ANC在线调试标志设置*/
void anc_online_busy_set(u8 en);

/*ANC在线调试标志获取*/
u8 anc_online_busy_get(void);

/*ANC滤波器大小设置*/
void anc_coeff_size_set(u8 mode, int len);

/*ANC滤波器大小获取*/
int anc_coeff_size_get(u8 mode);

/*ANC滤波器参数填充*/
int anc_coeff_fill(anc_coeff_t *db_coeff);

/*ANC功率数据获取，长度为8byte*/
void audio_anc_pow_get(u16 *pow);

/*ANC功率输出使能控制*/
void audio_anc_pow_export_en(u8 en, u8 tool_en);

/*ANC滤波器格式校验*/
int anc_coeff_check(anc_coeff_t *db_coeff, u16 len);

/*ANC量产模式设置*/
void anc_production_mode_set(u8 en);

/*ANC gains 单项写接口,支持在线更新, align 4byte*/
int anc_cfg_single_param_update(u8 id, void *buf, int len);

/*ANC gains 单项读接口*/
int anc_cfg_single_param_read(u8 id, int *data);

void audio_anc_fr_format(u8 *out_dat, float *fr, u8 order, const u8 *type);

void audio_anc_adaptive_data_analsis(anc_iir_t *iir);

anc_packet_data_t *anc_data_catch(anc_packet_data_t *row_data_packet, u8 *buf, u16 len, u16 id, u8 init_flag);

int anc_adaptive_ff_ref_data_get(u8 **buf);

void anc_adaptive_ff_ref_data_free(void);

int anc_adaptive_fb_ref_data_get(u8 **buf);

void anc_adaptive_ff_ref_data_save(u8 *buf, int len);

/*******************ANC增益自适应************************/
/*ANC自适应初始化*/
void anc_pow_adap_init(struct anc_power_adaptive_cfg *cfg);

/*ANC自适应启动*/
void audio_anc_pow_adap_start();

/*ANC自适应关闭*/
void audio_anc_pow_adap_stop(void);

/*
   ANC自适应复位：
   1、恢复默认增益；
   2、非ANC模式关闭，ANC模式重启
 */
void audio_anc_power_adaptive_reset(void);

/*ANC自适应增益切换定时器注册*/
void anc_pow_adap_fade_timer_add(int ref_lvl, int err_lvl);

/*ANC自适应当前增益等级获取*/
int anc_pow_adap_lvl_get(u8 anc_type);

/*设置当前的ANC增益等级, 只有普通增益模式才可以设置*/
void audio_anc_lvl_set(u8 lvl, u8 ex_en);

void free_anc_data_packet(anc_packet_data_t *anc_data_packet);

void audio_anc_sz_fft_start(audio_anc_t *param, int len);
void audio_anc_sz_fft_run(audio_anc_t *param);
void audio_anc_sz_fft_stop(audio_anc_t *param);
int audio_anc_sz_fft_outbuf_get(u8 **p);
void audio_anc_sz_fft_outbuf_release(void);
void audio_anc_sz_fft_trigger(void);

void anc_user_train_process(audio_anc_t *param);

/*ANC DMA SEL通道映射*/
void audio_anc_dma_sel_map(audio_anc_t *param, u8 *debug_sel);

/* ANC DMA 中断控制 */
void anc_dma_ie(u8 en);

/* ANC DMA 停止 */
void anc_dma_stop(void);

/*停止ANC DMA， 并关闭对应中断*/
void anc_dma_off(void);

/*获取DMA中断数据 PINGPONG BUF标志- double 模式使用*/
u8 anc_dma_done_ppflag(void);

/*
 ANC DMA 2通道初始化 full stop(一次中断，数据满则停止)
	param:  out_sel DMA通道sel(ch 0/1 数据)
			buf ANC DMA BUF
			irq_point ANC DMA IRQ_POINT
 */
void anc_dma_on(u8 out_sel, int *buf, int irq_point);

/*
 ANC DMA 2通道初始化 double (PINGPONG BUFF, 持续收数)
	param: 	out_sel DMA通道sel(ch 0/1 数据)
			buf ANC DMA BUF
			irq_point ANC DMA IRQ_POINT
 */
void anc_dma_on_double(u8 out_sel, int *buf, int irq_point);

/* 注册ANC DMA输出回调函数 */
void audio_anc_dma_add_output_handler(const char *name, void (*output)(void));

/* 删除ANC DMA输出回调函数 */
void audio_anc_dma_del_output_handler(const char *name);

void audio_anc_biquad2ab_double(anc_fr_t *iir, double *out_coeff, u8 order, int alogm);

/*读取ANC dc值*/
int audio_anc_dc_vld_read(u8 mic_type);

//BR28
/*更新DRC 动态DCC使能设置*/
void anc_drc_dcc_en_set(audio_anc_t *param);


#endif/*_ANC_BASE_H_*/
