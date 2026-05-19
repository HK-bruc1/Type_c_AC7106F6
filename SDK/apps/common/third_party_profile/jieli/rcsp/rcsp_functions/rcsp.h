#ifndef __RCSP_H__
#define __RCSP_H__

#include "typedef.h"
#include "app_config.h"

#include "rcsp_config.h"
#include "rcsp_device_status.h"
#include "rcsp_browser.h"
#include "rcsp_cmd_user.h"
#include "rcsp_task.h"

//#define RCSP_DEBUG_EN //应用层rcsp调试打印开关

void rcsp_init(void);
void rcsp_exit(void);

void rcsp_timer_contrl(u8 status);

#endif // __RCSP_H__
