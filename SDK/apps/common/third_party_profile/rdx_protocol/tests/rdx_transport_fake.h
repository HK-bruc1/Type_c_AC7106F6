#ifndef RDX_TRANSPORT_FAKE_H
#define RDX_TRANSPORT_FAKE_H

#include "rdx/ports/rdx_transport_port.h"

#define RDX_TRANSPORT_FAKE_TX_BUFFER_SIZE      160

struct rdx_transport_fake_state {
    rdx_transport_event_callback_t callback;
    void *context;
    uint32_t generation;
    enum rdx_transport_result send_result;
    unsigned int send_attempt_count;
    unsigned int accepted_count;
    unsigned int disconnect_count;
    unsigned int can_send_request_count;
    uint16_t accepted_len;
    uint8_t initialized;
    uint8_t connected;
    uint8_t ccc_enabled;
    uint8_t ota_ccc_enabled;
    uint8_t record_streaming;
    uint8_t can_send_pending;
    uint32_t can_send_owner;
    uint32_t can_send_token;
    uint8_t accepted_data[RDX_TRANSPORT_FAKE_TX_BUFFER_SIZE];
    unsigned int ota_send_attempt_count;
    unsigned int ota_accepted_count;
    uint16_t ota_accepted_len;
    uint8_t ota_accepted_data[RDX_TRANSPORT_FAKE_TX_BUFFER_SIZE];
};

const struct rdx_transport_fake_state *rdx_transport_fake_get_state(void);
void rdx_transport_fake_set_send_result(enum rdx_transport_result result);
int rdx_transport_fake_emit_event(const struct rdx_transport_event *event);
uint32_t rdx_transport_fake_connect(void);
void rdx_transport_fake_set_ccc(uint32_t generation, uint8_t enabled);
void rdx_transport_fake_set_ota_ccc(uint32_t generation, uint8_t enabled);
void rdx_transport_fake_ota_write(uint32_t generation,
                                  const uint8_t *data, uint16_t len);
void rdx_transport_fake_receive(uint32_t generation,
                                const uint8_t *data, uint16_t len);
void rdx_transport_fake_emit_simple(
    enum rdx_transport_event_type type, uint32_t generation);
void rdx_transport_fake_emit_can_send(uint32_t generation,
                                      uint32_t owner,
                                      uint32_t token);

#endif
