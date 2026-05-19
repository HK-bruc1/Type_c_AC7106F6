#ifndef CVP_DEVELOP_NODE_H
#define CVP_DEVELOP_NODE_H

#include "audio_config.h"
#include "audio_cvp.h"
#include "effects/effects_adj.h"
#include "adc_file.h"

#define CVP_DEV_MAX_MIC_NUM	    4	//通话第三方算法支持最大MIC通道

/*
    判断是否为普通ADC方案
    VPU 类型边界位(ADC方案 bit 0~7; 其他方案 BIT 8~15)
*/
#define CVP_DEV_IS_VPU_ADC_MODE(ch)    ((ch) & ((1U << AUDIO_ADC_MAX_NUM) - 1))

#define CVP_DEV_TALK_MIC_CH         BIT(0)
#define CVP_DEV_FF_MIC_CH           BIT(1)
#define CVP_DEV_FB_MIC_CH           BIT(2)


//第三方通话算法配置
struct cvp_dev_cfg_t {
    u32 algo_type;      //算法预设
    u8 mic_num;         //MIC数量(非用户配置，自动生成)
    u8 mic_type;        //MIC方案选择
    u16 talk_mic;       //主MIC通道选择
    u16 talk_ff_mic;    //FFMIC通道选择
    u16 talk_fb_mic;    //FBMIC通道选择
    u8 vpu_en;          //VPU方案使能
    u16 vpu_ch_sel;     //VPU通道选择(ADC方案 bit 0~7; 其他方案 BIT 8~15);
} __attribute__((packed));

int cvp_dev_node_output_handle(s16 *data, u16 len);
int cvp_dev_param_cfg_read(void);
struct cvp_dev_cfg_t *cvp_dev_cfg_get(void);

#endif/*CVP_DEVELOP_NODE_H*/

