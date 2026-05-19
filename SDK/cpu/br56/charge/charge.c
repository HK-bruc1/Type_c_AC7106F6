#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".charge.data.bss")
#pragma data_seg(".charge.data")
#pragma const_seg(".charge.text.const")
#pragma code_seg(".charge.text")
#endif
#include "timer.h"
#include "asm/charge.h"
#include "gpadc.h"
#include "uart.h"
#include "device/device.h"
#include "asm/power_interface.h"
#include "system/init.h"
#include "asm/efuse.h"
#include "gpio.h"
#include "clock.h"
#include "app_config.h"
#include "spinlock.h"
#include "syscfg_id.h"

#define LOG_TAG_CONST   CHARGE
#define LOG_TAG         "[CHARGE]"
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"

// PMU结构简述:(NVDC架构)
// VPWR为充电输入引脚,VBAT为电池接口
// 充电模块的输入为VIN
// 正常情况下,(VPWR>VBAT时,VIN=VPWR) (VPWR<VBAT时,VIN=VBAT)
// >>/<<为电流可能走向
// VPWR----->>-----VIN--->><<-----CHARGE-->><<----VBAT
//                  |
//                  |
//                IOVDD

/*
 * 充电线性环路1说明,开启后对跟随充更友好,将会限制 VIN>VBAT+100mV(@SLT=0)
 * 例如:
 * VPWR外部供电5V限流50mA,充电模块设置100mA充电,充电开启后VIN电压将被拉低
 * 但是由于线性环路开启,VIN恒定处于VBAT+100mV的电压,此时充电电流为50mA
 */

/*
 * 充电线性环路2说明,开启后将不支持跟随充功能,充电模块输入端VIN电压将限制在不低于4V.
 * 例如:
 * 1、VPWR外部供电5V限流50mA,充电模块设置100mA充电,充电开启后,VIN将被拉低,
 * 当VIN被拉低至4V时,线性环路2将起作用,主动限制充电电流,VIN电压会恒定处于4V;
 *
 * 2、VPWR外部供电5V限流50mA,充电模块设置20mA充电,系统推屏需要从IOVDD耗电40mA,充电推屏开启后,VIN将被拉低
 * 当VIN被拉低至4V时,线性环路2将起作用,将优先满足系统供电需求,主动限制充电电流,VIN电压会恒定处于4V;
 *
 * 3、VPWR外部供电5V限流50mA,电池为3.7V,充电模块设置20mA充电,系统推屏需要从IOVDD耗电80mA;
 * 充电推屏开启后,VIN将被拉低,系统会把VIN的电压扯到低于4V,充电电流会降低至0mA,VIN会继续下降,
 * 一旦下降到低于VBAT电压,电池就会给系统供电,这种模式称为补电模式,在改模式下,VPWR和VBAT同时为系统供电
 */

#ifndef TCFG_CHARGE_NVDC_EN
#define TCFG_CHARGE_NVDC_EN 0 // NVDC架构使能
#endif

#define CHARGE_VILOOP1_ENABLE       1//默认开启
#define CHARGE_VILOOP2_ENABLE       TCFG_CHARGE_NVDC_EN//默认关闭
#define SUPER_FOLLOW_CHARGE_ENABLE  0//超级跟随充(默认关闭)

#define VDET_MODE_INNER             0//超级跟随充使用内置MOS管
#define VDET_MODE_EXT               1//超级跟随充使用外置MOS管
#define VDET_MODE_SEL               VDET_MODE_INNER

#if SUPER_FOLLOW_CHARGE_ENABLE && CHARGE_VILOOP2_ENABLE
#error "use super charge, need close NVDC"
#endif

typedef struct _CHARGE_VAR {
    const struct charge_platform_data *data;
    u8 charge_flag;
    u8 charge_poweron_en;
    u8 cc_flag;
    u8 cc_counter;
    u8 charge_online_flag;
    u8 charge_event_flag;
    u8 charge_full_filter_cnt;
    u8 progi_status;
#if SUPER_FOLLOW_CHARGE_ENABLE
    u8 super_charge_flag;
    u8 super_charge_filter_cnt;
    u32 lptmr_prd;
#endif
    u8 init_ok;
    u8 detect_stop;
    u16 full_voltage;
    u16 ldo5v_timer;   //检测LDOIN状态变化的usr timer
    u16 charge_timer;  //检测充电是否充满的sys timer
    u16 cc_timer;      //涓流切恒流的usr timer
    u16 progi_timer;
    u16 constant_current_progi_volt;//恒流阶段PROGI的值
} CHARGE_VAR;

#define __this 	(&charge_var)
static CHARGE_VAR charge_var;
static spinlock_t ldo5v_lock;

