#include "asm/power_interface.h"
#include "app_config.h"
#include "gpio_config.h"
#include "system/init.h"

const u32 dcdc_L = DCDC_L_10uH;//DCDC_L_4p7uH;

//-----------------------------------------------------------------------------------------------------------------------
/* power_param
 */
struct _power_param power_param = {
    .config         = TCFG_LOWPOWER_LOWPOWER_SEL,
    .btosc_hz       = TCFG_CLOCK_OSC_HZ,
    .vddiom_lev     = TCFG_LOWPOWER_VDDIOM_LEVEL,
    .vddiow_lev     = TCFG_LOWPOWER_VDDIOW_LEVEL,
    .osc_type       = TCFG_LOWPOWER_OSC_TYPE,
};

//----------------------------------------------------------------------------------------------------------------------
/* power_pdata
 */
struct _power_pdata power_pdata = {
    .power_param_p  = &power_param,
};

//----------------------------------------------------------------------------------------------------------------------
void key_wakeup_init();

__INITCALL_BANK_CODE
void board_power_init()
{
    //gpio_config_init();

    power_control(PCONTROL_PD_VDDIO_KEEP, VDDIO_KEEP_TYPE_NORMAL);
    power_control(PCONTROL_SF_VDDIO_KEEP, VDDIO_KEEP_TYPE_NORMAL);

    /*power_control(PCONTROL_SF_KEEP_PVDD, 1);*/
    /*power_set_dcdc_type(TCFG_DCDC_TYPE);*/

    power_init(&power_pdata);

#if (!TCFG_CHARGE_ENABLE)
    power_set_mode(TCFG_LOWPOWER_POWER_SEL);
    //没开启充电时,关闭漏电寄存器(约2uA)
    CHG_VILOOP_EN(0);
    CHG_VILOOP2_EN(0);
    //关闭VMAX,PB5作为IO使用
    CHG_EXT_MODE(0);
    CHG_EXT_EN(0);
#endif

    key_wakeup_init();
}
