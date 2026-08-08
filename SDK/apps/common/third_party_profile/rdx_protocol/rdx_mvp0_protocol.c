#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_appkey_verifier.h"
#include "rdx_device_management.h"
#include "rdx_identity.h"
#include "rdx_mvp0_compat_config.h"
#include "rdx_mvp0_protocol.h"
#include "rdx_platform_br56.h"
#include "rdx_protocol_defs.h"

#define RDX_APPKEY_PAYLOAD_SIZE               32
#define RDX_APPKEY_HEX_STRING_SIZE            (RDX_APPKEY_PAYLOAD_SIZE * 2 + 1)

static const u8 rdx_mvp0_ping[] = "RDX_MVP0_PING";
static const u8 rdx_mvp0_pong[] = "RDX_MVP0_PONG";
static const u8 rdx_cmd_battery[] = "*APP#battery#";
static const u8 rdx_cmd_version[] = "*APP#version#";
static const u8 rdx_cmd_appkey[] = "*APP#appkey#";
static const u8 rdx_cmd_rtc[] = "*APP#rtc#";
static const u8 rdx_cmd_ostype[] = "*APP#ostype#";
static const u8 rdx_cmd_auth_sn[] = RDX_CMD_DL_AUTH_SN;
static const u8 rdx_cmd_offtime_check[] = RDX_CMD_DL_OFFTIME_CHECK;
static const u8 rdx_cmd_bound[] = RDX_CMD_DL_BOUND;
static const u8 rdx_rsp_ostype[] = "*DEV#ostype#";
static const char *const rdx_mvp0_app_keys[] = {
    RDX_COMPAT_APP_KEY_LIST
};

static rdx_mvp0_send_callback_t rdx_mvp0_send_callback;
static rdx_mvp0_disconnect_callback_t rdx_mvp0_disconnect_callback;

typedef struct {
    u8 connected;
    u8 identity_read;
    u8 ccc_ready;
    u8 app_ready;
} rdx_mvp0_state_t;

static rdx_mvp0_state_t rdx_mvp0_state;

static void rdx_mvp0_log_rx(const char *command, const u8 *data, u16 len)
{
    printf("[RDX][RX] cmd=%s wire=%.*s len=%u app_ready=%u\n",
           command, (int)len, (char *)data,
           len, rdx_mvp0_state.app_ready);
}

static void rdx_mvp0_bytes_to_hex(const u8 *data, u16 len,
                                  char *output, u16 output_size)
{
    static const char hex[] = "0123456789ABCDEF";
    u16 i;

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

static u8 rdx_mvp0_data_equals(const u8 *data, u16 len,
                               const u8 *expected, u16 expected_len)
{
    return len == expected_len && !memcmp(data, expected, expected_len);
}

static u8 rdx_mvp0_data_starts_with(const u8 *data, u16 len,
                                    const u8 *prefix, u16 prefix_len)
{
    return len >= prefix_len && !memcmp(data, prefix, prefix_len);
}

static int rdx_mvp0_send(const u8 *data, u16 len)
{
    if (!rdx_mvp0_state.connected || !rdx_mvp0_state.ccc_ready
        || !rdx_mvp0_send_callback) {
        return -1;
    }
    return rdx_mvp0_send_callback(data, len);
}

static void rdx_mvp0_send_battery(void)
{
    u8 response[32];
    int len;
    int ret;

    /* The 701 application keeps the legacy wire order C, R, L. */
    len = snprintf((char *)response, sizeof(response),
                   "*DEV#battery#0#0#%u#", RDX_MVP0_FIXED_BATTERY_LEVEL);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=battery reason=encode_failed len=%d\n", len);
        return;
    }

    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=battery wire=%s len=%d ret=%d\n",
           (char *)response, len, ret);
}

static void rdx_mvp0_send_version(void)
{
    u8 response[48];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   "*DEV#version#%s#%s#",
                   RDX_COMPAT_HARDWARE_VERSION, RDX_COMPAT_FIRMWARE_VERSION);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=version reason=encode_failed len=%d\n", len);
        return;
    }


    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=version wire=%s len=%d ret=%d\n",
           (char *)response, len, ret);
}

