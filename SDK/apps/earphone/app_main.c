#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".app_main.data.bss")
#pragma data_seg(".app_main.data")
#pragma const_seg(".app_main.text.const")
#pragma code_seg(".app_main.text")
#endif
#include "system/includes.h"
#include "btstack/btstack_task.h"
#include "app_config.h"
#include "app_action.h"
#include "gpadc.h"
#include "app_tone.h"
#include "gpio_config.h"
#include "app_main.h"
#include "asm/charge.h"
#include "update.h"
#include "app_power_manage.h"
#include "audio_config.h"
#include "app_charge.h"
#include "bt_profile_cfg.h"
#include "update_loader_download.h"
#include "idle.h"
#include "bt_tws.h"
#include "key_driver.h"
#include "adkey.h"
#include "user_cfg.h"
#include "usb/device/usb_stack.h"
#include "usb/usb_task.h"
#include "dev_manager.h"
#include "vm.h"
#include "iokey.h"
#include "asm/lp_touch_key_api.h"
#include "fs/sdfile.h"
#include "utils/syscfg_id.h"
#include "asm/power_interface.h"
#include "app_default_msg_handler.h"
#include "earphone.h"
#include "usb/otg.h"
#include "power_on.h"
#include "app_key_dut.h"
#include "linein.h"
#include "pc.h"
#include "app_music.h"
#include "rcsp_user_api.h"
#include "pwm_led/led_ui_api.h"
#include "dual_bank_updata_api.h"
#include "effect/effects_dev.h"
#include "audio_anc_includes.h"
#include "mix_record_api.h"
#include "lib_update_config.h"
#include "app_mode_sink.h"
#include "audio_avc.h"
#include "rtc_alarm_bridge.h"
#include "rtc_test.h"

#define LOG_TAG             "[APP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


/*任务列表 */
const struct task_info task_info_table[] = {
    {"app_core",            1,     0,    1024,   512 },
    {"btctrler",            4,     0,   512,   512 },
    {"btencry",             1,     0,   512,   128 },
#if (BT_FOR_APP_EN)
    {"btstack",             3,     0,   1024,  512 },
#else
    {"btstack",             3,     0,   768,   256 },
#endif
    {"jlstream",            3,     0,   768,   128 },
    {"jlstream_0",          5,     0,  768,   0 },
    {"jlstream_1",          5,     0,  768,   0 },
    {"jlstream_2",          5,     0,  768,   0 },
    {"jlstream_3",          5,     0,  768,   0 },
    {"jlstream_4",          5,     0,  768,   0 },
    {"jlstream_5",          5,     0,  768,   0 },
    {"jlstream_6",          5,     0,  768,   0 },
    {"jlstream_7",          5,     0,  768,   0 },

#if TCFG_VIRTUAL_SURROUND_PRO_MODULE_NODE_ENABLE
    /*virtual surround pro*/

    {"media0",          5,     0,  768,   0 },
    {"media1",          5,     0,  768,   0 },
    {"media2",          5,     0,  768,   0 },
#endif

#if (TCFG_BT_SUPPORT_LHDC || TCFG_BT_SUPPORT_LHDC_V5)
    {"a2dp_dec",            4,     1,   256 + 512,   0 },
#else
    {"a2dp_dec",            4,     1,   256,   0 },
#endif

    {"aec",					2,	   1,   768,   128 },
#if (defined(EFFECT_DEV_MULTI_TASK_ENABLE) && EFFECT_DEV_MULTI_TASK_ENABLE)
    {"eff_mtask",					6,	   1,   768,   0 },
#endif
    /*
     *file dec任务不打断jlstream任务运行,故优先级低于jlstream
     */
    {"file_dec",            4,     0,  640,   0 },
    {"file_cache",          6,     0,  512 - 128,   0 },
    {"write_file",		    5,	   0,  512,   0 },
    {"aec_dbg",				3,	   0,   512,   128 },
    {"update",				1,	   0,   512,   0   },
    {"tws_ota",				2,	   0,   256,   0   },
    {"tws_ota_msg",			2,	   0,   256,   128 },
    {"dw_update",		 	1,	   0,   512,   128 },
#if (CONFIG_USER_FILE_UPDATE_V2_EN)
    {"ex_f_update",			1,	    1,  512,   0    },
#endif
#if TCFG_AUDIO_DATA_EXPORT_DEFINE
    {"aud_capture",         4,     0,   512,   256 },
    {"data_export",         5,     0,   512,   256 },
#endif
    {"dac",                 2,     0,   256,   128 },

#ifdef CONFIG_BOARD_AISPEECH_NR
    {"aispeech_enc",		2,	   1,   512,   128 },
#endif
#if RCSP_MODE
    {"rcsp",		    	1,	   0,   768,   128 },
#if RCSP_FILE_OPT
    {"rcsp_file_bs",		1,	   0,   768,   128 },
#endif
#if (RCSP_TONE_FILE_TRANSFER_ENABLE && TCFG_USER_TWS_ENABLE)
    {"rcsp_ft_tws",			1,	   0,   256,   128 },
#endif
#endif
#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
    {"kws",                 2,     0,   256,   64  },
#endif

#if TCFG_LP_TOUCH_KEY_ENABLE
    {"lp_touch_key",        5,     0,   256,   32  },
#endif


#if TCFG_USB_SLAVE_ENABLE || TCFG_USB_HOST_ENABLE
    {"usb_stack",          	1,     0,   512,   128 },
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & (GFPS_EN | REALME_EN | TME_EN | DMA_EN | GMA_EN | MMA_EN | FMNA_EN))
    {"app_proto",           2,     0,   768,   64  },
