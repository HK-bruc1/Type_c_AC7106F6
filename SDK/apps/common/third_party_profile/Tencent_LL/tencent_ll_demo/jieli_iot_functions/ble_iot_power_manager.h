#ifndef BLE_IOT_POWER_MANAGER_H
#define BLE_IOT_POWER_MANAGER_H

#include "asm/cpu.h"

/**
 * @brief 获取左耳电量百分比信息
 */
u8 iot_get_leftear_battery_percentage();

/**
 * @brief 获取右耳电量百分比信息
 */
u8 iot_get_rightear_battery_percentage();

/**
 * @brief 获取充电仓电量百分比信息
 */
u8 iot_get_box_battery_percentage();

#endif  // BLE_IOT_POWER_MANAGER_H

