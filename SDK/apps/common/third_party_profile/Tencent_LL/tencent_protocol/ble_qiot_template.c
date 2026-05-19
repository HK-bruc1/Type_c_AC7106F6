#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_qiot_template.data.bss")
#pragma data_seg(".ble_qiot_template.data")
#pragma const_seg(".ble_qiot_template.text.const")
#pragma code_seg(".ble_qiot_template.text")
#endif
/*
* Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
* Licensed under the MIT License (the "License"); you may not use this file except in
* compliance with the License. You may obtain a copy of the License at
* http://opensource.org/licenses/MIT
* Unless required by applicable law or agreed to in writing, software distributed under the License is
* distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
* either express or implied. See the License for the specific language governing permissions and
* limitations under the License.
*
*/
#ifdef __cplusplus
extern "C" {
#endif

#include "ble_qiot_template.h"

/* #include <stdio.h> */
/* #include <stdbool.h> */
#include <string.h>

#include "ble_qiot_export.h"
#include "ble_qiot_common.h"
#include "ble_qiot_param_check.h"

#include "app_config.h"
#include "btstack/avctp_user.h"
#include "ble_iot_utils.h"
#include "ble_iot_power_manager.h"
#include "ble_iot_anc_manager.h"
#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

static int ble_property_leftear_battery_percentage_set(const char *data, uint16_t len)
{
    return 0;
}

static int ble_property_leftear_battery_percentage_get(char *data, uint16_t buf_len)
{
    uint32_t lefttear_battery_percentage = (uint32_t)iot_get_leftear_battery_percentage();
    /* printf("tiot %s, %d, value=%d, buf_len:%d\n", __FUNCTION__, __LINE__, lefttear_battery_percentage, buf_len); */
    IOT_WRITE_BIG_U32(data, lefttear_battery_percentage);
    return sizeof(uint32_t);
}

static int ble_property_rightear_battery_percentage_set(const char *data, uint16_t len)
{
    return 0;
}

static int ble_property_rightear_battery_percentage_get(char *data, uint16_t buf_len)
{
    uint32_t righttear_battery_percentage = (uint32_t)iot_get_rightear_battery_percentage();
    /* printf("tiot %s, %d, value=%d, buf_len:%d\n", __FUNCTION__, __LINE__, righttear_battery_percentage, buf_len); */
    IOT_WRITE_BIG_U32(data, righttear_battery_percentage);
    return sizeof(uint32_t);
}

static int ble_property_box_battery_percentage_set(const char *data, uint16_t len)
{
    return 0;
}

static int ble_property_box_battery_percentage_get(char *data, uint16_t buf_len)
{
    uint32_t box_battery_percentage = (uint32_t)iot_get_box_battery_percentage();
    /* printf("tiot %s, %d, value=%d, buf_len:%d\n", __FUNCTION__, __LINE__, box_battery_percentage, buf_len); */
    /* box_battery_percentage = HTONL(box_battery_percentage); */
    /* memcpy(data, &box_battery_percentage, sizeof(uint32_t)); */
    IOT_WRITE_BIG_U32(data, box_battery_percentage);
    return sizeof(uint32_t);
}

static int ble_property_noise_mode_set(const char *data, uint16_t len)
{
#if CONFIG_ANC_ENABLE
    u16 anc_mode = IOT_READ_BIG_U16(data);
    /* printf("%s, anc_mode:%d\n", __FUNCTION__, (anc_mode - 1)); */
    iot_anc_mode_update((u8)(anc_mode - 1));
#endif
    return 0;
}

static int ble_property_noise_mode_get(char *data, uint16_t buf_len)
{
#if CONFIG_ANC_ENABLE
    uint16_t anc_mode = (uint16_t)iot_get_anc_mode() + 1;
    IOT_WRITE_BIG_U16(data, anc_mode)
#endif
    return sizeof(uint16_t);
}

static int ble_property_leftear_conn_state_set(const char *data, uint16_t len)
{
    return 0;
}

static int ble_property_leftear_conn_state_get(char *data, uint16_t buf_len)
{
    uint32_t lefttear_battery_percentage = (uint32_t)iot_get_leftear_battery_percentage();
    /* printf("tiot %s, %d, value=%d, buf_len:%d\n", __FUNCTION__, __LINE__, lefttear_battery_percentage, buf_len); */
    uint16_t conn_state = BLE_QIOT_PROPERTY_LEFTEAR_CONN_STATE_DISCONNECTED;
    if (lefttear_battery_percentage) {
#if TCFG_USER_TWS_ENABLE
        conn_state = BLE_QIOT_PROPERTY_LEFTEAR_CONN_STATE_SECONDARY;
        if (bt_tws_get_local_channel() == 'L') {
            conn_state = BLE_QIOT_PROPERTY_LEFTEAR_CONN_STATE_PRIMARY;
        }
#else
        conn_state = BLE_QIOT_PROPERTY_LEFTEAR_CONN_STATE_PRIMARY;
#endif
    }
    IOT_WRITE_BIG_U16(data, conn_state);
    return sizeof(uint16_t);
}

static int ble_property_rightear_conn_state_set(const char *data, uint16_t len)
{
    return 0;
}

static int ble_property_rightear_conn_state_get(char *data, uint16_t buf_len)
{
    uint32_t righttear_battery_percentage = (uint32_t)iot_get_rightear_battery_percentage();
    /* printf("tiot %s, %d, value=%d, buf_len:%d\n", __FUNCTION__, __LINE__, righttear_battery_percentage, buf_len); */
    uint16_t conn_state = BLE_QIOT_PROPERTY_RIGHTEAR_CONN_STATE_DISCONNECTED;
    if (righttear_battery_percentage) {
#if TCFG_USER_TWS_ENABLE
        conn_state = BLE_QIOT_PROPERTY_RIGHTEAR_CONN_STATE_SECONDARY;
        if (bt_tws_get_local_channel() == 'R') {
            conn_state = BLE_QIOT_PROPERTY_RIGHTEAR_CONN_STATE_PRIMARY;
        }
#else
        conn_state = BLE_QIOT_PROPERTY_RIGHTEAR_CONN_STATE_PRIMARY;
#endif
    }
    IOT_WRITE_BIG_U16(data, conn_state);
    return sizeof(uint16_t);
}

static int ble_property_sound_effect_mode_set(const char *data, uint16_t len)
{
    return 0;
}

static int ble_property_sound_effect_mode_get(char *data, uint16_t buf_len)
{
    return sizeof(uint16_t);
}

static uint8_t ll_left_led = 0;
static uint8_t ll_right_led = 0;		// TCFG_USER_TWS_ENABLE使能之后，这个值才有意义
#define LL_ANTI_LOST_PIN IO_PORTA_00
#if TCFG_USER_TWS_ENABLE
#define TWS_FUNC_ID_LL_ANTI_LOST_SYNC \
	(((u8)('L' + 'L') << (3 * 8)) | \
	 ((u8)('A' + 'N' + 'T' + 'I') << (2 * 8)) | \
	 ((u8)('L' + 'O' + 'S' + 'T') << (1 * 8)) | \
	 ((u8)('S' + 'Y' + 'N' + 'C') << (0 * 8)))

static void ll_anti_lost_sync_in_irq(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        ll_left_led = data[0];
        ll_right_led = data[1];
        printf("%s, %s, %d, l:%d, r:%d\n", __FILE__, __FUNCTION__, __LINE__, ll_left_led, ll_right_led);
        if ('R' == bt_tws_get_local_channel()) {
            /* gpio_write(LL_ANTI_LOST_PIN, ll_right_led); */
        } else {
            /* gpio_write(LL_ANTI_LOST_PIN, ll_left_led); */
        }
    }
}

