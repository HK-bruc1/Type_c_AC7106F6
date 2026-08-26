#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE

#include "system/includes.h"
#include "rdx/rdx_record_engine.h"
#include "rdx/rdx_session_metadata_service.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23_internal.h"

#define RDX_RECORD_STREAM_PACKET_SIZE           128
#define RDX_RECORD_TX_QUEUE_DEPTH                RDX_CFG_BLE_TX_QUEUE_DEPTH
#define RDX_RECORD_TX_INLINE_BUDGET               1

static const u8 rdx_cmd_record[] = "*APP#record#";
#if RDX_CFG_RECORD_MARK_ENABLE
static const u8 rdx_cmd_recmark[] = "*APP#recmark#";
#endif

static struct rdx_protocol_v23_context *rdx_v23_record_context;
typedef enum {
    RDX_ONLINE_RECORD_IDLE = 0,
    RDX_ONLINE_RECORD_START_PENDING,
    RDX_ONLINE_RECORD_STREAMING,
    RDX_ONLINE_RECORD_PAUSE_PENDING,
    RDX_ONLINE_RECORD_PAUSED,
    RDX_ONLINE_RECORD_RESUME_PENDING,
    RDX_ONLINE_RECORD_STOP_PENDING,
} rdx_online_record_state_t;

typedef struct {
    volatile u8 state;
    u32 next_request_id;
    u32 session_id;
    u32 capture_generation;
    u32 sent_count;
    u32 send_fail_count;
    u32 destination_in_count;
    u32 queued_count;
    u32 queue_drop_count;
    u32 cancel_discard_count;
    u32 pause_discard_count;
    u32 drain_count;
    u32 event_poll_count;
    u32 budget_hit_count;
    u32 arm_fail_count;
    u8 tx_head;
    u8 tx_tail;
    u8 tx_count;
    u8 tx_peak;
    u8 wire_state;
    u8 scene;
    u8 final_stats_logged;
    u8 burst_peak;
    u16 tx_len[RDX_RECORD_TX_QUEUE_DEPTH];
    u8 tx_packet[RDX_RECORD_TX_QUEUE_DEPTH][RDX_RECORD_STREAM_PACKET_SIZE];
} rdx_online_record_runtime_t;

static rdx_online_record_runtime_t rdx_online_record;

static int rdx_v23_record_send_state(u8 wire_state)
{
    u8 response[32];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   "*DEV#record#%u#%u#1#0#1#0#", wire_state,
                   rdx_online_record.scene);
    if (len <= 0 || len >= sizeof(response)) {
        return -1;
    }
    ret = rdx_protocol_v23_send(rdx_v23_record_context, response, len);

    printf("[RDX][TX] cmd=record state=%u wire=%s len=%u ret=%d\n",
           wire_state, (char *)response, (unsigned int)len, ret);
    return ret;
}

#if RDX_CFG_RECORD_MARK_ENABLE
static int rdx_v23_record_send_mark(u8 result, u8 index,
                                     u32 offset_ms, u8 source)
{
    u8 response[64];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   "*DEV#recmark#%u#0##%u#%u#%u#",
                   result, index, (unsigned int)offset_ms, source);
    if (len <= 0 || len >= sizeof(response)) {
        return -1;
    }
    ret = rdx_protocol_v23_send(rdx_v23_record_context, response, len);
    printf("[RDX][TX] cmd=recmark result=%u index=%u offset=%u"
           " source=%u wire=%s len=%u ret=%d\n",
           result, index, (unsigned int)offset_ms, source,
           (char *)response, (unsigned int)len, ret);
    return ret;
}
#endif

static u32 rdx_v23_record_next_request_id(void)
{
    rdx_online_record.next_request_id++;
    if (!rdx_online_record.next_request_id) {
        rdx_online_record.next_request_id = 1;
    }
    return rdx_online_record.next_request_id;
}

