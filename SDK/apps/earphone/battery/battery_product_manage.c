#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".battery_product_manage.data.bss")
#pragma data_seg(".battery_product_manage.data")
#pragma const_seg(".battery_product_manage.text.const")
#pragma code_seg(".battery_product_manage.text")
#endif

#include "system/includes.h"
#include "battery_manager.h"
#include "app_power_manage.h"
#include "app_main.h"
#include "asm/charge.h"
#include "user_cfg.h"
#include "asm/charge.h"
#include "battery_product_manage.h"

#define LOG_TAG             "[BATTERY_PRODUCT_MANAGE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"
/************************产线电量管控***************************/
#if TCFG_BATTERY_PRODUCT_MANAGE_ENABLE

#define DEFAULT_BATTERY_CONTROL_VOLT_MAX    4040
#define DEFAULT_BATTERY_CONTROL_VOLT_MIN    3900

#define PRODUCT_LINE_HEART_BEAT_INTERVAL    5000 //5s间隔
#define PRODUCT_LINE_LOWER_POWER_PERIOD     60   //低于下限5分钟关机
#define PRODUCT_LINE_AUTO_OFF_PERIOD        120  //不在舱10分钟无动作超时关机

typedef struct battery_product_manage_data {
    u16 volt_ctrl_timer;
    u16 low_power_off_timer;
    u16 auto_power_off_timer;
    battery_volt_ctrl_info_t control_info;
} battery_product_manage_data_t;

static battery_product_manage_data_t g_batt_product_manege_data;
#define battery_manage_get_data()    (&g_batt_product_manege_data)

static u8 battery_manage_save_info_to_flash(battery_volt_ctrl_info_t *test_info)
{
    int ret = syscfg_write(VM_BATTERT_PRODUCT, test_info, sizeof(battery_volt_ctrl_info_t));
    if (ret < 0) {
        return 0;
    }
    return 1;
}

static u8 battery_manage_get_info_from_flash(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    int ret = syscfg_read(VM_BATTERT_PRODUCT, &data->control_info, sizeof(battery_volt_ctrl_info_t));
    if (ret < 0) {
        return 0;
    }
    return 1;
}

u16 battery_manage_get_volt_control_max(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    return data->control_info.batt_max_voltage;
}

u16 battery_manage_get_volt_control_min(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    return data->control_info.batt_min_voltage;
}

u8 battery_manage_get_batt_ctrl_flag(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    return data->control_info.is_volt_ctrl;
}

u8 battery_manage_is_batt_volt_ctrl(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    if ((data->control_info.batt_max_voltage != 0) && (data->control_info.batt_min_voltage != 0) && (data->control_info.is_volt_ctrl != 0)) {
        return 1;
    }
    return 0;
}

static void battery_manage_volt_ctrl_deal(void *priv)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    u16 real_batt_volt = get_vbat_voltage();
    u8 is_5v_online = LVCMP_DET_FILTER_GET();
    u8 is_in_casebox = get_charge_online_flag();

    log_info("min_volt:%d max_volt:%d is_volt_ctrl:%d is_5v_online:%d\n", data->control_info.batt_min_voltage, data->control_info.batt_max_voltage, data->control_info.is_volt_ctrl, is_5v_online);

    //出盒超时10分钟关机
    if (is_in_casebox == 0) {
        if (++data->auto_power_off_timer >= PRODUCT_LINE_AUTO_OFF_PERIOD) {
            log_info("timeout 10min, need shutdown");
            power_set_soft_poweroff();
        }
    } else {
        data->auto_power_off_timer = 0;
    }

    //低于下限超时5分钟关机
    if ((real_batt_volt < data->control_info.batt_min_voltage) && (is_5v_online == 0)) {
        if (++data->low_power_off_timer >= PRODUCT_LINE_LOWER_POWER_PERIOD) {
            log_info("batt volt < min_volt, need shutdown");
            power_set_soft_poweroff();
        }
    } else {
        data->low_power_off_timer = 0;
    }

    if (is_5v_online) {
        //大于上限关闭充电,小于上限100mV复充
        if ((real_batt_volt > data->control_info.batt_max_voltage) && (IS_CHARGE_EN())) {
            charge_close();
        } else if ((real_batt_volt < (data->control_info.batt_max_voltage - 100)) && (!IS_CHARGE_EN())) {
            charge_start();
        }
    }
}

