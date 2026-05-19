
/***********************************************
			ANC EXT 工具交互相关流程
1、耳道自适应

***********************************************/

#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".anc_ext.data.bss")
#pragma data_seg(".anc_ext.data")
#pragma const_seg(".anc_ext.text.const")
#pragma code_seg(".anc_ext.text")
#endif
#include "app_config.h"
#if TCFG_AUDIO_ANC_EXT_TOOL_ENABLE && TCFG_AUDIO_ANC_ENABLE
#include "anctool.h"
#include "audio_anc_includes.h"
#if TCFG_ANC_BOX_ENABLE
#include "chargestore/chargestore.h"
#endif

#if 1
#define anc_ext_log	printf
#else
#define anc_ext_log(...)
#endif/*log_en*/

//头戴式参数右声道复用左声道
#define ANC_EXT_CFG_R_REUSE_L_CHANNEL		0

//解析数据打印使能
#define ANC_EXT_CFG_LOG_EN		0
#define ANC_EXT_CFG_FLOAT_EN	0			//打印目标参数中的浮点数据

//耳道自适应产测数据最大长度
#define ANC_EXT_EAR_ADAPTIVE_DUT_MAX_LEN	60 * 2 * 2 * 4	//(pz[60]+sz[60]) * channel * sizeof(float)

/*
   ANC_EXT 耳道自适应工具 在线连接 权限说明
   1、支持工具自适应数据读取,会占用3-4K ram
   2、TWS区分左右参数
   3、进入后不支持产测，关机自动退出开发者模式
*/

//命令错误码
enum {
    ERR_NO = 0,
    ERR_NO_ANC_MODE = 1,    // 不支持非ANC模式打开
    ERR_EAR_FAIL = 2,   	// 耳机返回失败
};

//ANC_EXT命令
enum {
    CMD_GET_VERSION     		 = 0x01, //获取版本号
    CMD_TIME_DOMAIN_SHOW_CH_SET	 = 0x02, //时域频响SHOW CH设置
    CMD_BYPASS_TEST              = 0x03, //Bypass_test 模式切换
    CMD_EAR_ADAPTIVE_RUN         = 0x04, //自适应模式切换
    CMD_DEBUG_TOOL				 = 0x05, //Debug 开关
    CMD_ADAPTIVE_TYPE_GET		 = 0x06, //获取自适应类型
    CMD_ADAPTIVE_MODE_GET		 = 0x07, //获取自适应训练模式Debug 开关
    CMD_FUNCTION_ENABLEBIT_GET	 = 0x08, //获取小机已打开的功能(同时表示工具在线标志)
    CMD_EAR_ADAPTIVE_DUT_SAVE	 = 0x09, //耳道自适应产测数据保存VM
    CMD_EAR_ADAPTIVE_SIGN_TRIM	 = 0x0A, //耳道自适应符号校准
    CMD_EAR_ADAPTIVE_DUT_EN_SET	 = 0x0B, //耳道自适应产测使能
    CMD_EAR_ADAPTIVE_DUT_CLEAR	 = 0x0C, //耳道自适应产测数据清0
    CMD_RTANC_ADAPTIVE_OPEN		 = 0x0D, //打开RTANC
    CMD_RTANC_ADAPTIVE_CLOSE	 = 0x0E, //关闭RTANC
    CMD_RTANC_ADAPTIVE_SUSPEND	 = 0x0F, //挂起RTANC
    CMD_RTANC_ADAPTIVE_RESUME	 = 0x10, //恢复RTANC
    CMD_ADAPTIVE_EQ_RUN			 = 0x11, //运行AEQ（根据提示音生成SZ）
    CMD_ADAPTIVE_EQ_EFF_SEL		 = 0x12, //AEQ 选择参数(0 normal / 1 adaptive)

    CMD_WIND_DET_OPEN			 = 0x13, //打开风噪检测	data 0 normal; 1/2 ADT only wind_det
    CMD_WIND_DET_STOP			 = 0x14, //关闭风噪检测

    CMD_SOFT_HOWL_DET_OPEN		 = 0x15, //打开软件啸叫检测

    CMD_RTANC_ADAPTIVE_DUT_CLOSE = 0x16, //关闭RTANC_DUT
    CMD_RTANC_ADAPTIVE_DUT_OPEN = 0x17, //打开RTANC_DUT
    CMD_RTANC_ADAPTIVE_DUT_SUSPEND = 0x18, //暂停RTANC_DUT
    CMD_RTANC_ADAPTIVE_DUT_CMP_EN = 0x19, //使能RTANC 产测补偿

    CMD_SOFT_HOWL_DET_CLOSE		 = 0x1A, //关闭软件啸叫检测

    /* CMD_ADAPTIVE_DCC_OPEN		 = 0x1B, //打开自适应DCC */
    /* CMD_ADAPTIVE_DCC_STOP		 = 0x1C, //关闭自适应DCC */
    CMD_FUNCTION_SPP_DEBUG_SET	 = 0x1D, //设置算法模块SPP打印使能

    CMD_FUNCTION_ALGO_EN_SET   		 = 0X1E, //设置算法使能
    CMD_FUNCTION_ALGO_STATE_GET   	 = 0X1F, //获取小机算法状态
};

struct __anc_ext_tool_hdl {
    enum ANC_EXT_UART_SEL  uart_sel;				//工具协议选择
    u8 tool_online;									//工具在线连接标志
    u8 tool_ear_adaptive_en;						//工具启动耳道自适应标志
    u8 adaptive_cmp_en;								//工具自适应CMP使能
    u8 adaptive_eq_en;								//工具自适应EQ使能
    u16 adt_reset_timer;							//ADT 复位定时
    u32 DEBUG_TOOL_FUNCTION;						//工具调试模式:算法SPP打印使能
    struct list_head alloc_list;
    struct anc_ext_ear_adaptive_param ear_adaptive;	//耳道自适应工具参数

    u8 *report_state_buf;							//上报buffer
    int report_state_len;							//上报buffer长度
};


#define EAR_ADAPTIVE_STR_OFFSET(x)		offsetof(struct anc_ext_ear_adaptive_param, x)

//ANC_EXT 耳道自适应 参数ID/存储目标地址对照表
static const struct __anc_ext_subfile_id_table ear_adaptive_id_table[] = {

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    //BASE 界面参数 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_BASE
    { 0, ANC_EXT_EAR_ADAPTIVE_BASE_CFG_ID, 			EAR_ADAPTIVE_STR_OFFSET(base_cfg)},

    /*------------------------------头戴式 左声道 / TWS LR-----------------------------*/
    //FF->滤波器设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_IIR_FF
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_GENERAL_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_iir_general)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_HIGH_GAINS_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_iir_high_gains)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_GAINS_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_iir_medium_gains)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_LOW_GAINS_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_iir_low_gains)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_HIGH_IIR_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_iir_high)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_IIR_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_iir_medium)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_LOW_IIR_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_iir_low)},

    //FF->权重和性能设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT_PARAM_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_weight_param)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_HIGH_WEIGHT_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_weight_high)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_WEIGHT_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_weight_medium)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_LOW_WEIGHT_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_weight_low)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_HIGH_MSE_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_mse_high)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_MSE_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_mse_medium)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_LOW_MSE_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_mse_low)},

    //FF->TARGE挡位设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_TARGET
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_TARGET_PARAM_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_target_param)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_TARGET_SV_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_target_sv)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_TARGET_CMP_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_target_cmp)},

    //FF->耳道记忆设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_RECORDER
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PARAM_ID,		EAR_ADAPTIVE_STR_OFFSET(ff_ear_mem_param)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PZ_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_ear_mem_pz)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_SZ_ID, 		EAR_ADAPTIVE_STR_OFFSET(ff_ear_mem_sz)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PZ_COEFF_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_ear_mem_pz_coeff)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_SZ_COEFF_ID, 	EAR_ADAPTIVE_STR_OFFSET(ff_ear_mem_sz_coeff)},

    /*------------------------------------头戴式 右声道------------------------------------*/
    //FF->滤波器设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_IIR_FF
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_GENERAL_ID, 		EAR_ADAPTIVE_STR_OFFSET(rff_iir_general)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_GAINS_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_iir_high_gains)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_GAINS_ID, EAR_ADAPTIVE_STR_OFFSET(rff_iir_medium_gains)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_GAINS_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_iir_low_gains)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_IIR_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_iir_high)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_IIR_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_iir_medium)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_IIR_ID, 		EAR_ADAPTIVE_STR_OFFSET(rff_iir_low)},

    //FF->权重和性能设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_WEIGHT_PARAM_ID, EAR_ADAPTIVE_STR_OFFSET(rff_weight_param)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_WEIGHT_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_weight_high)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_WEIGHT_ID, EAR_ADAPTIVE_STR_OFFSET(rff_weight_medium)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_WEIGHT_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_weight_low)},

    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_MSE_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_mse_high)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_MSE_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_mse_medium)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_MSE_ID, 		EAR_ADAPTIVE_STR_OFFSET(rff_mse_low)},

    //FF->TARGE挡位设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_TARGET
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_PARAM_ID, EAR_ADAPTIVE_STR_OFFSET(rff_target_param)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_SV_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_target_sv)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_CMP_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_target_cmp)},

    //FF->耳道记忆设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_RECORDER
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PARAM_ID,		EAR_ADAPTIVE_STR_OFFSET(rff_ear_mem_param)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PZ_ID, 			EAR_ADAPTIVE_STR_OFFSET(rff_ear_mem_pz)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_SZ_ID, 			EAR_ADAPTIVE_STR_OFFSET(rff_ear_mem_sz)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PZ_COEFF_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_ear_mem_pz_coeff)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_SZ_COEFF_ID, 	EAR_ADAPTIVE_STR_OFFSET(rff_ear_mem_sz_coeff)},

    //FF->耳道自适应产测数据 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_DUT_PZ_CMP_ID,		EAR_ADAPTIVE_STR_OFFSET(ff_dut_pz_cmp)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_DUT_SZ_CMP_ID,		EAR_ADAPTIVE_STR_OFFSET(ff_dut_sz_cmp)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_DUT_PZ_CMP_ID,	EAR_ADAPTIVE_STR_OFFSET(rff_dut_pz_cmp)},
    { 0, ANC_EXT_EAR_ADAPTIVE_FF_R_DUT_SZ_CMP_ID,	EAR_ADAPTIVE_STR_OFFSET(rff_dut_sz_cmp)},
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    //RTANC adaptive 配置 文件ID FILE_ID_ANC_EXT_RTANC_ADAPTIVE_CFG
    { 0, ANC_EXT_RTANC_ADAPTIVE_CFG_ID,		EAR_ADAPTIVE_STR_OFFSET(rtanc_adaptive_cfg)},
    { 0, ANC_EXT_RTANC_R_ADAPTIVE_CFG_ID,	EAR_ADAPTIVE_STR_OFFSET(r_rtanc_adaptive_cfg)},

    //DYNAMIC 配置 文件ID FILE_ID_ANC_EXT_DYNAMIC_CFG
    { 0, ANC_EXT_DYNAMIC_CFG_ID,	EAR_ADAPTIVE_STR_OFFSET(dynamic_cfg)},
    { 0, ANC_EXT_DYNAMIC_R_CFG_ID,	EAR_ADAPTIVE_STR_OFFSET(r_dynamic_cfg)},
#endif

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    //CMP 配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_CMP_DATA
    { 0, ANC_EXT_ADAPTIVE_CMP_GAINS_ID,			EAR_ADAPTIVE_STR_OFFSET(cmp_gains)},
    { 0, ANC_EXT_ADAPTIVE_CMP_IIR_ID,			EAR_ADAPTIVE_STR_OFFSET(cmp_iir)},
    { 0, ANC_EXT_ADAPTIVE_CMP_WEIGHT_ID,		EAR_ADAPTIVE_STR_OFFSET(cmp_weight)},
    { 0, ANC_EXT_ADAPTIVE_CMP_MSE_ID,			EAR_ADAPTIVE_STR_OFFSET(cmp_mse)},
    { 0, ANC_EXT_ADAPTIVE_CMP_RECORDER_IIR_ID,	EAR_ADAPTIVE_STR_OFFSET(cmp_mem_iir)},
    { 0, ANC_EXT_ADAPTIVE_CMP_SZ_FACTOR_ID,	    EAR_ADAPTIVE_STR_OFFSET(cmp_sz_factor)},

    { 0, ANC_EXT_ADAPTIVE_CMP_R_GAINS_ID,		EAR_ADAPTIVE_STR_OFFSET(rcmp_gains)},
    { 0, ANC_EXT_ADAPTIVE_CMP_R_IIR_ID,			EAR_ADAPTIVE_STR_OFFSET(rcmp_iir)},
    { 0, ANC_EXT_ADAPTIVE_CMP_R_WEIGHT_ID,		EAR_ADAPTIVE_STR_OFFSET(rcmp_weight)},
    { 0, ANC_EXT_ADAPTIVE_CMP_R_MSE_ID,			EAR_ADAPTIVE_STR_OFFSET(rcmp_mse)},
    { 0, ANC_EXT_ADAPTIVE_CMP_R_RECORDER_IIR_ID, EAR_ADAPTIVE_STR_OFFSET(rcmp_mem_iir)},
    { 0, ANC_EXT_ADAPTIVE_CMP_R_SZ_FACTOR_ID,	EAR_ADAPTIVE_STR_OFFSET(rcmp_sz_factor)},