#endif
#if TCFG_RDX_ENABLE
    {"rdx_record",          4,     0,   512,   128 },
#endif
    //{"ui",                  3,     0,   384 - 64,  128  },
#if (TCFG_DEV_MANAGER_ENABLE)
    {"dev_mg",           	3,     0,   512,   512 },
#endif

#if (TCFG_PWMLED_ENABLE)
    {"led_driver",         	5,     0,   256,   128 },
#endif

#if TCFG_LP_TOUCH_KEY_ENABLE
    {"lp_touch_key",        5,     0,   256,   32  },
#endif

    {"audio_vad",           1,     1,   512,   128 },
#if TCFG_KEY_TONE_EN
    {"key_tone",            5,     0,   256,   32  },
#endif
#if (THIRD_PARTY_PROTOCOLS_SEL & TUYA_DEMO_EN)
    {"tuya",                2,     0,   640,   256},
#endif
#if CONFIG_P11_CPU_ENABLE
    {"pmu_task",            6,      0,  256,   128  },
#endif
#if TCFG_AUDIO_SPATIAL_EFFECT_ENABLE
    {"imu_sensor",      2,     1,   512,   128 },
#endif
#if (TCFG_AUDIO_SPATIAL_EFFECT_ENABLE || TCFG_AUDIO_SOMATOSENSORY_ENABLE)
    {"imu_trim",            1,     0,   256,   128 },
#endif
//  {"periph_demo",       3,     0,   512,   0 },
    {"CVP_RefTask",	        4,	   0,   256,   128	},

#if AUDIO_ENC_MPT_SELF_ENABLE
    {"enc_mpt_sel",		3,	   0,   512,   128 },
#endif

#if TCFG_AUDIO_ANC_ENABLE
    {"anc",                 3,     0,   512,   128 },
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    {"icsd_anc_0",          5,     0,   256,   128 },
    {"icsd_anc_1",          5,     1,   256,   128 },
    {"icsd_adt_0",          2,     0,   256,   128 },
    {"icsd_adt_1",          2,     1,   256,   128 },
    {"icsd_src",            4,     0,   256,   256 },
    {"speak_to_chat",       2,     0,   512,   128 },
