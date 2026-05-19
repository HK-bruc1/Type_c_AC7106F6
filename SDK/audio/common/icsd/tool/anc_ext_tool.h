#ifndef _ANC_EXT_TOOL_H_
#define _ANC_EXT_TOOL_H_

#include "typedef.h"
#include "generic/list.h"
#include "app_config.h"
#include "anc_ext_tool_cfg.h"

//自适应类型
#define ANC_ADAPTIVE_TYPE_TWS            	1 //显示: TWS
#define ANC_ADAPTIVE_TYPE_HEADSET        	2 //显示：HEADSET

//自适应训练模式
#define ANC_TRAIN_MODE_TONE_PNC          	1 //显示: TONE_PNC
#define ANC_TRAIN_MODE_TONE_BYPASS_PNC		2 //显示：TONE_BYPASS_PNC

//ANC_EXT功能列表(u16): SPP打印控制/获取本地代码开关
#define ANC_EXT_FUNC_EN_ADAPTIVE			BIT(0)	//自适应功能
// #define ANC_EXT_FUNC_EN_RTANC				BIT(1)	//RTANC(旧)
#define ANC_EXT_FUNC_EN_ADAPTIVE_CMP		BIT(2)	//自适应CMP
#define ANC_EXT_FUNC_EN_ADAPTIVE_EQ			BIT(3)	//自适应EQ
#define ANC_EXT_FUNC_EN_WIND_DET			BIT(4)	//风噪检测
#define ANC_EXT_FUNC_EN_SOFT_HOWL_DET		BIT(5)	//软件啸叫检测
#define ANC_EXT_FUNC_EN_RTANC				BIT(6)	//RTANC
#define ANC_EXT_FUNC_EN_DYNAMIC				BIT(7)	//DYNAMIC

#define ANC_EXT_FUNC_SPP_EN_APP				BIT(0 + 16) //APP SPP控制

//ANC_EXT 列表(u8)：状态获取，使能控制
enum ANC_EXT_ALGO {
    ANC_EXT_ALGO_EAR_ADAPTIVE = 1,
    ANC_EXT_ALGO_ADAPTIVE_CMP,
    ANC_EXT_ALGO_ADAPTIVE_EQ,
    ANC_EXT_ALGO_WIND_DET,
    ANC_EXT_ALGO_SOFT_HOWL_DET,
    ANC_EXT_ALGO_RTANC,
    ANC_EXT_ALGO_DYNAMIC,
    ANC_EXT_ALGO_EXIT,
};

enum ANC_EXT_ALGO_STATE {
    ANC_EXT_ALGO_STA_CLOSE = 0,
    ANC_EXT_ALGO_STA_OPEN,
    ANC_EXT_ALGO_STA_SUSPEND,
};

//文件 SUBFILE ID
enum {
    /* FILE_ID_ANC_EXT_START = 0XB0, */
    //耳道自适应 配置
    FILE_ID_ANC_EXT_EAR_ADAPTIVE_BASE = 0XB0,		//BASE 界面参数 文件ID
    FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_IIR = 0XB1,		//FF->滤波器设置文件ID
    FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT = 0XB2,	//FF->权重和性能设置文件ID
    FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_TARGET = 0XB3,	//FF->TARGET和补偿值设置文件ID

    FILE_ID_ANC_EXT_BIN = 0xB4,						//anc_ext.bin 总文件ID

    FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_RECORDER = 0xB5,//FF->耳道记忆 文件ID
    FILE_ID_ANC_EXT_EAR_ADAPTIVE_TIME_DOMAIN = 0xB6,//时域文件ID 文件ID

    FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP = 0xB7,	//产测：耳道自适应产测补偿 文件ID

    //RTANC 配置
    // FILE_ID_ANC_EXT_RTANC_ADAPTIVE_CFG = 0xB8,		//RTANC adaptive 配置文件ID(旧)
    FILE_ID_ANC_EXT_RTANC_DEBUG_DATA = 0xB9,		//debug: RTANC adaptive debug 数据 文件ID

    //CMP 配置
    FILE_ID_ANC_EXT_ADAPTIVE_CMP_DATA = 0xBA,		//自适应CMP配置 文件ID

    //AEQ 配置
    FILE_ID_ANC_EXT_ADAPTIVE_EQ_DATA = 0xBB,		//自适应EQ配置 文件ID
    FILE_ID_ANC_EXT_REF_SZ_DATA = 0xBC,				//参数SZ 文件ID

