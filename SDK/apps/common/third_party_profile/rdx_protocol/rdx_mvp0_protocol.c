#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_appkey_verifier.h"
#include "rdx_ble_transport_br56.h"
#include "rdx_device_management.h"
#include "rdx_identity.h"
#include "rdx_mvp0_compat_config.h"
#include "rdx_mvp0_protocol.h"
#include "rdx_platform_br56.h"
#include "rdx_protocol_defs.h"
#include "rdx_rtc.h"
#include "media/audio_base.h"
#include "ai_voice_recoder.h"
#if TCFG_APP_PC_EN && (TCFG_USB_SLAVE_AUDIO_MIC_ENABLE || TCFG_USB_SLAVE_AUDIO_SPK_ENABLE)
#include "uac_stream.h"
#endif

#define RDX_APPKEY_PAYLOAD_SIZE               32
#define RDX_APPKEY_HEX_STRING_SIZE            (RDX_APPKEY_PAYLOAD_SIZE * 2 + 1)
#define RDX_POST_ACK_DISCONNECT_DELAY_MS       100
#define RDX_RECORD_WARMUP_FRAMES                10
#define RDX_RECORD_EXPECTED_FRAME_SIZE           80
#define RDX_RECORD_MAX_FRAME_SIZE                96
#define RDX_RECORD_STREAM_PACKET_SIZE           128
#define RDX_RECORD_TX_QUEUE_DEPTH                16

static const u8 rdx_mvp0_ping[] = "RDX_MVP0_PING";
static const u8 rdx_mvp0_pong[] = "RDX_MVP0_PONG";
static const u8 rdx_cmd_battery[] = "*APP#battery#";
static const u8 rdx_cmd_version[] = "*APP#version#";
static const u8 rdx_cmd_appkey[] = "*APP#appkey#";
static const u8 rdx_cmd_rtc[] = "*APP#rtc#";
static const u8 rdx_cmd_ostype[] = "*APP#ostype#";
static const u8 rdx_cmd_record[] = "*APP#record#";
static const u8 rdx_cmd_auth_sn[] = RDX_CMD_DL_AUTH_SN;
static const u8 rdx_cmd_offtime_check[] = RDX_CMD_DL_OFFTIME_CHECK;
static const u8 rdx_cmd_bound[] = RDX_CMD_DL_BOUND;
static const u8 rdx_cmd_unbound[] = RDX_CMD_DL_UNBOUND;
static const u8 rdx_cmd_set_default[] = RDX_CMD_DL_SET_DEFAULT;
static const u8 rdx_rsp_ostype[] = "*DEV#ostype#";
static const u8 rdx_rsp_record_active[] =
    "*DEV#record#1#0#1#0#1#0#";
static const u8 rdx_rsp_record_stop[] =
    "*DEV#record#0#0#1#0#1#0#";
static const char *const rdx_mvp0_app_keys[] = {
    RDX_COMPAT_APP_KEY_LIST
};

static rdx_mvp0_send_callback_t rdx_mvp0_send_callback;
static rdx_mvp0_disconnect_callback_t rdx_mvp0_disconnect_callback;

typedef enum {
    RDX_POST_ACK_ACTION_NONE = 0,
    RDX_POST_ACK_ACTION_UNBOUND,
    RDX_POST_ACK_ACTION_SET_DEFAULT,
} rdx_post_ack_action_t;

typedef struct {
    u8 connected;
    u8 identity_read;
    u8 ccc_ready;
    u8 app_ready;
    u8 state_transition_pending;
    u8 post_ack_action;
    u16 post_ack_timer_id;
} rdx_mvp0_state_t;

static rdx_mvp0_state_t rdx_mvp0_state;

static u8 rdx_mvp0_management_query_ready(const char *command, u16 len);

typedef enum {
    RDX_RECORD_STATE_IDLE = 0,
    RDX_RECORD_STATE_STARTING,
    RDX_RECORD_STATE_ACTIVE,
    RDX_RECORD_STATE_STOPPING,
} rdx_record_state_t;

