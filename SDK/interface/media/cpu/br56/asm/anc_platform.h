#ifndef _ANC_PLATFROM_H_
#define _ANC_PLATFROM_H_

#include "generic/typedef.h"

/*ANC芯片版本定义(只读)*/
#define ANC_CHIP_VERSION			ANC_VERSION_BR56

#define ANC_GAINS_VERSION 		0X7100  //结构体版本号信息

/* ANC MIC类型定义 */
#define A_MIC0        0x00  // 模拟MIC0 对应IO-0(PA1 PA2)
#define A_MIC1        0x01  // 模拟MIC1 对应IO-0(PB10 PB9)

//BR56 MIC_SEL 0x02 没有意义
#define D_MIC0        0x03  // 数字MIC0(plnk_dat0_pin-上升沿采样)
#define D_MIC1        0x04  // 数字MIC1(plnk_dat1_pin-上升沿采样)
#define D_MIC2        0x05  // 数字MIC2(plnk_dat0_pin-下降沿采样)
#define D_MIC3        0x06  // 数字MIC3(plnk_dat1_pin-下降沿采样)

#define MIC_NULL      0xFF  // 没有定义相关的MIC

//6组滤波器组合(FF/FB/SZ), 无缝切换需要用到双倍滤波器，大于3组需使用96M 时钟
enum ANC_CORE_MDOE {
    ANC_MONO_HYBRID = 0,
    ANC_MONO_HYBRID_ADJ,
    ANC_MONO_FFX1,
    ANC_MONO_FFX2,
    ANC_STEREO_FF,
    ANC_MONO_FBX1,
    ANC_MONO_FBX2,
    ANC_STEREO_FB,
    ANC_MONO_FB_ADJ,
};

//DCC结构
typedef struct {
    u8  norm_dc_par; 	// range 0-15;    default 0
    u8  pwr_dc_par;   	// range 0-15;    default 2
    u8  amp1_dc_par;  	// range 0-15;    default 0
    u8  amp2_dc_par;  	// range 0-15;    default 6
    u16 pwr_thr;     	// range 0-65535; default 12000
    u16 amp_thr;     	// range 0-65535; default 12000

    u8  adj_dc_par1;  	// range 0-15;    default 0    // allow writing at any time
    u8  adj_dc_par2;  	// range 0-15;    default 6
    u8  adj_incr_step;	// range 0-7;     default 0
    u8  adj_upd_sample;	// range 0-7;     default 0
} anc_core_dcc_t;

//DRC结构
typedef struct {
    u16 threshold;		//drc threshold                              range 0-1023;  default 531
    u8  ratio;    		//drc ratio                                  range 4-255;   default 255
    u8  mkg;      		//drc makeup-gain                            range 0-6;     default 0
    u8  knee;     		//drc kneewidth                     	     range 4-255;   default 64
    u8  att_time; 		//drc attack time                            range 0-19;    default 3
    u8  rls_time; 		//drc release time                           range 0-19;    default 13
    u8  bypass;   		//DRC bypass: 0@not bypass DRC, 1@bypass DRC range 0-1;     default 1
} anc_core_drc_t;

