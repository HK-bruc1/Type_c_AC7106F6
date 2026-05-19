#include "asm/power_interface.h"
#include "init.h"
#include "app_config.h"
#include "cpu/includes.h"
#include "gpio_config.h"
#include "gSensor/gSensor_manage.h"
#if TCFG_HRSENSOR_ENABLE
#include "hr_sensor/hrSensor_manage.h"
#endif

void gpio_config_soft_poweroff(void);

/*-----------------------------------------------------------------------
 *进入、退出低功耗函数回调状态，函数单核操作、关中断，请勿做耗时操作
 *
 */
static u32 usb_io_con = 0;
void sleep_enter_callback(u8 step)
{
    /* 此函数禁止添加打印 */
    putchar('<');
}

void sleep_exit_callback(u32 usec)
{
    putchar('>');

}

//maskrom 使用到的io
static void __mask_io_cfg()
{
    struct app_soft_flag_t app_soft_flag = {0};

    mask_softflag_config(&app_soft_flag);
}

u8 power_soff_callback()
{

#if TCFG_GSENSOR_ENABLE || TCFG_HRSENSOR_ENABLE
    extern void sensor_del_data_check_cb();
    sensor_del_data_check_cb();
#endif
#if TCFG_GSENSOR_ENABLE
    gsensor_disable();
#endif      //end if CONFIG_GSENSOR_ENABLE
#if TCFG_HRSENSOR_ENABLE
    hr_sensor_measure_hr_stop();
#endif

    /*该函数未关中断*/
    do_platform_uninitcall();

    __mask_io_cfg();

    return 0;
}

void board_set_soft_poweroff()
{
    /*该函数已关中断*/
    gpio_config_soft_poweroff();

    //gpio_config_uninit();
}

__INITCALL_BANK_CODE
void power_early_flowing()
{
    PORT_TABLE(g);

    init_boot_rom();

    printf("get_boot_rom(): %d", get_boot_rom());

#if TCFG_LP_TOUCH_KEY_ENABLE
    //touchkey默认关闭按键长按复位
    gpio_longpress_pin0_reset_config(IO_PORTA_03, 0, 0, 1, PORT_KEEP_STATE, 0);
#else
    //iokey、adkey默认不关闭长按复位,烧录工具在efuse的长按复位配置才有效，可视化工具的长按复位如果配置了就会覆盖efuse的长按复位配置
    p33_and_1byte(P3_PINR_SAFE, (u8)~BIT(2));
#endif

    //长按复位1默认配置8s，写保护
#if TCFG_CHARGE_ENABLE
    gpio_longpress_pin1_reset_config(IO_LDOIN_DET, 1, 8, 1);
#endif

#if CONFIG_DEBUG_ENABLE || CONFIG_DEBUG_LITE_ENABLE
#ifdef TCFG_DEBUG_UART_TX_PIN
    PORT_PROTECT(TCFG_DEBUG_UART_TX_PIN);
#endif
#endif

    power_early_init((u32)gpio_config);
}

//early_initcall(_power_early_init);
__INITCALL_BANK_CODE
static int power_later_flowing()
{
    pmu_trim(0, 0);

    power_later_init(0);

    return 0;
}

late_initcall(power_later_flowing);