static int rdx_v23_record_queue_packet(const u8 *packet, u16 len)
{
    u8 tail;

    if (!packet || !len || len > RDX_RECORD_STREAM_PACKET_SIZE) {
        return -1;
    }
    if (rdx_online_record.tx_count >= RDX_RECORD_TX_QUEUE_DEPTH) {
        /* Keep the bounded queue current; a congested BLE link must not stop
         * the USB capture session.  Dropping the oldest packet bounds latency. */
        rdx_online_record.tx_head =
            (rdx_online_record.tx_head + 1) % RDX_RECORD_TX_QUEUE_DEPTH;
        rdx_online_record.tx_count--;
        rdx_online_record.queue_drop_count++;
    }

    tail = rdx_online_record.tx_tail;
    memcpy(rdx_online_record.tx_packet[tail], packet, len);
    rdx_online_record.tx_len[tail] = len;
    rdx_online_record.tx_tail = (tail + 1) % RDX_RECORD_TX_QUEUE_DEPTH;
    rdx_online_record.tx_count++;
    rdx_online_record.queued_count++;
    if (rdx_online_record.tx_count > rdx_online_record.tx_peak) {
        rdx_online_record.tx_peak = rdx_online_record.tx_count;
    }
    return 0;
}

static int rdx_v23_record_drain_queue(u8 budget)
{
    int ret = 0;
    u8 sent_now = 0;

    rdx_online_record.drain_count++;
    if (!budget) {
        budget = 0xff;
    }

    while (rdx_online_record.tx_count && sent_now < budget) {
        u8 head = rdx_online_record.tx_head;

        ret = rdx_protocol_v23_send(rdx_v23_record_context, rdx_online_record.tx_packet[head],
                            rdx_online_record.tx_len[head]);
        if (ret) {
            rdx_online_record.send_fail_count++;
            break;
        }
        rdx_online_record.sent_count++;
        rdx_online_record.tx_head =
            (head + 1) % RDX_RECORD_TX_QUEUE_DEPTH;
        rdx_online_record.tx_count--;
        sent_now++;
    }
    if (sent_now > rdx_online_record.burst_peak) {
        rdx_online_record.burst_peak = sent_now;
    }
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE
    if (rdx_online_record.tx_count) {
        if (sent_now >= budget) {
            rdx_online_record.budget_hit_count++;
        }
        if (rdx_transport_request_can_send_now(
                rdx_online_record.session_id,
                rdx_online_record.capture_generation)) {
            rdx_online_record.arm_fail_count++;
        }
    }
#endif
    return 0;
}

static void rdx_v23_record_log_final(const char *reason)
{
    if (rdx_online_record.final_stats_logged) {
        return;
    }
    rdx_online_record.final_stats_logged = 1;
#if RDX_CFG_BLE_TX_DIAG_ENABLE
    u32 accounted;

    accounted = rdx_online_record.sent_count
                + rdx_online_record.queue_drop_count
                + rdx_online_record.cancel_discard_count
                + rdx_online_record.tx_count;

    printf("[RDX][TXQ] final reason=%s session=%u capture=%u"
           " destination_in=%u sent=%u pending=%u tx_drop=%u"
           " cancel_discard=%u pause_discard=%u accounted=%u"
           " queued=%u q_peak=%u send_fail=%u\n",
           reason,
           (unsigned int)rdx_online_record.session_id,
           (unsigned int)rdx_online_record.capture_generation,
           (unsigned int)rdx_online_record.destination_in_count,
           (unsigned int)rdx_online_record.sent_count,
           rdx_online_record.tx_count,
           (unsigned int)rdx_online_record.queue_drop_count,
           (unsigned int)rdx_online_record.cancel_discard_count,
           (unsigned int)rdx_online_record.pause_discard_count,
           (unsigned int)accounted,
           (unsigned int)rdx_online_record.queued_count,
           rdx_online_record.tx_peak,
           (unsigned int)rdx_online_record.send_fail_count);
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE
    printf("[RDX][TXQ] drain=%u event_poll=%u budget_hit=%u"
           " burst_peak=%u arm_fail=%u budget=%u depth=%u direct_att=0\n",
           (unsigned int)rdx_online_record.drain_count,
           (unsigned int)rdx_online_record.event_poll_count,
           (unsigned int)rdx_online_record.budget_hit_count,
           rdx_online_record.burst_peak,
           (unsigned int)rdx_online_record.arm_fail_count,
           RDX_CFG_BLE_TX_BURST_BUDGET,
           RDX_RECORD_TX_QUEUE_DEPTH);
#endif
    rdx_transport_tx_diagnostics_log();
#else
    (void)reason;
#endif
}