    FILE_ID_ANC_EXT_ADAPTIVE_EQ_DEBUG_DATA = 0xBD,	//debug: EQ adaptive debug 数据 文件ID

    //DYNAMIC 配置
    FILE_ID_ANC_EXT_DYNAMIC_CFG = 0xBE,				//DYNAMIC 配置文件ID

    //RTANC 配置
    FILE_ID_ANC_EXT_RTANC_ADAPTIVE_CFG = 0xBF,		//RTANC adaptive 配置文件ID

    // FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_PZ_SZ_CMP = 0xBE,      //ANC自适应产测补偿 文件ID    
    // FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_GOLD_DATA = 0xBF,		//ANC自适应金机数据 文件ID
    // FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_SZ_DATA = 0xC0,		//产测SZ小机数据 文件ID
    // FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_PZ_DATA = 0xC1,		//产测PZ小机数据 文件ID
    // FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_BYPASS_DATA = 0xC2,	//产测BYPASS小机数据 文件ID
    // FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_SZ_BYPASS_DATA = 0xC3,	//产测SZ_BYPASS小机数据 文件ID

    FILE_ID_ANC_EXT_WIND_DET_CFG = 0xC4,			//风噪检测配置 文件ID
    FILE_ID_ANC_EXT_SOFT_HOWL_DET_CFG = 0xC5,		//软件-啸叫检测配置 文件ID

    /* FILE_ID_ANC_EXT_STOP = 0XD0, */
};

//anc_ext.bin数据类型 ID
enum {
    //BASE 界面参数 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_BASE
    ANC_EXT_EAR_ADAPTIVE_BASE_CFG_ID = 0x1,				//base 界面下的参数

    //======================左声道参数ID========================
    //FF->滤波器设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_IIR_FF
    ANC_EXT_EAR_ADAPTIVE_FF_GENERAL_ID = 0x11,			//通用设置参数

    ANC_EXT_EAR_ADAPTIVE_FF_HIGH_GAINS_ID = 0x12,		//高->gain相关参数配置
    ANC_EXT_EAR_ADAPTIVE_FF_HIGH_IIR_ID = 0x13,			//高->滤波器及滤波器限制配置

    ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_GAINS_ID = 0x14,		//中->gain相关参数配置
    ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_IIR_ID = 0x15,		//中->滤波器及滤波器限制配置

    ANC_EXT_EAR_ADAPTIVE_FF_LOW_GAINS_ID = 0x16,		//低->gain相关参数配置
    ANC_EXT_EAR_ADAPTIVE_FF_LOW_IIR_ID = 0x17,			//低->滤波器及滤波器限制配置

    //FF->权重和性能设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT
    ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT_PARAM_ID = 0x18,		//权重通用参数配置

    ANC_EXT_EAR_ADAPTIVE_FF_HIGH_WEIGHT_ID = 0x19,		//高->权重配置
    ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_WEIGHT_ID = 0x1A,	//中->权重配置
    ANC_EXT_EAR_ADAPTIVE_FF_LOW_WEIGHT_ID = 0x1B,		//低->权重配置

    ANC_EXT_EAR_ADAPTIVE_FF_HIGH_MSE_ID = 0x1C,			//高->性能配置
    ANC_EXT_EAR_ADAPTIVE_FF_MEDIUM_MSE_ID = 0x1D,		//中->性能配置
    ANC_EXT_EAR_ADAPTIVE_FF_LOW_MSE_ID = 0x1E,			//低->性能配置

    //FF->TARGE挡位设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_TARGET
    ANC_EXT_EAR_ADAPTIVE_FF_TARGET_PARAM_ID = 0x1F,		//TARGET相关参数配置
    ANC_EXT_EAR_ADAPTIVE_FF_TARGET_SV_ID = 0x20,		//TARGET配置
    ANC_EXT_EAR_ADAPTIVE_FF_TARGET_CMP_ID = 0x21,		//TARGET 补偿值配置

    //FF->耳道记忆设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_RECORDER
    ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PARAM_ID = 0x22,	//耳道记忆相关参数配置
    ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PZ_ID = 0x23,			//耳道记忆PZ配置
    ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_SZ_ID = 0x3D,			//耳道记忆SZ配置

