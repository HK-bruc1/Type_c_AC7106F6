#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

//#include "tyhx_hrs_custom.h"

#include "hx3011.h"
#include "hx3011_hrs_agc.h"
#include "tyhx_hrs_alg.h"
//#include "hx3011_prox.h"

#if TCFG_HX3011_ENABLE
#ifdef HRS_ALG_LIB

extern uint8_t hx3011_hrs_agc_idac;
extern uint8_t green_led_max_init;
extern uint8_t low_vbat_flag;
extern uint8_t read_fifo_first_flg;

extern const int32_t  hrs_ir_unwear_thres;
extern const int32_t  hrs_ir_wear_thres;
extern const int32_t  sar_unwear_thres;
extern const int32_t  sar_wear_thres;

static uint8_t s_ppg_state = 0;
static uint8_t s_cal_state = 0;
static int32_t agc_buf[4] = {0};

static uint8_t cal_delay = CAL_DELAY_COUNT;
static HRS_CAL_SET_T  calReg;

static hx3011_hrs_wear_msg_code_t hrs_wear_status = MSG_HRS_INIT;
static hx3011_hrs_wear_msg_code_t hrs_wear_status_pre = MSG_HRS_INIT;

static uint8_t no_touch_cnt = 0;

void hx3011_hrs_ppg_init(void) //20200615 ericy ppg fs=25hz, phase3 conversion ready interupt en
{
    uint16_t sample_rate = 25;                  /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate; /*period in clk num, num = Fclk/fs */

    uint8_t ps0_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps1_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps2_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps3_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */

    uint8_t ps0_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps1_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps2_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps3_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */

    uint8_t ps3_cp_avg_num_sel = 0;
    uint8_t ps2_cp_avg_num_sel = 2;
    uint8_t ps1_cp_avg_num_sel = 0;
    uint8_t ps0_cp_avg_num_sel = 0;

    uint8_t ps3_avg_num_sel_i2c = 0;
    uint8_t ps2_avg_num_sel_i2c = 0;
    uint8_t ps1_avg_num_sel_i2c = 0;
    uint8_t ps0_avg_num_sel_i2c = 0;

    /***********led open enable***********/

    uint8_t ps0_led_en_i2c = 1;
    uint8_t ps1_led_en_i2c = 0;
    uint8_t ps2_led_en_i2c = 1;
    uint8_t ps3_led_en_i2c = 0;

    uint8_t ps0_ofidac_en_i2c = 1;
    uint8_t ps1_ofidac_en_i2c = 1;
    uint8_t ps2_ofidac_en_i2c = 1;
    uint8_t ps3_ofidac_en_i2c = 1;

    uint8_t dccancel_ps0 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps1 = 0;  //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps2 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps3 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA

    uint8_t leddrive_ps0 = 64;    //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 0;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 0;   //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 0;    //LEDDRIVE_PS3  0~255 = 0~200ma

    uint8_t led_en_begin = 1;      // 0 = 2 ; 1 = 4 ; 2 = 8 ; 3 = 16 ;
    uint8_t afe_reset = 1;        //* 0 = 32clk ; 1 = 64clk ; 2 = 128clk ; 3 = 256clk(d) ;
    uint8_t en2rst_delay = 3;     // pen 2 rst delay 0 = 128,256, 512, 1024
    uint8_t init_wait_delay = 2; /* 0 = 800 ; 1 = 1600; 2 = 3200 ; 3 = 6400(d) */

    uint8_t ps1_interval_i2c = 0;   // ps2/ps3

    uint8_t thres_int = 1;   //thres int enable
    uint8_t data_rdy_int = 8;   //[3]:ps1_data2 [2]:ps1_data1 [1]:ps0_data2 [0]:ps0_data1


    uint8_t ldrsel_ps0 = IR_LED_SLE;
    uint8_t ldrsel_ps1 = 0;
    uint8_t ldrsel_ps2 = GREEN_LED_SLE;
    uint8_t ldrsel_ps3 = 0;

    /***********cap *********/
    uint8_t intcapsel_ps3 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps2 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps1 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps0 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf

    uint8_t ps0_led_on_time = 16;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps1_led_on_time = 16;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps2_led_on_time = 64;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps3_led_on_time = 64;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/

    uint8_t force_adc_clk_sel = 0;
    uint8_t force_adc_clk_cfg = 0;
    uint8_t force_PEN = 0;    //phase enable
    uint8_t force_PEN_cfg = 0;
    uint8_t force_LED_EN = 0;
    uint8_t force_LED_EN_cfg = 0;
    uint8_t force_CKAFEINT_sel = 0;
    uint8_t force_CKAFEINT_cfg = 0;

    hx3011_write_reg(0X16, (ps3_enable << 3 | ps2_enable << 2 | ps1_enable << 1 | ps0_enable));
    hx3011_write_reg(0X17, (ps3_osr << 6 | ps2_osr << 4 | ps1_osr << 2 | ps0_osr));
    hx3011_write_reg(0X18, (uint8_t)prf_clk_num);
    hx3011_write_reg(0X19, (uint8_t)(prf_clk_num >> 8));
    hx3011_write_reg(0X1a, (ps1_interval_i2c));
    hx3011_write_reg(0X1b, led_en_begin << 6 | afe_reset << 4 | en2rst_delay << 2 | init_wait_delay);
    //hx3011_write_reg(0X1b, 0x03);
    hx3011_write_reg(0X1c, ps0_led_on_time);
    hx3011_write_reg(0X1d, ps1_led_on_time);
    hx3011_write_reg(0X1e, ps2_led_on_time);
    hx3011_write_reg(0X1f, ps3_led_on_time);
    hx3011_write_reg(0X20, (ps3_cp_avg_num_sel << 6 | ps2_cp_avg_num_sel << 4 | ps1_cp_avg_num_sel << 2 | ps0_cp_avg_num_sel));
    hx3011_write_reg(0X21, (ps3_avg_num_sel_i2c << 6 | ps2_avg_num_sel_i2c << 4 | ps1_avg_num_sel_i2c << 2 | ps0_avg_num_sel_i2c));
    hx3011_write_reg(0X23, (ps0_led_en_i2c << 7 | ps1_led_en_i2c << 6 | ps2_led_en_i2c << 5 | ps3_led_en_i2c << 4 | ps0_ofidac_en_i2c << 3 | ps1_ofidac_en_i2c << 2 | ps2_ofidac_en_i2c << 1 | ps3_ofidac_en_i2c));
    hx3011_write_reg(0X24, (intcapsel_ps0 << 4 | intcapsel_ps1));
    hx3011_write_reg(0X25, (intcapsel_ps2 << 4 | intcapsel_ps3));
    hx3011_write_reg(0X26, leddrive_ps0);
    hx3011_write_reg(0X27, leddrive_ps1);
    hx3011_write_reg(0X28, leddrive_ps2);
    hx3011_write_reg(0X29, leddrive_ps3);
    hx3011_write_reg(0X2a, (ldrsel_ps0 << 4 | ldrsel_ps1));
    hx3011_write_reg(0X2b, (ldrsel_ps2 << 4 | ldrsel_ps3));
    hx3011_write_reg(0X2c, dccancel_ps0);
    hx3011_write_reg(0X2d, dccancel_ps1);
    hx3011_write_reg(0X2e, dccancel_ps2);
    hx3011_write_reg(0X2f, dccancel_ps3);
    hx3011_write_reg(0X36, (thres_int << 4 | data_rdy_int));
    hx3011_write_reg(0X4e, (force_adc_clk_sel << 7 | force_adc_clk_cfg << 6 | force_PEN << 5 | force_PEN_cfg << 4 | force_LED_EN << 3 |
                            force_LED_EN_cfg << 2 | force_CKAFEINT_sel << 1 | force_CKAFEINT_cfg));

    hx3011_write_reg(0X6d, 0x00);
    hx3011_write_reg(0XF0, 0x22);
    hx3011_write_reg(0xF1, 0x28);
    hx3011_write_reg(0xF2, 0x04);
    hx3011_write_reg(0xF3, 0x70);
    hx3011_write_reg(0xF4, 0x32);
    hx3011_write_reg(0xF5, 0x89);
    hx3011_write_reg(0xF6, 0x2b);
    hx3011_write_reg(0xF7, 0x85);
    hx3011_write_reg(0xF8, 0x27);
    hx3011_write_reg(0xF9, 0x00);
    hx3011_write_reg(0xFA, 0x88);
    hx3011_write_reg(0xFB, 0x40);
    hx3011_write_reg(0Xf6, 0X2b);

#if defined(TIMMER_MODE)
    hx3011_write_reg(0x3d, 0x83);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3011_write_reg(0x3e, 0x20);	//watermark
    hx3011_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3011_write_reg(0X37, 0x00);   // int sel,01=prf int,04=enable almost full int
    hx3011_write_reg(0X3b, 0x00);
    hx3011_write_reg(0X3b, 0x10);
#else ///////////INT Mode
    hx3011_write_reg(0x3d, 0x00);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3011_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3011_write_reg(0x37, 0x04);   // int sel,01=prf ,04=enable almost full
#endif

    hx3011_write_reg(0x4d, 0x01);   //soft reset
    hx3011_delay(5);
    hx3011_write_reg(0x4d, 0x00);   //release
    hx3011_delay(5);
    read_fifo_first_flg = 1;
}

