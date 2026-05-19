#ifndef _hx3011_H_
#define _hx3011_H_
#include <stdint.h>
#include <stdbool.h>

#include "app_config.h"

#define HX3011_LDO_EN_IO IO_PORTB_02

//#include "tyhx_mems.h"
#define DEBUG_PPG_ONLY     // 只用IR 判断佩带和非佩戴, 靠近感应功能关闭,  但是hx3011_PROX.C要合入编译
/********************* alg_swi *********************
HRS_ALG_LIB   心率功能
SPO2_ALG_LIB  血氧功能
HRV_ALG_LIB   HRV功能
CHECK_TOUCH_LIB   纯佩戴功能（非心率血氧模式）
CHECK_LIVING_LIB  纯物体识别功能（非心率血氧模式）
BP_CUSTDOWN_ALG_LIB  血压功能

TIMMER_MODE  定时器模式， 读PPG数据， 默认采用定时器模式
INT_MODE     中断模式，读PPG数据

DEMO_MI      demo相关配置， 客户关闭， 在下面else里进行配置
HRS_BLE_APP  DEMO蓝牙控制宏， 客户关闭
SPO2_DATA_CALI   血氧DC补偿模式， 客户关闭
HRS_DEBUG    调试宏， 根据平台定义打印函数
AGC_DEBUG    调试宏， 根据平台定义打印函数
GSEN_40MS_TIMMER   DEMO调试使用， 客户关闭
NEW_GSEN_SCHME     DEMO调试使用， 客户关闭
*/
#define HRS_ALG_LIB
#define SPO2_ALG_LIB
//#define HRV_ALG_LIB
#define CHECK_TOUCH_LIB
//#define CHECK_LIVING_LIB
//#define SAR_ALG_LIB
#define BP_CUSTDOWN_ALG_LIB



//**************** read_data_mode ******************//
#define TIMMER_MODE           //timmer read fifo
//#define INT_MODE							//fifo_all_most_full
//#define TYHX_DEMO           // for ty demo
//****************** gsen_cgf *********************//
#define GSENSER_DATA
//#define GSEN_40MS_TIMMER
//#define NEW_GSEN_SCHME

//****************** other_cgf ********************//
#define EVB
//#define TWON3IN1LED
//#define EVB
//#define INT_FLOATING
//#define HRS_BLE_APP
//#define SPO2_DATA_CALI

//***************** vecter_swi ********************//
//#define SPO2_VECTOR
//#define HR_VECTOR
//#define HRV_TESTVEC

//**************** lab_test_mode ******************//
//#define LAB_TEST
//#define LAB_TEST_AGC

//****************** print_swi ********************//
#define TYHX_LOG_ENABLE
#define AGC_DEBUG_ENABLE
#define HRS_DEBUG_ENABLE

//**************************************************//

#ifdef TYHX_LOG_ENABLE
#define  TYHX_LOG(fmt,...)     printf("[hx3011] " fmt,##__VA_ARGS__)
#else
#define	 TYHX_LOG(...)
#endif

#ifdef AGC_DEBUG_ENABLE
#define  AGC_LOG(fmt,...)     printf("[hx3011] " fmt,##__VA_ARGS__)
#else
#define	 AGC_LOG(...)
#endif

#ifdef HRS_DEBUG_ENABLE
#define  DEBUG_PRINTF(fmt,...)     printf("[hx3011] " fmt,##__VA_ARGS__)
#else
#define	 DEBUG_PRINTF(...)
#endif


//#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
typedef struct {
    uint16_t      lastesthrs;  			//??????????
    uint8_t       lastestspo2;			//????
    uint8_t       lastesthrv;			//HRV??
    uint16_t      wearstatus;
    uint32_t      living_data_cnt;
    uint8_t       living_signal_quality;
    uint8_t       living_motion_status;
    int32_t 	  leak_green;
    int32_t 	  leak_red;
    int32_t	      leak_ir;
    int32_t 	  card_green;
    int32_t 	  card_red;
    int32_t	  	  card_ir;
    uint8_t       hrv_tired_value;
    uint8_t       hrv_spirit_pressure;
    uint8_t       hr_living;
    uint32_t      hr_living_qu;
    uint32_t      hr_living_std;
    uint8_t       living_status;                //living_status
} tyhx_hrsresult_t;