    //时域文件ID 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_TIME_DOMAIN
    // ANC_EXT_TIME_DOMAIN_CH_TONE_SZ_L = 0x24,		//tone_sz_L
    // ANC_EXT_TIME_DOMAIN_CH_BYPASS_SZ_L = 0x25,		//bypass_sz_L
    // ANC_EXT_TIME_DOMAIN_CH_PNC_L = 0x26,			//pnc_sz_L
    // ANC_EXT_TIME_DOMAIN_CH_TONE_SZ_R = 0x27,		//tone_sz_R
    // ANC_EXT_TIME_DOMAIN_CH_BYPASS_SZ_R = 0x28,		//bypass_sz_R
    // ANC_EXT_TIME_DOMAIN_CH_PNC_R = 0x29,			//pnc_sz_R

    //======================右声道参数ID========================
    //FF->滤波器设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_IIR_FF
    ANC_EXT_EAR_ADAPTIVE_FF_R_GENERAL_ID = 0x2A,		//通用设置参数(右)

    ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_GAINS_ID = 0x2B,		//高->gain相关参数配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_IIR_ID = 0x2C,		//高->滤波器及滤波器限制配置(右)

    ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_GAINS_ID = 0x2D,	//中->gain相关参数配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_IIR_ID = 0x2E,		//中->滤波器及滤波器限制配置(右)

    ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_GAINS_ID = 0x2F,		//低->gain相关参数配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_IIR_ID = 0x30,		//低->滤波器及滤波器限制配置(右)

    //FF->权重和性能设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_WEIGHT
    ANC_EXT_EAR_ADAPTIVE_FF_R_WEIGHT_PARAM_ID = 0x31,	//权重通用参数配置(右)

    ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_WEIGHT_ID = 0x32,	//高->权重配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_WEIGHT_ID = 0x33,	//中->权重配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_WEIGHT_ID = 0x34,		//低->权重配置(右)

    ANC_EXT_EAR_ADAPTIVE_FF_R_HIGH_MSE_ID = 0x35,		//高->性能配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_MEDIUM_MSE_ID = 0x36,		//中->性能配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_LOW_MSE_ID = 0x37,		//低->性能配置(右)

    //FF->TARGE挡位设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_TARGET
    ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_PARAM_ID = 0x38,	//TARGET相关参数配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_SV_ID = 0x39,		//TARGET配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_TARGET_CMP_ID = 0x3A,		//TARGET 补偿值配置(右)

    //FF->耳道记忆设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_RECORDER
    ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PARAM_ID = 0x3B,	//耳道记忆相关参数配置(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PZ_ID = 0x3C,	//耳道记忆PZ配置(右)
    // ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_SZ_ID = 0x3D,	//↑位置上移
    ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_SZ_ID = 0x3E,	//耳道记忆SZ配置(右)

    //FF->耳道自适应产测数据
    ANC_EXT_EAR_ADAPTIVE_FF_DUT_PZ_CMP_ID = 0x3F,		//产测PZ补偿
    ANC_EXT_EAR_ADAPTIVE_FF_DUT_SZ_CMP_ID = 0x40,		//产测SZ补偿
    ANC_EXT_EAR_ADAPTIVE_FF_R_DUT_PZ_CMP_ID = 0x41,		//产测PZ补偿(右)
    ANC_EXT_EAR_ADAPTIVE_FF_R_DUT_SZ_CMP_ID = 0x42,		//产测SZ补偿(右)

    // ANC_EXT_RTANC_ADAPTIVE_CFG_ID = 0x43,				//RTANC adptive 配置(旧)

    //CMP 配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_CMP_DATA
    ANC_EXT_ADAPTIVE_CMP_GAINS_ID = 0x44,				//CMP gain相关参数配置
    ANC_EXT_ADAPTIVE_CMP_IIR_ID = 0x45,					//CMP 滤波器
    ANC_EXT_ADAPTIVE_CMP_WEIGHT_ID = 0x46,				//CMP 权重
    ANC_EXT_ADAPTIVE_CMP_MSE_ID = 0x47,					//CMP 性能

    ANC_EXT_ADAPTIVE_CMP_R_GAINS_ID = 0x48,				//CMP gain相关参数配置(右)
    ANC_EXT_ADAPTIVE_CMP_R_IIR_ID = 0x49,				//CMP 滤波器(右)
    ANC_EXT_ADAPTIVE_CMP_R_WEIGHT_ID = 0x4A,			//CMP 权重(右)
    ANC_EXT_ADAPTIVE_CMP_R_MSE_ID = 0x4B,				//CMP 性能(右)

