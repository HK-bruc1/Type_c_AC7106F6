#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".lp_touch_key.data.bss")
#pragma data_seg(".lp_touch_key.data")
#pragma const_seg(".lp_touch_key.text.const")
#pragma code_seg(".lp_touch_key.text")
#endif
#include "system/includes.h"
#include "init.h"
#include "app_config.h"
#include "app_msg.h"
#include "key_driver.h"
#include "key_event_deal.h"
#include "lp_touch_key_range_algo.h"
#include "lp_touch_key_identify_algo.h"
#include "asm/lp_touch_key_tool.h"
#include "asm/lp_touch_key_api.h"
#include "asm/power_interface.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "btstack/avctp_user.h"



#define LOG_TAG_CONST       LP_KEY
#define LOG_TAG             "[LP_KEY]"
/* #define LOG_ERROR_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"



#if TCFG_LP_TOUCH_KEY_ENABLE


VM_MANAGE_ID_REG(touch_key0_algo_cfg, VM_LP_TOUCH_KEY0_ALOG_CFG);
VM_MANAGE_ID_REG(touch_key1_algo_cfg, VM_LP_TOUCH_KEY1_ALOG_CFG);
VM_MANAGE_ID_REG(touch_key2_algo_cfg, VM_LP_TOUCH_KEY2_ALOG_CFG);
VM_MANAGE_ID_REG(touch_key0_idty_algo, VM_LP_TOUCH_KEY0_IDTY_ALGO);
VM_MANAGE_ID_REG(touch_key1_idty_algo, VM_LP_TOUCH_KEY1_IDTY_ALGO);
VM_MANAGE_ID_REG(touch_key2_idty_algo, VM_LP_TOUCH_KEY2_IDTY_ALGO);

static OS_SEM *touch_ready_sem;
static u8 touch_abandon_short_click_once = 0;
static struct lp_touch_key_config_data lp_touch_key_cfg_data;
#define __this (&lp_touch_key_cfg_data)

static u16 touch_ch_res_buf[LPCTMU_CHANNEL_SIZE];

static u8 key_msg_state = 0;


#if TCFG_LP_EARTCH_KEY_ENABLE
void lp_touch_key_reset_eartch_algo(void);
#endif

void lp_touch_key_reset_algo(void);


void lp_touch_key_save_identify_algo_param(void)
{
    if (!__this->pdata) {
        return;
    }
    if (__this->lpctmu_cfg.ch_wkp_en == 0) {
        return;
    }

    log_debug("save algo param\n");

    u32 tia_size = lp_touch_key_identify_algorithm_get_tia_size();
    u8 *tia_vm_buf = (u8 *)malloc(tia_size * __this->pdata->key_num);
    u32 tia_vm_size = 0;

    const struct touch_key_cfg *key_cfg;
    for (u32 ch_idx = 0; ch_idx < __this->pdata->key_num; ch_idx ++) {
        key_cfg = &(__this->pdata->key_cfg[ch_idx]);
        if (key_cfg->eartch_en) {
            continue;
        }
        void *tia_addr = lp_touch_key_identify_algorithm_get_tia_addr(ch_idx);
        if (tia_addr) {
            memcpy(tia_vm_buf + tia_vm_size, tia_addr, tia_size);
            tia_vm_size += tia_size;
        }
    }
    if (tia_vm_size) {
        int ret = syscfg_write(VM_LP_TOUCH_KEY2_IDTY_ALGO, tia_vm_buf, tia_vm_size);
        if (ret != tia_vm_size) {
            log_debug("write vm tia_algo param error !\n");
        }
    }
    free(tia_vm_buf);
    tia_vm_buf = NULL;
}


#include "../../../components/touch/lp_touch_key_common.c"

#include "../../../components/touch/lp_touch_key_eartch.c"

#include "../../../components/touch/lp_touch_key_click.c"

#include "../../../components/touch/lp_touch_key_slide.c"


void lp_touch_key_state_event_deal(u32 ch_idx, u32 event)
{
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    const struct touch_key_cfg *key_cfg = &(__this->pdata->key_cfg[ch_idx]);

    if (__this->pdata->slide_mode_en) {
        if (event == TOUCH_KEY_FALLING_EVENT) {
            lp_touch_key_fall_click_handle(ch_idx);
        } else if (event == TOUCH_KEY_RAISING_EVENT) {
            lp_touch_key_cnacel_long_hold_click_check(ch_idx);
        }
        u32 key_type = lp_touch_key_check_slide_key_type(event, ch_idx);
        if (key_type) {
            log_debug("touch key%d: key_type = 0x%x\n", ch_idx, key_type);
            lp_touch_key_send_slide_key_type_event(key_type);
        }
    } else {
        switch (event) {
        case TOUCH_KEY_FALLING_EVENT:
            log_debug("touch key%d FALLING !\n", ch_idx);

#if CTMU_CHECK_LONG_CLICK_BY_RES
            arg->falling_res_avg = lp_touch_key_ctmu_res_buf_avg(ch_idx);
            log_debug("falling_res_avg: %d", arg->falling_res_avg);
#endif
            lp_touch_key_fall_click_handle(ch_idx);
            lp_touch_key_send_key_tone_msg();
            break;

        case TOUCH_KEY_RAISING_EVENT:
            log_debug("touch key%d RAISING !\n", ch_idx);

#if CTMU_CHECK_LONG_CLICK_BY_RES
            lp_touch_key_ctmu_res_buf_clear(ch_idx);
#endif
            lp_touch_key_cnacel_long_hold_click_check(ch_idx);
            lp_touch_key_raise_click_handle(ch_idx);
            break;

        case TOUCH_KEY_LONG_EVENT:
            log_debug("touch key%d LONG !\n", ch_idx);

#if CTMU_CHECK_LONG_CLICK_BY_RES
            if (lp_touch_key_check_long_click_by_ctmu_res(ch_idx)) {
                return;
            }
#endif
            lp_touch_key_long_click_handle(ch_idx);
            lp_touch_key_send_key_long_tone_msg();
            break;

        case TOUCH_KEY_HOLD_EVENT:
            log_debug("touch key%d HOLD !\n", ch_idx);

#if CTMU_CHECK_LONG_CLICK_BY_RES
            if (lp_touch_key_check_long_click_by_ctmu_res(ch_idx)) {
                return;
            }
#endif
            lp_touch_key_hold_click_handle(ch_idx);
            break;
        }
    }
}

void lp_touch_key_ctmu_res_deal(u32 pnd_type)
{
    /* if (pnd_type & LPCTMU_KEY_MSG_PND) { */
    /*     log_debug("lpkey msg!"); */
    /* } */
    u32 data_len = lpctmu_get_res_kfifo_length();
    if (data_len == 0) {
        return;
    }

    const struct touch_key_cfg *key_cfg;
    const struct touch_key_algo_cfg *algo_cfg;

    u32 algo_valid;
    u16 ref_lim_l;
    u16 ref_lim_h;
    u16 ch_res, len;
    u32 touch_state;
    u32 ch_idx, ch;

    for (u32 i = 0; i < data_len; i ++) {
#if TCFG_LP_EARTCH_KEY_ENABLE
        len = lpctmu_get_res_kfifo_data((u16 *)&ch_res, 1);
        if (len == 0) {
            return;
        }
        ch = (ch_res >> 13) & 0x7;
        ch_res &= 0x1fff;
        ch_idx = lp_touch_key_get_idx_by_cur_ch(ch);
#else
        ch_idx = lpctmu_get_ch_idx_by_res_kfifo_out();
        ch = lp_touch_key_get_cur_ch_by_idx(ch_idx);
        len = lpctmu_get_res_kfifo_data((u16 *)&ch_res, 1);
        if (len == 0) {
            return;
        }
#endif

        key_cfg = &(__this->pdata->key_cfg[ch_idx]);
        algo_cfg = &(key_cfg->algo_cfg[key_cfg->index]);

        touch_ch_res_buf[ch] = ch_res;

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE
        if ((touch_bt_tool_enable) && (touch_bt_online_debug_send) && ((ch_idx + 1) == __this->pdata->key_num)) {
            touch_bt_online_debug_send(ch, touch_ch_res_buf);
        }
#if TCFG_LP_EARTCH_KEY_ENABLE
        lp_touch_key_eartch_product_test_debug_handler(touch_ch_res_buf);
        lp_touch_eartch_trim_dc_handler(touch_ch_res_buf);
#endif
#endif

        /* log_debug("idx:%d ch:%d, res:%d", ch_idx, ch, ch_res); */

#if TCFG_LP_EARTCH_KEY_ENABLE
        if (key_cfg->eartch_en) {
            u32 group = 1;
            if ((__this->eartch.ch_list[0] == key_cfg->key_ch) || \
                (__this->eartch.ref_ch_list[0] == key_cfg->key_ch)) {
                group = 0;
            }
            if (key_cfg->eartch_en == EARTCH_REFERENCE) {
                u32 tmp_ch = __this->eartch.ch_list[group];
                u32 tmp_ref_ch = __this->eartch.ref_ch_list[group];
                touch_state = lp_touch_key_eartch_algorithm_analyze(group, touch_ch_res_buf[tmp_ch], touch_ch_res_buf[tmp_ref_ch]);
                if (touch_state != (!!(__this->eartch.eartch_algo_state & BIT(group)))) {
                    if (touch_state) {
                        __this->eartch.eartch_algo_state |=  BIT(group);
                    } else {
                        __this->eartch.eartch_algo_state &= ~BIT(group);
                    }
                    lp_touch_key_eartch_event_deal(touch_state);
                }
            }
            continue;
        }
#endif

        algo_valid = 0;
        ref_lim_l = -1;
        ref_lim_h = -1;
        lp_touch_key_identify_algo_get_ref_lim(ch_idx, (u16 *)&ref_lim_l, (u16 *)&ref_lim_h, (u32 *)&algo_valid);
        if (algo_valid) {
            __this->identify_algo_invalid &= ~BIT(ch_idx);
            lpctmu_cache_ch_res_key_msg_lim(ch, ref_lim_l, ref_lim_h);
            if ((ch_res < (ref_lim_l + algo_cfg->algo_cfg0)) || (ch_res > (ref_lim_h - algo_cfg->algo_cfg0))) {
                key_msg_state |=  BIT(ch_idx);
            } else {
                key_msg_state &= ~BIT(ch_idx);
            }
            lpctmu_cur_trim_by_res(ch, 0);
        } else {
            key_msg_state &= ~BIT(ch_idx);
            __this->last_touch_state &= ~BIT(ch_idx);
            __this->identify_algo_invalid |=  BIT(ch_idx);
            if ((data_len <= __this->pdata->key_num) && (key_cfg->eartch_en == 0)) {
                lpctmu_cur_trim_by_res(ch, ch_res);
            }
        }

        if (key_cfg->eartch_en == 0) {
            lp_touch_key_range_algo_analyze(ch_idx, ch_res);
        }

        touch_state = lp_touch_key_identify_algorithm_analyze(ch_idx, ch_res);

        /* log_debug("idx:%d ch:%d, res:%d, L:%d H:%d key:%d", ch_idx, ch, ch_res, ref_lim_l, ref_lim_h, touch_state); */
        /* log_debug("idx:%d ch:%d, res:%d, key:%d", ch_idx, ch, ch_res, touch_state); */
        if (touch_state != (!!(__this->last_touch_state & BIT(ch_idx)))) {
            if (touch_state) {
                __this->last_touch_state |=  BIT(ch_idx);
                lp_touch_key_state_event_deal(ch_idx, TOUCH_KEY_FALLING_EVENT);
            } else {
                __this->last_touch_state &= ~BIT(ch_idx);
                lp_touch_key_state_event_deal(ch_idx, TOUCH_KEY_RAISING_EVENT);
            }
        }
    }

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE
    if (touch_bt_tool_enable) {
        lpctmu_set_dma_res_ie(1);
    } else
