#include "app_config.h"
#include "hr_sensor/hrSensor_manage.h"

#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".hr_manage.data.bss")
#pragma data_seg(".hr_manage.data")
#pragma const_seg(".hr_manage.text.const")
#pragma code_seg(".hr_manage.text")
#endif

#if (TCFG_HRSENSOR_ENABLE == 1)

#if TCFG_HRSENOR_USER_IIC_TYPE
#define _IIC_USE_HW//硬件iic
#endif
#include "iic_api.h"

#define LOG_TAG             "[HRSENSOR]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


static const struct hrsensor_platform_data *platform_data;
static hrsensor_algo_info   hrsensor_info;
HR_SENSOR_INTERFACE *hrSensor_hdl = NULL;
HR_SENSOR_INFO  __hrsensor_info = {.iic_delay = 0};
#define hrSensor_info (&__hrsensor_info)

spinlock_t sensor_iic;
u8 sensor_iic_init_status;

u8 hrsensor_write_nbyte(u8 w_chip_id, u8 register_address, u8 *buf, u8 data_len)
{
    // return i2c_master_write_nbytes_to_device_reg(hrSensor_info->iic_hdl, w_chip_id, &register_address, 1, buf, data_len);
    spin_lock(&sensor_iic);
    u8 write_len = 0;
    u8 i;

    iic_start(hrSensor_info->iic_hdl);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, w_chip_id)) {
        write_len = 0;
        r_printf("\n hrSensor iic w err 0\n");
        goto __wend;
    }

    delay_nops(hrSensor_info->iic_delay);

    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, register_address)) {
        write_len = 0;
        r_printf("\n hrSensor iic w err 1\n");
        goto __wend;
    }

    for (i = 0; i < data_len; i++) {
        delay_nops(hrSensor_info->iic_delay);
        if (0 == iic_tx_byte(hrSensor_info->iic_hdl, buf[i])) {
            write_len = 0;
            r_printf("\n hrSensor iic w err 2\n");
            goto __wend;
        }
        write_len++;
    }

__wend:
    iic_stop(hrSensor_info->iic_hdl);
    spin_unlock(&sensor_iic);
    return write_len;
}

u8 hrsensor_read_nbyte(u8 r_chip_id, u8 register_address, u8 *buf, u8 data_len)
{
    // return i2c_master_read_nbytes_from_device_reg(hrSensor_info->iic_hdl, r_chip_id-1, &register_address,1, buf, data_len);
    spin_lock(&sensor_iic);
    u8 read_len = 0;

    iic_start(hrSensor_info->iic_hdl);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, r_chip_id - 1)) {
        r_printf("\n hrSensor iic r err 0\n");
        read_len = 0;
        goto __rend;
    }

    delay_nops(hrSensor_info->iic_delay);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, register_address)) {
        r_printf("\n hrSensor iic r err 1\n");
        read_len = 0;
        goto __rend;
    }

    iic_start(hrSensor_info->iic_hdl);
    if (0 == iic_tx_byte(hrSensor_info->iic_hdl, r_chip_id)) {
        r_printf("\n hrSensor iic r err 2\n");
        read_len = 0;
        goto __rend;
    }

    delay_nops(hrSensor_info->iic_delay);

    for (; data_len > 1; data_len--) {
        *buf++ = iic_rx_byte(hrSensor_info->iic_hdl, 1);
        read_len ++;
    }

    *buf = iic_rx_byte(hrSensor_info->iic_hdl, 0);
    read_len ++;

__rend:
    iic_stop(hrSensor_info->iic_hdl);
    delay_nops(hrSensor_info->iic_delay);
    spin_unlock(&sensor_iic);

    return read_len;
}

int hr_sensor_io_ctl(u8 cmd, void *arg)
{
    if ((!hrSensor_hdl) || (!hrSensor_hdl->heart_rate_sensor_ctl)) {
        return -1;
    }
    return hrSensor_hdl->heart_rate_sensor_ctl(cmd, arg);
}

