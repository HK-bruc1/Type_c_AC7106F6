#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rdx_protocol/protocol/v23/rdx_protocol_session.h"
#include "rdx_transport_fake.h"

struct protocol_observer {
    struct rdx_protocol_session session;
    enum rdx_protocol_session_event_result last_result;
    uint32_t can_send_session_id;
    uint32_t can_send_capture_generation;
    const uint8_t *rx_data;
    uint16_t rx_len;
    unsigned int event_count;
};

static void protocol_transport_event(
    const struct rdx_transport_event *event, void *context)
{
    struct protocol_observer *observer = context;

    observer->last_result = rdx_protocol_session_handle_event(
        &observer->session, event);
    observer->event_count++;
    if (observer->last_result
        == RDX_PROTOCOL_SESSION_EVENT_CAN_SEND_NOW) {
        observer->can_send_session_id = event->data.can_send.session_id;
        observer->can_send_capture_generation =
            event->data.can_send.capture_generation;
    } else if (observer->last_result == RDX_PROTOCOL_SESSION_EVENT_RX) {
        observer->rx_data = event->data.rx.data;
        observer->rx_len = event->data.rx.len;
    }
}

static void test_transport_session_sequence(void)
{
    static const uint8_t packet[] = { 0x52, 0x44, 0x58 };
    const struct rdx_transport_fake_state *fake;
    struct protocol_observer observer;
    struct rdx_transport_event event;
    uint32_t first_generation;
    uint32_t second_generation;

    memset(&observer, 0, sizeof(observer));
    rdx_protocol_session_reset(&observer.session);
    assert(rdx_transport_init(protocol_transport_event, &observer) == 0);
    assert(rdx_protocol_session_mark_authenticated(&observer.session) != 0);

    memset(&event, 0, sizeof(event));
    event.type = RDX_TRANSPORT_EVENT_CONNECTED;
    assert(rdx_transport_fake_emit_event(&event) == 0);
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_IGNORED);

    first_generation = rdx_transport_fake_connect();
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_CONNECTED);
    assert(observer.session.connected);
    assert(observer.session.connection_generation == first_generation);
    assert(!rdx_protocol_session_can_send(&observer.session));
    assert(rdx_transport_send(packet, sizeof(packet))
           == RDX_TRANSPORT_UNAVAILABLE);
    assert(rdx_transport_send(NULL, sizeof(packet))
           == RDX_TRANSPORT_FATAL);

    rdx_transport_fake_emit_simple(RDX_TRANSPORT_EVENT_IDENTITY_READ,
                                   first_generation);
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_IDENTITY_READ);
    assert(observer.session.identity_read);

    rdx_transport_fake_set_ccc(first_generation, 1);
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_CCC_CHANGED);
    assert(rdx_protocol_session_can_send(&observer.session));
    rdx_transport_fake_receive(first_generation, packet, sizeof(packet));
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_RX);
    assert(observer.rx_data == packet);
    assert(observer.rx_len == sizeof(packet));
    assert(!rdx_protocol_session_is_ready(&observer.session));
    assert(rdx_protocol_session_mark_authenticated(&observer.session) == 0);
    assert(rdx_protocol_session_is_ready(&observer.session));

    rdx_transport_fake_set_send_result(RDX_TRANSPORT_RETRYABLE);
    assert(rdx_transport_send(packet, sizeof(packet))
           == RDX_TRANSPORT_RETRYABLE);
    assert(rdx_transport_set_record_streaming(1) == 0);
    assert(rdx_transport_request_can_send_now(7, 11) == 0);
    assert(observer.last_result
           == RDX_PROTOCOL_SESSION_EVENT_CAN_SEND_NOW);
    assert(observer.can_send_session_id == 7);
    assert(observer.can_send_capture_generation == 11);
    rdx_transport_fake_set_send_result(RDX_TRANSPORT_ACCEPTED);
    assert(rdx_transport_send(packet, sizeof(packet))
           == RDX_TRANSPORT_ACCEPTED);
    fake = rdx_transport_fake_get_state();
    assert(fake->accepted_count == 1);

    second_generation = rdx_transport_fake_connect();
    assert(second_generation != first_generation);
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_CONNECTED);
    assert(!observer.session.identity_read);
    assert(!observer.session.ccc_ready);
    assert(!observer.session.authenticated);

    rdx_transport_fake_set_ccc(first_generation, 1);
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_IGNORED);
    assert(!observer.session.ccc_ready);
    rdx_transport_fake_receive(first_generation, packet, sizeof(packet));
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_IGNORED);
    rdx_transport_fake_emit_simple(RDX_TRANSPORT_EVENT_LINK_LOST,
                                   first_generation);
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_IGNORED);
    assert(observer.session.connected);

    rdx_transport_fake_set_ccc(second_generation, 1);
    assert(rdx_protocol_session_mark_authenticated(&observer.session) == 0);
    rdx_transport_fake_set_ccc(second_generation, 0);
    assert(!observer.session.ccc_ready);
    assert(!observer.session.authenticated);
    assert(!rdx_protocol_session_is_ready(&observer.session));

    rdx_transport_disconnect();
    assert(observer.last_result == RDX_PROTOCOL_SESSION_EVENT_LINK_LOST);
    assert(!observer.session.connected);
    assert(!observer.session.connection_generation);
    assert(rdx_protocol_session_mark_authenticated(&observer.session) != 0);

    rdx_transport_exit();
}

int main(void)
{
    test_transport_session_sequence();
    puts("RDX transport contract tests passed");
    return 0;
}
