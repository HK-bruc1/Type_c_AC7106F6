#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "app_main.h"
#include "key_driver.h"
#include "rdx_product_key_routing.h"
#if RDX_CFG_ONLINE_RECORDING_ENABLE
#include "rdx/rdx_online_record_service.h"
#endif

enum rdx_key_dispatch_result {
    RDX_KEY_DISPATCH_POSTED,
    RDX_KEY_DISPATCH_RECORD_TOGGLED,
    RDX_KEY_DISPATCH_RECORD_TOGGLE_REJECTED,
    RDX_KEY_DISPATCH_IGNORED,
    RDX_KEY_DISPATCH_INVALID_ACTION,
};

struct rdx_key_route {
    u8 key_value;
    const int *action_map;
};

#if TCFG_MICKEY_ENABLE
static const int rdx_mic_num0_map[KEY_ACTION_MAX] = {
    [KEY_ACTION_DOWN]            = APP_MSG_PP_KEY_PRESS,
    [KEY_ACTION_CLICK]           = APP_MSG_PP_KEY_LIFT,
    [KEY_ACTION_UP]              = APP_MSG_PP_KEY_LIFT,
    [KEY_ACTION_DOUBLE_CLICK]    = APP_MSG_MUSIC_NEXT,
    [KEY_ACTION_TRIPLE_CLICK]    = APP_MSG_MUSIC_PREV,
};

static const int rdx_mic_num1_map[KEY_ACTION_MAX] = {
    [KEY_ACTION_DOWN]            = APP_MSG_UP_KEY_PRESS,
    [KEY_ACTION_CLICK]           = APP_MSG_UP_KEY_LIFT,
    [KEY_ACTION_UP]              = APP_MSG_UP_KEY_LIFT,
};

static const int rdx_mic_num2_map[KEY_ACTION_MAX] = {
    [KEY_ACTION_DOWN]            = APP_MSG_DOWN_KEY_PRESS,
    [KEY_ACTION_CLICK]           = APP_MSG_DOWN_KEY_LIFT,
    [KEY_ACTION_UP]              = APP_MSG_DOWN_KEY_LIFT,
};
#endif

#if TCFG_ADKEY_ENABLE
/*
 * The two physical keys on T2622 are KEY_AD_NUM0 (S3, 0 ohm) and
 * KEY_AD_NUM1 (S1, 20 kohm).  Their click actions are dispatched to the
 * RDX recording business below; every other ADKEY action stays null.
 */
static const int rdx_ad_num0_map[KEY_ACTION_MAX] = {
    [KEY_ACTION_CLICK]         = APP_MSG_NULL,
    [KEY_ACTION_LONG]          = APP_MSG_NULL,
    [KEY_ACTION_DOUBLE_CLICK]  = APP_MSG_NULL,
};

static const int rdx_ad_num1_map[KEY_ACTION_MAX] = {
    [KEY_ACTION_CLICK]         = APP_MSG_NULL,
    [KEY_ACTION_LONG]          = APP_MSG_NULL,
    [KEY_ACTION_HOLD]          = APP_MSG_NULL,
};
#endif

#if TCFG_MICKEY_ENABLE || TCFG_ADKEY_ENABLE

static const struct rdx_key_route rdx_key_routes[] = {
#if TCFG_ADKEY_ENABLE
    { KEY_AD_NUM0, rdx_ad_num0_map },
    { KEY_AD_NUM1, rdx_ad_num1_map },
#endif
#if TCFG_MICKEY_ENABLE
    { KEY_MIC_NUM0, rdx_mic_num0_map },
    { KEY_MIC_NUM1, rdx_mic_num1_map },
    { KEY_MIC_NUM2, rdx_mic_num2_map },
#endif
};
#endif

static bool rdx_key_is_owned(u8 key_value)
{
    if (!app_in_mode(APP_MODE_PC)) {
        return false;
    }

    switch (key_value) {
#if TCFG_ADKEY_ENABLE
    case KEY_AD_NUM0:
    case KEY_AD_NUM1:
        return true;
#endif
#if TCFG_MICKEY_ENABLE
    case KEY_MIC_NUM0:
    case KEY_MIC_NUM1:
    case KEY_MIC_NUM2:
        return true;
#endif

    default:
        return false;
    }
}

static const struct rdx_key_route *rdx_key_find_route(u8 key_value)
{
#if TCFG_MICKEY_ENABLE || TCFG_ADKEY_ENABLE
    u8 i;

    for (i = 0; i < ARRAY_SIZE(rdx_key_routes); i++) {
        if (rdx_key_routes[i].key_value == key_value) {
            return &rdx_key_routes[i];
        }
    }
#endif

    return NULL;
}