void Init_hrs_PPG_Calibration_Routine(HRS_CAL_SET_T *calR)
{
    calR->flag = CAL_FLG_LED_DR | CAL_FLG_LED_DAC | CAL_FLG_AMB_DAC | CAL_FLG_RF;

    calR->led_idac   = 0;
    calR->amb_idac   = 0;
    calR->ontime     = 5;
    calR->led_cur    = HRS_CAL_INIT_LED;
    calR->state      = hrsCalStart;
    calR->int_cnt    = 0;
    calR->cur255_cnt = 0;
    if (low_vbat_flag == 1) {
        calR->led_max_cur = green_led_max_init >> 1;
    } else {
        calR->led_max_cur = green_led_max_init;
    }
}

void Restart_hrs_PPG_Calibration_Routine(HRS_CAL_SET_T *calR)
{
    calR->flag = CAL_FLG_LED_DAC;
    calR->state = hrsCalLed;
    calR->int_cnt = 0;
}

hx3011_hrs_wear_msg_code_t hx3011_hrs_wear_mode_check(int32_t infrared_data)
{
    int32_t check_thre = 0;
    if (hrs_wear_status == MSG_HRS_NO_WEAR) {
        check_thre = hrs_ir_wear_thres;
    } else {
        check_thre = hrs_ir_unwear_thres;
    }

    if (infrared_data < check_thre) {
        if (no_touch_cnt < NO_TOUCH_CHECK_NUM) {
            no_touch_cnt++;
        }
        if (no_touch_cnt >= NO_TOUCH_CHECK_NUM) {
            hrs_wear_status = MSG_HRS_NO_WEAR;
        }
    } else if (infrared_data >= check_thre) {
        if (no_touch_cnt > 0) {
            no_touch_cnt--;
        }
        if (no_touch_cnt == 0) {
            hrs_wear_status = MSG_HRS_WEAR;
        }
    }
    //AGC_LOG("hrs wear: ir=%d,  %x\r\n",infrared_data,hx3011_read_reg(0x6a));
    //AGC_LOG("hrs wearstates: hrs_wear_status_pre=%d,hrs_wear_status=%d\r\n",\
    //hrs_wear_status_pre,hrs_wear_status);

    // hrs_wear_status = hx3011_get_wear_result(hrs_wear_status);
    TYHX_LOG("[hx3011_hrs_wear_mode_check end] final result = %d\r\n", hrs_wear_status);
    if (hrs_wear_status_pre != hrs_wear_status) {
        hrs_wear_status_pre = hrs_wear_status;
        if (hrs_wear_status == MSG_HRS_NO_WEAR) {
            hx3011_hrs_low_power();
        } else if (hrs_wear_status == MSG_HRS_WEAR) {
            tyhx_hrs_alg_open_deep();
            hx3011_hrs_set_mode(PPG_INIT);
            hx3011_hrs_set_mode(CAL_INIT);
        }
    }
    return hrs_wear_status;
}

