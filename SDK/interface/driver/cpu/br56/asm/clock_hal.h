#ifndef  __CLOCK_HAL_H__
#define  __CLOCK_HAL_H__

#include "typedef.h"
enum CLK_OUT_SOURCE0 {
    CLK_OUT_SRC0_NULL = 0,
    CLK_OUT_SRC0_RC_16M,
    CLK_OUT_SRC0_LRC_CLK,
    CLK_OUT_SRC0_RC_250K,
    CLK_OUT_SRC0_STD_12M,
    CLK_OUT_SRC0_STD_24M,
    CLK_OUT_SRC0_STD_48M,
    CLK_OUT_SRC0_PLL_96M,
    CLK_OUT_SRC0_LSB_CLK,
    CLK_OUT_SRC0_HSB_CLK,
    CLK_OUT_SRC0_BTOSC_24M,
    CLK_OUT_SRC0_BTOSC_48M,
    CLK_OUT_SRC0_LRC_24M,
    CLK_OUT_SRC0_ALNK0_CLK,
    CLK_OUT_SRC0_USB_CLK,
    CLK_OUT_SRC0_RF_CKO_CLK,
};

enum CLK_OUT_SOURCE1 {
    CLK_OUT_SRC1_NULL = 0,
    CLK_OUT_SRC1_HSB_CLK,
    CLK_OUT_SRC1_WAT_CLK,
    CLK_OUT_SRC1_RING_CLK,
    CLK_OUT_SRC1_SYS_PLL_D3P5,
    CLK_OUT_SRC1_SYS_PLL_D2P5,
    CLK_OUT_SRC1_SYS_PLL_D2P0,
    CLK_OUT_SRC1_SYS_PLL_D1P5,
    CLK_OUT_SRC1_SYS_PLL_D1P0,
    CLK_OUT_SRC1_D_PLL_D1P0,
};

void clk_out0(u8 gpio, enum CLK_OUT_SOURCE0 clk);
void clk_out2(u8 gpio, enum CLK_OUT_SOURCE1 clk, u8 div);
//无 clk_out1

void clk_out0_close(u8 gpio);
void clk_out2_close(u8 gpio);



enum CLK_OUT_SOURCE {
    //ch0,ch1.  no div
    CLK_OUT_NULL = 0x0100,
    CLK_OUT_RC_16M,
    CLK_OUT_LRC_CLK,
    CLK_OUT_RC_250K,
    CLK_OUT_STD_12M,
    CLK_OUT_STD_24M,
    CLK_OUT_STD_48M,
    CLK_OUT_PLL_96M,
    CLK_OUT_LSB_CLK,
    CLK_OUT_HSB_CLK,
    CLK_OUT_BTOSC_24M,
    CLK_OUT_BTOSC_48M,
    CLK_OUT_LRC_24M,
    CLK_OUT_ALNK0_CLK,
    CLK_OUT_USB_CLK,
    CLK_OUT_RF_CKO_CLK,

    //ch2.  div:0~63(div1~div64)
    CLK_OUT_NULL_DIV = 0x0400,
    CLK_OUT_SYS_PLL_D3P5_DIV = 0x0404,
    CLK_OUT_SYS_PLL_D2P5_DIV,
    CLK_OUT_SYS_PLL_D2P0_DIV,
    CLK_OUT_SYS_PLL_D1P5_DIV,
    CLK_OUT_SYS_PLL_D1P0_DIV,
    CLK_OUT_BSB_PLL_D3P5_DIV,
    CLK_OUT_BSB_PLL_D2P5_DIV,
    CLK_OUT_BSB_PLL_D2P0_DIV,
    CLK_OUT_BSB_PLL_D1P5_DIV,
    CLK_OUT_BSB_PLL_D1P0_DIV,
};

#define CLK_OUT_CH_MASK             0x00000005
#define CLK_OUT_CH0_SEL(clk)        SFR(JL_LSBCLK->STD_CON1,0,5,clk)
#define CLK_OUT_CH0_GET_CLK()       ((JL_LSBCLK->STD_CON1>>0)&0x01f)
#define CLK_OUT_CH0_DIV(div)        //no div
#define CLK_OUT_CH0_EN(en)         //no en
#define CLK_OUT0_FIXED_IO_EN(en)   //no en
#define IS_CLK_OUT0_FIXED_IO()     (0)//no fix

