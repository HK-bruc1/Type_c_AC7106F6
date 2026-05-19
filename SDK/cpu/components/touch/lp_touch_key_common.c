

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE

static u8 touch_bt_tool_enable = 0;
static int (*touch_bt_online_debug_init)(void) = NULL;
static int (*touch_bt_online_debug_send)(u32, u16 *) = NULL;
static int (*touch_bt_online_debug_key_event_handle)(u32, u32) = NULL;
void lp_touch_key_debug_init(u32 bt_debug_en, void *bt_debug_init, void *bt_debug_send, void *bt_debug_event)
{
    touch_bt_tool_enable = bt_debug_en;
    touch_bt_online_debug_init = bt_debug_init;
    touch_bt_online_debug_send = bt_debug_send;
    touch_bt_online_debug_key_event_handle = bt_debug_event;
}

#endif


u32 lp_touch_key_get_cur_ch_by_idx(u32 ch_idx)
{
    u32 ch = __this->pdata->key_cfg[ch_idx].key_ch;
    return ch;
}

u32 lp_touch_key_get_idx_by_cur_ch(u32 cur_ch)
{
    u32 ch_idx, ch;
    for (ch_idx = 0; ch_idx < __this->pdata->key_num; ch_idx ++) {
        ch = __this->pdata->key_cfg[ch_idx].key_ch;
        if (cur_ch == ch) {
            return ch_idx;
        }
    }
    return 0;
}

void __attribute__((weak)) lp_touch_key_send_key_tone_msg(void)
{
    //可以在此处发送按键音的消息
}

void __attribute__((weak)) lp_touch_key_send_key_long_tone_msg(void)
{
    //可以在此处发送按键音的消息
}

int __attribute__((weak)) lp_touch_key_event_remap(struct key_event *e)
{
    return true;
}

void lp_touch_key_record_last_key_action(u32 event)
{
    __this->last_key_action = event;
}
u32 lp_touch_key_get_last_key_action(void)
{
    u32 tmp = __this->last_key_action;
    __this->last_key_action = KEY_ACTION_NO_KEY;
    return tmp;
}
static void lp_touch_key_notify_key_event(struct key_event *event, u32 ch_idx)
{
    event->init = 1;
    event->type = KEY_DRIVER_TYPE_CTMU_TOUCH;

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE
    lp_touch_key_record_last_key_action(event->event);
    if ((touch_bt_tool_enable) && (touch_bt_online_debug_key_event_handle)) {
        u32 ch = lp_touch_key_get_cur_ch_by_idx(ch_idx);
        if (touch_bt_online_debug_key_event_handle(ch, event->event)) {
            return;
        }
    }
#endif

    if (lp_touch_key_event_remap(event)) {
        key_event_handler(event);
    }
}


#if CTMU_CHECK_LONG_CLICK_BY_RES

static void lp_touch_key_ctmu_res_buf_clear(u32 ch_idx)
{
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    arg->ctmu_res_buf_in = 0;
    for (u32 i = 0; i < CTMU_RES_BUF_SIZE; i ++) {
        arg->ctmu_res_buf[i] = 0;
    }
}

static void lp_touch_key_ctmu_res_all_buf_clear(void)
{
    for (u32 ch_idx = 0; ch_idx < __this->pdata->key_num; ch_idx ++) {
        lp_touch_key_ctmu_res_buf_clear(ch_idx);
    }
}

static u32 lp_touch_key_ctmu_res_buf_avg(u32 ch_idx)
{
    u32 res_sum = 0;
    u32 i, j = 0;
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    u32 cnt = arg->ctmu_res_buf_in;
    u16 *res_buf = arg->ctmu_res_buf;
    for (i = 0; i < (CTMU_RES_BUF_SIZE - 5); i ++) {
        if (res_buf[cnt]) {
            res_sum += res_buf[cnt];
            j ++;
        } else {
            return 0;
        }
        cnt ++;
        if (cnt >= CTMU_RES_BUF_SIZE) {
            cnt = 0;
        }
    }
    if (res_sum) {
        return (res_sum / j);
    }
    return 0;
}

