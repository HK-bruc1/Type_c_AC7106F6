#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "timer.h"
#include "rdx/rdx_appkey_verifier.h"
#include "rdx/rdx_ble_name_service.h"
#include "rdx/rdx_device_management_service.h"
#include "rdx/rdx_identity_service.h"
#include "rdx/rdx_wire_defs.h"
#include "rdx/ports/rdx_product_profile_port.h"
#include "rdx/ports/rdx_system_action_port.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23_internal.h"
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#include "rdx/rdx_mic_gain_service.h"
#endif
#if RDX_CFG_RTC_ENABLE
#include "rdx/rdx_rtc_service.h"
#endif

#define RDX_APPKEY_PAYLOAD_SIZE                 32
#define RDX_APPKEY_HEX_STRING_SIZE              (RDX_APPKEY_PAYLOAD_SIZE * 2 + 1)
#define RDX_POST_ACK_DISCONNECT_DELAY_MS         100

static const uint8_t rdx_cmd_battery[] = "*APP#battery#";
static const uint8_t rdx_cmd_version[] = "*APP#version#";
static const uint8_t rdx_cmd_appkey[] = "*APP#appkey#";
#if RDX_CFG_RTC_ENABLE
static const uint8_t rdx_cmd_rtc[] = "*APP#rtc#";
#endif
static const uint8_t rdx_cmd_ostype[] = "*APP#ostype#";
static const uint8_t rdx_cmd_auth_sn[] = RDX_CMD_DL_AUTH_SN;
static const uint8_t rdx_cmd_offtime_check[] = RDX_CMD_DL_OFFTIME_CHECK;
static const uint8_t rdx_cmd_bound[] = RDX_CMD_DL_BOUND;
static const uint8_t rdx_cmd_unbound[] = RDX_CMD_DL_UNBOUND;
static const uint8_t rdx_cmd_set_default[] = RDX_CMD_DL_SET_DEFAULT;
static const uint8_t rdx_cmd_ble_name_query[] = RDX_CMD_DL_BLE_NAME_QUERY;
static const uint8_t rdx_cmd_ble_name_set[] = RDX_CMD_DL_BLE_NAME_SET;
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
static const uint8_t rdx_cmd_mic_gain_query[] = RDX_CMD_DL_MIC_GAIN_QUERY;
static const uint8_t rdx_cmd_mic_gain_set[] = RDX_CMD_DL_MIC_GAIN_SET;
#endif
static const uint8_t rdx_rsp_ostype[] = "*DEV#ostype#";

typedef enum {
    RDX_POST_ACK_ACTION_NONE = 0,
    RDX_POST_ACK_ACTION_UNBOUND,
    RDX_POST_ACK_ACTION_SET_DEFAULT,
} rdx_post_ack_action_t;

struct rdx_protocol_v23_management_state {
    struct rdx_protocol_v23_context *context;
    uint8_t state_transition_pending;
    uint8_t post_ack_action;
    uint16_t post_ack_timer_id;
    uint32_t post_ack_generation;
};

static struct rdx_protocol_v23_management_state rdx_v23_management;
static uint32_t rdx_v23_post_ack_generation_seed;

static const char *rdx_v23_post_ack_action_name(uint8_t action);

static void rdx_v23_bytes_to_hex(const uint8_t *data, uint16_t len,
                                  char *output, uint16_t output_size)
{
    static const char hex[] = "0123456789ABCDEF";
    uint16_t i;

    if (!data || !output || output_size < len * 2 + 1) {
        if (output && output_size) {
            output[0] = '\0';
        }
        return;
    }
    for (i = 0; i < len; i++) {
        output[i * 2] = hex[data[i] >> 4];
        output[i * 2 + 1] = hex[data[i] & 0x0f];
    }
    output[len * 2] = '\0';
}
static void rdx_v23_send_battery(void)
{
    const struct rdx_product_profile *profile = rdx_product_profile_get();
    uint8_t response[32];
    int len;
    int ret;

    /* The 701 application keeps the legacy wire order C, R, L. */
    len = snprintf((char *)response, sizeof(response),
                   "*DEV#battery#0#0#%u#", profile->fixed_battery_level);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=battery reason=encode_failed len=%d\n", len);
        return;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=battery wire=%s len=%d ret=%d\n",
           (char *)response, len, ret);
}