    //AEQ 配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_EQ_DATA
    ANC_EXT_ADAPTIVE_EQ_GAINS_ID = 0x4C,				//AEQ gain相关参数配置
    ANC_EXT_ADAPTIVE_EQ_IIR_ID = 0x4D,					//AEQ 滤波器
    ANC_EXT_ADAPTIVE_EQ_WEIGHT_ID = 0x4E,				//AEQ 权重
    ANC_EXT_ADAPTIVE_EQ_MSE_ID = 0x4F,					//AEQ 性能
    ANC_EXT_ADAPTIVE_EQ_THR_ID = 0x50,					//AEQ 上限阈值

    ANC_EXT_ADAPTIVE_EQ_R_GAINS_ID = 0x51,				//AEQ gain相关参数配置(右)
    ANC_EXT_ADAPTIVE_EQ_R_IIR_ID = 0x52,				//AEQ 滤波器(右)
    ANC_EXT_ADAPTIVE_EQ_R_WEIGHT_ID = 0x53,				//AEQ 权重(右)
    ANC_EXT_ADAPTIVE_EQ_R_MSE_ID = 0x54,				//AEQ 性能(右)
    ANC_EXT_ADAPTIVE_EQ_R_THR_ID = 0x55,				//AEQ 上限阈值(右)

    //参考 SZ 数据 文件ID FILE_ID_ANC_EXT_REF_SZ_DATA
    ANC_EXT_REF_SZ_DATA_ID = 0x56,						//REF_SZ 参考数据
    ANC_EXT_REF_SZ_R_DATA_ID = 0x57,					//REF_SZ 参考数据(右)

    // ANC_EXT_RTANC_R_ADAPTIVE_CFG_ID = 0x58,				//RTANC adptive 配置(右)(旧)

    //DYNAMIC 配置 文件ID FILE_ID_ANC_EXT_DYNAMIC_CFG
    ANC_EXT_DYNAMIC_CFG_ID = 0x59,						//DYNAMIC 配置
    ANC_EXT_DYNAMIC_R_CFG_ID = 0x5A,					//DYNAMIC 配置(右)

    //RTANC adaptive 配置 文件ID FILE_ID_ANC_EXT_RTANC_ADAPTIVE_CFG
    ANC_EXT_RTANC_ADAPTIVE_CFG_ID = 0x5B,				//RTANC adptive 配置
    ANC_EXT_RTANC_R_ADAPTIVE_CFG_ID = 0x5C,				//RTANC adptive 配置(右)

    //产测：耳道自适应金机/小机 PZ/SZ 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_DUT_CMP
    // ANC_EXT_EAR_ADAPTIVE_FF_DUT_GOLD_PZ_ID = 0x59,      //产测PZ金机数据
    // ANC_EXT_EAR_ADAPTIVE_FF_DUT_GOLD_SZ_ID = 0x5A,      //产测SZ金机数据
    // ANC_EXT_EAR_ADAPTIVE_FF_DUT_GOLD_SZ_BP_ID = 0x5B,   //产测SZ_bp金机数据

    // ANC_EXT_EAR_ADAPTIVE_FF_DUT_LOCAL_SZ_ID = 0x5C,     //产测SZ小机数据
    // ANC_EXT_EAR_ADAPTIVE_FF_DUT_LOCAL_PZ_ID = 0x5D,   //产测PZ小机数据
    // ANC_EXT_EAR_ADAPTIVE_FF_DUT_LOCAL_BYPASS_ID = 0x5E,  //产测BYPASS小机数据
    // ANC_EXT_EAR_ADAPTIVE_FF_DUT_LOCAL_SZ_BYPASS_ID = 0x5F,  //产测SZ_BYPASS小机数据

    //风噪检测 配置 文件ID FILE_ID_ANC_EXT_WIND_DET_CFG
    ANC_EXT_WIND_DET_CFG_ID = 0x60,						//风噪检测配置

    //软件啸叫检测 配置 文件ID FILE_ID_ANC_EXT_SOFT_HOWL_DET_CFG
    ANC_EXT_SOFT_HOWL_DET_CFG_ID = 0x61,				//软件啸叫检测配置

    //风噪检测 配置 文件ID FILE_ID_ANC_EXT_WIND_DET_CFG
    ANC_EXT_WIND_TRIGGER_CFG_ID = 0x62,				//风噪触发配置