typedef struct {
    volatile u8 state;
    u32 frame_count;
    u32 sent_count;
    u32 warmup_count;
    u32 invalid_count;
    u32 send_fail_count;
    u32 queued_count;
    u32 queue_drop_count;
    u8 tx_head;
    u8 tx_tail;
    u8 tx_count;
    u8 tx_peak;
    u8 opus_format_checked;
    u8 opus_channels;
    u16 tx_len[RDX_RECORD_TX_QUEUE_DEPTH];
    u8 tx_packet[RDX_RECORD_TX_QUEUE_DEPTH][RDX_RECORD_STREAM_PACKET_SIZE];
} rdx_record_runtime_t;

static rdx_record_runtime_t rdx_record_runtime;

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

static int rdx_mvp0_record_send_state(u8 active)
{
    const u8 *response = active ? rdx_rsp_record_active
                         : rdx_rsp_record_stop;
    u16 len = active ? sizeof(rdx_rsp_record_active) - 1
              : sizeof(rdx_rsp_record_stop) - 1;
    int ret = rdx_mvp0_send(response, len);

    printf("[RDX][TX] cmd=record state=%s wire=%s len=%u ret=%d\n",
           active ? "active" : "stop", (char *)response,
           (unsigned int)len, ret);
    return ret;
}

static u8 rdx_mvp0_record_usb_audio_busy(void)
{
#if TCFG_APP_PC_EN && TCFG_USB_SLAVE_AUDIO_MIC_ENABLE
    if (uac_get_mic_stream_status()) {
        return 1;
    }
#endif
#if TCFG_APP_PC_EN && TCFG_USB_SLAVE_AUDIO_SPK_ENABLE
    if (uac_speaker_stream_status()) {
        return 1;
    }
#endif
    return 0;
}

static int rdx_mvp0_record_queue_packet(const u8 *packet, u16 len)
{
    u8 tail;

    if (!packet || !len || len > RDX_RECORD_STREAM_PACKET_SIZE) {
        return -1;
    }
    if (rdx_record_runtime.tx_count >= RDX_RECORD_TX_QUEUE_DEPTH) {
        rdx_record_runtime.queue_drop_count++;
        return -1;
    }

    tail = rdx_record_runtime.tx_tail;
    memcpy(rdx_record_runtime.tx_packet[tail], packet, len);
    rdx_record_runtime.tx_len[tail] = len;
    rdx_record_runtime.tx_tail = (tail + 1) % RDX_RECORD_TX_QUEUE_DEPTH;
    rdx_record_runtime.tx_count++;
    rdx_record_runtime.queued_count++;
    if (rdx_record_runtime.tx_count > rdx_record_runtime.tx_peak) {
        rdx_record_runtime.tx_peak = rdx_record_runtime.tx_count;
    }
    return 0;
}

static int rdx_mvp0_record_drain_queue(void)
{
    int ret;

    while (rdx_record_runtime.tx_count) {
        u8 head = rdx_record_runtime.tx_head;

        ret = rdx_mvp0_send(rdx_record_runtime.tx_packet[head],
                            rdx_record_runtime.tx_len[head]);
        if (ret) {
            rdx_record_runtime.send_fail_count++;
            return ret;
        }
        rdx_record_runtime.sent_count++;
        rdx_record_runtime.tx_head =
            (head + 1) % RDX_RECORD_TX_QUEUE_DEPTH;
        rdx_record_runtime.tx_count--;
    }
    return 0;
}

