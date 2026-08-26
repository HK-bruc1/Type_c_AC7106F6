#ifndef RDX_PROTOCOL_SESSION_H
#define RDX_PROTOCOL_SESSION_H

#include <stdint.h>

#include "rdx/ports/rdx_transport_port.h"

struct rdx_protocol_session {
    uint32_t connection_generation;
    uint8_t connected;
    uint8_t identity_read;
    uint8_t ccc_ready;
    uint8_t authenticated;
};

enum rdx_protocol_session_event_result {
    RDX_PROTOCOL_SESSION_EVENT_IGNORED = 0,
    RDX_PROTOCOL_SESSION_EVENT_CONNECTED,
    RDX_PROTOCOL_SESSION_EVENT_LINK_LOST,
    RDX_PROTOCOL_SESSION_EVENT_IDENTITY_READ,
    RDX_PROTOCOL_SESSION_EVENT_CCC_CHANGED,
    RDX_PROTOCOL_SESSION_EVENT_RX,
    RDX_PROTOCOL_SESSION_EVENT_CAN_SEND_NOW,
};

void rdx_protocol_session_reset(struct rdx_protocol_session *session);
enum rdx_protocol_session_event_result rdx_protocol_session_handle_event(
    struct rdx_protocol_session *session,
    const struct rdx_transport_event *event);
int rdx_protocol_session_mark_authenticated(
    struct rdx_protocol_session *session);
uint8_t rdx_protocol_session_can_send(
    const struct rdx_protocol_session *session);
uint8_t rdx_protocol_session_is_ready(
    const struct rdx_protocol_session *session);

#endif
