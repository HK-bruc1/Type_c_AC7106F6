#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

//#include "twi_master.h"
//#include "hx3918.h"
//#include "SEGGER_RTT.h"

//#include "tyhx_hrs_custom.h"
#include "hx3918.h"
#include "hx3918_check_touch.h"
#ifdef SAR_ALG_LIB
#include "hx3918_prox.h"
#endif
//#include "segger_rtt.h"

#if TCFG_HX3918_ENABLE
hx3918_wear_msg_code_t hx3918_wear_status = MSG_NO_WEAR;
hx3918_wear_msg_code_t hx3918_wear_status_pre = MSG_NO_WEAR;
extern const int32_t check_mode_unwear_thre;
extern const int32_t check_mode_wear_thre;
extern const int32_t check_mode_sar_thre;
uint8_t notouch_cnt = 0;
uint8_t touch_cnt = 0;
extern uint8_t read_fifo_first_flg;

#ifdef CHECK_TOUCH_LIB
void hx3918_check_touch_init(void) //20200615 ericy ppg fs=25hz, phase3 conversion ready interupt en
{
    uint16_t sample_rate = 5;                  /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate; /*period in clk num, num = Fclk/fs */

#ifdef IR_THRES_INT

    uint8_t ps0_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps1_enable = 0;     /*ps1_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps2_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps3_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */

    uint8_t ps0_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps1_osr = 0;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps2_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps3_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */

    uint8_t ps0_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps1_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps2_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps3_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/

    uint8_t leddrive_ps0 = 0;    //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 0;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 64;   //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 0;    //LEDDRIVE_PS3  0~255 = 0~200ma

    uint8_t ldrsel_ps0 = 0;
    uint8_t ldrsel_ps1 = 0;
    uint8_t ldrsel_ps2 = IR_LED_SLE;
    uint8_t ldrsel_ps3 = 0;

    uint8_t ps0_led_en_i2c = 0;
    uint8_t ps1_led_en_i2c = 0;
    uint8_t ps2_led_en_i2c = 1;
    uint8_t ps3_led_en_i2c = 0;

#else

    uint8_t ps0_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps1_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps2_enable = 0;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps3_enable = 0;     /*ps1_enable  , 1 mean enable ; 0 mean disable */

    uint8_t ps0_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps1_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps2_osr = 0;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps3_osr = 0;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */


    uint8_t ps0_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps1_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps2_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps3_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/

    uint8_t leddrive_ps0 = 64;    //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 0;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 0;   //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 0;    //LEDDRIVE_PS3  0~255 = 0~200ma

    uint8_t ldrsel_ps0 = IR_LED_SLE;
    uint8_t ldrsel_ps1 = 0;
    uint8_t ldrsel_ps2 = 0;
    uint8_t ldrsel_ps3 = 0;


    uint8_t ps0_led_en_i2c = 1;
    uint8_t ps1_led_en_i2c = 0;
    uint8_t ps2_led_en_i2c = 0;
    uint8_t ps3_led_en_i2c = 0;


#endif

    uint8_t ps3_cp_avg_num_sel = 0;
    uint8_t ps2_cp_avg_num_sel = 0;
    uint8_t ps1_cp_avg_num_sel = 0;
    uint8_t ps0_cp_avg_num_sel = 0;

    uint8_t ps3_avg_num_sel_i2c = 0;
    uint8_t ps2_avg_num_sel_i2c = 0;
    uint8_t ps1_avg_num_sel_i2c = 0;
    uint8_t ps0_avg_num_sel_i2c = 0;

    /***********led open enable***********/

    uint8_t ps0_ofidac_en_i2c = 1;
    uint8_t ps1_ofidac_en_i2c = 1;
    uint8_t ps2_ofidac_en_i2c = 1;
    uint8_t ps3_ofidac_en_i2c = 1;

    uint8_t dccancel_ps0 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps1 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps2 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps3 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA

    uint8_t led_en_begin = 1;      // 0 = 2 ; 1 = 4 ; 2 = 8 ; 3 = 16 ;
    uint8_t afe_reset = 1;        //* 0 = 32clk ; 1 = 64clk ; 2 = 128clk ; 3 = 256clk(d) ;
    uint8_t en2rst_delay = 3;     // pen 2 rst delay 0 = 128,256, 512, 1024
    uint8_t init_wait_delay = 2; /* 0 = 800 ; 1 = 1600; 2 = 3200 ; 3 = 6400(d) */

    uint8_t ps1_interval_i2c = 0;   // ps2/ps3

    uint8_t thres_int = 1;   //thres int enable
    uint8_t data_rdy_int = 8;   //[3]:ps1_data2 [2]:ps1_data1 [1]:ps0_data2 [0]:ps0_data1

    /***********cap *********/
    uint8_t intcapsel_ps3 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps2 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps1 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps0 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf

    uint8_t force_adc_clk_sel = 0;
    uint8_t force_adc_clk_cfg = 0;
    uint8_t force_PEN = 0;    //phase enable
    uint8_t force_PEN_cfg = 0;
    uint8_t force_LED_EN = 0;
    uint8_t force_LED_EN_cfg = 0;
    uint8_t force_CKAFEINT_sel = 0;
    uint8_t force_CKAFEINT_cfg = 0;
#ifdef SAR_ALG_LIB
    hx3918_set_cap_data_ready(0);
#endif
    hx3918_write_reg(0X16, (ps3_enable << 3 | ps2_enable << 2 | ps1_enable << 1 | ps0_enable));
    hx3918_write_reg(0X17, (ps3_osr << 6 | ps2_osr << 4 | ps1_osr << 2 | ps0_osr));
    hx3918_write_reg(0X18, (uint8_t)prf_clk_num);
    hx3918_write_reg(0X19, (uint8_t)(prf_clk_num >> 8));
    hx3918_write_reg(0X1a, (ps1_interval_i2c));
    hx3918_write_reg(0X1b, led_en_begin << 6 | afe_reset << 4 | en2rst_delay << 2 | init_wait_delay);
    //hx3918_write_reg(0X1b, 0x03);
    hx3918_write_reg(0X1c, ps0_led_on_time);
    hx3918_write_reg(0X1d, ps1_led_on_time);
    hx3918_write_reg(0X1e, ps2_led_on_time);
    hx3918_write_reg(0X1f, ps3_led_on_time);
    hx3918_write_reg(0X20, (ps3_cp_avg_num_sel << 6 | ps2_cp_avg_num_sel << 4 | ps1_cp_avg_num_sel << 2 | ps0_cp_avg_num_sel));
    hx3918_write_reg(0X21, (ps3_avg_num_sel_i2c << 6 | ps2_avg_num_sel_i2c << 4 | ps1_avg_num_sel_i2c << 2 | ps0_avg_num_sel_i2c));
    hx3918_write_reg(0X23, (ps0_led_en_i2c << 7 | ps1_led_en_i2c << 6 | ps2_led_en_i2c << 5 | ps3_led_en_i2c << 4 | ps0_ofidac_en_i2c << 3 | ps1_ofidac_en_i2c << 2 | ps2_ofidac_en_i2c << 1 | ps3_ofidac_en_i2c));
    hx3918_write_reg(0X24, (intcapsel_ps0 << 4 | intcapsel_ps1));
    hx3918_write_reg(0X25, (intcapsel_ps2 << 4 | intcapsel_ps3));
    hx3918_write_reg(0X26, leddrive_ps0);
    hx3918_write_reg(0X27, leddrive_ps1);
    hx3918_write_reg(0X28, leddrive_ps2);
    hx3918_write_reg(0X29, leddrive_ps3);
    hx3918_write_reg(0X2a, (ldrsel_ps0 << 4 | ldrsel_ps1));
    hx3918_write_reg(0X2b, (ldrsel_ps2 << 4 | ldrsel_ps3));
    hx3918_write_reg(0X2c, dccancel_ps0);
    hx3918_write_reg(0X2d, dccancel_ps1);
    hx3918_write_reg(0X2e, dccancel_ps2);
    hx3918_write_reg(0X2f, dccancel_ps3);
    hx3918_write_reg(0X36, (thres_int << 4 | data_rdy_int));
    hx3918_write_reg(0X4e, (force_adc_clk_sel << 7 | force_adc_clk_cfg << 6 | force_PEN << 5 | force_PEN_cfg << 4 | force_LED_EN << 3 |
                            force_LED_EN_cfg << 2 | force_CKAFEINT_sel << 1 | force_CKAFEINT_cfg));

    hx3918_write_reg(0X6d, 0x00);
    hx3918_write_reg(0XF0, 0x22);
    hx3918_write_reg(0xF1, 0x28);
    hx3918_write_reg(0xF2, 0x04);
    hx3918_write_reg(0xF3, 0x70);
    hx3918_write_reg(0xF4, 0x32);
    hx3918_write_reg(0xF5, 0x89);
    hx3918_write_reg(0xF6, 0x2b);
    hx3918_write_reg(0xF7, 0x85);
    hx3918_write_reg(0xF8, 0x27);
    hx3918_write_reg(0xF9, 0x00);
    hx3918_write_reg(0xFA, 0x88);
    hx3918_write_reg(0xFB, 0x40);
    hx3918_write_reg(0Xf6, 0X2b);

#if defined(TIMMER_MODE)

    hx3918_write_reg(0x3d, 0x83);   //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3918_write_reg(0x3e, 0x20);	//watermark
    hx3918_write_reg(0x3c, 0x1f);   // int_width_i2c
    hx3918_write_reg(0X37, 0x00);   // int sel,01=prf int,04=enable almost full int
    hx3918_write_reg(0X3b, 0x00);
    hx3918_write_reg(0X3b, 0x10);

#elif  defined(INT_MODE)///////////FIFO almost full mode
////////////////FIFO almost full mode/////////////////
    hx3918_write_reg(0x3d, 0x83);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3918_write_reg(0x3e, 0x12);	//watermark
    hx3918_write_reg(0X3b, 0x1C);
    hx3918_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3918_write_reg(0X37, 0x04);   // int sel,01=prf ,04=enable almost full
    hx3918_write_reg(0X3b, 0x18);
    hx3918_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3918_write_reg(0X37, 0x04);    // int sel,01=prf ,04=enable almost full

#else defined(PPG_INT_MODE)

    hx3918_write_reg(0x3d, 0x00);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3918_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3918_write_reg(0x37, 0x00);   // int sel,01=prf ,04=enable almost full
    hx3918_write_reg(0X3b, 0x1C);

#endif

#ifdef IR_THRES_INT

    hx3918_write_reg(0x3d, 0x00);
    hx3918_write_reg(0X37, 0x01);   // int sel,01=prf ,04=enable almost full
    hx3918_write_reg(0X36, 0x10);
    hx3918_write_reg(0X3b, 0x18);
    hx3918_write_reg(0X38, 0x02);
    hx3918_write_reg(0X39, (check_mode_wear_thre >> 4) & 0xFF); //ps_thres_high_i2c[7:0]   /16
    hx3918_write_reg(0X3a, (check_mode_wear_thre >> 12) & 0xFF); //ps_thres_high_i2c[15:8]
    hx3918_write_reg(0X7c, (check_mode_unwear_thre >> 4) & 0xFF); //ps_thres_low_i2c[7:0]
    hx3918_write_reg(0X7d, (check_mode_unwear_thre >> 12) & 0xFF); //ps_thres_low_i2c[15:8]

#endif

    hx3918_write_reg(0X4d, 0X01);   //soft reset
    hx3918_delay(5);
    hx3918_write_reg(0X4d, 0X00);   //release
    hx3918_delay(5);

#ifdef SAR_ALG_LIB
    hx3918_prox_reg_init();
#endif
    read_fifo_first_flg = 1;
}
void hx3917_check_touch_init(void) //20200615 ericy ppg fs=25hz, phase3 conversion ready interupt en
{
    uint16_t sample_rate = 5;                  /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate; /*period in clk num, num = Fclk/fs */

#ifdef IR_THRES_INT

    uint8_t ps0_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps1_enable = 0;     /*ps1_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps2_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps3_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */

    uint8_t ps0_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps1_osr = 0;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps2_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps3_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */

    uint8_t ps0_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps1_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps2_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps3_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                 64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/

    uint8_t leddrive_ps0 = 0;    //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 0;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 64;   //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 0;    //LEDDRIVE_PS3  0~255 = 0~200ma

    uint8_t ldrsel_ps0 = 0;
    uint8_t ldrsel_ps1 = 0;
    uint8_t ldrsel_ps2 = IR_LED_SLE;
    uint8_t ldrsel_ps3 = 0;

    uint8_t ps0_led_en_i2c = 0;
    uint8_t ps1_led_en_i2c = 0;
    uint8_t ps2_led_en_i2c = 1;
    uint8_t ps3_led_en_i2c = 0;

#else

    uint8_t ps0_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps1_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps2_enable = 0;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps3_enable = 0;     /*ps1_enable  , 1 mean enable ; 0 mean disable */

    uint8_t ps0_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps1_osr = 1;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps2_osr = 0;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps3_osr = 0;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */


    uint8_t ps0_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps1_led_on_time = 8;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps2_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps3_led_on_time = 0;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                         64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/

    uint8_t leddrive_ps0 = 0;    //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 64;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 0;   //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 0;    //LEDDRIVE_PS3  0~255 = 0~200ma

    uint8_t ldrsel_ps0 = 0;
    uint8_t ldrsel_ps1 = IR_LED_SLE;
    uint8_t ldrsel_ps2 = 0;
    uint8_t ldrsel_ps3 = 0;


    uint8_t ps0_led_en_i2c = 0;
    uint8_t ps1_led_en_i2c = 1;
    uint8_t ps2_led_en_i2c = 0;
    uint8_t ps3_led_en_i2c = 0;


#endif

    uint8_t ps3_cp_avg_num_sel = 0;
    uint8_t ps2_cp_avg_num_sel = 0;
    uint8_t ps1_cp_avg_num_sel = 0;
    uint8_t ps0_cp_avg_num_sel = 0;

    uint8_t ps3_avg_num_sel_i2c = 0;
    uint8_t ps2_avg_num_sel_i2c = 0;
    uint8_t ps1_avg_num_sel_i2c = 0;
    uint8_t ps0_avg_num_sel_i2c = 0;

    /***********led open enable***********/

    uint8_t ps0_ofidac_en_i2c = 1;
    uint8_t ps1_ofidac_en_i2c = 1;
    uint8_t ps2_ofidac_en_i2c = 1;
    uint8_t ps3_ofidac_en_i2c = 1;

    uint8_t dccancel_ps0 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps1 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps2 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps3 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA

    uint8_t led_en_begin = 1;      // 0 = 2 ; 1 = 4 ; 2 = 8 ; 3 = 16 ;
    uint8_t afe_reset = 1;        //* 0 = 32clk ; 1 = 64clk ; 2 = 128clk ; 3 = 256clk(d) ;
    uint8_t en2rst_delay = 3;     // pen 2 rst delay 0 = 128,256, 512, 1024
    uint8_t init_wait_delay = 2; /* 0 = 800 ; 1 = 1600; 2 = 3200 ; 3 = 6400(d) */

    uint8_t ps1_interval_i2c = 0;   // ps2/ps3

    uint8_t thres_int = 1;   //thres int enable
    uint8_t data_rdy_int = 8;   //[3]:ps1_data2 [2]:ps1_data1 [1]:ps0_data2 [0]:ps0_data1

    /***********cap *********/
    uint8_t intcapsel_ps3 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps2 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps1 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps0 = 2;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf

    uint8_t force_adc_clk_sel = 0;
    uint8_t force_adc_clk_cfg = 0;
    uint8_t force_PEN = 0;    //phase enable
    uint8_t force_PEN_cfg = 0;
    uint8_t force_LED_EN = 0;
    uint8_t force_LED_EN_cfg = 0;
    uint8_t force_CKAFEINT_sel = 0;
    uint8_t force_CKAFEINT_cfg = 0;
#ifdef SAR_ALG_LIB
    hx3918_set_cap_data_ready(0);
#endif
    hx3918_write_reg(0X16, (ps3_enable << 3 | ps2_enable << 2 | ps1_enable << 1 | ps0_enable));
    hx3918_write_reg(0X17, (ps3_osr << 6 | ps2_osr << 4 | ps1_osr << 2 | ps0_osr));
    hx3918_write_reg(0X18, (uint8_t)prf_clk_num);
    hx3918_write_reg(0X19, (uint8_t)(prf_clk_num >> 8));
    hx3918_write_reg(0X1a, (ps1_interval_i2c));
    hx3918_write_reg(0X1b, led_en_begin << 6 | afe_reset << 4 | en2rst_delay << 2 | init_wait_delay);
    //hx3918_write_reg(0X1b, 0x03);
    hx3918_write_reg(0X1c, ps0_led_on_time);
    hx3918_write_reg(0X1d, ps1_led_on_time);
    hx3918_write_reg(0X1e, ps2_led_on_time);
    hx3918_write_reg(0X1f, ps3_led_on_time);
    hx3918_write_reg(0X20, (ps3_cp_avg_num_sel << 6 | ps2_cp_avg_num_sel << 4 | ps1_cp_avg_num_sel << 2 | ps0_cp_avg_num_sel));
    hx3918_write_reg(0X21, (ps3_avg_num_sel_i2c << 6 | ps2_avg_num_sel_i2c << 4 | ps1_avg_num_sel_i2c << 2 | ps0_avg_num_sel_i2c));
    hx3918_write_reg(0X23, (ps0_led_en_i2c << 7 | ps1_led_en_i2c << 6 | ps2_led_en_i2c << 5 | ps3_led_en_i2c << 4 | ps0_ofidac_en_i2c << 3 | ps1_ofidac_en_i2c << 2 | ps2_ofidac_en_i2c << 1 | ps3_ofidac_en_i2c));
    hx3918_write_reg(0X24, (intcapsel_ps0 << 4 | intcapsel_ps1));
    hx3918_write_reg(0X25, (intcapsel_ps2 << 4 | intcapsel_ps3));
    hx3918_write_reg(0X26, leddrive_ps0);
    hx3918_write_reg(0X27, leddrive_ps1);
    hx3918_write_reg(0X28, leddrive_ps2);
    hx3918_write_reg(0X29, leddrive_ps3);
    hx3918_write_reg(0X2a, (ldrsel_ps0 << 4 | ldrsel_ps1));
    hx3918_write_reg(0X2b, (ldrsel_ps2 << 4 | ldrsel_ps3));
    hx3918_write_reg(0X2c, dccancel_ps0);
    hx3918_write_reg(0X2d, dccancel_ps1);
    hx3918_write_reg(0X2e, dccancel_ps2);
    hx3918_write_reg(0X2f, dccancel_ps3);
    hx3918_write_reg(0X36, (thres_int << 4 | data_rdy_int));
    hx3918_write_reg(0X4e, (force_adc_clk_sel << 7 | force_adc_clk_cfg << 6 | force_PEN << 5 | force_PEN_cfg << 4 | force_LED_EN << 3 |
                            force_LED_EN_cfg << 2 | force_CKAFEINT_sel << 1 | force_CKAFEINT_cfg));

    hx3918_write_reg(0X6d, 0x00);
    hx3918_write_reg(0XF0, 0x22);
    hx3918_write_reg(0xF1, 0x28);
    hx3918_write_reg(0xF2, 0x04);
    hx3918_write_reg(0xF3, 0x70);
    hx3918_write_reg(0xF4, 0x32);
    hx3918_write_reg(0xF5, 0x89);
    hx3918_write_reg(0xF6, 0x2b);
    hx3918_write_reg(0xF7, 0x85);
    hx3918_write_reg(0xF8, 0x27);
    hx3918_write_reg(0xF9, 0x00);
    hx3918_write_reg(0xFA, 0x88);
    hx3918_write_reg(0xFB, 0x40);
    hx3918_write_reg(0Xf6, 0X2b);

#if defined(TIMMER_MODE)

    hx3918_write_reg(0x3d, 0x81);   //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3918_write_reg(0x3e, 0x20);	//watermark
    hx3918_write_reg(0x3c, 0x1f);   // int_width_i2c
    hx3918_write_reg(0X37, 0x00);   // int sel,01=prf int,04=enable almost full int
    hx3918_write_reg(0X3b, 0x00);
    hx3918_write_reg(0X3b, 0x10);

#elif  defined(INT_MODE)///////////FIFO almost full mode
////////////////FIFO almost full mode/////////////////
    hx3918_write_reg(0x3d, 0x81);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3918_write_reg(0x3e, 0x12);	//watermark
    hx3918_write_reg(0X3b, 0x1C);
    hx3918_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3918_write_reg(0X37, 0x04);   // int sel,01=prf ,04=enable almost full
    hx3918_write_reg(0X3b, 0x18);
    hx3918_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3918_write_reg(0X37, 0x04);    // int sel,01=prf ,04=enable almost full

#else defined(PPG_INT_MODE)

    hx3918_write_reg(0x3d, 0x00);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3918_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3918_write_reg(0x37, 0x00);   // int sel,01=prf ,04=enable almost full
    hx3918_write_reg(0X3b, 0x1C);

#endif

#ifdef IR_THRES_INT

    hx3918_write_reg(0x3d, 0x00);
    hx3918_write_reg(0X37, 0x01);   // int sel,01=prf ,04=enable almost full
    hx3918_write_reg(0X36, 0x10);
    hx3918_write_reg(0X3b, 0x18);
    hx3918_write_reg(0X38, 0x02);
    hx3918_write_reg(0X39, (check_mode_wear_thre >> 4) & 0xFF); //ps_thres_high_i2c[7:0]   /16
    hx3918_write_reg(0X3a, (check_mode_wear_thre >> 12) & 0xFF); //ps_thres_high_i2c[15:8]
    hx3918_write_reg(0X7c, (check_mode_unwear_thre >> 4) & 0xFF); //ps_thres_low_i2c[7:0]
    hx3918_write_reg(0X7d, (check_mode_unwear_thre >> 12) & 0xFF); //ps_thres_low_i2c[15:8]

#endif

    hx3918_write_reg(0X4d, 0X01);   //soft reset
    hx3918_delay(5);
    hx3918_write_reg(0X4d, 0X00);   //release
    hx3918_delay(5);

#ifdef SAR_ALG_LIB
    hx3918_prox_reg_init();
#endif
    read_fifo_first_flg = 1;
}
void hx3918_check_touch_open(void)
{
    notouch_cnt = 0;
    touch_cnt = 0;
    hx3918_wear_status = MSG_NO_WEAR;
}