static void rdx_v23_send_version(void)
{
    const struct rdx_product_profile *profile = rdx_product_profile_get();
    uint8_t response[48];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   "*DEV#version#%s#%s#",
                   profile->hardware_version, profile->firmware_version);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=version reason=encode_failed len=%d\n", len);
        return;
    }


    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=version wire=%s len=%d ret=%d\n",
           (char *)response, len, ret);
}

static void rdx_v23_send_auth_sn(void)
{
    uint8_t response[96];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_AUTH_SN "%s#%s#%s#%s#",
                   rdx_identity_get_auth_key(),
                   rdx_identity_get_ble_mac_string(),
                   rdx_identity_get_wifi_mac_string(),
                   rdx_identity_get_label_sn());
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=authsn reason=encode_failed len=%d\n", len);
        return;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    /* AuthKey is part of this response; never echo the complete wire packet. */
    printf("[RDX][TX] cmd=authsn len=%d ret=%d\n", len, ret);
}

static void rdx_v23_send_offtime_check(void)
{
    uint8_t response[40];
    uint32_t minutes = 0;
    uint8_t result;
    int len;
    int ret;

    result = rdx_system_action_get_auto_off_time_minutes(&minutes) ? 1 : 0;
    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_OFFTIME_CHECK "%d#%u#",
                   (int)result, (unsigned int)minutes);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=cofftime reason=encode_failed len=%d\n", len);
        return;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=cofftime wire=%s result=%u minutes=%u len=%d ret=%d\n",
           (char *)response, (unsigned int)result,
           (unsigned int)minutes, len, ret);
}

static int rdx_v23_send_ble_name_result(uint8_t query, uint8_t result)
{
    uint8_t response[48];
    const char *name = rdx_ble_name_get();
    const char *prefix = query ? RDX_CMD_UP_BLE_NAME_QUERY
                               : RDX_CMD_UP_BLE_NAME_SET;
    const char *command = query ? "cblename" : "blename";
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response), "%s%u#%s#",
                   prefix, result, name);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=%s reason=encode_failed len=%d\n",
               command, len);
        return -1;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=%s result=%u name_len=%u wire=%s len=%d ret=%d\n",
           command, result, (unsigned int)strlen(name),
           (char *)response, len, ret);
    return ret;
}


static void rdx_v23_handle_ble_name_query(const uint8_t *data, uint16_t len)
{
    if (!rdx_protocol_v23_data_equals(data, len, rdx_cmd_ble_name_query,
                              sizeof(rdx_cmd_ble_name_query) - 1)) {
        printf("[RDX][DROP] cmd=cblename reason=malformed len=%u\n", len);
        return;
    }
    if (!rdx_protocol_v23_query_ready(rdx_v23_management.context, "cblename", len)) {
        return;
    }

    rdx_v23_send_ble_name_result(1, 0);
}

static void rdx_v23_handle_ble_name_set(const uint8_t *data, uint16_t len)
{
    const uint16_t prefix_len = sizeof(rdx_cmd_ble_name_set) - 1;
    uint16_t name_len;
    uint16_t i;
    uint8_t changed = 0;
    int ret;
    int refresh_ret = 0;

    if (len < prefix_len + 1 || data[len - 1] != '#') {
        printf("[RDX][DROP] cmd=blename reason=malformed len=%u\n", len);
        return;
    }
    name_len = len - prefix_len - 1;
    for (i = 0; i < name_len; i++) {
        if (data[prefix_len + i] == '#') {
            printf("[RDX][DROP] cmd=blename reason=malformed_fields len=%u\n",
                   len);
            return;
        }
    }
    if (!rdx_protocol_v23_query_ready(rdx_v23_management.context, "blename", len)) {
        return;
    }
    if (rdx_v23_management.state_transition_pending) {
        printf("[RDX][DROP] cmd=blename reason=transition_pending action=%s len=%u\n",
               rdx_v23_post_ack_action_name(
                   rdx_v23_management.post_ack_action), len);
        rdx_v23_send_ble_name_result(0, 1);
        return;
    }

    ret = rdx_ble_name_set(data + prefix_len, name_len, &changed);
    if (!ret && changed) {
        refresh_ret = rdx_transport_refresh_local_name();
        if (refresh_ret) {
            printf("[RDX][BLE] name_refresh result=degraded ret=%d\n",
                   refresh_ret);
        }
    }
    printf("[RDX][STATE] cmd=blename result=%s candidate_len=%u changed=%u refresh_ret=%d\n",
           ret ? "failed" : "ok", name_len, changed, refresh_ret);
    rdx_v23_send_ble_name_result(0, ret ? 1 : 0);
}