REGISTER_TWS_FUNC_STUB(tws_rcsp_auth_sync) = {
    .func_id = TWS_FUNC_ID_LL_ANTI_LOST_SYNC,
    .func = ll_anti_lost_sync_in_irq,
};

void ll_anti_lost_sync()
{
    u8 buf[2] = {0};
    buf[0] = ll_left_led;
    buf[1] = ll_right_led;
    printf("%s, %s, %d, l:%d, r:%d\n", __FILE__, __FUNCTION__, __LINE__, ll_left_led, ll_right_led);
    tws_api_send_data_to_sibling(buf, sizeof(buf), TWS_FUNC_ID_LL_ANTI_LOST_SYNC);
}
#endif
static int ble_property_leftear_anti_lost_set(const char *data, uint16_t len)
{
    printf("%s led to %d\n", __FUNCTION__, data[0]);
    ll_left_led = data[0];
#if TCFG_USER_TWS_ENABLE
    if ('R' == bt_tws_get_local_channel()) {
        ll_anti_lost_sync();
    } else {
        /* gpio_write(LL_ANTI_LOST_PIN, ll_left_led); */
        ll_anti_lost_sync();
    }
#else
    /* gpio_write(LL_ANTI_LOST_PIN, ll_left_led); */
#endif
    return 0;
}

