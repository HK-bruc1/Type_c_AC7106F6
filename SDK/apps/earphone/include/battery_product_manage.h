#ifndef BATTERY_PRODUCT_MANAGE_H
#define BATTERY_PRODUCT_MANAGE_H

#include "system/includes.h"

typedef struct _battery_volt_ctrl_info {
    u16 batt_max_voltage;
    u16 batt_min_voltage;
    u8  is_volt_ctrl;
} battery_volt_ctrl_info_t;

u16 battery_manage_get_volt_control_max(void);
u16 battery_manage_get_volt_control_min(void);
u8 battery_manage_get_batt_ctrl_flag(void);
void battery_manage_set_volt_ctl_flag(u8 bat_volt_control);
void battery_manage_set_batt_upperlimit(u16 max_volt);
void battery_manage_set_batt_lowerlimit(u16 min_volt);
void battery_manage_clear_batt_ctr(void);
u8 battery_manage_is_batt_volt_ctrl(void);

#endif