int hr_sensor_init(void *_data)
{
    int retval = 0;
    platform_data = (const struct hrsensor_platform_data *)_data;

    hrSensor_info->iic_hdl = platform_data->iic;
    if (sensor_iic_init_status == 0) {
        retval = iic_init(hrSensor_info->iic_hdl, get_iic_config(hrSensor_info->iic_hdl));
        if (retval < 0) {
            g_printf("\n  open iic for hrSensor err\n");
            return retval;
        } else {
            g_printf("\n iic open succ\n");
        }
        sensor_iic_init_status = 1;
    }
    spin_lock_init(&sensor_iic);
    retval = -EINVAL;
    list_for_each_hrsensor(hrSensor_hdl) {
        printf("%s==%s", hrSensor_hdl->logo, platform_data->hrSensor_name);
        if (!memcmp(hrSensor_hdl->logo, platform_data->hrSensor_name, strlen(platform_data->hrSensor_name))) {
            retval = 0;
            break;
        }
    }

    if (retval < 0) {
        log_e(">>>hrSensor_hdl logo err\n");
        return retval;
    }

    if (hrSensor_hdl->heart_rate_sensor_init()) {
        g_printf(">>>>hrSensor_Init SUCC");
        hrSensor_info->init_flag = 1;
    } else {
        g_printf(">>>>hrSensor_Init ERROR");
    }

    return 0;
}

u8 hr_sensor_measure_wear_start(u8 manual, u8 sport)
{
    hrsensor_seting_info seting = {manual, sport};
    return hr_sensor_io_ctl(HR_SENSOR_LP_DETECTION, &seting);
}

u8 hr_sensor_measure_hr_start(u8 manual, u8 sport)
{
    hrsensor_seting_info seting = {manual, sport};
    return hr_sensor_io_ctl(HR_SENSOR_ENABLE, &seting);
}

u8 hr_sensor_measure_hr_stop(void)
{
    return hr_sensor_io_ctl(HR_SENSOR_DISABLE, NULL);
}


u8 hr_sensor_measure_spo2_start(u8 manual, u8 sport)
{
    hrsensor_seting_info seting = {manual, sport};
    return hr_sensor_io_ctl(SPO2_SENSOR_ENABLE, &seting);
}

u8 hr_sensor_measure_spo2_stop(void)
{
    return hr_sensor_io_ctl(SPO2_SENSOR_DISABLE, NULL);
}

// 获取心率传感器数据接口实现
hrsensor_data_t hr_sensor_get_data(void)
{
    hrsensor_data_t data = {0};

#if TCFG_HRSENSOR_ENABLE
    // 对于HX3918传感器
#if TCFG_HX3918_ENABLE
    data.heart_rate = hrsresult.lastesthrs;
    data.spo2 = hrsresult.lastestspo2;
    data.wear_status = hrsresult.wearstatus;
    data.data_valid = (hrsresult.lastesthrs > 0 || hrsresult.lastestspo2 > 0) ? 1 : 0;

#elif TCFG_HX3011_ENABLE
    // 对于HX3011传感器
    data.heart_rate = hrsresult.lastesthrs;
    data.spo2 = hrsresult.lastestspo2;
    data.wear_status = hrsresult.wearstatus;
    data.data_valid = (hrsresult.lastesthrs > 0 || hrsresult.lastestspo2 > 0) ? 1 : 0;
#endif
#endif

    // printf("func:%s, lastesthrs:%d, lastestspo2:%d, wearstatus:%d\n", __FUNCTION__, hrsresult.lastesthrs, hrsresult.lastestspo2, hrsresult.wearstatus);
    // printf("func:%s, heart_rate:%d, spo2:%d, wear_status:%d, data_valid:%d\n", __FUNCTION__, data.heart_rate, data.spo2, data.wear_status, data.data_valid);

    return data;
}

u16 hr_sensor_get_heart_rate(void)
{
    hrsensor_data_t data = hr_sensor_get_data();
    // printf("%s, wear_status:%d, heart_rate:%d\n", __FUNCTION__, data.wear_status, data.heart_rate);
    return data.heart_rate;
}

u8 hr_sensor_get_spo2(void)
{
    hrsensor_data_t data = hr_sensor_get_data();
    // printf("%s, wear_status:%d, spo2:%d\n", __FUNCTION__, data.wear_status, data.spo2);
    return data.spo2;
}

u8 hr_sensor_get_wear_status(void)
{
    hrsensor_data_t data = hr_sensor_get_data();
    // printf("%s, wear_status:%d\n", __FUNCTION__, data.wear_status);
    return data.wear_status;
}
#endif
