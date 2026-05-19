#ifndef __APP_KEY_DUT_H__
#define __APP_KEY_DUT_H__

#include "generic/typedef.h"

/*获取产测按键事件表的地址，返回数据长度*/
int get_key_dut_event_list(u8 **key_list);

/*开始按键产测*/
int app_key_dut_start();

/*结束按键产测*/
int app_key_dut_stop();

/*获取已经按下的按键*/
int get_key_press_list(void *key_list);


/*
*********************************************************************
*                  app_key_dut_send
* Description: (标准SDK情景配置流程)发送按键事件到key dut
* Arguments  : msg  判断是否为按键触发;
*			    	传param->msg, 可参考audio_ability.c调用方式
               key_name  按键事件名称，需与key_event_list 一一对应
* Return	 : 0 成功 其他 失败
*********************************************************************
*/
int app_key_dut_send(int *msg, const char *key_name);

/*
*********************************************************************
*                  user_app_key_dut_send
* Description: (用户自定义)发送按键事件到key dut
               key_name  按键事件名称，需与key_event_list 一一对应
* Return	 : 0 成功 其他 失败
* Note       : 需确保是按键触发才调用此API !!!
*********************************************************************
*/
int app_key_dut_user_send(const char *key_name);

void app_key_dut_msg_handler(int app_msg);

#endif /*__APP_KEY_DUT_H__*/
