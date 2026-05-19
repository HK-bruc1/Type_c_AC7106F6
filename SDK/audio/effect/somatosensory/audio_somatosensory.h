#ifndef _AUDIO_SOMATOSENSORY_H_
#define _AUDIO_SOMATOSENSORY_H_

#include "tech_lib/Somatosensory_api.h"
#include "tech_lib/sensor_common_parm.h"
#include "spatial_effect/spatial_effect_imu.h"
#include "spatial_effect/spatial_imu_trim.h"

#define SOMATOSENSORY_TASK_NAME     "HA_Detect" //Head Action Detect
#define SENSOR_BUF_LEN              128 //可根据线程运行情况，结合实际读取的数据长度自行调整

//预设头部姿态检测响应通话事件：点头接听、摇头挂断。也可加流程自定义事件功能。
#define SOMATOSENSORY_CALL_EVENT    1

enum {
    UNIDENTIFIED = 0,                   //未识别
    TURNING_HEAD = 1,                   //转头1次
    SHAKING_HEAD = 2,                   //摇头2次
    NODDING_HEAD = 3,                   //点头2次
};

struct somatosensory_hdl {
    struct six_axis_para axis_parm;     //校正后加速器、陀螺仪角度
    struct somato_control somato_parm;  //算法参数
    cbuffer_t cbuf;
    u8 *cbuf_data;                      //cbuf内存
    u8 *sensor_data;                    //读传感器数据
    u8 *run_data;                       //传进算法的buf
    void *work_buf;                     //算法buf
    void *sensor;                       //传感器句柄
    u8 task_state;
    u8 task_busy;
};

//使用全局句柄接口
void somatosensory_open();
void somatosensory_close();

//驱动接口
struct somatosensory_hdl *audio_somatosensory_open();
void audio_somatosensory_close(struct somatosensory_hdl *hdl);
void audio_somatosensory_bypass();

#endif