static int rdx_mvp0_record_frame(u8 *data, u32 len)
{
    u8 packet[RDX_RECORD_STREAM_PACKET_SIZE];
    u32 frame_index;
    int header_len;
    int ret;

    if (rdx_record_runtime.state != RDX_RECORD_STATE_STARTING
        && rdx_record_runtime.state != RDX_RECORD_STATE_ACTIVE) {
        return 0;
    }

    frame_index = ++rdx_record_runtime.frame_count;
    if (frame_index <= RDX_RECORD_WARMUP_FRAMES) {
        rdx_record_runtime.warmup_count++;
        if (frame_index == RDX_RECORD_WARMUP_FRAMES) {
            printf("[RDX][RECORD] warmup_complete frames=%u\n",
                   (unsigned int)frame_index);
        }
        return 0;
    }

    /* START state must reach the APP before its first stream packet. */
    if (rdx_record_runtime.state != RDX_RECORD_STATE_ACTIVE) {
        return 0;
    }
    if (!data || !len || len > RDX_RECORD_MAX_FRAME_SIZE) {
        rdx_record_runtime.invalid_count++;
        return 0;
    }
    if (len != RDX_RECORD_EXPECTED_FRAME_SIZE) {
        rdx_record_runtime.invalid_count++;
    }
    if (!rdx_record_runtime.opus_format_checked) {
        rdx_record_runtime.opus_channels = (data[0] & BIT(2)) ? 2 : 1;
        rdx_record_runtime.opus_format_checked = 1;
        printf("[RDX][RECORD] opus_toc=0x%02X ch=%u expected_ch=%u %s\n",
               data[0], rdx_record_runtime.opus_channels,
               AI_VOICE_FIXED_OPUS_CHANNELS,
               rdx_record_runtime.opus_channels == AI_VOICE_FIXED_OPUS_CHANNELS
               ? "ok" : "mismatch");
    }

    header_len = snprintf((char *)packet, sizeof(packet),
                          "*DEV#stream#%u#%u#",
                          (unsigned int)len, (unsigned int)len);
    if (header_len <= 0 || (u32)header_len + len > sizeof(packet)) {
        rdx_record_runtime.invalid_count++;
        return 0;
    }
    memcpy(packet + header_len, data, len);
    rdx_mvp0_record_drain_queue();
    if (rdx_record_runtime.tx_count) {
        rdx_mvp0_record_queue_packet(packet, header_len + len);
    } else {
        ret = rdx_mvp0_send(packet, header_len + len);
        if (ret) {
            rdx_record_runtime.send_fail_count++;
            rdx_mvp0_record_queue_packet(packet, header_len + len);
        } else {
            rdx_record_runtime.sent_count++;
        }
    }

    if ((frame_index - RDX_RECORD_WARMUP_FRAMES) % 250 == 0) {
        printf("[RDX][RECORD] frames=%u sent=%u send_fail=%u invalid=%u"
               " queued=%u pending=%u peak=%u drop=%u"
               " frame_len=%u packet_len=%u\n",
               (unsigned int)rdx_record_runtime.frame_count,
               (unsigned int)rdx_record_runtime.sent_count,
               (unsigned int)rdx_record_runtime.send_fail_count,
               (unsigned int)rdx_record_runtime.invalid_count,
               (unsigned int)rdx_record_runtime.queued_count,
               (unsigned int)rdx_record_runtime.tx_count,
               (unsigned int)rdx_record_runtime.tx_peak,
               (unsigned int)rdx_record_runtime.queue_drop_count,
               (unsigned int)len, (unsigned int)(header_len + len));
    }
    return 0;
}