//判满时,VBAT的最小电压值
#define CHARGE_FULL_VBAT_MIN_VOLTAGE            (__this->full_voltage - 100)
//判满时,VPWR和VBAT的最小压差值
#define CHARGE_FULL_VPWR_VBAT_MIN_DIFF_VOLTAGE  (200)
//判满滤波时间s
#define CHARGE_FULL_FILTER_TIMES                (8)
//判断跟随充下进入恒压充电的电压值
#define CHARGE_CV_MODE_VOLTAGE                  (__this->full_voltage - 50)
//跟随充退出进入恒压充电的滤波时间s
#define CHARGE_CV_FILTER_TIMES                  (5)
//涓流切恒流滤波/s
#define CHARGE_TC2CC_FILTER_TIMES               4

#define BIT_LDO5V_IN		BIT(0)
#define BIT_LDO5V_OFF		BIT(1)
#define BIT_LDO5V_KEEP		BIT(2)

#define GET_PINR_EN()		((P33_CON_GET(P3_PINR_CON1) & BIT(0)) ? 1: 0)
#define GET_PINR_LEVEL()	((P33_CON_GET(P3_PINR_CON1) & BIT(2)) ? 1: 0)
#define GET_PINR_PIN()		((P33_CON_GET(P3_PINR_CON1) & BIT(3)) ? 1: 0)

extern void charge_event_to_user(u8 event);
static void charge_full_detect(void *priv);

u8 check_pinr_shutdown_enable(void)
{
    log_info("pinr lvl %d, ldo5v det %d", GET_PINR_LEVEL(), LDO5V_DET_GET());
    if (GET_PINR_EN() && (GET_PINR_LEVEL() == LDO5V_DET_GET()) && !GET_PINR_PIN()) {
        return 0;
    }
    return 1;
}

u8 get_charge_poweron_en(void)
{
    return __this->charge_poweron_en;
}

void set_charge_poweron_en(u32 onOff)
{
    __this->charge_poweron_en = onOff;
}

void charge_check_and_set_pinr(u8 level)
{
    u8 reg;
    reg = P33_CON_GET(P3_PINR_CON1);
    //开启LDO5V_DET长按复位
    if ((reg & BIT(0)) && ((reg & BIT(3)) == 0)) {
        if (level == 0) {
            P33_CON_SET(P3_PINR_CON1, 2, 1, 0);
        } else {
            P33_CON_SET(P3_PINR_CON1, 2, 1, 1);
        }
    }
}

static u8 check_charge_state(void)
{
    u16 i;
    __this->charge_online_flag = 0;

    for (i = 0; i < 20; i++) {
        if (LVCMP_DET_GET() || LDO5V_DET_GET()) {
            __this->charge_online_flag = 1;
            break;
        }
        udelay(1000);
    }
    return __this->charge_online_flag;
}

void set_charge_online_flag(u8 flag)
{
    __this->charge_online_flag = flag;
}

u8 get_charge_online_flag(void)
{
    return __this->charge_online_flag;
}

void set_charge_event_flag(u8 flag)
{
    __this->charge_event_flag = flag;
}

u8 get_ldo5v_online_hw(void)
{
    return LDO5V_DET_GET();
}

u8 get_lvcmp_det(void)
{
    return LVCMP_DET_GET();
}

u8 get_ldo5v_pulldown_en(void)
{
    if (!__this->data) {
        return 0;
    }
    if (get_ldo5v_online_hw()) {
        if (__this->data->ldo5v_pulldown_keep == 0) {
            return 0;
        }
    }
    return __this->data->ldo5v_pulldown_en;
}

u8 get_ldo5v_pulldown_res(void)
{
    if (__this->data) {
        return __this->data->ldo5v_pulldown_lvl;
    }
    return CHARGE_PULLDOWN_200K;
}

//更新progi口的电压用来判断满电电流
static void constant_current_progi_volt_config(void *priv)
{
    u16 cur_volt;
    static u32 max_progi_volt = 0;
    static u32 unit_cnt = 0;

    if (__this->constant_current_progi_volt) {
        goto __del_exit;
    }

    if (__this->progi_status == 0) {
        cur_volt = gpadc_battery_get_voltage();
        log_info("%s, %d, cur_vbat: %d mV\n", __FUNCTION__, __LINE__, cur_volt);
        if (cur_volt > __this->full_voltage - 500) {
            goto __del_exit;
        }
        max_progi_volt = 0;
        unit_cnt = 0;
        __this->progi_status = 1;
    }

    cur_volt = adc_get_voltage_blocking_filter(AD_CH_PMU_PROGI, 5);
    if (cur_volt > max_progi_volt) {
        max_progi_volt = cur_volt;
        log_info("%s, %d, max_progi: %d mV\n", __FUNCTION__, __LINE__, cur_volt);
    }

    unit_cnt++;
    if (unit_cnt > (3 * 60 * 1000 / 100)) {
        unit_cnt = 0;
        __this->progi_status = 0;
        //记录三分钟内的最大值,满足条件则记录到VM
        if ((max_progi_volt >= 950) && (max_progi_volt <= 1450)) {
            __this->constant_current_progi_volt = max_progi_volt;
            syscfg_write(VM_CHARGE_PROGI_VOLT, &__this->constant_current_progi_volt, 2);
            log_info("%s, %d, save_progi: %d mV\n", __FUNCTION__, __LINE__, __this->constant_current_progi_volt);
        }
        goto __del_exit;
    }
    return;
__del_exit:
    if (__this->progi_timer) {
        sys_timer_del(__this->progi_timer);
        __this->progi_timer  = 0;
    }
}

