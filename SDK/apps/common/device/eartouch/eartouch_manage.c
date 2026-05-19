#include "eartouch_manage.h"
#include "app_msg.h"
#include "classic/tws_api.h"
#include "bt_tws.h"
#include "sdk_config.h"
#include "asm/power_interface.h"

#if TCFG_OUTSIDE_EARTCH_ENABLE
eartouch_manage_handle eartouch_manage_hdl;
#define __this (&eartouch_manage_hdl)

void eartouch_detect_timer(void *priv)
{
    eartouch_state st =  __this->etch_interface->check(priv);
    if (st == EARTOUCH_STATE_IN_EAR && st != __this->state_hdl->local_st) {
        __this->filter_cnt ++;
        __this->idle_lock = 1;
        if (__this->filter_cnt >= __this->in_filter_cnt) {
            eartouch_event_handler(EARTOUCH_STATE_IN_EAR);
            __this->filter_cnt = 0;
            __this->idle_lock = 0;
        }
    } else if (st == EARTOUCH_STATE_OUT_EAR && st != __this->state_hdl->local_st) {
        __this->filter_cnt ++;
        __this->idle_lock = 1;
        if (__this->filter_cnt >= __this->out_filter_cnt) {
            eartouch_event_handler(EARTOUCH_STATE_OUT_EAR);
            __this->filter_cnt = 0;
            __this->idle_lock = 0;
        }
    }
}


void eartch_wakeup_callback(P33_IO_WKUP_EDGE edge)
{
    eartouch_state st =  __this->etch_interface->check(NULL);
    if (st == EARTOUCH_STATE_IN_EAR && st != __this->state_hdl->local_st) {
        eartouch_event_handler(EARTOUCH_STATE_IN_EAR);
    } else if (st == EARTOUCH_STATE_OUT_EAR && st != __this->state_hdl->local_st) {
        eartouch_event_handler(EARTOUCH_STATE_OUT_EAR);
    }
    //状态改变了需要修改唤醒边沿
    p33_io_wakeup_edge(__this->int_port, gpio_read(__this->int_port) ? FALLING_EDGE : RISING_EDGE);
}

void eartouch_init(eartouch_platform_data *platform_data)
{
    eartouch_interface *etch;
    list_for_each_eartouch(etch) {
        r_printf("logo:%s\n", etch->logo);
        if (!memcmp(etch->logo, platform_data->logo, strlen(platform_data->logo))) {
            if (platform_data->pwr_port != NO_CONFIG_PORT) {
                gpio_set_mode(IO_PORT_SPILT(platform_data->pwr_port), PORT_OUTPUT_HIGH);
            }
            if (etch->init(platform_data) == 0) {
                __this->state_hdl = get_eartouch_state_hdl();
                __this->etch_interface = etch;
                __this->pwr_port = platform_data->pwr_port;
                __this->int_port = platform_data->int_port;
                __this->in_filter_cnt = platform_data->in_filter_cnt;
                __this->out_filter_cnt = platform_data->out_filter_cnt;
                if (etch->interrupt_mode) {             //中断方式注意需要修改power_config.c的唤醒口
                    struct _p33_io_wakeup_config port0 = {
                        .pullup_down_enable = 1,
                        .filter      		= PORT_FLT_DISABLE,
                        .edge               = __this->etch_interface->int_level ? RISING_EDGE :  FALLING_EDGE,
                        .gpio              = __this->int_port,
                        .callback          =  eartch_wakeup_callback,
                    };
                    p33_io_wakeup_port_init(&port0);
                    p33_io_wakeup_enable(IO_PORTB_01, 1);
                    /* power_wakeup_gpio_edge(__this->int_port, __this->etch_interface->int_level ? RISING_EDGE : FALLING_EDGE); */
                    gpio_set_mode(IO_PORT_SPILT(platform_data->int_port), __this->etch_interface->int_level ? PORT_INPUT_PULLDOWN_10K : PORT_INPUT_PULLUP_10K);
                } else {
                    sys_s_hi_timer_add(NULL, eartouch_detect_timer, 10);
                }
            }
            break;
        }
    }
}

static u8 eartouch_idle()
{
    return !__this->idle_lock;
}

REGISTER_LP_TARGET(ear_detect_lp_target) = {
    .name = "eartouch",
    .is_idle = eartouch_idle,
};

#endif/* #if TCFG_OUTSIDE_EARTCH_ENABLE */