static void rdx_mvp0_record_close(const char *reason, u8 notify_app)
{
    u8 was_running = rdx_record_runtime.state != RDX_RECORD_STATE_IDLE;

    if (was_running) {
        rdx_record_runtime.state = RDX_RECORD_STATE_STOPPING;
        ai_voice_recoder_close();
        rdx_ble_transport_set_record_streaming(0);
        rdx_record_runtime.state = RDX_RECORD_STATE_IDLE;
        printf("[RDX][RECORD] stopped reason=%s frames=%u warmup=%u"
               " sent=%u send_fail=%u invalid=%u queued=%u"
               " pending=%u peak=%u drop=%u opus_ch=%u\n",
               reason,
               (unsigned int)rdx_record_runtime.frame_count,
               (unsigned int)rdx_record_runtime.warmup_count,
               (unsigned int)rdx_record_runtime.sent_count,
               (unsigned int)rdx_record_runtime.send_fail_count,
               (unsigned int)rdx_record_runtime.invalid_count,
               (unsigned int)rdx_record_runtime.queued_count,
               (unsigned int)rdx_record_runtime.tx_count,
               (unsigned int)rdx_record_runtime.tx_peak,
               (unsigned int)rdx_record_runtime.queue_drop_count,
               (unsigned int)rdx_record_runtime.opus_channels);
        rdx_record_runtime.tx_count = 0;
    }
    if (notify_app) {
        rdx_mvp0_record_send_state(0);
    }
}

static void rdx_mvp0_record_start(void)
{
    int ret;

    if (rdx_record_runtime.state == RDX_RECORD_STATE_ACTIVE) {
        rdx_ble_transport_set_record_streaming(1);
        rdx_mvp0_record_send_state(1);
        return;
    }
    if (rdx_record_runtime.state != RDX_RECORD_STATE_IDLE) {
        printf("[RDX][RECORD] start_rejected reason=transition state=%u\n",
               rdx_record_runtime.state);
        rdx_mvp0_record_send_state(0);
        return;
    }
    if (rdx_mvp0_record_usb_audio_busy()) {
        printf("[RDX][RECORD] start_rejected reason=usb_audio_busy\n");
        rdx_mvp0_record_send_state(0);
        return;
    }

    memset(&rdx_record_runtime, 0, sizeof(rdx_record_runtime));
    rdx_record_runtime.state = RDX_RECORD_STATE_STARTING;
    rdx_ble_transport_set_record_streaming(1);
    ret = ai_voice_recoder_open_with_tx(AUDIO_CODING_OPUS, 0,
                                         rdx_mvp0_record_frame);
    if (ret) {
        rdx_ble_transport_set_record_streaming(0);
        rdx_record_runtime.state = RDX_RECORD_STATE_IDLE;
        printf("[RDX][RECORD] start_failed err=%d\n", ret);
        rdx_mvp0_record_send_state(0);
        return;
    }

    ret = rdx_mvp0_record_send_state(1);
    if (ret) {
        printf("[RDX][RECORD] start_failed reason=ack_send ret=%d\n", ret);
        rdx_mvp0_record_close("start_ack_failed", 0);
        return;
    }
    rdx_record_runtime.state = RDX_RECORD_STATE_ACTIVE;
    printf("[RDX][RECORD] started scene=meeting sr=%u ch=%u"
           " bitrate=%u frame_ms=%u format=raw\n",
           AI_VOICE_FIXED_OPUS_SAMPLE_RATE,
           AI_VOICE_FIXED_OPUS_CHANNELS,
           AI_VOICE_FIXED_OPUS_BIT_RATE,
           AI_VOICE_FIXED_OPUS_FRAME_MS);
}

