#include <assert.h>
#include <string.h>

#include "rdx/rdx_record_coordinator.h"
#include "rdx/rdx_record_engine.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_session.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23_internal.h"

static rdx_record_engine_event_callback_t engine_callback;
static void *engine_callback_priv;
static struct rdx_record_request last_request;
static enum rdx_storage_result prepare_result;
static int submit_result;
static unsigned int prepare_calls;
static unsigned int cancel_calls;
static unsigned int submit_calls;
static unsigned int send_calls;
static unsigned int can_send_request_calls;
static int send_result;
static uint8_t last_wire[128];
static uint16_t last_wire_len;

static const struct rdx_record_destination_ops fake_composite_ops = {
    .requirement = RDX_RECORD_DESTINATION_REQUIRED,
};

enum rdx_record_result rdx_record_coordinator_prepare_engine_start(
    void *priv,
    struct rdx_record_request *request)
{
    struct rdx_record_coordinator *coordinator = priv;

    assert(coordinator);
    assert(request);
    prepare_calls++;
    if (prepare_result == RDX_STORAGE_RESULT_READY) {
        request->destination_ops = &fake_composite_ops;
        request->destination_priv = coordinator;
        return RDX_RECORD_RESULT_OK;
    }
    return prepare_result == RDX_STORAGE_RESULT_BUSY
               ? RDX_RECORD_RESULT_BUSY
               : RDX_RECORD_RESULT_STORAGE_ERROR;
}

enum rdx_storage_result rdx_record_coordinator_cancel_start(
    struct rdx_record_coordinator *coordinator,
    enum rdx_record_cause cause)
{
    assert(coordinator);
    assert(cause != RDX_RECORD_CAUSE_NONE);
    cancel_calls++;
    return RDX_STORAGE_RESULT_READY;
}

int rdx_record_coordinator_get_record_identity(
    const struct rdx_record_coordinator *coordinator,
    uint32_t *serial_number, char *filename, uint32_t filename_capacity)
{
    static const char compat_name[] = "42b595.raw";

    assert(coordinator);
    assert(serial_number);
    assert(filename);
    assert(filename_capacity >= sizeof(compat_name));
    *serial_number = 23;
    memcpy(filename, compat_name, sizeof(compat_name));
    return 0;
}

int rdx_record_engine_init(rdx_record_engine_event_callback_t callback,
                           void *callback_priv)
{
    engine_callback = callback;
    engine_callback_priv = callback_priv;
    return 0;
}

int rdx_record_engine_submit(const struct rdx_record_request *request)
{
    assert(request);
    last_request = *request;
    submit_calls++;
    return submit_result;
}

int rdx_record_engine_submit_system_event(
    enum rdx_record_system_event_source source,
    enum rdx_record_system_event_type type,
    enum rdx_record_cause cause)
{
    (void)source;
    (void)type;
    (void)cause;
    return RDX_RECORD_RESULT_OK;
}

int rdx_record_engine_notify_destination_ready(
    uint32_t session_id, uint32_t capture_generation)
{
    (void)session_id;
    (void)capture_generation;
    return RDX_RECORD_RESULT_OK;
}

int rdx_record_engine_shutdown(uint16_t timeout_ms)
{
    (void)timeout_ms;
    return 0;
}

enum rdx_record_state rdx_record_engine_get_state(void)
{
    return RDX_RECORD_STATE_IDLE;
}

int rdx_protocol_v23_send(struct rdx_protocol_v23_context *context,
                          const uint8_t *data, uint16_t len)
{
    assert(context);
    assert(data);
    assert(len);
    assert(len < sizeof(last_wire));
    memcpy(last_wire, data, len);
    last_wire[len] = 0;
    last_wire_len = len;
    send_calls++;
    return send_result;
}

int rdx_transport_request_can_send_now(
    uint32_t session_id, uint32_t capture_generation)
{
    assert(session_id);
    assert(capture_generation);
    can_send_request_calls++;
    return 0;
}

void rdx_protocol_v23_log_rx(struct rdx_protocol_v23_context *context,
                             const char *command,
                             const uint8_t *data, uint16_t len)
{
    (void)context;
    (void)command;
    (void)data;
    (void)len;
}

