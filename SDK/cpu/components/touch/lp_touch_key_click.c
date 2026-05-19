
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".lp_touch_key_click.data.bss")
#pragma data_seg(".lp_touch_key_click.data")
#pragma const_seg(".lp_touch_key_click.text.const")
#pragma code_seg(".lp_touch_key_click.text")
#endif

static void lp_touch_key_short_click_time_out_handle(void *priv)
{
    u32 ch_idx = (u32)priv;
    const struct touch_key_cfg *key_cfg = &(__this->pdata->key_cfg[ch_idx]);
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);

    struct key_event e;
    switch (arg->click_cnt) {
    case 1:
        e.event = KEY_ACTION_CLICK;
        break;
    case 2:
        e.event = KEY_ACTION_DOUBLE_CLICK;
        break;
    case 3:
        e.event = KEY_ACTION_TRIPLE_CLICK;
        break;
    case 4:
        e.event = KEY_ACTION_FOURTH_CLICK;
        break;
    case 5:
        e.event = KEY_ACTION_FIRTH_CLICK;
        break;
    default:
        e.event = KEY_ACTION_NO_KEY;
        break;
    }
    e.value = key_cfg->key_value;

    log_debug("notify key:%d short event, cnt: %d", ch_idx, arg->click_cnt);
    lp_touch_key_notify_key_event(&e, ch_idx);

    arg->short_timer = 0;
    arg->last_key = 0;
    arg->click_cnt = 0;
}

static void lp_touch_key_short_click_handle(u32 ch_idx)
{
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
    if (touch_abandon_short_click_once) {
        touch_abandon_short_click_once = 0;
        log_debug("lp touch key abandon the short click!\n");
        return;
    }
#endif

    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    arg->last_key =  TOUCH_KEY_SHORT_CLICK;
    if (arg->short_timer == 0) {
        arg->click_cnt = 1;
        arg->short_timer = usr_timeout_add((void *)ch_idx, lp_touch_key_short_click_time_out_handle, __this->pdata->short_click_check_time, 1);
    } else {
        arg->click_cnt++;
        usr_timer_modify(arg->short_timer, __this->pdata->short_click_check_time);
    }
}

static void lp_touch_key_raise_click_handle(u32 ch_idx)
{
    const struct touch_key_cfg *key_cfg = &(__this->pdata->key_cfg[ch_idx]);
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);

    struct key_event e = {0};
    if (arg->last_key >= TOUCH_KEY_LONG_CLICK) {
        e.event = KEY_ACTION_UP;
        e.value = key_cfg->key_value;
        lp_touch_key_notify_key_event(&e, ch_idx);

        arg->last_key =  TOUCH_KEY_NULL;
        log_debug("notify key HOLD UP event");
    } else {
        lp_touch_key_short_click_handle(ch_idx);
    }
}

#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS

static void lp_touch_key_cnacel_long_hold_click_check(u32 ch_idx)
{
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);

    if (arg->long_timer) {
        usr_timeout_del(arg->long_timer);
        arg->long_timer = 0;
    }
    if (arg->hold_timer) {
        usr_timeout_del(arg->hold_timer);
        arg->hold_timer = 0;
    }
}

static void lp_touch_key_trigger_hold_click_event(void *priv)
{
    u32 ch_idx = (u32)priv;
    extern void lp_touch_key_state_event_deal(u32 ch_idx, u32 event);
    lp_touch_key_state_event_deal(ch_idx, TOUCH_KEY_HOLD_EVENT);

}
static void lp_touch_key_trigger_long_click_event(void *priv)
{
    u32 ch_idx = (u32)priv;
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);

    extern void lp_touch_key_state_event_deal(u32 ch_idx, u32 event);
    lp_touch_key_state_event_deal(ch_idx, TOUCH_KEY_LONG_EVENT);

    arg->long_timer = 0;
    if (arg->hold_timer == 0) {
        arg->hold_timer = usr_timer_add((void *)ch_idx, lp_touch_key_trigger_hold_click_event, __this->pdata->hold_click_check_time, 1);
    }
}

static void lp_touch_key_fall_click_handle(u32 ch_idx)
{
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    if (arg->long_timer == 0) {
        arg->long_timer = usr_timeout_add((void *)ch_idx, lp_touch_key_trigger_long_click_event, __this->pdata->long_click_check_time, 1);
    }
}

#endif


static void lp_touch_key_long_click_handle(u32 ch_idx)
{
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
    touch_abandon_short_click_once = 0;
#endif

    const struct touch_key_cfg *key_cfg = &(__this->pdata->key_cfg[ch_idx]);
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    arg->last_key = TOUCH_KEY_LONG_CLICK;

    struct key_event e;
    e.event = KEY_ACTION_LONG;
    e.value = key_cfg->key_value;

    lp_touch_key_notify_key_event(&e, ch_idx);
}

static void lp_touch_key_hold_click_handle(u32 ch_idx)
{
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
    touch_abandon_short_click_once = 0;
#endif

    const struct touch_key_cfg *key_cfg = &(__this->pdata->key_cfg[ch_idx]);
    struct touch_key_arg *arg = &(__this->arg[ch_idx]);
    arg->last_key =  TOUCH_KEY_HOLD_CLICK;

    struct key_event e;
    e.event = KEY_ACTION_HOLD;
    e.value = key_cfg->key_value;

    lp_touch_key_notify_key_event(&e, ch_idx);
}

static void lp_touch_key_slide_up_handle(u32 ch_idx)
{
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
    if (touch_abandon_short_click_once) {
        touch_abandon_short_click_once = 0;
        log_debug("lp touch key abandon the short click!\n");
        return;
    }
#endif

    struct key_event e;
    e.event = KEY_SLIDER_UP;
    e.value = __this->pdata->slide_mode_key_value;
    lp_touch_key_notify_key_event(&e, ch_idx);
}

static void lp_touch_key_slide_down_handle(u32 ch_idx)
{
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
    if (touch_abandon_short_click_once) {
        touch_abandon_short_click_once = 0;
        log_debug("lp touch key abandon the short click!\n");
        return;
    }
#endif
    struct key_event e;
    e.event = KEY_SLIDER_DOWN;
    e.value = __this->pdata->slide_mode_key_value;
    lp_touch_key_notify_key_event(&e, ch_idx);
}

