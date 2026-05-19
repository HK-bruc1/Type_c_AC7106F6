#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".lp_touch_key_eartch.data.bss")
#pragma data_seg(".lp_touch_key_eartch.data")
#pragma const_seg(".lp_touch_key_eartch.text.const")
#pragma code_seg(".lp_touch_key_eartch.text")
#endif


#if TCFG_LP_EARTCH_KEY_ENABLE


#include "eartouch_manage.h"

#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
#include "lp_touch_key_eartch_algo.h"
#endif

//  选择更新TRIM数值或TRIM使能
enum lp_touch_eartch_trim_info_t {
    LP_TOUCH_EARTCH_TRIM_DC_INFO,
    LP_TOUCH_EARTCH_TRIM_EN_INFO
};

u32 lp_touch_key_eartch_get_state(void)
{
    log_debug("get eartch_detect_result = %d\n", __this->eartch.ear_state);
#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
    return __this->eartch.ear_state;
#else
    return M2P_CTMU_OUTEAR_VALUE_H;
#endif
}

void lp_touch_key_eartch_set_state(u32 state)
{
    __this->eartch.ear_state = state;

#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
#else
    M2P_CTMU_OUTEAR_VALUE_H = state;
#endif
    log_debug("set eartch_detect_result = %d\n", __this->eartch.ear_state);
}

u32 lp_touch_key_eartch_state_is_out_ear(void)
{
    if (lp_touch_key_eartch_get_state()) {
        return 0;
    }
    return 1;
}

u32 lp_touch_key_eartch_get_each_ch_state(void)
{
    u32 state = 0;
#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
    for (u32 group = 0; group < __this->eartch.ch_num; group++) {
        if (lp_touch_key_eartch_algorithm_get_state(group)) {
            state |= BIT(group);
        }
    }
#else
    state = P2M_CTMU_EARTCH_L_IIR_VALUE;

#endif
    log_debug("get eartch_touch_each_ch_state = 0x%x\n", state);
    return state;
}

void lp_touch_key_eartch_set_each_ch_state(u32 state)
{
#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
    for (u32 group = 0; group < __this->eartch.ch_num; group++) {
        if (state & BIT(group)) {
            lp_touch_key_eartch_algorithm_set_state(group, 1);
        } else {
            lp_touch_key_eartch_algorithm_set_state(group, 0);
        }
    }
#else
    M2P_CTMU_OUTEAR_VALUE_L = state;
#endif
    log_debug("set eartch_touch_each_ch_state = 0x%x\n", state);
}

u32 lp_touch_key_eartch_touch_trigger_together(void)
{
#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
    u32 ch_en = 0;
    for (u32 group = 0; group < __this->eartch.ch_num; group++) {
        ch_en |= BIT(group);
    }
#else
    u32 ch_en = M2P_CTMU_EARTCH_CH;
#endif

    if (lp_touch_key_eartch_get_each_ch_state() == ch_en) {
        return 1;
    }
    return 0;
}

u32 lp_touch_key_eartch_touch_trigger_either(void)
{
    if (lp_touch_key_eartch_get_each_ch_state()) {
        return 1;
    }
    return 0;
}

/***************************************************************************************/
/***************************************************************************************/

void __attribute__((weak)) eartch_notify_event(u32 state)
{
    eartouch_event_handler(state);
}

void lp_touch_key_eartch_notify_event(u32 state)
{
    if (get_charge_online_flag()) {
        state = EARTCH_MUST_BE_OUT_EAR;
    }

    u32 user_state;

    switch (state) {
    case EARTCH_MUST_BE_OUT_EAR:
        lp_touch_key_eartch_set_state(0);
        user_state = EARTOUCH_STATE_OUT_EAR;
        break;
    case EARTCH_MUST_BE_IN_EAR:
        lp_touch_key_eartch_set_state(1);
        user_state = EARTOUCH_STATE_IN_EAR;
        break;
    case EARTCH_MAY_BE_OUT_EAR:
        user_state = EARTOUCH_STATE_OUT_EAR;
        break;
    case EARTCH_MAY_BE_IN_EAR:
        user_state = EARTOUCH_STATE_IN_EAR;
        break;
    default :
        user_state = EARTOUCH_STATE_OUT_EAR;
        break;
    }

    if (__this->eartch.user_ear_state != user_state) {
        __this->eartch.user_ear_state = user_state;
        eartch_notify_event(user_state);
    }
}

