#ifndef RDX_PROTOCOL_V23_INTERNAL_H
#define RDX_PROTOCOL_V23_INTERNAL_H

#include <stdint.h>

#include "rdx_protocol/protocol/v23/rdx_protocol_session.h"

struct rdx_protocol_v23_context {
    struct rdx_protocol_session session;
    volatile uint8_t initialized;
};

int rdx_protocol_v23_send(struct rdx_protocol_v23_context *context,
                          const uint8_t *data, uint16_t len);
void rdx_protocol_v23_log_rx(struct rdx_protocol_v23_context *context,
                             const char *command,
                             const uint8_t *data, uint16_t len);
uint8_t rdx_protocol_v23_data_equals(const uint8_t *data, uint16_t len,
                                     const uint8_t *expected,
                                     uint16_t expected_len);
uint8_t rdx_protocol_v23_data_starts_with(const uint8_t *data, uint16_t len,
                                          const uint8_t *prefix,
                                          uint16_t prefix_len);
uint8_t rdx_protocol_v23_query_ready(
    const struct rdx_protocol_v23_context *context,
    const char *command, uint16_t len);

void rdx_protocol_v23_management_init(
    struct rdx_protocol_v23_context *context);
void rdx_protocol_v23_management_exit(void);
void rdx_protocol_v23_management_handle_link_change(uint8_t connected);
uint8_t rdx_protocol_v23_management_handle_command(
    const uint8_t *data, uint16_t len);

int rdx_protocol_v23_record_init(
    struct rdx_protocol_v23_context *context);
void rdx_protocol_v23_record_exit(void);
void rdx_protocol_v23_record_handle_link_change(uint8_t connected);
void rdx_protocol_v23_record_handle_ccc_change(void);
uint8_t rdx_protocol_v23_record_handle_command(
    const uint8_t *data, uint16_t len);
void rdx_protocol_v23_record_handle_can_send(uint32_t session_id,
                                             uint32_t capture_generation);

int rdx_protocol_v23_local_storage_init(
    struct rdx_protocol_v23_context *context);
void rdx_protocol_v23_local_storage_exit(void);
void rdx_protocol_v23_local_storage_handle_link_change(uint8_t connected);
void rdx_protocol_v23_local_storage_handle_ccc_change(void);
uint8_t rdx_protocol_v23_local_storage_handle_command(
    const uint8_t *data, uint16_t len);
void rdx_protocol_v23_local_storage_handle_can_send(
    uint32_t owner, uint32_t token);
void rdx_protocol_v23_local_storage_handle_upload_timeout(void);

#endif
