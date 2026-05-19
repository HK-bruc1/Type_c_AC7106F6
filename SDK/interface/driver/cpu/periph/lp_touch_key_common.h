#ifndef _LP_TOUCH_KEY_COMMON_H_
#define _LP_TOUCH_KEY_COMMON_H_

#include "typedef.h"
#include "asm/lpctmu_hw.h"


#define CTMU_CHECK_LONG_CLICK_BY_RES    1
#define CTMU_RES_BUF_SIZE               15


enum CTMU_P2M_EVENT {
    CTMU_P2M_CH0_RES_EVENT = 0x50,
    CTMU_P2M_CH0_SHORT_KEY_EVENT,
    CTMU_P2M_CH0_LONG_KEY_EVENT,
    CTMU_P2M_CH0_HOLD_KEY_EVENT,
    CTMU_P2M_CH0_FALLING_EVENT,
    CTMU_P2M_CH0_RAISING_EVENT,

    CTMU_P2M_CH1_RES_EVENT = 0x58,
    CTMU_P2M_CH1_SHORT_KEY_EVENT,
    CTMU_P2M_CH1_LONG_KEY_EVENT,
    CTMU_P2M_CH1_HOLD_KEY_EVENT,
    CTMU_P2M_CH1_FALLING_EVENT,
    CTMU_P2M_CH1_RAISING_EVENT,

    CTMU_P2M_CH2_RES_EVENT = 0x60,
    CTMU_P2M_CH2_SHORT_KEY_EVENT,
    CTMU_P2M_CH2_LONG_KEY_EVENT,
    CTMU_P2M_CH2_HOLD_KEY_EVENT,
    CTMU_P2M_CH2_FALLING_EVENT,
    CTMU_P2M_CH2_RAISING_EVENT,

    CTMU_P2M_CH3_RES_EVENT = 0x68,
    CTMU_P2M_CH3_SHORT_KEY_EVENT,
    CTMU_P2M_CH3_LONG_KEY_EVENT,
    CTMU_P2M_CH3_HOLD_KEY_EVENT,
    CTMU_P2M_CH3_FALLING_EVENT,
    CTMU_P2M_CH3_RAISING_EVENT,

    CTMU_P2M_CH4_RES_EVENT = 0x70,
    CTMU_P2M_CH4_SHORT_KEY_EVENT,
    CTMU_P2M_CH4_LONG_KEY_EVENT,
    CTMU_P2M_CH4_HOLD_KEY_EVENT,
    CTMU_P2M_CH4_FALLING_EVENT,
    CTMU_P2M_CH4_RAISING_EVENT,

    CTMU_P2M_EARTCH_IN_EVENT = 0x78,
    CTMU_P2M_EARTCH_OUT_EVENT,
};

enum {
    EARTCH_MUST_BE_OUT_EAR,
    EARTCH_MUST_BE_IN_EAR,
    EARTCH_MAY_BE_OUT_EAR,
    EARTCH_MAY_BE_IN_EAR,
};

enum {
    EARTCH_NULL,
    EARTCH_MASTER,
    EARTCH_REFERENCE,
};

enum {
    EARTCH_REF_IO_HIGHZ,
    EARTCH_REF_IO_OUTPUT_L,
    EARTCH_REF_IO_OUTPUT_H,
    EARTCH_REF_IO_PU_10K,
    EARTCH_REF_IO_PD_10K,
    EARTCH_REF_IO_PU_100K,
    EARTCH_REF_IO_PD_100K,
};

enum {
    EARTCH_MSYS_IO_CTL_BY_NULL,
    EARTCH_MSYS_IO_CTL_BY_RES0,
    EARTCH_MSYS_IO_CTL_BY_RES1,
    EARTCH_MSYS_IO_CTL_BY_DIODE0,
    EARTCH_MSYS_IO_CTL_BY_DIODE1,
};

enum TOUCH_KEY_EVENT {
    TOUCH_KEY_LONG_EVENT,
    TOUCH_KEY_HOLD_EVENT,
    TOUCH_KEY_FALLING_EVENT,
    TOUCH_KEY_RAISING_EVENT,
};

enum LP_TOUCH_SOFTOFF_MODE {
    LP_TOUCH_SOFTOFF_MODE_LEGACY  = 0, //普通关机
    LP_TOUCH_SOFTOFF_MODE_ADVANCE = 1, //带触摸关机
};

enum touch_key_type {
    TOUCH_KEY_NULL,
    TOUCH_KEY_SHORT_CLICK,
    TOUCH_KEY_LONG_CLICK,
    TOUCH_KEY_HOLD_CLICK,
};

enum {
    TOUCH_KEY_EVENT_SLIDE_UP,
    TOUCH_KEY_EVENT_SLIDE_DOWN,
    TOUCH_KEY_EVENT_SLIDE_LEFT,
    TOUCH_KEY_EVENT_SLIDE_RIGHT,
    TOUCH_KEY_EVENT_MAX,
};

struct touch_key_algo_cfg {
    u16 algo_cfg0;
    u16 algo_cfg1;
    u16 algo_cfg2;
    u16 algo_range_min;
    u16 algo_range_max;
    u8 range_sensity;
};

struct touch_key_cfg {
    u8 wakeup_enable;
    u8 eartch_en;
    u8 key_value;
    u8 key_ch;
    u8 index;
    struct touch_key_algo_cfg algo_cfg[5];
};

struct lp_touch_key_platform_data {

    u8 slide_mode_en;
    u8 slide_mode_key_value;

    u8 ldo_wkp_algo_reset;
    u8 charge_enter_algo_reset;
    u8 charge_exit_algo_reset;
    u8 charge_online_algo_reset;
    u8 charge_online_softoff_wakeup;
    u8 charge_mode_keep_touch;