uint8_t rdx_protocol_v23_data_equals(const uint8_t *data, uint16_t len,
                                     const uint8_t *expected,
                                     uint16_t expected_len)
{
    return len == expected_len && !memcmp(data, expected, expected_len);
}

uint8_t rdx_protocol_v23_data_starts_with(const uint8_t *data, uint16_t len,
                                          const uint8_t *prefix,
                                          uint16_t prefix_len)
{
    return len >= prefix_len && !memcmp(data, prefix, prefix_len);
}

uint8_t rdx_protocol_v23_query_ready(
    const struct rdx_protocol_v23_context *context,
    const char *command, uint16_t len)
{
    (void)context;
    (void)command;
    (void)len;
    return 1;
}

uint8_t rdx_protocol_session_can_send(
    const struct rdx_protocol_session *session)
{
    (void)session;
    return 1;
}

int rdx_transport_set_record_streaming(uint8_t enabled)
{
    (void)enabled;
    return 0;
}

void rdx_transport_tx_diagnostics_reset(void)
{
}

void rdx_transport_tx_diagnostics_log(void)
{
}

static void reset_test_state(void)
{
    engine_callback = NULL;
    engine_callback_priv = NULL;
    memset(&last_request, 0, sizeof(last_request));
    prepare_result = RDX_STORAGE_RESULT_READY;
    submit_result = RDX_RECORD_RESULT_OK;
    prepare_calls = 0;
    cancel_calls = 0;
    submit_calls = 0;
    send_calls = 0;
    can_send_request_calls = 0;
    send_result = RDX_TRANSPORT_ACCEPTED;
    memset(last_wire, 0, sizeof(last_wire));
    last_wire_len = 0;
}

static void emit_start_result(enum rdx_record_result result,
                              enum rdx_record_cause cause)
{
    struct rdx_record_engine_event event;

    assert(engine_callback);
    memset(&event, 0, sizeof(event));
    event.type = RDX_RECORD_ENGINE_START_COMPLETE;
    event.request_id = last_request.request_id;
    event.session_id = 23;
    event.capture_generation = 5;
    event.scene = last_request.scene;
    event.result = result;
    event.cause = cause;
    engine_callback(engine_callback_priv, &event);
}

static void emit_stopped(void)
{
    struct rdx_record_engine_event event;

    assert(engine_callback);
    memset(&event, 0, sizeof(event));
    event.type = RDX_RECORD_ENGINE_SESSION_STOPPED;
    event.session_id = 23;
    event.capture_generation = 5;
    event.result = RDX_RECORD_RESULT_OK;
    event.termination_mode = RDX_RECORD_TERMINATION_NORMAL;
    event.cause = RDX_RECORD_CAUSE_USER_REQUEST;
    engine_callback(engine_callback_priv, &event);
}

static void emit_stop_ack_ready(void)
{
    struct rdx_record_engine_event event;

    assert(engine_callback);
    memset(&event, 0, sizeof(event));
    event.type = RDX_RECORD_ENGINE_STOP_ACK_READY;
    event.session_id = 23;
    event.capture_generation = 5;
    event.result = RDX_RECORD_RESULT_OK;
    event.termination_mode = RDX_RECORD_TERMINATION_NORMAL;
    event.cause = RDX_RECORD_CAUSE_USER_REQUEST;
    engine_callback(engine_callback_priv, &event);
}

