#include <stddef.h>
#include <string.h>

#include "rdx/domain/rdx_record_session.h"

static uint32_t rdx_record_session_next_token(uint32_t token)
{
    token++;
    return token ? token : 1;
}

void rdx_record_session_init(struct rdx_record_session *session)
{
    if (!session) {
        return;
    }
    memset(session, 0, sizeof(*session));
    session->state = RDX_RECORD_STATE_IDLE;
    session->scene = RDX_RECORD_SCENE_MEETING;
    session->format_id = RDX_RECORD_FORMAT_NONE;
}

enum rdx_record_result rdx_record_session_validate_start(
    const struct rdx_record_session *session, uint8_t format_supported)
{
    if (!session || !format_supported) {
        return RDX_RECORD_RESULT_UNSUPPORTED_FORMAT;
    }
    if (session->state != RDX_RECORD_STATE_IDLE) {
        return RDX_RECORD_RESULT_BUSY;
    }
    return RDX_RECORD_RESULT_OK;
}

void rdx_record_session_begin_start(
    struct rdx_record_session *session,
    const struct rdx_record_request *request)
{
    if (!session || !request) {
        return;
    }
    session->state = RDX_RECORD_STATE_STARTING;
    session->session_id = rdx_record_session_next_token(
        session->next_session_id);
    session->next_session_id = session->session_id;
    session->capture_generation = rdx_record_session_next_token(
        session->next_capture_generation);
    session->next_capture_generation = session->capture_generation;
    session->start_request_id = request->request_id;
    session->controller_id = request->controller_id;
    session->scene = request->scene;
    session->format_id = request->format_id;
}

void rdx_record_session_complete_start(struct rdx_record_session *session)
{
    if (session && session->state == RDX_RECORD_STATE_STARTING) {
        session->state = RDX_RECORD_STATE_ACTIVE;
    }
}

void rdx_record_session_abort_start(struct rdx_record_session *session)
{
    if (!session) {
        return;
    }
    session->state = RDX_RECORD_STATE_IDLE;
}

enum rdx_record_result rdx_record_session_validate_control(
    const struct rdx_record_session *session,
    const struct rdx_record_request *request,
    enum rdx_record_state required_state)
{
    if (!session || !request) {
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    if (request->controller_id != session->controller_id) {
        return RDX_RECORD_RESULT_NOT_CONTROLLER;
    }
    if (session->state != required_state
        || request->session_id != session->session_id
        || request->scene != session->scene) {
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    return RDX_RECORD_RESULT_OK;
}

enum rdx_record_result rdx_record_session_validate_stop(
    const struct rdx_record_session *session,
    enum rdx_record_controller_id controller_id)
{
    if (!session) {
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    if (session->state != RDX_RECORD_STATE_IDLE
        && controller_id != session->controller_id) {
        return RDX_RECORD_RESULT_NOT_CONTROLLER;
    }
    return RDX_RECORD_RESULT_OK;
}

void rdx_record_session_begin_pause(struct rdx_record_session *session)
{
    if (session) {
        session->state = RDX_RECORD_STATE_PAUSING_BY_USER;
    }
}

void rdx_record_session_complete_pause(struct rdx_record_session *session)
{
    if (session && session->state == RDX_RECORD_STATE_PAUSING_BY_USER) {
        session->state = RDX_RECORD_STATE_PAUSED_BY_USER;
    }
}

void rdx_record_session_begin_resume(struct rdx_record_session *session)
{
    if (!session) {
        return;
    }
    session->state = RDX_RECORD_STATE_RESUMING;
    session->capture_generation = rdx_record_session_next_token(
        session->next_capture_generation);
    session->next_capture_generation = session->capture_generation;
}

void rdx_record_session_complete_resume(struct rdx_record_session *session)
{
    if (session && session->state == RDX_RECORD_STATE_RESUMING) {
        session->state = RDX_RECORD_STATE_ACTIVE;
    }
}

void rdx_record_session_abort_resume(struct rdx_record_session *session)
{
    if (session && session->state == RDX_RECORD_STATE_RESUMING) {
        session->state = RDX_RECORD_STATE_PAUSED_BY_USER;
    }
}

void rdx_record_session_begin_stop(struct rdx_record_session *session)
{
    if (session) {
        session->state = RDX_RECORD_STATE_STOPPING;
    }
}

void rdx_record_session_complete_stop(
    struct rdx_record_session *session,
    enum rdx_record_termination_mode mode,
    enum rdx_record_cause cause)
{
    if (!session) {
        return;
    }
    session->termination_mode = mode;
    session->termination_cause = cause;
    session->state = RDX_RECORD_STATE_IDLE;
}

void rdx_record_session_release_owner(struct rdx_record_session *session)
{
    if (!session) {
        return;
    }
    session->controller_id = RDX_RECORD_CONTROLLER_NONE;
    session->scene = RDX_RECORD_SCENE_MEETING;
    session->format_id = RDX_RECORD_FORMAT_NONE;
    session->start_request_id = 0;
}