#endif

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //AEQ 配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_EQ_DATA
    { 0, ANC_EXT_ADAPTIVE_EQ_GAINS_ID,			EAR_ADAPTIVE_STR_OFFSET(aeq_gains)},
    { 0, ANC_EXT_ADAPTIVE_EQ_IIR_ID,			EAR_ADAPTIVE_STR_OFFSET(aeq_iir)},
    { 0, ANC_EXT_ADAPTIVE_EQ_WEIGHT_ID,			EAR_ADAPTIVE_STR_OFFSET(aeq_weight)},
    { 0, ANC_EXT_ADAPTIVE_EQ_MSE_ID,			EAR_ADAPTIVE_STR_OFFSET(aeq_mse)},
    { 0, ANC_EXT_ADAPTIVE_EQ_THR_ID,			EAR_ADAPTIVE_STR_OFFSET(aeq_thr)},
    { 0, ANC_EXT_ADAPTIVE_EQ_RECORDER_IIR_ID,	EAR_ADAPTIVE_STR_OFFSET(aeq_mem_iir)},

    { 0, ANC_EXT_ADAPTIVE_EQ_R_GAINS_ID,		EAR_ADAPTIVE_STR_OFFSET(raeq_gains)},
    { 0, ANC_EXT_ADAPTIVE_EQ_R_IIR_ID,			EAR_ADAPTIVE_STR_OFFSET(raeq_iir)},
    { 0, ANC_EXT_ADAPTIVE_EQ_R_WEIGHT_ID,		EAR_ADAPTIVE_STR_OFFSET(raeq_weight)},
    { 0, ANC_EXT_ADAPTIVE_EQ_R_MSE_ID,			EAR_ADAPTIVE_STR_OFFSET(raeq_mse)},
    { 0, ANC_EXT_ADAPTIVE_EQ_R_THR_ID,			EAR_ADAPTIVE_STR_OFFSET(raeq_thr)},
    { 0, ANC_EXT_ADAPTIVE_EQ_R_RECORDER_IIR_ID,	EAR_ADAPTIVE_STR_OFFSET(raeq_mem_iir)},
#endif

    //参考 SZ 数据 文件ID FILE_ID_ANC_EXT_REF_SZ_DATA
    { 0, ANC_EXT_REF_SZ_DATA_ID,	EAR_ADAPTIVE_STR_OFFSET(sz_ref)},
    { 0, ANC_EXT_REF_SZ_R_DATA_ID,	EAR_ADAPTIVE_STR_OFFSET(rsz_ref)},

#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    //风噪检测 配置 文件ID FILE_ID_ANC_EXT_WIND_DET_CFG
    { 0, ANC_EXT_WIND_DET_CFG_ID,		EAR_ADAPTIVE_STR_OFFSET(wind_det_cfg)},
    { 0, ANC_EXT_WIND_TRIGGER_CFG_ID, 	EAR_ADAPTIVE_STR_OFFSET(wind_trigger_cfg)},
#endif

    //软件-啸叫检测 配置 文件ID FILE_ID_ANC_EXT_SOFT_HOWL_DET_CFG
    { 0, ANC_EXT_SOFT_HOWL_DET_CFG_ID,	EAR_ADAPTIVE_STR_OFFSET(soft_howl_det_cfg)},

};


#if ANC_EXT_CFG_LOG_EN

static void anc_file_ear_adaptive_base_cfg_printf(int id, void *buf, int len);
static void anc_file_ear_adaptive_iir_general_printf(int id, void *buf, int len);
static void anc_file_ear_adaptive_iir_gains_printf(int id, void *buf, int len);
static void anc_file_ear_adaptive_iir_printf(int id, void *buf, int len);
static void anc_file_ear_adaptive_weight_param_printf(int id, void *buf, int len);
static void anc_file_cfg_data_float_printf(int id, void *buf, int len);
static void anc_file_ear_adaptive_target_param_printf(int id, void *buf, int len);
static void anc_file_ear_adaptive_mem_param_printf(int id, void *buf, int len);
static void anc_file_rtanc_adaptive_cfg_printf(int id, void *buf, int len);
static void anc_file_dynamic_cfg_printf(int id, void *buf, int len);
static void anc_file_adaptive_eq_thr_printf(int id, void *buf, int len);
static void anc_file_wind_det_cfg_printf(int id, void *buf, int len);
static void anc_file_wind_trigger_cfg_printf(int id, void *buf, int len);
static void anc_file_soft_howl_det_cfg_printf(int id, void *buf, int len);
static void anc_file_adaptive_mem_iir_printf(int id, void *buf, int len);

struct __anc_ext_printf {
    void (*p)(int id, void *buf, int len);
};

//各个参数的打印信息，需严格按照ear_adaptive_id_table的顺序排序
const struct __anc_ext_printf anc_ext_printf[] = {

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    { anc_file_ear_adaptive_base_cfg_printf },

    /*--------头戴式 左声道 / TWS LR---------*/
    { anc_file_ear_adaptive_iir_general_printf },
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_ear_adaptive_weight_param_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_ear_adaptive_target_param_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_ear_adaptive_mem_param_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },

    /*------------头戴式 右声道--------------*/
    { anc_file_ear_adaptive_iir_general_printf },
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_ear_adaptive_weight_param_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_ear_adaptive_target_param_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_ear_adaptive_mem_param_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },

    //FF->耳道自适应产测数据
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    //RTANC 配置
    { anc_file_rtanc_adaptive_cfg_printf },
    { anc_file_rtanc_adaptive_cfg_printf },

    { anc_file_dynamic_cfg_printf },
    { anc_file_dynamic_cfg_printf },
#endif

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    //RTCMP 配置
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_adaptive_mem_iir_printf },
    { anc_file_cfg_data_float_printf },

    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_adaptive_mem_iir_printf },
    { anc_file_cfg_data_float_printf },
#endif

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //RTAEQ
    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_adaptive_eq_thr_printf },
    { anc_file_adaptive_mem_iir_printf },

    { anc_file_ear_adaptive_iir_gains_printf },
    { anc_file_ear_adaptive_iir_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },
    { anc_file_adaptive_eq_thr_printf },
    { anc_file_adaptive_mem_iir_printf },
#endif

    //参考SZ
    { anc_file_cfg_data_float_printf },
    { anc_file_cfg_data_float_printf },

#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    //风噪检测配置
    { anc_file_wind_det_cfg_printf },
    { anc_file_wind_trigger_cfg_printf },
#endif

    //啸叫检测配置
    { anc_file_soft_howl_det_cfg_printf },

};

#endif

static int anc_cfg_analysis_ear_adaptive(u8 *file_data, int file_len, u8 alloc_flag);
static u8 *anc_ext_tool_data_alloc(u32 id, u8 *data, int len);
static struct anc_ext_alloc_bulk_t *anc_ext_tool_data_alloc_query(u32 id);
static int anc_ext_algorithm_en_set(u8 func, u8 enable);
static int anc_ext_algorithm_state_report(u8 active);
static void anc_ext_algorithm_state_init(void);

static struct __anc_ext_tool_hdl *tool_hdl = NULL;

//ANC_EXT 初始化
void anc_ext_tool_init(void)
{
    int ret;
    tool_hdl = anc_malloc("ANC_EXT", sizeof(struct __anc_ext_tool_hdl));
    INIT_LIST_HEAD(&tool_hdl->alloc_list);
    //解析anc_ext.bin
    if (anc_ext_rsfile_read()) {
        anc_ext_log("ERR! anc_ext.bin read error\n");
    }

    anc_ext_algorithm_state_init();

    //获取耳道自适应产测数据文件
    u8 *tmp_dut_buf = anc_malloc("ANC_EXT", ANC_EXT_EAR_ADAPTIVE_DUT_MAX_LEN);
    ret = syscfg_read(CFG_ANC_ADAPTIVE_DUT_ID, tmp_dut_buf, ANC_EXT_EAR_ADAPTIVE_DUT_MAX_LEN);
    if (ret > 0) {
        //解析申请buffer使用
        anc_ext_log("ANC EAR ADAPTIVE DUT DATA SUSS!, ret %d\n", ret);
        anc_ext_subfile_analysis_each(FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP, tmp_dut_buf, ret, 1);
    } else {
        anc_ext_log("ANC EAR ADAPTIVE DUT DATA EMPTY!\n");
        //填充0数据,避免产测读数异常
        struct anc_ext_subfile_head dut_tmp;
        dut_tmp.file_id = FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP;
        dut_tmp.file_len = sizeof(struct anc_ext_subfile_head);
        dut_tmp.version = 0;
        dut_tmp.id_cnt = 0;
        anc_ext_subfile_analysis_each(FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP, (u8 *)&dut_tmp, dut_tmp.file_len, 1);
    }
    /* ret = syscfg_read(CFG_ANC_ADAPTIVE_DUT_EN_ID, &tool_hdl->ear_adaptive.dut_cmp_en, 1); */

    /* if (ret <= 0) { */
    /* anc_ext_log("ANC EAR ADAPTIVE DUT EN EMPTY!\n"); */
    /* } */
    anc_free(tmp_dut_buf);
}

/*
   ANC_EXT 数据发送
	param : cmd2pc 命令对象
			buf 发送buf
			len 发送长度
*/
static void anc_ext_tool_send_buf(u8 cmd2pc, u8 *buf, int len)
{
    if (tool_hdl->uart_sel == ANC_EXT_UART_SEL_SPP) {
        if (len > 512 - 4) {
            printf("ERROR anc_ext_tool send_buf len = %d overflow\n", len);
            return;
        }
        u8 *cmd = anc_malloc("ANC_EXT", len + 4);
        //透传命令标识
        cmd[0] = 0xFD;
        cmd[1] = 0x90;
        //ANC_EXT命令标识
        cmd[2] = 0xB0;
        cmd[3] = cmd2pc;
        memcpy(cmd + 4, buf, len);
        anctool_api_write(cmd, len + 4);
        anc_free(cmd);
    }
#if TCFG_ANC_BOX_ENABLE
    else {
        if (len > 32 - 3) {
            printf("ERROR anc_ext_tool send_buf len = %d overflow\n", len);
            return;
        }
        u8 *cmd = anc_malloc("ANC_EXT", len + 3);
        //透传命令标识
        cmd[0] = 0x90;
        //ANC_EXT命令标识
        cmd[1] = 0xB0;
        cmd[2] = cmd2pc;
        memcpy(cmd + 3, buf, len);
        chargestore_api_write(cmd, len + 3);
        anc_free(cmd);
    }
#endif
}

/*
   ANC_EXT 命令执行结果回复
	param : cmd2pc 命令对象
			ret 执行结果(TRUE/FALSE)
			err_num 错误代码
*/
static void anc_ext_tool_cmd_ack(u8 cmd2pc, u8 ret, u8 err_num)
{
    if (tool_hdl->uart_sel == ANC_EXT_UART_SEL_SPP) {
        u8 cmd[5];
        //透传命令标识
        cmd[0] = 0xFD;
        cmd[1] = 0x90;
        //ANC_EXT命令标识
        cmd[2] = 0xB0;
        if (ret == TRUE) {
            cmd[3] = cmd2pc;
            anctool_api_write(cmd, 4);
        } else {
            cmd[3] = 0xFE;
            cmd[4] = err_num;
            anctool_api_write(cmd, 5);
        }
    }
#if TCFG_ANC_BOX_ENABLE
    else {
        u8 cmd[3];
        //透传命令标识
        cmd[0] = 0x90;
        //ANC_EXT命令标识
        cmd[1] = 0xB0;
        if (ret == TRUE) {
            cmd[2] = cmd2pc;
        } else {
            cmd[2] = 0xFE;
        }
        chargestore_api_write(cmd, 3);
    }
#endif
}

