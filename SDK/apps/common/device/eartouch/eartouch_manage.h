#ifndef __WEAR_MANAGER_H__
#define __WEAR_MANAGER_H__

#include "generic/typedef.h"

#define EARTCH_INTERRUPT_MODE    1
#define EARTCH_TIMER_MODE        2

#define EARTCH_OUTSDIE_TCH       0
#define EARTCH_HX300X            1

typedef enum {
    EARTOUCH_STATE_IN_EAR,
    EARTOUCH_STATE_OUT_EAR,
    EARTOUCH_STATE_TRIM_OK,
    EARTOUCH_STATE_TRIM_ERR,
} eartouch_state;

typedef struct _eartouch_platform_data {
    char logo[20];
    s16 pwr_port;                       //入耳检测传感器供电脚
    s16 int_port;                       //检测脚
    s16 iic_clk;                        //光感iic时钟脚
    s16 iic_dat;                        //光感iic数据脚
    u16 in_filter_cnt; 			        //戴上消抖
    u16 out_filter_cnt;   		        //拿下消抖
} eartouch_platform_data;

typedef struct _eartouch_interface {
    char logo[20];
    u8 interrupt_mode;          //使用中断唤醒的方式进行检测还是使用systimer
    u8 int_level;               //触发入耳时的电平，用于中断方式改变唤醒边沿
    u8(*init)(eartouch_platform_data *data);
    eartouch_state(*check)(void *priv);
} eartouch_interface;

typedef struct _eartouch_state_handle {
    eartouch_state local_st;
    eartouch_state remote_st;
} eartouch_state_handle;

typedef struct _eartouch_manage_handle {
    eartouch_state_handle *state_hdl;
    u8 filter_cnt;
    u8 idle_lock;                                   //检测过程不进入低功耗
    u16 pwr_port;
    u16 int_port;
    u16 in_filter_cnt;
    u16 out_filter_cnt;
    eartouch_interface *etch_interface;
} eartouch_manage_handle;

eartouch_state get_local_eartouch_st(void);

eartouch_state  get_remote_eartouch_st(void);

extern eartouch_interface eartouch_begin[];
extern eartouch_interface eartouch_end[];

#define REGISTER_EARTOUCH(eartouch) \
	const eartouch_interface eartouch sec(.eartouch_dev)

#define list_for_each_eartouch(c) \
	for (c=eartouch_begin; c<eartouch_end; c++)

eartouch_state_handle *get_eartouch_state_hdl(void);
void eartouch_init(eartouch_platform_data *platform_data);
/* --------------------------------------------------------------------------*/
/**
 * @brief 入耳检测事件发送，同步更新状态给TWS
 *
 * @param state：本地入耳状态 EARTOUCH_STATE_IN_EAR / EARTOUCH_STATE_OUT_EAR
 */
/* ----------------------------------------------------------------------------*/
void eartouch_event_handler(u8 state);

#endif
