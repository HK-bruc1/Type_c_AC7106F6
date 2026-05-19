
#ifndef _ANC_EXT_TOOL_CFG_H_
#define _ANC_EXT_TOOL_CFG_H_

#include "typedef.h"

/************************************************
  			ANC EXT Config struct
Notes:ANC_EXT 工具参数结构体声明，支持ICSD LIB引用
	  仅放参数结构体或洽谈ICSD LIB会引用的定义
************************************************/

//耳道自适应 - BASE界面参数
struct __anc_ext_ear_adaptive_base_cfg {
    s8 calr_sign[4];	//校准符号（SZ/PZ/bypass/performance）

    u8 sign_trim_en;	//符号使能校准，校准过则为1
    s8 vld1;			//入耳检测阈值1				default:-10    	range:-128~127 
    s8 vld2;			//入耳检测阈值2   			default:-9 		range:-128~127 
    u8 adaptive_mode;	//自适应结果模式  			default:0 		range:0~1 

    u8 adaptive_times;	//自适应训练次数   			default:1 		range:1~10 
    u8 reserved[3];		//保留位

    u16 tonel_delay;	//提示应延时n(ms)获取szl	default:50   	range:0~10000
    u16 toner_delay;	//提示应延时n(ms)获取szr	default:2400  	range:0~10000
    u16 pzl_delay;		//提示应延时n(ms)获取pzl	default:900    	range:0~10000
    u16 pzr_delay;		//提示应延时n(ms)获取pzr	default:3200	range:0~10000
    float bypass_vol;	//bypass音量				default:1(0dB) 	range:0~100  
    float sz_pri_thr;	//提示音，bypass优先级阈值	default:60    	range:0~200  
};	//28byte

//IIR滤波器单元
struct __anc_ext_iir {
    float fre;
    float gain;
    float q;
};

//耳道自适应 - 滤波器界面-滤波器参数单元 高/中/低
//AEQ/CMP 滤波器参数
struct __anc_ext_ear_adaptive_iir {
    u8 type;								//滤波器类型
    u8 fixed_en;							//固定使能
    u8 reserved[2];
    struct __anc_ext_iir def;				//默认滤波器
    struct __anc_ext_iir upper_limit;		//调整上限
    struct __anc_ext_iir lower_limit;		//调整下限
};	//40byte

//耳道自适应 - 滤波器界面通用参数
struct __anc_ext_ear_adaptive_iir_general {
    u8 biquad_level_l;				//滤波器档位划分下限 default:5    range:0~255
    u8 biquad_level_h;				//滤波器档位划分上限 default:9    range:0~255
    u8 reserved[2];
    float total_gain_freq_l;		//总增益对齐频率下限 default:350  range:0~2700
    float total_gain_freq_h;		//总增益对齐频率上限 default:650  range:0~2700
    float total_gain_limit;			//滤波器最高增益限制 default:20   range:-80~80
};	//16byte

//耳道自适应 - 滤波器界面 gain参数 高/中/低
struct __anc_ext_ear_adaptive_iir_gains {
    float iir_target_gain_limit;	//滤波器和target增益差限制 default:0  range:-30~30
    float def_total_gain;			//默认增益  default 1.0(0dB)  range 0.0316(-30dB) - 31.622(+30dB);
    float upper_limit_gain;			//增益调整上限 default 2.0(6dB) range 0.0316(-30dB) - 31.622(+30dB);
    float lower_limit_gain;			//增益调整下限 default 0.5(-6dB) range 0.0316(-30dB) - 31.622(+30dB);
};	//16byte

//耳道自适应 - 权重和性能界面 参数
struct __anc_ext_ear_adaptive_weight_param {
    u8 	mse_level_l;			//低性能档位划分 	default:5    range:0~255
    u8 	mse_level_h;			//高性能档位划分 	default:9    range:0~255
    u8 reserved[2];
}; //4byte

//耳道自适应 - 权重和性能界面 权重曲线
struct __anc_ext_ear_adaptive_weight {
    float data[60];
}; //240byte

//耳道自适应 - 权重和性能界面 性能曲线
struct __anc_ext_ear_adaptive_mse {
    float data[60];
}; //240byte

//耳道自适应 - TARGET界面 参数
struct __anc_ext_ear_adaptive_target_param {
    u8 cmp_en;
    u8 cmp_curve_num;
    u8 reserved[2];
}; //4byte

