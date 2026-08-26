#ifndef RDX_RECORD_ENGINE_H
#define RDX_RECORD_ENGINE_H

#include "rdx/rdx_record_types.h"

int rdx_record_engine_init(rdx_record_engine_event_callback_t callback,
                           void *callback_priv);
int rdx_record_engine_submit(const struct rdx_record_request *request);
int rdx_record_engine_submit_system_event(
    enum rdx_record_system_event_source source,
    enum rdx_record_system_event_type type,
    enum rdx_record_cause cause);
int rdx_record_engine_notify_destination_ready(
    uint32_t session_id, uint32_t capture_generation);
int rdx_record_engine_shutdown(uint16_t timeout_ms);
enum rdx_record_state rdx_record_engine_get_state(void);

#endif