#if SUPER_FOLLOW_CHARGE_ENABLE

static u32 charge_get_lptmr_prd(void)
{
    u32 prd;
    prd = (P3_LP_PRD10 << 24) | (P3_LP_PRD11 << 16) | (P3_LP_PRD12 << 8) | (P3_LP_PRD13 << 0);
    return prd;
}

static u32 charge_get_lptmr_rsc(void)
{
    u32 rsc;
    rsc = (P3_LP_RSC10 << 24) | (P3_LP_RSC11 << 16) | (P3_LP_RSC12 << 8) | (P3_LP_RSC13 << 0);
    return rsc;
}

static void charge_lptmr_set_time(u32 rsc, u32 prd)
{
    p33_and_1byte(P3_LP_TMR1_CFG, (u8)~BIT(1));//mask rsc and prd cfg
    P3_LP_RSC10 = (rsc >> 24) & 0xff;
    P3_LP_RSC11 = (rsc >> 16) & 0xff;
    P3_LP_RSC12 = (rsc >>  8) & 0xff;
    P3_LP_RSC13 = (rsc >>  0) & 0xff;
    P3_LP_PRD10 = (prd >> 24) & 0xff;
    P3_LP_PRD11 = (prd >> 16) & 0xff;
    P3_LP_PRD12 = (prd >>  8) & 0xff;
    P3_LP_PRD13 = (prd >>  0) & 0xff;
    p33_or_1byte(P3_LP_TMR1_CFG, BIT(1));//enable rsc and prd cfg
}

static u8 charge_lptmr_init(void)
{
    u32 lrc_clk;
    u32 rsc, prd;
    if ((P3_LP_TMR1_CON & BIT(0)) == 0) {
        //触摸没有开启时,初始化LPTIMER,rsc=20ms-500us,prd:20ms
        lrc_clk = __get_lrc_hz();
        prd = lrc_clk * 20 / 1000;
        rsc = lrc_clk * (200 - 5) / 10000;
        SFR(P3_LP_TMR1_CON, 0, 1, 0);       //en
        charge_lptmr_set_time(rsc, prd);
        SFR(P3_LP_TMR1_CLK, 0, 3, 4);       //0:rclk; 1:btosc; 2:pat_clk; 3:osl_clk; 4:lrc_clk; 5:lrc24m_clk;
        SFR(P3_LP_TMR1_CLK, 4, 2, 0);       //0:div1; 1:div4; 2:div16; 3:div64;
        SFR(P3_LP_TMR1_CON, 3, 1, 0x1);     //PRD interrupt enable
        SFR(P3_LP_TMR1_CON, 2, 1, 0x0);     //RSC interrupt enable
        SFR(P3_LP_TMR1_CON, 1, 1, 1);
        SFR(P3_LP_TMR1_CON, 0, 1, 1);       //en
        P33_CON_SET(P3_LP_TMR1_CFG, 2, 1, 1);// enable p33 lptmr1 sw kst
        JL_PMU->PMU_CON |= BIT(9);           // tmr sw kick start
        P33_CON_SET(P3_LP_TMR1_CFG, 2, 1, 0);// disable p33 lptmr1 sw kst
        __this->lptmr_prd = prd;
        return KST_SEL_LPTMR1_PRD_PLS;//跟随充直接使用lptmr触发
    } else {
        //触摸开启后,重新设置PRD+1
        __this->lptmr_prd = charge_get_lptmr_prd();
        rsc = charge_get_lptmr_rsc();
        charge_lptmr_set_time(rsc, __this->lptmr_prd + 1);
        return KST_SEL_LPCTM_END_PLS;//跟随充使用触摸模块触发
    }
}

static void charge_lptmr_deinit(void)
{
    u32 src;
    u32 prd = charge_get_lptmr_prd();
    if (prd == __this->lptmr_prd) {
        //内置触摸没有初始化时,直接关闭定时器
        P3_LP_TMR1_CON &= ~BIT(0);
    } else {
        //内置触摸在使用,则设置PRD-1
        __this->lptmr_prd = charge_get_lptmr_prd();
        src = charge_get_lptmr_rsc();
        charge_lptmr_set_time(src, __this->lptmr_prd - 1);
    }
}

