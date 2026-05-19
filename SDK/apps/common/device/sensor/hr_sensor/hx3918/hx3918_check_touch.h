#ifndef _hx3918_CHECK_TOUCH_H_
#define _hx3918_CHECK_TOUCH_H_

#include <stdint.h>
#include <stdbool.h>
#include "hx3918.h"
//#include "tyhx_hrs_custom.h"



uint8_t hx3918_check_touch_read(ppg_sensor_data_t *s_dat);
SENSOR_ERROR_T hx3918_check_touch_enable(void);
uint8_t hx3918_check_touch_read(ppg_sensor_data_t *s_dat);
hx3918_wear_msg_code_t hx3690l_check_touch_send_data(int32_t *ir_data, uint8_t count);
hx3918_wear_msg_code_t check_touch_alg(int32_t ir_data);

#endif