static int rdx_v23_record_destination_attach(void *priv, u32 session_id,
                                              u32 capture_generation)
{
    (void)priv;
    memset(&rdx_online_record.tx_len, 0, sizeof(rdx_online_record.tx_len));
    rdx_online_record.session_id = session_id;
    rdx_online_record.capture_generation = capture_generation;
    rdx_online_record.sent_count = 0;
    rdx_online_record.send_fail_count = 0;
    rdx_online_record.destination_in_count = 0;
    rdx_online_record.queued_count = 0;
    rdx_online_record.queue_drop_count = 0;
    rdx_online_record.cancel_discard_count = 0;
    rdx_online_record.pause_discard_count = 0;
    rdx_online_record.drain_count = 0;
    rdx_online_record.event_poll_count = 0;
    rdx_online_record.budget_hit_count = 0;
    rdx_online_record.arm_fail_count = 0;
    rdx_online_record.tx_head = 0;
    rdx_online_record.tx_tail = 0;
    rdx_online_record.tx_count = 0;
    rdx_online_record.tx_peak = 0;
    rdx_online_record.burst_peak = 0;
    rdx_online_record.final_stats_logged = 0;
    rdx_transport_tx_diagnostics_reset();
    return 0;
}

static int rdx_v23_record_destination_consume(
    void *priv, const struct rdx_record_frame *frame)
{
    u8 packet[RDX_RECORD_STREAM_PACKET_SIZE];
    int header_len;
    int ret;

    (void)priv;
    if (!frame
        || frame->session_id != rdx_online_record.session_id
        || frame->capture_generation !=
            rdx_online_record.capture_generation) {
        return 0;
    }
    rdx_online_record.destination_in_count++;
    if (rdx_online_record.state != RDX_ONLINE_RECORD_STREAMING
        && rdx_online_record.state != RDX_ONLINE_RECORD_PAUSE_PENDING) {
        rdx_online_record.cancel_discard_count++;
        return 0;
    }

    header_len = snprintf((char *)packet, sizeof(packet),
                           "*DEV#stream#%u#%u#",
                           frame->payload_len, frame->payload_len);
    if (header_len <= 0
        || (u32)header_len + frame->payload_len > sizeof(packet)) {
        rdx_online_record.cancel_discard_count++;
        return -1;
    }
    memcpy(packet + header_len, frame->payload, frame->payload_len);
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE
    ret = rdx_v23_record_queue_packet(
        packet, header_len + frame->payload_len);
    if (ret) {
        return ret;
    }
    /* Keep frame ingestion cheap.  ATT-ready events use the larger burst
     * budget to recover backlog without turning a candidate batch into one
     * unbounded send loop. */
    rdx_v23_record_drain_queue(RDX_RECORD_TX_INLINE_BUDGET);
#else
    rdx_v23_record_drain_queue(0);
    if (rdx_online_record.tx_count) {
        ret = rdx_v23_record_queue_packet(
            packet, header_len + frame->payload_len);
    } else {
        ret = rdx_protocol_v23_send(rdx_v23_record_context, packet, header_len + frame->payload_len);
        if (ret) {
            rdx_online_record.send_fail_count++;
            ret = rdx_v23_record_queue_packet(
                packet, header_len + frame->payload_len);
        } else {
            rdx_online_record.sent_count++;
        }
    }
#endif

    if ((frame->session_frame_seq + 1) % 250 == 0) {
        printf("[RDX][ONLINE] frames=%u sent=%u send_fail=%u queued=%u"
               " pending=%u peak=%u drop=%u\n",
               (unsigned int)(frame->session_frame_seq + 1),
               (unsigned int)rdx_online_record.sent_count,
               (unsigned int)rdx_online_record.send_fail_count,
               (unsigned int)rdx_online_record.queued_count,
               (unsigned int)rdx_online_record.tx_count,
               (unsigned int)rdx_online_record.tx_peak,
               (unsigned int)rdx_online_record.queue_drop_count);
    }
    /* Transport back-pressure is handled by the bounded TX queue.  The
     * packet-size check above is the only fatal validation in this path. */
    return 0;
}