static void vdet_open_cfg(u8 open_en)
{
    u8 kst_sel;
    OS_ENTER_CRITICAL();
    if (open_en == 1) {
        if (IS_CHG_EXT_EN()) {
            OS_EXIT_CRITICAL();
            return;
        }
        log_debug("vdet open!\n");
        p33_io_wakeup_enable(IO_VBTCH_DET, 0);
        kst_sel = charge_lptmr_init();

        p33_and_1byte(P3_CLK_CON0, (u8)~BIT(7));    //pin_clk_gate_en disable
        CHARGE_EN(0);
        CHGGO_EN(0);

        WAIT_TIME_SEL(3);//wait time
        KST_SEL(kst_sel);
        VDET_SEL(VDET_SEL_VPWR_DET);
        VDET_FLOW_EN(1);

        OVER_TIME_SEL(3);//over time
        COMM_MODE(1);//comunication mode
        CHG_EXT_MODE(VDET_MODE_SEL);

        CHG_EXT_EN(1);
        PMU_BLOCK_COMPH(1);
        VPWR_LOAD_EN2(1);
    } else {
        if (IS_CHG_EXT_EN() == 0) {
            OS_EXIT_CRITICAL();
            return;
        }
        log_debug("vdet close!\n");
        charge_lptmr_deinit();
        VDET_FLOW_EN(0);
        VDET_SOFTR();
        CHG_EXT_EN(0);
        VPWR_LOAD_EN2(0);
        PMU_BLOCK_COMPH(0);
        CHARGE_EN(1);
        CHGGO_EN(1);
        p33_io_wakeup_enable(IO_VBTCH_DET, 1);
    }
    OS_EXIT_CRITICAL();
}
#endif

static void charge_cc_check(void *priv)
{
    u8 first_entry = (u8)priv;
    if ((adc_get_voltage_blocking(AD_CH_PMU_VBAT) * AD_CH_PMU_VBAT_DIV) > CHARGE_CCVOL_V) {
        if (__this->cc_flag == 0) {
            __this->cc_counter++;
        }
        if (first_entry || ((__this->cc_flag == 0) && (__this->cc_counter > CHARGE_TC2CC_FILTER_TIMES))) {
            __this->cc_flag = 1;
            set_charge_mA(__this->data->charge_mA);
            if (__this->charge_timer == 0) {
                __this->charge_timer = sys_timer_add(NULL, charge_full_detect, 1000);
            }
#if SUPER_FOLLOW_CHARGE_ENABLE
            __this->super_charge_flag = 1;
            vdet_open_cfg(1);
#else
            if (__this->progi_timer == 0) {
                __this->progi_status = 0;
                __this->progi_timer  = sys_timer_add(NULL, constant_current_progi_volt_config, 100);
            }
#endif
        }
    } else {
        __this->cc_counter = 0;
        if (first_entry || (__this->cc_flag == 1)) {
            __this->cc_flag = 0;
            set_charge_mA(__this->data->charge_trickle_mA);
            if (__this->charge_timer) {
                sys_timer_del(__this->charge_timer);
                __this->charge_timer = 0;
            }
#if (SUPER_FOLLOW_CHARGE_ENABLE == 0)
            if (__this->progi_timer) {
                sys_timer_del(__this->progi_timer);
                __this->progi_timer = 0;
            }
#endif
        }
    }

    //TC2CC or CC2TC check
    if (first_entry && (__this->cc_timer == 0)) {
        __this->cc_counter = 0;
        __this->cc_timer = usr_timer_add(0, charge_cc_check, 1000, 1);
    }
}

void charge_start(void)
{
    u32 vbat_voltage;
    log_info("%s\n", __func__);

    if (__this->charge_timer) {
        sys_timer_del(__this->charge_timer);
        __this->charge_timer = 0;
    }
    if (__this->progi_timer) {
        sys_timer_del(__this->progi_timer);
        __this->progi_timer = 0;
    }
    //进入恒流充电之后,才开启充满检测
    vbat_voltage = gpadc_battery_get_voltage();

    charge_cc_check((void *)1);

    PMU_NVDC_EN(CHARGE_VILOOP2_ENABLE);
    CHG_VILOOP_EN(CHARGE_VILOOP1_ENABLE);
    CHG_VILOOP2_EN(CHARGE_VILOOP2_ENABLE);
    CHARGE_EN(1);
    CHGGO_EN(1);

#if SUPER_FOLLOW_CHARGE_ENABLE
    //在恒流阶段才能进入跟随充
    if (__this->charge_timer && (vbat_voltage < CHARGE_CV_MODE_VOLTAGE)) {
        __this->super_charge_flag = 1;
        vdet_open_cfg(1);
    }
#endif

    charge_event_to_user(CHARGE_EVENT_CHARGE_START);
}

void charge_close(void)
{
    log_info("%s\n", __func__);

#if SUPER_FOLLOW_CHARGE_ENABLE
    vdet_open_cfg(0);
#endif

    CHARGE_EN(0);
    CHGGO_EN(0);
    CHG_VILOOP_EN(0);
    CHG_VILOOP2_EN(0);
    PMU_NVDC_EN(1);

    charge_event_to_user(CHARGE_EVENT_CHARGE_CLOSE);

    if (__this->charge_timer) {
        sys_timer_del(__this->charge_timer);
        __this->charge_timer = 0;
    }
    if (__this->cc_timer) {
        usr_timer_del(__this->cc_timer);
        __this->cc_timer = 0;
    }
    if (__this->progi_timer) {
        sys_timer_del(__this->progi_timer);
        __this->progi_timer = 0;
    }
}