u32 lp_touch_key_eartch_get_user_ear_state(void)
{
    return __this->eartch.user_ear_state;
}

void lp_touch_key_eartch_inear_valid_timeout(void *priv)
{
    log_debug("eartch_inear_valid_timeout");
    __this->eartch.inear_valid_timeout = 0;
    lp_touch_key_eartch_notify_event(EARTCH_MUST_BE_IN_EAR);
}

void lp_touch_key_eartch_outear_valid_timeout(void *priv)
{
    log_debug("eartch_inear_valid_timeout");
    __this->eartch.outear_valid_timeout = 0;
    lp_touch_key_eartch_notify_event(EARTCH_MUST_BE_OUT_EAR);
}

static void lp_touch_key_eartch_event_deal(u32 state)
{
    log_debug("eartch_touch_state = 0x%x\n", state);

    if (lp_touch_key_eartch_touch_trigger_together()) {
        if (lp_touch_key_eartch_state_is_out_ear()) {
            if (__this->eartch.inear_valid_timeout == 0) {
                __this->eartch.inear_valid_timeout = sys_hi_timeout_add(NULL, lp_touch_key_eartch_inear_valid_timeout, __this->pdata->eartch_inear_filter_time);
            } else {
                sys_hi_timeout_modify(__this->eartch.inear_valid_timeout, __this->pdata->eartch_inear_filter_time);
            }
        }
    } else if (lp_touch_key_eartch_touch_trigger_either()) {
        if (__this->eartch.inear_valid_timeout) {
            sys_hi_timeout_del(__this->eartch.inear_valid_timeout);
            __this->eartch.inear_valid_timeout = 0;
            log_debug("sys_hi_timeout_del  eartch_inear_valid_timeout");
        }
        if (__this->eartch.outear_valid_timeout) {
            sys_hi_timeout_del(__this->eartch.outear_valid_timeout);
            __this->eartch.outear_valid_timeout = 0;
            log_debug("sys_hi_timeout_del  eartch_outear_valid_timeout");
        }
    } else {
        if (0 == lp_touch_key_eartch_state_is_out_ear()) {
            if (__this->eartch.outear_valid_timeout == 0) {
                __this->eartch.outear_valid_timeout = sys_hi_timeout_add(NULL, lp_touch_key_eartch_outear_valid_timeout, __this->pdata->eartch_outear_filter_time);
            } else {
                sys_hi_timeout_modify(__this->eartch.outear_valid_timeout, __this->pdata->eartch_outear_filter_time);
            }
        }
    }
}

void lp_touch_key_eartch_up_trim_value(u32 valid, u16 *trim_buf, u32 kvld_valid, u16 *kvld_buf)
{
    u32 ch, ref_ch;
    u16 val_buf[LPCTMU_CHANNEL_SIZE];
    u16 k_buf[LPCTMU_CHANNEL_SIZE];
    for (ch = 0; ch < LPCTMU_CHANNEL_SIZE; ch++) {
        if ((valid) && (trim_buf)) {
            val_buf[ch] = trim_buf[ch];
        } else {
            val_buf[ch] = 0;
        }
        if ((kvld_valid) && (kvld_buf)) {
            k_buf[ch] = kvld_buf[ch];
        } else {
            k_buf[ch] = 0;
        }
    }

    for (u32 group = 0; group < __this->eartch.ch_num; group++) {
        ch = __this->eartch.ch_list[group];
        ref_ch = __this->eartch.ref_ch_list[group];

#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS

        lp_touch_key_eartch_algorithm_set_dc_trim(group, val_buf[ch], val_buf[ref_ch]);
        lp_touch_key_eartch_algorithm_set_kvld_trim(group, k_buf[ch]);
#else
        M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG1L + ch * 8) = (val_buf[ch] >> 0) & 0xffff;
        M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG1H + ch * 8) = (val_buf[ch] >> 8) & 0xffff;

        M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG1L + ref_ch * 8) = (val_buf[ref_ch] >> 0) & 0xffff;
        M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG1H + ref_ch * 8) = (val_buf[ref_ch] >> 8) & 0xffff;

        M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG3L + ch * 8) = (k_buf[ch] >> 0) & 0xffff;
        M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG3H + ch * 8) = (k_buf[ch] >> 8) & 0xffff;