typedef struct {
//cfg
    u16 version;		//当前结构体版本号
    u8 dac_gain;		//dac模拟增益 			range 0-3;   default 3
    u8 l_ffmic_gain;	//ANCL FFmic增益 		range 0-7;   default 2
    u8 l_fbmic_gain;	//ANCL FBmic增益		range 0-7;   default 2
    u8 r_ffmic_gain;	//ANCR FFmic增益		range 0-7;	 default 2
    u8 r_fbmic_gain;	//ANCR FBmic增益		range 0-7;	 default 2
    u8 cmp_en;			//音乐补偿使能			range 0-1;   default 1

    u8 drc_en;		    //DRC使能				range 0-7;   default 0
    u8 ahs_en;		    //AHS使能				range 0-1;   default 0
    u8 gain_sign; 		//ANC各类增益的符号     range 0-255; default 0
    u8 noise_lvl;		//训练的噪声等级		range 0-255; default 0

    u8 ff_alogm;		//ANC FF算法因素		range 0-7;	default 5
    u8 fb_alogm;		//ANC FB算法因素		range 4-7;	default 5
    u8 cmp_alogm;    	//ANC CMP算法因素		range 0-7;	default 5
    u8 trans_alogm;    	//通透 算法因素			range 0-7;	default 5

    float l_ffgain;	    //ANCL FF增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float l_fbgain;	    //ANCL FB增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float l_transgain;  //ANCL 通透增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float l_cmpgain;	//ANCL 音乐补偿增益		range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_ffgain;	    //ANCR FF增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_fbgain;	    //ANCR FB增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_transgain;  //ANCR 通透增益			range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_cmpgain;	//ANCR 音乐补偿增益		range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)

    anc_core_drc_t drc_top_ff;  	//8  byte
    anc_core_drc_t drc_top_fb;  	//8  byte
    anc_core_drc_t drc_top_trans;  	//8  byte
    anc_core_drc_t drc_core_ff;		//8  byte
    anc_core_drc_t drc_core_fb;		//8  byte
    anc_core_drc_t drc_core_trans; 	//8  byte
    anc_core_drc_t drc_dac_mux;		//8  byte

    anc_core_dcc_t dcc_ff;		//12 byte
    anc_core_dcc_t dcc_fb;		//12 byte

    u8 ahs_dly;			  //AHS_DLY				range 0-15;	   default 1;
    u8 ahs_tap;			  //AHS_TAP				range 0-255;   default 100;
    u8 ahs_wn_shift;	  //AHS_WN_SHIFT		range 0-15;	   default 9;
    u8 ahs_wn_sub;	  	  //AHS_WN_SUB  		range 0-1;	   default 1;
    u16 ahs_shift;		  //AHS_SHIFT   		range 0-65536; default 210;
    u16 ahs_u;			  //AHS步进				range 0-65536; default 4000;
    s16 ahs_gain;		  //AHS增益				range -32767-32767;default -1024;
    u8 ahs_nlms_sel;	  //AHS_NLMS			range 0-1;	   default 0;
    u8 developer_mode;	  //GAIN开发者模式		range 0-1;	   default 0;

    float audio_drc_thr;  //Audio DRC阈值       range -6.0-0;  default -6.0dB;

    u8 dcc_mode;		  //DCC模式			    range 0-2;	   default 0;
    u8 adaptive_ref_en;	  //耳道自适应-金机曲线参考使能
    u8 mic_cmp_gain_en;	  //MIC 补偿值使能控制  range 0-1;	   default 0;
    u8 reserve_1;		  //预留位

    float adaptive_ref_fb_f;	//FB参考F最深点位置中心值   range 80-200; default 135
    float adaptive_ref_fb_g;	//FB参考G最深点深度   range 12-22; default 18
    float adaptive_ref_fb_q;	//FB参考Q最深点降噪宽度  range 0.4-1.2; default 0.6

    float l_ffmic_cmp_gain;		//ANCL FFmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float l_fbmic_cmp_gain;		//ANCL FBmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_ffmic_cmp_gain;		//ANCR FFmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)
    float r_fbmic_cmp_gain;		//ANCR FBmic 补偿增益(产测使用), range 0.0316(-30dB) - 31.622(+30dB); default 1.0(0dB)

    //相关性检测 HD param
    u8 hd_en;				//啸叫检测使能		range 0-1;	   default 1;
    u8 hd_corr_thr;			//相关性检测阈值	range 0-255;   default 232;
    u16 hd_corr_gain;		//相关性预设增益	range 0-1024	default 512;
    u8 hd_corr_dly;			//相关性检测延时	range 2-30;	   default 30;

    //功率检测全频带，增益可预设
    u8 hd_pwr_rate;			//功率检测回音处理前后比例		 	range 0-3; 	   default 2;
    u8 hd_pwr_ctl_gain_en;	//功率检测是否使用预设的增益使能 	range 0-1; 	   default 0;
    u8 hd_pwr_ctl_ahsrst_en;//功率检测是否复位AHS 			 	range 0-1; 	   default 1;
    u16 hd_pwr_thr;			//功率检测阈值设置,增益自动 	 	range 0-32767; default 18000;
    u16 hd_pwr_ctl_gain;	//功率检测预设增益 				 	range 0-16384; default 1638;

    //可选带宽 增益自动调节
    u8 hd_pwr_ref_ctl_en;	//参考功率检测自动调增益使能 		range 0-1; 	   default 0;
    u8 hd_pwr_err_ctl_en;	//误差功率检测自动调增益使能 		range 0-1;     default 0;
    u16 hd_pwr_ref_ctl_hthr;	//参考功率检测H_THR,触发中断 	range 0-32767; default 2000;
    u16 hd_pwr_ref_ctl_lthr1;	//参考功率检测L1_THR 		 	range 0-32767; default 1000;
    u16 hd_pwr_ref_ctl_lthr2;	//参考功率检测L2_THR,小于L1_THR range 0-32767; default 200;

    u16 hd_pwr_err_ctl_hthr;	//误差功率检测H_THR,触发中断 	range 0-32767; default 2000;
    u16 hd_pwr_err_ctl_lthr1;	//误差功率检测L1_THR 		 	range 0-32767; default 1000;
    u16 hd_pwr_err_ctl_lthr2;	//误差功率检测L2_THR,小于L1_THR range 0-32767; default 200;
    u8 reserve_2;
    u8 reserve_3;

} anc_gain_param_t;

/*ANCIF配置区滤波器系数 anc_gains.bin*/
typedef struct {
    anc_gain_param_t gains;
    //236 + 20byte(header)
    u8 reserve[236 - 204];		//204 = 176 + 28(howl param)
    // u8 reserve[236 - 176];	//工具端说明大小
} anc_gain_t;

/*
 ANC DMA 4通道初始化 double (PINGPONG BUFF, 持续收数)
	param:	ch1_out_sel DMA通道ch1 sel(ch 0/1 数据)
			ch2_out_sel DMA通道ch2 sel(ch 2/3 数据)
			buf ANC DMA BUF
			irq_point ANC DMA IRQ_POINT
 */
void anc_dma_on_double_4ch(u8 ch1_out_sel, u8 ch2_out_sel, int *buf, int irq_point);

void audio_anc_common_param_init(void);

#endif/*_ANC_PLATFROM_H_*/