static void charge_full_detect(void *priv)
{
    u32 cur_progi_volt;
    u32 calc_progi_volt;
    u16 vbat_voltage;
    u16 vpwr_voltage;
#if SUPER_FOLLOW_CHARGE_ENABLE
    u8  comph_v;
    if (__this->super_charge_flag) {
        //关闭跟随充,关闭充电,检测电池电压
        vdet_open_cfg(0);
        CHARGE_EN(0);
        CHGGO_EN(0);
        udelay(50);
        //读取COMPH信号
        comph_v = LVCMP_DET_GET();
        if (comph_v) {
            vbat_voltage = adc_get_voltage_blocking_filter(AD_CH_PMU_VBAT, 5) * AD_CH_PMU_VBAT_DIV;
            log_debug("super charge, vbat: %d\n", vbat_voltage);
            if (vbat_voltage < CHARGE_CV_MODE_VOLTAGE) {
                __this->super_charge_filter_cnt = 0;
            } else {
                __this->super_charge_filter_cnt++;
                if (__this->super_charge_filter_cnt >= CHARGE_CV_FILTER_TIMES) {
                    __this->super_charge_filter_cnt = 0;
                    __this->super_charge_flag = 0;
                }
            }
            if (__this->super_charge_flag) {
                vdet_open_cfg(1);
                return;
            }
        }
        CHARGE_EN(1);
        CHGGO_EN(1);
        if (comph_v == 0) {
            log_info("Actively enter filtering\n");
            __this->super_charge_filter_cnt = 0;
            ldoin_wakeup_isr();
        }
        return;
    }
#endif

    vbat_voltage = gpadc_battery_get_voltage();
    vpwr_voltage = adc_get_voltage_blocking_filter(AD_CH_PMU_VPWR_4, 5) * 4;

    log_debug("LVCMP_DET_GET: %d, LDO5V_DET_GET: %d, vbat: %d, vpwr: %d\n", \
              LVCMP_DET_GET(), LDO5V_DET_GET(), vbat_voltage, vpwr_voltage);

    if ((!LVCMP_DET_GET()) || (vbat_voltage < CHARGE_FULL_VBAT_MIN_VOLTAGE) || \
        (vpwr_voltage < (vbat_voltage + CHARGE_FULL_VPWR_VBAT_MIN_DIFF_VOLTAGE))) {
        //比较器或者电池电压不满足要求时,不进行电流检测判满
        __this->charge_full_filter_cnt = 0;
        log_debug("vpwr_voltage: %d mV, vbat_voltage: %d mV, full_min_volt: %dmV\n", vpwr_voltage, vbat_voltage, CHARGE_FULL_VBAT_MIN_VOLTAGE);
        return;
    }

    cur_progi_volt = adc_get_voltage_blocking_filter(AD_CH_PMU_PROGI, 5);
    calc_progi_volt = __this->constant_current_progi_volt;
    if (calc_progi_volt == 0) {
        calc_progi_volt = 1200;
    }

    log_debug("cur_progi_volt: %d mV, calc_progi_volt: %d mV\n", cur_progi_volt, calc_progi_volt);

    if (cur_progi_volt < (calc_progi_volt / __this->data->charge_full_mA)) {
        __this->charge_full_filter_cnt++;
        if (__this->charge_full_filter_cnt > CHARGE_FULL_FILTER_TIMES) {
            __this->charge_full_filter_cnt = 0;
            sys_timer_del(__this->charge_timer);
            __this->charge_timer = 0;
            charge_event_to_user(CHARGE_EVENT_CHARGE_FULL);
        }
    } else {
        __this->charge_full_filter_cnt = 0;
    }
}