#endif
#if TCFG_AUDIO_ANC_REAL_TIME_ADAPTIVE_ENABLE
    {"rt_anc_0",            3,     0,   512,   128 },
    {"rt_anc_1",            3,     1,   512,   128 },
    {"rt_de",              	1,     1,   512,   128 },
#endif
#if TCFG_AUDIO_ANC_ENABLE && (TCFG_AUDIO_ANC_EXT_VERSION == ANC_EXT_V2)
    {"afq_common",         	1,     1,   512,   128 },
#endif
#if AUDIO_ANC_DATA_EXPORT_VIA_UART
    {"anc_dev",         	1,     0,   512,   128 },
#endif
#endif

#if (TCFG_MIC_EFFECT_ENABLE || TCFG_AUDIO_HEARING_AID_ENABLE)
    /* 混响任务优先级要高 */
    {"mic_effect1",         6,     0,  768,   0 },
    {"mic_effect2",         6,     0,  768,   0 },
#endif
#if (defined TCFG_AUDIO_SOMATOSENSORY_ENABLE && TCFG_AUDIO_SOMATOSENSORY_ENABLE)
    /*Head Action Detection*/
    {"HA_Detect",           2,     0,  512,   0 },
#endif
#if TCFG_ANC_BOX_ENABLE && TCFG_AUDIO_ANC_ENABLE
    {"anc_box",             7,     0,  512,   128},//配置高优先级避免产测被打断
#endif
#if (defined(TCFG_DEBUG_DLOG_ENABLE) && TCFG_DEBUG_DLOG_ENABLE)
    {"dlog",                1,     0,  256,   128 },
#endif
    {"aud_adc_demo",        1,     0,  512,   128 },
    {"aud_dac_demo",        1,     0,  512,   128 },
    {"vad_demo",        	1,     0,  512,   128 },
#if (TCFG_HRSENSOR_ENABLE || TCFG_GSENSOR_ENABLE)
    {"app_sensor",         1,     0,  512,   128 },
#endif
#if (CONFIG_UPDATE_MUTIL_CPU_UART)
    {"update_interactive_uart",   1,  0,   768,   512	},
#endif
#if TCFG_AUDIO_CVP_DEMO_ENABLE
    {"cvp_demo",			3,	   1,   768,   128 },
#endif
#if TCFG_AUDIO_AVC_NODE_ENABLE && AVC_USE_AEC && AVC_ESCO_AEC_PROCESS_TASK
    {"avc_esco_aec",			2,	   0,   512,   0 },
#endif
    {0, 0},
};


APP_VAR app_var;
struct bt_mode_var g_bt_hdl;