#endif
    }
}

int lp_touch_key_eartch_get_inear_info(struct eartch_inear_info *inear_info)
{
    int ret = syscfg_read(LP_KEY_EARTCH_TRIM_VALUE, (void *)inear_info, sizeof(struct eartch_inear_info));
    if (ret != sizeof(struct eartch_inear_info)) {
        return 1;
    }
    return 0;
}

int lp_touch_key_eartch_trim_get_all_chs_dc(u16 *trim_dc)
{
    struct eartch_inear_info inear_info = {0};
    int ret = syscfg_read(LP_KEY_EARTCH_TRIM_VALUE, (void *)(&inear_info), sizeof(struct eartch_inear_info));
    if (ret != sizeof(struct eartch_inear_info)) {
        return 1;
    }
    memcpy(trim_dc, inear_info.ctmu_diff_trim, sizeof(inear_info.ctmu_diff_trim));
    return 0;
}

u32 lp_touch_key_eartch_trim_get_valid(void)
{
    struct eartch_inear_info inear_info = {0};
    int ret = syscfg_read(LP_KEY_EARTCH_TRIM_VALUE, (void *)(&inear_info), sizeof(struct eartch_inear_info));
    if (ret != sizeof(struct eartch_inear_info)) {
        return 0;
    }
    return !!inear_info.ctmu_trim_valid;
}

void lp_touch_key_eartch_init(void)
{
    if (__this->eartch.ch_num == 0) {
        return;
    }
    lp_touch_key_eartch_set_state(0);
    __this->eartch.user_ear_state = EARTOUCH_STATE_OUT_EAR;
    __this->eartch.eartch_enbale = 1;

    struct eartch_inear_info inear_info;
    int err = lp_touch_key_eartch_get_inear_info(&inear_info);
    if (err) {
        return;
    }
    if (inear_info.valid == 0) {
        return;
    }
    for (u32 group = 0; group < __this->eartch.ch_num; group++) {
        __this->lpctmu_cfg.ch_fixed_isel[__this->eartch.ch_list[group]] = inear_info.ctmu_ch_isel_level[group];
        __this->lpctmu_cfg.ch_fixed_isel[__this->eartch.ref_ch_list[group]] = inear_info.ctmu_ref_ch_isel_level[group];
    }
    lp_touch_key_eartch_up_trim_value(inear_info.ctmu_trim_valid, inear_info.ctmu_diff_trim, inear_info.ctmu_kvld_valid, inear_info.ctmu_kvld_trim);
}

u32 lp_touch_key_get_wear_ch(u32 group)
{
    if (group < __this->eartch.ref_ch_num) {
        return __this->eartch.ch_list[group];
    } else {
        return 0x0;
    }
}
u32 lp_touch_key_get_wear_ref_ch(u32 group)
{
    if (group < __this->eartch.ref_ch_num) {
        return __this->eartch.ref_ch_list[group];
    } else {
        return 0x0;
    }
}