//ANC_EXT CMD事件处理
void anc_ext_tool_cmd_deal(u8 *data, u16 len, enum ANC_EXT_UART_SEL uart_sel)
{
    int ret = TRUE;
    int err = ERR_NO;
    u8 cmd = data[0];
#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    int func_ret;
#endif
    tool_hdl->uart_sel = uart_sel;
    switch (cmd) {
    //---------------公共指令start -----------------
    case CMD_GET_VERSION:
        anc_ext_log("CMD_GET_VERSION\n");
        break;
    case CMD_DEBUG_TOOL:
        anc_ext_log("CMD_DEBUG_TOOL, mode %d\n", data[1]);
        audio_anc_debug_spp_log_en(data[1]);
        break;
    case CMD_ADAPTIVE_TYPE_GET:
        anc_ext_log("CMD_ADAPTIVE_TYPE_GET\n");
#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
        u8 type = (ICSD_EP_TYPE_V2 == ICSD_HEADSET) ? ANC_ADAPTIVE_TYPE_HEADSET : ANC_ADAPTIVE_TYPE_TWS;
#else
        u8 type = ANC_ADAPTIVE_TYPE_TWS;
#endif
        anc_ext_tool_send_buf(cmd, &type, 1);
        break;
    case CMD_ADAPTIVE_MODE_GET:
        anc_ext_log("CMD_ADAPTIVE_MODE_GET\n");
        u8 mode = ANC_TRAIN_MODE_TONE_BYPASS_PNC;
        anc_ext_tool_send_buf(cmd, &mode, 1);
        break;
    case CMD_FUNCTION_ENABLEBIT_GET:
        anc_ext_log("CMD_FUNCTION_ENABLEBIT_GET\n");
        tool_hdl->tool_online = 1;
        u16 en = 0;
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        en |= ANC_EXT_FUNC_EN_RTANC;
        en |= ANC_EXT_FUNC_EN_DYNAMIC;
#endif
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
        en |= ANC_EXT_FUNC_EN_ADAPTIVE_CMP;
#endif
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
        en |= ANC_EXT_FUNC_EN_ADAPTIVE_EQ;
#endif
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
        en |= ANC_EXT_FUNC_EN_WIND_DET;
#endif
#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
        en |= ANC_EXT_FUNC_EN_ADAPTIVE;
        audio_anc_param_map_init();
#endif
#if TCFG_AUDIO_ANC_MULT_ORDER_ENABLE
        if (!audio_anc_develop_get()) { //用于ANC滤波映射，初始化空间，若开发者模式，则在开发者那边先初始化完毕
            anc_mult_scene_set(audio_anc_mult_scene_get());
        }
#endif
        anc_ext_tool_send_buf(cmd, (u8 *)&en, 2);
        break;
        //---------------公共指令end -----------------

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    case CMD_TIME_DOMAIN_SHOW_CH_SET:
        anc_ext_log("CMD_TIME_DOMAIN_SHOW_CH_SET, %d\n", data[1]);
        tool_hdl->ear_adaptive.time_domain_show_en = data[1];
        if (data[1]) {
            tool_hdl->ear_adaptive.time_domain_len = sizeof(__icsd_time_data);
            tool_hdl->ear_adaptive.time_domain_buf = anc_malloc("ANC_EXT", tool_hdl->ear_adaptive.time_domain_len);
            if (!tool_hdl->ear_adaptive.time_domain_buf) {
                ret = FALSE;
            }
        } else {
            anc_free(tool_hdl->ear_adaptive.time_domain_buf);
            tool_hdl->ear_adaptive.time_domain_buf = NULL;
        }
        break;

    case CMD_BYPASS_TEST:
        anc_ext_log("CMD_BYPASS_TEST, mode %d\n", data[1]);
        if (anc_mode_get() == ANC_ON && (anc_ext_ear_adaptive_param_check() == 0)) {
            if (data[1]) {
                //初始化参数依赖自适应流程
                struct anc_ext_ear_adaptive_param *cfg = anc_ext_ear_adaptive_cfg_get();
                anc_ear_adaptive_train_set_bypass(ANC_ADAPTIVE_FF_ORDER, \
                                                  cfg->base_cfg->bypass_vol, cfg->base_cfg->calr_sign[2]);
            } else {
                anc_ear_adaptive_train_set_bypass_off();
            }
        } else {
            ret = FALSE;
        }
        break;

    case CMD_EAR_ADAPTIVE_RUN:
        anc_ext_log("CMD_EAR_ADAPTIVE_RUN, mode %d\n", data[1]);
        tool_hdl->ear_adaptive.train_mode = data[1];
        if (audio_anc_mode_ear_adaptive(1)) {
            ret = FALSE;
            err = ERR_EAR_FAIL;
        } else {
            tool_hdl->tool_ear_adaptive_en = 1;
            return;	//等耳道自适应训练结束才回复
        }
        break;
    case CMD_EAR_ADAPTIVE_DUT_SAVE:
        anc_ext_log("CMD_EAR_ADAPTIVE_DUT_SAVE\n");
        struct anc_ext_alloc_bulk_t *bulk = anc_ext_tool_data_alloc_query(FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP);
        if (bulk) {
            func_ret = syscfg_write(CFG_ANC_ADAPTIVE_DUT_ID, bulk->alloc_addr, bulk->alloc_len);
            if (func_ret <= 0) {
                anc_ext_log("ANC_EAR_ADAPTIVE DUT write to vm err, ret %d\n", func_ret);
                ret = FALSE;
            }
        }
        /* func_ret = syscfg_write(CFG_ANC_ADAPTIVE_DUT_EN_ID, &tool_hdl->ear_adaptive.dut_cmp_en, 1); */
        /* if (func_ret <= 0) { */
        /* anc_ext_log("ANC_EAR_ADAPTIVE DUT_EN write to vm err, ret %d\n", func_ret); */
        /* ret = FALSE; */
        /* break; */
        /* } */
        break;
    case CMD_EAR_ADAPTIVE_DUT_EN_SET:
        anc_ext_log("CMD_EAR_ADAPTIVE_DUT_EN_SET %d \n", data[1]);
        tool_hdl->ear_adaptive.dut_cmp_en = data[1];
        break;
    case CMD_EAR_ADAPTIVE_DUT_CLEAR:
        anc_ext_log("CMD_EAR_ADAPTIVE_DUT_CLEAR\n");
        if (anc_ear_adaptive_busy_get()) {
            ret = FALSE;
            err = ERR_EAR_FAIL;
            break;
        }
        //填充0数据,避免产测读数异常
        struct anc_ext_subfile_head dut_tmp;
        dut_tmp.file_id = FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP;
        dut_tmp.file_len = sizeof(struct anc_ext_subfile_head);
        dut_tmp.version = 0;
        dut_tmp.id_cnt = 0;
        anc_ext_subfile_analysis_each(FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP, (u8 *)&dut_tmp, dut_tmp.file_len, 1);
        tool_hdl->ear_adaptive.ff_dut_pz_cmp = NULL;
        tool_hdl->ear_adaptive.ff_dut_sz_cmp = NULL;
        tool_hdl->ear_adaptive.rff_dut_pz_cmp = NULL;
        tool_hdl->ear_adaptive.rff_dut_sz_cmp = NULL;
        func_ret = syscfg_write(CFG_ANC_ADAPTIVE_DUT_ID, (u8 *)&dut_tmp, dut_tmp.file_len);
        if (func_ret <= 0) {
            anc_ext_log("ANC_EAR_ADAPTIVE DUT write to vm err, ret %d\n", func_ret);
            ret = FALSE;
        }
        break;
    case CMD_EAR_ADAPTIVE_SIGN_TRIM:
        anc_ext_log("CMD_EAR_ADAPTIVE_SIGN_TRIM\n");
        tool_hdl->ear_adaptive.train_mode = EAR_ADAPTIVE_MODE_SIGN_TRIM;
        if (!audio_anc_mode_ear_adaptive(1)) {
            //符号校准完毕后回复命令
            return;
        }
        ret = FALSE;
        err = ERR_EAR_FAIL;
        break;
#endif
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    case CMD_RTANC_ADAPTIVE_OPEN:
        anc_ext_log("CMD_RTANC_ADAPTIVE_OPEN\n");
        audio_real_time_adaptive_app_open_demo();
        break;
    case CMD_RTANC_ADAPTIVE_DUT_OPEN:
        anc_ext_log("CMD_RTANC_ADAPTIVE_DUT_OPEN\n");
        //产测：使用耳道自适应的流程测试
        anc_ext_ear_adaptive_dut_mode_set(ANC_ADPATIVE_DUT_MODE_PRODUCTION);
        if (audio_anc_mode_ear_adaptive(1)) {
            //耳道自适应训练完毕之后回复命令
            return;
        }
        ret = FALSE;
        err = ERR_EAR_FAIL;
        break;
    case CMD_RTANC_ADAPTIVE_CLOSE:
        anc_ext_log("CMD_RTANC_ADAPTIVE_CLOSE\n");
        audio_real_time_adaptive_app_close_demo();
        break;
    case CMD_RTANC_ADAPTIVE_DUT_CLOSE:
        anc_ext_log("CMD_RTANC_ADAPTIVE_DUT_CLOSE\n");
        // tool_hdl->ear_adaptive.dut_mode = 0;
        anc_ext_ear_adaptive_dut_mode_set(ANC_ADPATIVE_DUT_MODE_NORAML);
        audio_real_time_adaptive_app_close_demo();
        break;
    case CMD_RTANC_ADAPTIVE_SUSPEND:
    case CMD_RTANC_ADAPTIVE_DUT_SUSPEND:
        anc_ext_log("CMD_RTANC_ADAPTIVE_SUSPEND\n");
        audio_anc_real_time_adaptive_suspend("ANC_EXT_TOOL");
        break;
    case CMD_RTANC_ADAPTIVE_RESUME:
        anc_ext_log("CMD_RTANC_ADAPTIVE_RESUME\n");
        audio_anc_real_time_adaptive_resume("ANC_EXT_TOOL");
        break;
#endif
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    case CMD_ADAPTIVE_EQ_RUN:
        anc_ext_log("CMD_ADAPTIVE_EQ_RUN\n");
        if (audio_adaptive_eq_app_open()) {
            ret = FALSE;
        }
        break;
    case CMD_ADAPTIVE_EQ_EFF_SEL:
        anc_ext_log("CMD_ADAPTIVE_EQ_EFF_SEL %d\n", data[1]);
        if (audio_adaptive_eq_eff_set(data[1])) {
            ret = FALSE;
        }
        break;
#endif
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    case CMD_WIND_DET_OPEN:
        anc_ext_log("CMD_WIND_DET_OPEN data %d\n", data[1]);
        audio_icsd_wind_detect_open();
        break;
    case CMD_WIND_DET_STOP:
        anc_ext_log("CMD_WIND_DET_CLOSE\n");
        audio_icsd_wind_detect_close();
        break;
#endif
#if TCFG_AUDIO_ADAPTIVE_DCC_ENABLE
    case CMD_ADAPTIVE_DCC_OPEN:
        anc_ext_log("CMD_ADAPTIVE_DCC_OPEN data %d\n", data[1]);
        /* audio_icsd_wind_detect_en(1); */
        break;
    case CMD_ADAPTIVE_DCC_STOP:
        anc_ext_log("CMD_ADAPTIVE_DCC_STOP\n");
        /* audio_icsd_wind_detect_en(0); */
        break;
#endif
    case CMD_FUNCTION_SPP_DEBUG_SET:
        memcpy(&tool_hdl->DEBUG_TOOL_FUNCTION, data + 1, 4);
        anc_ext_log("CMD_FUNCTION_SPP_DEBUG_SET, 0x%x\n", tool_hdl->DEBUG_TOOL_FUNCTION);
        //修改SPP打印之后需要复位算法
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
        if (icsd_adt_is_running()) {
            audio_icsd_adt_reset(0);
        }
#endif
        break;
    case CMD_FUNCTION_ALGO_EN_SET:
        anc_ext_log("CMD_FUNCTION_ALGO_EN_SET 0x%x, en 0x%x\n", data[1], data[2]);
        anc_ext_algorithm_en_set(data[1], data[2]);
        break;
    case CMD_FUNCTION_ALGO_STATE_GET:
        anc_ext_log("CMD_FUNCTION_ALGO_STATE_GET\n");
        //内部回复命令
        anc_ext_algorithm_state_report(0);
        return;
    default:
        ret = FALSE;
        break;
    }
    anc_ext_tool_cmd_ack(cmd, ret, err);
}