#endif
    {
#if TCFG_LP_EARTCH_KEY_ENABLE
        if ((__this->last_touch_state) || \
            (__this->identify_algo_invalid) ||  \
            (__this->eartch.inear_valid_timeout) ||  \
            (__this->eartch.outear_valid_timeout) ||  \
            (key_msg_state)) {
#else
        /* log_debug("key:%x invld:%x",  __this->last_touch_state, __this->identify_algo_invalid); */
        if ((__this->last_touch_state) || \
            (__this->identify_algo_invalid) || \
            (key_msg_state)) {
#endif
            lpctmu_set_dma_res_ie(1);
        } else {
            lpctmu_set_dma_res_ie(0);
        }
    }

    lpctmu_ch_res_key_msg_lim_upgrade();
}

u32 lp_touch_key_get_last_touch_state(void)
{
    if (!__this) {
        return 0;
    }
    u32 state = (key_msg_state << 24) | (__this->identify_algo_invalid << 16) | __this->last_touch_state;
    return state;
}

void lp_touch_key_ctmu_isr_cbfunc(u32 pnd_type)
{
    if (!__this->pdata) {
        return;
    }
    /* log_debug("pnd type 0x%x\n", pnd_type); */
    int msg[1];
    msg[0] = pnd_type;
    os_taskq_post_type("lp_touch_key", MSG_FROM_KEY, 1, msg);
}