static int ble_property_leftear_anti_lost_get(char *data, uint16_t buf_len)
{
    printf("%s, led state is %d\n", __FUNCTION__, ll_left_led);
    data[0] = ll_left_led;
    return sizeof(uint8_t);
}

static int ble_property_rightear_anti_lost_set(const char *data, uint16_t len)
{
    printf("%s led to %d\n", __FUNCTION__, data[0]);
    ll_right_led = data[0];
#if TCFG_USER_TWS_ENABLE
    if ('R' == bt_tws_get_local_channel()) {
        /* gpio_write(LL_ANTI_LOST_PIN, ll_right_led); */
        ll_anti_lost_sync();
    } else {
        ll_anti_lost_sync();
    }
#else
    /* gpio_write(LL_ANTI_LOST_PIN, ll_right_led); */
#endif
    return 0;
}

static int ble_property_rightear_anti_lost_get(char *data, uint16_t buf_len)
{
    printf("%s, led state is %d\n", __FUNCTION__, ll_right_led);
    data[0] = ll_right_led;
    return sizeof(uint8_t);
}

static ble_property_t sg_ble_property_array[BLE_QIOT_PROPERTY_ID_BUTT] = {
    {ble_property_box_battery_percentage_set, ble_property_box_battery_percentage_get, BLE_QIOT_PROPERTY_AUTH_READ, BLE_QIOT_DATA_TYPE_INT},
    {ble_property_leftear_conn_state_set, ble_property_leftear_conn_state_get, BLE_QIOT_PROPERTY_AUTH_READ, BLE_QIOT_DATA_TYPE_ENUM},
    {ble_property_rightear_conn_state_set, ble_property_rightear_conn_state_get, BLE_QIOT_PROPERTY_AUTH_READ, BLE_QIOT_DATA_TYPE_ENUM},
    {ble_property_noise_mode_set,   ble_property_noise_mode_get,   BLE_QIOT_PROPERTY_AUTH_RW, BLE_QIOT_DATA_TYPE_ENUM},
    {ble_property_leftear_battery_percentage_set, ble_property_leftear_battery_percentage_get, BLE_QIOT_PROPERTY_AUTH_READ, BLE_QIOT_DATA_TYPE_INT},
    {ble_property_rightear_battery_percentage_set, ble_property_rightear_battery_percentage_get, BLE_QIOT_PROPERTY_AUTH_READ, BLE_QIOT_DATA_TYPE_INT},
    {ble_property_sound_effect_mode_set, ble_property_sound_effect_mode_get, BLE_QIOT_PROPERTY_AUTH_RW, BLE_QIOT_DATA_TYPE_ENUM},
    {ble_property_leftear_anti_lost_set, ble_property_leftear_anti_lost_get, BLE_QIOT_PROPERTY_AUTH_RW, BLE_QIOT_DATA_TYPE_BOOL},
    {ble_property_rightear_anti_lost_set, ble_property_rightear_anti_lost_get, BLE_QIOT_PROPERTY_AUTH_RW, BLE_QIOT_DATA_TYPE_BOOL},
};

static bool ble_check_space_enough_by_type(uint8_t type, uint16_t left_size)
{
    switch (type) {
    case BLE_QIOT_DATA_TYPE_BOOL:
        return left_size >= sizeof(uint8_t);
    case BLE_QIOT_DATA_TYPE_INT:
    case BLE_QIOT_DATA_TYPE_FLOAT:
    case BLE_QIOT_DATA_TYPE_TIME:
        return left_size >= sizeof(uint32_t);
    case BLE_QIOT_DATA_TYPE_ENUM:
        return left_size >= sizeof(uint16_t);
    default:
        // string length is unknow, default true
        return true;
    }
}