    //FF->耳道记忆设置界面 文件ID FILE_ID_ANC_EXT_EAR_ADAPTIVE_FF_RECORDER
    ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_PZ_COEFF_ID = 0x63,			//耳道记忆PZ_COEFF配置(RTANC 专属)
    ANC_EXT_EAR_ADAPTIVE_FF_RECORDER_SZ_COEFF_ID = 0x64,			//耳道记忆SZ_COEFF配置(RTANC 专属)

    ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_PZ_COEFF_ID = 0x65,			//耳道记忆PZ_COEFF配置(右)(RTANC 专属)
    ANC_EXT_EAR_ADAPTIVE_FF_R_RECORDER_SZ_COEFF_ID = 0x66,			//耳道记忆SZ_COEFF配置(右)(RTANC 专属)

    //CMP 配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_CMP_DATA
    ANC_EXT_ADAPTIVE_CMP_RECORDER_IIR_ID = 0x67,					//CMP 滤波器存线配置(RTANC 专属)
    ANC_EXT_ADAPTIVE_CMP_R_RECORDER_IIR_ID = 0x68,					//CMP 滤波器存线配置(右)(RTANC 专属)

    //AEQ 配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_EQ_DATA
    ANC_EXT_ADAPTIVE_EQ_RECORDER_IIR_ID = 0x69,						//AEQ 滤波器存线配置(RTANC 专属)
    ANC_EXT_ADAPTIVE_EQ_R_RECORDER_IIR_ID = 0x6A,					//AEQ 滤波器存线配置(右)(RTANC 专属)

    //CMP 配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_CMP_DATA
    ANC_EXT_ADAPTIVE_CMP_SZ_FACTOR_ID = 0x6B,					//CMP SZ补偿(RTANC 专属)(双FB方案)
    ANC_EXT_ADAPTIVE_CMP_R_SZ_FACTOR_ID = 0x6C,					//CMP SZ补偿(右)(RTANC 专属)(双FB方案)

    //自适应DCC配置 文件ID FILE_ID_ANC_EXT_ADAPTIVE_DCC_CFG
    // ANC_EXT_ADAPTIVE_DCC_CFG_ID = 0x63,				//自适应DCC配置
};

//工具debug bin文件 数据ID
enum ANC_EXT_DEBUG_DATA_ID {

    //--------滤波器ID--------
    //0x0 - 0x3 FF/FB/CMP/TRANS
    AEQ_L_IIR_VOL_LOW = 0x04,
    AEQ_L_IIR_VOL_MIDDLE = 0x05,
    AEQ_L_IIR_VOL_HIGH = 0x06,

    //0x10 - 0x13 FF/FB/CMP/TRANS
    AEQ_R_IIR_VOL_LOW = 0x14,
    AEQ_R_IIR_VOL_MIDDLE = 0x15,
    AEQ_R_IIR_VOL_HIGH = 0x16,

    //------频响/曲线ID-------
    ANC_L_ADAP_FRE = 0x20,
    ANC_L_ADAP_PZ = 0x21,
    ANC_L_ADAP_SZPZ = 0x22,
    ANC_L_ADAP_TARGET = 0x23,
    ANC_L_ADAP_TARGET_CMP = 0x25,
    ANC_L_ADAP_TARGET_BEFORE_CMP = 0x26,
    ANC_L_ADAP_CMP_FORM_TRAIN = 0x27,
    ANC_L_ADAP_FRE_2 = 0x28,
    ANC_L_ADAP_MSE = 0x29,
    ANC_L_ADAP_SZ_CMP = 0x2A,

    //----------- R ------------
    ANC_R_ADAP_FRE = 0x30,
    ANC_R_ADAP_PZ = 0x31,
    ANC_R_ADAP_SZPZ = 0x32,
    ANC_R_ADAP_TARGET = 0x33,
    ANC_R_ADAP_TARGET_CMP = 0x35,
    ANC_R_ADAP_TARGET_BEFORE_CMP = 0x36,
    ANC_R_ADAP_CMP_FORM_TRAIN = 0x37,
    ANC_R_ADAP_FRE_2 = 0x38,
    ANC_R_ADAP_MSE = 0x39,
    ANC_R_ADAP_SZ_CMP = 0x3A,
};

//自适应训练模式
enum ANC_EAR_ADPTIVE_MODE {
    EAR_ADAPTIVE_MODE_NORAML = 0,	//正常模式
    EAR_ADAPTIVE_MODE_TEST,			//工具测试模式
    EAR_ADAPTIVE_MODE_SIGN_TRIM,	//工具自适应符号校准模式
};