//耳道自适应执行结果回调函数
void anc_ext_tool_ear_adaptive_end_cb(u8 result)
{
    if (tool_hdl->tool_ear_adaptive_en) {
        anc_ext_tool_send_buf(CMD_EAR_ADAPTIVE_RUN, &result, 1);
        tool_hdl->tool_ear_adaptive_en = 0;
    }
    tool_hdl->ear_adaptive.train_mode = EAR_ADAPTIVE_MODE_NORAML;
}

//耳道自适应-符号校准 执行结果回调函数
void anc_ext_tool_ear_adaptive_sign_trim_end_cb(u8 *buf, int len)
{
    struct anc_ext_ear_adaptive_param *p = &tool_hdl->ear_adaptive;
    //符号校准数据上传工具
    anc_ext_tool_send_buf(CMD_EAR_ADAPTIVE_SIGN_TRIM, buf, len);
    //覆盖当前自适应参数符号对应指针
    /* p->base_sign = (struct __anc_ext_ear_adaptive_base_sign *)anc_ext_tool_data_alloc(\ */
    /* ANC_EXT_EAR_ADAPTIVE_BASE_SIGN_ID, buf, sizeof(struct __anc_ext_ear_adaptive_base_sign)); */
    /* p->base_sign->sign_trim_en = 1; */
    /* memcpy(&p->base_sign->sz_calr_sign, buf, len); */
    p->train_mode = EAR_ADAPTIVE_MODE_NORAML;
}

//参数临时缓存申请
static u8 *anc_ext_tool_data_alloc(u32 id, u8 *data, int len)
{
    struct anc_ext_alloc_bulk_t *bulk;
    //更新ID对应地址
    list_for_each_entry(bulk, &tool_hdl->alloc_list, entry) {
        if (id == bulk->alloc_id) {
            anc_free(bulk->alloc_addr);
            goto __exit;
        }
    }
    //新增ID 链表
    bulk = anc_malloc("ANC_EXT", sizeof(struct anc_ext_alloc_bulk_t));
    bulk->alloc_id = id;
    list_add_tail(&bulk->entry, &tool_hdl->alloc_list);

__exit:
    bulk->alloc_addr = anc_malloc("ANC_EXT", len);
    bulk->alloc_len = len;
    if (!bulk->alloc_addr) {
        return NULL;
    }
    memcpy(bulk->alloc_addr, data, len);
    return bulk->alloc_addr;
}


//参数临时缓存查询
static struct anc_ext_alloc_bulk_t *anc_ext_tool_data_alloc_query(u32 id)
{
    struct anc_ext_alloc_bulk_t *bulk;
    list_for_each_entry(bulk, &tool_hdl->alloc_list, entry) {
        if (id == bulk->alloc_id) {
            return bulk;
        }
    }
    return NULL;
}

//参数临时缓存释放
static void anc_ext_tool_data_alloc_free(void)
{
    struct anc_ext_alloc_bulk_t *bulk;
    struct anc_ext_alloc_bulk_t *temp;
    //更新文件ID对应地址
    list_for_each_entry_safe(bulk, temp, &tool_hdl->alloc_list, entry) {
        anc_free(bulk->alloc_addr);
        list_del(&bulk->entry);
        anc_free(bulk);
    }
}

//ANC_EXT SUBFILE文件解析遍历
int anc_ext_subfile_analysis_each(u32 file_id, u8 *data, int len, u8 alloc_flag)
{
    int ret = 0;

    //anc_ext_log("Analysis - FILE ID 0x%x\n", file_id);
    switch (file_id) {
    //anc_ext.bin 文件ID解析
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_BASE:
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_IIR:
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT:
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_TARGET:
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_RECORDER:
    case FILE_ID_ANC_EXT_RTANC_ADAPTIVE_CFG:
    case FILE_ID_ANC_EXT_ADAPTIVE_CMP_DATA:
    case FILE_ID_ANC_EXT_ADAPTIVE_EQ_DATA:
    case FILE_ID_ANC_EXT_REF_SZ_DATA:
    case FILE_ID_ANC_EXT_WIND_DET_CFG:
    case FILE_ID_ANC_EXT_SOFT_HOWL_DET_CFG:
    case FILE_ID_ANC_EXT_DYNAMIC_CFG:
        ret = anc_cfg_analysis_ear_adaptive(data, len, alloc_flag);
        /* put_buf(data, len); */
        break;
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP:
        if (alloc_flag) {
            u8 *dut_buf = anc_ext_tool_data_alloc(FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP, data, len);
            ret = anc_cfg_analysis_ear_adaptive(dut_buf, len, 0);
        }
        /* put_buf(data, len); */
        break;
    case FILE_ID_ANC_EXT_BIN:
        break;
    default:
        break;
    }

#if ANC_EXT_CFG_R_REUSE_L_CHANNEL && (TCFG_AUDIO_ANC_CH == (ANC_L_CH | ANC_R_CH))

    //右声道参数 = 左声道
    struct anc_ext_ear_adaptive_param *param = &tool_hdl->ear_adaptive;

    // ======================================
    // 1. 耳道自适应模块（ANC_EAR_ADAPTIVE）
    // ======================================
#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    // 右声道绑定到左声道
    param->rff_iir_general = param->ff_iir_general;
    param->rff_iir_high_gains = param->ff_iir_high_gains;
    param->rff_iir_medium_gains = param->ff_iir_medium_gains;
    param->rff_iir_low_gains = param->ff_iir_low_gains;
    param->rff_iir_high = param->ff_iir_high;
    param->rff_iir_medium = param->ff_iir_medium;
    param->rff_iir_low = param->ff_iir_low;
    param->rff_weight_param = param->ff_weight_param;
    param->rff_weight_high = param->ff_weight_high;
    param->rff_weight_medium = param->ff_weight_medium;
    param->rff_weight_low = param->ff_weight_low;
    param->rff_mse_high = param->ff_mse_high;
    param->rff_mse_medium = param->ff_mse_medium;
    param->rff_mse_low = param->ff_mse_low;
    param->rff_target_param = param->ff_target_param;
    param->rff_target_sv = param->ff_target_sv;
    param->rff_target_cmp = param->ff_target_cmp;
    param->rff_ear_mem_param = param->ff_ear_mem_param;
    param->rff_ear_mem_pz = param->ff_ear_mem_pz;
    param->rff_ear_mem_sz = param->ff_ear_mem_sz;
    param->rff_ear_mem_pz_coeff = param->ff_ear_mem_pz_coeff;
    param->rff_ear_mem_sz_coeff = param->ff_ear_mem_sz_coeff;
    param->rff_dut_pz_cmp = param->ff_dut_pz_cmp;
    param->rff_dut_sz_cmp = param->ff_dut_sz_cmp;
#endif


    // ======================================
    // 2. 实时自适应模块（RTANC）
    // ======================================
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    // 右声道绑定到左声道（右声道成员：r_rtanc_adaptive_cfg、r_dynamic_cfg）
    param->r_rtanc_adaptive_cfg = param->rtanc_adaptive_cfg;
    param->r_dynamic_cfg = param->dynamic_cfg;
#endif


    // ======================================
    // 3. 自适应比较模块（RTCMP）
    // ======================================
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    // 右声道绑定到左声道（右声道成员：rcmp_xxx）
    param->rcmp_gains = param->cmp_gains;
    param->rcmp_iir = param->cmp_iir;
    param->rcmp_weight = param->cmp_weight;
    param->rcmp_mse = param->cmp_mse;
    param->rcmp_mem_iir = param->cmp_mem_iir;
#endif


    // ======================================
    // 4. 自适应EQ模块（RTAEQ）
    // ======================================
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    // 右声道绑定到左声道（右声道成员：raeq_xxx）
    param->raeq_gains = param->aeq_gains;
    param->raeq_iir = param->aeq_iir;
    param->raeq_weight = param->aeq_weight;
    param->raeq_mse = param->aeq_mse;
    param->raeq_thr = param->aeq_thr;
    param->raeq_mem_iir = param->aeq_mem_iir;
#endif

    // 右声道绑定到左声道
    param->rsz_ref = param->sz_ref;
#endif

    return ret;
}

void anc_ext_write_adt_reset_timeout(void *priv)
{
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_async_reset(0);
    tool_hdl->adt_reset_timer = 0;
#endif
}

//SUBFILE 工具写文件文件结束
int anc_ext_tool_write_end(u32 file_id, u8 *data, int len, u8 alloc_flag)
{
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    u8 adt_suspend = icsd_adt_is_running();
    if (adt_suspend) {
        audio_icsd_adt_algom_suspend();
    }
#endif
    int ret = anc_ext_subfile_analysis_each(file_id, data, len, alloc_flag);
    //工具下发参数针对特殊ID做处理
    switch (file_id) {
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP:
        break;
    }
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    if (adt_suspend) {
        if (tool_hdl->adt_reset_timer) {
            sys_timer_modify(tool_hdl->adt_reset_timer, 2000);
        } else {
            tool_hdl->adt_reset_timer = sys_timeout_add(NULL, anc_ext_write_adt_reset_timeout, 2000);
        }
    }
#endif
    return ret;
}


//SUBFILE 工具开始读文件
int anc_ext_tool_read_file_start(u32 file_id, u8 **data, u32 *len)
{
    g_printf("anc_ext_tool_read_file_start %x, \n", file_id);
    switch (file_id) {
    case FILE_ID_ANC_EXT_BIN:
        anc_ext_log("read FILE_ID_ANC_EXT_BIN\n");
        *data = anc_ext_rsfile_get(len);
        if (*data == NULL) {
            return 1;
        }
        break;
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_TIME_DOMAIN:
        anc_ext_log("read FILE_ID_ANC_EXT_EAR_ADAPTIVE_TIME_DOMAIN\n");
        *len = tool_hdl->ear_adaptive.time_domain_len;
        *data = tool_hdl->ear_adaptive.time_domain_buf;
        if (*data == NULL) {
            return 1;
        }
        break;
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP:
        anc_ext_log("read FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP\n");
        struct anc_ext_alloc_bulk_t *bulk = anc_ext_tool_data_alloc_query(FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP);
        if (bulk) {
            *len = bulk->alloc_len;
            *data = bulk->alloc_addr;
        } else {
            return 1;
        }
        break;
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    case FILE_ID_ANC_EXT_RTANC_DEBUG_DATA:
        anc_ext_log("read FILE_ID_ANC_EXT_RTANC_DEBUG_DATA\n");
        return audio_anc_real_time_adaptive_tool_data_get(data, len);
#endif
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    case FILE_ID_ANC_EXT_ADAPTIVE_EQ_DEBUG_DATA:
        anc_ext_log("read FILE_ID_ANC_EXT_ADAPTIVE_EQ_DEBUG_DATA\n");
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE	//打开RTANC，则需挂起RTANC才能获取
        if (audio_anc_real_time_adaptive_suspend_get() || (!audio_anc_real_time_adaptive_state_get())) {
            return audio_adaptive_eq_tool_data_get(data, len);
        } else {
            return 1;
        }
#else
        return audio_adaptive_eq_tool_data_get(data, len);
#endif
    case FILE_ID_ANC_EXT_REF_SZ_DATA:
        anc_ext_log("read FILE_ID_ANC_EXT_REF_SZ_DATA\n");
        return audio_adaptive_eq_tool_sz_data_get(data, len);
#endif
    default:
        return 1;
    }
    return 0;
}

//SUBFILE 工具读文件结束
int anc_ext_tool_read_file_end(u32 file_id)
{
    switch (file_id) {
    case FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_IIR:
        break;
    default:
        break;
    }
    return 0;
}

/*-----------------------文件解析------------------------------*/

static void anc_file_ear_adaptive_base_cfg_printf(int id, void *buf, int len)
{
    struct __anc_ext_ear_adaptive_base_cfg *cfg = (struct __anc_ext_ear_adaptive_base_cfg *)buf;
    anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_BASE_CFG_ID 0x%x---\n", id);

    anc_ext_log("sign_trim_en      :%d \n", cfg->sign_trim_en);
    anc_ext_log("sz_calr_sign      :%d \n", cfg->calr_sign[0]);
    anc_ext_log("pz_calr_sign      :%d \n", cfg->calr_sign[1]);
    anc_ext_log("bypass_calr_sign  :%d \n", cfg->calr_sign[2]);
    anc_ext_log("perf_calr_sign    :%d \n", cfg->calr_sign[3]);
    anc_ext_log("vld1              :%d \n", cfg->vld1);
    anc_ext_log("vld2              :%d \n", cfg->vld2);
    anc_ext_log("adaptive_mode     :%d \n", cfg->adaptive_mode);
    anc_ext_log("adaptive_times    :%d \n", cfg->adaptive_times);
    anc_ext_log("tonel_delay       :%d \n", cfg->tonel_delay);
    anc_ext_log("toner_delay       :%d \n", cfg->toner_delay);
    anc_ext_log("pzl_delay 	       :%d \n", cfg->pzl_delay);
    anc_ext_log("pzr_delay         :%d \n", cfg->pzr_delay);
    anc_ext_log("bypass_vol        :%d/100 \n", (int)(cfg->bypass_vol * 100));
    anc_ext_log("sz_pri_thr        :%d/100 \n", (int)(cfg->sz_pri_thr * 100));
}

