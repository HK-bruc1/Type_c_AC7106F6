#ifndef __EAR_SPORTS_DATA_OPT_H__
#define __EAR_SPORTS_DATA_OPT_H__

#include "typedef.h"
#include "app_config.h"

#define TCFG_HR_CHECK_ENABLE    1    // 心率功能的宏
#define TCFG_SPO2_CHECK_ENABLE  1    // 血氧功能的宏
#define TCFG_STEP_COUNT_ENABLE  1    // 计步功能的宏

// 运动健康数据功能掩码定义
#define EAR_SENSOR_MASK_DEV_DETECT_RUN      0x00  // 查询状态
#define EAR_SENSOR_MASK_HR_DETECT_START     0x01  // 开始检测心率
#define EAR_SENSOR_MASK_HR_DETECT_CANCEL    0x02  // 取消检测心率
#define EAR_SENSOR_MASK_SPO2_DETECT_START   0x03  // 开始检测血氧
#define EAR_SENSOR_MASK_SPO2_DETECT_CANCEL  0x04  // 取消检测血氧
#define EAR_SENSOR_MASK_STEPS_DETECT_START  0x05  // 开始检测步数
#define EAR_SENSOR_MASK_STEPS_DETECT_STOP   0x06  // 取消检测步数
#define EAR_SENSOR_MASK_HR_DATA_REPORT      0x20  // 获取全天心率数据
#define EAR_SENSOR_MASK_HR_DATA_CLEAR       0x21  // app获取到数据，清除已发数据

#define TWS_FUNC_ID_WEAR_STATE              TWS_FUNC_ID('W', 'E', 'A', 'R')
#define TWS_FUNC_ID_HEAL_DETCT_WORK_STATUS  TWS_FUNC_ID('H', 'D', 'W', 'S')

// 初始化函数
void ear_sports_data_opt_init(void);

// 主处理函数
int JL_rcsp_ear_sports_data_funciton(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len);

// 推送函数声明
int ear_sports_push_hr_detect_success(void *priv, u8 OpCode_SN);
int ear_sports_push_hr_detect_fail(void *priv, u8 error_code);
int ear_sports_push_spo2_detect_success(void *priv);
int ear_sports_push_spo2_detect_fail(void *priv, u8 error_code);

// 穿戴状态变化上报函数
int ear_sports_push_wear_status_change(u8 wear_status);

// 外部调用接口：手动触发穿戴状态上报（在app_sensor.c中实现）
void app_sensor_report_wear_status(void);

int ear_sports_handle_hr_data_send();

// 主机通知开启心率检测
void tws_sibling_hr_start(void);
// 主机通知停止心率检测
void tws_sibling_hr_stop(void);
// 主机通知开启血氧检测
void tws_sibling_spo2_start(void);
// 主机通知停止血氧检测
void tws_sibling_spo2_stop(void);
// 主耳通知开启计步
void tws_sibling_steps_detect_start(void);
// 主耳通知停止计步
void tws_sibling_steps_detect_stop(void);

// 获取对耳的佩戴状态
int bt_tws_get_sibling_wear_status(void);
// 通知对耳同步健康检测状态
void tws_sync_health_detect_work_status(void);
#endif

