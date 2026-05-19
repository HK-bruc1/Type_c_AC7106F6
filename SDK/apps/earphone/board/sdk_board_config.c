#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".sdk_board_config.data.bss")
#pragma data_seg(".sdk_board_config.data")
#pragma const_seg(".sdk_board_config.text.const")
#pragma code_seg(".sdk_board_config.text")
#endif
#include "asm/power_interface.h"
#include "gpadc.h"
#include "app_config.h"
#include "gSensor/gSensor_manage.h"
#include "imuSensor_manage.h"
#include "iic_api.h"
#include "audio_adc.h"
#include "eartouch_manage.h"
#include "iokey_config.h"
#include "adkey_config.h"
#include "mickey_config.h"
#include "key_driver.h"
#include "gpio_config.h"
#include "app_power_manage.h"
#include "asm/lp_touch_key_api.h"

#if TCFG_MPU6887P_ENABLE
#include "imu_sensor/mpu6887/mpu6887p.h"
#endif
#if TCFG_QMI8658_ENABLE
#include "imu_sensor/qmi8658/qmi8658c.h"
#endif
#if TCFG_LSM6DSL_ENABLE
#include "imu_sensor/lsm6dsl/lsm6dsl.h"
#endif
#if TCFG_ICM42670P_ENABLE
#include "imu_sensor/icm_42670p/icm_42670p.h"
#endif
#if TCFG_HRSENSOR_ENABLE
#include "hr_sensor/hrSensor_manage.h"
#endif

#include "sdk_config.c"

#include "../iokey_config.c"
#include "../adkey_config.c"
#include "../mickey_config.c"


// *INDENT-OFF*

/************************** imu sensor ****************************/
#if TCFG_IMUSENSOR_ENABLE
IMU_SENSOR_PLATFORM_DATA_BEGIN(imu_sensor_data)
#if TCFG_SH3001_ENABLE
{
    .peripheral_hdl = TCFG_SH3001_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
     .peripheral_param0 = 0,  //iic_delay(iic byte间间隔)   or spi_cs_pin
      .imu_sensor_name = "sh3001",
       .imu_sensor_int_io = TCFG_SH3001_DETECT_IO,
},
#endif
#if TCFG_TP_MPU9250_ENABLE
{
    .peripheral_hdl = TCFG_MPU9250_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
    .peripheral_param0 = 1, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
    .imu_sensor_name = "mpu9250",
    .imu_sensor_int_io = TCFG_MPU9250_DETECT_IO,
},
#endif
#if TCFG_MPU6887P_ENABLE
{
    .peripheral_hdl = TCFG_MPU6887P_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
    .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
    .imu_sensor_name = "mpu6887p",
    .imu_sensor_int_io = TCFG_MPU6887P_DETECT_IO,
},
#endif
#if TCFG_ICM42670P_ENABLE
{
    .peripheral_hdl = TCFG_ICM42670P_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
    .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
    .imu_sensor_name = "icm42670p",
    .imu_sensor_int_io = TCFG_ICM42670P_DETECT_IO,
},
#endif
#if TCFG_MPU6050_EN //未整合参数如GSENSOR_PLATFORM_DATA_BEGIN配置
{
    .peripheral_hdl = 0,     //iic_hdl     or  spi_hdl
    .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
    .imu_sensor_name = "mpu6050",
    .imu_sensor_int_io = -1,
},
#endif
#if TCFG_QMI8658_ENABLE
{
    .peripheral_hdl = TCFG_QMI8658_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
    .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
    .imu_sensor_name = "qmi8658",
    .imu_sensor_int_io = TCFG_QMI8658_DETECT_IO,
},
#endif
#if TCFG_LSM6DSL_ENABLE
{
    .peripheral_hdl = TCFG_LSM6DSL_USER_IIC_INDEX,     //iic_hdl     or  spi_hdl
    .peripheral_param0 = 0, //iic_delay(iic byte间间隔)   or spi_cs_pin//iic byte间间隔
    .imu_sensor_name = "lsm6dsl",
    .imu_sensor_int_io = TCFG_LSM6DSL_DETECT_IO,
},
#endif
IMU_SENSOR_PLATFORM_DATA_END();
#endif
/************************** gsensor ****************************/
#if TCFG_GSENSOR_ENABLE
GSENSOR_PLATFORM_DATA_BEGIN(gSensor_data)
#if TCFG_SC7A20_EN
    .iic = 0,
    .gSensor_name = "sc7a20",
#elif TCFG_STK832x_EN
    .iic = 0,
    .gSensor_name = "stk832x",
#endif
GSENSOR_PLATFORM_DATA_END();
#endif

