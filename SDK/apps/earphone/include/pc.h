#ifndef _PC_H_
#define _PC_H_

#include "app_msg.h"

/**
 * @brief   进入pc模式
 * @param   arg 附加参数
 * @return  下一个要进入的模式
 */
struct app_mode *app_enter_pc_mode(int arg);

#endif