#define CLK_OUT_CH1_SEL(clk)        //no ch1
#define CLK_OUT_CH1_GET_CLK()       (0)//no ch1
#define CLK_OUT_CH1_DIV(div)        //no div
#define CLK_OUT_CH1_EN(en)         //no en
#define CLK_OUT1_FIXED_IO_EN(en)   //no en
#define IS_CLK_OUT1_FIXED_IO()     (0)//no fix

#define CLK_OUT_CH2_SEL(clk)        SFR(JL_LSBCLK->STD_CON1,10,4,clk)
#define CLK_OUT_CH2_GET_CLK()       ((JL_LSBCLK->STD_CON1>>10)&0x0f)
#define CLK_OUT_CH2_DIV(div)        SFR(JL_LSBCLK->STD_CON1,14,6,div)
#define CLK_OUT_CH2_EN(en)         //no en
#define CLK_OUT2_FIXED_IO_EN(en)   //no en
#define IS_CLK_OUT2_FIXED_IO()     (0)//no fix

#define CLK_OUT_CH3_SEL(clk)        //no ch3
#define CLK_OUT_CH3_GET_CLK()       (0)//no ch3
#define CLK_OUT_CH3_DIV(div)        //no div
#define CLK_OUT_CH3_EN(en)         //no en
#define CLK_OUT3_FIXED_IO_EN(en)   //no en
#define IS_CLK_OUT3_FIXED_IO()     (0)//no fix

#define CLK_OUT_CH4_SEL(clk)        //no ch4
#define CLK_OUT_CH4_GET_CLK()       (0)//no ch4
#define CLK_OUT_CH4_DIV(div)        //no div
#define CLK_OUT_CH4_EN(en)         //no en
#define CLK_OUT4_FIXED_IO_EN(en)   //no en
#define IS_CLK_OUT4_FIXED_IO()     (0)//no fix
u32 clk_out_fixed_io_check(u32 gpio);


void ic_btosc_init(void);
u32 ic_pll_init(u32 osc_freq);
void clock_std_clock_config();

void xosc_common_init(u32 lrc_clk, void(*udly)(u32));//1.2ms~2.4ms

//for bt
void xosc_ctl_sys2lp(void(*udly)(u32));//only can use in msys_ctl
void xosc_ctl_lp2sys(void(*udly)(u32));//only can use in lp_ctl
void xosc_ctl_bt2sys(void);//only can use in bt_ctl
void xosc_ctl_sys2bt(void);//only can use in msys_ctl
void xosc_set_bt_cfg(u32 bt_hcs, u32 bt_cls, u32 bt_crs);
void xosc_set_sys_cfg(u32 sys_hcs, u32 sys_cls, u32 sys_crs);
void xosc_set_lp_cfg(u32 lp_hcs, u32 lp_cls, u32 lp_crs);

void xosc_set_cap_fine(u32 clsf, u32 crsf);

void clk_set_osc_cap(u8 sel_l, u8 sel_r);
u32 clk_get_osc_cap();
u32 get_btosc_info_for_update(void *info);


#define CLK_INDEPENDENT_CTL_DCVDD
void clk_set_vdc_lowest_voltage(u32 vdc_level);
void clk_vdc_mode_init(u32 mode, u32 vdc_level);

void clk_set_dcvdd_audio_ctl(u32 max_level, u32 min_level);


void clock_enter_sleep_prepare();
void clock_exit_sleep_post();



#define BT_CLOCK_IN(x)          //SFR(JL_CLOCK->CLK_CON1,  14,  2,  x)
//for MACRO - BT_CLOCK_IN
enum {
    BT_CLOCK_IN_PLL48M = 0,
    BT_CLOCK_IN_HSB,
    BT_CLOCK_IN_LSB,
    BT_CLOCK_IN_DISABLE,
};


#endif  /*CLOCK_HAL_H*/