void imu_sensor_ad0_selete(u32 gpio, u8 out_en)
{
#if (TCFG_MPU6887P_AD0_SELETE_IO != NO_CONFIG_PORT) || \
    (TCFG_QMI8658_AD0_SELETE_IO != NO_CONFIG_PORT)
    gpio_set_mode(IO_PORT_SPILT(gpio), !!out_en);
#endif
}

void imu_sensor_power_ctl(u32 gpio, u8 out_en)
{
#if (TCFG_IMU_SENSOR_PWR_PORT != NO_CONFIG_PORT)
    gpio_set_mode(IO_PORT_SPILT(gpio), !!out_en);
#endif
}

/************************** hrsensor ****************************/
#if TCFG_HRSENSOR_ENABLE
HRSENSOR_PLATFORM_DATA_BEGIN(hr_sensor_data)
#if TCFG_HX3918_ENABLE
    .iic = 0,
    .hrSensor_name = "hx3918",
#elif TCFG_HX3011_ENABLE
    .iic = 0,
    .hrSensor_name = "hx3011",
#endif
HRSENSOR_PLATFORM_DATA_END()
#endif

void board_imu_sensor_init()
{
#if (TCFG_AUDIO_SPATIAL_EFFECT_ENABLE || TCFG_AUDIO_SOMATOSENSORY_ENABLE)
#ifdef TCFG_IMU_SENSOR_PWR_PORT
    imu_sensor_power_ctl(TCFG_IMU_SENSOR_PWR_PORT, 1);
#endif

    os_time_dly(1);

#if TCFG_MPU6050_EN
    gravity_sensor_init(&motion_sensor_data);
#endif

#if TCFG_MPU6887P_ENABLE
    imu_sensor_ad0_selete(TCFG_MPU6887P_AD0_SELETE_IO, MPU6887P_SA0_IIC_ADDR);
#endif

#if TCFG_QMI8658_ENABLE
    imu_sensor_ad0_selete(TCFG_QMI8658_AD0_SELETE_IO, QMI8658_SD0_IIC_ADDR);
#endif

#if TCFG_LSM6DSL_ENABLE
    imu_sensor_ad0_selete(TCFG_LSM6DSL_AD0_SELETE_IO, LSM6DSL_SD0_IIC_ADDR);
#endif

#if TCFG_ICM42670P_ENABLE
    imu_sensor_ad0_selete(TCFG_ICM42670P_AD0_SELETE_IO, ICM42670P_SA0_IIC_ADDR);
#endif

#if TCFG_IMUSENSOR_ENABLE
    imu_sensor_init((void *)imu_sensor_data, sizeof(imu_sensor_data));
#endif

    /* //MPU进入睡眠 */
#if TCFG_MPU6887P_ENABLE
    imu_sensor_io_ctl("mpu6887p", IMU_SENSOR_SLEEP, NULL);
#endif

#if TCFG_QMI8658_ENABLE
    imu_sensor_io_ctl("qmi8658", IMU_SENSOR_SLEEP, NULL);
#endif

#if TCFG_LSM6DSL_ENABLE
    imu_sensor_io_ctl("lsm6dsl", IMU_SENSOR_SLEEP, NULL);
#endif

#if TCFG_ICM42670P_ENABLE
    imu_sensor_io_ctl("icm42670p", IMU_SENSOR_SLEEP, NULL);
#endif

#endif/*TCFG_AUDIO_SPATIAL_EFFECT_ENABLE || TCFG_AUDIO_SOMATOSENSORY_ENABLE*/
}


#if TCFG_OUTSIDE_EARTCH_ENABLE
eartouch_platform_data eartouch_cfg = {
#if TCFG_EARTCH_SENSOR_SEL == EARTCH_OUTSDIE_TCH
    .logo = "outside_tch",
#elif TCFG_EARTCH_SENSOR_SEL == EARTCH_HX300X
    .logo = "hx300x",
#endif
    .pwr_port = TCFG_EARTCH_PWR_PORT,
    .int_port = TCFG_EARTCH_INT_PORT,
#if (TCFG_EARTCH_SENSOR_SEL  !=  EARTCH_OUTSDIE_TCH)
    .iic_clk  = TCFG_EARTCH_CLK_PORT,
    .iic_dat  = TCFG_EARTCH_DAT_PORT,
#endif
    .in_filter_cnt = 2, 			    //戴上消抖
    .out_filter_cnt = 3,   		        //拿下消抖
};
#endif

void gpio_config_set(const struct gpio_cfg_item *table, int table_size)
{
    for (int i = 0; i < table_size; i++) {
        struct gpio_config config = {
            .pin   = BIT(table[i].gpio % 16),
            .mode  = table[i].mode,
            .hd    = table[i].hd,
        };
        gpio_init(table[i].gpio / 16, &config);
    }
}