static void rdx_mvp0_send_auth_sn(void)
{
    u8 response[96];
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

    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=authsn wire=%s len=%d ret=%d\n",
           (char *)response, len, ret);
}

static void rdx_mvp0_send_offtime_check(void)
{
    u8 response[40];
    u32 minutes = 0;
    u8 result;
    int len;
    int ret;

    result = rdx_platform_get_auto_off_time_minutes(&minutes) ? 1 : 0;
    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_OFFTIME_CHECK "%d#%u#",
                   (int)result, (unsigned int)minutes);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=cofftime reason=encode_failed len=%d\n", len);
        return;
    }

    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=cofftime wire=%s result=%u minutes=%u len=%d ret=%d\n",
           (char *)response, (unsigned int)result,
           (unsigned int)minutes, len, ret);
}

static u8 rdx_mvp0_management_query_ready(const char *command, u16 len)
{
    if (rdx_mvp0_state.app_ready) {
        return 1;
    }

    printf("[RDX][DROP] cmd=%s reason=app_not_ready len=%u\n", command, len);
    return 0;
}

static void rdx_mvp0_send_bound_result(u8 result)
{
    u8 response[32];
    u8 bound = rdx_device_management_get_bound();
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_BOUND "%u#%u#", result, bound);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=bound reason=encode_failed len=%d\n", len);
        return;
    }

    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=bound wire=%s result=%u bound=%u len=%d ret=%d\n",
           (char *)response, result, bound, len, ret);
}

static void rdx_mvp0_handle_bound(const u8 *data, u16 len)
{
    const u16 prefix_len = sizeof(rdx_cmd_bound) - 1;
    const u16 request_len = prefix_len + 2;
    int ret;

    if (len != request_len || data[len - 1] != '#'
        || (data[prefix_len] != '0' && data[prefix_len] != '1')) {
        printf("[RDX][DROP] cmd=bound reason=malformed len=%u\n", len);
        return;
    }

    if (!rdx_mvp0_management_query_ready("bound", len)) {
        return;
    }

    if (data[prefix_len] != '1') {
        printf("[RDX][DROP] cmd=bound reason=unbind_deferred len=%u\n", len);
        rdx_mvp0_send_bound_result(1);
        return;
    }

    ret = rdx_device_management_bind();
    rdx_mvp0_send_bound_result(ret ? 1 : 0);
}

static void rdx_mvp0_handle_appkey(const u8 *data, u16 len)
{
    const u16 prefix_len = sizeof(rdx_cmd_appkey) - 1;
    const u16 request_len = prefix_len + RDX_APPKEY_PAYLOAD_SIZE + 1;
    static const u8 success[] = "*DEV#appkey#0#";
    static const u8 failure[] = "*DEV#appkey#1#";
    char appkey_hex[RDX_APPKEY_HEX_STRING_SIZE];
    char appkey_plaintext[RDX_APPKEY_PLAINTEXT_SIZE + 1];
    char appkey_plaintext_hex[RDX_APPKEY_PLAINTEXT_SIZE * 2 + 1];
    int result = -1;
    int send_ret;

    if (len != request_len || data[len - 1] != '#') {
        printf("[RDX][RX] cmd=appkey len=%u app_ready=%u\n",
               len, rdx_mvp0_state.app_ready);
        printf("[RDX][DROP] cmd=appkey reason=malformed len=%u\n", len);
        goto reject;
    }

    rdx_mvp0_bytes_to_hex(data + prefix_len, RDX_APPKEY_PAYLOAD_SIZE,
                          appkey_hex, sizeof(appkey_hex));
    result = rdx_appkey_verify(data + prefix_len,
                               RDX_APPKEY_PAYLOAD_SIZE,
                               rdx_mvp0_app_keys,
                               sizeof(rdx_mvp0_app_keys)
                               / sizeof(rdx_mvp0_app_keys[0]),
                               appkey_plaintext,
                               sizeof(appkey_plaintext));
    rdx_mvp0_bytes_to_hex((const u8 *)appkey_plaintext,
                          RDX_APPKEY_PLAINTEXT_SIZE,
                          appkey_plaintext_hex,
                          sizeof(appkey_plaintext_hex));
    printf("[RDX][RX] cmd=appkey wire_payload=%s appkey=%s appkey_hex=%s len=%u app_ready=%u\n",
           appkey_hex, appkey_plaintext, appkey_plaintext_hex,
           len, rdx_mvp0_state.app_ready);
    if (result) {
        printf("[RDX][AUTH] verify=failed\n");
        goto reject;
    }

    if (!rdx_mvp0_state.ccc_ready) {
        printf("[RDX][AUTH] verify=ok state=invalid ccc=0\n");
        goto reject;
    }

    result = rdx_mvp0_send(success, sizeof(success) - 1);
    printf("[RDX][TX] cmd=appkey wire=%s len=%u ret=%d\n",
           (char *)success, (unsigned int)(sizeof(success) - 1), result);
    if (!result) {
        rdx_mvp0_state.app_ready = 1;
        printf("[RDX][AUTH] verify=ok app_ready=1\n");
        return;
    }
    printf("[RDX][AUTH] verify=ok ack=failed ret=%d\n", result);

reject:
    send_ret = rdx_mvp0_send(failure, sizeof(failure) - 1);
    printf("[RDX][TX] cmd=appkey wire=%s len=%u ret=%d\n",
           (char *)failure, (unsigned int)(sizeof(failure) - 1), send_ret);
    if (rdx_mvp0_disconnect_callback) {
        rdx_mvp0_disconnect_callback();
    }
}