static u32 lp_touch_key_check_long_click_by_ctmu_res(u32 ch_idx)
{
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    arg->long_event_res_avg = lp_touch_key_ctmu_res_buf_avg(ch_idx);
    log_debug("long_event_res_avg: %d\n", arg->long_event_res_avg);
    log_debug("falling_res_avg: %d\n", arg->falling_res_avg);

    if ((arg->falling_res_avg == 0) || (arg->long_event_res_avg == 0)) {
        return 0;
    }

    u32 cfg2 = lp_touch_key_identify_algo_get_edge_down_th(ch_idx);
    if (arg->falling_res_avg >= arg->long_event_res_avg) {
        u32 diff = arg->falling_res_avg - arg->long_event_res_avg;
        if (diff < (cfg2 / 2)) {
            log_debug("long event return ! diff: %d  <  cfg2/2: %d\n", diff, (cfg2 / 2));
            lp_touch_key_reset_algo();
            return 1;
        }
    } else {
        log_debug("long event return ! falling_res_avg < long_event_res_avg\n");
        lp_touch_key_reset_algo();
        return 1;
    }
    return 0;
}

#endif

u32 lp_touch_key_alog_range_display(u8 *display_buf)
{
    if (!__this->pdata) {
        return 0;
    }
    u8 tmp_buf[32];
    u32 range, i = 0;
    struct touch_key_arg *arg;
    memset(tmp_buf, 0, sizeof(tmp_buf));
    for (u32 ch_idx = 0; ch_idx < __this->pdata->key_num; ch_idx ++) {
        arg = &(__this->arg[ch_idx]);
        range = arg->algo_data.range % 1000;
        tmp_buf[0 + i * 4] = (range / 100) + '0';
        tmp_buf[1 + i * 4] = ((range % 100) / 10) + '0';
        tmp_buf[2 + i * 4] = (range % 10) + '0';
        tmp_buf[3 + i * 4] = ' ';
        i ++;
    }
    if (i) {
        display_buf[0] = i * 4 + 1;
        display_buf[1] = 0x01;
        memcpy((u8 *)&display_buf[2], tmp_buf, i * 4);
        /* printf_buf(display_buf, i * 4 + 2); */
        return (display_buf[0] + 1);
    }
    return 0;
}

static void lp_touch_key_save_range_algo_data(u32 ch_idx)
{
    log_debug("save algo rfg\n");

    struct touch_key_range_algo_data *algo_data;
    algo_data = &(__this->arg[ch_idx].algo_data);

    int ret = syscfg_write(VM_LP_TOUCH_KEY0_ALOG_CFG + ch_idx, (void *)algo_data, sizeof(struct touch_key_range_algo_data));
    if (ret != sizeof(struct touch_key_range_algo_data)) {
        log_debug("write vm algo cfg error !\n");
    }
}

static u32 lp_touch_key_get_range_sensity_threshold(u32 sensity, u32 range)
{
    u32 cfg2_new = range * (10 - sensity) / 10;
    return cfg2_new;
}

static void lp_touch_key_range_algo_cfg_check_update(u32 ch_idx, u32 touch_range, s32 touch_sigma)
{
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    struct touch_key_range_algo_data *algo_data = &(arg->algo_data);
    const struct touch_key_cfg *key_cfg = &(__this->pdata->key_cfg[ch_idx]);
    const struct touch_key_algo_cfg *algo_cfg = &(key_cfg->algo_cfg[key_cfg->index]);

    if (touch_range != algo_data->range) {
        algo_data->range = touch_range;
        algo_data->sigma = touch_sigma;
        u32 cfg2_new = lp_touch_key_get_range_sensity_threshold(algo_cfg->range_sensity, touch_range);
        lp_touch_key_identify_algo_set_edge_down_th(ch_idx, cfg2_new);
        int msg[3];
        msg[0] = (int)lp_touch_key_save_range_algo_data;
        msg[1] = 1;
        msg[2] = (int)ch_idx;
        os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
    }
}

