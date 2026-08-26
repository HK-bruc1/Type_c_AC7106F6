#include <stddef.h>
#include <string.h>

#include "rdx_protocol/protocol/v23/rdx_protocol_session.h"

void rdx_protocol_session_reset(struct rdx_protocol_session *session)
{
    if (session) {
        memset(session, 0, sizeof(*session));
    }
}

enum rdx_protocol_session_event_result rdx_protocol_session_handle_event(
    struct rdx_protocol_session *session,
    const struct rdx_transport_event *event)
{
    if (!session || !event || !event->connection_generation) {
        return RDX_PROTOCOL_SESSION_EVENT_IGNORED;
    }

    if (event->type == RDX_TRANSPORT_EVENT_CONNECTED) {
        rdx_protocol_session_reset(session);
        session->connected = 1;
        session->connection_generation = event->connection_generation;
        return RDX_PROTOCOL_SESSION_EVENT_CONNECTED;
    }

    if (!session->connected
        || event->connection_generation != session->connection_generation) {
        return RDX_PROTOCOL_SESSION_EVENT_IGNORED;
    }

    switch (event->type) {
    case RDX_TRANSPORT_EVENT_LINK_LOST:
        rdx_protocol_session_reset(session);
        return RDX_PROTOCOL_SESSION_EVENT_LINK_LOST;
    case RDX_TRANSPORT_EVENT_IDENTITY_READ:
        if (session->identity_read) {
            return RDX_PROTOCOL_SESSION_EVENT_IGNORED;
        }
        session->identity_read = 1;
        return RDX_PROTOCOL_SESSION_EVENT_IDENTITY_READ;
    case RDX_TRANSPORT_EVENT_CCC_CHANGED:
        session->ccc_ready = !!event->data.ccc_enabled;
        if (!session->ccc_ready) {
            session->authenticated = 0;
        }
        return RDX_PROTOCOL_SESSION_EVENT_CCC_CHANGED;
    case RDX_TRANSPORT_EVENT_RX:
        return RDX_PROTOCOL_SESSION_EVENT_RX;
    case RDX_TRANSPORT_EVENT_CAN_SEND_NOW:
        return RDX_PROTOCOL_SESSION_EVENT_CAN_SEND_NOW;
    default:
        return RDX_PROTOCOL_SESSION_EVENT_IGNORED;
    }
}

int rdx_protocol_session_mark_authenticated(
    struct rdx_protocol_session *session)
{
    if (!session || !session->connected || !session->ccc_ready) {
        return -1;
    }

    session->authenticated = 1;
    return 0;
}

uint8_t rdx_protocol_session_can_send(
    const struct rdx_protocol_session *session)
{
    return session && session->connected && session->ccc_ready;
}

uint8_t rdx_protocol_session_is_ready(
    const struct rdx_protocol_session *session)
{
    return rdx_protocol_session_can_send(session)
        && session->authenticated;
}