static uint16_t ble_check_ret_value_by_type(uint8_t type, uint16_t buf_len, uint16_t ret_val)
{
    switch (type) {
    case BLE_QIOT_DATA_TYPE_BOOL:
        return ret_val <= sizeof(uint8_t);
    case BLE_QIOT_DATA_TYPE_INT:
    case BLE_QIOT_DATA_TYPE_FLOAT:
    case BLE_QIOT_DATA_TYPE_TIME:
        return ret_val <= sizeof(uint32_t);
    case BLE_QIOT_DATA_TYPE_ENUM:
        return ret_val <= sizeof(uint16_t);
    default:
        // string length is unknow, default true
        return ret_val <= buf_len;
    }
}

uint8_t ble_get_property_type_by_id(uint8_t id)
{
    if (id >= BLE_QIOT_PROPERTY_ID_BUTT) {
        ble_qiot_log_e("invalid property id %d", id);
        return BLE_QIOT_DATA_TYPE_BUTT;
    }
    return sg_ble_property_array[id].type;
}

int ble_user_property_set_data(const e_ble_tlv *tlv)
{
    POINTER_SANITY_CHECK(tlv, BLE_QIOT_RS_ERR_PARA);
    if (tlv->id >= BLE_QIOT_PROPERTY_ID_BUTT) {
        ble_qiot_log_e("invalid property id %d", tlv->id);
        return BLE_QIOT_RS_ERR;
    }

    if (NULL != sg_ble_property_array[tlv->id].set_cb) {
        if (0 != sg_ble_property_array[tlv->id].set_cb(tlv->val, tlv->len)) {
            ble_qiot_log_e("set property id %d failed", tlv->id);
            return BLE_QIOT_RS_ERR;
        } else {
            return BLE_QIOT_RS_OK;
        }
    }
    ble_qiot_log_e("invalid set callback, id %d", tlv->id);

    return BLE_QIOT_RS_ERR;
}

int ble_user_property_get_data_by_id(uint8_t id, char *buf, uint16_t buf_len)
{
    int ret_len = 0;

    POINTER_SANITY_CHECK(buf, BLE_QIOT_RS_ERR_PARA);
    if (id >= BLE_QIOT_PROPERTY_ID_BUTT) {
        ble_qiot_log_e("invalid property id %d", id);
        return -1;
    }

    if (NULL != sg_ble_property_array[id].get_cb) {
        if (!ble_check_space_enough_by_type(sg_ble_property_array[id].type, buf_len)) {
            ble_qiot_log_e("not enough space get property id %d data", id);
            return -1;
        }
        ret_len = sg_ble_property_array[id].get_cb(buf, buf_len);
        if (ret_len < 0) {
            ble_qiot_log_e("get property id %d data failed", id);
            return -1;
        } else {
            if (ble_check_ret_value_by_type(sg_ble_property_array[id].type, buf_len, ret_len)) {
                return ret_len;
            } else {
                ble_qiot_log_e("property id %d length invalid", id);
                return -1;
            }
        }
    }
    ble_qiot_log_e("invalid callback, property id %d", id);

    return 0;
}

int ble_user_property_report_reply_handle(uint8_t result)
{
    ble_qiot_log_d("report reply result %d", result);

    return BLE_QIOT_RS_OK;
}

int ble_user_property_struct_handle(const char *in_buf, uint16_t buf_len, ble_property_t struct_arr[], uint8_t arr_size)
{
    uint16_t              parse_len = 0;
    uint16_t              ret_len   = 0;
    e_ble_tlv             tlv;

    while (parse_len < buf_len) {
        memset(&tlv, 0, sizeof(e_ble_tlv));
        ret_len = ble_lldata_parse_tlv(in_buf + parse_len, buf_len - parse_len, &tlv);
        parse_len += ret_len;
        if (parse_len > buf_len) {
            ble_qiot_log_e("parse struct failed");
            return parse_len;
        }

        if (tlv.id >= arr_size) {
            ble_qiot_log_e("invalid array index %d", tlv.id);
            return parse_len;
        }
        if (NULL == struct_arr[tlv.id].set_cb) {
            ble_qiot_log_e("invalid member id %d", tlv.id);
            return parse_len;
        }
        if (BLE_QIOT_RS_OK != struct_arr[tlv.id].set_cb(tlv.val, tlv.len)) {
            ble_qiot_log_e("user handle property error, member id %d, type %d, len %d", tlv.id, tlv.type, tlv.len);
            return parse_len;
        }
    }

    return 0;
}