//耳道自适应 - TARGET界面 sv
struct __anc_ext_ear_adaptive_target_sv {
    float data[7 * 11];
};	//308 byte

//耳道自适应 - TARGET界面 cmp
struct __anc_ext_ear_adaptive_target_cmp {
    float data[2 * 60 * 11];	//BR28 长度为75 * 11
};	//5180 byte

//耳道自适应 - 耳道记忆界面 参数
struct __anc_ext_ear_adaptive_mem_param {
    u8 mem_curve_nums;          // 记忆曲线数目：DLL函数输出结果  default:0  range:0~60
    s8 ear_recorder;			//耳道记忆使能				default:0    	range:0,1
    u8 reserved[2];
};

//耳道自适应 - 耳道记忆界面 pz/sz数据
struct __anc_ext_ear_adaptive_mem_data {
    float data[0];      // DLL函数输出数组，实际大小为25*2*mem_curve_nums
};

//耳道自适应 - 产测数据 pz/sz数据
struct __anc_ext_ear_adaptive_dut_data {
    float data[0];
};

//RTANC 配置
struct __anc_ext_rtanc_adaptive_cfg {
    // Wind Controller
    u8  wind_det_en;           //def:0     ; range:[0, 1]       ; precision:1
    u8  wind_lvl_thr;          //def:30    ; range:[0, 255]     ; precision:1
    u8  wind_lock_cnt;         //def:4     ; range:[0, 255]     ; precision:1
    u8  idx_use_same;          //def:1     ; range:[0, 1]       ; precision:1

    // mode1 AEQ_CMP Controller
    u8  m1_dem_cnt0;           //def:5     ; range:[0, 255]     ; precision:1
    u8  m1_dem_cnt1;           //def:4     ; range:[0, 255]     ; precision:1
    u8  m1_dem_cnt2;           //def:10    ; range:[0, 255]     ; precision:1
    u8  m1_scnt0;              //def:3     ; range:[0, 255]     ; precision:1
    u8  m1_scnt1;              //def:3     ; range:[0, 255]     ; precision:1
    u8  m1_dem_off_cnt0;       //def:10    ; range:[0, 255]     ; precision:1
    u8  m1_dem_off_cnt1;       //def:10    ; range:[0, 255]     ; precision:1
    u8  m1_dem_off_cnt2;       //def:10    ; range:[0, 255]     ; precision:1
    u8  m1_scnt0_off;          //def:5     ; range:[0, 255]     ; precision:1
    u8  m1_scnt1_off;          //def:5     ; range:[0, 255]     ; precision:1
    u8  m1_scnt2_off;          //def:10    ; range:[0, 255]     ; precision:1
    u8  m1_idx_thr;            //def:7     ; range:[0, 255]     ; precision:1

    // Music Controller
    u8  msc_iir_idx_thr;       //def:5     ; range:[0, 255]     ; precision:1
    u8  msc_atg_sm_lidx;       //def:9     ; range:[0, 255]     ; precision:1
    u8  msc_atg_sm_hidx;       //def:24    ; range:[0, 255]     ; precision:1
    u8  msc_atg_diff_lidx;     //def:10    ; range:[0, 255]     ; precision:1
    u8  msc_atg_diff_hidx;     //def:25    ; range:[0, 255]     ; precision:1
    u8  msc_spl_idx_cut;       //def:20    ; range:[0, 255]     ; precision:1
    u8  msc_dem_thr0;          //def:8     ; range:[0, 255]     ; precision:1
    u8  msc_dem_thr1;          //def:10    ; range:[0, 255]     ; precision:1
    u8  msc_mat_lidx;          //def:5     ; range:[0, 25]      ; precision:1
    u8  msc_mat_hidx;          //def:12    ; range:[0, 25]      ; precision:1
    u8  msc_idx_thr;           //def:6     ; range:[0, 255]     ; precision:1
    u8  msc_scnt_thr;          //def:2     ; range:[0, 255]     ; precision:1
    u8  msc2norm_updat_cnt;    //def:5     ; range:[0, 255]     ; precision:1
    u8  msc_lock_cnt;          //def:1     ; range:[0, 255]     ; precision:1
    u8  msc_tg_diff_lidx;      //def:5     ; range:[0, 25]      ; precision:1
    u8  msc_tg_diff_hidx;      //def:12    ; range:[0, 25]      ; precision:1

