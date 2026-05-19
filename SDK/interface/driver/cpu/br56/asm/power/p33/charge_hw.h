
#ifndef __CHARGE_HW_H__
#define __CHARGE_HW_H__

/************************P3_CHG_CON0*****************************/
#define CHARGE_EN(en)           	p33_fast_access(P3_CHG_CON0, BIT(0), en)

#define CHGGO_EN(en)            	p33_fast_access(P3_CHG_CON0, BIT(1), en)

#define IS_CHARGE_EN()				(((P33_CON_GET(P3_CHG_CON0) & BIT(0)) || (P33_CON_GET(P3_ANA_FLOW5) & BIT(2))) ? 1: 0 )

#define CHG_HV_MODE(mode)       	p33_fast_access(P3_CHG_CON0, BIT(2), mode)

#define CHG_HV4P5V_MODE(mode)     	p33_fast_access(P3_CHG_CON0, BIT(3), mode)

#define CHG_CCLOOP_EN(en)           p33_fast_access(P3_CHG_CON0, BIT(4), en)

#define CHG_VILOOP_EN(en)           p33_fast_access(P3_CHG_CON0, BIT(5), en)

#define CHG_VILOOP2_EN(en)          p33_fast_access(P3_CHG_CON0, BIT(6), en)

#define CHG_VINLOOP_SLT(sel)        p33_fast_access(P3_CHG_CON0, BIT(7), sel)

/************************P3_CHG_CON1*****************************/
#define CHARGE_mA_SEL(a)			P33_CON_SET(P3_CHG_CON1, 0, 4, a)

/************************P3_CHG_CON2*****************************/
#define CHARGE_FULL_V_SEL(a)		P33_CON_SET(P3_CHG_CON2, 4, 4, a)

/************************P3_CHG_CON3*****************************/
#define CHG_FOLLOWC_SLT(sel)        p33_fast_access(P3_CHG_CON3, BIT(3), sel)

enum {
    CHARGE_DET_VOL_365V,
    CHARGE_DET_VOL_375V,
    CHARGE_DET_VOL_385V,
    CHARGE_DET_VOL_395V,
};
#define CHARGE_DET_VOL(a)			P33_CON_SET(P3_CHG_CON3, 1, 2, a)

#define CHARGE_DET_EN(en)			p33_fast_access(P3_CHG_CON3, BIT(0), en)

/************************P3_CHG_CON4*****************************/
#define CHGI_TRIM_SEL(a)            P33_CON_SET(P3_CHG_CON4, 0, 4, a)

/************************P3_CHG_CON5*****************************/
#define CHG_TRICKLE_EN(a)           P33_CON_SET(P3_CHG_CON5, 0, 4, a)

/************************P3_L5V_CON0*****************************/
#define L5V_IO_MODE(a)              p33_fast_access(P3_VPWR_CON0, BIT(2), a)

#define IS_L5V_LOAD_EN()        	((P33_CON_GET(P3_VPWR_CON0) & BIT(0)) ? 1: 0)

#define L5V_LOAD_EN(a)		    	p33_fast_access(P3_VPWR_CON0, BIT(0), a)

/************************P3_L5V_CON1*****************************/
#define L5V_RES_DET_S_SEL(a)		P33_CON_SET(P3_VPWR_CON1, 0, 2, a)

#define GET_L5V_RES_DET_S_SEL() 	(P33_CON_GET(P3_VPWR_CON1) & 0x03)

/************************P3_AWKUP_LEVEL*****************************/
#define VBAT_DET_FILTER_GET()	    ((P33_CON_GET(P3_AWKUP_LEVEL) & BIT(2)) ? 1: 0)

#define LVCMP_DET_FILTER_GET()      ((P33_CON_GET(P3_AWKUP_LEVEL) & BIT(1)) ? 1: 0)

#define LDO5V_DET_FILTER_GET()      ((P33_CON_GET(P3_AWKUP_LEVEL) & BIT(0)) ? 1: 0)

/************************P3_ANA_READ*****************************/
#define LDO5V_DET_GET()			    ((P33_CON_GET(P3_ANA_READ) & BIT(2)) ? 1: 0 )

