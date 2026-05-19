#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_iot_power_manager.data.bss")
#pragma data_seg(".ble_iot_power_manager.data")
#pragma const_seg(".ble_iot_power_manager.text.const")
#pragma code_seg(".ble_iot_power_manager.text")
#endif
#include "ble_iot_power_manager.h"
#include "app_config.h"
#include "app_power_manage.h"
#include "os/os_api.h"
#include "bt_tws.h"
#include "asm/charge.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

#if 0
#define log_info(x, ...)       printf("[LL_LOG_POWER]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

struct _qiot_power_handle {
    u8 bat_charge_L;
    u8 bat_charge_R;
    u8 bat_charge_C;
    u8 bat_percent_L;
    u8 bat_percent_R;
    u8 bat_percent_C;
} qiot_power_handle = {0};
#define __this (&qiot_power_handle)

#define QIOT_POWER_UPDATE_TIME_INTERVAL 			3 // 3s需要更新一次电量

u32 last_update_time = 0;

/**
 * @brief 更新缓存的电量信息，获取电量信息前调用一下
 *
 * @return 是否更新到最新
 */
bool iot_update_dev_battery_level(void)
{
    bool is_update = FALSE;

    u32 current_time = jiffies_msec();
    u32 time_interval = (current_time - last_update_time) / 1000;
    bool need_update = (time_interval >= QIOT_POWER_UPDATE_TIME_INTERVAL) ? TRUE : FALSE;

    if (!need_update) {
        return is_update;
    }
    last_update_time = current_time;

    u8 master_bat = 0;
    u8 master_charge = 0;
    u8 slave_bat = 0;
    u8 slave_charge = 0;
    u8 box_bat = 0;
    u8 box_charge = 0;

//	Master bat
#if CONFIG_DISPLAY_DETAIL_BAT
    master_bat = get_vbat_percent();
#else
    master_bat = get_self_battery_level() * 10 + 10;
#endif
    if (master_bat > 100) {
        master_bat = 100;
    }
    master_charge = get_charge_online_flag();

// Slave bat

#if	TCFG_USER_TWS_ENABLE
    slave_bat = get_tws_sibling_bat_persent();
#if TCFG_CHARGESTORE_ENABLE
    if (slave_bat == 0xff) {
        /* log_info("--update_bat_01\n"); */
        if (get_bt_tws_connect_status()) {
            slave_bat = chargestore_get_sibling_power_level();
        }
    }
#endif

    if (slave_bat == 0xff) {
        /*  log_info("--update_bat_02\n"); */
        slave_bat = 0x00;
    }

    slave_charge = !!(slave_bat & 0x80);
    slave_bat &= 0x7F;
#else
    slave_charge = 0;
    slave_bat = 0;
#endif

// Box bat
    if ((master_charge && (master_bat != 0))
        || (slave_charge && (slave_bat != 0))) {
        //earphone in charge
#if TCFG_CHARGESTORE_ENABLE
        box_bat = chargestore_get_power_level();
        if (box_bat == 0xFF) {
            box_bat = 0;
        }
#else
        box_bat = 0;
#endif
    } else {
        //all not in charge
        box_bat = 0;
    }
    box_charge = !!(box_bat & 0x80);
    box_bat &= 0x7F;
// check if master is L
    u8 tbat_charge_L = 0;
    u8 tbat_charge_R = 0;
    u8 tbat_percent_L = 0;
    u8 tbat_percent_R = 0;
    u8 tbat_percent_C = box_bat;
    u8 tbat_charge_C = box_charge;
#if TCFG_USER_TWS_ENABLE
    if (get_bt_tws_connect_status()) {
        if ('L' == tws_api_get_local_channel()) {
            tbat_charge_L = master_charge;
            tbat_charge_R = slave_charge;
            tbat_percent_L = master_bat;
            tbat_percent_R = slave_bat;
        } else if ('R' == tws_api_get_local_channel()) {
            tbat_charge_L = slave_charge;
            tbat_charge_R = master_charge;
            tbat_percent_L = slave_bat;
            tbat_percent_R = master_bat;
        }
    } else {
        switch (tws_api_get_local_channel()) {
        case 'R':
            tbat_charge_R = master_charge;
            tbat_percent_R = master_bat;
            break;
        case 'L':
        default:
            tbat_charge_L = master_charge;
            tbat_percent_L = master_bat;
            break;
        }
    }
#else
    tbat_charge_L = master_charge;
    tbat_percent_L = master_bat;
#endif

#if 0//TCFG_CHARGESTORE_ENABLE
    u8 tpercent = 0;
    u8 tcharge = 0;
    if (chargestore_get_earphone_pos() == 'L') {
        //switch position
        log_info("is L pos\n");
        tpercent = tbat_percent_R;
        tcharge = tbat_charge_R;
        tbat_percent_R = tbat_percent_L;
        tbat_charge_R = tbat_charge_L;
        tbat_percent_L = tpercent;
        tbat_charge_L = tcharge;
    } else {
        log_info("is R pos\n");
    }
#endif

    if ((!!__this->bat_charge_L) || (!!__this->bat_charge_R) || (!!__this->bat_charge_C)) {
        if (((__this->bat_percent_C + 1) & 0x7F) <= 0x2) {
            __this->bat_percent_C = 0x2;
        }
    }

    // 防止充电仓电量跳变
    if (__this->bat_percent_C && ((!!tbat_charge_L) || (!!tbat_charge_R))) {
        // 左耳或右耳进仓
        if ((!!tbat_charge_C)) {
            // 充电仓充电, 新值比旧值大
            if ((__this->bat_percent_C > tbat_percent_C) && (__this->bat_percent_C == (tbat_percent_C + 1))) {
                tbat_percent_C = __this->bat_percent_C;
            }
        } else {
            // 充电仓没有充电, 新值比旧值小
            if ((__this->bat_percent_C < tbat_percent_C) && ((__this->bat_percent_C + 1) == tbat_percent_C)) {
                tbat_percent_C = __this->bat_percent_C;
            }
        }
    }
    // check if change
    if ((tbat_charge_L  != __this->bat_charge_L)
        || (tbat_charge_R  != __this->bat_charge_R)
        || (tbat_charge_C  != __this->bat_charge_C)
        || (tbat_percent_L != __this->bat_percent_L)
        || (tbat_percent_R != __this->bat_percent_R)
        || (tbat_percent_C != __this->bat_percent_C)) {
        is_update = TRUE;
    }

    __this->bat_charge_L  = tbat_charge_L;
    __this->bat_charge_R  = tbat_charge_R;
    __this->bat_charge_C  = tbat_charge_C;
    __this->bat_percent_L = tbat_percent_L;
    __this->bat_percent_R = tbat_percent_R;
    __this->bat_percent_C = tbat_percent_C;

    return is_update;
}

