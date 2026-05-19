#ifndef _CHARGE_H_
#define _CHARGE_H_

#include "typedef.h"
#include "device.h"

/*------充满电电压选择 4.041V-4.534V-------*/
//4.2V低压压电池配置0~15
#define CHARGE_FULL_V_MIN_4040  0
#define CHARGE_FULL_V_MIN_4060	1
#define CHARGE_FULL_V_MIN_4080	2
#define CHARGE_FULL_V_MIN_4100	3
#define CHARGE_FULL_V_MIN_4120	4
#define CHARGE_FULL_V_MIN_4140	5
#define CHARGE_FULL_V_MIN_4160	6
#define CHARGE_FULL_V_MIN_4180	7
#define CHARGE_FULL_V_MIN_4200	8
#define CHARGE_FULL_V_MIN_4220	9
#define CHARGE_FULL_V_MIN_4240	10
#define CHARGE_FULL_V_MIN_4260	11
#define CHARGE_FULL_V_MIN_4280	12
#define CHARGE_FULL_V_MIN_4300	13
#define CHARGE_FULL_V_MIN_4320	14
#define CHARGE_FULL_V_MIN_4340	15
//4.4v高压电池配置16~31
#define CHARGE_FULL_V_MID_4240	16
#define CHARGE_FULL_V_MID_4260	17
#define CHARGE_FULL_V_MID_4280	18
#define CHARGE_FULL_V_MID_4300	19
#define CHARGE_FULL_V_MID_4320	20
#define CHARGE_FULL_V_MID_4340	21
#define CHARGE_FULL_V_MID_4360	22
#define CHARGE_FULL_V_MID_4380	23
#define CHARGE_FULL_V_MID_4400	24
#define CHARGE_FULL_V_MID_4420	25
#define CHARGE_FULL_V_MID_4440	26
#define CHARGE_FULL_V_MID_4460	27
#define CHARGE_FULL_V_MID_4480	28
#define CHARGE_FULL_V_MID_4500	29
#define CHARGE_FULL_V_MID_4520	30
#define CHARGE_FULL_V_MID_4540	31
//4.5v高压电池配置32~47
#define CHARGE_FULL_V_MAX_4340	32
#define CHARGE_FULL_V_MAX_4360	33
#define CHARGE_FULL_V_MAX_4380	34
#define CHARGE_FULL_V_MAX_4400	35
#define CHARGE_FULL_V_MAX_4420	36
#define CHARGE_FULL_V_MAX_4440	37
#define CHARGE_FULL_V_MAX_4460	38
#define CHARGE_FULL_V_MAX_4480	39
#define CHARGE_FULL_V_MAX_4500	40
#define CHARGE_FULL_V_MAX_4520	41
#define CHARGE_FULL_V_MAX_4540	42
#define CHARGE_FULL_V_MAX_4560	43
#define CHARGE_FULL_V_MAX_4580	44
#define CHARGE_FULL_V_MAX_4600	45
#define CHARGE_FULL_V_MAX_4620	46
#define CHARGE_FULL_V_MAX_4640	47
#define CHARGE_FULL_V_MAX       48

/*
 	充电电流选择
	恒流：30-225mA
    实际充电电流=恒流档位*分频器
*/
#define CHARGE_mA_30			0
#define CHARGE_mA_37P5			1
#define CHARGE_mA_45			2
#define CHARGE_mA_52P5			3
#define CHARGE_mA_60			4
#define CHARGE_mA_75			5
#define CHARGE_mA_90			6
#define CHARGE_mA_105			7
#define CHARGE_mA_120			8
#define CHARGE_mA_135			9
#define CHARGE_mA_150			10
#define CHARGE_mA_165			11
#define CHARGE_mA_180			12
#define CHARGE_mA_195			13
#define CHARGE_mA_210			14
#define CHARGE_mA_225			15
//电流分频器
#define CHARGE_DIV_1            0x00
#define CHARGE_DIV_2            0x10
#define CHARGE_DIV_3            0x20
#define CHARGE_DIV_4            0x30
#define CHARGE_DIV_5            0x60
#define CHARGE_DIV_6            0x70
#define CHARGE_DIV_7            0xa0
#define CHARGE_DIV_8            0xb0
#define CHARGE_DIV_9            0xe0
#define CHARGE_DIV_10           0xf0

/* 充电口下拉电阻 50k ~ 200k */
#define CHARGE_PULLDOWN_50K     0
#define CHARGE_PULLDOWN_100K    1
#define CHARGE_PULLDOWN_150K    2
#define CHARGE_PULLDOWN_200K    3