static const char *rdx_v23_post_ack_action_name(uint8_t action)
{
    switch (action) {
    case RDX_POST_ACK_ACTION_UNBOUND:
        return "unbound";
    case RDX_POST_ACK_ACTION_SET_DEFAULT:
        return "default";
    default:
        return "none";
    }
}

static uint32_t rdx_v23_next_generation(uint32_t generation)
{
    generation++;
    return generation ? generation : 1;
}

static void rdx_v23_cancel_post_ack_action(const char *reason)
{
    uint8_t action;
    uint16_t timer_id;

    if (!rdx_v23_management.state_transition_pending
        && !rdx_v23_management.post_ack_timer_id) {
        return;
    }

    action = rdx_v23_management.post_ack_action;
    timer_id = rdx_v23_management.post_ack_timer_id;
    rdx_v23_management.state_transition_pending = 0;
    rdx_v23_management.post_ack_action = RDX_POST_ACK_ACTION_NONE;
    rdx_v23_management.post_ack_timer_id = 0;
    rdx_v23_management.post_ack_generation = 0;
    if (timer_id) {
        sys_timeout_del(timer_id);
    }
    printf("[RDX][SESSION] post_ack action=%s event=cancel reason=%s timer_id=%u\n",
           rdx_v23_post_ack_action_name(action), reason, timer_id);
}

static void rdx_v23_post_ack_timeout(void *priv)
{
    uint32_t generation = (uint32_t)(uintptr_t)priv;
    uint8_t action = rdx_v23_management.post_ack_action;

    if (!rdx_v23_management.context
        || !rdx_v23_management.context->initialized
        || !rdx_v23_management.state_transition_pending
        || !generation
        || generation != rdx_v23_management.post_ack_generation) {
        printf("[RDX][SESSION] post_ack event=ignore reason=stale"
               " generation=%u active_generation=%u\n",
               (unsigned int)generation,
               (unsigned int)rdx_v23_management.post_ack_generation);
        return;
    }

    rdx_v23_management.state_transition_pending = 0;
    rdx_v23_management.post_ack_action = RDX_POST_ACK_ACTION_NONE;
    rdx_v23_management.post_ack_timer_id = 0;
    rdx_v23_management.post_ack_generation = 0;
    if (!rdx_v23_management.context->session.connected) {
        printf("[RDX][SESSION] post_ack action=%s event=skip reason=session_inactive\n",
               rdx_v23_post_ack_action_name(action));
        return;
    }

    printf("[RDX][SESSION] post_ack action=%s event=execute disconnect=rdx_ble\n",
           rdx_v23_post_ack_action_name(action));
    rdx_transport_disconnect();
}

static void rdx_v23_schedule_post_ack_disconnect(uint8_t action, int ack_ret)
{
    uint32_t generation;
    uint16_t timer_id;

    if (!rdx_v23_management.context->session.connected) {
        printf("[RDX][SESSION] post_ack action=%s event=skip reason=session_inactive ack_ret=%d\n",
               rdx_v23_post_ack_action_name(action), ack_ret);
        return;
    }

    rdx_v23_management.state_transition_pending = 1;
    rdx_v23_management.post_ack_action = action;
    generation = rdx_v23_next_generation(
        rdx_v23_post_ack_generation_seed);
    rdx_v23_post_ack_generation_seed = generation;
    rdx_v23_management.post_ack_generation = generation;
    timer_id = sys_timeout_add((void *)(uintptr_t)generation,
                               rdx_v23_post_ack_timeout,
                               RDX_POST_ACK_DISCONNECT_DELAY_MS);
    rdx_v23_management.post_ack_timer_id = timer_id;
    printf("[RDX][SESSION] post_ack action=%s event=schedule delay_ms=%u timer_id=%u ack_ret=%d\n",
           rdx_v23_post_ack_action_name(action),
           RDX_POST_ACK_DISCONNECT_DELAY_MS, timer_id, ack_ret);
    if (timer_id) {
        return;
    }

    printf("[RDX][SESSION] post_ack action=%s event=fallback reason=timer_alloc_failed\n",
           rdx_v23_post_ack_action_name(action));
    rdx_v23_post_ack_timeout((void *)(uintptr_t)generation);
}

