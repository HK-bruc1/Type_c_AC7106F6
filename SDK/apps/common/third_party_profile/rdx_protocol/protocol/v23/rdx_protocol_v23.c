#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_session.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23_internal.h"

/* Published diagnostic Wire literals; the internal module name is V23. */
static const u8 rdx_v23_diagnostic_ping[] = "RDX_MVP0_PING";
static const u8 rdx_v23_diagnostic_pong[] = "RDX_MVP0_PONG";

static struct rdx_protocol_v23_context rdx_v23_context;

int rdx_protocol_v23_send(struct rdx_protocol_v23_context *context,
                          const u8 *data, u16 len)
{
    if (!context || !context->initialized
        || !rdx_protocol_session_can_send(&context->session)) {
        return RDX_TRANSPORT_UNAVAILABLE;
    }
    return rdx_transport_send(data, len);
}

void rdx_protocol_v23_log_rx(struct rdx_protocol_v23_context *context,
                             const char *command,
                             const u8 *data, u16 len)
{
    printf("[RDX][RX] cmd=%s wire=%.*s len=%u app_ready=%u\n",
           command, (int)len, (char *)data, len,
           context ? context->session.authenticated : 0);
}

u8 rdx_protocol_v23_data_equals(const u8 *data, u16 len,
                                const u8 *expected, u16 expected_len)
{
    return len == expected_len && !memcmp(data, expected, expected_len);
}

u8 rdx_protocol_v23_data_starts_with(const u8 *data, u16 len,
                                     const u8 *prefix, u16 prefix_len)
{
    return len >= prefix_len && !memcmp(data, prefix, prefix_len);
}

u8 rdx_protocol_v23_query_ready(
    const struct rdx_protocol_v23_context *context,
    const char *command, u16 len)
{
    if (context && rdx_protocol_session_is_ready(&context->session)) {
        return 1;
    }
    printf("[RDX][DROP] cmd=%s reason=app_not_ready len=%u\n",
           command, len);
    return 0;
}

int rdx_protocol_v23_init(void)
{
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    int ret;
#endif

    if (rdx_v23_context.initialized) {
        return 0;
    }

    memset(&rdx_v23_context, 0, sizeof(rdx_v23_context));
    rdx_protocol_v23_management_init(&rdx_v23_context);
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    ret = rdx_protocol_v23_record_init(&rdx_v23_context);
    if (ret) {
        rdx_protocol_v23_management_exit();
        memset(&rdx_v23_context, 0, sizeof(rdx_v23_context));
        return ret;
    }
#endif
    rdx_v23_context.initialized = 1;
    return 0;
}

void rdx_protocol_v23_exit(void)
{
    if (!rdx_v23_context.initialized) {
        return;
    }
    rdx_v23_context.initialized = 0;
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    rdx_protocol_v23_record_exit();
#endif
    rdx_protocol_v23_management_exit();
    memset(&rdx_v23_context, 0, sizeof(rdx_v23_context));
}

static void rdx_protocol_v23_handle_link_change(u8 connected)
{
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    rdx_protocol_v23_record_handle_link_change(connected);
#endif
    rdx_protocol_v23_management_handle_link_change(connected);
    printf("[RDX][SESSION] link=%s app_ready=0\n",
           connected ? "connected" : "disconnected");
}

static void rdx_protocol_v23_handle_ccc_change(void)
{
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    rdx_protocol_v23_record_handle_ccc_change();
#endif
    printf("[RDX][SESSION] ccc=%u app_ready=%u\n",
           rdx_v23_context.session.ccc_ready,
           rdx_v23_context.session.authenticated);
}

static void rdx_protocol_v23_receive(const u8 *data, u16 len)
{
    if (!data || !len) {
        return;
    }

    if (rdx_protocol_v23_data_equals(
            data, len, rdx_v23_diagnostic_ping,
            sizeof(rdx_v23_diagnostic_ping) - 1)) {
        int ret;

        rdx_protocol_v23_log_rx(&rdx_v23_context, "ping", data, len);
        ret = rdx_protocol_v23_send(
            &rdx_v23_context, rdx_v23_diagnostic_pong,
            sizeof(rdx_v23_diagnostic_pong) - 1);
        printf("[RDX][TX] cmd=pong wire=%s len=%u ret=%d\n",
               (char *)rdx_v23_diagnostic_pong,
               (unsigned int)(sizeof(rdx_v23_diagnostic_pong) - 1), ret);
    } else if (rdx_protocol_v23_management_handle_command(data, len)) {
        return;
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    } else if (rdx_protocol_v23_record_handle_command(data, len)) {
        return;
#endif
    } else {
        printf("[RDX][DROP] cmd=unknown wire=%.*s len=%u type=%02X%02X\n",
               (int)len, (char *)data, len,
               data[0], len > 1 ? data[1] : 0);
    }
}

void rdx_protocol_v23_handle_transport_event(
    const struct rdx_transport_event *event, void *context)
{
    enum rdx_protocol_session_event_result result;

    (void)context;
    if (!rdx_v23_context.initialized) {
        return;
    }

    result = rdx_protocol_session_handle_event(
        &rdx_v23_context.session, event);
    switch (result) {
    case RDX_PROTOCOL_SESSION_EVENT_CONNECTED:
        rdx_protocol_v23_handle_link_change(1);
        break;
    case RDX_PROTOCOL_SESSION_EVENT_LINK_LOST:
        rdx_protocol_v23_handle_link_change(0);
        break;
    case RDX_PROTOCOL_SESSION_EVENT_IDENTITY_READ:
        printf("[RDX][GATT] identity=read\n");
        break;
    case RDX_PROTOCOL_SESSION_EVENT_CCC_CHANGED:
        rdx_protocol_v23_handle_ccc_change();
        break;
    case RDX_PROTOCOL_SESSION_EVENT_RX:
        rdx_protocol_v23_receive(event->data.rx.data,
                                 event->data.rx.len);
        break;
    case RDX_PROTOCOL_SESSION_EVENT_CAN_SEND_NOW:
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE
        rdx_protocol_v23_record_handle_can_send(
            event->data.can_send.session_id,
            event->data.can_send.capture_generation);
#endif
        break;
    default:
        break;
    }
}

#endif /* TCFG_RDX_ENABLE */
