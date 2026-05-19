#ifndef _AUDIO_GENERAL_CONFIG_H_
#define _AUDIO_GENERAL_CONFIG_H_

#include "generic/typedef.h"
#include "sdk_config.h"

#define CH_SEL_AUTO BIT(0)
#define CH_SEL_MIX  BIT(1)
/*
 * TWS不连接时，输出:
 * CH_SEL_AUTO: 根据当前耳机角色输出对应单声道数据
 * CH_SEL_MIX: 单声道LR混合数据
 */
#define TCFG_TWS_SIBLING_DISCONNECTED_CH_SEL        CH_SEL_MIX

/*
 * 强行设置解码器输出声道，非AUTO时，该配置会覆盖 TCFG_TWS_SIBLING_DISCONNECTED_CH_SEL
 * CH_SEL_AUTO: 自适应配置，根据TWS角色设置
 * AUDIO_CH_L: 解码器固定输出左声道
 * AUDIO_CH_R: 解码器固定输出右声道
 * AUDIO_CH_MIX: 解码器固定输出左右混合数据
 * AUDIO_CH_LR: 解码器固定输出立体声
 */
#define TCFG_DECODER_OUTPUT_CH_SEL                  CH_SEL_AUTO
//空间音效需要解码器输出真立体声
#if ((defined(TCFG_SPATIAL_ADV_NODE_ENABLE) && TCFG_SPATIAL_ADV_NODE_ENABLE) || \
    (defined(TCFG_SPATIAL_AUDIO_ENABLE) && TCFG_SPATIAL_AUDIO_ENABLE) || \
    (defined(TCFG_VIRTUAL_SURROUND_HP_NODE_ENABLE) && TCFG_VIRTUAL_SURROUND_HP_NODE_ENABLE) || \
    (defined(TCFG_LHDC_X_NODE_ENABLE) && TCFG_LHDC_X_NODE_ENABLE))
#undef TCFG_DECODER_OUTPUT_CH_SEL
#define TCFG_DECODER_OUTPUT_CH_SEL                  AUDIO_CH_LR
#endif

#endif