/*
hx3011_hrs_wear_msg_code_t hx3011_hrs_check_unwear(int32_t infrared_data)
{
    AGC_LOG("infrared_data=%d,hrs_wear_status=%d\r\n",infrared_data,hrs_wear_status);
    if(infrared_data < hrs_ir_unwear_thres)
    {
        if(no_touch_cnt == NO_TOUCH_CHECK_NUM)
        {
            hrs_wear_status = MSG_HRS_NO_WEAR;
            if(hrs_wear_status_pre != hrs_wear_status)
            {
                hx3011_hrs_low_power();
            }
        }
        else
        {
            no_touch_cnt++;
        }
    }
    else
    {
        no_touch_cnt = 0;
    }
    hrs_wear_status_pre = hrs_wear_status;
    return hrs_wear_status;
}
*/
void PPG_hrs_Calibration_Routine(HRS_CAL_SET_T *calR, int32_t led, int32_t amb)
{
    switch (calR->state) {
    case hrsCalStart:
        calR->state = hrsLedCur;
        break;

    case hrsLedCur:
        if (amb > 280000) {
            calR->amb_idac = (amb - 280000) / 10000;
        } else {
            calR->amb_idac = 0;
        }
        if (led > amb + 128) {
            calR->led_step = (led - amb) / HRS_CAL_INIT_LED;
            calR->led_cur = 5000 * (hx3011_hrs_agc_idac + calR->amb_idac) / calR->led_step;
            if (calR->led_cur > calR->led_max_cur) {
                calR->led_cur = calR->led_max_cur;
            }
            calR->led_idac = hx3011_hrs_agc_idac >> 1;
            calR->ontime = 64;
            calR->flag = CAL_FLG_LED_DR | CAL_FLG_LED_DAC | CAL_FLG_AMB_DAC | CAL_FLG_RF;
            calR->state = hrsCalLed;
        } else {
            calR->led_cur = calR->led_max_cur;
            calR->led_idac = hx3011_hrs_agc_idac >> 1;
            calR->ontime = 64;
            calR->flag = CAL_FLG_LED_DR | CAL_FLG_LED_DAC | CAL_FLG_AMB_DAC | CAL_FLG_RF;
            calR->state = hrsCalLed;
        }
        break;

    case hrsCalLed:
        if (led > 350000 && calR->led_idac <= 120) {
            calR->led_idac = calR->led_idac + 1;
            calR->state = hrsCalLed;
        } else if (led < 150000 && calR->led_idac >= 1) {
            calR->led_idac = calR->led_idac - 1;
            calR->state = hrsCalLed;
        } else {
            calR->state = hrsCalFinish;
        }
        calR->flag = CAL_FLG_LED_DAC;
        break;

    default:
        break;

    }
    AGC_LOG("AGC: led_drv=%d,ledDac=%d,ambDac=%d,ledstep=%d,ontime=%d,state=%d\r\n", \
            calR->led_cur, calR->led_idac, calR->amb_idac, calR->led_step, calR->ontime, calR->state);
}