void lp_touch_key_task(void *p)
{
    int ret;
    int msg[16];

    u32 ch_idx, ch;
    const struct touch_key_cfg *key_cfg;
    const struct touch_key_algo_cfg *algo_cfg;

    u16 ref_lim_l;
    u16 ref_lim_h;
    u32 algo_valid;

    u32 tia_size = lp_touch_key_identify_algorithm_get_tia_size();
    u8 *tia_vm_buf = (u8 *)malloc(tia_size * __this->pdata->key_num);
    u32 tia_vm_size = 0;
    for (u32 ch_idx = 0; ch_idx < __this->pdata->key_num; ch_idx ++) {
        key_cfg = &(__this->pdata->key_cfg[ch_idx]);
        if (key_cfg->eartch_en) {
            continue;
        }
        tia_vm_size += tia_size;
    }
    u32 read_tia_succ = 0;
    if (tia_vm_size) {
        ret = syscfg_read(VM_LP_TOUCH_KEY2_IDTY_ALGO, tia_vm_buf, tia_vm_size);
        if (ret == tia_vm_size) {
            read_tia_succ = 1;
        }
    }
    tia_vm_size = 0;

    if ((is_wakeup_source(PWR_WK_REASON_P11)) && (is_wakeup_source(PWR_WK_REASON_LPCTMU))) {
        touch_abandon_short_click_once = 1;
    } else {
        touch_abandon_short_click_once = 0;
    }

    key_msg_state = 0;
    __this->last_touch_state = 0;
    __this->identify_algo_invalid = 0;

    __this->lpctmu_cfg.pdata = __this->pdata->lpctmu_pdata;
    __this->lpctmu_cfg.isr_cbfunc = lp_touch_key_ctmu_isr_cbfunc;
    for (ch_idx = 0; ch_idx < __this->pdata->key_num; ch_idx ++) {
        key_cfg = &(__this->pdata->key_cfg[ch_idx]);
        algo_cfg = &(key_cfg->algo_cfg[key_cfg->index]);
        ch = key_cfg->key_ch;
        __this->lpctmu_cfg.ch_num = __this->pdata->key_num;
        __this->lpctmu_cfg.ch_list[ch_idx] = ch;
        __this->lpctmu_cfg.ch_fixed_isel[ch] = 0;
        __this->lpctmu_cfg.ch_en |= BIT(ch);
        __this->lpctmu_cfg.lim_l[ch] = -1;
        __this->lpctmu_cfg.lim_h[ch] = -1;
        __this->lpctmu_cfg.lim_span_max[ch] = algo_cfg->algo_range_max;
        if (__this->lpctmu_cfg.lim_span_max[ch] < algo_cfg->algo_cfg2) {
            __this->lpctmu_cfg.lim_span_max[ch] = algo_cfg->algo_cfg2;
        }
        if ((key_cfg->wakeup_enable) && (key_cfg->eartch_en == 0)) {
            __this->lpctmu_cfg.ch_wkp_en |= BIT(ch);
        }

#if TCFG_LP_EARTCH_KEY_ENABLE
        if (key_cfg->eartch_en) {
            __this->lpctmu_cfg.ear_sample_cnt_max = 2;
        } else {
            __this->lpctmu_cfg.ch_key_num ++;
            __this->lpctmu_cfg.ch_key_en |= BIT(ch);
        }

        __this->lpctmu_cfg.sample_cnt_max  = __this->lpctmu_cfg.ch_key_num;
        __this->lpctmu_cfg.sample_cnt_max += __this->lpctmu_cfg.ear_sample_cnt_max;

        if (key_cfg->eartch_en == EARTCH_MASTER) {
            __this->lpctmu_cfg.ch_ear_en |= BIT(ch);
            __this->lpctmu_cfg.ear_ch_list[__this->eartch.ch_num][0] = ch;

            __this->eartch.ch_list[__this->eartch.ch_num] = ch;
            lp_touch_key_eartch_algorithm_init(__this->eartch.ch_num, 0, algo_cfg->algo_cfg0, algo_cfg->algo_cfg1, algo_cfg->algo_cfg2);
            __this->eartch.ch_num ++;
            continue;
        } else if (key_cfg->eartch_en == EARTCH_REFERENCE) {
            __this->lpctmu_cfg.ear_ref_io_mode[__this->eartch.ref_ch_num] = __this->pdata->eartch_ref_io_mode[__this->eartch.ref_ch_num];
            __this->lpctmu_cfg.ch_ear_en |= BIT(ch);
            __this->lpctmu_cfg.ear_ch_list[__this->eartch.ref_ch_num][1] = ch;
            __this->eartch.ref_ch_list[__this->eartch.ref_ch_num] = ch;
            lp_touch_key_eartch_algorithm_init(__this->eartch.ref_ch_num, 1, algo_cfg->algo_cfg0, algo_cfg->algo_cfg1, algo_cfg->algo_cfg2);
            __this->eartch.ref_ch_num ++;
            __this->lpctmu_cfg.ear_group_cnt_max ++;
            continue;
        }
#endif

        void *tia_addr = malloc(tia_size);
        lp_touch_key_identify_algorithm_set_tia_addr(ch_idx, tia_addr);

        if ((!is_wakeup_source(PWR_WK_REASON_P11)) || \
            (__this->pdata->ldo_wkp_algo_reset && is_ldo5v_wakeup()) || \
            (__this->pdata->charge_online_algo_reset && get_charge_online_flag())) {
            __this->identify_algo_invalid |= BIT(ch_idx);
            lp_touch_key_identify_algorithm_init(ch_idx, algo_cfg->algo_cfg0, algo_cfg->algo_cfg2);
        } else {
            log_debug("read vm algo param\n");
            if (read_tia_succ == 0) {
                log_debug("read vm algo param error\n");
                __this->identify_algo_invalid |= BIT(ch_idx);
                lp_touch_key_identify_algorithm_init(ch_idx, algo_cfg->algo_cfg0, algo_cfg->algo_cfg2);
            } else {
                log_debug("read vm algo param succ\n");
                memcpy(tia_addr, tia_vm_buf + tia_vm_size, tia_size);
                tia_vm_size += tia_size;
                algo_valid = 0;
                ref_lim_l = -1;
                ref_lim_h = -1;
                lp_touch_key_identify_algo_get_ref_lim(ch_idx, (u16 *)&ref_lim_l, (u16 *)&ref_lim_h, (u32 *)&algo_valid);
                if (algo_valid) {
                    __this->lpctmu_cfg.lim_l[ch] = ref_lim_l;
                    __this->lpctmu_cfg.lim_h[ch] = ref_lim_h;
                }
            }
        }

        lp_touch_key_range_algo_init(ch_idx, algo_cfg);
    }

    free(tia_vm_buf);
    tia_vm_buf = NULL;

#if TCFG_LP_EARTCH_KEY_ENABLE
    lp_touch_key_eartch_init();
#endif

    if (__this->lpctmu_cfg.ch_wkp_en) {
        __this->lpctmu_cfg.softoff_wakeup_cfg = LPCTMU_WAKEUP_EN_WITHOUT_CHARGE_ONLINE;
        if (__this->pdata->charge_online_softoff_wakeup) {
            __this->lpctmu_cfg.softoff_wakeup_cfg = LPCTMU_WAKEUP_EN_ALWAYS;
        }
    } else {
        __this->lpctmu_cfg.softoff_wakeup_cfg = LPCTMU_WAKEUP_DISABLE;
    }

    lpctmu_init(&__this->lpctmu_cfg);

    if (__this->pdata->slide_mode_en) {
        lp_touch_key_slide_algo_reset();
    }

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE

    lp_touch_key_debug_init(
        1,
        lp_touch_key_online_debug_init,
        lp_touch_key_online_debug_send,
        lp_touch_key_online_debug_key_event_handle);

    if ((touch_bt_tool_enable) && (touch_bt_online_debug_init)) {
        touch_bt_online_debug_init();
    }

#else

#if CTMU_CHECK_LONG_CLICK_BY_RES
    lp_touch_key_ctmu_res_all_buf_clear();
#endif

#endif

    os_time_dly(3);

    os_sem_post(touch_ready_sem);

    while (1) {
        ret = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        if (ret != OS_TASKQ) {
            continue;
        }
        switch (msg[0]) {
        case MSG_FROM_KEY:
            lp_touch_key_ctmu_res_deal(msg[1]);
            break;
        default:
            break;
        }
    }
}