#if defined(DEMO_COMMON)
#define HRS4100_IIC_CLK  30
#define HRS4100_IIC_SDA  0
#define LIS3DH_IIC_CLK   18
#define LIS3DH_IIC_SDA   16
#define EXT_INT_PIN      1
#define GREEN_LED_SLE    2
#define RED_LED_SLE      4
#define IR_LED_SLE       1
#define RED_AGC_OFFSET   12
#define IR_AGC_OFFSET    12
#define GREEN_AGC_OFFSET    8
#define HR_GREEN_AGC_OFFSET 8
#define BIG_SCREEN
#elif defined(TWON3IN1LED)
#define HRS4100_IIC_CLK  30
#define HRS4100_IIC_SDA  0
#define LIS3DH_IIC_CLK   18
#define LIS3DH_IIC_SDA   16
#define EXT_INT_PIN      1
#define GREEN_LED_SLE    2
#define RED_LED_SLE      4
#define IR_LED_SLE       1
#define RED_AGC_OFFSET   12
#define IR_AGC_OFFSET    12
#define GREEN_AGC_OFFSET    8
#define HR_GREEN_AGC_OFFSET 8
#define BIG_SCREEN
#elif defined(DEMO_NEW)
#define HRS4100_IIC_CLK  0
#define HRS4100_IIC_SDA  30
#define LIS3DH_IIC_CLK   18
#define LIS3DH_IIC_SDA   16
#define EXT_INT_PIN      1
#define GREEN_LED_SLE    1
#define RED_LED_SLE      2
#define IR_LED_SLE       4
#define RED_AGC_OFFSET   50
#define IR_AGC_OFFSET    50
#define HR_GREEN_AGC_OFFSET 4
#define BIG_SCREEN

#elif defined(DEMO_BAND)
#define HRS4100_IIC_CLK  4
#define HRS4100_IIC_SDA  3
#define LIS3DH_IIC_CLK   14
#define LIS3DH_IIC_SDA   12
#define EXT_INT_PIN      7
#define GREEN_LED_SLE    0
#define RED_LED_SLE      2
#define IR_LED_SLE       1
#define RED_AGC_OFFSET   64
#define IR_AGC_OFFSET    64
#define GREEN_AGC_OFFSET 8
#define HR_GREEN_AGC_OFFSET 8

#elif defined(DEMO_GT01)
#define HRS4100_IIC_CLK  5
#define HRS4100_IIC_SDA  6
#define LIS3DH_IIC_CLK   14
#define LIS3DH_IIC_SDA   12
#define EXT_INT_PIN      7
#define GREEN_LED_SLE    1
#define RED_LED_SLE      4
#define IR_LED_SLE       2
#define RED_AGC_OFFSET   32
#define IR_AGC_OFFSET    32
#define GREEN_AGC_OFFSET 8
#define BIG_SCREEN

#elif defined(EVB)
#define HRS4100_IIC_CLK  9
#define HRS4100_IIC_SDA  10
#define LIS3DH_IIC_CLK   13
#define LIS3DH_IIC_SDA   14
#define EXT_INT_PIN      11
#define GREEN_LED_SLE    2
#define RED_LED_SLE      4
#define IR_LED_SLE       1
#define RED_AGC_OFFSET   32
#define IR_AGC_OFFSET    32
#define GREEN_AGC_OFFSET 5
#define HR_GREEN_AGC_OFFSET 5

#else
#define HRS4100_IIC_CLK  9
#define HRS4100_IIC_SDA  10
#define LIS3DH_IIC_CLK   13
#define LIS3DH_IIC_SDA   14
#define EXT_INT_PIN      11
#define GREEN_LED_SLE    1
#define RED_LED_SLE      4
#define IR_LED_SLE       2
#define RED_AGC_OFFSET   64
#define IR_AGC_OFFSET    64
#define GREEN_AGC_OFFSET 8

#endif


typedef enum {
    PPG_INIT,
    PPG_OFF,
    PPG_LED_OFF,
    CAL_INIT,
    CAL_OFF,
    RECAL_INIT
} hx3011_mode_t;

typedef enum {
    SENSOR_OK,
    SENSOR_OP_FAILED,
} SENSOR_ERROR_T;

typedef enum {
    HRS_MODE,
    SPO2_MODE,
    WEAR_MODE,
    HRV_MODE,
    LIVING_MODE,
    LAB_TEST_MODE,
    FT_LEAK_LIGHT_MODE,
    FT_GRAY_CARD_MODE,
    FT_INT_TEST_MODE,
    NULL_MODE
} WORK_MODE_T;