/*充满判断电流为恒流电流的比例配置*/
#define CHARGE_FC_IS_CC_DIV_5	5 // full current = constant_current / 5
#define CHARGE_FC_IS_CC_DIV_6	6
#define CHARGE_FC_IS_CC_DIV_7	7
#define CHARGE_FC_IS_CC_DIV_8	8
#define CHARGE_FC_IS_CC_DIV_9	9
#define CHARGE_FC_IS_CC_DIV_10	10
#define CHARGE_FC_IS_CC_DIV_11	11
#define CHARGE_FC_IS_CC_DIV_12	12
#define CHARGE_FC_IS_CC_DIV_13	13
#define CHARGE_FC_IS_CC_DIV_14	14
#define CHARGE_FC_IS_CC_DIV_15	15
#define CHARGE_FC_IS_CC_DIV_16	16
#define CHARGE_FC_IS_CC_DIV_17	17
#define CHARGE_FC_IS_CC_DIV_18	18
#define CHARGE_FC_IS_CC_DIV_19	19
#define CHARGE_FC_IS_CC_DIV_20	20

#define CHARGE_CCVOL_V			3000        //涓流充电向恒流充电的转换点

#define DEVICE_EVENT_FROM_CHARGE	(('C' << 24) | ('H' << 16) | ('G' << 8) | '\0')

struct charge_platform_data {
    u8 charge_en;	        //内置充电使能
    u8 charge_poweron_en;	//开机充电使能
    u8 charge_full_V;	    //充满电电压大小
    u8 charge_full_mA;	    //充满电电流大小
    u16 charge_mA;	        //恒流充电电流大小
    u16 charge_trickle_mA;  //涓流充电电流大小
    u8 ldo5v_pulldown_en;   //下拉使能位
    u8 ldo5v_pulldown_lvl;	//ldo5v的下拉电阻配置项,若充电舱需要更大的负载才能检测到插入时，请将该变量置为对应阻值
    u8 ldo5v_pulldown_keep; //下拉电阻在softoff时是否保持,ldo5v_pulldown_en=1时有效
    u16 ldo5v_off_filter;	//ldo5v拔出过滤值，过滤时间 = (filter*2 + 20)ms,ldoin<0.6V且时间大于过滤时间才认为拔出,对于充满直接从5V掉到0V的充电仓，该值必须设置成0，对于充满由5V先掉到0V之后再升压到xV的充电仓，需要根据实际情况设置该值大小
    u16 ldo5v_on_filter;    //ldo5v>vbat插入过滤值,电压的过滤时间 = (filter*2)ms
    u16 ldo5v_keep_filter;  //1V<ldo5v<vbat维持电压过滤值,过滤时间= (filter*2)ms
    u16 charge_full_filter; //充满过滤值,连续检测充满信号恒为1才认为充满,过滤时间 = (filter*2)ms
};

#define CHARGE_PLATFORM_DATA_BEGIN(data) \
		struct charge_platform_data data  = {

#define CHARGE_PLATFORM_DATA_END()  \
};


enum {
    CHARGE_EVENT_CHARGE_START,
    CHARGE_EVENT_CHARGE_CLOSE,
    CHARGE_EVENT_CHARGE_FULL,
    CHARGE_EVENT_LDO5V_KEEP,
    CHARGE_EVENT_LDO5V_IN,
    CHARGE_EVENT_LDO5V_OFF,
};


void set_charge_event_flag(u8 flag);
void set_charge_online_flag(u8 flag);
void set_charge_event_flag(u8 flag);
u8 get_charge_online_flag(void);
u8 get_charge_poweron_en(void);
void set_charge_poweron_en(u32 onOff);
void charge_start(void);
void charge_close(void);
u16 get_charge_mA_config(void);
void set_charge_mA(u16 charge_mA);
u8 get_ldo5v_pulldown_en(void);
u8 get_ldo5v_pulldown_res(void);
u8 get_ldo5v_online_hw(void);
u8 get_lvcmp_det(void);
void charge_check_and_set_pinr(u8 mode);
u16 get_charge_full_value(void);
void charge_module_stop(void);
void charge_module_restart(void);
void ldoin_wakeup_isr(void);
void charge_wakeup_isr(void);
int charge_init(const struct charge_platform_data *data);
void charge_set_ldo5v_detect_stop(u8 stop);
u8 check_pinr_shutdown_enable(void);
void charge_exit_shipping(void);
void charge_system_reset(void);

#endif    //_CHARGE_H_
