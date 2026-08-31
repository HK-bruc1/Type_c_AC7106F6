#include <string.h>

#include "rdx_transport_fake.h"

static struct rdx_transport_fake_state fake_transport;

const struct rdx_transport_fake_state *rdx_transport_fake_get_state(void)
{
    return &fake_transport;
}

void rdx_transport_fake_set_send_result(enum rdx_transport_result result)
{
    fake_transport.send_result = result;
}

int rdx_transport_fake_emit_event(const struct rdx_transport_event *event)
{
    if (!fake_transport.initialized || !fake_transport.callback || !event) {
        return -1;
    }
    fake_transport.callback(event, fake_transport.context);
    return 0;
}

uint32_t rdx_transport_fake_connect(void)
{
    struct rdx_transport_event event;

    if (!fake_transport.initialized) {
        return 0;
    }
    fake_transport.generation++;
    if (!fake_transport.generation) {
        fake_transport.generation = 1;
    }
    fake_transport.connected = 1;
    fake_transport.ccc_enabled = 0;
    memset(&event, 0, sizeof(event));
    event.type = RDX_TRANSPORT_EVENT_CONNECTED;
    event.connection_generation = fake_transport.generation;
    rdx_transport_fake_emit_event(&event);
    return fake_transport.generation;
}

void rdx_transport_fake_set_ccc(uint32_t generation, uint8_t enabled)
{
    struct rdx_transport_event event;

    if (generation == fake_transport.generation) {
        fake_transport.ccc_enabled = !!enabled;
    }
    memset(&event, 0, sizeof(event));
    event.type = RDX_TRANSPORT_EVENT_CCC_CHANGED;
    event.connection_generation = generation;
    event.data.ccc_enabled = enabled;
    rdx_transport_fake_emit_event(&event);
}

void rdx_transport_fake_receive(uint32_t generation,
                                const uint8_t *data, uint16_t len)
{
    struct rdx_transport_event event;

    memset(&event, 0, sizeof(event));
    event.type = RDX_TRANSPORT_EVENT_RX;
    event.connection_generation = generation;
    event.data.rx.data = data;
    event.data.rx.len = len;
    rdx_transport_fake_emit_event(&event);
}

void rdx_transport_fake_emit_simple(
    enum rdx_transport_event_type type, uint32_t generation)
{
    struct rdx_transport_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.connection_generation = generation;
    rdx_transport_fake_emit_event(&event);
}

void rdx_transport_fake_emit_can_send(uint32_t generation,
                                      uint32_t owner,
                                      uint32_t token)
{
    struct rdx_transport_event event;

    memset(&event, 0, sizeof(event));
    event.type = RDX_TRANSPORT_EVENT_CAN_SEND_NOW;
    event.connection_generation = generation;
    event.data.can_send.session_id = owner;
    event.data.can_send.capture_generation = token;
    fake_transport.can_send_pending = 0;
    rdx_transport_fake_emit_event(&event);
}

int rdx_transport_init(rdx_transport_event_callback_t event_callback,
                       void *context)
{
    if (!event_callback) {
        return -1;
    }
    memset(&fake_transport, 0, sizeof(fake_transport));
    fake_transport.callback = event_callback;
    fake_transport.context = context;
    fake_transport.send_result = RDX_TRANSPORT_ACCEPTED;
    fake_transport.initialized = 1;
    return 0;
}

void rdx_transport_exit(void)
{
    memset(&fake_transport, 0, sizeof(fake_transport));
}

enum rdx_transport_result rdx_transport_send(
    const uint8_t *data, uint16_t len)
{
    if (!data || !len || len > sizeof(fake_transport.accepted_data)) {
        return RDX_TRANSPORT_FATAL;
    }
    if (!fake_transport.initialized || !fake_transport.connected
        || !fake_transport.ccc_enabled) {
        return RDX_TRANSPORT_UNAVAILABLE;
    }

    fake_transport.send_attempt_count++;
    if (fake_transport.send_result != RDX_TRANSPORT_ACCEPTED) {
        return fake_transport.send_result;
    }

    memcpy(fake_transport.accepted_data, data, len);
    fake_transport.accepted_len = len;
    fake_transport.accepted_count++;
    return RDX_TRANSPORT_ACCEPTED;
}

int rdx_transport_request_can_send_now(
    uint32_t session_id, uint32_t capture_generation)
{
    if (!fake_transport.initialized || !fake_transport.connected
        || !fake_transport.ccc_enabled
        || !session_id || !capture_generation) {
        return -1;
    }
    fake_transport.can_send_request_count++;
    fake_transport.can_send_pending = 1;
    fake_transport.can_send_owner = session_id;
    fake_transport.can_send_token = capture_generation;
    return 0;
}

int rdx_transport_set_record_streaming(uint8_t enabled)
{
    if (!fake_transport.initialized) {
        return -1;
    }
    fake_transport.record_streaming = !!enabled;
    return 0;
}

void rdx_transport_tx_diagnostics_reset(void)
{
}

void rdx_transport_tx_diagnostics_log(void)
{
}

int rdx_transport_refresh_local_name(void)
{
    return fake_transport.initialized ? 0 : -1;
}

void rdx_transport_disconnect(void)
{
    struct rdx_transport_event event;

    fake_transport.disconnect_count++;
    if (!fake_transport.initialized || !fake_transport.connected) {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.type = RDX_TRANSPORT_EVENT_LINK_LOST;
    event.connection_generation = fake_transport.generation;
    fake_transport.connected = 0;
    fake_transport.ccc_enabled = 0;
    rdx_transport_fake_emit_event(&event);
}