static void ldo5v_detect(void *priv)
{
    static u16 ldo5v_on_cnt = 0;
    static u16 ldo5v_keep_cnt = 0;
    static u16 ldo5v_off_cnt = 0;

#if SUPER_FOLLOW_CHARGE_ENABLE
    vdet_open_cfg(0);
#endif

    if (__this->detect_stop) {
        return;
    }

    if (LVCMP_DET_GET()) {	//ldoin > vbat
        log_char('X');
        if (ldo5v_on_cnt < __this->data->ldo5v_on_filter) {
            ldo5v_on_cnt++;
            if (ldo5v_off_cnt >= (__this->data->ldo5v_off_filter + 4)) {
                ldo5v_off_cnt = __this->data->ldo5v_off_filter + 4;
            }
            if (__this->data->ldo5v_keep_filter <= 16) {
                ldo5v_keep_cnt = 0;
            } else if (ldo5v_keep_cnt >= (__this->data->ldo5v_keep_filter - 16)) {
                ldo5v_keep_cnt = __this->data->ldo5v_keep_filter - 16;
            }
        } else {
            log_debug("ldo5V_IN\n");
            set_charge_online_flag(1);
            ldo5v_off_cnt = 0;
            ldo5v_keep_cnt = 0;
            //消息线程未准备好接收消息,继续扫描
            if (__this->charge_event_flag == 0) {
                return;
            }
            ldo5v_on_cnt = 0;
            spin_lock(&ldo5v_lock);
            usr_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            spin_unlock(&ldo5v_lock);
            if ((__this->charge_flag & BIT_LDO5V_IN) == 0) {
                __this->charge_flag = BIT_LDO5V_IN;
                charge_event_to_user(CHARGE_EVENT_LDO5V_IN);
            } else {
#if SUPER_FOLLOW_CHARGE_ENABLE
                if (__this->super_charge_flag) {
                    vdet_open_cfg(1);
                }
#endif
            }
        }
    } else if (LDO5V_DET_GET() == 0) {	//ldoin<拔出电压（0.6）
        log_char('Q');
        if (ldo5v_off_cnt < (__this->data->ldo5v_off_filter + 20)) {
            ldo5v_off_cnt++;
            if (__this->data->ldo5v_on_filter <= 16) {
                ldo5v_on_cnt = 0;
            } else if (ldo5v_on_cnt >= (__this->data->ldo5v_on_filter - 16)) {
                ldo5v_on_cnt = __this->data->ldo5v_on_filter - 16;
            }
            if (__this->data->ldo5v_keep_filter <= 16) {
                ldo5v_keep_cnt = 0;
            } else if (ldo5v_keep_cnt >= (__this->data->ldo5v_keep_filter - 16)) {
                ldo5v_keep_cnt = __this->data->ldo5v_keep_filter - 16;
            }
        } else {
            log_debug("ldo5V_OFF\n");
            set_charge_online_flag(0);
            ldo5v_on_cnt = 0;
            ldo5v_keep_cnt = 0;
            //消息线程未准备好接收消息,继续扫描
            if (__this->charge_event_flag == 0) {
                return;
            }
            ldo5v_off_cnt = 0;
            spin_lock(&ldo5v_lock);
            usr_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            spin_unlock(&ldo5v_lock);
            if ((__this->charge_flag & BIT_LDO5V_OFF) == 0) {
                __this->charge_flag = BIT_LDO5V_OFF;
                charge_event_to_user(CHARGE_EVENT_LDO5V_OFF);
            }
        }
    } else {	//拔出电压（0.6左右）< ldoin < vbat
        log_char('E');
        if (ldo5v_keep_cnt < __this->data->ldo5v_keep_filter) {
            ldo5v_keep_cnt++;
            if (ldo5v_off_cnt >= (__this->data->ldo5v_off_filter + 4)) {
                ldo5v_off_cnt = __this->data->ldo5v_off_filter + 4;
            }
            if (__this->data->ldo5v_on_filter <= 16) {
                ldo5v_on_cnt = 0;
            } else if (ldo5v_on_cnt >= (__this->data->ldo5v_on_filter - 16)) {
                ldo5v_on_cnt = __this->data->ldo5v_on_filter - 16;
            }
        } else {
            log_debug("ldo5V_ERR\n");
            set_charge_online_flag(1);
            ldo5v_off_cnt = 0;
            ldo5v_on_cnt = 0;
            //消息线程未准备好接收消息,继续扫描
            if (__this->charge_event_flag == 0) {
                return;
            }
            ldo5v_keep_cnt = 0;
            spin_lock(&ldo5v_lock);
            usr_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            spin_unlock(&ldo5v_lock);
            if ((__this->charge_flag & BIT_LDO5V_KEEP) == 0) {
                __this->charge_flag = BIT_LDO5V_KEEP;
                if (__this->data->ldo5v_off_filter) {
                    charge_event_to_user(CHARGE_EVENT_LDO5V_KEEP);
                }
            }
        }
    }
}

void ldoin_wakeup_isr(void)
{
    if (!__this->init_ok) {
        return;
    }
    spin_lock(&ldo5v_lock);
    if (__this->ldo5v_timer == 0) {
        __this->ldo5v_timer = usr_timer_add(0, ldo5v_detect, 2, 1);
    }
    spin_unlock(&ldo5v_lock);
}

void charge_set_ldo5v_detect_stop(u8 stop)
{
    __this->detect_stop = stop;
}

u16 get_charge_mA_config(void)
{
    return __this->data->charge_mA;
}

void set_charge_mA(u16 charge_mA)
{
    //7-4:为分频系数
    //3-0:为恒流档位值
    u8 charge_lev = (charge_mA >> 0) & 0x0f;
    u8 charge_div = (charge_mA >> 4) & 0x0f;
    CHARGE_mA_SEL(charge_lev);
    CHG_TRICKLE_EN(charge_div);
}