static void test_online_backpressure_wait_gate(void)
{
    uint8_t payload[80] = {0};
    struct rdx_record_coordinator coordinator;
    struct rdx_protocol_v23_context context;
    struct rdx_record_frame frame;
    const struct rdx_record_destination_ops *online_ops;
    unsigned int requests_before;
    unsigned int sends_before;
    unsigned int i;
    static const uint8_t start[] = "*APP#record#1#0#1#";

    reset_test_state();
    memset(&coordinator, 0, sizeof(coordinator));
    memset(&context, 0, sizeof(context));
    memset(&frame, 0, sizeof(frame));
    assert(rdx_protocol_v23_record_bind_coordinator(&coordinator) == 0);
    assert(rdx_protocol_v23_record_init(&context) == 0);
    assert(rdx_protocol_v23_record_handle_command(
               start, sizeof(start) - 1) == 1);
    assert(last_request.prepare_start(last_request.prepare_priv,
                                      &last_request)
           == RDX_RECORD_RESULT_OK);
    online_ops = rdx_protocol_v23_record_online_destination_ops();
    assert(online_ops);
    assert(online_ops->poll);
    assert(online_ops->attach(NULL, 23, 5) == 0);
    emit_start_result(RDX_RECORD_RESULT_OK, RDX_RECORD_CAUSE_NONE);

    frame.session_id = 23;
    frame.capture_generation = 5;
    frame.payload = payload;
    frame.payload_len = sizeof(payload);
    sends_before = send_calls;
    requests_before = can_send_request_calls;
    send_result = RDX_TRANSPORT_RETRYABLE;

    assert(online_ops->consume(NULL, &frame) == 0);
    assert(send_calls == sends_before + 1);
    assert(can_send_request_calls == requests_before + 1);
    for (i = 1; i < 7; i++) {
        frame.session_frame_seq = i;
        frame.capture_frame_seq = i;
        assert(online_ops->consume(NULL, &frame) == 0);
    }
    /* Source frames may keep the waiter armed, but must not re-enter ATT
     * while the previous retryable result is still awaiting CAN_SEND. */
    assert(send_calls == sends_before + 1);
    assert(can_send_request_calls == requests_before + 7);

    send_result = RDX_TRANSPORT_ACCEPTED;
    online_ops->poll(NULL, 23, 5);
    assert(send_calls == sends_before + 2);
    assert(can_send_request_calls == requests_before + 8);

    online_ops->cancel(NULL, 23, 5);
    emit_stopped();
    rdx_protocol_v23_record_exit();
    assert(rdx_protocol_v23_record_bind_coordinator(NULL) == 0);
}