static void rdx_mvp0_handle_rtc(const u8 *data, u16 len)
{
    const u16 prefix_len = sizeof(rdx_cmd_rtc) - 1;
    static const u8 response_prefix[] = "*DEV#rtc#0#";
    u8 response[40];
    u16 value_len;
    u16 response_len;
    u16 i;
    int ret;

    if (len <= prefix_len || data[len - 1] != '#') {
        printf("[RDX][DROP] cmd=rtc reason=malformed len=%u\n", len);
        return;
    }

    value_len = len - prefix_len - 1;
    if (!value_len || value_len > 10) {
        printf("[RDX][DROP] cmd=rtc reason=invalid_timestamp_length value_len=%u\n",
               value_len);
        return;
    }

    for (i = 0; i < value_len; i++) {
        if (data[prefix_len + i] < '0' || data[prefix_len + i] > '9') {
            printf("[RDX][DROP] cmd=rtc reason=invalid_timestamp index=%u\n", i);
            return;
        }
    }

    response_len = sizeof(response_prefix) - 1 + value_len + 1;
    memcpy(response, response_prefix, sizeof(response_prefix) - 1);
    memcpy(response + sizeof(response_prefix) - 1,
           data + prefix_len, value_len);
    response[response_len - 1] = '#';
    response[response_len] = '\0';
    ret = rdx_mvp0_send(response, response_len);
    printf("[RDX][TX] cmd=rtc wire=%s len=%u ret=%d\n",
           (char *)response, response_len, ret);
}

void rdx_mvp0_protocol_init(rdx_mvp0_send_callback_t send_callback,
                            rdx_mvp0_disconnect_callback_t disconnect_callback)
{
    rdx_mvp0_send_callback = send_callback;
    rdx_mvp0_disconnect_callback = disconnect_callback;
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
}

void rdx_mvp0_protocol_exit(void)
{
    rdx_mvp0_send_callback = NULL;
    rdx_mvp0_disconnect_callback = NULL;
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
}

void rdx_mvp0_protocol_set_connected(u8 connected)
{
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
    rdx_mvp0_state.connected = !!connected;
    printf("[RDX][SESSION] link=%s app_ready=0\n",
           rdx_mvp0_state.connected ? "connected" : "disconnected");
}

void rdx_mvp0_protocol_set_identity_read(void)
{
    if (rdx_mvp0_state.connected && !rdx_mvp0_state.identity_read) {
        rdx_mvp0_state.identity_read = 1;
        printf("[RDX][GATT] identity=read\n");
    }
}