int ble_user_property_struct_get_data(char *in_buf, uint16_t buf_len, ble_property_t struct_arr[], uint8_t arr_size)
{
    uint8_t  property_id                       = 0;
    uint8_t  property_type                     = 0;
    int      property_len                      = 0;
    char     *data_buf                         = in_buf;
    uint16_t data_len                          = 0;
    uint16_t string_len                        = 0;

    for (property_id = 0; property_id < arr_size; property_id++) {
        property_type = struct_arr[property_id].type;
        if (property_type >= BLE_QIOT_DATA_TYPE_BUTT) {
            ble_qiot_log_e("member id %d type %d invalid", property_id, property_type);
            return BLE_QIOT_RS_ERR;
        }
        data_buf[data_len++] = BLE_QIOT_PACKAGE_TLV_HEAD(property_type, property_id);
        if (BLE_QIOT_DATA_TYPE_STRING == property_type) {
            // reserved 2 bytes for string length
            property_len = struct_arr[property_id].get_cb((char *)data_buf + data_len + 2, buf_len - data_len - 2);
        } else {
            property_len = struct_arr[property_id].get_cb((char *)data_buf + data_len, buf_len - data_len);
        }
        if (property_len < 0) {
            ble_qiot_log_e("too long data, member id %d, data length %d", property_id, data_len);
            return BLE_QIOT_RS_ERR;
        } else if (property_len == 0) {
            // no data to post
            data_len--;
            data_buf[data_len] = '0';
            ble_qiot_log_d("member id %d no data to post", property_id);
        } else {
            if (BLE_QIOT_DATA_TYPE_STRING == property_type) {
                string_len = HTONS(property_len);
                memcpy(data_buf + data_len, &string_len, sizeof(uint16_t));
                data_len += sizeof(uint16_t);
            }
            data_len += property_len;
        }
    }

    return data_len;
}

static int ble_event_get_tws_will_disconnect_type(char *data, uint16_t buf_len)
{
#if TCFG_USER_TWS_ENABLE
    uint16_t disconnect_type = BLE_QIOT_EVENT_TWS_WILL_DISCONNECT_TYPE_BUTT;
    uint32_t lefttear_battery_percentage = (uint32_t)iot_get_leftear_battery_percentage();
    uint32_t righttear_battery_percentage = (uint32_t)iot_get_rightear_battery_percentage();
    if (bt_tws_get_local_channel() == 'L') {
        disconnect_type = BLE_QIOT_EVENT_TWS_WILL_DISCONNECT_TYPE_RIGHT;
    } else if (bt_tws_get_local_channel() == 'R') {
        disconnect_type = BLE_QIOT_EVENT_TWS_WILL_DISCONNECT_TYPE_LEFT;
    } else {
        disconnect_type = BLE_QIOT_EVENT_TWS_WILL_DISCONNECT_TYPE_BUTT;
    }
    IOT_WRITE_BIG_U16(data, disconnect_type);
    /* printf("%s, type:%d\n", __FUNCTION__, disconnect_type); */
#endif
    return sizeof(uint16_t);
}

static ble_event_param sg_ble_event_tws_will_disconnect_array[BLE_QIOT_EVENT_TWS_WILL_DISCONNECT_PARAM_ID_BUTT] = {
    {ble_event_get_tws_will_disconnect_type,  BLE_QIOT_DATA_TYPE_ENUM},
};

static ble_event_t sg_ble_event_array[BLE_QIOT_EVENT_ID_BUTT] = {
    {sg_ble_event_tws_will_disconnect_array, sizeof(sg_ble_event_tws_will_disconnect_array) / sizeof(ble_event_param)},
};

int ble_event_get_id_array_size(uint8_t event_id)
{
    if (event_id >= BLE_QIOT_EVENT_ID_BUTT) {
        ble_qiot_log_e("invalid event id %d", event_id);
        return -1;
    }

    return sg_ble_event_array[event_id].array_size;
}