HRS_CAL_SET_T PPG_hrs_agc(void)
{
    int32_t led_val, amb_val;
    calReg.work = false;
    if (!s_cal_state) {
        return  calReg;
    }

#ifdef INT_MODE
    calReg.int_cnt ++;
    if (calReg.int_cnt < 4) {
        return calReg;
    }
    calReg.int_cnt = 0;
    hx3011_gpioint_cfg(false);
#endif
    calReg.work = true;

    read_data_packet(agc_buf);
    led_val = agc_buf[2];
    amb_val = agc_buf[3];

    AGC_LOG("cal dat led_val=%d,amb_val=%d \r\n", led_val, amb_val);

    PPG_hrs_Calibration_Routine(&calReg, led_val, amb_val);

    if (calReg.state == hrsCalFinish) {
        hx3011_hrs_set_mode(CAL_OFF);
    } else {
        hx3011_hrs_updata_reg();
    }
#if defined(INT_MODE)
    hx3011_gpioint_cfg(true);
#endif

    return  calReg;
}

void hx3011_hrs_cal_init(void) // 20200615 ericy afe cali online
{
    uint16_t sample_rate = 100;                      /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate;      /*period in clk num, num = Fclk/fs */
    uint8_t thres_int = 0;   //thres int enable
    uint8_t data_rdy_int = 8;   //[3]:ps1_data2 [2]:ps1_data1 [1]:ps0_data2 [0]:ps0_data1

    hx3011_write_reg(0X18, (uint8_t)prf_clk_num);    // prf bit<7:0>6   default 0x00
    hx3011_write_reg(0X19, (uint8_t)(prf_clk_num >> 8)); // prf bit<15:8>  default 0x03
    hx3011_write_reg(0x3d, 0x00);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3011_write_reg(0X36, (thres_int << 4 | data_rdy_int)); //default 0x0f

    hx3011_write_reg(0X4d, 0X01);  //soft reset
    hx3011_delay(5);
    hx3011_write_reg(0X4d, 0X00);  //release
    hx3011_delay(5);             //Delay for reset time
}

