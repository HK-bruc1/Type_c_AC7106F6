#include "eartouch_manage.h"
#include "gpio.h"
#include "sdk_config.h"

#if TCFG_EARTCH_OUTSIDE_TOUCH_ENABLE

eartouch_platform_data *platform_data;

eartouch_state outside_tch_check(void *priv)
{
    u8 gpio_st = gpio_read(platform_data->int_port);
    if (gpio_st == platform_data->int_level) {
        return EARTOUCH_STATE_IN_EAR;
    } else {
        return EARTOUCH_STATE_OUT_EAR;
    }
}

u8 outside_tch_init(eartouch_platform_data  *cfg)
{
    ASSERT(cfg);
    platform_data = cfg;
    return 0;
}

REGISTER_EARTOUCH(eartouch_tch) = {
    .logo = "outside_touch",
    .interrupt_mode = EARTCH_INTERRUPT_MODE,
    .int_level = 0,
    .init = outside_tch_init,
    .check = outside_tch_check,
};

#endif

