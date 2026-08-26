#ifndef RDX_RECORD_SESSION_H
#define RDX_RECORD_SESSION_H

#include <stdint.h>

#include "rdx/rdx_record_types.h"

struct rdx_record_session {
    enum rdx_record_state state;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_scene scene;
    enum rdx_record_format_id format_id;
    enum rdx_record_termination_mode termination_mode;
    enum rdx_record_cause termination_cause;
    uint32_t next_session_id;
    uint32_t next_capture_generation;
    uint32_t session_id;
    uint32_t capture_generation;
    uint32_t start_request_id;
};

void rdx_record_session_init(struct rdx_record_session *session);
enum rdx_record_result rdx_record_session_validate_start(
    const struct rdx_record_session *session, uint8_t format_supported);
void rdx_record_session_begin_start(
    struct rdx_record_session *session,
    const struct rdx_record_request *request);
void rdx_record_session_complete_start(struct rdx_record_session *session);
void rdx_record_session_abort_start(struct rdx_record_session *session);
enum rdx_record_result rdx_record_session_validate_control(
    const struct rdx_record_session *session,
    const struct rdx_record_request *request,
    enum rdx_record_state required_state);
enum rdx_record_result rdx_record_session_validate_stop(
    const struct rdx_record_session *session,
    enum rdx_record_controller_id controller_id);
void rdx_record_session_begin_pause(struct rdx_record_session *session);
void rdx_record_session_complete_pause(struct rdx_record_session *session);
void rdx_record_session_begin_resume(struct rdx_record_session *session);
void rdx_record_session_complete_resume(struct rdx_record_session *session);
void rdx_record_session_abort_resume(struct rdx_record_session *session);
void rdx_record_session_begin_stop(struct rdx_record_session *session);
void rdx_record_session_complete_stop(
    struct rdx_record_session *session,
    enum rdx_record_termination_mode mode,
    enum rdx_record_cause cause);
void rdx_record_session_release_owner(struct rdx_record_session *session);

#endif