u16 get_charge_full_value(void)
{
    ASSERT(__this->init_ok, "charge not init ok!\n");
    return __this->full_voltage;
}

__INITCALL_BANK_CODE
static void charge_config(void)
{
    u8 charge_trim_val;
    u8 offset = 0;
    u8 charge_full_v_val = 0;
    u8 charge_curr_trim;
    //判断是用高压档还是低压档
    if (__this->data->charge_full_V <= CHARGE_FULL_V_MIN_4340) {
        CHG_HV_MODE(0);
        CHG_HV4P5V_MODE(0);
        charge_trim_val = efuse_get_vbat_trim_4p20();//4.20V对应的trim出来的实际档位
        log_info("low charge_trim_val = %d\n", charge_trim_val);
        if (__this->data->charge_full_V >= CHARGE_FULL_V_MIN_4200) {
            offset = __this->data->charge_full_V - CHARGE_FULL_V_MIN_4200;
            charge_full_v_val = charge_trim_val + offset;
            if (charge_full_v_val > 0xf) {
                charge_full_v_val = 0xf;
            }
            __this->full_voltage = 4200 + (charge_full_v_val - charge_trim_val) * 20;
        } else {
            offset = CHARGE_FULL_V_MIN_4200 - __this->data->charge_full_V;
            if (charge_trim_val >= offset) {
                charge_full_v_val = charge_trim_val - offset;
            } else {
                charge_full_v_val = 0;
            }
            __this->full_voltage = 4200 - (charge_trim_val - charge_full_v_val) * 20;
        }
    } else if (__this->data->charge_full_V <= CHARGE_FULL_V_MID_4540) {
        CHG_HV_MODE(1);
        CHG_HV4P5V_MODE(0);
        charge_trim_val = efuse_get_vbat_trim_4p40();//4.40V对应的trim出来的实际档位
        log_info("mid charge_trim_val = %d\n", charge_trim_val);
        if (__this->data->charge_full_V >= CHARGE_FULL_V_MID_4400) {
            offset = __this->data->charge_full_V - CHARGE_FULL_V_MID_4400;
            charge_full_v_val = charge_trim_val + offset;
            if (charge_full_v_val > 0xf) {
                charge_full_v_val = 0xf;
            }
            __this->full_voltage = 4400 + (charge_full_v_val - charge_trim_val) * 20;
        } else {
            offset = CHARGE_FULL_V_MID_4400 - __this->data->charge_full_V;
            if (charge_trim_val >= offset) {
                charge_full_v_val = charge_trim_val - offset;
            } else {
                charge_full_v_val = 0;
            }
            __this->full_voltage = 4400 - (charge_trim_val - charge_full_v_val) * 20;
        }
    } else {
        CHG_HV_MODE(1);
        CHG_HV4P5V_MODE(1);
        charge_trim_val = efuse_get_vbat_trim_4p50();//4.50V对应的trim出来的实际档位
        log_info("high charge_trim_val = %d\n", charge_trim_val);
        if (__this->data->charge_full_V >= CHARGE_FULL_V_MAX_4500) {
            offset = __this->data->charge_full_V - CHARGE_FULL_V_MAX_4500;
            charge_full_v_val = charge_trim_val + offset;
            if (charge_full_v_val > 0xf) {
                charge_full_v_val = 0xf;
            }
            __this->full_voltage = 4500 + (charge_full_v_val - charge_trim_val) * 20;
        } else {
            offset = CHARGE_FULL_V_MAX_4500 - __this->data->charge_full_V;
            if (charge_trim_val >= offset) {
                charge_full_v_val = charge_trim_val - offset;
            } else {
                charge_full_v_val = 0;
            }
            __this->full_voltage = 4500 - (charge_trim_val - charge_full_v_val) * 20;
        }

    }
    log_info("charge_full_v_val = %d\n", charge_full_v_val);
    log_info("charge_full_voltage = %d mV\n", __this->full_voltage);

    //电流校准
    charge_curr_trim = efuse_get_charge_cur_trim();
    log_info("charge curr set value = %d\n", charge_curr_trim);

    CHGI_TRIM_SEL(charge_curr_trim);
    CHARGE_FULL_V_SEL(charge_full_v_val);
    set_charge_mA(__this->data->charge_trickle_mA);
}

