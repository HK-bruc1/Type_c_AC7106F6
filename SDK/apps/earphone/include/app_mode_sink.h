#ifndef APP_MODE_SINK_H
#define APP_MODE_SINK_H

#include "app_mode_manager/app_mode_manager.h"

extern const struct key_remap_table sink_mode_key_table[];

/**
 * @brief 进入sink模式
 */
struct app_mode *app_enter_sink_mode(int arg);

/**
 * @brief 记录source的mode
 */
void app_sink_set_sink_mode_rsp_arg(enum app_mode_t arg);

#endif