#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE
static void rdx_v23_record_destination_poll(
    void *priv, u32 session_id, u32 capture_generation)
{
    (void)priv;
    if (session_id != rdx_online_record.session_id
        || capture_generation != rdx_online_record.capture_generation
        || rdx_online_record.state != RDX_ONLINE_RECORD_STREAMING) {
        return;
    }
    rdx_online_record.event_poll_count++;
    rdx_v23_record_drain_queue(RDX_CFG_BLE_TX_BURST_BUDGET);
}
#endif

static void rdx_v23_record_destination_cancel(void *priv, u32 session_id,
                                               u32 capture_generation)
{
    (void)priv;
    if (session_id == rdx_online_record.session_id
        && capture_generation == rdx_online_record.capture_generation) {
        rdx_online_record.cancel_discard_count +=
            rdx_online_record.tx_count;
        rdx_online_record.tx_head = 0;
        rdx_online_record.tx_tail = 0;
        rdx_online_record.tx_count = 0;
        rdx_v23_record_log_final("cancel");
    }
}

static void rdx_v23_record_destination_detach(void *priv, u32 session_id,
                                               u32 capture_generation)
{
    (void)priv;
    if (session_id == rdx_online_record.session_id
        && capture_generation == rdx_online_record.capture_generation
        && !rdx_online_record.final_stats_logged) {
        rdx_online_record.cancel_discard_count +=
            rdx_online_record.tx_count;
        rdx_online_record.tx_head = 0;
        rdx_online_record.tx_tail = 0;
        rdx_online_record.tx_count = 0;
        rdx_v23_record_log_final("detach");
    }
}

static const struct rdx_record_destination_ops rdx_v23_record_destination = {
    .attach = rdx_v23_record_destination_attach,
    .consume = rdx_v23_record_destination_consume,
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE
    .poll = rdx_v23_record_destination_poll,
#endif
    .cancel = rdx_v23_record_destination_cancel,
    .detach = rdx_v23_record_destination_detach,
};