uint8_t ble_event_get_param_id_type(uint8_t event_id, uint8_t param_id)
{
    if (event_id >= BLE_QIOT_EVENT_ID_BUTT) {
        ble_qiot_log_e("invalid event id %d", event_id);
        return BLE_QIOT_DATA_TYPE_BUTT;
    }
    if (param_id >= sg_ble_event_array[event_id].array_size) {
        ble_qiot_log_e("invalid param id %d", param_id);
        return BLE_QIOT_DATA_TYPE_BUTT;
    }

    return sg_ble_event_array[event_id].event_array[param_id].type;
}

int ble_event_get_data_by_id(uint8_t event_id, uint8_t param_id, char *out_buf, uint16_t buf_len)
{
    int ret_len = 0;

    if (event_id >= BLE_QIOT_EVENT_ID_BUTT) {
        ble_qiot_log_e("invalid event id %d", event_id);
        return -1;
    }
    if (param_id >= sg_ble_event_array[event_id].array_size) {
        ble_qiot_log_e("invalid param id %d", param_id);
        return -1;
    }
    if (NULL == sg_ble_event_array[event_id].event_array[param_id].get_cb) {
        ble_qiot_log_e("invalid callback, event id %d, param id %d", event_id, param_id);
        return 0;
    }

    if (!ble_check_space_enough_by_type(sg_ble_event_array[event_id].event_array[param_id].type, buf_len)) {
        ble_qiot_log_e("not enough space get data, event id %d, param id %d", event_id, param_id);
        return -1;
    }
    ret_len = sg_ble_event_array[event_id].event_array[param_id].get_cb(out_buf, buf_len);
    if (ret_len < 0) {
        ble_qiot_log_e("get event data failed, event id %d, param id %d", event_id, param_id);
        return -1;
    } else {
        if (ble_check_ret_value_by_type(sg_ble_event_array[event_id].event_array[param_id].type, buf_len, ret_len)) {
            return ret_len;
        } else {
            ble_qiot_log_e("evnet data length invalid, event id %d, param id %d", event_id, param_id);
            return -1;
        }
    }
}

int ble_user_event_reply_handle(uint8_t event_id, uint8_t result)
{
    ble_qiot_log_d("event id %d, reply result %d", event_id, result);

    return BLE_QIOT_RS_OK;
}