//自适应失败 滤波器来源
enum ANC_EAR_ADAPTIVE_RESULT_MODE {
    EAR_ADAPTIVE_FAIL_RESULT_FROM_ADAPTIVE = 0,	//由自适应校准
    EAR_ADAPTIVE_FAIL_RESULT_FROM_NORMAL,		//来源默认滤波器
};

// 工具协议来源
enum ANC_EXT_UART_SEL {
    ANC_EXT_UART_SEL_BOX = 0,
    ANC_EXT_UART_SEL_SPP,
};

// 自适应测试模式
enum {
    ANC_ADPATIVE_DUT_MODE_NORAML = 0,    //标准
    ANC_ADPATIVE_DUT_MODE_PRODUCTION,    //产测
    ANC_ADPATIVE_DUT_MODE_SLIGHT,        //轻度降噪
};

struct anc_ext_alloc_bulk_t {
    struct list_head entry;
    u32 alloc_id;			//申请ID
    u32 alloc_len;			//临时内存大小
    u8 *alloc_addr;			//临时内存地址
};

//subfile id head
struct anc_ext_id_head {
    u32 id;
    u32 offset;
    u32 len;
};

//subfile head
struct anc_ext_subfile_head {
    u32 file_id;
    u32 file_len;
    u16 version;
    u16 id_cnt;
    u8 data[0];
};

struct __anc_ext_subfile_id_table {
    u8 pointer_type;	//是否为指针类型
    int id;				//参数对应的ID
    u32 offset;			//目标存储地址相对偏移量
};

//耳道自适应工具参数
struct anc_ext_ear_adaptive_param {

    enum ANC_EAR_ADPTIVE_MODE train_mode;	//自适应训练模式
    u8 time_domain_show_en;					//时域debug使能
    u8 dut_cmp_en;							//产测补偿使能
    u8 dut_mode;							//产测模式
    u8 *time_domain_buf;					//时域debug buff
    int time_domain_len;					//时域debug buff len

#if TCFG_AUDIO_ANC_EAR_ADAPTIVE_EN
    struct __anc_ext_ear_adaptive_base_cfg	*base_cfg;

    /*----------------头戴式 左声道 / TWS LR------------------*/
    struct __anc_ext_ear_adaptive_iir_general *ff_iir_general;
    struct __anc_ext_ear_adaptive_iir_gains	*ff_iir_high_gains;
    struct __anc_ext_ear_adaptive_iir_gains	*ff_iir_medium_gains;
    struct __anc_ext_ear_adaptive_iir_gains	*ff_iir_low_gains;
    struct __anc_ext_ear_adaptive_iir *ff_iir_high;//[10];
    struct __anc_ext_ear_adaptive_iir *ff_iir_medium;//[10];
    struct __anc_ext_ear_adaptive_iir *ff_iir_low;//[10];

    struct __anc_ext_ear_adaptive_weight_param *ff_weight_param;
    struct __anc_ext_ear_adaptive_weight *ff_weight_high;
    struct __anc_ext_ear_adaptive_weight *ff_weight_medium;
    struct __anc_ext_ear_adaptive_weight *ff_weight_low;
    struct __anc_ext_ear_adaptive_mse *ff_mse_high;
    struct __anc_ext_ear_adaptive_mse *ff_mse_medium;
    struct __anc_ext_ear_adaptive_mse *ff_mse_low;

    struct __anc_ext_ear_adaptive_target_param *ff_target_param;
    struct __anc_ext_ear_adaptive_target_sv *ff_target_sv;
    struct __anc_ext_ear_adaptive_target_cmp *ff_target_cmp;

    struct __anc_ext_ear_adaptive_mem_param *ff_ear_mem_param;
    struct __anc_ext_ear_adaptive_mem_data *ff_ear_mem_pz;
    struct __anc_ext_ear_adaptive_mem_data *ff_ear_mem_sz;
    struct __anc_ext_ear_adaptive_mem_data *ff_ear_mem_pz_coeff;
    struct __anc_ext_ear_adaptive_mem_data *ff_ear_mem_sz_coeff;