static void rdx_mvp0_handle_record(const u8 *data, u16 len)
{
    static const u8 stop[] = "*APP#record#0#";
    static const u8 stop_meeting_16k[] = "*APP#record#0#0#1#";
    static const u8 start[] = "*APP#record#1#";
    static const u8 start_meeting[] = "*APP#record#1#0#";
    static const u8 start_meeting_16k[] = "*APP#record#1#0#1#";

    if (!rdx_mvp0_management_query_ready("record", len)) {
        return;
    }
    if (rdx_mvp0_data_equals(data, len, stop, sizeof(stop) - 1)
        || rdx_mvp0_data_equals(data, len, stop_meeting_16k,
                                sizeof(stop_meeting_16k) - 1)) {
        rdx_mvp0_record_close("app_stop", 1);
        return;
    }
    if (rdx_mvp0_data_equals(data, len, start, sizeof(start) - 1)
        || rdx_mvp0_data_equals(data, len, start_meeting,
                                sizeof(start_meeting) - 1)
        || rdx_mvp0_data_equals(data, len, start_meeting_16k,
                                sizeof(start_meeting_16k) - 1)) {
        rdx_mvp0_record_start();
        return;
    }

    printf("[RDX][DROP] cmd=record reason=unsupported_or_malformed"
           " wire=%.*s len=%u\n", (int)len, (char *)data, len);
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

static const char *rdx_mvp0_post_ack_action_name(u8 action)
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

static void rdx_mvp0_cancel_post_ack_action(const char *reason)
{
    if (!rdx_mvp0_state.state_transition_pending
        && !rdx_mvp0_state.post_ack_timer_id) {
        return;
    }

    if (rdx_mvp0_state.post_ack_timer_id) {
        sys_timeout_del(rdx_mvp0_state.post_ack_timer_id);
    }
    printf("[RDX][SESSION] post_ack action=%s event=cancel reason=%s timer_id=%u\n",
           rdx_mvp0_post_ack_action_name(rdx_mvp0_state.post_ack_action),
           reason, rdx_mvp0_state.post_ack_timer_id);
    rdx_mvp0_state.state_transition_pending = 0;
    rdx_mvp0_state.post_ack_action = RDX_POST_ACK_ACTION_NONE;
    rdx_mvp0_state.post_ack_timer_id = 0;
}

static void rdx_mvp0_post_ack_timeout(void *priv)
{
    u8 action = rdx_mvp0_state.post_ack_action;

    (void)priv;
    rdx_mvp0_state.state_transition_pending = 0;
    rdx_mvp0_state.post_ack_action = RDX_POST_ACK_ACTION_NONE;
    rdx_mvp0_state.post_ack_timer_id = 0;
    if (!rdx_mvp0_state.connected || !rdx_mvp0_disconnect_callback) {
        printf("[RDX][SESSION] post_ack action=%s event=skip reason=session_inactive\n",
               rdx_mvp0_post_ack_action_name(action));
        return;
    }

    printf("[RDX][SESSION] post_ack action=%s event=execute disconnect=rdx_ble\n",
           rdx_mvp0_post_ack_action_name(action));
    rdx_mvp0_disconnect_callback();
}

static void rdx_mvp0_schedule_post_ack_disconnect(u8 action, int ack_ret)
{
    u16 timer_id;

    if (!rdx_mvp0_state.connected || !rdx_mvp0_disconnect_callback) {
        printf("[RDX][SESSION] post_ack action=%s event=skip reason=session_inactive ack_ret=%d\n",
               rdx_mvp0_post_ack_action_name(action), ack_ret);
        return;
    }

    rdx_mvp0_state.state_transition_pending = 1;
    rdx_mvp0_state.post_ack_action = action;
    timer_id = sys_timeout_add(NULL, rdx_mvp0_post_ack_timeout,
                               RDX_POST_ACK_DISCONNECT_DELAY_MS);
    rdx_mvp0_state.post_ack_timer_id = timer_id;
    printf("[RDX][SESSION] post_ack action=%s event=schedule delay_ms=%u timer_id=%u ack_ret=%d\n",
           rdx_mvp0_post_ack_action_name(action),
           RDX_POST_ACK_DISCONNECT_DELAY_MS, timer_id, ack_ret);
    if (timer_id) {
        return;
    }

    printf("[RDX][SESSION] post_ack action=%s event=fallback reason=timer_alloc_failed\n",
           rdx_mvp0_post_ack_action_name(action));
    rdx_mvp0_post_ack_timeout(NULL);
}

static int rdx_mvp0_parse_nonnegative_s32(const u8 *data, u16 len,
                                          s32 *value)
{
    u32 parsed = 0;
    u16 i;

    if (!data || !len || !value) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        u8 digit;

        if (data[i] < '0' || data[i] > '9') {
            return -1;
        }
        digit = data[i] - '0';
        if (parsed > (0x7fffffffUL - digit) / 10) {
            return -1;
        }
        parsed = parsed * 10 + digit;
    }

    *value = (s32)parsed;
    return 0;
}