    u8 key_num;

    u8 eartch_msys_io_ctl;
    u8 eartch_ref_io_mode[2];

    u16 long_press_reset_time;
    u16 softoff_wakeup_time;

    u16 short_click_check_time;
    u16 long_click_check_time;
    u16 hold_click_check_time;

    u16 eartch_inear_filter_time;
    u16 eartch_outear_filter_time;

    const struct touch_key_cfg *key_cfg;

    const struct lpctmu_platform_data *lpctmu_pdata;
};


#define LP_TOUCH_KEY_PLATFORM_DATA_BEGIN(data) \
    const struct lp_touch_key_platform_data data

#define LP_TOUCH_KEY_PLATFORM_DATA_END() \
    .ldo_wkp_algo_reset = 1,\
    .charge_enter_algo_reset = 0,\
    .charge_exit_algo_reset = 1,\
    .charge_online_algo_reset = 1,\
    .charge_online_softoff_wakeup = 0,\
    .softoff_wakeup_time = 1000, \
    .short_click_check_time = 500, \
    .long_click_check_time = 2000, \
    .hold_click_check_time = 200, \
    .eartch_inear_filter_time = 200, \
    .eartch_outear_filter_time = 200, \
    .eartch_msys_io_ctl = 0, \
    .eartch_ref_io_mode = {1, 1}, \
}


struct touch_key_range_algo_data {
    u16 ready_flag;
    u16 range;
    s32 sigma;
};

struct touch_key_arg {
    u8 click_cnt;
    u8 last_key;
    u16 short_timer;
    u16 long_timer;
    u16 hold_timer;
    u16 algo_sta_cnt;
    u16 save_algo_cfg_timeout;

#if CTMU_CHECK_LONG_CLICK_BY_RES
    u16 ctmu_res_buf[CTMU_RES_BUF_SIZE];
    u16 ctmu_res_buf_in;
    u16 falling_res_avg;
    u16 long_event_res_avg;
#endif

    struct touch_key_range_algo_data algo_data;
};

struct eartch_inear_info {
    u8 valid;
    u8 p2m_each_ch_state;
    u8 ctmu_ch_isel_level[2];
    u8 ctmu_ref_ch_isel_level[2];
    u8 ctmu_trim_valid;
    u8 ctmu_kvld_valid;
    u16 ctmu_diff_trim[LPCTMU_CHANNEL_SIZE];
    u16 ctmu_kvld_trim[LPCTMU_CHANNEL_SIZE];
};

struct touch_key_eartch {
    u8  ch_num;
    u8  ch_list[2];
    u8  ref_ch_num;
    u8  ref_ch_list[2];
    u8  ref_io_mode[2];
    u8  eartch_enbale;
    u8  ear_state;
    u8  user_ear_state;
    u8  eartch_algo_state;
    u8  trim_start_flag;
    u16 inear_valid_timeout;
    u16 outear_valid_timeout;
};

struct lp_touch_key_config_data {

#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
    u16 last_touch_state;
    u16 identify_algo_invalid;
#endif
    u16 last_key_action;

    struct touch_key_eartch eartch;

    struct touch_key_arg arg[LPCTMU_CHANNEL_SIZE];

    struct lpctmu_config_data lpctmu_cfg;

    const struct lp_touch_key_platform_data *pdata;
};



u32 lp_touch_key_get_cur_ch_by_idx(u32 ch_idx);

u32 lp_touch_key_get_idx_by_cur_ch(u32 cur_ch);

u32 lp_touch_key_power_on_status(void);

u32 lp_touch_key_alog_range_display(u8 *display_buf);

void lp_touch_key_init(const struct lp_touch_key_platform_data *pdata);

void lp_touch_key_disable(void);

void lp_touch_key_enable(void);

void lp_touch_key_charge_mode_enter(void);

void lp_touch_key_charge_mode_exit(void);


u32 lp_touch_key_eartch_get_state(void);
void lp_touch_key_eartch_set_state(u32 state);
u32 lp_touch_key_eartch_state_is_out_ear(void);
u32 lp_touch_key_eartch_get_each_ch_state(void);
void lp_touch_key_eartch_set_each_ch_state(u32 state);
u32 lp_touch_key_eartch_touch_trigger_together(void);
void lp_touch_key_eartch_notify_event(u32 state);
u32 lp_touch_key_eartch_get_user_ear_state(void);
void lp_touch_key_eartch_event_deal(u32 state);
void lp_touch_key_eartch_init(void);
void lp_touch_key_eartch_state_reset(void);
void lp_touch_key_eartch_save_inear_info(struct eartch_inear_info *inear_info);
void lp_touch_key_testbox_inear_trim(u32 flag);

u32 lp_touch_key_get_wear_ch(u32 group);
u32 lp_touch_key_get_wear_ref_ch(u32 group);
int lp_touch_key_eartch_trim_get_all_chs_dc(u16 *trim_dc);
u32 lp_touch_key_eartch_trim_get_valid(void);
u32 lp_touch_key_eartch_get_trim_dc_flag(void);
void lp_touch_eartch_trim_dc_start(u32 is_out);
void lp_touch_eartch_set_trim_valid(u32 valid);
void lp_touch_eartch_trim_dc_clear(void);
u32 lp_touch_key_eartch_get_ch_num(void);
u32 lp_touch_key_get_last_key_action(void);
u8 lp_touch_key_online_get_state();
u32 lp_touch_key_product_test_get_state(void);
void lp_touch_key_eartch_product_test_recieve_handler(u8 *dat, u16 size);
void lp_touch_key_eartch_product_test_debug_handler(u16 *res_buf);
void lp_touch_key_get_res(u16 *res_buf);
#endif