typedef struct {
    uint8_t count;
    int32_t green_data[32];
    int32_t red_data[16];
    int32_t ir_data[16];
    int32_t s_buf[64];
    int32_t sar_data;
    uint8_t green_cur;
    uint8_t red_cur;
    uint8_t ir_cur;
    uint8_t green_offset;
    uint8_t red_offset;
    uint8_t ir_offset;
} ppg_sensor_data_t;

typedef enum {
    MSG_LIVING_NO_WEAR,
    MSG_LIVING_WEAR
} hx3011_living_wear_msg_code_t;

typedef enum {
    MSG_NO_WEAR,
    MSG_WEAR
} hx3011_wear_msg_code_t;

typedef struct {
    hx3011_living_wear_msg_code_t  wear_status;
    uint32_t           data_cnt;
    uint8_t            signal_quality;
    uint8_t            motion_status;
} hx3011_living_results_t;

typedef struct {
    int32_t p1_noise;
    int32_t p2_noise;
    int32_t p3_noise;
    int32_t p4_noise;
} NOISE_PS_T;



extern uint8_t alg_ram[6 * 1024 + 512];
extern uint8_t hx3011_chip_id;

extern WORK_MODE_T work_mode_flag;

void hx_set_touch_mode(uint8_t current_mode);
uint16_t hx3011_read_fifo_data(int32_t *buf, uint8_t phase_num, uint8_t sig);
void read_data_packet(int32_t *ps_data);
void hx3011_delay_us(uint32_t us);
void hx3011_delay(uint32_t ms);
bool hx3011_write_reg(uint8_t addr, uint8_t data);
bool hx3011_write_reg(uint8_t addr, uint8_t data);
uint8_t hx3011_read_reg(uint8_t addr);
bool hx3011_brust_read_reg(uint8_t addr, uint8_t *buf, uint8_t length);
bool hx3011_chip_check(void);
uint8_t hx3011_read_fifo_size(void);
void hx3011_ppg_off(void);
void hx3011_ppg_on(void);
void hx3011_ppg_timer_cfg(bool en);
void hx3011_agc_timer_cfg(bool en);
void hx3011_gpioint_cfg(bool en);
bool hx3011_init(WORK_MODE_T mode);
void hx3011_agc_Int_handle(void);
void hx3011_gesensor_Int_handle(void);
void hx3011_spo2_ppg_init(void);
void hx3011_spo2_ppg_Int_handle(void);
void hx3011_wear_ppg_Int_handle(void);
void hx3011_ft_hrs_Int_handle(void);
void hx3011_ft_spo2_Int_handle(void);
void hx3011_hrs_ppg_init(void);
void hx3011_hrs_ppg_Int_handle(void);
void hx3011_hrv_ppg_Int_handle(void);
void hx3011_living_Int_handle(void);
//void hx3011_gesensor_Int_handle(void);
void hx3011_ppg_Int_handle(void);
uint32_t hx3011_timers_start(void);
uint32_t hx3011_timers_stop(void);
uint32_t hx3011_gpioint_init(void);

uint32_t hx3011_gpioint_enable(void);
uint32_t hx3011_gpioint_disable(void);
uint32_t hx3011_40ms_timers_start(void);
uint32_t hx3011_40ms_timers_stop(void);

void hx3011_vin_check(uint16_t led_vin);

#ifdef LAB_TEST
void hx3011_lab_test_init(void);
void hx3011_test_alg_cfg(void);
SENSOR_ERROR_T hx3011_lab_test_enable(void);
void hx3011_lab_test_Int_handle(void);
void hx3011_lab_test_read_packet(uint32_t *data_buf);
#endif

#ifdef CHECK_TOUCH_LIB
hx3011_wear_msg_code_t hx3011_check_touch(int32_t *ir_data, uint8_t data_len);
SENSOR_ERROR_T hx3011_check_touch_enable(void);
#endif

bool hx3011_living_send_data(int32_t *new_raw_data, uint8_t dat_len, uint32_t green_data_als, int16_t *gsen_data_x, int16_t *gsen_data_y, int16_t *gsen_data_z);
hx3011_living_results_t hx3011_living_get_results(void);
void agc_timeout_handler(void *p_context);
void heart_rate_meas_timeout_handler(void *p_context);

int16_t hx3918xxx_read_two_reg_low_triple(uint8_t addh, uint8_t addl);
void display_refresh(void);

// 3011进入低功耗
int hx3011_enter_low_power();
// 3011退出低功耗
int hx3011_exit_low_power();
// 3011切换低功耗模式
int hx3011_switch_sniff(bool en);
#endif
