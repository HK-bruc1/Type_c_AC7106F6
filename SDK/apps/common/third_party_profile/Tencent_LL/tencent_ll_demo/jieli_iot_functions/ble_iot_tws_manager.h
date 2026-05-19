#ifndef BLE_IOT_TWS_MANAGER_H
#define BLE_IOT_TWS_MANAGER_H

#include "asm/cpu.h"

// iot平台的若干功能的tws同步
#define IOT_TWS_FUNC_ID_TWS_SYNC \
	TWS_FUNC_ID('I' + 'O' + 'T', \
			'F' + 'U'  + 'N' + 'C', \
			'T' + 'W'  + 'S', \
			'S' + 'Y' + 'N' + 'C')

typedef enum {
    IotFuncTwsSyncTypeAncEffect = 0,	// ANC效果
} IotFuncTwsSyncType;	// 同步效果
/**
 * @brief 同步功能信息到对耳
 *
 * @param u16 types (IotFuncTwsSyncType | IotFuncTwsSyncType | ...)
 */
void iot_func_tws_sync_to_sibling(u16 types);

/**
 *	@brief 更新tws消息缓存到vm及更新功能状态
 */
void iot_update_function_from_tws_info(void);

#endif  // BLE_IOT_TWS_MANAGER_H