static void battery_manage_control_start()
{
    battery_product_manage_data_t *data = battery_manage_get_data();

    if (data->control_info.batt_min_voltage > data->control_info.batt_max_voltage) {
        /* log_info("batt_min_voltage > batt_max_voltage is error:%d %d\n", data->control_info.batt_min_voltage, data->control_info.batt_max_voltage); */
        return;
    }
    if ((data->control_info.batt_max_voltage > 5000) || (data->control_info.batt_max_voltage < 3000)) {
        /* log_info("data->control_info.batt_max_voltage set error:%d\n", data->control_info.batt_max_voltage); */
        return;
    }
    if ((data->control_info.batt_min_voltage > 5000) || (data->control_info.batt_min_voltage < 3000)) {
        /* log_info("data->control_info.batt_min_voltage set error:%d\n", data->control_info.batt_min_voltage); */
        return;
    }

    if ((data->control_info.batt_max_voltage != 0) && (data->control_info.batt_min_voltage != 0) && (data->control_info.is_volt_ctrl != 0)) {
        if (data->volt_ctrl_timer == 0) {
            log_info("Control_Start min_volt:%d,max_volt:%d", data->control_info.batt_min_voltage, data->control_info.batt_max_voltage);
            data->volt_ctrl_timer = sys_timer_add(NULL, battery_manage_volt_ctrl_deal, PRODUCT_LINE_HEART_BEAT_INTERVAL);
        }
    }
}

void battery_manage_set_volt_ctl_flag(u8 bat_volt_control)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    /* log_info("is_volt_ctrl:%d\n",bat_volt_control); */
    if (bat_volt_control == 0) {
        data->control_info.is_volt_ctrl = bat_volt_control;
        battery_manage_save_info_to_flash(&data->control_info);
        if (data->volt_ctrl_timer) {
            sys_timer_del(data->volt_ctrl_timer);
            data->volt_ctrl_timer = 0;
        }
        return ;
    } else {
        bat_volt_control = 1;
    }
    data->control_info.is_volt_ctrl = bat_volt_control;
    battery_manage_save_info_to_flash(&data->control_info);
    battery_manage_control_start();
}

void battery_manage_set_batt_upperlimit(u16 max_volt)
{
    battery_product_manage_data_t *data = battery_manage_get_data();

    if ((max_volt < 3000) || (max_volt > 5000)) {
        /* log_info("batt_upperlimit set error:%d\n",max_volt); */
        return;
    }
    data->control_info.batt_max_voltage = max_volt;
    battery_manage_save_info_to_flash(&data->control_info);
    /* log_info("batt_upperlimit:%d\n",data->control_info.batt_max_voltage); */
    battery_manage_control_start();
}

void battery_manage_set_batt_lowerlimit(u16 min_volt)
{
    battery_product_manage_data_t *data = battery_manage_get_data();

    if ((min_volt < 3000) || (min_volt > 5000)) {
        /* log_info("batt_lowerlimit set error:%d\n",min_volt); */
        return;
    }
    data->control_info.batt_min_voltage = min_volt;
    battery_manage_save_info_to_flash(&data->control_info);
    /* log_info("batt_lowerlimit:%d\n",data->control_info.batt_min_voltage); */
    battery_manage_control_start();
}

void battery_manage_clear_batt_ctr(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    data->control_info.batt_max_voltage = 0;
    data->control_info.batt_min_voltage = 0;
    data->control_info.is_volt_ctrl = 0;
    battery_manage_save_info_to_flash(&data->control_info);
    if (data->volt_ctrl_timer) {
        sys_timer_del(data->volt_ctrl_timer);
        data->volt_ctrl_timer = 0;
    }
}

static void battery_manage_volt_mapping(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();

    if (data->control_info.batt_min_voltage == 0xFFFF) {
        data->control_info.batt_min_voltage = 0;
    }
    if (data->control_info.batt_max_voltage == 0xFFFF) {
        data->control_info.batt_max_voltage = 0;
    }
    if (data->control_info.is_volt_ctrl == 0xFF) {
        data->control_info.is_volt_ctrl = 0;
    }
    if ((data->control_info.batt_max_voltage > 5000) || (data->control_info.batt_max_voltage < 3000)) {
        data->control_info.batt_max_voltage = DEFAULT_BATTERY_CONTROL_VOLT_MAX;/******管控电压值不在正常范围内，重置电压值*******/
    }
    if ((data->control_info.batt_min_voltage > 5000) || (data->control_info.batt_min_voltage < 3000)) {
        data->control_info.batt_min_voltage = DEFAULT_BATTERY_CONTROL_VOLT_MIN;/******管控电压值不在正常范围内，重置电压值*******/
    }
    battery_manage_save_info_to_flash(&data->control_info);
}

static int battery_product_manage_init(void)
{
    battery_product_manage_data_t *data = battery_manage_get_data();
    data->low_power_off_timer = 0;
    data->auto_power_off_timer = 0;
    battery_manage_get_info_from_flash();

    battery_manage_volt_mapping();
    if ((data->control_info.batt_max_voltage != 0) && (data->control_info.batt_min_voltage != 0) && (data->control_info.is_volt_ctrl != 0)) {
        if (data->volt_ctrl_timer == 0) {
            data->volt_ctrl_timer = sys_timer_add(NULL, battery_manage_volt_ctrl_deal, PRODUCT_LINE_HEART_BEAT_INTERVAL);
        }
    }
    return 0;
}

late_initcall(battery_product_manage_init);

#endif