static void rdx_v23_record_engine_event(
    void *priv, const struct rdx_record_engine_event *event)
{
    int ret;

    (void)priv;
    if (!event) {
        return;
    }

    if (event->type == RDX_RECORD_ENGINE_START_COMPLETE) {
        if (event->result) {
            u8 wait_for_stop =
                event->result == RDX_RECORD_RESULT_CANCELLED
                && event->cause == RDX_RECORD_CAUSE_USER_REQUEST
                && rdx_online_record.state == RDX_ONLINE_RECORD_STOP_PENDING;
            u8 connection_invalid =
                event->cause == RDX_RECORD_CAUSE_LINK_LOST
                || event->cause == RDX_RECORD_CAUSE_CCC_OFF
                || event->cause == RDX_RECORD_CAUSE_SHUTDOWN;

            if (!wait_for_stop) {
                rdx_online_record.state = RDX_ONLINE_RECORD_IDLE;
            }
            rdx_transport_set_record_streaming(0);
            if (!wait_for_stop && !connection_invalid
                && rdx_protocol_session_can_send(
                    &rdx_v23_record_context->session)) {
                rdx_v23_record_send_state(0);
            }
            printf("[RDX][ONLINE] start_failed request=%u result=%u\n",
                   (unsigned int)event->request_id, event->result);
            return;
        }
        rdx_online_record.session_id = event->session_id;
        rdx_online_record.scene = event->scene;
        rdx_online_record.capture_generation = event->capture_generation;
#if RDX_CFG_RECORD_MARK_ENABLE
        rdx_session_metadata_begin(event->session_id);
#endif
        rdx_transport_set_record_streaming(1);
        ret = rdx_v23_record_send_state(1);
        if (ret) {
            rdx_record_engine_submit_system_event(
                RDX_RECORD_SYSTEM_SOURCE_ENGINE,
                RDX_RECORD_SYSTEM_ENGINE_FAULT,
                RDX_RECORD_CAUSE_START_FAILED);
            return;
        }
        rdx_online_record.state = RDX_ONLINE_RECORD_STREAMING;
        rdx_online_record.wire_state = 1;
        printf("[RDX][ONLINE] streaming session=%u capture=%u\n",
               (unsigned int)event->session_id,
               (unsigned int)event->capture_generation);
        return;
    }

    if (event->type == RDX_RECORD_ENGINE_PAUSE_COMPLETE) {
        if (rdx_online_record.state == RDX_ONLINE_RECORD_STOP_PENDING) {
            return;
        }
        if (event->result) {
            rdx_online_record.state = RDX_ONLINE_RECORD_STREAMING;
            rdx_v23_record_send_state(rdx_online_record.wire_state);
            printf("[RDX][ONLINE] pause_failed request=%u result=%u\n",
                   (unsigned int)event->request_id, event->result);
            return;
        }
        rdx_transport_set_record_streaming(0);
        rdx_online_record.pause_discard_count += rdx_online_record.tx_count;
        rdx_online_record.cancel_discard_count += rdx_online_record.tx_count;
        rdx_online_record.tx_head = 0;
        rdx_online_record.tx_tail = 0;
        rdx_online_record.tx_count = 0;
        rdx_online_record.state = RDX_ONLINE_RECORD_PAUSED;
        rdx_online_record.wire_state = 2;
        rdx_v23_record_send_state(2);
        printf("[RDX][ONLINE] paused session=%u capture=%u"
               " tx_discard=%u\n",
               (unsigned int)event->session_id,
               (unsigned int)event->capture_generation,
               (unsigned int)rdx_online_record.pause_discard_count);
        return;
    }

    if (event->type == RDX_RECORD_ENGINE_RESUME_COMPLETE) {
        if (rdx_online_record.state == RDX_ONLINE_RECORD_STOP_PENDING) {
            return;
        }
        if (event->result) {
            rdx_online_record.state = RDX_ONLINE_RECORD_PAUSED;
            rdx_v23_record_send_state(2);
            printf("[RDX][ONLINE] resume_failed request=%u result=%u\n",
                   (unsigned int)event->request_id, event->result);
            return;
        }
        rdx_online_record.capture_generation = event->capture_generation;
        rdx_transport_set_record_streaming(1);
        rdx_online_record.state = RDX_ONLINE_RECORD_STREAMING;
        rdx_online_record.wire_state = 3;
        rdx_v23_record_send_state(3);
        printf("[RDX][ONLINE] resumed session=%u capture=%u\n",
               (unsigned int)event->session_id,
               (unsigned int)event->capture_generation);
        return;
    }

#if RDX_CFG_RECORD_MARK_ENABLE
    if (event->type == RDX_RECORD_ENGINE_MARK_COMPLETE) {
        struct rdx_session_mark mark;

        rdx_session_metadata_apply_mark(
            event->session_id, event->active_pts, event->source,
            event->result, &mark);
        rdx_v23_record_send_mark(mark.result, mark.index,
                                  mark.offset_ms, mark.source);
        return;
    }
#endif

    if (event->type == RDX_RECORD_ENGINE_SESSION_STOPPED) {
        u8 notify = event->cause != RDX_RECORD_CAUSE_LINK_LOST
                    && event->cause != RDX_RECORD_CAUSE_CCC_OFF
                    && event->cause != RDX_RECORD_CAUSE_SHUTDOWN;

        rdx_transport_set_record_streaming(0);
        rdx_online_record.state = RDX_ONLINE_RECORD_IDLE;
        rdx_online_record.wire_state = 0;
        rdx_online_record.tx_count = 0;
#if RDX_CFG_RECORD_MARK_ENABLE
        rdx_session_metadata_end(event->session_id);
#endif
        if (notify && rdx_protocol_session_can_send(
                &rdx_v23_record_context->session)) {
            rdx_v23_record_send_state(0);
        }
        printf("[RDX][ONLINE] stopped session=%u mode=%u cause=%u"
               " result=%u notify=%u\n",
               (unsigned int)event->session_id,
               event->termination_mode, event->cause,
               event->result, notify);
    }
}

static enum rdx_record_format_id rdx_v23_record_format_for_scene(
    enum rdx_record_scene scene)
{
    return scene == RDX_RECORD_SCENE_CALL
           ? RDX_RECORD_FORMAT_CALL_V1
           : RDX_RECORD_FORMAT_MEETING_V1;
}