void hx3011_hrs_cal_off(void) // 20200615 ericy afe cali offline
{
    uint16_t sample_rate = 25;                      /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate;      /*period in clk num, num = Fclk/fs */

    uint8_t thres_int = 0;   //thres int enable
    uint8_t data_rdy_int = 0;   //[3]:ps1_data2 [2]:ps1_data1 [1]:ps0_data2 [0]:ps0_data1

    hx3011_write_reg(0X18, (uint8_t)prf_clk_num);    // prf bit<7:0>6   default 0x00
    hx3011_write_reg(0X19, (uint8_t)(prf_clk_num >> 8)); // prf bit<15:8>  default 0x03

    hx3011_write_reg(0x3d, 0x83);     //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3011_write_reg(0X36, (thres_int << 4 | data_rdy_int)); //default 0x0f

    hx3011_write_reg(0X4d, 0X01);  //soft reset
    hx3011_delay(5);
    hx3011_write_reg(0X4d, 0X00);  //release
    hx3011_delay(5);             //Delay for reset time
    //hx3011_prox_init();
    read_fifo_first_flg = 1;
}

void hx3011_hrs_low_power(void)
{
    uint16_t sample_rate = 10;                  /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate; /*period in clk num, num = Fclk/fs */

    uint8_t ps3_cp_avg_num_sel = 0;
    uint8_t ps2_cp_avg_num_sel = 0;
    uint8_t ps1_cp_avg_num_sel = 0;
    uint8_t ps0_cp_avg_num_sel = 0;

    uint8_t ps3_avg_num_sel_i2c = 0;
    uint8_t ps2_avg_num_sel_i2c = 0;
    uint8_t ps1_avg_num_sel_i2c = 0;
    uint8_t ps0_avg_num_sel_i2c = 0;

    uint8_t ps0_led_en_i2c = 1;
    uint8_t ps1_led_en_i2c = 0;
    uint8_t ps2_led_en_i2c = 0;
    uint8_t ps3_led_en_i2c = 0;

    uint8_t dccancel_ps0 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps1 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps2 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps3 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA

    uint8_t leddrive_ps0 = 64;   //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 0;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 0;    //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 0;    //LEDDRIVE_PS3  0~255 = 0~200ma

    hx3011_write_reg(0X18, (uint8_t)prf_clk_num);    // prf bit<7:0>6   default 0x00
    hx3011_write_reg(0X19, (uint8_t)(prf_clk_num >> 8)); // prf bit<11:8>  default 0x03
    hx3011_write_reg(0X20, (ps3_cp_avg_num_sel << 6 | ps2_cp_avg_num_sel << 4 | ps1_cp_avg_num_sel << 2 | ps0_cp_avg_num_sel));
    hx3011_write_reg(0X21, (ps3_avg_num_sel_i2c << 6 | ps2_avg_num_sel_i2c << 4 | ps1_avg_num_sel_i2c << 2 | ps0_avg_num_sel_i2c));
    hx3011_write_reg(0X23, (ps0_led_en_i2c << 7 | ps1_led_en_i2c << 6 | ps2_led_en_i2c << 5 | ps3_led_en_i2c << 4 | 0x0f));

    hx3011_write_reg(0X26, leddrive_ps0);
    hx3011_write_reg(0X27, leddrive_ps1);
    hx3011_write_reg(0X28, leddrive_ps2);
    hx3011_write_reg(0X29, leddrive_ps3);

    hx3011_write_reg(0X2c, dccancel_ps0);
    hx3011_write_reg(0X2d, dccancel_ps1);
    hx3011_write_reg(0X2e, dccancel_ps2);
    hx3011_write_reg(0X2f, dccancel_ps3);

    hx3011_write_reg(0x4d, 0x01);  //soft reset
    hx3011_delay(5);
    hx3011_write_reg(0x4d, 0x00);  //release
    hx3011_delay(5);

    calReg.led_cur = 0;
    //hx3011_prox_init();
    read_fifo_first_flg = 1;
    AGC_LOG(" chip go to low power mode  \r\n");

}