void lp_touch_key_eartch_save_inear_info(struct eartch_inear_info *inear_info)
{
    if (__this->eartch.ch_num == 0) {
        return;
    }
    inear_info->valid = 1;
    for (u32 group = 0; group < __this->eartch.ch_num; group++) {
        inear_info->ctmu_ch_isel_level[group] = lpctmu_get_ana_cur_level(__this->eartch.ch_list[group]);
        inear_info->ctmu_ref_ch_isel_level[group] = lpctmu_get_ana_cur_level(__this->eartch.ref_ch_list[group]);
    }
    syscfg_write(LP_KEY_EARTCH_TRIM_VALUE, (void *)inear_info, sizeof(struct eartch_inear_info));
}


static void lp_touch_eartch_update_trim_info_callback(void *dat, enum lp_touch_eartch_trim_info_t trim_info)
{
    struct eartch_inear_info inear_info;
    memset(&inear_info, 0, sizeof(inear_info));
    lp_touch_key_eartch_get_inear_info(&inear_info);
    if (trim_info == LP_TOUCH_EARTCH_TRIM_DC_INFO) {
        for (int i = 0; i < LPCTMU_CHANNEL_SIZE; i++) {
            inear_info.ctmu_diff_trim[i] = ((u16 *)dat)[i];
        }
    } else {
        inear_info.ctmu_trim_valid = *((u32 *)dat);
    }
    lp_touch_key_eartch_up_trim_value(inear_info.ctmu_trim_valid, inear_info.ctmu_diff_trim, 0, inear_info.ctmu_kvld_trim);
#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
    lp_touch_key_reset_eartch_algo();
#else
    lpctmu_send_m2p_cmd(RESET_EARTCH_ALGO);
#endif
    lp_touch_key_eartch_save_inear_info(&inear_info);
    free(dat);
}
static void lp_touch_eartch_update_trim_dc(u16 *trim_dc)
{
    int msg[4];
    u8 *dat = malloc(sizeof(u16) * LPCTMU_CHANNEL_SIZE);
    memcpy(dat, trim_dc, sizeof(u16) * LPCTMU_CHANNEL_SIZE);
    msg[0] = (int)lp_touch_eartch_update_trim_info_callback;
    msg[1] = 2;
    msg[2] = (int)dat;
    msg[3] = (int)LP_TOUCH_EARTCH_TRIM_DC_INFO;
    os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
}
static void lp_touch_eartch_update_trim_valid(u32 valid)
{
    int msg[4];
    u8 *dat = malloc(sizeof(u32));
    memcpy(dat, &valid, sizeof(u32));
    msg[0] = (int)lp_touch_eartch_update_trim_info_callback;
    msg[1] = 2;
    msg[2] = (int)dat;
    msg[3] = (int)LP_TOUCH_EARTCH_TRIM_EN_INFO;
    os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
}
typedef struct {
    int Ch1ProductionTestOut;
    int Ch2ProductionTestOut;
    int Ch3ProductionTestOut;
    int Ch4ProductionTestOut;

    int Ch1ProductionTestIn;
    int Ch2ProductionTestIn;
    int Ch3ProductionTestIn;
    int Ch4ProductionTestIn;

    int k12;
    int k34;
} __touch_product_fppar;

#define FP_SCALE_IIR 1000
#define SIGNAL_SCALE 4

//__touch_product_fppar touch_product_fppar1;
void touch_production_init(__touch_product_fppar *touch_product_fppar1)
{
    touch_product_fppar1->Ch1ProductionTestOut = 5555;
    touch_product_fppar1->Ch2ProductionTestOut = 5555;
    touch_product_fppar1->Ch3ProductionTestOut = 5555;
    touch_product_fppar1->Ch4ProductionTestOut = 5555;

    touch_product_fppar1->Ch1ProductionTestIn = 5555;
    touch_product_fppar1->Ch2ProductionTestIn = 5555;
    touch_product_fppar1->Ch3ProductionTestIn = 5555;
    touch_product_fppar1->Ch4ProductionTestIn = 5555;

}

#define PT_SCALE_IIR 100