void rdx_mvp0_protocol_set_ccc(u8 enabled)
{
    rdx_mvp0_state.ccc_ready = !!enabled;
    if (!rdx_mvp0_state.ccc_ready) {
        rdx_mvp0_state.app_ready = 0;
    }
    printf("[RDX][SESSION] ccc=%u app_ready=%u\n",
           rdx_mvp0_state.ccc_ready, rdx_mvp0_state.app_ready);
}

void rdx_mvp0_protocol_receive(const u8 *data, u16 len)
{
    if (!data || !len) {
        return;
    }

    if (rdx_mvp0_data_equals(data, len, rdx_mvp0_ping,
                             sizeof(rdx_mvp0_ping) - 1)) {
        int ret;

        rdx_mvp0_log_rx("ping", data, len);
        ret = rdx_mvp0_send(rdx_mvp0_pong, sizeof(rdx_mvp0_pong) - 1);
        printf("[RDX][TX] cmd=pong wire=%s len=%u ret=%d\n",
               (char *)rdx_mvp0_pong,
               (unsigned int)(sizeof(rdx_mvp0_pong) - 1), ret);
    } else if (rdx_mvp0_data_equals(data, len, rdx_cmd_battery,
                                    sizeof(rdx_cmd_battery) - 1)) {
        rdx_mvp0_log_rx("battery", data, len);
        rdx_mvp0_send_battery();
    } else if (rdx_mvp0_data_equals(data, len, rdx_cmd_version,
                                    sizeof(rdx_cmd_version) - 1)) {
        rdx_mvp0_log_rx("version", data, len);
        rdx_mvp0_send_version();
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_appkey,
                                         sizeof(rdx_cmd_appkey) - 1)) {
        rdx_mvp0_handle_appkey(data, len);
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_rtc,
                                         sizeof(rdx_cmd_rtc) - 1)) {
        rdx_mvp0_log_rx("rtc", data, len);
        rdx_mvp0_handle_rtc(data, len);
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_ostype,
                                         sizeof(rdx_cmd_ostype) - 1)) {
        int ret;

        rdx_mvp0_log_rx("ostype", data, len);
        ret = rdx_mvp0_send(rdx_rsp_ostype, sizeof(rdx_rsp_ostype) - 1);
        printf("[RDX][TX] cmd=ostype wire=%s len=%u ret=%d\n",
               (char *)rdx_rsp_ostype,
               (unsigned int)(sizeof(rdx_rsp_ostype) - 1), ret);
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_auth_sn,
                                         sizeof(rdx_cmd_auth_sn) - 1)) {
        rdx_mvp0_log_rx("authsn", data, len);
        if (!rdx_mvp0_data_equals(data, len, rdx_cmd_auth_sn,
                                  sizeof(rdx_cmd_auth_sn) - 1)) {
            printf("[RDX][DROP] cmd=authsn reason=malformed len=%u\n", len);
        } else if (rdx_mvp0_management_query_ready("authsn", len)) {
            rdx_mvp0_send_auth_sn();
        }
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_offtime_check,
                                         sizeof(rdx_cmd_offtime_check) - 1)) {
        rdx_mvp0_log_rx("cofftime", data, len);
        if (!rdx_mvp0_data_equals(data, len, rdx_cmd_offtime_check,
                                  sizeof(rdx_cmd_offtime_check) - 1)) {
            printf("[RDX][DROP] cmd=cofftime reason=malformed len=%u\n", len);
        } else if (rdx_mvp0_management_query_ready("cofftime", len)) {
            rdx_mvp0_send_offtime_check();
        }
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_bound,
                                         sizeof(rdx_cmd_bound) - 1)) {
        rdx_mvp0_log_rx("bound", data, len);
        rdx_mvp0_handle_bound(data, len);
    } else {
        printf("[RDX][DROP] cmd=unknown wire=%.*s len=%u type=%02X%02X\n",
               (int)len, (char *)data, len,
               data[0], len > 1 ? data[1] : 0);
    }
}

#endif