    /*--------------------头戴式 右声道------------------------*/
    struct __anc_ext_ear_adaptive_iir_general *rff_iir_general;
    struct __anc_ext_ear_adaptive_iir_gains	*rff_iir_high_gains;
    struct __anc_ext_ear_adaptive_iir_gains	*rff_iir_medium_gains;
    struct __anc_ext_ear_adaptive_iir_gains	*rff_iir_low_gains;
    struct __anc_ext_ear_adaptive_iir *rff_iir_high;//[10];
    struct __anc_ext_ear_adaptive_iir *rff_iir_medium;//[10];
    struct __anc_ext_ear_adaptive_iir *rff_iir_low;//[10];

    struct __anc_ext_ear_adaptive_weight_param *rff_weight_param;
    struct __anc_ext_ear_adaptive_weight *rff_weight_high;
    struct __anc_ext_ear_adaptive_weight *rff_weight_medium;
    struct __anc_ext_ear_adaptive_weight *rff_weight_low;
    struct __anc_ext_ear_adaptive_mse *rff_mse_high;
    struct __anc_ext_ear_adaptive_mse *rff_mse_medium;
    struct __anc_ext_ear_adaptive_mse *rff_mse_low;

    struct __anc_ext_ear_adaptive_target_param *rff_target_param;
    struct __anc_ext_ear_adaptive_target_sv *rff_target_sv;
    struct __anc_ext_ear_adaptive_target_cmp *rff_target_cmp;

    struct __anc_ext_ear_adaptive_mem_param *rff_ear_mem_param;
    struct __anc_ext_ear_adaptive_mem_data *rff_ear_mem_pz;
    struct __anc_ext_ear_adaptive_mem_data *rff_ear_mem_sz;
    struct __anc_ext_ear_adaptive_mem_data *rff_ear_mem_pz_coeff;
    struct __anc_ext_ear_adaptive_mem_data *rff_ear_mem_sz_coeff;

    //FF->耳道自适应产测数据
    struct __anc_ext_ear_adaptive_dut_data *ff_dut_pz_cmp;
    struct __anc_ext_ear_adaptive_dut_data *ff_dut_sz_cmp;
    struct __anc_ext_ear_adaptive_dut_data *rff_dut_pz_cmp;
    struct __anc_ext_ear_adaptive_dut_data *rff_dut_sz_cmp;
#endif

#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    //RTANC 配置
    struct __anc_ext_rtanc_adaptive_cfg *rtanc_adaptive_cfg;
    struct __anc_ext_rtanc_adaptive_cfg *r_rtanc_adaptive_cfg;
    struct __anc_ext_dynamic_cfg *dynamic_cfg;
    struct __anc_ext_dynamic_cfg *r_dynamic_cfg;
#endif

#if TCFG_AUDIO_ANC_ADAPTIVE_CMP_EN
    //RTCMP 配置
    struct __anc_ext_ear_adaptive_iir_gains	*cmp_gains;
    struct __anc_ext_ear_adaptive_iir *cmp_iir; //[6]
    struct __anc_ext_ear_adaptive_weight *cmp_weight;	//[60]
    struct __anc_ext_ear_adaptive_mse *cmp_mse;	//[60]
    struct __anc_ext_adaptive_mem_iir *cmp_mem_iir;
    struct __anc_ext_sz_factor_data	*cmp_sz_factor;
    struct __anc_ext_ear_adaptive_iir_gains	*rcmp_gains;
    struct __anc_ext_ear_adaptive_iir *rcmp_iir; //[6]
    struct __anc_ext_ear_adaptive_weight *rcmp_weight;	//[60]
    struct __anc_ext_ear_adaptive_mse *rcmp_mse;	//[60]
    struct __anc_ext_adaptive_mem_iir *rcmp_mem_iir;
    struct __anc_ext_sz_factor_data	*rcmp_sz_factor;
#endif

#if TCFG_AUDIO_ADAPTIVE_EQ_ENABLE
    //RTAEQ
    struct __anc_ext_ear_adaptive_iir_gains	*aeq_gains;
    struct __anc_ext_ear_adaptive_iir *aeq_iir; //[10]
    struct __anc_ext_ear_adaptive_weight *aeq_weight;	//[60]
    struct __anc_ext_ear_adaptive_mse *aeq_mse;	//[60]
    struct __anc_ext_adaptive_eq_thr *aeq_thr;
    struct __anc_ext_adaptive_mem_iir *aeq_mem_iir;
    struct __anc_ext_ear_adaptive_iir_gains	*raeq_gains;
    struct __anc_ext_ear_adaptive_iir *raeq_iir; //[10]
    struct __anc_ext_ear_adaptive_weight *raeq_weight;	//[60]
    struct __anc_ext_ear_adaptive_mse *raeq_mse;	//[60]
    struct __anc_ext_adaptive_eq_thr *raeq_thr;
    struct __anc_ext_adaptive_mem_iir *raeq_mem_iir;
#endif