void touch_product_single_ch(s16 dat, int *dat_iir, int alpha_dn)
{
    /* int cur_iir = *dat_iir / PT_SCALE_IIR; */
    if (*dat_iir == 5555) {
        *dat_iir = (PT_SCALE_IIR) * dat;
    } else {
        *dat_iir = alpha_dn * (*dat_iir) / PT_SCALE_IIR + (PT_SCALE_IIR - alpha_dn) * dat;
    }

}


void touch_production_out_run(s16 datch1, s16 datch2, s16 datch3, s16 datch4, __touch_product_fppar *touch_product_fppar1)
{
    int *Ch1ProductionTestOut = &touch_product_fppar1->Ch1ProductionTestOut;
    int *Ch2ProductionTestOut = &touch_product_fppar1->Ch2ProductionTestOut;
    int *Ch3ProductionTestOut = &touch_product_fppar1->Ch3ProductionTestOut;
    int *Ch4ProductionTestOut = &touch_product_fppar1->Ch4ProductionTestOut;

    touch_product_single_ch(datch1, Ch1ProductionTestOut, 99); // dat_ch1 为输入
    touch_product_single_ch(datch2, Ch2ProductionTestOut, 99); // dat_ch2 为输入
    touch_product_single_ch(datch3, Ch3ProductionTestOut, 99); // dat_ch3 为输入
    touch_product_single_ch(datch4, Ch4ProductionTestOut, 99); // dat_ch4 为输入

}
void touch_production_in_run(s16 datch1, s16 datch2, s16 datch3, s16 datch4, __touch_product_fppar *touch_product_fppar1)
{
    int *Ch1ProductionTestIn = &touch_product_fppar1->Ch1ProductionTestIn;
    int *Ch2ProductionTestIn = &touch_product_fppar1->Ch2ProductionTestIn;
    int *Ch3ProductionTestIn = &touch_product_fppar1->Ch3ProductionTestIn;
    int *Ch4ProductionTestIn = &touch_product_fppar1->Ch4ProductionTestIn;

    touch_product_single_ch(datch1, Ch1ProductionTestIn, 99); // dat_ch1 为输入
    touch_product_single_ch(datch2, Ch2ProductionTestIn, 99); // dat_ch2 为输入
    touch_product_single_ch(datch3, Ch3ProductionTestIn, 99); // dat_ch3 为输入
    touch_product_single_ch(datch4, Ch4ProductionTestIn, 99); // dat_ch4 为输入

}

void touch_production_calk(__touch_product_fppar touch_product_fppar1, int aim, int *k12, int *k34)
{
    int Ch1ProductionTestOut = touch_product_fppar1.Ch1ProductionTestOut;
    int Ch2ProductionTestOut = touch_product_fppar1.Ch2ProductionTestOut;
    int Ch3ProductionTestOut = touch_product_fppar1.Ch3ProductionTestOut;
    int Ch4ProductionTestOut = touch_product_fppar1.Ch4ProductionTestOut;
    int Ch1ProductionTestIn = touch_product_fppar1.Ch1ProductionTestIn;
    int Ch2ProductionTestIn = touch_product_fppar1.Ch2ProductionTestIn;
    int Ch3ProductionTestIn = touch_product_fppar1.Ch3ProductionTestIn;
    int Ch4ProductionTestIn = touch_product_fppar1.Ch4ProductionTestIn;

    /* int a = (Ch1ProductionTestOut - Ch1ProductionTestIn) / PT_SCALE_IIR; */
    /* int b = (1 + (Ch1ProductionTestOut - Ch1ProductionTestIn) / PT_SCALE_IIR - (Ch2ProductionTestOut - Ch2ProductionTestIn) / PT_SCALE_IIR); */
    /* int c = Ch1ProductionTestOut - Ch1ProductionTestIn; */

    *k12 = aim * 1000 / (1 + (Ch1ProductionTestOut - Ch1ProductionTestIn) / PT_SCALE_IIR - (Ch2ProductionTestOut - Ch2ProductionTestIn) / PT_SCALE_IIR);
    *k34 = aim * 1000 / (1 + (Ch3ProductionTestOut - Ch3ProductionTestIn) / PT_SCALE_IIR - (Ch4ProductionTestOut - Ch4ProductionTestIn) / PT_SCALE_IIR);

}

