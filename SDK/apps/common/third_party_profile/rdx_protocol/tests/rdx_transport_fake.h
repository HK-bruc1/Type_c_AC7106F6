#ifndef RDX_TRANSPORT_FAKE_H
#define RDX_TRANSPORT_FAKE_H

#include "rdx_transport.h"

#define RDX_TRANSPORT_FAKE_TX_BUFFER_SIZE      160

struct rdx_transport_fake_state {
    rdx_transport_event_callback_t callback;
    void *context;
    uint32_t generation;
    enum rdx_transport_result send_result;
    unsigned int send_attempt_count;
    unsigned int accepted_count;
    unsigned int disconnect_count;
    uint16_t accepted_len;
    uint8_t initialized;
    uint8_t connected;
    uint8_t ccc_enabled;
    uint8_t record_streaming;
    uint8_t accepted_data[RDX_TRANSPORT_FAKE_TX_BUFFER_SIZE];
};

const struct rdx_transport_fake_state *rdx_transport_fake_get_state(void);
void rdx_transport_fake_set_send_result(enum rdx_transport_result result);
int rdx_transport_fake_emit_event(const struct rdx_transport_event *event);
uint32_t rdx_transport_fake_connect(void);
void rdx_transport_fake_set_ccc(uint32_t generation, uint8_t enabled);
void rdx_transport_fake_receive(uint32_t generation,
                                const uint8_t *data, uint16_t len);
void rdx_transport_fake_emit_simple(
    enum rdx_transport_event_type type, uint32_t generation);

#endif