static void lp_touch_key_range_algo_analyze(u32 ch_idx, u32 ch_res)
{
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    struct touch_key_range_algo_data *algo_data = &(arg->algo_data);

    /* log_debug("key%d res:%d\n", ch_idx, ch_res); */

    //数据范围过滤
    if ((ch_res < 1000) || (ch_res > 20000)) {
        return;
    }

    //算法的启动标志检查
    if (algo_data->ready_flag == 0) {
        return;
    }

    //开机预留稳定的时间
    if (arg->algo_sta_cnt < 50) {
        arg->algo_sta_cnt ++;
        return;
    }

#if CTMU_CHECK_LONG_CLICK_BY_RES
    arg->ctmu_res_buf[arg->ctmu_res_buf_in] = ch_res;
    arg->ctmu_res_buf_in ++;
    if (arg->ctmu_res_buf_in >= CTMU_RES_BUF_SIZE) {
        arg->ctmu_res_buf_in = 0;
    }
#endif

    //变化量训练算法输入
    TouchRangeAlgo_Update(ch_idx, ch_res);

    //获取算法结果
    u8 range_valid = 0;
    u16 touch_range = TouchRangeAlgo_GetRange(ch_idx, (u8 *)&range_valid);
    s32 touch_sigma = TouchRangeAlgo_GetSigma(ch_idx);

    const struct touch_key_cfg *key_cfg = &(__this->pdata->key_cfg[ch_idx]);
    const struct touch_key_algo_cfg *algo_cfg = &(key_cfg->algo_cfg[key_cfg->index]);
    //算法结果的处理
    /* log_debug("key%d res:%d range:%d val:%d sigma:%d\n", ch_idx, ch_res, touch_range, range_valid, touch_sigma); */
    if ((range_valid) && (touch_range >= algo_cfg->algo_range_min) && (touch_range <= algo_cfg->algo_range_max)) {
        lp_touch_key_range_algo_cfg_check_update(ch_idx, touch_range, touch_sigma);
    } else if ((range_valid) && (touch_range > algo_cfg->algo_range_max)) {
        TouchRangeAlgo_SetRange(ch_idx, algo_data->range);
    }
}

static void lp_touch_key_range_algo_init(u32 ch_idx, const struct touch_key_algo_cfg *algo_cfg)
{
    __this->arg[ch_idx].algo_sta_cnt = 0;
    TouchRangeAlgo_Init(ch_idx, algo_cfg->algo_range_min, algo_cfg->algo_range_max);

    log_debug("read vm algo cfg\n");

    struct touch_key_range_algo_data *algo_data;
    algo_data = &(__this->arg[ch_idx].algo_data);

    int ret = syscfg_read(VM_LP_TOUCH_KEY0_ALOG_CFG + ch_idx, (void *)algo_data, sizeof(struct touch_key_range_algo_data));

    if ((ret == (sizeof(struct touch_key_range_algo_data)))
        && (algo_data->ready_flag)
        && (algo_data->range >= algo_cfg->algo_range_min)
        && (algo_data->range <= algo_cfg->algo_range_max)) {
        log_debug("vm read key:%d algo ready:%d sigma:%d range:%d\n",
                  ch_idx,
                  algo_data->ready_flag,
                  algo_data->sigma,
                  algo_data->range);

        TouchRangeAlgo_SetSigma(ch_idx, algo_data->sigma);
        TouchRangeAlgo_SetRange(ch_idx, algo_data->range);

        u32 cfg2_new = lp_touch_key_get_range_sensity_threshold(algo_cfg->range_sensity, algo_data->range);
        lp_touch_key_identify_algo_set_edge_down_th(ch_idx, cfg2_new);
    } else {

        if (get_charge_online_flag()) {
            log_debug("check charge online !\n");
            algo_data->ready_flag = 1;
            lp_touch_key_save_range_algo_data(ch_idx);
        }
    }
}

static int lp_touch_key_charge_msg_handler(int msg, int type)
{
    switch (msg) {
    case CHARGE_EVENT_LDO5V_IN:
    case CHARGE_EVENT_LDO5V_KEEP:
        lp_touch_key_charge_mode_enter();
        break;
    case CHARGE_EVENT_CHARGE_FULL:
        break;
    case CHARGE_EVENT_LDO5V_OFF:
        lp_touch_key_charge_mode_exit();
        break;
    }
    return 0;
}
APP_CHARGE_HANDLER(lp_touch_key_charge_msg_entry, 0) = {
    .handler = lp_touch_key_charge_msg_handler,
};