static int rdx_v23_parse_nonnegative_s32(const uint8_t *data, uint16_t len,
                                          int32_t *value)
{
    uint32_t parsed = 0;
    uint16_t i;

    if (!data || !len || !value) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        uint8_t digit;

        if (data[i] < '0' || data[i] > '9') {
            return -1;
        }
        digit = data[i] - '0';
        if (parsed > (0x7fffffffUL - digit) / 10) {
            return -1;
        }
        parsed = parsed * 10 + digit;
    }

    *value = (int32_t)parsed;
    return 0;
}

#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
static int rdx_v23_parse_decimal_fields(const uint8_t *data, uint16_t len,
                                          int32_t *values, uint8_t field_count)
{
    uint16_t field_start = 0;
    uint16_t i;
    uint8_t field = 0;

    if (!data || !len || !values || !field_count
        || data[len - 1] != '#') {
        return -1;
    }

    for (i = 0; i < len; i++) {
        if (data[i] != '#') {
            continue;
        }
        if (i == field_start || field >= field_count
            || rdx_v23_parse_nonnegative_s32(
                   data + field_start, i - field_start,
                   &values[field])) {
            return -1;
        }
        field++;
        field_start = i + 1;
    }

    return field == field_count && field_start == len ? 0 : -1;
}

static int rdx_v23_send_mic_gain_result(uint8_t query, uint8_t result,
                                         int32_t mode, uint8_t gain)
{
    uint8_t response[64];
    const char *prefix = query ? RDX_CMD_UP_MIC_GAIN_QUERY
                               : RDX_CMD_UP_MIC_GAIN_SET;
    const char *command = query ? "cmicgain" : "micgain";
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   "%s%u#%d#%u#%u#", prefix, result,
                   (int)mode, gain, gain);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=%s reason=encode_failed len=%d\n",
               command, len);
        return -1;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=%s wire=%s result=%u mode=%d gain=%u len=%d ret=%d\n",
           command, (char *)response, result, (int)mode, gain, len, ret);
    return ret;
}

static void rdx_v23_handle_mic_gain_query(const uint8_t *data, uint16_t len)
{
    const uint16_t prefix_len = sizeof(rdx_cmd_mic_gain_query) - 1;
    int32_t fields[1];
    uint8_t gain = 0;
    int ret;

    if (len <= prefix_len
        || rdx_v23_parse_decimal_fields(data + prefix_len,
                                          len - prefix_len,
                                          fields, 1)) {
        printf("[RDX][DROP] cmd=cmicgain reason=malformed len=%u\n", len);
        return;
    }
    if (!rdx_protocol_v23_query_ready(rdx_v23_management.context, "cmicgain", len)) {
        return;
    }

    if (fields[0] > 0xff) {
        printf("[RDX][MIC_GAIN] cmd=cmicgain result=failed reason=unsupported_mode mode=%d\n",
               (int)fields[0]);
        rdx_v23_send_mic_gain_result(1, 1, fields[0], 0);
        return;
    }

    ret = rdx_mic_gain_get_configured((uint8_t)fields[0], &gain);
    rdx_v23_send_mic_gain_result(1, ret ? 1 : 0,
                                  fields[0], ret ? 0 : gain);
}