void lp_touch_key_init(const struct lp_touch_key_platform_data *pdata)
{
    log_info("%s >>>>", __func__);

    memset(__this, 0, sizeof(struct lp_touch_key_config_data));
    __this->pdata = pdata;
    if (!__this->pdata) {
        return;
    }
    touch_ready_sem = (OS_SEM *)malloc(sizeof(OS_SEM));

    os_sem_create(touch_ready_sem, 0);

    int err = task_create(lp_touch_key_task, NULL, "lp_touch_key");
    if (err != OS_NO_ERR) {
        ASSERT(0, "lp_touch_key task_create fail!!! %x\n", err);
    } else {
        os_sem_pend(touch_ready_sem, 0);
    }

    free(touch_ready_sem);
    touch_ready_sem = NULL;
}

void lp_touch_key_get_res(u16 *res_buf)
{
    if (res_buf == NULL) {
        return;
    }
    memcpy(res_buf, touch_ch_res_buf, sizeof(touch_ch_res_buf));
    log_info("lp_touch_key_get_res\n");
}

u32 lp_touch_key_power_on_status()
{
    if (!__this->pdata) {
        return 0;
    }
    u32 state = 0xff & (lp_touch_key_get_last_touch_state());
    return state;
}

void lp_touch_key_disable(void)
{
    if (!__this->pdata) {
        return;
    }
    log_debug("%s", __func__);
    lpctmu_disable();
}