__attribute__((weak))
int eSystemConfirmStopStatus(void)
{
    /* 系统进入在未来时间里，无任务超时唤醒，可根据用户选择系统停止，
     * 或者系统定时唤醒(100ms)，或自己指定唤醒时间
     * return:
     *   1:Endless Sleep
     *   0:100 ms wakeup
     *   other: x ms wakeup
     */
#if TCFG_APP_PC_EN && (TCFG_OTG_MODE & OTG_SLAVE_MODE)
    // PC模式充满不关机
    if (get_charge_full_flag() && (usb_otg_online(0) != SLAVE_MODE)) {
#else
    if (get_charge_full_flag()) {
#endif
#if TCFG_PWMLED_ENABLE
        if (!led_ui_state_is_idle()) {
            return 0;
        }
#endif

#if (!TCFG_CHARGESTORE_ENABLE)
        power_set_soft_poweroff();
#endif
        return 1;
    } else {
        return 0;
    }
}

__attribute__((used))
int *__errno()
{
    static int err;
    return &err;
}

_INLINE_
void app_var_init(void)
{
    app_var.play_poweron_tone = 1;
}


u8 get_power_on_status(void)
{
    u8 flag = 0;
#if TCFG_IOKEY_ENABLE
    if (is_iokey_press_down()) {
        flag = 1;
    }
#endif

#if TCFG_ADKEY_ENABLE
    if (is_adkey_press_down()) {
        flag = 1;
    }
#endif

#if TCFG_LP_TOUCH_KEY_ENABLE
    if (lp_touch_key_power_on_status()) {
        flag = 1;
    }
#endif
    return flag;
}


__INITCALL_BANK_CODE
void check_power_on_key(void)
{
    u32 delay_10ms_cnt = 0;
    int key_ng_cnt = 0;

    while (1) {
        wdt_clear();
        os_time_dly(1);

        if (get_power_on_status()) {
            putchar('+');
            delay_10ms_cnt++;
            key_ng_cnt = 0;
            if (delay_10ms_cnt > 70) {
                app_var.poweron_reason = SYS_POWERON_BY_KEY;
                return;
            }
        } else {
            log_info("enter softpoweroff\n");
            delay_10ms_cnt = 0;
            key_ng_cnt++;
            if (key_ng_cnt > 10) {
                app_var.poweroff_reason = SYS_POWEROFF_BY_KEY;
                power_set_soft_poweroff();
            }
        }
    }
}


__attribute__((weak))
u8 get_charge_online_flag(void)
{
    return 0;
}

/*充电拔出,CPU软件复位, 不检测按键，直接开机*/
__INITCALL_BANK_CODE
static void app_poweron_check(int update)
{
#if (CONFIG_BT_MODE == BT_NORMAL)
    if (!update && cpu_reset_by_soft()) {
        app_var.play_poweron_tone = 0;
        return;
    }
#if TCFG_CHARGE_ENABLE
    if (is_ldo5v_wakeup()) {
#if TCFG_CHARGE_OFF_POWERON_EN
        app_var.play_poweron_tone = 0;
        app_var.poweron_reason = SYS_POWERON_BY_OUT_BOX;
        return;
#else
        //拔出关机
        power_set_soft_poweroff();
#endif
    }
#endif

#if TCFG_AUTO_POWERON_ENABLE
    return;
#endif
    check_power_on_key();

#endif
}



__attribute__((weak))
void board_init()
{

}


__INITCALL_BANK_CODE
static struct app_mode *app_task_init()
{
    app_var_init();

#if !(defined(CONFIG_CPU_BR56) || defined(CONFIG_CPU_BR50))
    sdfile_init();
    syscfg_tools_init();
#endif
    cfg_file_parse(0);

    jlstream_init();

    do_early_initcall();
    board_init();
    do_platform_initcall();

#if (defined(TCFG_DEBUG_DLOG_ENABLE) && TCFG_DEBUG_DLOG_ENABLE)
    dlog_init();
    dlog_enable(1);
    dlog_uart_output_set(DLOG_OUTPUT_2_FLASH | dlog_output_type_get());
    dlog_uart_auto_enable_init();
#endif

    key_driver_init();

    do_initcall();
    do_module_initcall();
    do_late_initcall();

    dev_manager_init();


    int update = 0;
    if (CONFIG_UPDATE_ENABLE) {
        update = update_result_deal();
    }
#if (TCFG_MC_BIAS_AUTO_ADJUST && TCFG_AUDIO_ADC_ENABLE)
    mic_capless_trim_init(update);
#endif

    int msg[4] = { MSG_FROM_APP, APP_MSG_GOTO_MODE, 0, 0 };

    if (get_charge_online_flag()) {
#if(TCFG_SYS_LVD_EN == 1)
        vbat_check_init();
#endif
        msg[2] = APP_MODE_IDLE | (IDLE_MODE_CHARGE << 8);
    } else {
        msg[2] = APP_MODE_POWERON;
        check_power_on_voltage();
        app_poweron_check(update);

        app_send_message(APP_MSG_POWER_ON, 0);
    }

#if TCFG_CHARGE_ENABLE
    set_charge_event_flag(1);
#endif

    struct app_mode *mode;
    mode = app_mode_switch_handler(msg);
    ASSERT(mode != NULL);
#if TCFG_APP_RTC_EN
    rtc_alarm_wakeup_event_replay();
    rtc_test_init();
#endif
    return mode;
}

static int g_mode_switch_arg;
static int g_mode_switch_msg[2];

static void retry_goto_mode(void *_arg)
{
    app_send_message(g_mode_switch_msg[0], g_mode_switch_msg[1]);
}

int app_core_get_message(int *msg, int max_num)
{
    while (1) {
        int res = os_taskq_pend(NULL, msg, max_num);
        if (res != OS_TASKQ) {
            continue;
        }
        if (msg[0] & Q_MSG) {
            return 1;
        }
    }
    return 0;
}

int app_get_message(int *msg, int max_num, const struct key_remap_table *key_table)
{
    const struct app_msg_handler *handler;

    app_core_get_message(msg, max_num);

    //消息截获,返回1表示中断消息分发
    for_each_app_msg_prob_handler(handler) {
        if (handler->from == msg[0]) {
            int abandon = handler->handler(msg + 1);
            if (abandon) {
                return 0;
            }
        }
    }
#if RCSP_ADV_KEY_SET_ENABLE
    if (msg[0] == MSG_FROM_KEY) {
        int _msg = rcsp_key_event_remap(msg + 1);
        if (_msg != -1) {
            msg[0] = MSG_FROM_APP;
            msg[1] = _msg;
            log_info("rcsp_key_remap: %d\n", _msg);
        }
    }
#endif

    if (msg[0] == MSG_FROM_KEY && key_table) {
        /*
         * 按键消息映射成当前模式的消息
         */
        struct app_mode *mode = app_get_current_mode();
        if (mode) {
#if TCFG_AUDIO_WIDE_AREA_TAP_ENABLE
            audio_wide_area_tap_ignore_flag_set(1, 1000);
#endif
            int key_msg = app_key_event_remap(key_table, msg + 1);
            log_info(">>>>>key_msg = %d\n", key_msg);
            if (key_msg == APP_MSG_NULL) {
                return 1;
            }
            msg[0] = MSG_FROM_APP;
            msg[1] = key_msg;
#if TCFG_APP_KEY_DUT_ENABLE
            app_key_dut_msg_handler(key_msg);
#endif
        }
    }

    return 1;
}

struct app_mode *app_mode_switch_handler(int *msg)
{
    if (msg[0] != MSG_FROM_APP) {
        return NULL;
    }

    u8 mode_name = msg[2] & 0xff;
    int arg = msg[2] >> 8;
    int err = 0;
    struct app_mode *next_mode = NULL, *traverse_first;

#if TCFG_APP_BT_EN
    /*一些情况不希望退出蓝牙模式*/
    if ((app_get_current_mode() != NULL) &&
        (app_get_current_mode()->name != mode_name) && (bt_app_exit_check() == 0)) {
        log_info("app_mode_switch: bt mode can't exit!, current mode:%d, goto mode:%d\n",
                 app_get_current_mode()->name, mode_name);
        return NULL;
    }
#endif

    switch (msg[1]) {
    case APP_MSG_GOTO_MODE:
        //需要根据具体开发需求选择是否开启切mode也需要缓存flash
        //1.需要注意的缓存到flash需要较长一段时间（1~3s），特别是从蓝牙后台模式切换到蓝牙前台模式，会出现用户明显感知卡顿现象，因此需要具体需求选择性开启。
#if 0
        //如果开启了VM配置项暂存RAM功能则在每次切模式的时候保存数据到vm_flash,避免丢失数据
        if (get_vm_ram_storage_enable()) {
            vm_flush2flash();
        }
#endif //#if 0

        next_mode = app_get_mode_by_name(mode_name);
        break;
    case APP_MSG_GOTO_NEXT_MODE:
        next_mode = app_get_next_mode();
        break;
    default:
        return NULL;
    }
#if TCFG_USER_TWS_ENABLE && TCFG_LOCAL_TWS_ENABLE && TCFG_APP_LINEIN_EN && ((defined TCFG_LINEIN_DETECT_ENABLE) && (TCFG_LINEIN_DETECT_ENABLE == 0))
    if ((msg[1] == APP_MSG_GOTO_NEXT_MODE) && (msg[2] == APP_KEY_MSG_FROM_TWS) && (next_mode->name == APP_MODE_LINEIN)) {
        g_printf("local_tws goto_next_mode msg_from_tws dont goto linein\n");
        return NULL;
    }
#endif
    g_mode_switch_arg = arg;

#if TCFG_APP_BT_EN && TCFG_BT_BACKGROUND_ENABLE
    if (!bt_check_already_initializes()) {
        //如果是后台使能蓝牙还没初始化，需要先记录切换的模式，等到蓝牙初始化完成之后再切回去
        if (next_mode->name != APP_MODE_BT &&
            next_mode->name != APP_MODE_POWERON &&
            next_mode->name != APP_MODE_IDLE) {
            bt_background_set_switch_mode(next_mode->name);
            if ((app_get_current_mode() != NULL) &&
                (app_get_current_mode()->name == APP_MODE_POWERON)) {
                next_mode = app_get_mode_by_name(APP_MODE_BT);
            } else {
                return NULL;
            }
        }
    }
#endif

    /**
     * 循环检查下一个模式是否可以进入
     */
    traverse_first = next_mode;
    do {
        if (app_try_enter_mode(next_mode, arg)) {
            break;
        }
        next_mode = app_next_mode(next_mode);
        if (traverse_first == next_mode) {      //已经遍历了一轮没有可以进入的模式，则跳转到IDLE
            next_mode = app_get_mode_by_name(APP_MODE_IDLE);
            g_mode_switch_arg = IDLE_MODE_WAIT_DEVONLINE;
            break;
        }
    } while (next_mode);

    if ((app_get_current_mode() != NULL) && (next_mode == app_get_current_mode())) {
        return NULL;
    }

#if TCFG_MIX_RECORD_ENABLE
    //切换模式前关闭混合录音
    if (get_mix_recorder_status()) {
        mix_recorder_stop();
    }
#endif // TCFG_MIX_RECORD_ENABLE

    err = app_goto_mode(next_mode->name, arg);

    if (err != 0) {
        g_mode_switch_msg[0] = msg[1];
        g_mode_switch_msg[1] = msg[2];
        if (!sys_timeout_add(NULL, retry_goto_mode, 100)) {
            ASSERT(0, "goto mode no ram!\n");
        }
        return NULL;
    } else {
        return next_mode;
    }
}

#if 0
//用于查找重复出现的字符
static u8 putchar_num = 0;
int (putchar)(int a)    //重定义putchar
{
    uint32_t rets_addr;
    __asm__ volatile("%0 = rets ;" : "=r"(rets_addr));
    if (a == 'a') {   //修改需要查找putchar的字符
        putchar_num++;
    }
    if (putchar_num == 5) {    //打印连续出现5次的字符的rets
        y_printf("char:%c rets:%x\n", a, rets_addr);
        putchar_num =  0;
    }
#ifdef __LOG_ENABLE
    if (log_output_lock() != 0) {
        struct logbuf *lb = log_output_start(1);
        if (lb) {
            log_putchar(lb, a);
            log_output_end(lb);
        }
    } else {
        log_putbyte(a);
        log_output_unlock();
    }
#else
    if (a == '\r') {
        return a;
    }
    putbyte(a);
#endif
    return a;
}
#endif

#if 0
void timer_no_response_callback(const char *task_name, void *func, u32 msec, void *timer, u32 curr_msec)
{
    extern const char *pcTaskName(void *pxTCB);
    extern TaskHandle_t task_get_current_handle(u8 cpu_id);
    if (CPU_CORE_NUM == 2) {
        TaskHandle_t task0 = task_get_current_handle(0);
        TaskHandle_t task1 = task_get_current_handle(1);
        printf("timer_no_response: %s, %p, %d, %p, %d, c0:%s, c1:%s\n", task_name, func, msec, timer, curr_msec, pcTaskName(task0), pcTaskName(task1));
    } else {
        TaskHandle_t task0 = task_get_current_handle(0);
        printf("timer_no_response: %s, %p, %d, %p, %d, c:%s\n", task_name, func, msec, timer, curr_msec, pcTaskName(task0));
    }
    //用于debug任务无响应情况
    task_trace_info_dump(task_name);
}
#endif

#if 0
static void test_printf(void *_arg)
{
    mem_stats();

    int role = tws_api_get_role();
    log_info(">tws role:%d\n", role);   //打印tws主从

    //char channel = tws_api_get_local_channel();
    //log_info(">tws channel:%c\n", channel);    //打印tws通道

    int curr_clk = clk_get("sys");
    log_info(">curr_clk:%d\n", curr_clk);  //打印当前时钟
}
#endif

static void app_task_loop(void *p)
{
    struct app_mode *mode;

    mode = app_task_init();
    //sys_timer_add(NULL, test_printf, 2000);  //定时调试打印
#if CONFIG_FINDMY_INFO_ENABLE || (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)
#if (VFS_ENABLE == 1)
    if (mount(NULL, "mnt/sdfile", "sdfile", 0, NULL)) {
        log_debug("sdfile mount succ");
    } else {
        log_debug("sdfile mount failed!!!");
    }
#if (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)
    int update = 0;
    u32 realme_breakpoint = 0;
    if (CONFIG_UPDATE_ENABLE) {
        update = update_result_deal();
        realme_check_upgrade_area(update);
    }
#endif
#endif /* #if (VFS_ENABLE == 1) */

#else
    extern void flash_w_protected_check_en(u8 en);
    if (support_dual_bank_update_no_erase) {
        if (0 == dual_bank_update_bp_info_get()) {
            norflash_set_write_protect_remove();
            flash_w_protected_check_en(0);
            dual_bank_check_flash_update_area(0);
            flash_w_protected_check_en(1);
            norflash_set_write_protect_en();
        }
    }
#endif

    while (1) {
        app_set_current_mode(mode);

        switch (mode->name) {
        case APP_MODE_IDLE:
            mode = app_enter_idle_mode(g_mode_switch_arg);
            break;
        case APP_MODE_POWERON:
            mode = app_enter_poweron_mode(g_mode_switch_arg);
            break;
#if TCFG_APP_BT_EN
        case APP_MODE_BT:
            mode = app_enter_bt_mode(g_mode_switch_arg);
            break;
#endif
#if TCFG_APP_LINEIN_EN
        case APP_MODE_LINEIN:
            mode = app_enter_linein_mode(g_mode_switch_arg);
            break;
#endif
#if TCFG_APP_PC_EN
        case APP_MODE_PC:
            mode = app_enter_pc_mode(g_mode_switch_arg);
            break;
#endif
#if TCFG_APP_MUSIC_EN
        case APP_MODE_MUSIC:
            mode = app_enter_music_mode(g_mode_switch_arg);
            break;
#endif
#if TCFG_LOCAL_TWS_ENABLE
        case APP_MODE_SINK:
            mode = app_enter_sink_mode(g_mode_switch_arg);
            break;
#endif
        }
    }
}

void app_main()
{
    task_create(app_task_loop, NULL, "app_core");

    os_start(); //no return
    while (1) {
        asm("idle");
    }
}
