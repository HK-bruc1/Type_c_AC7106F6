#ifndef _PHONE_CALL_H_
#define _PHONE_CALL_H_

#include "system/includes.h"

/**
 * @brief 开启esco音频流
 *
 * @param bt_addr 设备的蓝牙地址
 * @result 0:成功;1:失败
 */
int bt_phone_esco_play(u8 *bt_addr);

#endif