void hx3011_hrs_updata_reg(void)
{
    if (calReg.flag & CAL_FLG_LED_DR) {
        hx3011_write_reg(0X28, calReg.led_cur);
    }

    if (calReg.flag & CAL_FLG_LED_DAC) {
        hx3011_write_reg(0X2e, (uint8_t)calReg.led_idac);
    }

    if (calReg.flag & CAL_FLG_AMB_DAC) {
        hx3011_write_reg(0X2f, (uint8_t)calReg.amb_idac);
    }

    if (calReg.flag & CAL_FLG_RF) {
        hx3011_write_reg(0X1e, calReg.ontime); //3918 p2
        hx3011_write_reg(0X1f, calReg.ontime); //3918 p3
    }
}

void hx3011_hrs_set_mode(uint8_t mode_cmd)
{
    switch (mode_cmd) {
    case PPG_INIT:
        hx3011_hrs_ppg_init();
#if defined(TIMMER_MODE)
        hx3011_ppg_timer_cfg(true);
#else
        hx3011_gpioint_cfg(true);
#endif
        s_ppg_state = 1;
        AGC_LOG("ppg init mode\r\n");
        break;

    case PPG_OFF:
        hx3011_ppg_off();
        s_ppg_state = 0;
        AGC_LOG("ppg off mode\r\n");
        break;

    case PPG_LED_OFF:
        hx3011_hrs_low_power();
        s_ppg_state = 0;
        AGC_LOG("ppg led off mode\r\n");
        break;

    case CAL_INIT:
        Init_hrs_PPG_Calibration_Routine(&calReg);
        hx3011_hrs_cal_init();
        hx3011_hrs_updata_reg();
#if defined(TIMMER_MODE)
        hx3011_agc_timer_cfg(true);
#endif
        s_cal_state = 1;
        AGC_LOG("cal init mode\r\n");
        break;

    case RECAL_INIT:
        Restart_hrs_PPG_Calibration_Routine(&calReg);
        hx3011_hrs_cal_init();
        hx3011_hrs_updata_reg();
#if defined(TIMMER_MODE)
        hx3011_agc_timer_cfg(true);
#endif
        s_cal_state = 1;
        AGC_LOG("Recal init mode\r\n");
        break;

    case CAL_OFF:
#if defined(TIMMER_MODE)
        hx3011_agc_timer_cfg(false);
#endif
        hx3011_hrs_cal_off();
        s_cal_state = 0;
        AGC_LOG("cal off mode\r\n");
        break;

    default:
        break;
    }
}