#define STABLE_WATING_COUNT        10
#define LP_TOUCH_TRIM_DC_MAX_COUNT (50 + STABLE_WATING_COUNT)
static __touch_product_fppar touch_product_fppar;
static void lp_touch_eartch_trim_dc_handler(u16 *res_buf)
{
    static u16 counting = 0;
    u16 trim_result[LPCTMU_CHANNEL_SIZE];
    if (__this->eartch.trim_start_flag) {
        if (__this->eartch.trim_start_flag == 1 && counting == 0) { //out
            touch_production_init(&touch_product_fppar);
        }
        counting++;
        if (counting < STABLE_WATING_COUNT) {
            return;
        }
        u32 wear0_ch = lp_touch_key_get_wear_ch(0);
        u32 wear0_ref_ch = lp_touch_key_get_wear_ref_ch(0);
        u32 wear1_ch = lp_touch_key_get_wear_ch(1);
        u32 wear1_ref_ch = lp_touch_key_get_wear_ref_ch(1);
        switch (__this->eartch.trim_start_flag) {
        case 1:/* out */
            touch_production_out_run(res_buf[wear0_ch], res_buf[wear0_ref_ch], res_buf[wear1_ch], res_buf[wear1_ref_ch], &touch_product_fppar);
            break;
        case 2:/* in  */
            touch_production_in_run(res_buf[wear0_ch], res_buf[wear0_ref_ch], res_buf[wear1_ch], res_buf[wear1_ref_ch], &touch_product_fppar);
            break;
        default:
            break;
        }
        if (counting >= LP_TOUCH_TRIM_DC_MAX_COUNT) {
            if (__this->eartch.trim_start_flag == 2) { // in
                memset(trim_result, 0, sizeof(trim_result));
                trim_result[wear0_ch] = (u16)(touch_product_fppar.Ch1ProductionTestOut / PT_SCALE_IIR);
                trim_result[wear0_ref_ch] = (u16)(touch_product_fppar.Ch2ProductionTestOut / PT_SCALE_IIR);
                trim_result[wear1_ch] = (u16)(touch_product_fppar.Ch3ProductionTestOut / PT_SCALE_IIR);
                trim_result[wear1_ref_ch] = (u16)(touch_product_fppar.Ch4ProductionTestOut / PT_SCALE_IIR);
                lp_touch_eartch_update_trim_dc(trim_result);
            }
            counting = 0;
            __this->eartch.trim_start_flag = 0;
        }
    }
}

void lp_touch_eartch_trim_dc_start(u32 is_out)
{
    if (is_out) {
        __this->eartch.trim_start_flag = 1;
#if TOUCH_KEY_EARTCH_ALGO_IN_MSYS
        lp_touch_key_reset_eartch_algo();
        lp_touch_key_reset_algo();
#else
        lpctmu_send_m2p_cmd(RESET_ALL_ALGO_WITH_TRIM);
#endif
    } else {
        __this->eartch.trim_start_flag = 2;
    }
}
u32 lp_touch_key_eartch_get_trim_dc_flag(void)
{
    return __this->eartch.trim_start_flag;
}

void lp_touch_eartch_set_trim_valid(u32 valid)
{
    lp_touch_eartch_update_trim_valid(valid);
}

void lp_touch_eartch_trim_dc_clear(void)
{
    u16 tmp[LPCTMU_CHANNEL_SIZE] = {0};
    u32 valid = 0;
    lp_touch_eartch_update_trim_info_callback(tmp, LP_TOUCH_EARTCH_TRIM_DC_INFO);
    lp_touch_eartch_update_trim_info_callback(&valid, LP_TOUCH_EARTCH_TRIM_EN_INFO);
}

u32 lp_touch_key_eartch_get_ch_num(void)
{
    return __this->eartch.ch_num;
}

void lp_touch_key_testbox_inear_trim(u32 flag)
{
}

#endif