static void rdx_v23_record_start_scene(enum rdx_record_scene scene)
{
    struct rdx_record_request request;
    int ret;

    if (rdx_online_record.state == RDX_ONLINE_RECORD_STREAMING) {
        rdx_v23_record_send_state(rdx_online_record.wire_state);
        return;
    }
    if (rdx_online_record.state != RDX_ONLINE_RECORD_IDLE) {
        printf("[RDX][ONLINE] duplicate_start state=%u\n",
               rdx_online_record.state);
        return;
    }

    memset(&request, 0, sizeof(request));
    request.request_id = rdx_v23_record_next_request_id();
    request.type = RDX_RECORD_REQUEST_START;
    request.controller_id = RDX_RECORD_CONTROLLER_ONLINE;
    request.scene = scene;
    request.format_id = rdx_v23_record_format_for_scene(scene);
    request.destination_ops = &rdx_v23_record_destination;
    rdx_online_record.scene = scene;
    rdx_online_record.state = RDX_ONLINE_RECORD_START_PENDING;
    ret = rdx_record_engine_submit(&request);
    if (ret) {
        rdx_online_record.state = RDX_ONLINE_RECORD_IDLE;
        printf("[RDX][ONLINE] start_submit_failed request=%u ret=%d\n",
               (unsigned int)request.request_id, ret);
        rdx_v23_record_send_state(0);
    }
}

static void rdx_v23_record_start(void)
{
    rdx_v23_record_start_scene(RDX_RECORD_SCENE_MEETING);
}

#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
static void rdx_v23_record_control(enum rdx_record_request_type type)
{
    struct rdx_record_request request;
    int ret;

    if (type == RDX_RECORD_REQUEST_PAUSE) {
        if (rdx_online_record.state == RDX_ONLINE_RECORD_PAUSED) {
            rdx_v23_record_send_state(2);
            return;
        }
        if (rdx_online_record.state != RDX_ONLINE_RECORD_STREAMING) {
            printf("[RDX][ONLINE] pause_bad_state state=%u\n",
                   rdx_online_record.state);
            return;
        }
        rdx_online_record.state = RDX_ONLINE_RECORD_PAUSE_PENDING;
    } else {
        if (rdx_online_record.state == RDX_ONLINE_RECORD_STREAMING
            && rdx_online_record.wire_state == 3) {
            rdx_v23_record_send_state(3);
            return;
        }
        if (rdx_online_record.state != RDX_ONLINE_RECORD_PAUSED) {
            printf("[RDX][ONLINE] resume_bad_state state=%u\n",
                   rdx_online_record.state);
            return;
        }
        rdx_online_record.state = RDX_ONLINE_RECORD_RESUME_PENDING;
    }

    memset(&request, 0, sizeof(request));
    request.request_id = rdx_v23_record_next_request_id();
    request.session_id = rdx_online_record.session_id;
    request.type = type;
    request.controller_id = RDX_RECORD_CONTROLLER_ONLINE;
    request.scene = rdx_online_record.scene;
    request.format_id = rdx_v23_record_format_for_scene(
        rdx_online_record.scene);
    ret = rdx_record_engine_submit(&request);
    if (ret) {
        rdx_online_record.state = type == RDX_RECORD_REQUEST_PAUSE
                                  ? RDX_ONLINE_RECORD_STREAMING
                                  : RDX_ONLINE_RECORD_PAUSED;
        printf("[RDX][ONLINE] control_submit_failed type=%u request=%u"
               " ret=%d\n", type, (unsigned int)request.request_id, ret);
    }
}
#endif

#if RDX_CFG_RECORD_MARK_ENABLE
static void rdx_v23_record_mark(void)
{
    struct rdx_record_request request;
    int ret;

    if (!rdx_protocol_v23_query_ready(rdx_v23_record_context, "recmark",
                                          sizeof(rdx_cmd_recmark) - 1)) {
        return;
    }
    if (rdx_online_record.state == RDX_ONLINE_RECORD_IDLE
        || rdx_online_record.state == RDX_ONLINE_RECORD_START_PENDING
        || rdx_online_record.state == RDX_ONLINE_RECORD_STOP_PENDING) {
        rdx_v23_record_send_mark(1, 0, 0, RDX_RECORD_MARK_SOURCE_APP);
        return;
    }

    memset(&request, 0, sizeof(request));
    request.request_id = rdx_v23_record_next_request_id();
    request.session_id = rdx_online_record.session_id;
    request.type = RDX_RECORD_REQUEST_MARK;
    request.controller_id = RDX_RECORD_CONTROLLER_ONLINE;
    request.scene = rdx_online_record.scene;
    request.source = RDX_RECORD_MARK_SOURCE_APP;
    ret = rdx_record_engine_submit(&request);
    if (ret) {
        rdx_v23_record_send_mark(ret == RDX_RECORD_RESULT_CONTROL_QUEUE_FULL
                                  ? 2 : 1,
                                  0, 0, RDX_RECORD_MARK_SOURCE_APP);
    }
}
#endif