/**
 * @brief 获取左耳电量百分比信息
 */
u8 iot_get_leftear_battery_percentage()
{
    iot_update_dev_battery_level();
    u8 battery_percentage = 0;

    battery_percentage = __this->bat_percent_L ? (((!!__this->bat_charge_L) << 7) | (__this->bat_percent_L & 0x7F)) : 0;

    log_info("%s, bat:%d\n", __FUNCTION__, battery_percentage);
    return battery_percentage;
}

/**
 * @brief 获取右耳电量百分比信息
 */
u8 iot_get_rightear_battery_percentage()
{
    iot_update_dev_battery_level();
    u8 battery_percentage = 0;

    battery_percentage = __this->bat_percent_R ? (((!!__this->bat_charge_R) << 7) | (__this->bat_percent_R & 0x7F)) : 0;

    log_info("%s, bat:%d\n", __FUNCTION__, battery_percentage);
    return battery_percentage;
}

/**
 * @brief 获取充电仓电量百分比信息
 */
u8 iot_get_box_battery_percentage()
{
    iot_update_dev_battery_level();
    u8 battery_percentage = 0;

    battery_percentage = __this->bat_percent_C ? (((!!__this->bat_charge_C) << 7) | (__this->bat_percent_C & 0x7F)) : 0;

    log_info("%s, bat:%d\n", __FUNCTION__, battery_percentage);
    return battery_percentage;
}


#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)