static void anc_file_ear_adaptive_iir_general_printf(int id, void *buf, int len)
{
    struct __anc_ext_ear_adaptive_iir_general *cfg = (struct __anc_ext_ear_adaptive_iir_general *)buf;
    anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_GENERAL_ID 0x%x---\n", id);
    anc_ext_log("biquad_level_l    :%d \n", cfg->biquad_level_l);
    anc_ext_log("biquad_level_h    :%d \n", cfg->biquad_level_h);
    anc_ext_log("total_gain_freq_l :%d/100 \n", (int)(cfg->total_gain_freq_l * 100));
    anc_ext_log("total_gain_freq_h :%d/100 \n", (int)(cfg->total_gain_freq_h * 100));
    anc_ext_log("total_gain_limit  :%d/100 \n", (int)(cfg->total_gain_limit * 100));
}

static void anc_file_ear_adaptive_iir_gains_printf(int id, void *buf, int len)
{
    struct __anc_ext_ear_adaptive_iir_gains *cfg = (struct __anc_ext_ear_adaptive_iir_gains *)buf;
    anc_ext_log("---ANC_EXT_ADAPTIVE_GAINS_ID 0x%x---\n", id);
    anc_ext_log("iir_target_gain_limit :%d/100 \n", (int)(cfg->iir_target_gain_limit * 100));
    anc_ext_log("def_total_gain        :%d/100 \n", (int)(cfg->def_total_gain * 100));
    anc_ext_log("upper_limit_gain      :%d/100 \n", (int)(cfg->upper_limit_gain * 100));
    anc_ext_log("lower_limit_gain      :%d/100 \n", (int)(cfg->lower_limit_gain * 100));
}

static void anc_file_ear_adaptive_iir_printf(int id, void *buf, int len)
{

    struct __anc_ext_ear_adaptive_iir *iir = (struct __anc_ext_ear_adaptive_iir *)buf;
    anc_ext_log("---ANC_EXT_ADAPTIVE_IIR_ID 0x%x---\n", id);
    int cnt = len / sizeof(struct __anc_ext_ear_adaptive_iir);
    for (int i = 0; i < cnt; i++) {
        anc_ext_log("iir type_%d %d, fixed_en %d\n", i, iir[i].type, iir[i].fixed_en);
        anc_ext_log("iir_def      f %d/100, g %d/100, q %d/100\n", (int)(iir[i].def.fre * 100.0), \
                    (int)(iir[i].def.gain * 100.0), (int)(iir[i].def.q * 100.0));
        anc_ext_log("up_limit     f %d/100, g %d/100, q %d/100\n", (int)(iir[i].upper_limit.fre * 100.0), \
                    (int)(iir[i].upper_limit.gain * 100.0), (int)(iir[i].upper_limit.q * 100.0));
        anc_ext_log("lower_limit  f %d/100, g %d/100, q %d/100\n", (int)(iir[i].lower_limit.fre * 100.0), \
                    (int)(iir[i].lower_limit.gain * 100.0), (int)(iir[i].lower_limit.q * 100.0));
    }
}

static void anc_file_ear_adaptive_weight_param_printf(int id, void *buf, int len)
{
    struct __anc_ext_ear_adaptive_weight_param *cfg = (struct __anc_ext_ear_adaptive_weight_param *)buf;
    anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT_PARAM_ID 0x%x---\n", id);
    anc_ext_log("mse_level_l           :%d \n", cfg->mse_level_l);
    anc_ext_log("mse_level_h           :%d \n", cfg->mse_level_h);
}