    //参考SZ
    struct __anc_ext_sz_data *sz_ref; //[120]
    struct __anc_ext_sz_data *rsz_ref; //[120]

#if TCFG_AUDIO_ANC_WIND_NOISE_DET_ENABLE
    //风噪检测配置
    struct __anc_ext_wind_det_cfg *wind_det_cfg;
    struct __anc_ext_wind_trigger_cfg *wind_trigger_cfg;
#endif

    //啸叫检测配置
    struct __anc_ext_soft_howl_det_cfg *soft_howl_det_cfg;

};


void anc_ext_tool_init(void);

//自适应结束工具回调函数
void anc_ext_tool_ear_adaptive_end_cb(u8 result);

//耳道自适应-符号校准 执行结果回调函数
void anc_ext_tool_ear_adaptive_sign_trim_end_cb(u8 *buf, int len);

int anc_ext_rsfile_read(void);

u8 *anc_ext_rsfile_get(u32 *file_len);

//subfile数据文件拼接
struct anc_ext_subfile_head *anc_ext_subfile_catch_init(u32 file_id);
struct anc_ext_subfile_head *anc_ext_subfile_catch(struct anc_ext_subfile_head *head, u8 *buf, u32 len, u32 id);
// 获取工具是否连接
u8 anc_ext_tool_online_get(void);

//ANC_EXT 工具断开连接
void anc_ext_tool_disconnect(void);

//获取耳道自适应参数
struct anc_ext_ear_adaptive_param *anc_ext_ear_adaptive_cfg_get(void);

//获取耳道自适应训练模式
enum ANC_EAR_ADPTIVE_MODE anc_ext_ear_adaptive_train_mode_get(void);

//获取耳道自适应-结果来源
u8 anc_ext_ear_adaptive_result_from(void);

//获取耳道自适应SZ校准符号
s8 anc_ext_ear_adaptive_sz_calr_sign_get(void);

// 检查是否有自适应参数, 0 正常/ 1 有参数为空
u8 anc_ext_ear_adaptive_param_check(void);

/*
   ANC_EXT SUBFILE文件解析遍历
	param: 	file_id 文件ID
		   	data ：目标数据地址
			len : 目标数据长度
			alloc_flag : 是否需要申请缓存空间（flash 文件解析传0，工具调试传1）
*/
int anc_ext_subfile_analysis_each(u32 file_id, u8 *data, int len, u8 alloc_flag);

//获取RTANC 工具参数
struct __anc_ext_rtanc_adaptive_cfg *anc_ext_rtanc_adaptive_cfg_get(u8 ch);

//获取DYNAMIC 工具参数
struct __anc_ext_dynamic_cfg *anc_ext_dynamic_cfg_get(u8 ch);

u8 anc_ext_rtanc_param_check(void);

//获取算法工具SPP使能
int anc_ext_debug_tool_function_get(void);

//设置耳道自适应测试模式
void anc_ext_ear_adaptive_dut_mode_set(u8 dut_mode);

int anc_ext_algorithm_state_update(enum ANC_EXT_ALGO func, u8 state, u8 info);

u8 anc_ext_algorithm_state_get(enum ANC_EXT_ALGO func);

//工具-自适应CMP使能设置
int anc_ext_adaptive_cmp_tool_en_set(u8 en);

//工具-自适应CMP使能获取
int anc_ext_adaptive_cmp_tool_en_get(void);

//工具-自适应EQ使能设置
int anc_ext_adaptive_eq_tool_en_set(u8 en);

//工具-自适应EQ使能获取
int anc_ext_adaptive_eq_tool_en_get(void);

/*-------------------ANCTOOL交互接口---------------------*/
//事件处理
void anc_ext_tool_cmd_deal(u8 *data, u16 len, enum ANC_EXT_UART_SEL uart_sel);

//写文件结束
int anc_ext_tool_write_end(u32 file_id, u8 *data, int len, u8 alloc_flag);

//读文件开始
int anc_ext_tool_read_file_start(u32 file_id, u8 **data, u32 *len);

//读文件结束
int anc_ext_tool_read_file_end(u32 file_id);

/*-------------------ANCTOOL交互接口END---------------------*/

#endif/*_ANC_EXT_TOOL_H_*/

