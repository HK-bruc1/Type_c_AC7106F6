#ifndef BLE_IOT_MSG_MANAGER_H
#define BLE_IOT_MSG_MANAGER_H

#include "asm/cpu.h"

#define DEVICE_EVENT_FROM_IOT	(('I' << 24) | ('O' << 16) | ('T' << 8) | 'P')

typedef enum {
    IOT_MSG_TYPE_UPDATE_ANC_VOICE_MAX_SYNC,
    IOT_MSG_TYPE_SETTING_UPDATE,
} IOT_MSG_TYPE;

/**
 * @brief 发送iot消息到app_core处理
 *
 * @pragma IOT_MSG_TYPE
 */
void iot_msg_send_to_app_core(IOT_MSG_TYPE msg_type);

#endif  // BLE_IOT_MSG_MANAGER_H