    // CMP Controller
    u8  cmp_diff_lidx;         //def:2     ; range:[0, 25]      ; precision:1
    u8  cmp_diff_hidx;         //def:12    ; range:[0, 25]      ; precision:1
    u8  cmp_idx_thr;           //def:4     ; range:[0, 255]     ; precision:1
    u8  cmp_adpt_en;           //def:0     ; range:[0, 1]       ; precision:1

    // Other
    u8  spl2norm_cnt;          //def:5     ; range:[0, 255]     ; precision:1
    u8  talk_lock_cnt;         //def:3     ; range:[0, 255]     ; precision:1

    // Large Noise Controller
    u8  noise_idx_thr;         //def:6     ; range:[0, 255]     ; precision:1
    u8  noise_updat_thr;       //def:20    ; range:[0, 255]     ; precision:1
    u8  noise_ffdb_thr;        //def:20    ; range:[0, 255]     ; precision:1

    // LMS Controller
    u8  mse_lidx;              //def:5     ; range:[0, 25]      ; precision:1
    u8  mse_hidx;              //def:12    ; range:[0, 25]      ; precision:1

    // Fast Update Controller
    u8  fast_cnt;              //def:2     ; range:[0, 255]     ; precision:1
    u8  fast_ind_thr;          //def:20    ; range:[0, 255]     ; precision:1
    u8  fast_ind_div;          //def:2     ; range:[0, 255]     ; precision:1

    // Update Controller
    u8  norm_mat_lidx;         //def:5     ; range:[0, 25]      ; precision:1
    u8  norm_mat_hidx;         //def:12    ; range:[0, 25]      ; precision:1
    u8  tg_lidx;               //def:5     ; range:[0, 25]      ; precision:1
    u8  tg_hidx;               //def:12    ; range:[0, 25]      ; precision:1
    u8  rewear_idx_thr;        //def:4     ; range:[0, 255]     ; precision:1
    u8  norm_updat_cnt0;       //def:10    ; range:[0, 255]     ; precision:1
    u8  norm_updat_cnt1;       //def:0     ; range:[0, 255]     ; precision:1
    u8  norm_cmp_cnt0;         //def:8     ; range:[0, 255]     ; precision:1
    u8  norm_cmp_cnt1;         //def:3     ; range:[0, 255]     ; precision:1
    u8  norm_cmp_lidx;         //def:5     ; range:[0, 25]      ; precision:1
    u8  norm_cmp_hidx;         //def:12    ; range:[0, 25]      ; precision:1
    u8  updat_lock_cnt;        //def:2     ; range:[0, 255]     ; precision:1

    // Fitting Algorithm Controller
    u8  ls_fix_mode;           //def:0     ; range:[0, 1]       ; precision:1
    u8  ls_fix_lidx;           //def:5     ; range:[0, 25]      ; precision:1
    u8  ls_fix_hidx;           //def:10    ; range:[0, 25]      ; precision:1
    u8  iter_max0;             //def:20    ; range:[0, 255]     ; precision:1
    u8  iter_max1;             //def:20    ; range:[0, 255]     ; precision:1

    // Tight Divide Controller
    u8  tight_divide;			//def:0     ; range:[0, 255]    ; precision:1
    u8  tight_idx_thr;			//def:40    ; range:[0, 255]    ; precision:1
    u8  tight_idx_diff;			//def:4     ; range:[0, 255]    ; precision:1

    u8  undefine_param0[22];   //def:0     ; range:[0, 1000]    ; precision:1

    // Wind Controller (Float)
    float wind_ref_thr;        //def:100   ; range:[0, 1000]    ; precision:0.1
    float wind_ref_max;        //def:110   ; range:[0, 1000]    ; precision:0.1
    float wind_ref_min;        //def:12    ; range:[0, 1000]    ; precision:0.1
    float wind_miu_div;        //def:100   ; range:[0, 1000]    ; precision:0.1

