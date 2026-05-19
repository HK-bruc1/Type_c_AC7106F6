
#ifndef _AUDIO_ANC_DEBUG_TOOL_H_
#define _AUDIO_ANC_DEBUG_TOOL_H_

#include "app_anctool.h"

#define AUDIO_ANC_DEBUG_CMD_RTANC_EN		0		//RTANC相关验证、测试命令debug使能

enum {
    ANC_DEBUG_STA_CLOSE = 0,
    ANC_DEBUG_STA_OPEN,
    ANC_DEBUG_STA_RUN,
};

//SPP-命令
enum {
    ANC_DEBUG_CMD_ICSD = 0,		//算法指令
    ANC_DEBUG_CMD_APP,			//应用指令
    ANC_DEBUG_CMD_PASS,			//透传指令
    ANC_DEBUG_CMD_ERR,			//错误上报

    ANC_DEBUG_ACTIVE_REPORT = 0xF0, //主动上报
};

//SPP-APP/ERR 子命令
enum {
    ANC_DEBUG_APP_ADAPTIVE_EQ = 0,
    ANC_DEBUG_APP_RTANC,
    ANC_DEBUG_APP_EAR_ADAPTIVE,
    ANC_DEBUG_APP_ADAPTIVE_CMP,
    ANC_DEBUG_APP_WIND_DET,
    ANC_DEBUG_APP_SOFT_HOWL_DET,
    ANC_DEBUG_APP_DYNAMIC,
    ANC_DEBUG_APP_OTHER_MSG,
};

//debug 启动
void audio_anc_debug_tool_open(void);

//debug 关闭
void audio_anc_debug_tool_close(void);

//debug spp打印开关
void audio_anc_debug_spp_log_en(u8 en);

//debug spp打印挂起
void audio_anc_debug_spp_suspend(u8 en);

/*
   debug 数据发送
   return len表示写入成功，0则写入失败
*/
int audio_anc_debug_send_data(u8 *buf, int len);

//APP层数据写入
void audio_anc_debug_app_send_data(u8 cmd, u8 cmd_2nd, u8 *buf, int len);

/*
   小机状态主动上报
 */
void audio_anc_debug_report_send_data(u8 cmd, u8 *buf, int len);

/*
   错误状态上报
 */
void audio_anc_debug_err_send_data(u8 cmd, void *buf, int len);

u8 audio_anc_debug_busy_get(void);

//自定义命令处理函数
int audio_anc_debug_user_cmd_process(u8 *data, int len);

#endif/*_AUDIO_ANC_DEBUG_TOOL_H_*/
