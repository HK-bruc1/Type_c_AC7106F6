/**
* 注意点：
* 0.此文件变化，在工具端会自动同步修改到工具配置中
* 1.功能块通过【---------xxx------】和 【#endif // xxx 】，是工具识别的关键位置，请勿随意改动
* 2.目前工具暂不支持非文件已有的C语言语法，此文件应使用文件内已有的语法增加业务所需的代码，避免产生不必要的bug
* 3.修改该文件出现工具同步异常或者导出异常时，请先检查文件内语法是否正常
**/


#if TCFG_BATTERY_CURVE_ENABLE
const struct battery_curve g_battery_curve_table [] =  {
    {
        .voltage = 3300,
        .percent = 0
    },
    {
        .voltage = 3450,
        .percent = 5
    },
    {
        .voltage = 3680,
        .percent = 10
    },
    {
        .voltage = 3740,
        .percent = 20
    },
    {
        .voltage = 3770,
        .percent = 30
    },
    {
        .voltage = 3790,
        .percent = 40
    },
    {
        .voltage = 3820,
        .percent = 50
    },
    {
        .voltage = 3870,
        .percent = 60
    },
    {
        .voltage = 3920,
        .percent = 70
    },
    {
        .voltage = 3980,
        .percent = 80
    },
    {
        .voltage = 4060,
        .percent = 90
    },
    {
        .voltage = 4120,
        .percent = 100
    }
};
#endif // TCFG_BATTERY_CURVE_ENABLE

#if TCFG_IO_CFG_AT_POWER_ON
const struct gpio_cfg_item g_io_cfg_at_poweron [] =  {
    
};
#endif // TCFG_IO_CFG_AT_POWER_ON

#if TCFG_IO_CFG_AT_POWER_OFF
const struct gpio_cfg_item g_io_cfg_at_poweroff [] =  {
    
};
#endif // TCFG_IO_CFG_AT_POWER_OFF

#if TCFG_IOKEY_ENABLE
const struct iokey_info g_iokey_info [] =  {
    {
        .key_value = KEY_POWER,
        .key_io = IO_PORTB_04,
        .detect = 0,
        .long_press_reset_enable = 0,
        .long_press_reset_time = 8
    }
};
#endif // TCFG_IOKEY_ENABLE

#if TCFG_ADKEY_ENABLE
const struct adkey_res_value adkey_res_table [] =  {
    {
        .key_value = KEY_AD_NUM0,
        .res_value = 0
    },
    {
        .key_value = KEY_AD_NUM1,
        .res_value = 62
    },
    {
        .key_value = KEY_AD_NUM2,
        .res_value = 150
    },
    {
        .key_value = KEY_AD_NUM3,
        .res_value = 330
    }
};

const struct adkey_info g_adkey_data =  {
    .key_io = IO_PORTB_04,
    .pull_up_type = 1,
    .pull_up_value = 220,
    .max_ad_value = 1023,
    .long_press_reset_enable = 0,
    .long_press_reset_time = 8,
    .res_table = adkey_res_table
};
#endif // TCFG_ADKEY_ENABLE

#if TCFG_MICKEY_ENABLE
const struct mickey_res_value mickey_res_table =  {
    .pp_res_value = 0,
    .up_res_value = 200,
    .down_res_value = 400
};
#endif // TCFG_MICKEY_ENABLE

const struct touch_key_cfg lp_touch_key_table [] =  {
    {
        .key_ch = LPCTMU_CH4_PB4,
        .key_value = KEY_POWER,
        .wakeup_enable = 0,
        .eartch_en = 0,
        .index = 0,
        .algo_cfg = {
            {
                .algo_cfg0 = 20,
                .algo_cfg1 = 25,
                .algo_cfg2 = 80,
                .algo_range_min = 50,
                .algo_range_max = 500,
                .range_sensity = 6
            }
        }
    }
};
