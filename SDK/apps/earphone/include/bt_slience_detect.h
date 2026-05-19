#ifndef BT_SLIENCE_DETECT_H
#define BT_SLIENCE_DETECT_H

#include "generic/typedef.h"

enum {
    BT_SLIENCE_NO_DETECTING,		// 没在检测
    BT_SLIENCE_NO_ENERGY,			// 检测到没能量
    BT_SLIENCE_HAVE_ENERGY,			// 检测到有能量
};

/**
 * @brief 使能对应地址的设备的能量检测
 *
 * @param bt_addr  设备地址
 * @param ingore_packet_num  忽略的包数
 *
 */
void bt_start_a2dp_slience_detect(u8 *bt_addr, int ingore_packet_num);

/**
 * @brief  停止对应地址的设备能量检测
 *
 * @param bt_addr  设备地址
 *
 */
void bt_stop_a2dp_slience_detect(u8 *bt_addr);

/**
 * @brief  获取对应地址的设备能量检测结果
 *
 * @param bt_addr  设备地址
 *
 * @return  0:没有检测  1:没有能量  2:有能量
 */
int bt_slience_detect_get_result(u8 *bt_addr);

/**
 * @brief 重置能量检测
 */
void bt_reset_a2dp_slience_detect();

/**
 * @brief 获取能量检测的设备数量
 */
u8 bt_a2dp_slience_detect_num();

/**
 * @brief 获取对应地址的设备的是否在能量检测
 *
 * @param bt_addr   设备地址
 *
 * @return  0:没有检测  1:正在检测
 */
int bt_slience_get_detect_addr(u8 *bt_addr);

#endif