static void anc_file_cfg_data_float_printf(int id, void *buf, int len)
{
    struct __anc_ext_ear_adaptive_weight *cfg = (struct __anc_ext_ear_adaptive_weight *)buf;
    int cnt = len / sizeof(float);
    switch (id) {
    case ANC_EXT_EAR_ADAPTIVE_FF_HIGH_WEIGHT_ID ... ANC_EXT_EAR_ADAPTIVE_FF_LOW_MSE_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_WEIGHT_ID ... ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_MSE_ID:
        anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT/MSE_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    case ANC_EXT_EAR_ADAPTIVE_FF_TARGET_SV_ID ... ANC_EXT_EAR_ADAPTIVE_FF_TARGET_CMP_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_SV_ID ... ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_CMP_ID:
        anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_TARGET_SV/CMP_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    case ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PZ_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_SZ_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PZ_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_SZ_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PZ_COEFF_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_SZ_COEFF_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PZ_COEFF_ID:
    case ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_SZ_COEFF_ID:
        anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    case ANC_EXT_EAR_ADAPTIVE_FF_DUT_PZ_CMP_ID ... ANC_EXT_EAR_ADAPTIVE_FF_R_DUT_SZ_CMP_ID:
        anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_DUT_CMP_ID 0x%x, cnt %d---\n", id, cnt);
    case ANC_EXT_ADAPTIVE_CMP_WEIGHT_ID ... ANC_EXT_ADAPTIVE_CMP_MSE_ID:
    case ANC_EXT_ADAPTIVE_CMP_R_WEIGHT_ID ... ANC_EXT_ADAPTIVE_CMP_R_MSE_ID:
        anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_CMP_WEIGHT/MSE_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    case ANC_EXT_ADAPTIVE_EQ_WEIGHT_ID ... ANC_EXT_ADAPTIVE_EQ_MSE_ID:
    case ANC_EXT_ADAPTIVE_EQ_R_WEIGHT_ID ... ANC_EXT_ADAPTIVE_EQ_R_MSE_ID:
        anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_EQ_WEIGHT/MSE_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    case ANC_EXT_REF_SZ_DATA_ID ... ANC_EXT_REF_SZ_R_DATA_ID:
        anc_ext_log("---ANC_EXT_REF_SZ_DATA_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    case ANC_EXT_ADAPTIVE_CMP_SZ_FACTOR_ID ... ANC_EXT_ADAPTIVE_CMP_R_SZ_FACTOR_ID:
        anc_ext_log("---ANC_EXT_ADAPTIVE_CMP_SZ_FACTOR_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    }
#if ANC_EXT_CFG_FLOAT_EN
    for (int i = 0; i < cnt; i++) {
        anc_ext_log("data :%d/100 \n", (int)(cfg->data[i] * 100));
    }
#endif
    //put_buf(buf, len);
}

static void anc_file_ear_adaptive_target_param_printf(int id, void *buf, int len)
{
    struct __anc_ext_ear_adaptive_target_param *cfg = (struct __anc_ext_ear_adaptive_target_param *)buf;
    anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_TARGET_PARAM_ID 0x%x---\n", id);
    anc_ext_log("cmp_en           :%d \n", cfg->cmp_en);
}

static void anc_file_ear_adaptive_mem_param_printf(int id, void *buf, int len)
{
    struct __anc_ext_ear_adaptive_mem_param *cfg = (struct __anc_ext_ear_adaptive_mem_param *)buf;
    anc_ext_log("---ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PARAM_ID 0x%x---\n", id);
    anc_ext_log("mem_curve_nums   :%d \n", cfg->mem_curve_nums);
    anc_ext_log("ear_recorder       :%d \n", cfg->ear_recorder);
}

static void anc_file_rtanc_adaptive_cfg_printf(int id, void *buf, int len)
{
    struct __anc_ext_rtanc_adaptive_cfg *cfg = (struct __anc_ext_rtanc_adaptive_cfg *)buf;

    anc_ext_log("---ANC_EXT_RTANC_ADAPTIVE_CFG_ID 0x%x---\n", id);

    anc_ext_log("== Configuration ==\n");
    anc_ext_log("norm_mat_lidx    :%d \n", cfg->norm_mat_lidx);
    anc_ext_log("norm_mat_hidx    :%d \n", cfg->norm_mat_hidx);
    anc_ext_log("norm_tg_thr      :%d/100 \n", (int)(cfg->norm_tg_thr * 100.0f));
    // -Music Controller
    anc_ext_log("msc_tg_thr       :%d/100 \n", (int)(cfg->msc_tg_thr * 100.0f));
    anc_ext_log("msc_err_thr0     :%d/100 \n", (int)(cfg->msc_err_thr0 * 100.0f));
    anc_ext_log("msc_err_thr1     :%d/100 \n", (int)(cfg->msc_err_thr1 * 100.0f));
    // -Other
    anc_ext_log("low_spl_thr      :%d/100 \n", (int)(cfg->low_spl_thr * 100.0f));
    anc_ext_log("splcnt_add_thr   :%d/100 \n", (int)(cfg->splcnt_add_thr * 100.0f));
    // -LMS Controller
    anc_ext_log("lms_err          :%d/100 \n", (int)(cfg->lms_err * 100.0f));

    anc_ext_log("== Wind Controller ==\n");
    anc_ext_log("wind_det_en      :%d \n", cfg->wind_det_en);
    anc_ext_log("wind_lvl_thr     :%d \n", cfg->wind_lvl_thr);
    anc_ext_log("wind_lock_cnt    :%d \n", cfg->wind_lock_cnt);
    anc_ext_log("wind_ref_thr     :%d/100 \n", (int)(cfg->wind_ref_thr * 100.0f));
    anc_ext_log("wind_ref_max     :%d/100 \n", (int)(cfg->wind_ref_max * 100.0f));
    anc_ext_log("wind_ref_min     :%d/100 \n", (int)(cfg->wind_ref_min * 100.0f));
    anc_ext_log("wind_miu_div     :%d/100 \n", (int)(cfg->wind_miu_div * 100.0f));

    anc_ext_log("== Music Controller ==\n");
    anc_ext_log("msc_dem_thr0     :%d \n", cfg->msc_dem_thr0);
    anc_ext_log("msc_dem_thr1     :%d \n", cfg->msc_dem_thr1);
    anc_ext_log("msc_mat_lidx     :%d \n", cfg->msc_mat_lidx);
    anc_ext_log("msc_mat_hidx     :%d \n", cfg->msc_mat_hidx);
    anc_ext_log("msc_idx_thr      :%d \n", cfg->msc_idx_thr);
    anc_ext_log("msc_scnt_thr     :%d \n", cfg->msc_scnt_thr);
    anc_ext_log("msc2norm_updat_cnt:%d \n", cfg->msc2norm_updat_cnt);
    anc_ext_log("msc_lock_cnt     :%d \n", cfg->msc_lock_cnt);
    anc_ext_log("msc_tg_diff_lidx :%d \n", cfg->msc_tg_diff_lidx);
    anc_ext_log("msc_tg_diff_hidx :%d \n", cfg->msc_tg_diff_hidx);

    anc_ext_log("== CMP Controller ==\n");
    anc_ext_log("cmp_updat_thr    :%d/100 \n", (int)(cfg->cmp_updat_thr * 100.0f));
    anc_ext_log("cmp_updat_fast_thr:%d/100 \n", (int)(cfg->cmp_updat_fast_thr * 100.0f));
    anc_ext_log("cmp_mul_factor   :%d/100 \n", (int)(cfg->cmp_mul_factor * 100.0f));
    anc_ext_log("cmp_div_factor   :%d/100 \n", (int)(cfg->cmp_div_factor * 100.0f));
    anc_ext_log("cmp_diff_lidx    :%d \n", cfg->cmp_diff_lidx);
    anc_ext_log("cmp_diff_hidx    :%d \n", cfg->cmp_diff_hidx);
    anc_ext_log("cmp_idx_thr      :%d \n", cfg->cmp_idx_thr);
    anc_ext_log("cmp_adpt_en      :%d \n", cfg->cmp_adpt_en);

    anc_ext_log("== Large Noise Controller ==\n");
    anc_ext_log("noise_mse_thr    :%d/100 \n", (int)(cfg->noise_mse_thr * 100.0f));
    anc_ext_log("noise_idx_thr    :%d \n", cfg->noise_idx_thr);
    anc_ext_log("noise_updat_thr  :%d \n", cfg->noise_updat_thr);
    anc_ext_log("noise_ffdb_thr   :%d \n", cfg->noise_ffdb_thr);

    anc_ext_log("== LMS Controller ==\n");
    anc_ext_log("mse_lidx         :%d \n", cfg->mse_lidx);
    anc_ext_log("mse_hidx         :%d \n", cfg->mse_hidx);
    anc_ext_log("mse_thr1         :%d/100 \n", (int)(cfg->mse_thr1 * 100.0f));
    anc_ext_log("mse_thr2         :%d/100 \n", (int)(cfg->mse_thr2 * 100.0f));
    anc_ext_log("uscale           :%d/100 \n", (int)(cfg->uscale * 100.0f));
    anc_ext_log("uoffset          :%d/100 \n", (int)(cfg->uoffset * 100.0f));
    anc_ext_log("u_pre_thr        :%d/100 \n", (int)(cfg->u_pre_thr * 100.0f));
    anc_ext_log("u_cur_thr        :%d/100 \n", (int)(cfg->u_cur_thr * 100.0f));
    anc_ext_log("u_first_thr      :%d/100 \n", (int)(cfg->u_first_thr * 100.0f));

    anc_ext_log("== Fast Update Controller ==\n");
    anc_ext_log("fast_cnt         :%d \n", cfg->fast_cnt);
    anc_ext_log("fast_ind_thr     :%d \n", cfg->fast_ind_thr);
    anc_ext_log("fast_ind_div     :%d \n", cfg->fast_ind_div);

    anc_ext_log("== Update Controller ==\n");
    anc_ext_log("tg_lidx          :%d \n", cfg->tg_lidx);
    anc_ext_log("tg_hidx          :%d \n", cfg->tg_hidx);
    anc_ext_log("rewear_idx_thr   :%d \n", cfg->rewear_idx_thr);
    anc_ext_log("norm_updat_cnt0  :%d \n", cfg->norm_updat_cnt0);
    anc_ext_log("norm_updat_cnt1  :%d \n", cfg->norm_updat_cnt1);
    anc_ext_log("norm_cmp_cnt0    :%d \n", cfg->norm_cmp_cnt0);
    anc_ext_log("norm_cmp_cnt1    :%d \n", cfg->norm_cmp_cnt1);
    anc_ext_log("norm_cmp_thr     :%d/100 \n", (int)(cfg->norm_cmp_thr * 100.0f));
    anc_ext_log("norm_cmp_lidx    :%d \n", cfg->norm_cmp_lidx);
    anc_ext_log("norm_cmp_hidx    :%d \n", cfg->norm_cmp_hidx);
    anc_ext_log("updat_lock_cnt   :%d \n", cfg->updat_lock_cnt);

    anc_ext_log("== Fitting Algorithm Controller ==\n");
    anc_ext_log("ls_fix_mode      :%d \n", cfg->ls_fix_mode);
    anc_ext_log("ls_fix_lidx      :%d \n", cfg->ls_fix_lidx);
    anc_ext_log("ls_fix_hidx      :%d \n", cfg->ls_fix_hidx);
    anc_ext_log("ls_fix_range0    :");
    for (int i = 0; i < sizeof(cfg->ls_fix_range0) / sizeof(cfg->ls_fix_range0[0]); i++) {
        anc_ext_log("%d/100\n", (int)(cfg->ls_fix_range0[i] * 100.0f));
    }

    anc_ext_log("ls_fix_range1    :");
    for (int i = 0; i < sizeof(cfg->ls_fix_range1) / sizeof(cfg->ls_fix_range1[0]); i++) {
        anc_ext_log("%d/100\n", (int)(cfg->ls_fix_range1[i] * 100.0f));
    }

    anc_ext_log("ls_fix_range2    :");
    for (int i = 0; i < sizeof(cfg->ls_fix_range2) / sizeof(cfg->ls_fix_range2[0]); i++) {
        anc_ext_log("%d/100\n", (int)(cfg->ls_fix_range2[i] * 100.0f));
    }
    anc_ext_log("iter_max0        :%d \n", cfg->iter_max0);
    anc_ext_log("iter_max1        :%d \n", cfg->iter_max1);

    anc_ext_log("== mode1 AEQ_CMP Controller ==\n");
    anc_ext_log("m1_dem_cnt0      : %d\n", cfg->m1_dem_cnt0);
    anc_ext_log("m1_dem_cnt1      : %d\n", cfg->m1_dem_cnt1);
    anc_ext_log("m1_dem_cnt2      : %d\n", cfg->m1_dem_cnt2);
    anc_ext_log("m1_scnt0         : %d\n", cfg->m1_scnt0);
    anc_ext_log("m1_scnt1         : %d\n", cfg->m1_scnt1);
    anc_ext_log("m1_dem_off_cnt0  : %d\n", cfg->m1_dem_off_cnt0);
    anc_ext_log("m1_dem_off_cnt1  : %d\n", cfg->m1_dem_off_cnt1);
    anc_ext_log("m1_dem_off_cnt2  : %d\n", cfg->m1_dem_off_cnt2);
    anc_ext_log("m1_scnt0_off     : %d\n", cfg->m1_scnt0_off);
    anc_ext_log("m1_scnt1_off     : %d\n", cfg->m1_scnt1_off);
    anc_ext_log("m1_scnt2_off     : %d\n", cfg->m1_scnt2_off);
    anc_ext_log("m1_idx_thr       : %d\n", cfg->m1_idx_thr);

    anc_ext_log("== Music target Controller ==\n");
    anc_ext_log("msc_iir_idx_thr  : %d\n", cfg->msc_iir_idx_thr);
    anc_ext_log("msc_atg_sm_lidx  : %d\n", cfg->msc_atg_sm_lidx);
    anc_ext_log("msc_atg_sm_hidx  : %d\n", cfg->msc_atg_sm_hidx);
    anc_ext_log("msc_atg_diff_lidx: %d\n", cfg->msc_atg_diff_lidx);
    anc_ext_log("msc_atg_diff_hidx: %d\n", cfg->msc_atg_diff_hidx);
    anc_ext_log("msc_spl_idx_cut  : %d\n", cfg->msc_spl_idx_cut);
    anc_ext_log("msc_tar_coef0    : %d/100\n", (int)(cfg->msc_tar_coef0 * 100.0f));
    anc_ext_log("msc_ind_coef0    : %d/100\n", (int)(cfg->msc_ind_coef0 * 100.0f));
    anc_ext_log("msc_tar_coef1    : %d/100\n", (int)(cfg->msc_tar_coef1 * 100.0f));
    anc_ext_log("msc_ind_coef1    : %d/100\n", (int)(cfg->msc_ind_coef1 * 100.0f));
    anc_ext_log("msc_atg_diff_thr0: %d/100\n", (int)(cfg->msc_atg_diff_thr0 * 100.0f));
    anc_ext_log("msc_atg_diff_thr1: %d/100\n", (int)(cfg->msc_atg_diff_thr1 * 100.0f));
    anc_ext_log("msc_tg_spl_thr   : %d/100\n", (int)(cfg->msc_tg_spl_thr * 100.0f));
    anc_ext_log("msc_sz_sm_thr    : %d/100\n", (int)(cfg->msc_sz_sm_thr * 100.0f));
    anc_ext_log("msc_atg_sm_thr   : %d/100\n", (int)(cfg->msc_atg_sm_thr * 100.0f));

    anc_ext_log("== Target divide Controller ==\n");
    anc_ext_log("tight_divide     : %d\n", cfg->tight_divide);
    anc_ext_log("tight_idx_thr    : %d\n", cfg->tight_idx_thr);
    anc_ext_log("tight_idx_diff   : %d\n", cfg->tight_idx_diff);

    anc_ext_log("== Other Controller ==\n");
    anc_ext_log("spl2norm_cnt     :%d \n", cfg->spl2norm_cnt);
    anc_ext_log("talk_lock_cnt    :%d \n", cfg->talk_lock_cnt);
    anc_ext_log("idx_use_same     :%d \n", cfg->idx_use_same);

    // Undefined parameters (u8 array)
    anc_ext_log("undefine_param0  :");
    for (int i = 0; i < sizeof(cfg->undefine_param0) / sizeof(cfg->undefine_param0[0]); i++) {
        anc_ext_log("%d\n", cfg->undefine_param0[i]);
    }

    // Undefined parameters (float array)
    anc_ext_log("undefine_param1  :");
    for (int i = 0; i < sizeof(cfg->undefine_param1) / sizeof(cfg->undefine_param1[0]); i++) {
        anc_ext_log("%d/100\n ", (int)(cfg->undefine_param1[i] * 100.0f));
    }
}

static void anc_file_dynamic_cfg_printf(int id, void *buf, int len)
{
    struct __anc_ext_dynamic_cfg *cfg = (struct __anc_ext_dynamic_cfg *)buf;

    anc_ext_log("---ANC_EXT_DYNAMIC_CFG_ID 0x%x---\n", id);

    anc_ext_log("== Common ==\n");
    anc_ext_log("ref_iir_coef      :%d/10000 \n", (int)(cfg->ref_iir_coef * 10000.0f));
    anc_ext_log("err_iir_coef      :%d/10000 \n", (int)(cfg->err_iir_coef * 10000.0f));

    anc_ext_log("== SDCC Controller ==\n");
    anc_ext_log("sdcc_det_en       :%d \n", cfg->sdcc_det_en);
    anc_ext_log("sdcc_ref_thr      :%d/100 \n", (int)(cfg->sdcc_ref_thr * 100.0f));
    anc_ext_log("sdcc_err_thr      :%d/100 \n", (int)(cfg->sdcc_err_thr * 100.0f));
    anc_ext_log("sdcc_thr_cmp      :%d/100 \n", (int)(cfg->sdcc_thr_cmp * 100.0f));
    anc_ext_log("sdcc_par1         :%d \n", cfg->sdcc_par1);
    anc_ext_log("sdcc_par2         :%d \n", cfg->sdcc_par2);
    anc_ext_log("sdcc_flag_thr     :%d \n", cfg->sdcc_flag_thr);

    anc_ext_log("== SDRC Trigger ==\n");
    anc_ext_log("sdrc_det_en       :%d \n", cfg->sdrc_det_en);
    anc_ext_log("sdrc_ref_thr      :%d/100 \n", (int)(cfg->sdrc_ref_thr * 100.0f));
    anc_ext_log("sdrc_err_thr      :%d/100 \n", (int)(cfg->sdrc_err_thr * 100.0f));
    anc_ext_log("sdrc_flag_thr     :%d \n", cfg->sdrc_flag_thr);
    anc_ext_log("sdrc_cnt_thr      :%d \n", cfg->sdrc_cnt_thr);

    anc_ext_log("== SDRC Controller ==\n");
    anc_ext_log("sdrc_ref_margin   :%d/100 \n", (int)(cfg->sdrc_ref_margin * 100.0f));
    anc_ext_log("sdrc_err_margin   :%d/100 \n", (int)(cfg->sdrc_err_margin * 100.0f));
    anc_ext_log("sdrc_fb_att_thr   :%d/100 \n", (int)(cfg->sdrc_fb_att_thr * 100.0f));
    anc_ext_log("sdrc_fb_rls_thr   :%d/100 \n", (int)(cfg->sdrc_fb_rls_thr * 100.0f));
    anc_ext_log("sdrc_fb_att_step  :%d/100 \n", (int)(cfg->sdrc_fb_att_step * 100.0f));
    anc_ext_log("sdrc_fb_rls_step  :%d/100 \n", (int)(cfg->sdrc_fb_rls_step * 100.0f));
    anc_ext_log("sdrc_fb_set_thr   :%d/100 \n", (int)(cfg->sdrc_fb_set_thr * 100.0f));
    anc_ext_log("sdrc_ff_att_thr   :%d/100 \n", (int)(cfg->sdrc_ff_att_thr * 100.0f));
    anc_ext_log("sdrc_ff_rls_thr   :%d/100 \n", (int)(cfg->sdrc_ff_rls_thr * 100.0f));
    anc_ext_log("sdrc_ls_min_gain  :%d/100 \n", (int)(cfg->sdrc_ls_min_gain * 100.0f));
    anc_ext_log("sdrc_ls_att_step  :%d/100 \n", (int)(cfg->sdrc_ls_att_step * 100.0f));
    anc_ext_log("sdrc_ls_rls_step  :%d/100 \n", (int)(cfg->sdrc_ls_rls_step * 100.0f));
    anc_ext_log("sdrc_ls_rls_lidx  :%d \n", cfg->sdrc_ls_rls_lidx);
    anc_ext_log("sdrc_ls_rls_hidx  :%d \n", cfg->sdrc_ls_rls_hidx);
    anc_ext_log("sdrc_rls_ls_cmp   :%d/100 \n", (int)(cfg->sdrc_rls_ls_cmp * 100.0f));

    anc_ext_log("== Other Controller ==\n");

    // Undefined parameters (u8 array)
    anc_ext_log("undefine_param0   :");
    for (int i = 0; i < sizeof(cfg->undefine_param0) / sizeof(cfg->undefine_param0[0]); i++) {
        anc_ext_log("%d\n ", cfg->undefine_param0[i]);
    }

    anc_ext_log("undefine_param1   :");
    for (int i = 0; i < sizeof(cfg->undefine_param1) / sizeof(cfg->undefine_param1[0]); i++) {
        anc_ext_log("%d/100\n ", (int)(cfg->undefine_param1[i] * 100.0f));
    }
}

static void anc_file_adaptive_eq_thr_printf(int id, void *buf, int len)
{
    struct __anc_ext_adaptive_eq_thr *cfg = (struct __anc_ext_adaptive_eq_thr *)buf;
    anc_ext_log("---ANC_EXT_ADAPTIVE_EQ_THR_ID 0x%x---\n", id);

    // 音量阈值参数
    anc_ext_log("vol_thr1         :%d\n", cfg->vol_thr1);
    anc_ext_log("vol_thr2         :%d\n", cfg->vol_thr2);

    // 打印二维数组 max_dB[3][3]
    anc_ext_log("max_dB           :\n");
    for (int i = 0; i < 3; i++) {
        anc_ext_log("                [%d, %d, %d]\n", cfg->max_dB[i][0], cfg->max_dB[i][1], cfg->max_dB[i][2]);
    }

    // dot阈值参数
    anc_ext_log("dot_thr1         :%d/100\n", (int)(cfg->dot_thr1 * 100.f));
    anc_ext_log("dot_thr2         :%d/100\n", (int)(cfg->dot_thr2 * 100.f));
}

//风噪检测配置打印
static void anc_file_wind_det_cfg_printf(int id, void *buf, int len)
{
    struct __anc_ext_wind_det_cfg *cfg = (struct __anc_ext_wind_det_cfg *)buf;

    anc_ext_log("---ANC_EXT_WIND_DET_CFG_ID 0x%x---\n", id);

    anc_ext_log("wind_lvl_scale   :%d \n", cfg->wind_lvl_scale);
    anc_ext_log("icsd_wind_num_thr1 :%d \n", cfg->icsd_wind_num_thr1);
    anc_ext_log("icsd_wind_num_thr2 :%d \n", cfg->icsd_wind_num_thr2);
    anc_ext_log("wind_iir_alpha   :%d/100 \n", (int)(cfg->wind_iir_alpha * 100.0f));
    anc_ext_log("corr_thr         :%d/100 \n", (int)(cfg->corr_thr * 100.0f));
    anc_ext_log("msc_lp_thr       :%d/100 \n", (int)(cfg->msc_lp_thr * 100.0f));
    anc_ext_log("msc_mp_thr       :%d/100 \n", (int)(cfg->msc_mp_thr * 100.0f));
    anc_ext_log("cpt_1p_thr       :%d/100 \n", (int)(cfg->cpt_1p_thr * 100.0f));
    anc_ext_log("ref_pwr_thr      :%d/100 \n", (int)(cfg->ref_pwr_thr * 100.0f));
    for (int i = 0; i < 6; i++) {
        anc_ext_log("param[%d]         :%d/100 \n", i, (int)(cfg->param[i] * 100.0f));
    }
}

//风噪触发配置打印
static void anc_file_wind_trigger_cfg_printf(int id, void *buf, int len)
{
    struct __anc_ext_wind_trigger_cfg *cfg = (struct __anc_ext_wind_trigger_cfg *)buf;

    anc_ext_log("---ANC_EXT_WIND_TRIGGER_CFG_ID 0x%x---\n", id);

    for (int i = 0; i < 5; i++) {
        anc_ext_log("thr[%d]            :%d \n", i, cfg->thr[i]);
        anc_ext_log("gain[%d]           :%d \n", i, cfg->gain[i]);
    }
}

static void anc_file_soft_howl_det_cfg_printf(int id, void *buf, int len)
{
    struct __anc_ext_soft_howl_det_cfg *cfg = (struct __anc_ext_soft_howl_det_cfg *)buf;

    anc_ext_log("---ANC_EXT_SOFT_HOWL_DET_CFG_ID 0x%x---\n", id);
    anc_ext_log("hd_scale          :%d/100 \n", (int)(cfg->hd_scale * 100.0f));
    anc_ext_log("hd_sat_thr        :%d \n", cfg->hd_sat_thr);
    anc_ext_log("hd_det_thr        :%d/100 \n", (int)(cfg->hd_det_thr * 100.0f));
    anc_ext_log("hd_diff_pwr_thr   :%d/100 \n", (int)(cfg->hd_diff_pwr_thr * 100.0f));
    anc_ext_log("hd_maxind_thr1    :%d \n", cfg->hd_maxind_thr1);
    anc_ext_log("hd_maxind_thr2    :%d \n", cfg->hd_maxind_thr2);

    for (int i = 0; i < 6; i++) {
        anc_ext_log("param[%d]         :%d/100 \n", i, (int)(cfg->param[i] * 100.0f));
    }
}

static void anc_file_adaptive_mem_iir_printf(int id, void *buf, int len)
{
    struct __anc_ext_adaptive_mem_iir *cfg = (struct __anc_ext_adaptive_mem_iir *)buf;
    //获取mem_iir的成员个数
    int cnt = (len - offsetof(struct __anc_ext_adaptive_mem_iir, mem_iir)) / sizeof(s16);
    switch (id) {
    case ANC_EXT_ADAPTIVE_CMP_RECORDER_IIR_ID:
    case ANC_EXT_ADAPTIVE_CMP_R_RECORDER_IIR_ID:
        anc_ext_log("---ANC_EXT_ADAPTIVE_CMP_RECORDER_IIR_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    case ANC_EXT_ADAPTIVE_EQ_RECORDER_IIR_ID:
    case ANC_EXT_ADAPTIVE_EQ_R_RECORDER_IIR_ID:
        anc_ext_log("---ANC_EXT_ADAPTIVE_EQ_RECORDER_IIR_ID 0x%x, cnt %d---\n", id, cnt);
        break;
    }
    anc_ext_log("input_crc 0x%x\n", cfg->input_crc);
#if ANC_EXT_CFG_FLOAT_EN
    for (int i = 0; i < cnt; i++) {
        anc_ext_log("mem_iir[%d] :%d \n", i, cfg->mem_iir[i]);
    }
#endif
    //put_buf(buf, len);
}

/*
   ANCEXT 耳道自适应SUBFILE内参数ID解析
param: file_data, file_len
*/
static int anc_cfg_analysis_ear_adaptive(u8 *file_data, int file_len, u8 alloc_flag)
{
    int i, j;
    u8 *temp_dat;
    struct anc_ext_subfile_head *file = (struct anc_ext_subfile_head *)file_data;
    struct anc_ext_ear_adaptive_param *p = &tool_hdl->ear_adaptive;
    const struct __anc_ext_subfile_id_table *table = ear_adaptive_id_table;
    int table_cnt = ARRAY_SIZE(ear_adaptive_id_table);

    if (file) {
        /* anc_ext_log("file->cnt %d\n", file->id_cnt); */
        if (!file->id_cnt) {
            return 1;
        }
        struct anc_ext_id_head *id_head = (struct anc_ext_id_head *)file->data;
        temp_dat = file->data + (file->id_cnt * sizeof(struct anc_ext_id_head));
        for (i = 0; i < file->id_cnt; i++) {
            /* anc_ext_log("id:0X%x, offset:%d,len:%d\n", id_head->id, id_head->offset, id_head->len); */
            for (j = 0; j < table_cnt; j++) {
                if (id_head->id == table[j].id) {
                    if (id_head->len) {
                        u32 dat_p = (u32)(temp_dat + id_head->offset);
                        if (alloc_flag) {	//工具在线调试申请临时空间
                            u8 *temp = anc_ext_tool_data_alloc(id_head->id, temp_dat + id_head->offset, id_head->len);
                            if (!temp) {
                                anc_ext_log("Analysis tool_alloc err!! data_id %d\n", id_head->id);
                                return -1;
                            }
                            dat_p = (u32)temp;
                        }
                        //拷贝目标地址至对应参数结构体指针
                        memcpy(((u8 *)p) + table[j].offset, (u8 *)(&dat_p), sizeof(u32));
#if ANC_EXT_CFG_LOG_EN
                        /* put_buf((u8 *)dat_p, id_head->len); */
                        anc_ext_printf[j].p(id_head->id, (void *)dat_p, id_head->len);
#endif
                    }
                }
            }
            id_head++;
        }
        return 0;
    }
    return 1;
}

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
/*
   ANC_EXT 耳道自适应参数检查,如报错请检查
   1、使用时: anc_ext.bin是否包含对应参数？
   2、调试时：工具是否下发对应参数？
*/
u8 anc_ext_ear_adaptive_param_check(void)
{
    struct anc_ext_ear_adaptive_param *cfg = &tool_hdl->ear_adaptive;
    for (u32 i = (u32)(&cfg->base_cfg); i <= (u32)(&cfg->ff_target_param); i += 4) {
        if ((*(u32 *)i) == 0) {
            anc_ext_log("ERR:ANC_EXT cfg no enough!, offset=%d\n", i);
            return ANC_EXT_FAIL_EAR_BASE_CFG_MISS;
        }
    }
    if (cfg->ff_target_param->cmp_curve_num && \
        (!cfg->ff_target_sv || !cfg->ff_target_cmp)) {
        anc_ext_log("ERR:ANC_EXT target cfg no enough!, sv %p, cmp %p\n", \
                    cfg->ff_target_sv, cfg->ff_target_cmp);
        return ANC_EXT_FAIL_EAR_TARGET_CFG_MISS;
    }
    if (cfg->ff_ear_mem_param) {
        if (cfg->ff_ear_mem_param->ear_recorder && (!cfg->ff_ear_mem_pz || !cfg->ff_ear_mem_sz)) {
            anc_ext_log("ERR:ANC_EXT ear_recoder cfg no enough! pz %p, sz %p\n", \
                        cfg->ff_ear_mem_pz, cfg->ff_ear_mem_sz);
            return ANC_EXT_FAIL_EAR_MEM_CFG_MISS;
        }
    } else {
        anc_ext_log("ERR:ANC_EXT ear_recoder cfg no enough! param %p\n", cfg->ff_ear_mem_param);
        return ANC_EXT_FAIL_EAR_MEM_CFG_MISS;
    }
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    if (anc_ext_adaptive_cmp_tool_en_get()) {
        if (!cfg->cmp_gains) {
            anc_ext_log("ERR:ANC_EXT adaptive cmp cfg no enough! %p\n", cfg->cmp_gains);
            return ANC_EXT_FAIL_CMP_CFG_MISS;
        }
#if AUDIO_ANC_ADAPTIVE_CMP_SZ_FACTOR
        if (!cfg->cmp_sz_factor) {
            anc_ext_log("ERR:ANC_EXT adaptive cmp sz_factor no enough! %p\n", cfg->cmp_sz_factor);
            return ANC_EXT_FAIL_CMP_SZ_FACTOR_MISS;
        }
#endif
    }
#endif

    if (ICSD_EP_TYPE_V2 == ICSD_HEADSET) {
        for (u32 i = (u32)(&cfg->rff_iir_high_gains); i <= (u32)(&cfg->rff_target_param); i += 4) {
            if ((*(u32 *)i) == 0) {
                anc_ext_log("ERR:ANC_EXT R cfg no enough!, offset=%d\n", i);
                return ANC_EXT_FAIL_EAR_BASE_CFG_MISS;
            }
        }
        if (cfg->rff_target_param->cmp_curve_num && \
            (!cfg->rff_target_sv || !cfg->rff_target_cmp)) {
            anc_ext_log("ERR:ANC_EXT R target cfg no enough!, sv %p, cmp %p\n", \
                        cfg->rff_target_sv, cfg->rff_target_cmp);
            return ANC_EXT_FAIL_EAR_TARGET_CFG_MISS;
        }
        if (cfg->rff_ear_mem_param) {
            if (cfg->rff_ear_mem_param->ear_recorder && (!cfg->rff_ear_mem_pz || !cfg->rff_ear_mem_sz)) {
                anc_ext_log("ERR:ANC_EXT R ear_recoder cfg no enough! pz %p, sz %p\n", \
                            cfg->rff_ear_mem_pz, cfg->rff_ear_mem_sz);
                return ANC_EXT_FAIL_EAR_MEM_CFG_MISS;
            }
        } else {
            anc_ext_log("ERR:ANC_EXT R ear_recoder cfg no enough! param %p\n", cfg->rff_ear_mem_param);
            return ANC_EXT_FAIL_EAR_MEM_CFG_MISS;
        }
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
        if (anc_ext_adaptive_cmp_tool_en_get()) {
            if (!cfg->rcmp_gains) {
                anc_ext_log("ERR:ANC_EXT R adaptive cmp cfg no enough! %p\n", cfg->rcmp_gains);
                return ANC_EXT_FAIL_CMP_CFG_MISS;
            }
        }
#if AUDIO_ANC_ADAPTIVE_CMP_SZ_FACTOR
        if (!cfg->rcmp_sz_factor) {
            anc_ext_log("ERR:ANC_EXT R adaptive cmp sz_factor no enough! %p\n", cfg->rcmp_sz_factor);
            return ANC_EXT_FAIL_CMP_SZ_FACTOR_MISS;
        }
#endif
#endif
    }
    return 0;
}

/*
   ANC_EXT RTANC参数检查, 如报错请检查
   1、使用时: anc_ext.bin是否包含对应参数？
   2、调试时：工具是否下发对应参数？
*/
u8 anc_ext_rtanc_param_check(void)
{
    struct anc_ext_ear_adaptive_param *cfg = &tool_hdl->ear_adaptive;
    if ((!anc_ext_rtanc_adaptive_cfg_get(0)) || anc_ext_ear_adaptive_param_check()) {
        anc_ext_log("ERR:ANC_EXT RTANC cfg no enough! rt %p, ear_adaptive = %d\n", anc_ext_rtanc_adaptive_cfg_get(0), \
                    anc_ext_ear_adaptive_param_check());
        return ANC_EXT_FAIL_RTANC_CFG_MISS;
    }
    if (!anc_ext_dynamic_cfg_get(0)) {
        anc_ext_log("ERR:ANC_EXT DYNAMIC cfg no enough! param %p\n", anc_ext_dynamic_cfg_get(0));
        return ANC_EXT_FAIL_DYNAMIC_CFG_MISS;
    }
    if (!cfg->ff_ear_mem_pz_coeff || !cfg->ff_ear_mem_sz_coeff) {
        anc_ext_log("ERR:ANC_EXT ear_recoder cfg no enough! pz_coeff %p, sz_coeff %p\n", \
                    cfg->ff_ear_mem_pz_coeff, cfg->ff_ear_mem_sz_coeff);
        return ANC_EXT_FAIL_SPZ_COEFF_CFG_MISS;
    }
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    if (anc_ext_adaptive_cmp_tool_en_get()) {
        if (!cfg->cmp_mem_iir) {
            anc_ext_log("ERR:ANC_EXT ear_recoder cfg no enough! cmp_mem_iir %p\n", cfg->cmp_mem_iir);
            return ANC_EXT_FAIL_CMP_MEM_CFG_MISS;
        }
    }
#endif

#if TCFG_AUDIO_ANC_CH == (ANC_L_CH | ANC_R_CH)
    if (!anc_ext_rtanc_adaptive_cfg_get(1)) {
        anc_ext_log("ERR:ANC_EXT RTANC R cfg no enough! rt %p\n", anc_ext_rtanc_adaptive_cfg_get(1));
        return ANC_EXT_FAIL_RTANC_CFG_MISS;
    }
    if (!anc_ext_dynamic_cfg_get(1)) {
        anc_ext_log("ERR:ANC_EXT DYNAMIC R cfg no enough! param %p\n", anc_ext_dynamic_cfg_get(1));
        return ANC_EXT_FAIL_DYNAMIC_CFG_MISS;
    }
    if (!cfg->rff_ear_mem_pz_coeff || !cfg->rff_ear_mem_sz_coeff) {
        anc_ext_log("ERR:ANC_EXT ear_recoder R cfg no enough! pz_coeff %p, sz_coeff %p\n", \
                    cfg->rff_ear_mem_pz_coeff, cfg->rff_ear_mem_sz_coeff);
        return ANC_EXT_FAIL_SPZ_COEFF_CFG_MISS;
    }
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    if (anc_ext_adaptive_cmp_tool_en_get()) {
        if (!cfg->rcmp_mem_iir) {
            anc_ext_log("ERR:ANC_EXT ear_recoder R cfg no enough! cmp_mem_iir %p\n", cfg->rcmp_mem_iir);
            return ANC_EXT_FAIL_CMP_MEM_CFG_MISS;
        }
    }
#endif
#endif
    return 0;
}

u8 anc_ext_ear_adaptive_result_from(void)
{
    if (tool_hdl) {
        if (tool_hdl->ear_adaptive.base_cfg) {
            return tool_hdl->ear_adaptive.base_cfg->adaptive_mode;
        }
    }
    return EAR_ADAPTIVE_FAIL_RESULT_FROM_ADAPTIVE;
}

s8 anc_ext_ear_adaptive_sz_calr_sign_get(void)
{
    if (tool_hdl) {
        if (tool_hdl->ear_adaptive.base_cfg) {
            return tool_hdl->ear_adaptive.base_cfg->calr_sign[0];
        }
    }
    return 0;
}

#endif

void anc_ext_ear_adaptive_dut_mode_set(u8 dut_mode)
{
    tool_hdl->ear_adaptive.dut_mode = dut_mode;
}

//ANC_EXT 获取工具配置参数句柄
struct anc_ext_ear_adaptive_param *anc_ext_ear_adaptive_cfg_get(void)
{
    if (tool_hdl) {
        return &tool_hdl->ear_adaptive;
    }
    return NULL;
}

#if TCFG_AUDIO_ANC_ENABLE && TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
//ANC_EXT 获取RTANC 工具参数
struct __anc_ext_rtanc_adaptive_cfg *anc_ext_rtanc_adaptive_cfg_get(u8 ch)
{
    if (tool_hdl) {
        return (ch) ? tool_hdl->ear_adaptive.r_rtanc_adaptive_cfg :
               tool_hdl->ear_adaptive.rtanc_adaptive_cfg;
    }
    return NULL;
}

struct __anc_ext_dynamic_cfg *anc_ext_dynamic_cfg_get(u8 ch)
{
    if (tool_hdl) {
        return (ch) ? tool_hdl->ear_adaptive.r_dynamic_cfg :
               tool_hdl->ear_adaptive.dynamic_cfg;
    }
    return NULL;
}
#endif

//ANC_EXT 查询工具是否在线
u8 anc_ext_tool_online_get(void)
{
    if (tool_hdl) {
        return tool_hdl->tool_online;
    }
    return 0;
}

//ANC_EXT 工具断开连接
void anc_ext_tool_disconnect(void)
{
    if (tool_hdl) {
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        //退出工具恢复工具挂起的RTANC
        audio_anc_real_time_adaptive_resume("ANC_EXT_TOOL");
#endif
        /* tool_hdl->tool_online = 0; */
    }
}

//获取耳道自适应训练模式
enum ANC_EAR_ADPTIVE_MODE anc_ext_ear_adaptive_train_mode_get(void)
{
    if (tool_hdl) {
        return tool_hdl->ear_adaptive.train_mode;
    }
    return EAR_ADAPTIVE_MODE_NORAML;
}

int anc_ext_debug_tool_function_get(void)
{
    if (tool_hdl) {
        return tool_hdl->DEBUG_TOOL_FUNCTION;
    }
    return 0;
}

int anc_ext_adaptive_cmp_tool_en_set(u8 en)
{
    if (tool_hdl) {
        tool_hdl->adaptive_cmp_en = en;
        if (en) {
            anc_ext_algorithm_state_update(ANC_EXT_ALGO_ADAPTIVE_CMP, ANC_EXT_ALGO_STA_OPEN, 0);
        } else {
            anc_ext_algorithm_state_update(ANC_EXT_ALGO_ADAPTIVE_CMP, ANC_EXT_ALGO_STA_CLOSE, 0);
        }
    }
    return 0;
}

int anc_ext_adaptive_cmp_tool_en_get(void)
{
    if (tool_hdl) {
        return tool_hdl->adaptive_cmp_en;
    }
    return 1;
}

int anc_ext_adaptive_eq_tool_en_set(u8 en)
{
    if (tool_hdl) {
        tool_hdl->adaptive_eq_en = en;
        if (en) {
            anc_ext_algorithm_state_update(ANC_EXT_ALGO_ADAPTIVE_EQ, ANC_EXT_ALGO_STA_OPEN, 0);
        } else {
            anc_ext_algorithm_state_update(ANC_EXT_ALGO_ADAPTIVE_EQ, ANC_EXT_ALGO_STA_CLOSE, 0);
        }
    }
    return 0;
}

int anc_ext_adaptive_eq_tool_en_get(void)
{
    if (tool_hdl) {
        return tool_hdl->adaptive_eq_en;
    }
    return 1;
}

static int anc_ext_algorithm_en_set(u8 func, u8 enable)
{
    switch (func) {
#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    case ANC_EXT_ALGO_ADAPTIVE_CMP:
        anc_ext_adaptive_cmp_tool_en_set(enable);
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        if (audio_anc_real_time_adaptive_state_get()) {
            audio_icsd_adt_reset(0);
        }
#endif
        break;
#endif
#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    case ANC_EXT_ALGO_ADAPTIVE_EQ:
        anc_ext_adaptive_eq_tool_en_set(enable);
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
        audio_anc_real_time_adaptive_suspend("AEQ_EN");
        if (enable) {
            audio_real_time_adaptive_eq_open(AUDIO_ADAPTIVE_FRE_SEL_ANC, NULL);
        } else {
            audio_real_time_adaptive_eq_close();
        }
        audio_anc_real_time_adaptive_resume("AEQ_EN");
#endif
        break;
#endif
#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    case ANC_EXT_ALGO_WIND_DET:
        if (enable) {
            audio_icsd_wind_detect_open();
        } else {
            audio_icsd_wind_detect_close();
        }
        break;
#endif
#if TCFG_AUDIO_ANC_HOWLING_DET_ENABLE
    case ANC_EXT_ALGO_SOFT_HOWL_DET:
        if (enable) {
            audio_anc_howling_detect_open();
        } else {
            audio_anc_howling_detect_close();
        }
        break;
#endif
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    case ANC_EXT_ALGO_RTANC:
        if (enable) {
            audio_real_time_adaptive_app_open_demo();
        } else {
            audio_real_time_adaptive_app_close_demo();
        }
        break;
#endif
    default:
        break;
    }
    return 0;
}

static void anc_ext_algorithm_state_init(void)
{
    if (!tool_hdl) {
        return;
    }
    tool_hdl->report_state_len = (ANC_EXT_ALGO_EXIT - 1) << 1;
    u8 *buf = anc_malloc("ANC_EXT", tool_hdl->report_state_len);
    for (int i = 1; i < ANC_EXT_ALGO_EXIT; i++) {
        buf[(i - 1) << 1] = i;
    }
    tool_hdl->report_state_buf = buf;
}

/*
   ANC算法状态数据上报
	param: active  0 被动应答；1 主动发送
*/
static int anc_ext_algorithm_state_report(u8 active)
{
    if (!tool_hdl) {
        return 0;
    }
    //anc_ext_log("%s\n", __func__);
    //put_buf(tool_hdl->report_state_buf, tool_hdl->report_state_len);
    if (active)  {
        audio_anc_debug_report_send_data(CMD_FUNCTION_ALGO_STATE_GET, tool_hdl->report_state_buf, tool_hdl->report_state_len);
    } else {
        anc_ext_tool_send_buf(CMD_FUNCTION_ALGO_STATE_GET, tool_hdl->report_state_buf, tool_hdl->report_state_len);
    }
    return 0;
}

/*
   算法状态更新函数
   规则：保证UI一致性，在APP层操作接口时更新，内部互斥暂不更新；
*/
int anc_ext_algorithm_state_update(enum ANC_EXT_ALGO func, u8 state, u8 info)
{
    u8 state_buf = state | (info << 4);

    if (!tool_hdl) {
        return 0;
    }

    //buf[0] func_a, buf[1] func_a state ...
    for (int i = 0; i < ANC_EXT_ALGO_EXIT; i++) {
        if (tool_hdl->report_state_buf[i << 1] == func) {
            tool_hdl->report_state_buf[(i << 1) + 1] = state_buf;
            break;
        }
    }

    //数据上报
    anc_ext_algorithm_state_report(1);
    return 0;
}

u8 anc_ext_algorithm_state_get(enum ANC_EXT_ALGO func)
{
    for (int i = 0; i < ANC_EXT_ALGO_EXIT; i++) {
        if (tool_hdl->report_state_buf[i << 1] == func) {
            return tool_hdl->report_state_buf[(i << 1) + 1];
        }
    }
    return 0;
}

#endif