static void rdx_v23_handle_mic_gain_set(const uint8_t *data, uint16_t len)
{
    const uint16_t prefix_len = sizeof(rdx_cmd_mic_gain_set) - 1;
    int32_t fields[3];
    uint8_t effective = 0;
    int ret;

    if (len <= prefix_len
        || rdx_v23_parse_decimal_fields(data + prefix_len,
                                          len - prefix_len,
                                          fields, 3)) {
        printf("[RDX][DROP] cmd=micgain reason=malformed len=%u\n", len);
        return;
    }
    if (!rdx_protocol_v23_query_ready(rdx_v23_management.context, "micgain", len)) {
        return;
    }

    if (fields[0] > 0xff
        || fields[0] != RDX_MIC_GAIN_MODE_CONFERENCE) {
        printf("[RDX][MIC_GAIN] cmd=micgain result=failed reason=unsupported_mode mode=%d wire_gain1=%d wire_gain2=%d\n",
               (int)fields[0], (int)fields[1], (int)fields[2]);
        rdx_v23_send_mic_gain_result(0, 1, fields[0], 0);
        return;
    }
    if (fields[1] > RDX_MIC_GAIN_LEVEL_MAX
        || fields[2] > RDX_MIC_GAIN_LEVEL_MAX) {
        printf("[RDX][MIC_GAIN] cmd=micgain result=failed reason=range mode=%d wire_gain1=%d wire_gain2=%d\n",
               (int)fields[0], (int)fields[1], (int)fields[2]);
        rdx_v23_send_mic_gain_result(0, 1, fields[0], 0);
        return;
    }
    if (fields[1] != fields[2]) {
        printf("[RDX][MIC_GAIN] cmd=micgain result=failed reason=mirror mode=%d wire_gain1=%d wire_gain2=%d\n",
               (int)fields[0], (int)fields[1], (int)fields[2]);
        rdx_v23_send_mic_gain_result(0, 1, fields[0], 0);
        return;
    }
    if (rdx_v23_management.state_transition_pending) {
        rdx_mic_gain_get_configured((uint8_t)fields[0], &effective);
        printf("[RDX][MIC_GAIN] cmd=micgain result=failed reason=transition_pending action=%s mode=%d wire_gain1=%d wire_gain2=%d effective=%u\n",
               rdx_v23_post_ack_action_name(
                   rdx_v23_management.post_ack_action),
               (int)fields[0], (int)fields[1], (int)fields[2], effective);
        rdx_v23_send_mic_gain_result(0, 1, fields[0], effective);
        return;
    }

    ret = rdx_mic_gain_set_configured((uint8_t)fields[0],
                                      (uint8_t)fields[1], &effective);
    printf("[RDX][MIC_GAIN] cmd=micgain result=%s mode=%d wire_gain1=%d wire_gain2=%d configured_gain=%u apply=next_session ret=%d\n",
           ret ? "failed" : "ok", (int)fields[0],
           (int)fields[1], (int)fields[2], effective, ret);
    rdx_v23_send_mic_gain_result(0, ret ? 1 : 0,
                                  fields[0], effective);
}
#endif

static int rdx_v23_send_bound_result(uint8_t result)
{
    uint8_t response[32];
    uint8_t bound = rdx_device_management_get_bound();
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_BOUND "%u#%u#", result, bound);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=bound reason=encode_failed len=%d\n", len);
        return -1;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=bound wire=%s result=%u bound=%u len=%d ret=%d\n",
           (char *)response, result, bound, len, ret);
    return ret;
}

static int rdx_v23_send_unbound_result(uint8_t result)
{
    uint8_t response[32];
    uint8_t bound = rdx_device_management_get_bound();
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_UNBOUND "%u#%u#", result, bound);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=unbound reason=encode_failed len=%d\n", len);
        return -1;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=unbound wire=%s result=%u bound=%u len=%d ret=%d\n",
           (char *)response, result, bound, len, ret);
    return ret;
}

static int rdx_v23_send_set_default_result(uint8_t result)
{
    uint8_t response[24];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_SET_DEFAULT "%u#", result);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=default reason=encode_failed len=%d\n", len);
        return -1;
    }

    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, len);
    printf("[RDX][TX] cmd=default wire=%s result=%u len=%d ret=%d\n",
           (char *)response, result, len, ret);
    return ret;
}

static void rdx_v23_handle_bound(const uint8_t *data, uint16_t len)
{
    const uint16_t prefix_len = sizeof(rdx_cmd_bound) - 1;
    const uint16_t request_len = prefix_len + 2;
    int ret;
    int ack_ret;

    if (len != request_len || data[len - 1] != '#'
        || (data[prefix_len] != '0' && data[prefix_len] != '1')) {
        printf("[RDX][DROP] cmd=bound reason=malformed len=%u\n", len);
        return;
    }

    if (!rdx_protocol_v23_query_ready(rdx_v23_management.context, "bound", len)) {
        return;
    }

    if (rdx_v23_management.state_transition_pending) {
        printf("[RDX][DROP] cmd=bound reason=transition_pending action=%s len=%u\n",
               rdx_v23_post_ack_action_name(rdx_v23_management.post_ack_action),
               len);
        rdx_v23_send_bound_result(1);
        return;
    }

    if (data[prefix_len] == '1') {
        ret = rdx_device_management_bind();
        rdx_v23_send_bound_result(ret ? 1 : 0);
        return;
    }

    ret = rdx_device_management_unbind();
    ack_ret = rdx_v23_send_bound_result(ret ? 1 : 0);
    if (!ret) {
        rdx_v23_schedule_post_ack_disconnect(RDX_POST_ACK_ACTION_UNBOUND,
                                              ack_ret);
    }
}

