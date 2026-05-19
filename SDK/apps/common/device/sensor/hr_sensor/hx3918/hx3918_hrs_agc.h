#ifndef _hx3918_agc_H_
#define _hx3918_agc_H_
#include <stdint.h>
#include <stdbool.h>
#include "hx3918.h"

#define CAL_DELAY_COUNT (3)

#define CAL_FLG_LED_DR     0x01
#define CAL_FLG_LED_DAC    0x02
#define CAL_FLG_AMB_DAC    0x04
#define CAL_FLG_RF         0x08

#define HRS_CAL_INIT_LED   64
#define NO_TOUCH_CHECK_NUM 2


typedef enum {
    hrsCalStart,
    hrsLedCur,
    hrsCalLed,
    hrsCalFinish,
} HRS_STATE_T;

typedef struct {
    uint8_t flag;
    bool work;
    uint8_t int_cnt;
    uint8_t cur255_cnt;
    uint16_t led_cur;
    uint8_t led_idac;
    uint8_t amb_idac;
    uint8_t ontime;
    uint16_t led_step;
    uint8_t state;
    uint8_t led_max_cur;
} HRS_CAL_SET_T;

typedef enum {
    MSG_HRS_NO_WEAR,
    MSG_HRS_WEAR,
    MSG_HRS_INIT
} hx3918_hrs_wear_msg_code_t;


void Init_hrs_PPG_Calibration_Routine(HRS_CAL_SET_T *calR);
void Restart_hrs_PPG_Calibration_Routine(HRS_CAL_SET_T *calR);
void PPG_hrs_Calibration_Routine(HRS_CAL_SET_T *calR, int32_t led, int32_t amb);
HRS_CAL_SET_T PPG_hrs_agc(void);
void hx3918_hrs_cal_init(void);
void hx3918_hrs_cal_off(void);

void hx3918_hrs_low_power(void);
void hx3918_hrs_normal_power(void);
void hx3918_hrs_updata_reg(void);
void hx3918_hrs_set_mode(uint8_t mode_cmd);
void hx3917_hrs_set_mode(uint8_t mode_cmd);
SENSOR_ERROR_T hx3918_hrs_enable(void);
void hx3917_hrs_updata_reg(void);
void hx3917_hrs_low_power(void);
void hx3918_hrs_disable(void);
hx3918_hrs_wear_msg_code_t hx3918_hrs_get_wear_status(void);
uint8_t hx3918_hrs_read(ppg_sensor_data_t *s_dat);
bool hx3918_hrs_init(void);

hx3918_hrs_wear_msg_code_t hx3918_hrs_wear_mode_check(int32_t infrared_data);
//hx3918_hrs_wear_msg_code_t hx3918_hrs_check_unwear(int32_t infrared_data);
hx3918_hrs_wear_msg_code_t hx3918_hrs_check_wear(void);
HRS_CAL_SET_T get_hrs_agc_status(void);
void hx3918_hrs_data_reset(void);

#endif
