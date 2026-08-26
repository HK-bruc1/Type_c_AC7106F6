#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rdx/domain/rdx_record_session.h"

static struct rdx_record_request make_request(
    enum rdx_record_request_type type,
    enum rdx_record_controller_id controller,
    enum rdx_record_scene scene,
    enum rdx_record_format_id format)
{
    struct rdx_record_request request;

    memset(&request, 0, sizeof(request));
    request.type = type;
    request.request_id = 7;
    request.controller_id = controller;
    request.scene = scene;
    request.format_id = format;
    return request;
}

static void test_start_and_control_invariants(void)
{
    struct rdx_record_session session;
    struct rdx_record_request request = make_request(
        RDX_RECORD_REQUEST_START,
        RDX_RECORD_CONTROLLER_ONLINE,
        RDX_RECORD_SCENE_MEETING,
        RDX_RECORD_FORMAT_MEETING_V1);

    rdx_record_session_init(&session);
    assert(session.state == RDX_RECORD_STATE_IDLE);
    assert(rdx_record_session_validate_start(&session, 0)
           == RDX_RECORD_RESULT_UNSUPPORTED_FORMAT);
    assert(rdx_record_session_validate_start(&session, 1)
           == RDX_RECORD_RESULT_OK);

    rdx_record_session_begin_start(&session, &request);
    assert(session.state == RDX_RECORD_STATE_STARTING);
    assert(session.session_id == 1);
    assert(session.capture_generation == 1);
    assert(rdx_record_session_validate_start(&session, 1)
           == RDX_RECORD_RESULT_BUSY);
    rdx_record_session_complete_start(&session);
    assert(session.state == RDX_RECORD_STATE_ACTIVE);

    request.type = RDX_RECORD_REQUEST_PAUSE;
    request.session_id = session.session_id;
    assert(rdx_record_session_validate_control(
               &session, &request, RDX_RECORD_STATE_ACTIVE)
           == RDX_RECORD_RESULT_OK);
    request.controller_id = RDX_RECORD_CONTROLLER_NONE;
    assert(rdx_record_session_validate_control(
               &session, &request, RDX_RECORD_STATE_ACTIVE)
           == RDX_RECORD_RESULT_NOT_CONTROLLER);
    request.controller_id = session.controller_id;
    request.session_id++;
    assert(rdx_record_session_validate_control(
               &session, &request, RDX_RECORD_STATE_ACTIVE)
           == RDX_RECORD_RESULT_BAD_STATE);
}

static void test_pause_resume_and_generation(void)
{
    struct rdx_record_session session;
    struct rdx_record_request request = make_request(
        RDX_RECORD_REQUEST_START,
        RDX_RECORD_CONTROLLER_ONLINE,
        RDX_RECORD_SCENE_CALL,
        RDX_RECORD_FORMAT_CALL_V1);

    rdx_record_session_init(&session);
    rdx_record_session_begin_start(&session, &request);
    rdx_record_session_complete_start(&session);
    rdx_record_session_begin_pause(&session);
    assert(session.state == RDX_RECORD_STATE_PAUSING_BY_USER);
    rdx_record_session_complete_pause(&session);
    assert(session.state == RDX_RECORD_STATE_PAUSED_BY_USER);

    session.next_capture_generation = UINT32_MAX;
    rdx_record_session_begin_resume(&session);
    assert(session.state == RDX_RECORD_STATE_RESUMING);
    assert(session.capture_generation == 1);
    rdx_record_session_abort_resume(&session);
    assert(session.state == RDX_RECORD_STATE_PAUSED_BY_USER);
    rdx_record_session_begin_resume(&session);
    assert(session.capture_generation == 2);
    rdx_record_session_complete_resume(&session);
    assert(session.state == RDX_RECORD_STATE_ACTIVE);
}

static void test_stop_and_owner_release(void)
{
    struct rdx_record_session session;
    struct rdx_record_request request = make_request(
        RDX_RECORD_REQUEST_START,
        RDX_RECORD_CONTROLLER_ONLINE,
        RDX_RECORD_SCENE_MEETING,
        RDX_RECORD_FORMAT_MEETING_V1);
    uint32_t session_id;
    uint32_t capture_generation;

    rdx_record_session_init(&session);
    assert(rdx_record_session_validate_stop(
               &session, RDX_RECORD_CONTROLLER_NONE)
           == RDX_RECORD_RESULT_OK);
    rdx_record_session_begin_start(&session, &request);
    rdx_record_session_complete_start(&session);
    assert(rdx_record_session_validate_stop(
               &session, RDX_RECORD_CONTROLLER_NONE)
           == RDX_RECORD_RESULT_NOT_CONTROLLER);
    assert(rdx_record_session_validate_stop(
               &session, RDX_RECORD_CONTROLLER_ONLINE)
           == RDX_RECORD_RESULT_OK);

    session_id = session.session_id;
    capture_generation = session.capture_generation;
    rdx_record_session_begin_stop(&session);
    assert(session.state == RDX_RECORD_STATE_STOPPING);
    rdx_record_session_complete_stop(
        &session, RDX_RECORD_TERMINATION_NORMAL,
        RDX_RECORD_CAUSE_USER_REQUEST);
    assert(session.state == RDX_RECORD_STATE_IDLE);
    assert(session.termination_mode == RDX_RECORD_TERMINATION_NORMAL);
    assert(session.termination_cause == RDX_RECORD_CAUSE_USER_REQUEST);
    rdx_record_session_release_owner(&session);
    assert(session.controller_id == RDX_RECORD_CONTROLLER_NONE);
    assert(session.format_id == RDX_RECORD_FORMAT_NONE);
    assert(session.session_id == session_id);
    assert(session.capture_generation == capture_generation);
}

int main(void)
{
    test_start_and_control_invariants();
    test_pause_resume_and_generation();
    test_stop_and_owner_release();
    puts("RDX record session tests passed");
    return 0;
}