SENSOR_ERROR_T hx3918_check_touch_enable(void)
{
    if (!hx3918_chip_check()) {
        AGC_LOG("check id failed!\r\n");
        return SENSOR_OP_FAILED;
    }
    AGC_LOG(" check id success!\r\n");

    hx3918_check_touch_open();
    if (hx3918_chip_id == HX3918_ID) {
        hx3918_check_touch_init();
    } else {
        hx3917_check_touch_init();
    }
#if defined(INT_MODE) || defined(IR_THRES_INT)
    hx3918_gpioint_cfg(true);
#else
    hx3918_ppg_timer_cfg(true);
#endif
    return SENSOR_OK;
}

hx3918_wear_msg_code_t hx3918_check_touch(int32_t *ir_data, uint8_t data_len)
{
    uint8_t ii;
    int16_t sar_data = 32767;
#ifdef SAR_ALG_LIB
    sar_data = hx90xx_get_prox_data();
#endif
    for (ii = 0; ii < data_len; ii++) {
        TYHX_LOG("%d	", ir_data[ii]);
        if (ir_data[ii] > check_mode_wear_thre && sar_data > sar_wear_thres) {
            if (notouch_cnt > 0) {
                notouch_cnt--;
            }
            if (touch_cnt >= 5) {
                hx3918_wear_status = MSG_WEAR;
                touch_cnt = 5;
            } else {
                touch_cnt++;
            }
        } else if (ir_data[ii] < check_mode_unwear_thre || sar_data < sar_unwear_thres) {
            if (touch_cnt > 0) {
                touch_cnt--;
            }
            if (notouch_cnt >= 5) {
                hx3918_wear_status = MSG_NO_WEAR;
                notouch_cnt = 5;
            } else {
                notouch_cnt++;
            }
        }
    }

#ifdef SAR_ALG_LIB
    if (hx3918_wear_status == MSG_NO_WEAR) {
        hx3918_cali_with_ir(ir_data[0], check_mode_unwear_thre);
    }
#endif

    TYHX_LOG("\r\n");
    return hx3918_wear_status;
}

uint8_t hx3918_check_touch_read(ppg_sensor_data_t *s_dat)
{
    int32_t *s_buf =  &(s_dat->s_buf[0]);
    //int32_t *sar_lp_data  = &s_dat->sar_data;
    uint8_t data_count = 0;

    data_count = hx3918_read_fifo_data(s_buf, 1, 1);
    s_dat->count =  data_count;

    return data_count;
}

#endif // CHECK_TOUCH_LIB
#endif // TCFG_HX3918_ENABLE