void lp_touch_key_enable(void)
{
    if (!__this->pdata) {
        return;
    }
    log_debug("%s", __func__);
    lpctmu_enable();
}

#if TCFG_LP_EARTCH_KEY_ENABLE
void lp_touch_key_reset_eartch_algo(void)
{
    if (!__this->pdata) {
        return;
    }
    for (u32 group = 0; group < __this->eartch.ch_num; group++) {
        lp_touch_key_eartch_algorithm_reset(group);
    }
}
#endif

void lp_touch_key_reset_algo(void)
{
    if (!__this->pdata) {
        return;
    }
    log_debug("lp_touch_key_identify_algo_reset\n");

    if (__this->pdata->slide_mode_en) {
        lp_touch_key_slide_algo_reset();
    }

    const struct touch_key_cfg *key_cfg;
    for (u32 ch_idx = 0; ch_idx < __this->pdata->key_num; ch_idx ++) {
        key_cfg = &(__this->pdata->key_cfg[ch_idx]);
        if (key_cfg->eartch_en) {
            continue;
        }
        __this->identify_algo_invalid |= BIT(ch_idx);
        lp_touch_key_identify_algo_reset(ch_idx);
        lp_touch_key_cnacel_long_hold_click_check(ch_idx);
    }
    lpctmu_abandon_dma_data();
    lpctmu_set_dma_res_ie(1);
}