static int ble_action_handle_earphone_media_control_input_cb(e_ble_tlv *input_param_array, uint8_t input_array_size, uint8_t *output_id_array)
{
    /* printf("%s, input_array_size:%d\n", __FUNCTION__, input_array_size); */
    u16 media_control_action = BLE_QIOT_ACTION_INPUT_EARPHONE_MEDIA_CONTROL_ACTION_PAUSE;
    for (int i = 0; i < input_array_size; i++) {
        e_ble_tlv input_param = input_param_array[i];
        /* printf("type:%d, id:%d\n", input_param.type, input_param.id); */
        /* put_buf((u8*)input_param.val, input_param.len); */
        media_control_action = IOT_READ_BIG_U16(input_param.val);
    }
    printf("media_control_action:%d\n", media_control_action);
    // 音乐播放
    if (media_control_action == BLE_QIOT_ACTION_INPUT_EARPHONE_MEDIA_CONTROL_ACTION_PLAY) {
        if (bt_a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
    } else {
        if (bt_a2dp_get_status() == BT_MUSIC_STATUS_STARTING) {
            bt_cmd_prepare(USER_CTRL_AVCTP_PAUSE_MUSIC, 0, NULL);
        }
    }
    return 0;
}

static int ble_action_handle_earphone_media_control_output_cb(uint8_t output_id, char *buf, uint16_t buf_len)
{
    // 暂不知道如何触发这个函数，如何返回类型
    /* printf("%s, output_id:%d\n", __FUNCTION__, output_id); */
    /* if (buf_len > 0) { */
    /* put_buf((u8 *)buf, buf_len); */
    /* printf("===========end!\n"); */
    /* } */
    return buf_len;
}

static uint8_t sg_ble_action_earphone_media_control_input_type_array[BLE_QIOT_ACTION_EARPHONE_MEDIA_CONTROL_INPUT_ID_BUTT] = {
    BLE_QIOT_DATA_TYPE_ENUM,
};

static uint8_t sg_ble_action_earphone_media_control_output_type_array[BLE_QIOT_ACTION_EARPHONE_MEDIA_CONTROL_OUTPUT_ID_BUTT] = {
    BLE_QIOT_DATA_TYPE_BOOL,
};

static ble_action_t sg_ble_action_array[BLE_QIOT_ACTION_ID_BUTT] = {
    {
        ble_action_handle_earphone_media_control_input_cb, ble_action_handle_earphone_media_control_output_cb,
        sg_ble_action_earphone_media_control_input_type_array, sg_ble_action_earphone_media_control_output_type_array,
        sizeof(sg_ble_action_earphone_media_control_input_type_array) / sizeof(uint8_t),
        sizeof(sg_ble_action_earphone_media_control_output_type_array) / sizeof(uint8_t)
    },
};

uint8_t ble_action_get_intput_type_by_id(uint8_t action_id, uint8_t input_id)
{
    if (action_id >= BLE_QIOT_ACTION_ID_BUTT) {
        ble_qiot_log_e("invalid action id %d", action_id);
        return BLE_QIOT_DATA_TYPE_BUTT;
    }
    if (input_id >= sg_ble_event_array[action_id].array_size) {
        ble_qiot_log_e("invalid input id %d", input_id);
        return BLE_QIOT_DATA_TYPE_BUTT;
    }

    return sg_ble_action_array[action_id].input_type_array[input_id];
}

uint8_t ble_action_get_output_type_by_id(uint8_t action_id, uint8_t output_id)
{
    if (action_id >= BLE_QIOT_ACTION_ID_BUTT) {
        ble_qiot_log_e("invalid action id %d", action_id);
        return BLE_QIOT_DATA_TYPE_BUTT;
    }
    if (output_id >= sg_ble_event_array[action_id].array_size) {
        ble_qiot_log_e("invalid output id %d", output_id);
        return BLE_QIOT_DATA_TYPE_BUTT;
    }

    return sg_ble_action_array[action_id].output_type_array[output_id];
}

int ble_action_get_input_id_size(uint8_t action_id)
{
    if (action_id >= BLE_QIOT_ACTION_ID_BUTT) {
        ble_qiot_log_e("invalid action id %d", action_id);
        return -1;
    }

    return sg_ble_action_array[action_id].input_id_size;
}

int ble_action_get_output_id_size(uint8_t action_id)
{
    if (action_id >= BLE_QIOT_ACTION_ID_BUTT) {
        ble_qiot_log_e("invalid action id %d", action_id);
        return -1;
    }

    return sg_ble_action_array[action_id].output_id_size;
}

int ble_action_user_handle_input_param(uint8_t action_id, e_ble_tlv *input_param_array, uint8_t input_array_size, uint8_t *output_id_array)
{
    if (action_id >= BLE_QIOT_ACTION_ID_BUTT) {
        ble_qiot_log_e("invalid action id %d", action_id);
        return -1;
    }

    if (NULL != sg_ble_action_array[action_id].input_cb) {
        if (0 != sg_ble_action_array[action_id].input_cb(input_param_array, input_array_size, output_id_array)) {
            ble_qiot_log_e("input handle error");
            return -1;
        }
    }

    return 0;
}

int ble_action_user_handle_output_param(uint8_t action_id, uint8_t output_id, char *buf, uint16_t buf_len)
{
    int ret_len = 0;

    if (action_id >= BLE_QIOT_ACTION_ID_BUTT) {
        ble_qiot_log_e("invalid action id %d", action_id);
        return -1;
    }
    if (NULL == sg_ble_action_array[action_id].output_cb) {
        ble_qiot_log_e("invalid callback, action id %d", action_id);
        return 0;
    }

    if (!ble_check_space_enough_by_type(sg_ble_action_array[action_id].output_type_array[output_id], buf_len)) {
        ble_qiot_log_e("not enough space get data, action id %d, output id %d", action_id, output_id);
        return -1;
    }

    ret_len = sg_ble_action_array[action_id].output_cb(output_id, buf, buf_len);
    if (ret_len < 0) {
        ble_qiot_log_e("get action data failed, action id %d, output id %d", action_id, output_id);
        return -1;
    } else {
        if (ble_check_ret_value_by_type(sg_ble_action_array[action_id].output_type_array[output_id], buf_len, ret_len)) {
            return ret_len;
        } else {
            ble_qiot_log_e("action data length invalid, action id %d, output id %d", action_id, output_id);
            return -1;
        }
    }
}

#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)

#ifdef __cplusplus
}
#endif