static void rdx_v23_handle_unbound(const uint8_t *data, uint16_t len)
{
    const uint16_t prefix_len = sizeof(rdx_cmd_unbound) - 1;
    uint16_t separator;
    int32_t user_para;
    uint8_t format_en;
    int ret;
    int ack_ret;

    if (len <= prefix_len + 3 || data[len - 1] != '#') {
        printf("[RDX][DROP] cmd=unbound reason=malformed len=%u\n", len);
        return;
    }

    for (separator = prefix_len; separator < len - 1; separator++) {
        if (data[separator] == '#') {
            break;
        }
    }
    if (separator == prefix_len || separator + 2 != len - 1
        || data[separator + 2] != '#'
        || (data[separator + 1] != '0' && data[separator + 1] != '1')
        || rdx_v23_parse_nonnegative_s32(data + prefix_len,
                                          separator - prefix_len,
                                          &user_para)) {
        printf("[RDX][DROP] cmd=unbound reason=malformed len=%u\n", len);
        return;
    }
    format_en = data[separator + 1] - '0';

    if (!rdx_protocol_v23_query_ready(rdx_v23_management.context, "unbound", len)) {
        return;
    }
    if (rdx_v23_management.state_transition_pending) {
        printf("[RDX][DROP] cmd=unbound reason=transition_pending action=%s len=%u\n",
               rdx_v23_post_ack_action_name(rdx_v23_management.post_ack_action),
               len);
        rdx_v23_send_unbound_result(1);
        return;
    }

    printf("[RDX][SESSION] cmd=unbound user_para=%d format_en=%u storage_format=ignored\n",
           (int)user_para, format_en);
    ret = rdx_device_management_unbind();
    ack_ret = rdx_v23_send_unbound_result(ret ? 1 : 0);
    if (!ret) {
        rdx_v23_schedule_post_ack_disconnect(RDX_POST_ACK_ACTION_UNBOUND,
                                              ack_ret);
    }
}

static void rdx_v23_handle_set_default(const uint8_t *data, uint16_t len)
{
    int ret;
    int ack_ret;

    if (!rdx_protocol_v23_data_equals(data, len, rdx_cmd_set_default,
                              sizeof(rdx_cmd_set_default) - 1)) {
        printf("[RDX][DROP] cmd=default reason=malformed len=%u\n", len);
        return;
    }
    if (!rdx_protocol_v23_query_ready(rdx_v23_management.context, "default", len)) {
        return;
    }
    if (rdx_v23_management.state_transition_pending) {
        printf("[RDX][DROP] cmd=default reason=transition_pending action=%s len=%u\n",
               rdx_v23_post_ack_action_name(rdx_v23_management.post_ack_action),
               len);
        rdx_v23_send_set_default_result(1);
        return;
    }

    ret = rdx_device_management_restore_defaults();
    ack_ret = rdx_v23_send_set_default_result(ret ? 1 : 0);
    if (!ret) {
        rdx_v23_schedule_post_ack_disconnect(
            RDX_POST_ACK_ACTION_SET_DEFAULT, ack_ret);
    }
}