static void rdx_v23_record_stop(void)
{
    struct rdx_record_request request;
    int ret;

    if (rdx_online_record.state == RDX_ONLINE_RECORD_IDLE) {
        rdx_v23_record_send_state(0);
        return;
    }
    if (rdx_online_record.state == RDX_ONLINE_RECORD_STOP_PENDING) {
        return;
    }

    memset(&request, 0, sizeof(request));
    request.request_id = rdx_v23_record_next_request_id();
    request.type = RDX_RECORD_REQUEST_STOP;
    request.controller_id = RDX_RECORD_CONTROLLER_ONLINE;
    request.scene = rdx_online_record.scene;
    request.format_id = rdx_v23_record_format_for_scene(
        rdx_online_record.scene);
    rdx_online_record.state = RDX_ONLINE_RECORD_STOP_PENDING;
    ret = rdx_record_engine_submit(&request);
    if (ret) {
        printf("[RDX][ONLINE] stop_submit_failed request=%u ret=%d\n",
               (unsigned int)request.request_id, ret);
    }
}

static void rdx_v23_handle_record(const u8 *data, u16 len)
{
    static const u8 stop[] = "*APP#record#0#";
    static const u8 stop_meeting_16k[] = "*APP#record#0#0#1#";
    static const u8 stop_call[] = "*APP#record#0#1#";
    static const u8 stop_call_16k[] = "*APP#record#0#1#1#";
    static const u8 start[] = "*APP#record#1#";
    static const u8 start_meeting[] = "*APP#record#1#0#";
    static const u8 start_meeting_16k[] = "*APP#record#1#0#1#";
    static const u8 start_call[] = "*APP#record#1#1#";
    static const u8 start_call_16k[] = "*APP#record#1#1#1#";
    static const u8 pause[] = "*APP#record#2#";
    static const u8 pause_meeting_16k[] = "*APP#record#2#0#1#";
    static const u8 pause_call[] = "*APP#record#2#1#";
    static const u8 pause_call_16k[] = "*APP#record#2#1#1#";
    static const u8 resume[] = "*APP#record#3#";
    static const u8 resume_meeting_16k[] = "*APP#record#3#0#1#";
    static const u8 resume_call[] = "*APP#record#3#1#";
    static const u8 resume_call_16k[] = "*APP#record#3#1#1#";

    if (!rdx_protocol_v23_query_ready(rdx_v23_record_context, "record", len)) {
        return;
    }
    if (rdx_protocol_v23_data_equals(data, len, stop, sizeof(stop) - 1)
        || rdx_protocol_v23_data_equals(data, len, stop_meeting_16k,
                                sizeof(stop_meeting_16k) - 1)
        || rdx_protocol_v23_data_equals(data, len, stop_call,
                                sizeof(stop_call) - 1)
        || rdx_protocol_v23_data_equals(data, len, stop_call_16k,
                                sizeof(stop_call_16k) - 1)) {
        rdx_v23_record_stop();
        return;
    }
    if (rdx_protocol_v23_data_equals(data, len, start_call,
                             sizeof(start_call) - 1)
        || rdx_protocol_v23_data_equals(data, len, start_call_16k,
                                sizeof(start_call_16k) - 1)) {
        rdx_v23_record_start_scene(RDX_RECORD_SCENE_CALL);
        return;
    }
    if (rdx_protocol_v23_data_equals(data, len, start, sizeof(start) - 1)
        || rdx_protocol_v23_data_equals(data, len, start_meeting,
                                sizeof(start_meeting) - 1)
        || rdx_protocol_v23_data_equals(data, len, start_meeting_16k,
                                sizeof(start_meeting_16k) - 1)) {
        rdx_v23_record_start();
        return;
    }
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
    if (rdx_protocol_v23_data_equals(data, len, pause, sizeof(pause) - 1)
        || rdx_protocol_v23_data_equals(data, len, pause_meeting_16k,
                                sizeof(pause_meeting_16k) - 1)
        || rdx_protocol_v23_data_equals(data, len, pause_call,
                                sizeof(pause_call) - 1)
        || rdx_protocol_v23_data_equals(data, len, pause_call_16k,
                                sizeof(pause_call_16k) - 1)) {
        rdx_v23_record_control(RDX_RECORD_REQUEST_PAUSE);
        return;
    }
    if (rdx_protocol_v23_data_equals(data, len, resume, sizeof(resume) - 1)
        || rdx_protocol_v23_data_equals(data, len, resume_meeting_16k,
                                sizeof(resume_meeting_16k) - 1)
        || rdx_protocol_v23_data_equals(data, len, resume_call,
                                sizeof(resume_call) - 1)
        || rdx_protocol_v23_data_equals(data, len, resume_call_16k,
                                sizeof(resume_call_16k) - 1)) {
        rdx_v23_record_control(RDX_RECORD_REQUEST_RESUME);
        return;
    }
#endif

    printf("[RDX][DROP] cmd=record reason=unsupported_or_malformed"
           " wire=%.*s len=%u\n", (int)len, (char *)data, len);
}
int rdx_protocol_v23_record_init(
    struct rdx_protocol_v23_context *context)
{
    int ret;