static int rdx_mvp0_send_bound_result(u8 result)
{
    u8 response[32];
    u8 bound = rdx_device_management_get_bound();
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_BOUND "%u#%u#", result, bound);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=bound reason=encode_failed len=%d\n", len);
        return -1;
    }

    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=bound wire=%s result=%u bound=%u len=%d ret=%d\n",
           (char *)response, result, bound, len, ret);
    return ret;
}

static int rdx_mvp0_send_unbound_result(u8 result)
{
    u8 response[32];
    u8 bound = rdx_device_management_get_bound();
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_UNBOUND "%u#%u#", result, bound);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=unbound reason=encode_failed len=%d\n", len);
        return -1;
    }

    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=unbound wire=%s result=%u bound=%u len=%d ret=%d\n",
           (char *)response, result, bound, len, ret);
    return ret;
}

static int rdx_mvp0_send_set_default_result(u8 result)
{
    u8 response[24];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_SET_DEFAULT "%u#", result);
    if (len <= 0 || len >= sizeof(response)) {
        printf("[RDX][TX] cmd=default reason=encode_failed len=%d\n", len);
        return -1;
    }

    ret = rdx_mvp0_send(response, len);
    printf("[RDX][TX] cmd=default wire=%s result=%u len=%d ret=%d\n",
           (char *)response, result, len, ret);
    return ret;
}

static void rdx_mvp0_handle_bound(const u8 *data, u16 len)
{
    const u16 prefix_len = sizeof(rdx_cmd_bound) - 1;
    const u16 request_len = prefix_len + 2;
    int ret;
    int ack_ret;

    if (len != request_len || data[len - 1] != '#'
        || (data[prefix_len] != '0' && data[prefix_len] != '1')) {
        printf("[RDX][DROP] cmd=bound reason=malformed len=%u\n", len);
        return;
    }

    if (!rdx_mvp0_management_query_ready("bound", len)) {
        return;
    }

    if (rdx_mvp0_state.state_transition_pending) {
        printf("[RDX][DROP] cmd=bound reason=transition_pending action=%s len=%u\n",
               rdx_mvp0_post_ack_action_name(rdx_mvp0_state.post_ack_action),
               len);
        rdx_mvp0_send_bound_result(1);
        return;
    }

    if (data[prefix_len] == '1') {
        ret = rdx_device_management_bind();
        rdx_mvp0_send_bound_result(ret ? 1 : 0);
        return;
    }

    ret = rdx_device_management_unbind();
    ack_ret = rdx_mvp0_send_bound_result(ret ? 1 : 0);
    if (!ret) {
        rdx_mvp0_schedule_post_ack_disconnect(RDX_POST_ACK_ACTION_UNBOUND,
                                              ack_ret);
    }
}

static void rdx_mvp0_handle_unbound(const u8 *data, u16 len)
{
    const u16 prefix_len = sizeof(rdx_cmd_unbound) - 1;
    u16 separator;
    s32 user_para;
    u8 format_en;
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
        || rdx_mvp0_parse_nonnegative_s32(data + prefix_len,
                                          separator - prefix_len,
                                          &user_para)) {
        printf("[RDX][DROP] cmd=unbound reason=malformed len=%u\n", len);
        return;
    }
    format_en = data[separator + 1] - '0';

    if (!rdx_mvp0_management_query_ready("unbound", len)) {
        return;
    }
    if (rdx_mvp0_state.state_transition_pending) {
        printf("[RDX][DROP] cmd=unbound reason=transition_pending action=%s len=%u\n",
               rdx_mvp0_post_ack_action_name(rdx_mvp0_state.post_ack_action),
               len);
        rdx_mvp0_send_unbound_result(1);
        return;
    }

    printf("[RDX][SESSION] cmd=unbound user_para=%d format_en=%u storage_format=ignored\n",
           (int)user_para, format_en);
    ret = rdx_device_management_unbind();
    ack_ret = rdx_mvp0_send_unbound_result(ret ? 1 : 0);
    if (!ret) {
        rdx_mvp0_schedule_post_ack_disconnect(RDX_POST_ACK_ACTION_UNBOUND,
                                              ack_ret);
    }
}