#define LVCMP_DET_GET()			    ((P33_CON_GET(P3_ANA_READ) & BIT(1)) ? 1: 0 )

#define VBAT_DET_GET()              ((P33_CON_GET(P3_ANA_READ) & BIT(0)) ? 1: 0 )


/************************P3_ANA_FLOW2*****************************/
#define PMU_NVDC_EN(a)              p33_fast_access(P3_ANA_FLOW2, BIT(4), a)

/************************P3_ANA_FLOW5*****************************/
//1:片外跟随充模式,VPWR到VBAT通路有外置功率管
//0:片内跟随充模式
#define CHG_EXT_MODE(mode)          p33_fast_access(P3_ANA_FLOW5, BIT(3), mode)
//跟随充使能
#define CHG_EXT_EN(en)              p33_fast_access(P3_ANA_FLOW5, BIT(2), en)
//读取跟随充使能信号
#define IS_CHG_EXT_EN()             ((P33_CON_GET(P3_ANA_FLOW5) & BIT(2)) ? 1: 0)
//强制PMU输出VPWR_COMPH信号为低电平的使能信号
//0:COMPH信号正常
//1:COMPH信号恒为0
#define PMU_BLOCK_COMPH(sel)        p33_fast_access(P3_ANA_FLOW5, BIT(1), sel)
//VPWR 5k ohm下拉信号使能
#define VPWR_LOAD_EN2(en)           p33_fast_access(P3_ANA_FLOW5, BIT(0), en)

/************************P3_VDET_CON0*****************************/
enum {
    KST_SEL_LPTMR0_PRD_PLS,
    KST_SEL_LPTMR1_PRD_PLS,
    KST_SEL_LPCTM_END_PLS,
    KST_SEL_PCNT_OVF,
};
#define KST_SEL(sel)                P33_CON_SET(P3_VDET_CON0, 6, 2, sel)

enum {
    VDET_SEL_VPWR_DET,
    VDET_SEL_PP0,
    VDET_SEL_VPWR_COMPH,
};
#define VDET_SEL(sel)               P33_CON_SET(P3_VDET_CON0, 4, 2, sel)

enum {
    VDET_STA_IDLE,
    VDET_STA_CTRL_D,
    VDET_STA_WAITING,
    VDET_STA_DETECT,
    VDET_STA_CTRL_U,
};
#define GET_VDET_STA()              ((P33_CON_GET(P3_VDET_CON0) >> 1) & 0x07)

//跟随充硬件状态机使能
#define VDET_FLOW_EN(en)            p33_fast_access(P3_VDET_CON0, BIT(0), en)

/************************P3_VDET_CON1*****************************/
//0:no running 1:running
#define GET_VDET_RUN()              ((P33_CON_GET(P3_VDET_CON1) & BIT(3)) ? 1: 0)

//跟随充停止硬件自动开启跟随充模式的标志位,只有 C_MODE 设置为1,且在 over_time_sel
//计时窗口内检测到一次低电平才会被硬件置1,硬件停止自动开启跟随充模式,只有软件 VDET_SOFR
//复位后才能恢复0
#define GET_HW_FLOW_DIS()           ((P33_CON_GET(P3_VDET_CON1) & BIT(2)) ? 1: 0)

//跟随充通信模式,开启后在 over_cnt 计时窗口内检测到一次低电平都会停止硬件自动开启跟随充的模式
#define COMM_MODE(mode)             p33_fast_access(P3_VDET_CON1, BIT(1), mode)

//跟随充硬件模块复位
#define VDET_SOFTR()                p33_fast_access(P3_VDET_CON1, BIT(0), 1)

/************************P3_VDET_CNT0*****************************/
//检测信号超时窗口时间设置 2^n rclk
#define OVER_TIME_SEL(n)            P33_CON_SET(P3_VDET_CNT0, 4, 4, n)

//等待检测信号稳定时间设置 2^n rclk
#define WAIT_TIME_SEL(n)            P33_CON_SET(P3_VDET_CNT0, 0, 4, n)

#endif

