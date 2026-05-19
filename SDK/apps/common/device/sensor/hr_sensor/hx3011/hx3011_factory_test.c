#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "hx3011.h"
#include "hx3011_factory_test.h"

#if TCFG_HX3011_ENABLE

void hx3011_factory_ft_card_test_config(void)//灰卡10mm高度测试配置
{
    uint16_t sample_rate = 25;                  /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate; /*period in clk num, num = Fclk/fs */

    uint8_t ps0_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps1_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps2_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps3_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */

    uint8_t ps0_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps1_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps2_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps3_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */

    uint8_t ps3_cp_avg_num_sel = 0;
    uint8_t ps2_cp_avg_num_sel = 0;
    uint8_t ps1_cp_avg_num_sel = 0;
    uint8_t ps0_cp_avg_num_sel = 0;

    uint8_t ps3_avg_num_sel_i2c = 0;
    uint8_t ps2_avg_num_sel_i2c = 0;
    uint8_t ps1_avg_num_sel_i2c = 0;
    uint8_t ps0_avg_num_sel_i2c = 0;

    /***********led open enable***********/

    uint8_t ps0_led_en_i2c = 1;
    uint8_t ps1_led_en_i2c = 1;
    uint8_t ps2_led_en_i2c = 1;
    uint8_t ps3_led_en_i2c = 1;

    uint8_t ps0_ofidac_en_i2c = 1;
    uint8_t ps1_ofidac_en_i2c = 1;
    uint8_t ps2_ofidac_en_i2c = 1;
    uint8_t ps3_ofidac_en_i2c = 1;

    uint8_t dccancel_ps0 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps1 = 0;  //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps2 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps3 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA

    uint8_t leddrive_ps0 = 0;    //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 32;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 32;   //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 32;    //LEDDRIVE_PS3  0~255 = 0~200ma

    uint8_t led_en_begin = 1;      // 0 = 2 ; 1 = 4 ; 2 = 8 ; 3 = 16 ;
    uint8_t afe_reset = 1;        //* 0 = 32clk ; 1 = 64clk ; 2 = 128clk ; 3 = 256clk(d) ;
    uint8_t en2rst_delay = 3;     // pen 2 rst delay 0 = 128,256, 512, 1024
    uint8_t init_wait_delay = 2; /* 0 = 800 ; 1 = 1600; 2 = 3200 ; 3 = 6400(d) */

    uint8_t ps1_interval_i2c = 0;   // ps2/ps3

    uint8_t thres_int = 1;   //thres int enable
    uint8_t data_rdy_int = 8;   //[3]:ps1_data2 [2]:ps1_data1 [1]:ps0_data2 [0]:ps0_data1


    uint8_t ldrsel_ps0 = 0;
    uint8_t ldrsel_ps1 = GREEN_LED_SLE;
    uint8_t ldrsel_ps2 = RED_LED_SLE;
    uint8_t ldrsel_ps3 = IR_LED_SLE;

    /***********cap *********/
    uint8_t intcapsel_ps3 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps2 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps1 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps0 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf

    uint8_t ps0_led_on_time = 32;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps1_led_on_time = 32;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps2_led_on_time = 32;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps3_led_on_time = 32;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
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

    hx3011_write_reg(0x3d, 0x00);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3011_write_reg(0x3e, 0x20);	//watermark
    hx3011_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3011_write_reg(0X37, 0x00);   // int sel,01=prf int,04=enable almost full int
    hx3011_write_reg(0X3b, 0x00);
    hx3011_write_reg(0X3b, 0x10);

    hx3011_write_reg(0x4d, 0x01);   //soft reset
    hx3011_delay(5);
    hx3011_write_reg(0x4d, 0x00);   //release
    hx3011_delay(5);
}