static void rdx_v23_handle_appkey(const uint8_t *data, uint16_t len)
{
    const struct rdx_product_profile *profile = rdx_product_profile_get();
    const uint16_t prefix_len = sizeof(rdx_cmd_appkey) - 1;
    const uint16_t request_len = prefix_len + RDX_APPKEY_PAYLOAD_SIZE + 1;
    static const uint8_t success[] = "*DEV#appkey#0#";
    static const uint8_t failure[] = "*DEV#appkey#1#";
    char appkey_hex[RDX_APPKEY_HEX_STRING_SIZE];
    char appkey_plaintext[RDX_APPKEY_PLAINTEXT_SIZE + 1];
    char appkey_plaintext_hex[RDX_APPKEY_PLAINTEXT_SIZE * 2 + 1];
    int result = -1;
    int send_ret;

    if (len != request_len || data[len - 1] != '#') {
        printf("[RDX][RX] cmd=appkey len=%u app_ready=%u\n",
               len, rdx_v23_management.context->session.authenticated);
        printf("[RDX][DROP] cmd=appkey reason=malformed len=%u\n", len);
        goto reject;
    }

    rdx_v23_bytes_to_hex(data + prefix_len, RDX_APPKEY_PAYLOAD_SIZE,
                          appkey_hex, sizeof(appkey_hex));
    result = rdx_appkey_verify(data + prefix_len,
                               RDX_APPKEY_PAYLOAD_SIZE,
                               profile->app_keys,
                               profile->app_key_count,
                               appkey_plaintext,
                               sizeof(appkey_plaintext));
    rdx_v23_bytes_to_hex((const uint8_t *)appkey_plaintext,
                          RDX_APPKEY_PLAINTEXT_SIZE,
                          appkey_plaintext_hex,
                          sizeof(appkey_plaintext_hex));
    printf("[RDX][RX] cmd=appkey wire_payload=%s appkey=%s appkey_hex=%s len=%u app_ready=%u\n",
           appkey_hex, appkey_plaintext, appkey_plaintext_hex,
           len, rdx_v23_management.context->session.authenticated);
    if (result) {
        printf("[RDX][AUTH] verify=failed\n");
        goto reject;
    }

    if (!rdx_v23_management.context->session.ccc_ready) {
        printf("[RDX][AUTH] verify=ok state=invalid ccc=0\n");
        goto reject;
    }

    result = rdx_protocol_v23_send(rdx_v23_management.context, success, sizeof(success) - 1);
    printf("[RDX][TX] cmd=appkey wire=%s len=%u ret=%d\n",
           (char *)success, (unsigned int)(sizeof(success) - 1), result);
    if (!result) {
        if (!rdx_protocol_session_mark_authenticated(
                &rdx_v23_management.context->session)) {
            printf("[RDX][AUTH] verify=ok app_ready=1\n");
            return;
        }
        printf("[RDX][AUTH] verify=ok state=invalid_after_ack\n");
        rdx_transport_disconnect();
        return;
    }
    printf("[RDX][AUTH] verify=ok ack=failed ret=%d\n", result);

reject:
    send_ret = rdx_protocol_v23_send(rdx_v23_management.context, failure, sizeof(failure) - 1);
    printf("[RDX][TX] cmd=appkey wire=%s len=%u ret=%d\n",
           (char *)failure, (unsigned int)(sizeof(failure) - 1), send_ret);
    rdx_transport_disconnect();
}

#if RDX_CFG_RTC_ENABLE
static void rdx_v23_handle_rtc(const uint8_t *data, uint16_t len)
{
    const uint16_t prefix_len = sizeof(rdx_cmd_rtc) - 1;
    uint8_t response[40];
    uint16_t value_len;
    uint16_t i;
    uint32_t timestamp = 0;
    uint8_t result = 1;
    int response_len;
    int ret;

    if (len <= prefix_len || data[len - 1] != '#') {
        printf("[RDX][DROP] cmd=rtc reason=malformed len=%u\n", len);
        goto send_ack;
    }

    value_len = len - prefix_len - 1;
    if (!value_len || value_len > 10) {
        printf("[RDX][DROP] cmd=rtc reason=invalid_timestamp_length value_len=%u\n",
               value_len);
        goto send_ack;
    }

    for (i = 0; i < value_len; i++) {
        if (data[prefix_len + i] < '0' || data[prefix_len + i] > '9') {
            printf("[RDX][DROP] cmd=rtc reason=invalid_timestamp index=%u\n", i);
            timestamp = 0;
            goto send_ack;
        }
        if (timestamp > (0xffffffffUL - (data[prefix_len + i] - '0')) / 10) {
            printf("[RDX][DROP] cmd=rtc reason=timestamp_overflow\n");
            timestamp = 0;
            goto send_ack;
        }
        timestamp = timestamp * 10 + data[prefix_len + i] - '0';
    }

    ret = rdx_rtc_set_timestamp(timestamp);
    result = ret == 0 ? 0 : 1;

send_ack:
    response_len = snprintf((char *)response, sizeof(response),
                             "*DEV#rtc#%u#%u#", result,
                             (unsigned int)timestamp);
    if (response_len <= 0 || response_len >= sizeof(response)) {
        printf("[RDX][TX] cmd=rtc reason=encode_failed len=%d\n",
               response_len);
        return;
    }
    ret = rdx_protocol_v23_send(rdx_v23_management.context, response, response_len);
    printf("[RDX][TX] cmd=rtc wire=%s len=%u ret=%d\n",
           (char *)response, (unsigned int)response_len, ret);
}
#endif
void rdx_protocol_v23_management_init(
    struct rdx_protocol_v23_context *context)
{
    memset(&rdx_v23_management, 0, sizeof(rdx_v23_management));
    rdx_v23_management.context = context;
}

