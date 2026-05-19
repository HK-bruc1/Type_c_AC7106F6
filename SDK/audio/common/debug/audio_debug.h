#ifndef _AUD_DEBUG_H_
#define _AUD_DEBUG_H_

#include "generic/typedef.h"

/*定时打印音频配置参数开关，默认3s打印一次
 *量产版本应该关闭，仅作debug调试使用
 */
#define TCFG_AUDIO_CONFIG_TRACE     		0	//跟踪使能配置
#define TCFG_AUDIO_CONFIG_TRACE_INTERVAL    3000//跟踪频率配置unit:ms

//<1> 配置信息跟踪
#define AUD_CFG_DUMP_ENABLE					1	//音频配置跟踪使能
#define AUD_REG_DUMP_ENABLE					0	//音频寄存器跟踪使能
#define AUD_CACHE_INFO_DUMP_ENABLE			0 	//cache信息跟踪使能
#define AUD_TASK_INFO_DUMP_ENABLE			0	//任务运行信息跟踪使能
#define AUD_JLSTREAM_MEM_DUMP_ENABLE		0	//jlstream内存跟踪
#define AUD_BT_INFO_DUMP_ENABLE				0	//蓝牙音频流跟踪
#define SYS_MEM_INFO_DUMP_ENABLE			0	//系统内存信息跟踪
#define AUD_MEM_INFO_DUMP_ENABLE			0	//音频模块内存申请信息跟踪
#define AUD_BT_DELAY_INFO_DUMP_ENABLE   	0   //蓝牙播放延时跟踪

//<2> debug使能
#define FLASH_INTERFERE_WITH_AUDIO_DEBUG	0
#define BT_INTERFERE_WITH_AUDIO_DEBUG		0

/*
**************************************************************
*                  Audio Config Trace Setup
* Description: 音频配置跟踪函数
* Arguments  : interval 跟踪间隔，单位ms
* Return	 : None.
* Note(s)    : None.
**************************************************************
*/
void audio_config_trace_setup(int interval);

void a2dp_delay_dump();
#endif