void hx3011_factory_ft_leak_light_test_config(void)//漏光测试配置
{
    uint16_t sample_rate = 25;                  /*config the data rate of chip alps2_fm ,uint is Hz*/
    uint32_t prf_clk_num = 32000 / sample_rate; /*period in clk num, num = Fclk/fs */

    uint8_t ps0_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps1_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps2_enable = 1;     /*ps0_enable  , 1 mean enable ; 0 mean disable */
    uint8_t ps3_enable = 1;     /*ps1_enable  , 1 mean enable ; 0 mean disable */

    uint8_t ps0_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps1_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps2_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */
    uint8_t ps3_osr = 3;    /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 */

    uint8_t ps3_cp_avg_num_sel = 0;
    uint8_t ps2_cp_avg_num_sel = 0;
    uint8_t ps1_cp_avg_num_sel = 0;
    uint8_t ps0_cp_avg_num_sel = 0;

    uint8_t ps3_avg_num_sel_i2c = 0;
    uint8_t ps2_avg_num_sel_i2c = 0;
    uint8_t ps1_avg_num_sel_i2c = 0;
    uint8_t ps0_avg_num_sel_i2c = 0;

    /***********led open enable***********/

    uint8_t ps0_led_en_i2c = 1;
    uint8_t ps1_led_en_i2c = 1;
    uint8_t ps2_led_en_i2c = 1;
    uint8_t ps3_led_en_i2c = 1;

    uint8_t ps0_ofidac_en_i2c = 1;
    uint8_t ps1_ofidac_en_i2c = 1;
    uint8_t ps2_ofidac_en_i2c = 1;
    uint8_t ps3_ofidac_en_i2c = 1;

    uint8_t dccancel_ps0 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps1 = 0;  //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps2 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA
    uint8_t dccancel_ps3 = 0;   //offset idac   BIT[9:0]  0 31.25nA, 1023 32uA, step 31.25nA

    uint8_t leddrive_ps0 = 0;    //LEDDRIVE_PS0  0~255 = 0~200ma
    uint8_t leddrive_ps1 = 64;    //LEDDRIVE_PS1  0~255 = 0~200ma
    uint8_t leddrive_ps2 = 64;   //LEDDRIVE_PS2  0~255 = 0~200ma
    uint8_t leddrive_ps3 = 64;    //LEDDRIVE_PS3  0~255 = 0~200ma

    uint8_t led_en_begin = 1;      // 0 = 2 ; 1 = 4 ; 2 = 8 ; 3 = 16 ;
    uint8_t afe_reset = 1;        //* 0 = 32clk ; 1 = 64clk ; 2 = 128clk ; 3 = 256clk(d) ;
    uint8_t en2rst_delay = 3;     // pen 2 rst delay 0 = 128,256, 512, 1024
    uint8_t init_wait_delay = 2; /* 0 = 800 ; 1 = 1600; 2 = 3200 ; 3 = 6400(d) */

    uint8_t ps1_interval_i2c = 0;   // ps2/ps3

    uint8_t thres_int = 1;   //thres int enable
    uint8_t data_rdy_int = 8;   //[3]:ps1_data2 [2]:ps1_data1 [1]:ps0_data2 [0]:ps0_data1


    uint8_t ldrsel_ps0 = 0;
    uint8_t ldrsel_ps1 = GREEN_LED_SLE;
    uint8_t ldrsel_ps2 = RED_LED_SLE;
    uint8_t ldrsel_ps3 = IR_LED_SLE;

    /***********cap *********/
    uint8_t intcapsel_ps3 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps2 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps1 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf
    uint8_t intcapsel_ps0 = 5;  //00=4fp 01=8pf 02=12pf 03=16pf 04=20pf 05=24pf 06=28pf 07=32pf 08=36pf 09=40pf 10=44pf 11=48pf 12=52pf 13=56pf 14=60pf

    uint8_t ps0_led_on_time = 64;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
                                     64 = 512clk=128us ; 128= 1024clk=256us; 256= 2048clk=512us*/
    uint8_t ps1_led_on_time = 64;    /* 0 = 32clk2=8us ; 8 = 64clk=16us; 16=128clk=32us ; 32 = 256clk=64us ;
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

    hx3011_write_reg(0x3d, 0x00);    //bits<3:0> fifo data sel, 0000 = p1;0001= p2;0010=p3;0011=p4;bits<7> fifo enble
    hx3011_write_reg(0x3e, 0x20);	//watermark
    hx3011_write_reg(0x3c, 0x1f);    // int_width_i2c
    hx3011_write_reg(0X37, 0x00);   // int sel,01=prf int,04=enable almost full int
    hx3011_write_reg(0X3b, 0x00);
    hx3011_write_reg(0X3b, 0x10);

    hx3011_write_reg(0x4d, 0x01);   //soft reset
    hx3011_delay(5);
    hx3011_write_reg(0x4d, 0x00);   //release
    hx3011_delay(5);
}

void hx3011_factory_ft_int_test_config(void)//中断测试配置
{
}

bool hx3011_factory_test_read(int32_t *ps_data)
{
    uint8_t  databuf1[6] = {0};
    uint8_t  databuf2[6] = {0};
    int32_t P1 = 0, P2 = 0, P4 = 0, P3 = 0;
    hx3011_brust_read_reg(0x02, databuf1, 6);
    hx3011_brust_read_reg(0x08, databuf2, 6);

    P1 = ((databuf1[0]) | (databuf1[1] << 8) | (databuf1[2] << 16));
    P2 = ((databuf1[3]) | (databuf1[4] << 8) | (databuf1[5] << 16));
    P3 = ((databuf2[0]) | (databuf2[1] << 8) | (databuf2[2] << 16));
    P4 = ((databuf2[3]) | (databuf2[4] << 8) | (databuf2[5] << 16));

    ps_data[0] = P1 ;
    ps_data[1] = P2 ;
    ps_data[2] = P3 ;
    ps_data[3] = P4 ;
    DEBUG_PRINTF("p1,p2,p3,p4 %d %d %d %d\r\n", P1, P2, P3, P4);
    return true;
}

bool hx3011_chip_check_id(void)
{
    uint8_t i = 0;
    uint8_t chip_idf = 0;
    for (i = 0; i < 10; i++) {
        hx3011_write_reg(0x01, 0x00);
        hx3011_delay(5);
        chip_idf = hx3011_read_reg(0x00);
        DEBUG_PRINTF("hx3011_chip_check_id  %d\r\n", chip_idf);
        if (chip_idf == 0x27) {
            return true;
        }
    }
    return false;
}

factory_result_t hx3011_factroy_test(uint32_t  test_mode)
{
    factory_result_t factory_reslts = {0, 0, 0};
    int32_t green_buffer[4] = {0, 0, 0, 0};
    int32_t red_buffer[4] = {0, 0, 0, 0};
    int32_t ir_buffer[4] = {0, 0, 0, 0};
    int32_t ps_data[4] = {0, 0, 0, 0};

    int32_t  green_led_data = 0;
    int32_t  red_led_data = 0;
    int32_t  ir_led_data = 0;

    int32_t green_psdif[4] = {0, 0, 0, 0};
    int32_t red_psdif[4] = {0, 0, 0, 0};
    int32_t ir_psdif[4] = {0, 0, 0, 0};

    int32_t green_final = 0;
    int32_t red_final = 0;
    int32_t ir_final = 0;
    int32_t testdata_temp = 0;

    uint8_t ii = 0;
    uint8_t jj = 0;

    if (hx3011_chip_check_id() == false) {
        return factory_reslts;
    }

    switch (test_mode) {
    case LEAK_LIGHT_TEST:
        hx3011_factory_ft_leak_light_test_config();
        break;
    case GRAY_CARD_TEST:
        hx3011_factory_ft_card_test_config();
        break;
    case FT_INT_TEST:
        hx3011_factory_ft_int_test_config();
        break;
    default:
        break;
    }
    hx3011_delay(100);
    if (test_mode == FT_INT_TEST) {
        return factory_reslts;
    }

    for (ii = 0; ii < 4; ii++) { //40ms read data
        hx3011_delay(40);
        hx3011_factory_test_read(ps_data);
        green_led_data = ps_data[1] - ps_data[0];
        red_led_data = ps_data[2] - ps_data[0];
        ir_led_data = ps_data[3] - ps_data[0];
        green_buffer[ii] = green_led_data;
        ir_buffer[ii] = ir_led_data;
        red_buffer[ii] = red_led_data;
    }

    for (ii = 0; ii < 3; ii++) {
        for (jj = 0; jj < 3 - ii; jj++) {
            if (green_buffer[jj] >= green_buffer[jj + 1]) {
                testdata_temp = green_buffer[jj];
                green_buffer[jj] = green_buffer[jj + 1];
                green_buffer[jj + 1] = testdata_temp;
            }

            if (red_buffer[jj] >= red_buffer[jj + 1]) {
                testdata_temp = red_buffer[jj];
                red_buffer[jj] = red_buffer[jj + 1];
                red_buffer[jj + 1] = testdata_temp;
            }
            if (ir_buffer[jj] >= ir_buffer[jj + 1]) {
                testdata_temp = ir_buffer[jj];
                ir_buffer[jj] = ir_buffer[jj + 1];
                ir_buffer[jj + 1] = testdata_temp;
            }
        }
    }

    if (test_mode == LEAK_LIGHT_TEST) {
        hx3011_write_reg(0X2a, 0x00);
        hx3011_write_reg(0X2b, 0x00);
        hx3011_delay(100);
        for (ii = 0; ii < 4; ii++) { //40ms read data
            hx3011_delay(40);
            hx3011_factory_test_read(ps_data);
            green_led_data = ps_data[1] - ps_data[0];
            red_led_data = ps_data[2] - ps_data[0];
            ir_led_data = ps_data[3] - ps_data[0];
            green_psdif[ii] = green_led_data;
            ir_psdif[ii] = ir_led_data;
            red_psdif[ii] = red_led_data;
        }
        for (jj = 0; jj < 3 - ii; jj++) {
            if (green_psdif[jj] >= green_psdif[jj + 1]) {
                testdata_temp = green_psdif[jj];
                green_psdif[jj] = green_psdif[jj + 1];
                green_psdif[jj + 1] = testdata_temp;
            }

            if (red_psdif[jj] >= red_psdif[jj + 1]) {
                testdata_temp = red_psdif[jj];
                red_psdif[jj] = red_psdif[jj + 1];
                red_psdif[jj + 1] = testdata_temp;
            }
            if (ir_psdif[jj] >= ir_psdif[jj + 1]) {
                testdata_temp = ir_psdif[jj];
                ir_psdif[jj] = ir_psdif[jj + 1];
                ir_psdif[jj + 1] = testdata_temp;
            }
        }
    }

    switch (test_mode) {
    case LEAK_LIGHT_TEST:
        green_final = (green_buffer[1] + green_buffer[2]) / 2 - (green_psdif[1] + green_psdif[2]) / 2;
        red_final = (red_buffer[1] + red_buffer[2]) / 2 - (red_psdif[1] + red_psdif[2]) / 2;
        ir_final = (ir_buffer[1] + ir_buffer[2]) / 2 - (ir_psdif[1] + ir_psdif[2]) / 2;
        break;
    case GRAY_CARD_TEST:
        green_final = (green_buffer[1] + green_buffer[2]) / 2;
        red_final = (red_buffer[1] + red_buffer[2]) / 2;
        ir_final = (ir_buffer[1] + ir_buffer[2]) / 2;
        break;
    default:
        break;
    }
    if (green_final < 0) {
        green_final = 0;
    }
    if (red_final < 0) {
        red_final = 0;
    }
    if (ir_final < 0) {
        ir_final = 0;
    }
    factory_reslts.green_data_final = green_final;
    factory_reslts.red_data_final = red_final;
    factory_reslts.ir_data_final = ir_final;
    DEBUG_PRINTF("%d %d %d\r\n", factory_reslts.green_data_final, factory_reslts.red_data_final, factory_reslts.ir_data_final);
    return factory_reslts;
}
#endif // TCFG_HX3011_ENABLE