    // Music Controller (Float)
    float msc_err_thr0;        //def:4     ; range:[0, 1000]    ; precision:0.1
    float msc_err_thr1;        //def:1.8   ; range:[0, 1000]    ; precision:0.1
    float msc_tg_thr;          //def:3     ; range:[0, 1000]    ; precision:0.1
    float msc_tar_coef0;       //def:0.8   ; range:[0, 1000]    ; precision:0.1
    float msc_ind_coef0;       //def:0.6   ; range:[0, 1000]    ; precision:0.1
    float msc_tar_coef1;       //def:0.2   ; range:[0, 1000]    ; precision:0.1
    float msc_ind_coef1;	   //def:0.3   ; range:[0, 1000]    ; precision:0.1
    float msc_atg_diff_thr0;   //def:4     ; range:[0, 1000]    ; precision:0.1
    float msc_atg_diff_thr1;   //def:1     ; range:[0, 1000]    ; precision:0.1
    float msc_tg_spl_thr;      //def:4     ; range:[0, 1000]    ; precision:0.1
    float msc_sz_sm_thr;       //def:4     ; range:[0, 1000]    ; precision:0.1
    float msc_atg_sm_thr;      //def:3     ; range:[0, 1000]    ; precision:0.1

    // CMP Controller (Float)
    float cmp_updat_thr;       //def:1     ; range:[0, 1000]    ; precision:0.1
    float cmp_updat_fast_thr;  //def:2.5   ; range:[0, 1000]    ; precision:0.1
    float cmp_mul_factor;      //def:2     ; range:[0, 1000]    ; precision:0.1
    float cmp_div_factor;      //def:60    ; range:[0, 1000]    ; precision:0.1

    // Other (Float)
    float low_spl_thr;         //def:0     ; range:[0, 1000]    ; precision:1
    float splcnt_add_thr;      //def:0     ; range:[0, 1000]    ; precision:1

    // Large Noise Controller (Float)
    float noise_mse_thr;       //def:8     ; range:[0, 1000]    ; precision:1

    // LMS Controller (Float)
    float lms_err;             //def:100   ; range:[0, 1000]    ; precision:0.1
    float mse_thr1;            //def:5     ; range:[0, 1000]    ; precision:0.1
    float mse_thr2;            //def:8     ; range:[0, 1000]    ; precision:0.1
    float uscale;              //def:0.8   ; range:[0, 1000]    ; precision:0.01
    float uoffset;             //def:0.1   ; range:[0, 1000]    ; precision:0.01
    float u_pre_thr;           //def:0.5   ; range:[0, 1000]    ; precision:0.01
    float u_cur_thr;           //def:0.15  ; range:[0, 1000]    ; precision:0.01
    float u_first_thr;         //def:0.4   ; range:[0, 1000]    ; precision:0.01

    // Update Controller (Float)
    float norm_tg_thr;         //def:1.5   ; range:[0, 1000]    ; precision:0.1
    float norm_cmp_thr;        //def:2     ; range:[0, 1000]    ; precision:0.1

    // Fitting Algorithm Controller (Float)
    float ls_fix_range0[2];    //def:-10,10; range:[0, 1000]    ; precision:0.1
    float ls_fix_range1[2];    //def:0,16  ; range:[0, 1000]    ; precision:0.1
    float ls_fix_range2[2];    //def:12,40 ; range:[0, 1000]    ; precision:0.1

    // Other (Float)
    float undefine_param1[20]; //def:0     ; range:[0, 1000]    ; precision:0.1
};

//DYNAMIC 配置
struct __anc_ext_dynamic_cfg {
    // SDCC Controller
    u8  sdcc_det_en;           //def:0     ; range:[0, 1]
    u8  sdcc_par1;             //def:0     ; range:[0, 15]
    u8  sdcc_par2;             //def:4     ; range:[0, 15]
    u8  sdcc_flag_thr;         //def:0     ; range:[0, 3]

    // DRC Trigger
    u8  sdrc_det_en;           //def:0     ; range:[0, 1]
    u8  sdrc_flag_thr;         //def:1     ; range:[0, 2]
    u8  sdrc_cnt_thr;          //def:8     ; range:[0, 255]

    // SDRC Controller
    u8  sdrc_ls_rls_lidx;      //def:4     ; range:[0, 25]
    u8  sdrc_ls_rls_hidx;      //def:9     ; range:[0, 25]
    u8  undefine_param0[11];   //def:0     ; range:[0, 1000]

    // Common
    float ref_iir_coef;        //def:0.0078; range:[0, 1]       ; precision:0.0001
    float err_iir_coef;        //def:0.0078; range:[0, 1]       ; precision:0.0001

