#ifndef _APP_LE_AURACAST_H
#define _APP_LE_AURACAST_H

#include "typedef.h"
#include "btstack/le/auracast_sink_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_AURACAST_STATUS_STOP,
    APP_AURACAST_STATUS_SUSPEND,
    APP_AURACAST_STATUS_BROADCAST,
    APP_AURACAST_STATUS_SCAN,
    APP_AURACAST_STATUS_SYNC,
} APP_AURACAST_STATUS;

/**
 * @brief le auracast是否正在运行
 *
 * @return 1:正在运行；0:空闲
 */
u8 le_auracast_is_running();

/**
 * @brief 关闭auracast功能（音频、扫描）
 *
 * @param need_recover 需要恢复：0:不恢复; 1:恢复
 */
void le_auracast_stop(u8 need_recover);

/**
 * @brief auracast音频恢复
 */
void le_auracast_audio_recover();

/**
 * @brief 手机选中广播设备开始播歌
 *
 * @param param 要监听的广播设备
 */
int app_auracast_sink_big_sync_create(auracast_sink_source_info_t *param);

/**
 * @brief 主动关闭所有正在监听播歌的广播设备
 *
 * @param need_recover 需要恢复：0:不恢复; 1:恢复
 */
int app_auracast_sink_big_sync_terminate(u8 need_recover);

/**
 * @brief 手机通知设备开始搜索auracast广播
 */
int app_auracast_sink_scan_start(void);

/**
 * @brief 手机通知设备关闭搜索auracast广播
 */
int app_auracast_sink_scan_stop(void);

#ifdef __cplusplus
};
#endif

#endif