SENSOR_ERROR_T hx3011_hrs_enable(void)
{
    if (!hx3011_chip_check()) {
        TYHX_LOG("hx3918 check id failed!\r\n");
        return SENSOR_OP_FAILED;
    } else {
        TYHX_LOG("hx3918 check id success!\r\n");
    }
    if (s_ppg_state) {
        TYHX_LOG("ppg already on!\r\n");
        return SENSOR_OP_FAILED;
    }
    if (!tyhx_hrs_alg_open()) {
        TYHX_LOG("hrs alg open fail,or dynamic ram not enough!\r\n");
    }

    hrs_wear_status = MSG_HRS_INIT;
    hrs_wear_status_pre = MSG_HRS_INIT;
    hx3011_hrs_set_mode(PPG_INIT);
    TYHX_LOG("hx3918 enable!\r\n");

    return SENSOR_OK;
}

void hx3011_hrs_disable(void)
{
#if defined(TIMMER_MODE)
    hx3011_ppg_timer_cfg(false);
    hx3011_agc_timer_cfg(false);
#elif defined(INT_MODE)
    hx3011_gpioint_cfg(false);
#endif
    hx3011_hrs_set_mode(PPG_OFF);
    s_ppg_state = 0;
    s_cal_state = 0;
    tyhx_hrs_alg_close();
}

void hx3011_hrs_data_reset(void)
{
    s_ppg_state = 0;
    s_cal_state = 0;
    tyhx_hrs_alg_close();
}

hx3011_hrs_wear_msg_code_t hx3011_hrs_get_wear_status(void)
{
    return  hrs_wear_status;
}

HRS_CAL_SET_T get_hrs_agc_status(void)
{
    HRS_CAL_SET_T cal;
    cal.flag     = calReg.flag;
    cal.int_cnt  = calReg.int_cnt;
    cal.led_cur  = calReg.led_cur;
    cal.led_idac = calReg.led_idac;
    cal.amb_idac = calReg.amb_idac;
    cal.ontime   = calReg.ontime;
    cal.led_step = calReg.led_step;
    cal.state    = calReg.state;
    return cal;
}

uint8_t hx3011_hrs_read(ppg_sensor_data_t *s_dat)
{
    int32_t PPG_src_data;
    int32_t Ir_src_data;
    bool recal = false;
    int32_t *PPG_buf =  &(s_dat->green_data[0]);
    int32_t *ir_buf =  &(s_dat->ir_data[0]);
    int32_t *s_buf =  &(s_dat->s_buf[0]);
    s_dat->green_cur =  calReg.led_cur;
    //int32_t *sar_lp_data  = &s_dat->sar_data;
    int32_t ps_data[4] = {0};
    int32_t ps_green_data = 0;
    uint8_t data_count = 0;

    if (!s_ppg_state || s_cal_state) {
        return 0;
    }

    read_data_packet(ps_data);
    ps_green_data = ps_data[2];

    data_count = hx3011_read_fifo_data(s_buf, 2, 1);
    s_dat->count =  data_count;
    TYHX_LOG("\r\n hrs  ir data:	");
    for (uint8_t i = 0; i < data_count; i++) {
        PPG_src_data = s_buf[i * 2 + 1];
        Ir_src_data = s_buf[i * 2];
        PPG_buf[i] = 2 * PPG_src_data;
        ir_buf[i] = Ir_src_data;
        TYHX_LOG("%d	", ir_buf[i]);
        //DEBUG_PRINTF("ppg data size: %d\r\n", Ir_src_data);
        hx3011_hrs_wear_mode_check(Ir_src_data);

        //DEBUG_PRINTF("%d/%d %d %d\r\n",1+i,data_count,PPG_buf[i],ir_buf[i]);
    }
    TYHX_LOG("\r\n");

    if (ps_green_data < 100000 || ps_green_data > 400000) {
        recal = true;

        if (hrs_wear_status == MSG_HRS_NO_WEAR) {
            recal = false;
        }
    }
    if (recal) {
        cal_delay--;

        if (cal_delay <= 0) {
            cal_delay = CAL_DELAY_COUNT;
            hx3011_hrs_set_mode(RECAL_INIT);
        }
    } else {
        cal_delay = CAL_DELAY_COUNT;
    }
    return 1;
}
#endif

#endif // TCFG_HX3011_ENABLE