__INITCALL_BANK_CODE
int charge_init(const struct charge_platform_data *data)
{
    log_info("%s\n", __func__);

    __this->data = data;

    ASSERT(__this->data);

    __this->init_ok = 0;
    __this->charge_online_flag = 0;
    __this->charge_poweron_en = data->charge_poweron_en;
    __this->constant_current_progi_volt = 0;

    spin_lock_init(&ldo5v_lock);

    /*先关闭充电使能，后面检测到充电插入再开启*/
    CHARGE_EN(0);
    CHGGO_EN(0);
    CHG_CCLOOP_EN(1);
    CHG_VILOOP_EN(0);
    CHG_VILOOP2_EN(0);
    PMU_NVDC_EN(1);
    L5V_IO_MODE(0);
#if (!SUPER_FOLLOW_CHARGE_ENABLE)
    //关闭VMAX,PB5作为IO使用
    CHG_EXT_MODE(0);
    CHG_EXT_EN(0);
#endif

    //消除vbat到vpwr的漏电再判断ldo5v状态
    u8 temp = 10;
    if (is_reset_source(P33_VDDIO_POR_RST)) {
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_00), PORT_INPUT_PULLDOWN_10K);
        while (temp) {
            udelay(1000);
            if (LDO5V_DET_GET() == 0) {
                temp = 0;
            } else {
                temp--;
            }
        }
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_00), PORT_INPUT_FLOATING);
    }

    /*LDO5V的100K下拉电阻使能*/
    L5V_RES_DET_S_SEL(__this->data->ldo5v_pulldown_lvl);
    L5V_LOAD_EN(__this->data->ldo5v_pulldown_en);

#if (SUPER_FOLLOW_CHARGE_ENABLE && (VDET_MODE_SEL == VDET_MODE_EXT))
    //VMUX IO设置为高阻态
    gpio_set_mode(IO_PORT_SPILT(IO_PORTB_05), PORT_HIGHZ);
#endif

    charge_config();

    if (check_charge_state()) {
        if (__this->ldo5v_timer == 0) {
            __this->ldo5v_timer = usr_timer_add(0, ldo5v_detect, 2, 1);
        }
    } else {
        __this->charge_flag = BIT_LDO5V_OFF;
    }

    syscfg_read(VM_CHARGE_PROGI_VOLT, &__this->constant_current_progi_volt, 2);
    log_info("constant_current_progi_volt: %d\n", __this->constant_current_progi_volt);

    __this->init_ok = 1;
    return 0;
}

void charge_system_reset(void)
{
    //充电采用LV0的复位
    system_reset(CHARGE_FLAG);
}

void charge_module_stop(void)
{
    if (!__this->init_ok) {
        return;
    }
    charge_close();
    p33_io_wakeup_enable(IO_LDOIN_DET, 0);
    p33_io_wakeup_enable(IO_VBTCH_DET, 0);
    if (__this->ldo5v_timer) {
        usr_timer_del(__this->ldo5v_timer);
        __this->ldo5v_timer = 0;
    }
}

void charge_module_restart(void)
{
    if (!__this->init_ok) {
        return;
    }
    if (!__this->ldo5v_timer) {
        __this->ldo5v_timer = usr_timer_add(NULL, ldo5v_detect, 2, 1);
    }
    p33_io_wakeup_enable(IO_LDOIN_DET, 1);
    p33_io_wakeup_enable(IO_VBTCH_DET, 1);
}

// 开机激活锂保退船运
void charge_exit_shipping(void)
{
    if (LVCMP_DET_GET()) {
        CHARGE_FULL_V_SEL(CHARGE_FULL_V_MIN_4040);
        set_charge_mA(CHARGE_mA_30 | CHARGE_DIV_10);
        L5V_IO_MODE(0);
        CHG_CCLOOP_EN(1);
        PMU_NVDC_EN(CHARGE_VILOOP2_ENABLE);
        CHG_VILOOP_EN(CHARGE_VILOOP1_ENABLE);
        CHG_VILOOP2_EN(CHARGE_VILOOP2_ENABLE);
        CHARGE_EN(1);
        CHGGO_EN(1);
    }
}

//系统进入低功耗时会调用
void charge_enter_lowpower(enum LOW_POWER_LEVEL lp_mode)
{
    if (lp_mode == LOW_POWER_MODE_SOFF) {
        CHARGE_EN(0);
        CHGGO_EN(0);
        PMU_NVDC_EN(0);
        CHG_CCLOOP_EN(0);
        CHG_VILOOP_EN(0);
        CHG_VILOOP2_EN(0);
        VDET_FLOW_EN(0);
        VDET_SOFTR();
        CHG_EXT_EN(0);
        VPWR_LOAD_EN2(0);
        PMU_BLOCK_COMPH(0);
        L5V_LOAD_EN(get_ldo5v_pulldown_en());
        L5V_RES_DET_S_SEL(get_ldo5v_pulldown_res());
    } else if (lp_mode == LOW_POWER_MODE_SLEEP) {
        if (IS_CHARGE_EN()) {
            //KEEP MVBG VIN MIOVDD
            p33_or_1byte(P3_ANA_KEEP0, BIT(7) | BIT(6) | BIT(5));
            //KEEP MVIO_VLMT MVIO_IFULL
            p33_or_1byte(P3_ANA_KEEP1, BIT(4) | BIT(3));
            //KEEP NVDC STATUS
            p33_fast_access(P3_ANA_KEEP2, BIT(4), 1);
        } else {
            p33_fast_access(P3_ANA_KEEP2, BIT(4), 0);
        }
    }
}
