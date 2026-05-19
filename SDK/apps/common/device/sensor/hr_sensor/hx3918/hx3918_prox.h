/**
  ******************************************************************************
  * @file           : hx3918_PROX.h
  * @version        : v1.0
  * @brief          :
*/
#ifndef __HX3918_PROX_H_
#define __HX3918_PROX_H_

#include <stdbool.h>
#include <stdint.h>




#define CH0   0
#define CH1   1
#define CH2   2




#define CH0_OFFSET   0
#define CH1_OFFSET   1
#define CH2_OFFSET   2


#define RAW0   0
#define RAW1   1
#define RAW2   2

#define BL0   0
#define BL1   1
#define BL2   2


#define LP0   0
#define LP1   1
#define LP2   2


#define DIFF0   0
#define DIFF1   1
#define DIFF2   2

// user  to config
#define WEAR_CH   CH1
#define REF_CH    CH0

#define hx_min(x,y)  ( (x)>(y)?(y):(x) )
#define hx_abs(x)    ((x)>0?(x):(-(x)))


#define PROX_CHIPID         0x27


#define NORMALWEARDIFF      8000                 //正常佩戴测试到的平均值, 宽松佩戴

#define WEAR_HIGH_DIFF		NORMALWEARDIFF/6
#define WEAR_LOW_DIFF     	NORMALWEARDIFF/8
#define REF_HIGH_DIFF 		NORMALWEARDIFF/6
#define REF_LOW_DIFF 		NORMALWEARDIFF/8



//HX3918 cap 测试电容的满量程设置, 最小
//#define FULL0625PF  //HX3918 full range cap = 0.625pf //一般不用这个, 这个满量程设置, 说明感应pad很小,
//#define FULL125PF   //HX3918 full range cap = 1.25pf
#define FULL25PF      //HX3918 full range cap = 2.5pf, 一般PAD大小默认这个.
//#define FULL375PF   //HX3918 full range cap = 3.75 pf
//#define FULL50PF    //HX3918 full range cap = 5.0 pf

//READ_DEVATION ,read offset 的偏差控制, 一个offset = 0.0586pf 按照不同量程, 对应的code, 留10%余量
#ifdef FULL125PF
#define READ_DEVATION        1680
#elif defined(FULL25PF)
#define READ_DEVATION        845
#elif defined(FULL375PF)
#define READ_DEVATION        560
#elif defined(FULL375PF)
#define READ_DEVATION        420
#elif defined(FULL50PF)
#define READ_DEVATION        420
#endif



typedef enum {
    prox_no_detect = 0,   // 靠近感应没有检测到
    prox_detect = 1,      // 靠近感应检测到
} prox_result_t;

typedef enum {
    NORMAL_WEAR_MODE = 0,//
    SLEEP_WEAR_MODE = 1,//
} wear_mode_t;


extern int32_t  sar_wear_thres;
extern int32_t  sar_unwear_thres;

extern int32_t gdiff[2];
extern int16_t glp[2];
extern int16_t gbl[2];
extern int16_t nv_base_data[2];
extern int16_t nv_wear_offset[2];


#define FDSLEN 4  //flash 存储的长度

extern uint32_t fds_read[FDSLEN]; //flash 读
extern uint32_t fds_write[FDSLEN];//flash 写
extern uint8_t wear_detect_mode;



void hx3918_power_up(void);
void hx3918_power_down(void);
void hx3918_power_restart(void);
uint8_t hx3918_check_device_id(void);
bool hx3918_prox_reg_init(void);
uint8_t hx3918_report_prox_status(void);

void hx3918_prox_factory_hanging_cali(void);
bool hx3918_prox_init(void);
void hx3918_factory_cali_save_nv(void);
int16_t hx3918_read_diff_lp(uint8_t chx_diff);
bool hx3918_prox_factory_init(void);
void hx_set_wear_detect_mode(uint8_t current_mode);
int32_t hx90xx_get_prox_data(void);
void hx3918_cali_with_ir(int32_t infrared_data, int32_t ir_thres);
void hx3918_update_min_nv_data(int16_t min_data);
void hx3918_set_cap_enable(bool enable);
void hx3918_set_cap_data_ready(uint8_t data_ready);

#endif
