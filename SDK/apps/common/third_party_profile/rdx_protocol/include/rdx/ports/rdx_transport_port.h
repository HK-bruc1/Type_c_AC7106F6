#ifndef RDX_TRANSPORT_PORT_H
#define RDX_TRANSPORT_PORT_H

#include <stdint.h>

/*
 * ACCEPTED means that the local transport queue accepted the borrowed buffer.
 * The caller may reuse the buffer after rdx_transport_send() returns.
 */
enum rdx_transport_result {
    RDX_TRANSPORT_ACCEPTED = 0,
    RDX_TRANSPORT_RETRYABLE,
    RDX_TRANSPORT_FATAL,
    RDX_TRANSPORT_UNAVAILABLE,
};

/* Startup failures are kept distinct from send-queue results. */
#define RDX_TRANSPORT_ERR_CONTROLLER_MAC       (-301)
#define RDX_TRANSPORT_ERR_HANDLE_ALLOC         (-302)
#define RDX_TRANSPORT_ERR_HANDLE_MAC           (-303)
#define RDX_TRANSPORT_ERR_ADVERTISING          (-304)

enum rdx_transport_event_type {
    RDX_TRANSPORT_EVENT_CONNECTED = 0,
    RDX_TRANSPORT_EVENT_LINK_LOST,
    RDX_TRANSPORT_EVENT_IDENTITY_READ,
    RDX_TRANSPORT_EVENT_CCC_CHANGED,
    RDX_TRANSPORT_EVENT_RX,
    RDX_TRANSPORT_EVENT_CAN_SEND_NOW,
};

struct rdx_transport_rx_event {
    const uint8_t *data;
    uint16_t len;
};

struct rdx_transport_can_send_event {
    uint32_t session_id;
    uint32_t capture_generation;
};

struct rdx_transport_event {
    enum rdx_transport_event_type type;
    uint32_t connection_generation;
    union {
        uint8_t ccc_enabled;
        struct rdx_transport_rx_event rx;
        struct rdx_transport_can_send_event can_send;
    } data;
};

/* RX data is borrowed and is valid only for the duration of the callback. */
typedef void (*rdx_transport_event_callback_t)(
    const struct rdx_transport_event *event, void *context);

int rdx_transport_init(rdx_transport_event_callback_t event_callback,
                       void *context);
void rdx_transport_exit(void);
enum rdx_transport_result rdx_transport_send(
    const uint8_t *data, uint16_t len);
int rdx_transport_request_can_send_now(
    uint32_t session_id, uint32_t capture_generation);
int rdx_transport_set_record_streaming(uint8_t enabled);
void rdx_transport_tx_diagnostics_reset(void);
void rdx_transport_tx_diagnostics_log(void);
int rdx_transport_refresh_local_name(void);
void rdx_transport_disconnect(void);

#endif