void rdx_protocol_v23_management_exit(void)
{
    rdx_v23_cancel_post_ack_action("protocol_exit");
}

void rdx_protocol_v23_management_handle_link_change(uint8_t connected)
{
    rdx_v23_cancel_post_ack_action(connected ? "new_connection"
                                             : "link_disconnected");
}

uint8_t rdx_protocol_v23_management_handle_command(const uint8_t *data, uint16_t len)
{
    if (rdx_protocol_v23_data_equals(
            data, len, rdx_cmd_battery, sizeof(rdx_cmd_battery) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "battery", data, len);
        rdx_v23_send_battery();
    } else if (rdx_protocol_v23_data_equals(
                   data, len, rdx_cmd_version,
                   sizeof(rdx_cmd_version) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "version", data, len);
        rdx_v23_send_version();
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_appkey,
                   sizeof(rdx_cmd_appkey) - 1)) {
        rdx_v23_handle_appkey(data, len);
#if RDX_CFG_RTC_ENABLE
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_rtc, sizeof(rdx_cmd_rtc) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "rtc", data, len);
        rdx_v23_handle_rtc(data, len);
#endif
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_ostype,
                   sizeof(rdx_cmd_ostype) - 1)) {
        int ret;

        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "ostype", data, len);
        ret = rdx_protocol_v23_send(rdx_v23_management.context,
                                    rdx_rsp_ostype,
                                    sizeof(rdx_rsp_ostype) - 1);
        printf("[RDX][TX] cmd=ostype wire=%s len=%u ret=%d\n",
               (char *)rdx_rsp_ostype,
               (unsigned int)(sizeof(rdx_rsp_ostype) - 1), ret);
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_auth_sn,
                   sizeof(rdx_cmd_auth_sn) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "authsn", data, len);
        if (!rdx_protocol_v23_data_equals(
                data, len, rdx_cmd_auth_sn,
                sizeof(rdx_cmd_auth_sn) - 1)) {
            printf("[RDX][DROP] cmd=authsn reason=malformed len=%u\n", len);
        } else if (rdx_protocol_v23_query_ready(
                       rdx_v23_management.context, "authsn", len)) {
            rdx_v23_send_auth_sn();
        }
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_offtime_check,
                   sizeof(rdx_cmd_offtime_check) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "cofftime", data, len);
        if (!rdx_protocol_v23_data_equals(
                data, len, rdx_cmd_offtime_check,
                sizeof(rdx_cmd_offtime_check) - 1)) {
            printf("[RDX][DROP] cmd=cofftime reason=malformed len=%u\n", len);
        } else if (rdx_protocol_v23_query_ready(
                       rdx_v23_management.context, "cofftime", len)) {
            rdx_v23_send_offtime_check();
        }
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_bound, sizeof(rdx_cmd_bound) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "bound", data, len);
        rdx_v23_handle_bound(data, len);
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_unbound,
                   sizeof(rdx_cmd_unbound) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "unbound", data, len);
        rdx_v23_handle_unbound(data, len);
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_set_default,
                   sizeof(rdx_cmd_set_default) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "default", data, len);
        rdx_v23_handle_set_default(data, len);
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_ble_name_query,
                   sizeof(rdx_cmd_ble_name_query) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "cblename", data, len);
        rdx_v23_handle_ble_name_query(data, len);
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_ble_name_set,
                   sizeof(rdx_cmd_ble_name_set) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "blename", data, len);
        rdx_v23_handle_ble_name_set(data, len);
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_mic_gain_query,
                   sizeof(rdx_cmd_mic_gain_query) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "cmicgain", data, len);
        rdx_v23_handle_mic_gain_query(data, len);
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_mic_gain_set,
                   sizeof(rdx_cmd_mic_gain_set) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_management.context,
                                "micgain", data, len);
        rdx_v23_handle_mic_gain_set(data, len);
#endif
    } else {
        return 0;
    }
    return 1;
}

#endif /* TCFG_RDX_ENABLE */
