#ifndef _HRSENSOR_MANAGE_H
#define _HRSENSOR_MANAGE_H

#include "app_config.h"
#include "system/includes.h"

#if TCFG_HX3918_ENABLE
#include "hr_sensor/hx3918/hx3918.h"
#elif TCFG_HX3011_ENABLE
#include "hr_sensor/hx3011/hx3011.h"
#endif

enum {
    HR_SENSOR_ENABLE = 0,
    HR_SENSOR_DISABLE,
    SPO2_SENSOR_ENABLE,
    SPO2_SENSOR_DISABLE,
    HR_SENSOR_READ_DATA,
    SPO2_SENSOR_READ_DATA,
    HR_SEARCH_SENSOR,
    HR_SENSOR_LP_DETECTION,   //低功耗检测：一般检测佩戴状态
    HR_SENSOR_FACTORY_TEST,   //工厂测试模式：一般包含漏光和灰卡测试
    INPUT_ACCELEROMETER_DATA, //运动心率需要加速度数据
};

typedef struct {
    u8   logo[20];
    u8(*heart_rate_sensor_init)(void);
    char (*heart_rate_sensor_check)(void);
    int (*heart_rate_sensor_ctl)(u8 cmd, void *arg);
} HR_SENSOR_INTERFACE;

typedef struct {
    u8   iic_hdl;
    u8   iic_delay;                 //这个延时并非影响iic的时钟频率，而是2Byte数据之间的延时
    int  init_flag;
} HR_SENSOR_INFO;

struct hrsensor_platform_data {
    u8    iic;
    char  hrSensor_name[20];
    int   hrSensor_int_io;
};

typedef struct  {
    u8  manual_measurement;
    u8  sport_mode;
} hrsensor_seting_info;

typedef struct  {
    u8  wear;
    u8  hr_value;
    u8  spo2_value;
} hrsensor_algo_info;

typedef struct {
    u16 heart_rate;     // 心率值
    u8  spo2;           // 血氧值
    u8  wear_status;    // 佩戴状态
    u8  data_valid;     // 数据有效性标志
} hrsensor_data_t;

u8 hrsensor_write_nbyte(u8 w_chip_id, u8 register_address, u8 *buf, u8 data_len);
u8 hrsensor_read_nbyte(u8 r_chip_id, u8 register_address, u8 *buf, u8 data_len);
int hr_sensor_io_ctl(u8 cmd, void *arg);
int hr_sensor_init(void *_data);

u8 hr_sensor_measure_wear_start(u8 manual, u8 sport);
u8 hr_sensor_measure_hr_start(u8 manual, u8 sport);
u8 hr_sensor_measure_hr_stop(void);
u8 hr_sensor_measure_spo2_start(u8 manual, u8 sport);
u8 hr_sensor_measure_spo2_stop(void);
u8 hr_sensor_data_process(void);

// 获取心率传感器数据接口
hrsensor_data_t hr_sensor_get_data(void);
u16 hr_sensor_get_heart_rate(void);
u8 hr_sensor_get_spo2(void);
u8 hr_sensor_get_wear_status(void);

extern HR_SENSOR_INTERFACE  hrsensor_dev_begin[];
extern HR_SENSOR_INTERFACE hrsensor_dev_end[];

#if (TCFG_HX3011_ENABLE || TCFG_HX3918_ENABLE)
extern tyhx_hrsresult_t hrsresult;
#endif
#define REGISTER_HR_SENSOR(hrSensor) \
	static HR_SENSOR_INTERFACE hrSensor SEC_USED(.hrsensor_dev)

#define list_for_each_hrsensor(c) \
	for (c=hrsensor_dev_begin; c<hrsensor_dev_end; c++)

#define HRSENSOR_PLATFORM_DATA_BEGIN(data) \
		static const struct hrsensor_platform_data data = {

#define HRSENSOR_PLATFORM_DATA_END() \
};

#endif