static enum rdx_key_dispatch_result
rdx_key_dispatch(u8 key_value, u8 key_action, int source, int *routed_app_msg)
{
    const struct rdx_key_route *route;
    int app_msg;
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    int record_ret;
#endif

    *routed_app_msg = APP_MSG_NULL;

    if (key_action >= KEY_ACTION_MAX) {
        return RDX_KEY_DISPATCH_INVALID_ACTION;
    }

    route = rdx_key_find_route(key_value);
    if (!route) {
        return RDX_KEY_DISPATCH_IGNORED;
    }

#if RDX_CFG_ONLINE_RECORDING_ENABLE
    if (key_action == KEY_ACTION_CLICK) {
        switch (key_value) {
        case KEY_AD_NUM0:
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
            if (source != 0) {
                printf("[RDX][KEY] record_toggle_rejected reason=non_local"
                       " scene=%u source=%d\n",
                       RDX_RECORD_SCENE_MEETING, source);
                return RDX_KEY_DISPATCH_RECORD_TOGGLE_REJECTED;
            }
            record_ret = rdx_online_record_request_toggle(
                RDX_RECORD_SCENE_MEETING, RDX_RECORD_SOURCE_KEY);
            return record_ret == 0
                   ? RDX_KEY_DISPATCH_RECORD_TOGGLED
                   : RDX_KEY_DISPATCH_RECORD_TOGGLE_REJECTED;
#endif
            break;
        case KEY_AD_NUM1:
#if RDX_CFG_CALL_RECORDING_ENABLE
            if (source != 0) {
                printf("[RDX][KEY] record_toggle_rejected reason=non_local"
                       " scene=%u source=%d\n",
                       RDX_RECORD_SCENE_CALL, source);
                return RDX_KEY_DISPATCH_RECORD_TOGGLE_REJECTED;
            }
            record_ret = rdx_online_record_request_toggle(
                RDX_RECORD_SCENE_CALL, RDX_RECORD_SOURCE_KEY);
            return record_ret == 0
                   ? RDX_KEY_DISPATCH_RECORD_TOGGLED
                   : RDX_KEY_DISPATCH_RECORD_TOGGLE_REJECTED;
#endif
            break;
        default:
            break;
        }
    }
#endif

    app_msg = route->action_map[key_action];
    if (app_msg == APP_MSG_NULL) {
        return RDX_KEY_DISPATCH_IGNORED;
    }

    *routed_app_msg = app_msg;
    app_send_message(app_msg, 0);
    return RDX_KEY_DISPATCH_POSTED;
}

#if RDX_CFG_KEY_DIAG_ENABLE
static const char *rdx_key_dispatch_result_name(enum rdx_key_dispatch_result result)
{
    switch (result) {
    case RDX_KEY_DISPATCH_POSTED:
        return "posted";

    case RDX_KEY_DISPATCH_RECORD_TOGGLED:
        return "record_toggled";

    case RDX_KEY_DISPATCH_RECORD_TOGGLE_REJECTED:
        return "record_toggle_rejected";

    case RDX_KEY_DISPATCH_IGNORED:
        return "ignored";

    case RDX_KEY_DISPATCH_INVALID_ACTION:
        return "invalid_action";

    default:
        return "unknown";
    }
}

static void rdx_key_diag_log(u8 key_value, u8 key_action, int source,
                             u8 owned, int app_msg, const char *result)
{
    printf("[RDX][KEY] value=%u action=%u source=%d owned=%u "
           "app_msg=%d result=%s\n",
           key_value, key_action, source, owned, app_msg, result);
}
#endif

int rdx_product_key_routing_handle(int *msg)
{
    enum rdx_key_dispatch_result result;
    int app_msg = APP_MSG_NULL;
    u8 key_value;
    u8 key_action;

    if (!msg) {
        return 0;
    }

    key_value = APP_MSG_KEY_VALUE(msg[0]);
    key_action = APP_MSG_KEY_ACTION(msg[0]);

    if (!rdx_key_is_owned(key_value)) {
#if RDX_CFG_KEY_DIAG_ENABLE
        rdx_key_diag_log(key_value, key_action, msg[1], 0,
                         APP_MSG_NULL, "passthrough");
#endif
        return 0;
    }

    result = rdx_key_dispatch(key_value, key_action, msg[1], &app_msg);
#if RDX_CFG_KEY_DIAG_ENABLE
    rdx_key_diag_log(key_value, key_action, msg[1], 1, app_msg,
                     rdx_key_dispatch_result_name(result));
#else
    if (result == RDX_KEY_DISPATCH_INVALID_ACTION) {
        printf("[RDX][KEY] invalid action: value=%u action=%u\n",
               key_value, key_action);
    }
#endif

    return 1;
}

APP_MSG_PROB_HANDLER(rdx_product_key_msg_entry) = {
    .owner   = 0xff,
    .from    = MSG_FROM_KEY,
    .handler = rdx_product_key_routing_handle,
};

#endif