    // SDCC Controller (Float)
    float sdcc_ref_thr;        //def:-24   ; range:[-100, 100]  ; precision:0.1
    float sdcc_err_thr;        //def:-12   ; range:[-100, 100]  ; precision:0.1
    float sdcc_thr_cmp;        //def:-24   ; range:[-100, 100]  ; precision:0.1

    // DRC Trigger (Float)
    float sdrc_ref_thr;        //def:-24   ; range:[-100, 100]  ; precision:0.1
    float sdrc_err_thr;        //def:-12   ; range:[-100, 100]  ; precision:0.1

    // SDRC Controller (Float)
    float sdrc_ref_margin;     //def:30    ; range:[-100, 100]  ; precision:0.1
    float sdrc_err_margin;     //def:35    ; range:[-100, 100]  ; precision:0.1
    float sdrc_fb_att_thr;     //def:0     ; range:[-100, 100]  ; precision:0.1
    float sdrc_fb_rls_thr;     //def:-35   ; range:[-100, 100]  ; precision:0.1
    float sdrc_fb_att_step;    //def:2     ; range:[-100, 100]  ; precision:1
    float sdrc_fb_rls_step;    //def:2     ; range:[-100, 100]  ; precision:1
    float sdrc_fb_set_thr;     //def:3     ; range:[-100, 100]  ; precision:0.1
    float sdrc_ff_att_thr;     //def:0     ; range:[-100, 100]  ; precision:0.1
    float sdrc_ff_rls_thr;     //def:-24   ; range:[-100, 100]  ; precision:0.1
    float sdrc_ls_min_gain;    //def:0     ; range:[-100, 100]  ; precision:0.1
    float sdrc_ls_att_step;    //def:6     ; range:[-100, 100]  ; precision:0.1
    float sdrc_ls_rls_step;    //def:2     ; range:[-100, 100]  ; precision:0.1
    float sdrc_rls_ls_cmp;     //def:16    ; range:[-100, 100]  ; precision:0.1

    //Other
    float undefine_param1[10]; //def:0     ; range:[0, 1000]    ; precision:0.1
};


//参考SZ
struct __anc_ext_sz_data {
    float data[120];
};

//AEQ 阈值配置
struct __anc_ext_adaptive_eq_thr {
    /*
       音量分档: 当前音量vol
       1、vol > vol_thr2 			  	判定:小
       2、vol_thr2 >= vol > vol_thr1 	判定:中
       3、vol_thr1  >= vol				判定:大
    */
    u8 vol_thr1;		//def:5;	range:[0, 16]
    u8 vol_thr2;		//def:10;	range:[0, 16]
    u8 max_dB[3][3];	//[松紧度档位][音量档位] def [[22,12,3],[20,14,2],[15,10,1]]; range:[0, 30]
    /*
       松紧度分档：阈值thr
       1、thr > dot_thr2 			  	判定:紧
       2、dot_thr1 >= thr > dot_thr2 	判定:正常
       3、dot_thr1  >= thr				判定:松
     */
    float dot_thr1;		//def:-6.0f;	range:[-100, 100]
    float dot_thr2;		//def:0.0f;		range:[-100, 100]
};

//风噪检测配置
struct __anc_ext_wind_det_cfg {
    u8 wind_lvl_scale;
    u8 icsd_wind_num_thr1;
    u8 icsd_wind_num_thr2;
    u8 reserved;
    float wind_iir_alpha;
    float corr_thr;
    float msc_lp_thr;
    float msc_mp_thr;
    float cpt_1p_thr;
    float ref_pwr_thr;
    float param[6];
};

//风噪触发配置
struct __anc_ext_wind_trigger_cfg {
    u16 thr[5]; //def:[30,60,90,120,150] range [0, 255]
    u16 gain[5]; //def[10000, 8000, 6000, 3000, 0] range [0, 16384]
};

//软件啸叫检测配置
struct __anc_ext_soft_howl_det_cfg {
    float hd_scale;
    int hd_sat_thr;
    float hd_det_thr;
    float hd_diff_pwr_thr;
    int hd_maxind_thr1;
    int hd_maxind_thr2;
    float param[6];
};

//CMP/AEQ滤波器存线配置
struct __anc_ext_adaptive_mem_iir {
    u16 input_crc;
    u16 reserved;
    s16 mem_iir[0];	//31 * 30
};

//CMP SZ补偿
struct __anc_ext_sz_factor_data {
    float data[0];
};

#endif /*_ANC_EXT_TOOL_CFG_H_*/