int main(void)
{
    static const uint8_t start[] = "*APP#record#1#0#1#";
    static const uint8_t stop[] = "*APP#record#0#0#1#";
    struct rdx_record_coordinator coordinator;
    struct rdx_protocol_v23_context context;
    unsigned int sends_before;

    reset_test_state();
    memset(&coordinator, 0, sizeof(coordinator));
    memset(&context, 0, sizeof(context));
    assert(rdx_protocol_v23_record_bind_coordinator(NULL) == 0);
    assert(rdx_protocol_v23_record_init(&context) != 0);
    assert(rdx_protocol_v23_record_bind_coordinator(&coordinator) == 0);
    assert(rdx_protocol_v23_record_init(&context) == 0);

    submit_result = RDX_RECORD_RESULT_CONTROL_QUEUE_FULL;
    assert(rdx_protocol_v23_record_handle_command(
               start, sizeof(start) - 1) == 1);
    assert(prepare_calls == 0);
    assert(submit_calls == 1);
    assert(cancel_calls == 0);
    assert(last_request.prepare_start
           == rdx_record_coordinator_prepare_engine_start);
    assert(last_request.prepare_priv == &coordinator);
    assert(last_request.destination_ops == NULL);

    prepare_result = RDX_STORAGE_RESULT_NOT_READY;
    submit_result = RDX_RECORD_RESULT_OK;
    sends_before = send_calls;
    assert(rdx_protocol_v23_record_handle_command(
               start, sizeof(start) - 1) == 1);
    assert(prepare_calls == 0);
    assert(submit_calls == 2);
    assert(last_request.prepare_start(last_request.prepare_priv,
                                      &last_request)
           == RDX_RECORD_RESULT_STORAGE_ERROR);
    assert(prepare_calls == 1);
    emit_start_result(RDX_RECORD_RESULT_STORAGE_ERROR,
                      RDX_RECORD_CAUSE_STORAGE_ERROR);
    assert(cancel_calls == 1);
    assert(send_calls == sends_before + 1);

    prepare_result = RDX_STORAGE_RESULT_READY;
    assert(rdx_protocol_v23_record_handle_command(
               start, sizeof(start) - 1) == 1);
    assert(prepare_calls == 1);
    assert(submit_calls == 3);
    assert(last_request.prepare_start(last_request.prepare_priv,
                                      &last_request)
           == RDX_RECORD_RESULT_OK);
    assert(prepare_calls == 2);
    emit_start_result(RDX_RECORD_RESULT_START_FAILED,
                      RDX_RECORD_CAUSE_SOURCE_LOST);
    assert(cancel_calls == 2);

    assert(rdx_protocol_v23_record_handle_command(
               start, sizeof(start) - 1) == 1);
    assert(prepare_calls == 2);
    assert(submit_calls == 4);
    assert(last_request.prepare_start(last_request.prepare_priv,
                                      &last_request)
           == RDX_RECORD_RESULT_OK);
    assert(prepare_calls == 3);
    emit_start_result(RDX_RECORD_RESULT_OK, RDX_RECORD_CAUSE_NONE);
    assert(last_request.type == RDX_RECORD_REQUEST_START);
    assert(last_request.destination_ops == &fake_composite_ops);
    assert(last_wire_len == sizeof("*DEV#record#1#0#1#0#1#0#23#42b595.raw#") - 1);
    assert(!strcmp((char *)last_wire,
                   "*DEV#record#1#0#1#0#1#0#23#42b595.raw#"));

    /* A failed STOP submit must restore STREAMING and retain the persistent
     * identity, otherwise all later STOP/START commands remain stuck behind
     * STOP_PENDING. */
    submit_result = RDX_RECORD_RESULT_CONTROL_QUEUE_FULL;
    sends_before = send_calls;
    assert(rdx_protocol_v23_record_handle_command(
               stop, sizeof(stop) - 1) == 1);
    assert(submit_calls == 5);
    assert(last_request.type == RDX_RECORD_REQUEST_STOP);
    assert(send_calls == sends_before + 1);
    assert(!strcmp((char *)last_wire,
                   "*DEV#record#1#0#1#0#1#0#23#42b595.raw#"));

    /* The retry is accepted.  A duplicate before the Engine establishes
     * the STOP cut must neither resubmit nor emit a premature STOP ACK. */
    submit_result = RDX_RECORD_RESULT_OK;
    sends_before = send_calls;
    assert(rdx_protocol_v23_record_handle_command(
               stop, sizeof(stop) - 1) == 1);
    assert(submit_calls == 6);
    assert(last_request.type == RDX_RECORD_REQUEST_STOP);
    assert(send_calls == sends_before);
    assert(rdx_protocol_v23_record_handle_command(
               stop, sizeof(stop) - 1) == 1);
    assert(submit_calls == 6);
    assert(send_calls == sends_before);

    /* Once capture is closed and all pre-cut candidates are owned by the
     * Destination, report the long V23 STOP state without waiting for the
     * Artifact commit.  If ATT rejects the first attempt, a later STOP
     * retry repeats only that ACK and does not resubmit commit. */
    send_result = RDX_TRANSPORT_RETRYABLE;
    emit_stop_ack_ready();
    assert(send_calls == sends_before + 1);
    assert(!strcmp((char *)last_wire,
                   "*DEV#record#0#0#1#0#1#0#23#42b595.raw#"));
    send_result = RDX_TRANSPORT_ACCEPTED;
    assert(rdx_protocol_v23_record_handle_command(
               stop, sizeof(stop) - 1) == 1);
    assert(submit_calls == 6);
    assert(send_calls == sends_before + 2);
    assert(!strcmp((char *)last_wire,
                   "*DEV#record#0#0#1#0#1#0#23#42b595.raw#"));

    /* Successful commit closes the internal lifecycle but does not send a
     * second STOP notification after the early V23 state ACK. */
    sends_before = send_calls;
    emit_stopped();
    assert(send_calls == sends_before);
    assert(!strcmp((char *)last_wire,
                   "*DEV#record#0#0#1#0#1#0#23#42b595.raw#"));

    /* Once complete, another STOP is an idempotent state query and does not
     * expose a stale file identity. */
    sends_before = send_calls;
    assert(rdx_protocol_v23_record_handle_command(
               stop, sizeof(stop) - 1) == 1);
    assert(submit_calls == 6);
    assert(send_calls == sends_before + 1);
    assert(!strcmp((char *)last_wire,
                   "*DEV#record#0#0#1#0#1#0#"));

    rdx_protocol_v23_record_exit();
    assert(rdx_protocol_v23_record_bind_coordinator(NULL) == 0);
    test_online_backpressure_wait_gate();
    return 0;
}
