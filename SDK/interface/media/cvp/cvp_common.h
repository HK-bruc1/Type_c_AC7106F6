#ifndef _CVP_COMMON_H_
#define _CVP_COMMON_H_
#include "audio_cvp_def.h"

extern const int const_audio_cvp_debug_online_enable;

/*支持外部参考数据的流程，才需要判断output_way类型，否则(aec->attr->output_way == 0)为真*/
extern const int config_audio_cvp_ref_source;
extern const int CVP_V3_ALGO_ENABLEBIT;

typedef enum {
    CVP_TALK_MIC = 0,
    CVP_FF_MIC,
    CVP_FB_MIC,
} CVP_MIC_ENUM;

/*DMS输出选择*/
typedef enum {
    CVP_OUTPUT_SEL_DEFAULT = 0, /*默认输出：dms处理后的数据*/
    CVP_OUTPUT_SEL_TALK_MIC,    /*主mic原始数据*/
    CVP_OUTPUT_SEL_FF_MIC,      /*副mic原始数据*/
    CVP_OUTPUT_SEL_FB_MIC,      /*FB mic原始数据*/
    CVP_OUTPUT_SEL_VPU,         /*VPU 原始数据*/
} CVP_OUTPUT_ENUM;

/*
*********************************************************************
*                  			Audio CVP IOCTL
* Description: CVP功能配置
* Arguments  : cmd 		操作命令
*		       value 	操作数
*		       priv 	操作内存地址
* Return	 : 0 成功 其他 失败
* Note(s)    : (1)比如动态开关降噪NS模块:
*				aec_dms_ioctl(CVP_NS_SWITCH,1,NULL);	//降噪关
*				aec_dms_ioctl(CVP_NS_SWITCH,0,NULL);  //降噪开
*********************************************************************
*/
enum {
    CVP_AEC_SWITCH = 1,
    CVP_NLP_SWITCH,
    CVP_NS_SWITCH,
    CVP_AGC_SWITCH,
    CVP_ENC_SWITCH,
    CVP_AGC_MAX_LVL,
    CVP_ANS_NOISE_FLOOR,
    CVP_ANS_LOWCUTTHR,
    CVP_WNC_SWITCH,
    CVP_MFDT_SWITCH,
    CVP_OUTWAY_REF_IGNORE,
    CVP_DNS_NOISEGATE_VAL,
    CVP_DRC_SWITCH,
    CVP_SET_EQ,
    CVP_SET_ANC_STATEMODE,
    CVP_EQ_SWITCH,    /*三代算法关闭EQ 1->开启EQ 0->关闭EQ*/
    CVP_SET_MIC_NUM_ALGO,/*三代算法双麦切换单双麦 0->双麦 1->mic0 2->mic1*/
};
int aec_ioctl(int cmd, int value, void *priv);
int sms_tde_ioctl(int cmd, int value, void *priv);
int sms_vf_tde_ioctl(int cmd, int value, void *priv);
int aec_dms_ioctl(int cmd, int value, void *priv);
int aec_dms_flexible_ioctl(int cmd, int value, void *priv);
int aec_dms_hybrid_ioctl(int cmd, int value, void *priv);
int aec_dms_awn_ioctl(int cmd, int value, void *priv);
int aec_tms_ioctl(int cmd, int value, void *priv);
int cvp_ioctl(int cmd, int value, void *priv);


#endif /*_CVP_COMMON_H_*/