void lp_touch_key_charge_mode_enter()
{
    if (!__this->pdata) {
        return;
    }
    log_debug("%s", __func__);

    //复位算法

#if TCFG_LP_EARTCH_KEY_ENABLE
    lp_touch_key_eartch_notify_event(EARTCH_MUST_BE_OUT_EAR);
    lpctmu_eartch_disable();
    __this->eartch.eartch_enbale = 0;
#endif

    if (__this->pdata->charge_enter_algo_reset) {
        lp_touch_key_reset_algo();
    }

    if (__this->pdata->charge_mode_keep_touch) {
        return;
    }

    log_debug("lpctmu disable\n");
    lpctmu_disable();
}

void lp_touch_key_charge_mode_exit()
{
    log_debug("%s", __func__);
    if (!__this->pdata) {
        return;
    }

    //复位算法

#if TCFG_LP_EARTCH_KEY_ENABLE
    lp_touch_key_reset_eartch_algo();
    lpctmu_eartch_enable();
    __this->eartch.eartch_enbale = 1;
#endif

    if (__this->pdata->charge_exit_algo_reset) {
        lp_touch_key_reset_algo();
    }

    if (__this->pdata->charge_mode_keep_touch) {
        return;
    }

    log_debug("lpctmu enable\n");
    lpctmu_enable();
}

static void lp_touch_key_pre_softoff_cbfunc(void)
{
    if (0 == lpctmu_is_working()) {
        return;
    }
    u32 key_state = lp_touch_key_get_last_touch_state();
    while (key_state) {//等待松手
        os_time_dly(2);
        key_state = lp_touch_key_get_last_touch_state();
    }
    lp_touch_key_save_identify_algo_param();
}
platform_uninitcall(lp_touch_key_pre_softoff_cbfunc);


static enum LOW_POWER_LEVEL lpkey_level_query()
{
#if TCFG_LP_EARTCH_KEY_ENABLE
    if (!__this->pdata) {
        return LOW_POWER_MODE_DEEP_SLEEP;
    }
    if (lpctmu_is_working()) {
        return LOW_POWER_MODE_LIGHT_SLEEP;
    }
#endif
    return LOW_POWER_MODE_DEEP_SLEEP;
}

static u8 lpkey_idle_query(void)
{
    return 1;
}

REGISTER_LP_TARGET(key_lp_target) = {
    .name = "lpkey",
    .level = lpkey_level_query,
    .is_idle = lpkey_idle_query,
};


#endif