int gpio_config_init()
{
#if TCFG_IO_CFG_AT_POWER_ON
    puts("gpio_cfg_at_power_on\n");
    gpio_config_set(g_io_cfg_at_poweron, ARRAY_SIZE(g_io_cfg_at_poweron));
#endif
    return 0;
}

int gpio_config_uninit()
{
#if TCFG_IO_CFG_AT_POWER_OFF
    gpio_config_set(g_io_cfg_at_poweroff, ARRAY_SIZE(g_io_cfg_at_poweroff));
#endif

    return 0;
}


// *INDENT-OFF*
#if TCFG_LP_TOUCH_KEY_ENABLE
LPCTMU_PLATFORM_DATA_BEGIN(lpctmu_pdata) = {
#if LPCTMU_ANA_CFG_ADAPTIVE
    .aim_vol_delta              = TCFG_LP_KEY_LIMIT_VOLTAGE_DELTA,
    .aim_charge_khz             = TCFG_LP_KEY_CHARGE_FREQ_KHz,
#else
    .hv_level                   = TCFG_LP_KEY_UPPER_LIMIT_VOLTAGE,
    .lv_level                   = TCFG_LP_KEY_LOWER_LIMIT_VOLTAGE,
    .cur_level                  = TCFG_LP_KEY_CURRENT_LEVEL,
#endif
LPCTMU_PLATFORM_DATA_END();

LP_TOUCH_KEY_PLATFORM_DATA_BEGIN(lp_touch_key_pdata) = {
    .slide_mode_en              = TCFG_LP_KEY_SLIDE_ENABLE,
    .slide_mode_key_value       = TCFG_LP_KEY_SLIDE_VALUE,
    .charge_mode_keep_touch     = TCFG_LP_KEY_ENABLE_IN_CHARGE,
#if TCFG_LP_KEY_LONG_PRESS_RESET
    .long_press_reset_time      = TCFG_LP_KEY_LONG_PRESS_RESET_TIME,
#else
    .long_press_reset_time      = 0,
#endif
    .key_cfg                    = lp_touch_key_table,
    .key_num                    = ARRAY_SIZE(lp_touch_key_table),
    .lpctmu_pdata               = &lpctmu_pdata,
LP_TOUCH_KEY_PLATFORM_DATA_END();
#endif

__INITCALL_BANK_CODE
void board_init()
{
    board_power_init();

#if TCFG_UPDATE_UART_IO_EN
    {
#include "uart_update.h"
        /* uart_update_cfg  update_cfg = { */
        /*     .rx = IO_PORTA_02, */
        /*     .tx = IO_PORTA_03, */
        /* }; */
        y_printf(">>>[test]:old uart update\n");
        uart_update_init();
    }
#endif

#if CONFIG_UPDATE_MUTIL_CPU_UART
    {
#include "update_interactive_uart.h"
        /* update_interactive_uart_cfg  update_interactive_cfg = { */
        /*     .rx = CONFIG_UPDATE_MUTIL_CPU_UART_TX_PIN, */
        /*     .tx = CONFIG_UPDATE_MUTIL_CPU_UART_RX_PIN, */
        /* }; */
        y_printf(">>>[test]:new uart update\n");
        update_interactive_uart_init();
    }
#endif

    gpadc_init();

#if TCFG_BATTERY_CURVE_ENABLE
    vbat_curve_init(g_battery_curve_table, ARRAY_SIZE(g_battery_curve_table));
#endif

#if TCFG_OUTSIDE_EARTCH_ENABLE
    eartouch_init(&eartouch_cfg);
#endif

#if ((TCFG_AUDIO_SPATIAL_EFFECT_ENABLE && TCFG_SPATIAL_AUDIO_SENSOR_ENABLE) || TCFG_AUDIO_SOMATOSENSORY_ENABLE)
    board_imu_sensor_init();
#endif
#if TCFG_GSENSOR_ENABLE
    gravity_sensor_init((void*)&gSensor_data);
#endif      //end if CONFIG_GSENSOR_ENABLE

#if TCFG_HRSENSOR_ENABLE
    hr_sensor_init((void *)&hr_sensor_data);
#endif

#if TCFG_GSENSOR_ENABLE || TCFG_HRSENSOR_ENABLE
void app_sensor_init(void);
    app_sensor_init();
#endif
}

#if TCFG_LP_TOUCH_KEY_ENABLE
__INITCALL_BANK_CODE
static int touch_key_init(void)
{
    lp_touch_key_init(&lp_touch_key_pdata);
    return 0;
}
late_initcall(touch_key_init);//触摸按键的初始化要在pmu和充电初始化之后
#endif

