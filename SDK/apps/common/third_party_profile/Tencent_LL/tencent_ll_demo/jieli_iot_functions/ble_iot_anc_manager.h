#ifndef BLE_IOT_ANC_MANAGER_H
#define BLE_IOT_ANC_MANAGER_H

#include "asm/cpu.h"
#if CONFIG_ANC_ENABLE
#include "audio_anc.h"
#endif

/**
 * @brief 获取anc效果缓存信息
 *
 * @param *anc_info[25]
 */
void iot_get_anc_effect_info(u8 *anc_info);

/**
 * @brief 设置anc效果缓存信息
 *
 * @param *anc_info[25]
 */
void iot_set_anc_effect_info(u8 *anc_info);

/**
 * @brief 处理anc效果
 *
 * @param anc_setting[25] anc效果
 * @param write_vm 是否写vm
 * @param tws_sync 是否tws同步
 */
void iot_deal_anc_effect(u8 *anc_setting, u8 write_vm, u8 tws_sync);

/**
 * @brief 获取anc模式
 *
 * @result anc模式，0 - 关闭模式，1 - 降噪模式，2 - 通透模式
 */
u8 iot_get_anc_mode();

/**
 * @brief 设置anc模式
 *
 * @param mode - 模式，0 - 关闭模式，1 - 降噪模式，2 - 通透模式
 */
void iot_anc_mode_update(u8 mode);

/**
 * @brief 更新缓存anc数据到vm
 */
void iot_anc_effect_update_vm_value();

/**
 * @brief 获取anc模式对应的音效数据
 *
 * @param mode - 模式，0 - 关闭模式，1 - 降噪模式，2 - 通透模式
 * @result 返回数据，0是失败
 */
u16 iot_anc_effect_value_get(u8 mode);

/**
 * @brief anc效果同步，该函数一般用于初始化或者tws连接的时候
 */
int iot_anc_effect_setting_sync();

/**
 * 初始化anc效果
 */
int iot_anc_effect_setting_init(void);

#endif  // BLE_IOT_ANC_MANAGER_H
