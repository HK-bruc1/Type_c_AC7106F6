#ifndef TUYA_BLE_PORT_JL_H_
#define TUYA_BLE_PORT_JL_H_
#include "os/os_type.h"
#include "btstack/le/le_user.h"

struct ble_task_param {
    char *ble_task_name;
    OS_SEM ble_sem;
};

enum {
    BLE_TASK_MSG_MSG_COMES,
    BLE_TASK_MSG_INFO_SYNC_AUTH,
    BLE_TASK_MSG_INFO_SYNC_SYS,
    BLE_TASK_PAIR_INFO_SYNC_SYS,
};

extern struct ble_task_param ble_task;
u8 JL_tuya_ble_gatt_receive_data(u8 *p_data, u16 len);
extern void tuya_ble_operation_register(struct ble_server_operation_t *operation);
uint8_t *tuya_get_ble_characteristic_value(uint16_t *len);

#endif //