static void rdx_mvp0_handle_set_default(const u8 *data, u16 len)
{
    int ret;
    int ack_ret;

    if (!rdx_mvp0_data_equals(data, len, rdx_cmd_set_default,
                              sizeof(rdx_cmd_set_default) - 1)) {
        printf("[RDX][DROP] cmd=default reason=malformed len=%u\n", len);
        return;
    }
    if (!rdx_mvp0_management_query_ready("default", len)) {
        return;
    }
    if (rdx_mvp0_state.state_transition_pending) {
        printf("[RDX][DROP] cmd=default reason=transition_pending action=%s len=%u\n",
               rdx_mvp0_post_ack_action_name(rdx_mvp0_state.post_ack_action),
               len);
        rdx_mvp0_send_set_default_result(1);
        return;
    }

    ret = rdx_device_management_restore_defaults();
    ack_ret = rdx_mvp0_send_set_default_result(ret ? 1 : 0);
    if (!ret) {
        rdx_mvp0_schedule_post_ack_disconnect(
            RDX_POST_ACK_ACTION_SET_DEFAULT, ack_ret);
    }
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
    u8 response[40];
    u16 value_len;
    u16 i;
    u32 timestamp = 0;
    u8 result = 1;
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
    ret = rdx_mvp0_send(response, response_len);
    printf("[RDX][TX] cmd=rtc wire=%s len=%u ret=%d\n",
           (char *)response, (unsigned int)response_len, ret);
}

void rdx_mvp0_protocol_init(rdx_mvp0_send_callback_t send_callback,
                            rdx_mvp0_disconnect_callback_t disconnect_callback)
{
    rdx_mvp0_send_callback = send_callback;
    rdx_mvp0_disconnect_callback = disconnect_callback;
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
    memset(&rdx_record_runtime, 0, sizeof(rdx_record_runtime));
}

void rdx_mvp0_protocol_exit(void)
{
    rdx_mvp0_record_close("protocol_exit", 0);
    rdx_mvp0_cancel_post_ack_action("protocol_exit");
    rdx_mvp0_send_callback = NULL;
    rdx_mvp0_disconnect_callback = NULL;
    memset(&rdx_mvp0_state, 0, sizeof(rdx_mvp0_state));
}

void rdx_mvp0_protocol_set_connected(u8 connected)
{
    if (!connected) {
        rdx_mvp0_record_close("link_disconnected", 0);
    }
    rdx_mvp0_cancel_post_ack_action(connected ? "new_connection"
                                              : "link_disconnected");
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
    if (!enabled) {
        rdx_mvp0_record_close("ccc_disabled", 0);
    }
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
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_record,
                                         sizeof(rdx_cmd_record) - 1)) {
        rdx_mvp0_log_rx("record", data, len);
        rdx_mvp0_handle_record(data, len);
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
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_unbound,
                                         sizeof(rdx_cmd_unbound) - 1)) {
        rdx_mvp0_log_rx("unbound", data, len);
        rdx_mvp0_handle_unbound(data, len);
    } else if (rdx_mvp0_data_starts_with(data, len, rdx_cmd_set_default,
                                         sizeof(rdx_cmd_set_default) - 1)) {
        rdx_mvp0_log_rx("default", data, len);
        rdx_mvp0_handle_set_default(data, len);
    } else {
        printf("[RDX][DROP] cmd=unknown wire=%.*s len=%u type=%02X%02X\n",
               (int)len, (char *)data, len,
               data[0], len > 1 ? data[1] : 0);
    }
}

#endif
