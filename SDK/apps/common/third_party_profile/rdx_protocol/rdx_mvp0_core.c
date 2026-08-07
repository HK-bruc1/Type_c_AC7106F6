#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_crypto.h"
#include "rdx_mvp0_core.h"
#include "rdx_product_config.h"

#define RDX_APPKEY_PAYLOAD_SIZE               32

static const u8 rdx_mvp0_ping[] = "RDX_MVP0_PING";
static const u8 rdx_mvp0_pong[] = "RDX_MVP0_PONG";
static const u8 rdx_cmd_battery[] = "*APP#battery#";
static const u8 rdx_cmd_version[] = "*APP#version#";
static const u8 rdx_cmd_appkey[] = "*APP#appkey#";
static const u8 rdx_cmd_rtc[] = "*APP#rtc#";
static const u8 rdx_cmd_ostype[] = "*APP#ostype#";
static const u8 rdx_rsp_ostype[] = "*DEV#ostype#";
static const char *const rdx_mvp0_app_keys[] = {
    RDX_MVP0_APP_KEY_LIST
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

    /* The 701 application keeps the legacy wire order C, R, L. */
    len = snprintf((char *)response, sizeof(response),
                   "*DEV#battery#0#0#%u#", RDX_MVP0_BATTERY_LEVEL);
    if (len > 0 && len < sizeof(response)) {
        rdx_mvp0_send(response, len);
    }
}

static void rdx_mvp0_send_version(void)
{
    u8 response[48];
    int len;

    len = snprintf((char *)response, sizeof(response),
                   "*DEV#version#%s#%s#",
                   RDX_MVP0_HARDWARE_VERSION, RDX_MVP0_FIRMWARE_VERSION);
    if (len > 0 && len < sizeof(response)) {
        rdx_mvp0_send(response, len);
    }
}

static void rdx_mvp0_handle_appkey(const u8 *data, u16 len)
{
    const u16 prefix_len = sizeof(rdx_cmd_appkey) - 1;
    const u16 request_len = prefix_len + RDX_APPKEY_PAYLOAD_SIZE + 1;
    static const u8 success[] = "*DEV#appkey#0#";
    static const u8 failure[] = "*DEV#appkey#1#";
    int result = -1;

    if (len != request_len || data[len - 1] != '#') {
        printf("[RDX] malformed appkey len=%u\n", len);
        goto reject;
    }

    result = rdx_crypto_verify_appkey(data + prefix_len,
                                      RDX_APPKEY_PAYLOAD_SIZE,
                                      rdx_mvp0_app_keys,
                                      sizeof(rdx_mvp0_app_keys)
                                      / sizeof(rdx_mvp0_app_keys[0]));
    if (result) {
        printf("[RDX] appkey verify failed\n");
        goto reject;
    }

    if (!rdx_mvp0_state.ccc_ready) {
        printf("[RDX] appkey handshake state invalid ccc=0\n");
        goto reject;
    }

    result = rdx_mvp0_send(success, sizeof(success) - 1);
    if (!result) {
        rdx_mvp0_state.app_ready = 1;
        printf("[RDX] APP-ready\n");
        return;
    }
    printf("[RDX] appkey response send failed=%d\n", result);

reject:
    rdx_mvp0_send(failure, sizeof(failure) - 1);
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

    if (len <= prefix_len || data[len - 1] != '#') {
        return;
    }

    value_len = len - prefix_len - 1;
    if (!value_len || value_len > 10) {
        return;
    }

    for (i = 0; i < value_len; i++) {
        if (data[prefix_len + i] < '0' || data[prefix_len + i] > '9') {
            return;
        }
    }

    response_len = sizeof(response_prefix) - 1 + value_len + 1;
    memcpy(response, response_prefix, sizeof(response_prefix) - 1);
    memcpy(response + sizeof(response_prefix) - 1,
           data + prefix_len, value_len);
    response[response_len - 1] = '#';
    rdx_mvp0_send(response, response_len);
}

void rdx_mvp0_core_init(rdx_mvp0_send_callback_t send_callback,
                        rdx_mvp0_disconnect_callback_t disconnect_callback)
{
    rdx_mvp0_send_callback = send_callback;
    rdx_mvp0_disconnect_callback = disconnect_callback;
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
}

void rdx_mvp0_core_exit(void)
{
    rdx_mvp0_send_callback = NULL;
    rdx_mvp0_disconnect_callback = NULL;
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
}

void rdx_mvp0_core_set_connected(u8 connected)
{
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
    rdx_mvp0_state.connected = !!connected;
    printf("[RDX] link %s\n", rdx_mvp0_state.connected ? "connected" : "disconnected");
}

void rdx_mvp0_core_set_identity_read(void)
{
    if (rdx_mvp0_state.connected && !rdx_mvp0_state.identity_read) {
        rdx_mvp0_state.identity_read = 1;
        printf("[RDX] identity read\n");
    }
}

void rdx_mvp0_core_set_ccc(u8 enabled)
{
    rdx_mvp0_state.ccc_ready = !!enabled;
    if (!rdx_mvp0_state.ccc_ready) {
        rdx_mvp0_state.app_ready = 0;
    }
}

void rdx_mvp0_core_receive(const u8 *data, u16 len)
{
    if (!data || !len) {
        return;
    }

    printf("[RDX] rx len=%u type=%02x%02x\n", len, data[0], len > 1 ? data[1] : 0);

    if (rdx_mvp0_data_equals(data, len, rdx_mvp0_ping,
                             sizeof(rdx_mvp0_ping) - 1)) {
        rdx_mvp0_send(rdx_mvp0_pong, sizeof(rdx_mvp0_pong) - 1);
    } else if (rdx_mvp0_data_equals(data, len, rdx_cmd_battery,
                                    sizeof(rdx_cmd_battery) - 1)) {
        rdx_mvp0_send_battery();
    } else if (rdx_mvp0_data_equals(data, len, rdx_cmd_version,
                                    sizeof(rdx_cmd_version) - 1)) {
        rdx_mvp0_send_version();
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_appkey,
                                         sizeof(rdx_cmd_appkey) - 1)) {
        rdx_mvp0_handle_appkey(data, len);
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_rtc,
                                         sizeof(rdx_cmd_rtc) - 1)) {
        rdx_mvp0_handle_rtc(data, len);
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_ostype,
                                         sizeof(rdx_cmd_ostype) - 1)) {
        rdx_mvp0_send(rdx_rsp_ostype, sizeof(rdx_rsp_ostype) - 1);
    }
}

#endif