    if (!context) {
        return -1;
    }
    rdx_v23_record_context = context;
    memset(&rdx_online_record, 0, sizeof(rdx_online_record));
    ret = rdx_record_engine_init(rdx_v23_record_engine_event, NULL);
    return ret;
}

void rdx_protocol_v23_record_exit(void)
{
    if (rdx_online_record.state != RDX_ONLINE_RECORD_IDLE) {
        rdx_online_record.state = RDX_ONLINE_RECORD_STOP_PENDING;
        rdx_transport_set_record_streaming(0);
    }
}

void rdx_protocol_v23_record_handle_link_change(u8 connected)
{
    if (connected) {
        return;
    }
    if (rdx_online_record.state != RDX_ONLINE_RECORD_IDLE) {
        rdx_online_record.state = RDX_ONLINE_RECORD_STOP_PENDING;
        rdx_transport_set_record_streaming(0);
    }
    rdx_record_engine_submit_system_event(
        RDX_RECORD_SYSTEM_SOURCE_TRANSPORT,
        RDX_RECORD_SYSTEM_LINK_LOST,
        RDX_RECORD_CAUSE_LINK_LOST);
}

void rdx_protocol_v23_record_handle_ccc_change(void)
{
    if (!rdx_v23_record_context
        || rdx_v23_record_context->session.ccc_ready) {
        return;
    }
    if (rdx_online_record.state != RDX_ONLINE_RECORD_IDLE) {
        rdx_online_record.state = RDX_ONLINE_RECORD_STOP_PENDING;
        rdx_transport_set_record_streaming(0);
    }
    rdx_record_engine_submit_system_event(
        RDX_RECORD_SYSTEM_SOURCE_TRANSPORT,
        RDX_RECORD_SYSTEM_CCC_OFF,
        RDX_RECORD_CAUSE_CCC_OFF);
}

u8 rdx_protocol_v23_record_handle_command(const u8 *data, u16 len)
{
    if (rdx_protocol_v23_data_starts_with(
            data, len, rdx_cmd_record, sizeof(rdx_cmd_record) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_record_context,
                                "record", data, len);
        rdx_v23_handle_record(data, len);
        return 1;
    }
#if RDX_CFG_RECORD_MARK_ENABLE
    if (rdx_protocol_v23_data_equals(
            data, len, rdx_cmd_recmark, sizeof(rdx_cmd_recmark) - 1)) {
        rdx_protocol_v23_log_rx(rdx_v23_record_context,
                                "recmark", data, len);
        rdx_v23_record_mark();
        return 1;
    }
#endif
    return 0;
}

void rdx_protocol_v23_record_handle_can_send(
    u32 session_id, u32 capture_generation)
{
#if RDX_CFG_BLE_TX_CAN_SEND_ENABLE
    rdx_record_engine_notify_destination_ready(
        session_id, capture_generation);
#else
    (void)session_id;
    (void)capture_generation;
#endif
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE */
