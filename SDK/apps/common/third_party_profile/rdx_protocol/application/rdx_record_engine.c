#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE

#include "generic/jiffies.h"
#include "task.h"
#include "spinlock.h"
#include "audio_capture_lease.h"
#include "rdx/domain/rdx_record_session.h"
#include "rdx/ports/rdx_audio_capture_port.h"
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#include "rdx/rdx_mic_gain_service.h"
#endif
#include "rdx/rdx_record_engine.h"

#define RDX_RECORD_TASK_NAME                    "rdx_record"
#define RDX_RECORD_TASK_WAKE                    0x52445801

struct rdx_record_candidate {
    uint32_t engine_generation;
    uint32_t session_id;
    uint32_t capture_generation;
    uint16_t len;
    uint8_t payload[RDX_RECORD_CANDIDATE_MAX_SIZE];
};

struct rdx_record_system_latch {
    uint8_t pending;
    enum rdx_record_system_event_source source;
    enum rdx_record_system_event_type type;
    enum rdx_record_cause cause;
};

struct rdx_record_stop_latch {
    uint8_t pending;
    uint32_t request_id;
    uint32_t last_request_id;
    uint16_t duplicate_count;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_cause cause;
};

struct rdx_record_destination_latch {
    uint8_t pending;
    uint32_t session_id;
    uint32_t capture_generation;
};

struct rdx_record_engine_runtime {
    spinlock_t lock;
    OS_SEM stopped_sem;
    struct rdx_record_request requests[RDX_RECORD_CONTROL_QUEUE_DEPTH];
    struct rdx_record_candidate candidates[RDX_RECORD_CANDIDATE_QUEUE_DEPTH];
    struct rdx_record_system_latch system_latch;
    struct rdx_record_stop_latch stop_latch;
    struct rdx_record_destination_latch destination_latch;
    struct audio_capture_lease capture_lease;
    const struct rdx_record_destination_ops *destination_ops;
    void *destination_priv;
    rdx_record_engine_event_callback_t event_callback;
    void *event_priv;
    struct rdx_record_session session;
    volatile uint8_t initialized;
    volatile uint8_t accepting;
    volatile uint8_t frame_gate;
    volatile uint8_t worker_created;
    volatile uint8_t task_running;
    uint8_t worker_wake_pending;
    uint8_t request_head;
    uint8_t request_tail;
    uint8_t request_count;
    uint8_t candidate_head;
    uint8_t candidate_tail;
    uint8_t candidate_count;
    uint8_t session_mic_gain_override_valid;
    uint8_t session_mic_gain;
    uint32_t engine_generation;
    uint32_t session_frame_seq;
    uint32_t capture_frame_seq;
    uint32_t warmup_count;
    uint32_t warmup_discard_count;
    uint32_t produced_count;
    uint32_t enqueued_count;
    uint32_t overflow_count;
    uint32_t stale_count;
    uint32_t stop_discarded;
    uint32_t format_mismatch_count;
#if RDX_CFG_RECORD_PERF_DIAG_ENABLE
    uint32_t wake_post_count;
    uint32_t wake_coalesced_count;
    uint32_t wake_post_fail_count;
#endif
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
    uint8_t first_frame_after_resume;
#endif
};

static struct rdx_record_engine_runtime rdx_record_engine;

static void rdx_record_engine_process_candidate(
    const struct rdx_record_candidate *candidate);
static uint8_t rdx_record_engine_take_candidate(
    struct rdx_record_candidate *candidate);

struct rdx_record_format_spec {
    uint16_t payload_size;
    uint16_t frame_ms;
    uint8_t warmup_frames;
};

static const struct rdx_record_format_spec *
rdx_record_engine_format_spec(enum rdx_record_format_id format_id)
{
    static const struct rdx_record_format_spec meeting_v1 = {
        .payload_size = RDX_RECORD_MEETING_V1_PAYLOAD_SIZE,
        .frame_ms = RDX_RECORD_MEETING_V1_FRAME_MS,
        .warmup_frames = RDX_RECORD_MEETING_V1_WARMUP_FRAMES,
    };
    static const struct rdx_record_format_spec call_v1 = {
        .payload_size = RDX_RECORD_CALL_V1_PAYLOAD_SIZE,
        .frame_ms = RDX_RECORD_CALL_V1_FRAME_MS,
        .warmup_frames = RDX_RECORD_CALL_V1_WARMUP_FRAMES,
    };

    switch (format_id) {
    case RDX_RECORD_FORMAT_MEETING_V1:
        return &meeting_v1;
    case RDX_RECORD_FORMAT_CALL_V1:
        return &call_v1;
    default:
        return NULL;
    }
}

static uint32_t rdx_record_engine_next_generation(uint32_t generation)
{
    generation++;
    return generation ? generation : 1;
}

static void rdx_record_engine_wake(void)
{
    uint8_t post = 0;
    int ret;

    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.worker_created) {
        if (!rdx_record_engine.worker_wake_pending) {
            rdx_record_engine.worker_wake_pending = 1;
            post = 1;
        } else {
#if RDX_CFG_RECORD_PERF_DIAG_ENABLE
            rdx_record_engine.wake_coalesced_count++;
#endif
        }
    }
    spin_unlock(&rdx_record_engine.lock);
    if (!post) {
        return;
    }

    ret = os_taskq_post_msg(RDX_RECORD_TASK_NAME, 1,
                            RDX_RECORD_TASK_WAKE);
    spin_lock(&rdx_record_engine.lock);
    if (ret) {
        /* A full queue already contains a wake for this worker.  Clear the
         * edge latch so a later producer can retry if the failure came from
         * another OS condition. */
        rdx_record_engine.worker_wake_pending = 0;
#if RDX_CFG_RECORD_PERF_DIAG_ENABLE
        rdx_record_engine.wake_post_fail_count++;
#endif
    } else {
#if RDX_CFG_RECORD_PERF_DIAG_ENABLE
        rdx_record_engine.wake_post_count++;
#endif
    }
    spin_unlock(&rdx_record_engine.lock);
}

static void rdx_record_engine_emit(
    enum rdx_record_engine_event_type type,
    uint32_t request_id,
    enum rdx_record_result result,
    enum rdx_record_termination_mode termination_mode,
    enum rdx_record_cause cause)
{
    struct rdx_record_engine_event event = {
        .type = type,
        .request_id = request_id,
        .session_id = rdx_record_engine.session.session_id,
        .capture_generation = rdx_record_engine.session.capture_generation,
        .controller_id = rdx_record_engine.session.controller_id,
        .scene = rdx_record_engine.session.scene,
        .result = result,
        .termination_mode = termination_mode,
        .cause = cause,
        .active_pts = 0,
        .source = 0,
    };

    if (rdx_record_engine.event_callback) {
        rdx_record_engine.event_callback(rdx_record_engine.event_priv, &event);
    }
}

#if RDX_CFG_RECORD_MARK_ENABLE
static void rdx_record_engine_emit_mark(
    const struct rdx_record_request *request,
    enum rdx_record_result result)
{
    const struct rdx_record_format_spec *format =
        rdx_record_engine_format_spec(rdx_record_engine.session.format_id);
    struct rdx_record_engine_event event = {
        .type = RDX_RECORD_ENGINE_MARK_COMPLETE,
        .request_id = request->request_id,
        .session_id = rdx_record_engine.session.session_id,
        .capture_generation = rdx_record_engine.session.capture_generation,
        .controller_id = request->controller_id,
        .scene = rdx_record_engine.session.scene,
        .result = result,
        .termination_mode = RDX_RECORD_TERMINATION_NONE,
        .cause = RDX_RECORD_CAUSE_NONE,
        .active_pts = (uint64_t)rdx_record_engine.session_frame_seq
                      * (format ? format->frame_ms : 0),
        .source = request->source,
    };

    if (rdx_record_engine.event_callback) {
        rdx_record_engine.event_callback(rdx_record_engine.event_priv, &event);
    }
}
#endif

static void rdx_record_engine_clear_queues(void)
{
    spin_lock(&rdx_record_engine.lock);
    rdx_record_engine.candidate_head = 0;
    rdx_record_engine.candidate_tail = 0;
    rdx_record_engine.candidate_count = 0;
    memset(&rdx_record_engine.destination_latch, 0,
           sizeof(rdx_record_engine.destination_latch));
    spin_unlock(&rdx_record_engine.lock);
}

static uint32_t rdx_record_engine_discard_candidates(void)
{
    uint32_t count;

    spin_lock(&rdx_record_engine.lock);
    count = rdx_record_engine.candidate_count;
    rdx_record_engine.candidate_head = 0;
    rdx_record_engine.candidate_tail = 0;
    rdx_record_engine.candidate_count = 0;
    memset(&rdx_record_engine.destination_latch, 0,
           sizeof(rdx_record_engine.destination_latch));
    spin_unlock(&rdx_record_engine.lock);
    return count;
}

static int rdx_record_engine_audio_frame(void *priv,
                                         uint32_t engine_generation,
                                         uint32_t session_id,
                                         uint32_t capture_generation,
                                         const uint8_t *data,
                                         uint32_t len)
{
    struct rdx_record_candidate *candidate;
    uint8_t tail;

    if (priv != &rdx_record_engine) {
        return 0;
    }
    rdx_record_engine.produced_count++;
    spin_lock(&rdx_record_engine.lock);
    if (engine_generation != rdx_record_engine.engine_generation
        || session_id != rdx_record_engine.session.session_id
        || capture_generation != rdx_record_engine.session.capture_generation) {
        rdx_record_engine.stale_count++;
        spin_unlock(&rdx_record_engine.lock);
        return 0;
    }
    if (!rdx_record_engine.frame_gate
        || (rdx_record_engine.session.state != RDX_RECORD_STATE_STARTING
            && rdx_record_engine.session.state != RDX_RECORD_STATE_ACTIVE)) {
        rdx_record_engine.stop_discarded++;
        spin_unlock(&rdx_record_engine.lock);
        return 0;
    }
    if (!data || !len || len > RDX_RECORD_CANDIDATE_MAX_SIZE) {
        rdx_record_engine.format_mismatch_count++;
        rdx_record_engine.system_latch.pending = 1;
        rdx_record_engine.system_latch.source = RDX_RECORD_SYSTEM_SOURCE_ENGINE;
        rdx_record_engine.system_latch.type = RDX_RECORD_SYSTEM_ENGINE_FAULT;
        rdx_record_engine.system_latch.cause = RDX_RECORD_CAUSE_FORMAT_MISMATCH;
        rdx_record_engine.frame_gate = 0;
        spin_unlock(&rdx_record_engine.lock);
        rdx_record_engine_wake();
        return 0;
    }
    if (rdx_record_engine.candidate_count >= RDX_RECORD_CANDIDATE_QUEUE_DEPTH) {
        if (rdx_record_engine.destination_ops
            && rdx_record_engine.destination_ops->requirement
               == RDX_RECORD_DESTINATION_REQUIRED) {
            /* Local storage is REQUIRED in Composite mode. Do not overwrite
             * an older candidate: that would make the Artifact appear
             * complete while its frame counts are no longer recoverable. */
            rdx_record_engine.overflow_count++;
            rdx_record_engine.system_latch.pending = 1;
            rdx_record_engine.system_latch.source =
                RDX_RECORD_SYSTEM_SOURCE_ENGINE;
            rdx_record_engine.system_latch.type =
                RDX_RECORD_SYSTEM_ENGINE_FAULT;
            rdx_record_engine.system_latch.cause = RDX_RECORD_CAUSE_QUEUE_FULL;
            rdx_record_engine.frame_gate = 0;
            spin_unlock(&rdx_record_engine.lock);
            rdx_record_engine_wake();
            return 0;
        }
        /* Keep capture aligned with the newest audio.  A temporary
         * destination/transport stall may drop an encoded frame, but must
         * not stop the USB call recording session. */
        rdx_record_engine.overflow_count++;
        rdx_record_engine.candidate_head =
            (rdx_record_engine.candidate_head + 1)
            % RDX_RECORD_CANDIDATE_QUEUE_DEPTH;
        rdx_record_engine.candidate_count--;
    }

    tail = rdx_record_engine.candidate_tail;
    candidate = &rdx_record_engine.candidates[tail];
    candidate->engine_generation = engine_generation;
    candidate->session_id = session_id;
    candidate->capture_generation = capture_generation;
    candidate->len = len;
    memcpy(candidate->payload, data, len);
    rdx_record_engine.candidate_tail =
        (tail + 1) % RDX_RECORD_CANDIDATE_QUEUE_DEPTH;
    rdx_record_engine.candidate_count++;
    rdx_record_engine.enqueued_count++;
    spin_unlock(&rdx_record_engine.lock);
    rdx_record_engine_wake();
    return 0;
}

static uint8_t rdx_record_engine_format_supported(
    enum rdx_record_scene scene, enum rdx_record_format_id format_id)
{
    switch (scene) {
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    case RDX_RECORD_SCENE_MEETING:
        return format_id == RDX_RECORD_FORMAT_MEETING_V1;
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
    case RDX_RECORD_SCENE_CALL:
        return format_id == RDX_RECORD_FORMAT_CALL_V1;
#endif
    default:
        return 0;
    }
}

static uint8_t rdx_record_engine_uses_capture_lease(void)
{
    return rdx_record_engine.session.scene == RDX_RECORD_SCENE_MEETING;
}

static enum rdx_audio_capture_result rdx_record_engine_capture_readiness(void)
{
    return rdx_audio_capture_check_readiness(
        rdx_record_engine.session.scene,
        rdx_record_engine.session.format_id);
}

static enum rdx_audio_capture_result rdx_record_engine_capture_open(
    const struct rdx_audio_capture_open_params *params, uint8_t resume)
{
    return resume ? rdx_audio_capture_resume(params)
                  : rdx_audio_capture_open(params);
}

static void rdx_record_engine_capture_close(uint8_t pause)
{
    if (pause) {
        rdx_audio_capture_pause(rdx_record_engine.session.scene);
    } else {
        rdx_audio_capture_close(rdx_record_engine.session.scene);
    }
}

static void rdx_record_engine_release_capture_lease(void)
{
    if (rdx_record_engine_uses_capture_lease()) {
        audio_capture_lease_release(&rdx_record_engine.capture_lease);
    }
}

static void rdx_record_engine_audio_fault(
    void *priv, enum rdx_record_cause cause)
{
    if (priv != &rdx_record_engine) {
        return;
    }
    rdx_record_engine_submit_system_event(
        cause == RDX_RECORD_CAUSE_SOURCE_LOST
        ? RDX_RECORD_SYSTEM_SOURCE_USB
        : RDX_RECORD_SYSTEM_SOURCE_ENGINE,
        cause == RDX_RECORD_CAUSE_SOURCE_LOST
        ? RDX_RECORD_SYSTEM_SOURCE_LOST
        : RDX_RECORD_SYSTEM_ENGINE_FAULT,
        cause);
}

static void rdx_record_engine_stop(enum rdx_record_termination_mode mode,
                                   enum rdx_record_cause cause,
                                   uint32_t request_id,
                                   enum rdx_record_result result)
{
    uint8_t was_running = rdx_record_engine.session.state != RDX_RECORD_STATE_IDLE;
    enum rdx_record_cause completion_cause = cause;
    enum rdx_record_result completion_result = result;
    int destination_ret = 0;

    if (mode == RDX_RECORD_TERMINATION_FORCED) {
        spin_lock(&rdx_record_engine.lock);
        memset(&rdx_record_engine.stop_latch, 0,
               sizeof(rdx_record_engine.stop_latch));
        spin_unlock(&rdx_record_engine.lock);
    }
    if (!was_running) {
        if (mode == RDX_RECORD_TERMINATION_NORMAL) {
            rdx_record_engine_emit(RDX_RECORD_ENGINE_SESSION_STOPPED,
                                   request_id, result, mode, cause);
        }
        return;
    }

    rdx_record_engine.frame_gate = 0;
    rdx_record_session_begin_stop(&rdx_record_engine.session);
    rdx_record_engine_capture_close(0);
    if (mode == RDX_RECORD_TERMINATION_NORMAL
        && rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->seal) {
        struct rdx_record_candidate candidate;

        /* A destination that advertises seal owns the STOP cut.  Legacy
         * online destinations retain their old discard-on-stop path. */
        while (rdx_record_engine_take_candidate(&candidate)) {
            rdx_record_engine_process_candidate(&candidate);
            if (rdx_record_engine.session.state == RDX_RECORD_STATE_IDLE) {
                return;
            }
        }
        /* V23 record#0 acknowledges the capture state, not durable Artifact
         * commit.  All pre-cut candidates now belong to the Destination and
         * the audio source is closed, so Protocol may answer APP before the
         * potentially O(n) media verification below. */
        rdx_record_engine_emit(RDX_RECORD_ENGINE_STOP_ACK_READY,
                               request_id, result, mode, cause);
        destination_ret = rdx_record_engine.destination_ops->seal(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation);
        if (destination_ret) {
            /* seal() runs after the candidate queue has been drained.  A
             * failure here is an IO/commit error, never a capture queue
             * overflow; preserve that distinction for the APP contract. */
            completion_result = RDX_RECORD_RESULT_STORAGE_ERROR;
            completion_cause = RDX_RECORD_CAUSE_STORAGE_ERROR;
        }
    } else {
        rdx_record_engine.stop_discarded +=
            rdx_record_engine_discard_candidates();
    }

    if (mode == RDX_RECORD_TERMINATION_FORCED
        && rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->abort) {
        rdx_record_engine.destination_ops->abort(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation,
            cause);
    } else if (mode == RDX_RECORD_TERMINATION_NORMAL && destination_ret
               && rdx_record_engine.destination_ops
               && rdx_record_engine.destination_ops->abort) {
        rdx_record_engine.destination_ops->abort(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation,
            completion_cause);
    } else if ((!rdx_record_engine.destination_ops
                || !rdx_record_engine.destination_ops->seal
                || mode == RDX_RECORD_TERMINATION_FORCED
                || destination_ret)
               && rdx_record_engine.destination_ops
               && rdx_record_engine.destination_ops->cancel) {
        /* Compatibility path for the current Online destination. */
        rdx_record_engine.destination_ops->cancel(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation);
    }
    if (rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->detach_capture) {
        rdx_record_engine.destination_ops->detach_capture(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation);
    } else if (rdx_record_engine.destination_ops
               && rdx_record_engine.destination_ops->detach) {
        rdx_record_engine.destination_ops->detach(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation);
    }
    rdx_record_engine_release_capture_lease();
    rdx_record_session_complete_stop(&rdx_record_engine.session,
                                     mode, completion_cause);

    printf("[RDX][ENGINE] stopped session=%u capture=%u scene=%u gain_override=%u mic_gain=%u mode=%u cause=%u"
           " produced=%u enqueued=%u overflow=%u mismatch=%u stale=%u"
           " stop_discarded=%u\n",
           (unsigned int)rdx_record_engine.session.session_id,
           (unsigned int)rdx_record_engine.session.capture_generation,
           rdx_record_engine.session.scene,
           rdx_record_engine.session_mic_gain_override_valid,
           rdx_record_engine.session_mic_gain, mode, completion_cause,
           (unsigned int)rdx_record_engine.produced_count,
           (unsigned int)rdx_record_engine.enqueued_count,
           (unsigned int)rdx_record_engine.overflow_count,
           (unsigned int)rdx_record_engine.format_mismatch_count,
           (unsigned int)rdx_record_engine.stale_count,
           (unsigned int)rdx_record_engine.stop_discarded);
    printf("[RDX][ENGINE] stop_account session=%u capture=%u"
           " warmup_discard=%u destination_out=%u\n",
           (unsigned int)rdx_record_engine.session.session_id,
           (unsigned int)rdx_record_engine.session.capture_generation,
           (unsigned int)rdx_record_engine.warmup_discard_count,
           (unsigned int)rdx_record_engine.session_frame_seq);
#if RDX_CFG_RECORD_PERF_DIAG_ENABLE
    printf("[RDX][ENGINE] wake post=%u coalesced=%u post_fail=%u"
           " pending=%u\n",
           (unsigned int)rdx_record_engine.wake_post_count,
           (unsigned int)rdx_record_engine.wake_coalesced_count,
           (unsigned int)rdx_record_engine.wake_post_fail_count,
           rdx_record_engine.worker_wake_pending);
#endif
    rdx_record_engine_emit(RDX_RECORD_ENGINE_SESSION_STOPPED,
                           request_id, completion_result, mode,
                           completion_cause);

    rdx_record_engine.destination_ops = NULL;
    rdx_record_engine.destination_priv = NULL;
    rdx_record_session_release_owner(&rdx_record_engine.session);
    rdx_record_engine.session_mic_gain_override_valid = 0;
    rdx_record_engine.session_mic_gain = 0;
}

static void rdx_record_engine_revoke(void *priv, uint32_t preempt_token,
                                     uint32_t lease_generation)
{
    struct rdx_record_engine_runtime *engine = priv;

    spin_lock(&engine->lock);
    if (engine->capture_lease.held
        && engine->capture_lease.generation == lease_generation) {
        engine->system_latch.pending = 1;
        engine->system_latch.source = RDX_RECORD_SYSTEM_SOURCE_USB;
        engine->system_latch.type = RDX_RECORD_SYSTEM_USB_PREEMPT;
        engine->system_latch.cause = RDX_RECORD_CAUSE_USB_PREEMPT;
        engine->frame_gate = 0;
    }
    spin_unlock(&engine->lock);
    printf("[RDX][ENGINE] revoke token=%u lease=%u\n",
           (unsigned int)preempt_token, (unsigned int)lease_generation);
    rdx_record_engine_wake();
}

static const struct audio_capture_lease_client rdx_record_capture_client = {
    .name = "record_engine",
    .priority = 10,
    .revoke = rdx_record_engine_revoke,
    .priv = &rdx_record_engine,
};

static void rdx_record_engine_start(struct rdx_record_request *request)
{
    struct rdx_audio_capture_open_params audio_params;
    enum rdx_audio_capture_result capture_result;
    enum rdx_record_result failure_result = RDX_RECORD_RESULT_START_FAILED;
    enum rdx_record_termination_mode failure_mode =
        RDX_RECORD_TERMINATION_FORCED;
    enum rdx_record_cause failure_cause = RDX_RECORD_CAUSE_START_FAILED;
    enum rdx_record_result validation_result;
    uint8_t cancelled;
    uint8_t destination_attached = 0;
    uint8_t recorder_open_attempted = 0;
    int ret;

    validation_result = rdx_record_session_validate_start(
        &rdx_record_engine.session,
        rdx_record_engine_format_supported(request->scene,
                                           request->format_id));
    if (validation_result != RDX_RECORD_RESULT_OK) {
        rdx_record_engine_emit(RDX_RECORD_ENGINE_START_COMPLETE,
                               request->request_id,
                               validation_result,
                               RDX_RECORD_TERMINATION_NONE,
                               RDX_RECORD_CAUSE_NONE);
        return;
    }

    if (request->prepare_start) {
        printf("[RDX][ENGINE] start_prepare request=%u\n",
               (unsigned int)request->request_id);
        validation_result = request->prepare_start(request->prepare_priv,
                                                   request);
        if (validation_result != RDX_RECORD_RESULT_OK) {
            printf("[RDX][ENGINE] start_prepare_failed request=%u result=%u\n",
                   (unsigned int)request->request_id,
                   validation_result);
            rdx_record_engine_emit(RDX_RECORD_ENGINE_START_COMPLETE,
                                   request->request_id,
                                   validation_result,
                                   RDX_RECORD_TERMINATION_NONE,
                                   RDX_RECORD_CAUSE_STORAGE_ERROR);
            return;
        }
    }

    rdx_record_session_begin_start(&rdx_record_engine.session, request);
    rdx_record_engine.destination_ops = request->destination_ops;
    rdx_record_engine.destination_priv = request->destination_priv;
    rdx_record_engine.session_frame_seq = 0;
    rdx_record_engine.capture_frame_seq = 0;
    rdx_record_engine.warmup_count = 0;
    rdx_record_engine.warmup_discard_count = 0;
    rdx_record_engine.produced_count = 0;
    rdx_record_engine.enqueued_count = 0;
    rdx_record_engine.overflow_count = 0;
    rdx_record_engine.stale_count = 0;
    rdx_record_engine.stop_discarded = 0;
    rdx_record_engine.format_mismatch_count = 0;
#if RDX_CFG_RECORD_PERF_DIAG_ENABLE
    rdx_record_engine.wake_post_count = 0;
    rdx_record_engine.wake_coalesced_count = 0;
    rdx_record_engine.wake_post_fail_count = 0;
#endif
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
    rdx_record_engine.first_frame_after_resume = 0;
#endif
    rdx_record_engine_clear_queues();

    rdx_record_engine.session_mic_gain_override_valid = 0;
    rdx_record_engine.session_mic_gain = 0;
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    if (rdx_record_engine.session.scene == RDX_RECORD_SCENE_MEETING) {
        ret = rdx_mic_gain_get_override(
            RDX_MIC_GAIN_MODE_CONFERENCE,
            &rdx_record_engine.session_mic_gain_override_valid,
            &rdx_record_engine.session_mic_gain);
        if (ret) {
            printf("[RDX][ENGINE] start_failed request=%u reason=mic_gain_resolve ret=%d\n",
                   (unsigned int)request->request_id, ret);
            goto start_failed;
        }
    }
#endif
    printf("[RDX][ENGINE] session_gain_snapshot request=%u session=%u scene=%u override=%u gain=%u\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session.session_id,
           rdx_record_engine.session.scene,
           rdx_record_engine.session_mic_gain_override_valid,
           rdx_record_engine.session_mic_gain);

    capture_result = rdx_record_engine_capture_readiness();
    if (capture_result != RDX_AUDIO_CAPTURE_OK) {
        failure_result = RDX_RECORD_RESULT_SOURCE_LOST;
        failure_cause = RDX_RECORD_CAUSE_SOURCE_LOST;
        goto rollback;
    }

    if (rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->attach) {
        ret = rdx_record_engine.destination_ops->attach(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation);
        if (ret) {
            goto start_failed;
        }
        destination_attached = 1;
    }

    if (rdx_record_engine_uses_capture_lease()) {
        ret = audio_capture_lease_acquire(&rdx_record_capture_client,
                                          &rdx_record_engine.capture_lease,
                                          0, 0);
        if (ret) {
            failure_result = RDX_RECORD_RESULT_BUSY;
            failure_mode = RDX_RECORD_TERMINATION_NONE;
            failure_cause = RDX_RECORD_CAUSE_NONE;
            goto rollback;
        }
    }

    spin_lock(&rdx_record_engine.lock);
    cancelled = rdx_record_engine.stop_latch.pending
                || rdx_record_engine.system_latch.pending;
    if (rdx_record_engine.system_latch.pending) {
        failure_cause = rdx_record_engine.system_latch.cause;
    } else if (rdx_record_engine.stop_latch.pending) {
        failure_cause = rdx_record_engine.stop_latch.cause;
    }
    if (!cancelled) {
        rdx_record_engine.frame_gate = 1;
    }
    spin_unlock(&rdx_record_engine.lock);
    if (cancelled) {
        failure_result = RDX_RECORD_RESULT_CANCELLED;
        goto rollback;
    }

    memset(&audio_params, 0, sizeof(audio_params));
    audio_params.scene = rdx_record_engine.session.scene;
    audio_params.format_id = rdx_record_engine.session.format_id;
    audio_params.mic_gain_override_valid =
        rdx_record_engine.session_mic_gain_override_valid;
    audio_params.mic_gain = rdx_record_engine.session_mic_gain;
    audio_params.engine_generation = rdx_record_engine.engine_generation;
    audio_params.session_id = rdx_record_engine.session.session_id;
    audio_params.capture_generation =
        rdx_record_engine.session.capture_generation;
    audio_params.frame_callback = rdx_record_engine_audio_frame;
    audio_params.fault_callback = rdx_record_engine_audio_fault;
    audio_params.frame_priv = &rdx_record_engine;
    recorder_open_attempted = 1;
    capture_result = rdx_record_engine_capture_open(&audio_params, 0);
    if (capture_result != RDX_AUDIO_CAPTURE_OK) {
        rdx_record_engine.frame_gate = 0;
        if (capture_result == RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE) {
            failure_result = RDX_RECORD_RESULT_SOURCE_LOST;
            failure_cause = RDX_RECORD_CAUSE_SOURCE_LOST;
        }
        goto start_failed;
    }
    spin_lock(&rdx_record_engine.lock);
    cancelled = !rdx_record_engine.frame_gate
                || rdx_record_engine.stop_latch.pending
                || rdx_record_engine.system_latch.pending;
    if (rdx_record_engine.system_latch.pending) {
        failure_cause = rdx_record_engine.system_latch.cause;
    } else if (rdx_record_engine.stop_latch.pending) {
        failure_cause = rdx_record_engine.stop_latch.cause;
    }
    if (!cancelled) {
        rdx_record_session_complete_start(&rdx_record_engine.session);
    }
    spin_unlock(&rdx_record_engine.lock);
    if (cancelled) {
        failure_result = RDX_RECORD_RESULT_CANCELLED;
        goto rollback;
    }
    printf("[RDX][ENGINE] started request=%u session=%u capture=%u"
           " scene=%u format=opus_16k_mono_v1 gain_override=%u mic_gain=%u\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session.session_id,
           (unsigned int)rdx_record_engine.session.capture_generation,
           rdx_record_engine.session.scene,
           rdx_record_engine.session_mic_gain_override_valid,
           rdx_record_engine.session_mic_gain);
    rdx_record_engine_emit(RDX_RECORD_ENGINE_START_COMPLETE,
                           request->request_id, RDX_RECORD_RESULT_OK,
                           RDX_RECORD_TERMINATION_NONE,
                           RDX_RECORD_CAUSE_NONE);
    return;

start_failed:
rollback:
    rdx_record_engine.frame_gate = 0;
    if (recorder_open_attempted) {
        rdx_record_engine_capture_close(0);
    }
    rdx_record_engine_clear_queues();
    rdx_record_engine_release_capture_lease();
    if (rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->requirement
        == RDX_RECORD_DESTINATION_REQUIRED
        && rdx_record_engine.destination_ops->abort) {
        /* A REQUIRED destination may have allocated persistent state before
         * capture open completed.  Abort it even when attach was reached only
         * partially, so a failed START cannot strand an active artifact. */
        rdx_record_engine.destination_ops->abort(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session.session_id,
            rdx_record_engine.session.capture_generation,
            failure_cause);
    } else if (destination_attached && rdx_record_engine.destination_ops
               && rdx_record_engine.destination_ops->detach) {
        rdx_record_engine.destination_ops->detach(
                rdx_record_engine.destination_priv,
                rdx_record_engine.session.session_id,
                rdx_record_engine.session.capture_generation);
    }
    rdx_record_session_abort_start(&rdx_record_engine.session);
    rdx_record_engine_emit(RDX_RECORD_ENGINE_START_COMPLETE,
                           request->request_id,
                           failure_result,
                           failure_mode,
                           failure_cause);
    rdx_record_engine.destination_ops = NULL;
    rdx_record_engine.destination_priv = NULL;
    rdx_record_session_release_owner(&rdx_record_engine.session);
    rdx_record_engine.session_mic_gain_override_valid = 0;
    rdx_record_engine.session_mic_gain = 0;
}

static uint8_t rdx_record_engine_take_system(
    struct rdx_record_system_latch *event)
{
    uint8_t taken = 0;

    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.system_latch.pending) {
        *event = rdx_record_engine.system_latch;
        memset(&rdx_record_engine.system_latch, 0,
               sizeof(rdx_record_engine.system_latch));
        taken = 1;
    }
    spin_unlock(&rdx_record_engine.lock);
    return taken;
}

static uint8_t rdx_record_engine_take_stop(struct rdx_record_stop_latch *stop)
{
    uint8_t taken = 0;

    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.stop_latch.pending) {
        *stop = rdx_record_engine.stop_latch;
        memset(&rdx_record_engine.stop_latch, 0,
               sizeof(rdx_record_engine.stop_latch));
        taken = 1;
    }
    spin_unlock(&rdx_record_engine.lock);
    return taken;
}

static uint8_t rdx_record_engine_take_request(struct rdx_record_request *request)
{
    uint8_t taken = 0;

    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.request_count) {
        *request = rdx_record_engine.requests[rdx_record_engine.request_head];
        rdx_record_engine.request_head =
            (rdx_record_engine.request_head + 1)
            % RDX_RECORD_CONTROL_QUEUE_DEPTH;
        rdx_record_engine.request_count--;
        taken = 1;
    }
    spin_unlock(&rdx_record_engine.lock);
    return taken;
}

static uint8_t rdx_record_engine_take_destination_ready(
    struct rdx_record_destination_latch *ready)
{
    uint8_t taken = 0;

    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.destination_latch.pending) {
        *ready = rdx_record_engine.destination_latch;
        memset(&rdx_record_engine.destination_latch, 0,
               sizeof(rdx_record_engine.destination_latch));
        taken = 1;
    }
    spin_unlock(&rdx_record_engine.lock);
    return taken;
}

static void rdx_record_engine_cancel_requests(
    enum rdx_record_termination_mode termination_mode,
    enum rdx_record_cause cause)
{
    struct rdx_record_request request;

    while (rdx_record_engine_take_request(&request)) {
        enum rdx_record_engine_event_type type;

        switch (request.type) {
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
        case RDX_RECORD_REQUEST_PAUSE:
            type = RDX_RECORD_ENGINE_PAUSE_COMPLETE;
            break;
        case RDX_RECORD_REQUEST_RESUME:
            type = RDX_RECORD_ENGINE_RESUME_COMPLETE;
            break;
#endif
#if RDX_CFG_RECORD_MARK_ENABLE
        case RDX_RECORD_REQUEST_MARK:
            rdx_record_engine_emit_mark(&request,
                                        RDX_RECORD_RESULT_CANCELLED);
            continue;
#endif
        case RDX_RECORD_REQUEST_START:
        default:
            type = RDX_RECORD_ENGINE_START_COMPLETE;
            break;
        }
        rdx_record_engine_emit(type, request.request_id,
                               RDX_RECORD_RESULT_CANCELLED,
                               termination_mode, cause);
    }
}

static uint8_t rdx_record_engine_take_candidate(
    struct rdx_record_candidate *candidate)
{
    uint8_t taken = 0;

    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.candidate_count) {
        *candidate =
            rdx_record_engine.candidates[rdx_record_engine.candidate_head];
        rdx_record_engine.candidate_head =
            (rdx_record_engine.candidate_head + 1)
            % RDX_RECORD_CANDIDATE_QUEUE_DEPTH;
        rdx_record_engine.candidate_count--;
        taken = 1;
    }
    spin_unlock(&rdx_record_engine.lock);
    return taken;
}

static void rdx_record_engine_process_candidate(
    const struct rdx_record_candidate *candidate)
{
    const struct rdx_record_format_spec *format =
        rdx_record_engine_format_spec(rdx_record_engine.session.format_id);
    struct rdx_record_frame frame;
    int ret;

    if (candidate->engine_generation != rdx_record_engine.engine_generation
        || candidate->session_id != rdx_record_engine.session.session_id
        || candidate->capture_generation !=
        rdx_record_engine.session.capture_generation
        || (rdx_record_engine.session.state != RDX_RECORD_STATE_ACTIVE
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
            && rdx_record_engine.session.state
            != RDX_RECORD_STATE_PAUSING_BY_USER
#endif
            && rdx_record_engine.session.state != RDX_RECORD_STATE_STOPPING
            )) {
        rdx_record_engine.stale_count++;
        return;
    }
    if (!format || candidate->len != format->payload_size
        || (candidate->payload[0] & BIT(2))) {
        rdx_record_engine.format_mismatch_count++;
        rdx_record_engine_stop(RDX_RECORD_TERMINATION_FORCED,
                               RDX_RECORD_CAUSE_FORMAT_MISMATCH, 0,
                               RDX_RECORD_RESULT_FORMAT_MISMATCH);
        return;
    }
    if (rdx_record_engine.warmup_count < format->warmup_frames) {
        rdx_record_engine.warmup_count++;
        rdx_record_engine.warmup_discard_count++;
        return;
    }

    memset(&frame, 0, sizeof(frame));
    frame.session_id = rdx_record_engine.session.session_id;
    frame.capture_generation = rdx_record_engine.session.capture_generation;
    frame.session_frame_seq = rdx_record_engine.session_frame_seq++;
    frame.capture_frame_seq = rdx_record_engine.capture_frame_seq++;
    frame.active_pts = (uint64_t)frame.session_frame_seq * format->frame_ms;
    frame.duration = format->frame_ms;
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
    if (rdx_record_engine.first_frame_after_resume) {
        frame.flags |= RDX_RECORD_FRAME_FIRST_AFTER_RESUME;
        rdx_record_engine.first_frame_after_resume = 0;
    }
#endif
    frame.payload = candidate->payload;
    frame.payload_len = candidate->len;
    if (!rdx_record_engine.destination_ops
        || !rdx_record_engine.destination_ops->consume) {
        return;
    }
    ret = rdx_record_engine.destination_ops->consume(
        rdx_record_engine.destination_priv, &frame);
    if (ret) {
        if (ret == RDX_RECORD_DESTINATION_ERROR_STORAGE) {
            rdx_record_engine_stop(RDX_RECORD_TERMINATION_FORCED,
                                   RDX_RECORD_CAUSE_STORAGE_ERROR, 0,
                                   RDX_RECORD_RESULT_STORAGE_ERROR);
        } else {
            rdx_record_engine_stop(RDX_RECORD_TERMINATION_FORCED,
                                   RDX_RECORD_CAUSE_QUEUE_FULL, 0,
                                   RDX_RECORD_RESULT_QUEUE_FULL);
        }
    }
}

#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
static void rdx_record_engine_pause(const struct rdx_record_request *request)
{
    const struct rdx_record_format_spec *format =
        rdx_record_engine_format_spec(rdx_record_engine.session.format_id);
    struct rdx_record_candidate candidate;
    enum rdx_record_result validation_result;

    validation_result = rdx_record_session_validate_control(
        &rdx_record_engine.session, request, RDX_RECORD_STATE_ACTIVE);
    if (validation_result != RDX_RECORD_RESULT_OK) {
        rdx_record_engine_emit(RDX_RECORD_ENGINE_PAUSE_COMPLETE,
                               request->request_id,
                               validation_result,
                               RDX_RECORD_TERMINATION_NONE,
                               RDX_RECORD_CAUSE_NONE);
        return;
    }

    spin_lock(&rdx_record_engine.lock);
    rdx_record_engine.frame_gate = 0;
    rdx_record_session_begin_pause(&rdx_record_engine.session);
    spin_unlock(&rdx_record_engine.lock);

    rdx_record_engine_capture_close(1);
    while (rdx_record_engine_take_candidate(&candidate)) {
        rdx_record_engine_process_candidate(&candidate);
        if (rdx_record_engine.session.state == RDX_RECORD_STATE_IDLE) {
            return;
        }
    }
    rdx_record_engine_release_capture_lease();
    rdx_record_session_complete_pause(&rdx_record_engine.session);
    printf("[RDX][ENGINE] paused request=%u session=%u capture=%u scene=%u"
           " active_pts=%u\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session.session_id,
           (unsigned int)rdx_record_engine.session.capture_generation,
           rdx_record_engine.session.scene,
           (unsigned int)(rdx_record_engine.session_frame_seq
                          * (format ? format->frame_ms : 0)));
    rdx_record_engine_emit(RDX_RECORD_ENGINE_PAUSE_COMPLETE,
                           request->request_id, RDX_RECORD_RESULT_OK,
                           RDX_RECORD_TERMINATION_NONE,
                           RDX_RECORD_CAUSE_NONE);
}

static void rdx_record_engine_resume(const struct rdx_record_request *request)
{
    struct rdx_audio_capture_open_params audio_params;
    enum rdx_audio_capture_result capture_result;
    enum rdx_record_result validation_result;
    uint32_t previous_capture_generation;
    uint8_t recorder_open_attempted = 0;
    int ret;

    validation_result = rdx_record_session_validate_control(
        &rdx_record_engine.session, request,
        RDX_RECORD_STATE_PAUSED_BY_USER);
    if (validation_result != RDX_RECORD_RESULT_OK) {
        rdx_record_engine_emit(RDX_RECORD_ENGINE_RESUME_COMPLETE,
                               request->request_id,
                               validation_result,
                               RDX_RECORD_TERMINATION_NONE,
                               RDX_RECORD_CAUSE_NONE);
        return;
    }

    previous_capture_generation =
        rdx_record_engine.session.capture_generation;
    rdx_record_session_begin_resume(&rdx_record_engine.session);
    rdx_record_engine.capture_frame_seq = 0;
    rdx_record_engine.warmup_count = 0;
    rdx_record_engine_clear_queues();

    if (rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->rebind_capture
        && rdx_record_engine.destination_ops->rebind_capture(
               rdx_record_engine.destination_priv,
               rdx_record_engine.session.session_id,
               previous_capture_generation,
               rdx_record_engine.session.capture_generation)) {
        /* rebind_capture is atomic by contract.  Restore the live binding
         * while retaining next_capture_generation's monotonic high-water. */
        rdx_record_engine.session.capture_generation =
            previous_capture_generation;
        ret = -1;
        printf("[RDX][ENGINE] resume_rebind_failed request=%u session=%u"
               " capture=%u\n",
               (unsigned int)request->request_id,
               (unsigned int)rdx_record_engine.session.session_id,
               (unsigned int)previous_capture_generation);
        goto resume_failed;
    }

    capture_result = rdx_record_engine_capture_readiness();
    if (capture_result != RDX_AUDIO_CAPTURE_OK) {
        ret = capture_result;
        goto resume_failed;
    }
    if (rdx_record_engine_uses_capture_lease()) {
        ret = audio_capture_lease_acquire(&rdx_record_capture_client,
                                          &rdx_record_engine.capture_lease,
                                          0, 0);
        if (ret) {
            goto resume_failed;
        }
    }

    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.stop_latch.pending
        || rdx_record_engine.system_latch.pending) {
        spin_unlock(&rdx_record_engine.lock);
        ret = -1;
        goto release_capture;
    }
    rdx_record_engine.frame_gate = 1;
    spin_unlock(&rdx_record_engine.lock);

    memset(&audio_params, 0, sizeof(audio_params));
    audio_params.scene = rdx_record_engine.session.scene;
    audio_params.format_id = rdx_record_engine.session.format_id;
    audio_params.mic_gain_override_valid =
        rdx_record_engine.session_mic_gain_override_valid;
    audio_params.mic_gain = rdx_record_engine.session_mic_gain;
    audio_params.engine_generation = rdx_record_engine.engine_generation;
    audio_params.session_id = rdx_record_engine.session.session_id;
    audio_params.capture_generation = rdx_record_engine.session.capture_generation;
    audio_params.frame_callback = rdx_record_engine_audio_frame;
    audio_params.fault_callback = rdx_record_engine_audio_fault;
    audio_params.frame_priv = &rdx_record_engine;
    recorder_open_attempted = 1;
    capture_result = rdx_record_engine_capture_open(&audio_params, 1);
    if (capture_result != RDX_AUDIO_CAPTURE_OK) {
        ret = capture_result;
        rdx_record_engine.frame_gate = 0;
        goto release_capture;
    }

    rdx_record_engine.first_frame_after_resume = 1;
    rdx_record_session_complete_resume(&rdx_record_engine.session);
    printf("[RDX][ENGINE] resumed request=%u session=%u capture=%u scene=%u gain_override=%u mic_gain=%u\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session.session_id,
           (unsigned int)rdx_record_engine.session.capture_generation,
           rdx_record_engine.session.scene,
           rdx_record_engine.session_mic_gain_override_valid,
           rdx_record_engine.session_mic_gain);
    rdx_record_engine_emit(RDX_RECORD_ENGINE_RESUME_COMPLETE,
                           request->request_id, RDX_RECORD_RESULT_OK,
                           RDX_RECORD_TERMINATION_NONE,
                           RDX_RECORD_CAUSE_NONE);
    return;

release_capture:
    if (recorder_open_attempted) {
        rdx_record_engine_capture_close(0);
    }
    rdx_record_engine_release_capture_lease();
resume_failed:
    rdx_record_engine.frame_gate = 0;
    rdx_record_engine_clear_queues();
    if (ret == RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE) {
        printf("[RDX][ENGINE] resume_source_lost request=%u session=%u\n",
               (unsigned int)request->request_id,
               (unsigned int)rdx_record_engine.session.session_id);
        rdx_record_engine_stop(RDX_RECORD_TERMINATION_FORCED,
                               RDX_RECORD_CAUSE_SOURCE_LOST,
                               request->request_id,
                               RDX_RECORD_RESULT_SOURCE_LOST);
        return;
    }
    rdx_record_session_abort_resume(&rdx_record_engine.session);
    printf("[RDX][ENGINE] resume_failed request=%u session=%u ret=%d\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session.session_id, ret);
    rdx_record_engine_emit(RDX_RECORD_ENGINE_RESUME_COMPLETE,
                           request->request_id,
                           ret == RDX_AUDIO_CAPTURE_BUSY
                           ? RDX_RECORD_RESULT_BUSY
                           : ret == RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE
                             ? RDX_RECORD_RESULT_SOURCE_LOST
                             : RDX_RECORD_RESULT_START_FAILED,
                           RDX_RECORD_TERMINATION_NONE,
                           ret == RDX_AUDIO_CAPTURE_SOURCE_UNAVAILABLE
                           ? RDX_RECORD_CAUSE_SOURCE_LOST
                           : RDX_RECORD_CAUSE_START_FAILED);
}
#endif

#if RDX_CFG_RECORD_MARK_ENABLE
static void rdx_record_engine_mark(const struct rdx_record_request *request)
{
    enum rdx_record_result result = rdx_record_session_validate_control(
        &rdx_record_engine.session, request, RDX_RECORD_STATE_ACTIVE);

    rdx_record_engine_emit_mark(request, result);
}
#endif

static uint8_t rdx_record_engine_process_one(void)
{
    struct rdx_record_system_latch system_event;
    struct rdx_record_stop_latch stop;
    struct rdx_record_destination_latch destination_ready;
    struct rdx_record_request request;
    struct rdx_record_candidate candidate;

    if (rdx_record_engine_take_system(&system_event)) {
        enum rdx_record_result result;

        rdx_record_engine_cancel_requests(RDX_RECORD_TERMINATION_FORCED,
                                          system_event.cause);
        switch (system_event.cause) {
        case RDX_RECORD_CAUSE_USB_PREEMPT:
            result = RDX_RECORD_RESULT_RESOURCE_PREEMPTED;
            break;
        case RDX_RECORD_CAUSE_LINK_LOST:
        case RDX_RECORD_CAUSE_CCC_OFF:
            result = RDX_RECORD_RESULT_LINK_LOST;
            break;
        case RDX_RECORD_CAUSE_QUEUE_FULL:
            result = RDX_RECORD_RESULT_QUEUE_FULL;
            break;
        case RDX_RECORD_CAUSE_FORMAT_MISMATCH:
            result = RDX_RECORD_RESULT_FORMAT_MISMATCH;
            break;
        case RDX_RECORD_CAUSE_SOURCE_LOST:
            result = RDX_RECORD_RESULT_SOURCE_LOST;
            break;
        case RDX_RECORD_CAUSE_START_FAILED:
            result = RDX_RECORD_RESULT_START_FAILED;
            break;
        default:
            result = RDX_RECORD_RESULT_CANCELLED;
            break;
        }
        rdx_record_engine_stop(RDX_RECORD_TERMINATION_FORCED,
                               system_event.cause, 0, result);
        if (system_event.type == RDX_RECORD_SYSTEM_SHUTDOWN) {
            rdx_record_engine.accepting = 0;
            os_sem_post(&rdx_record_engine.stopped_sem);
        }
        return 1;
    }
    if (rdx_record_engine_take_stop(&stop)) {
        enum rdx_record_result validation_result =
            rdx_record_session_validate_stop(
                &rdx_record_engine.session, stop.controller_id);

        if (validation_result != RDX_RECORD_RESULT_OK) {
            rdx_record_engine_emit(RDX_RECORD_ENGINE_SESSION_STOPPED,
                                   stop.request_id,
                                   validation_result,
                                   RDX_RECORD_TERMINATION_NONE,
                                   RDX_RECORD_CAUSE_NONE);
        } else {
            rdx_record_engine_cancel_requests(
                RDX_RECORD_TERMINATION_NORMAL, stop.cause);
            rdx_record_engine_stop(RDX_RECORD_TERMINATION_NORMAL,
                                   stop.cause, stop.request_id,
                                   RDX_RECORD_RESULT_OK);
        }
        return 1;
    }
    if (rdx_record_engine_take_request(&request)) {
        if (request.type == RDX_RECORD_REQUEST_START) {
            rdx_record_engine_start(&request);
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
        } else if (request.type == RDX_RECORD_REQUEST_PAUSE) {
            rdx_record_engine_pause(&request);
        } else if (request.type == RDX_RECORD_REQUEST_RESUME) {
            rdx_record_engine_resume(&request);
#endif
#if RDX_CFG_RECORD_MARK_ENABLE
        } else if (request.type == RDX_RECORD_REQUEST_MARK) {
            rdx_record_engine_mark(&request);
#endif
        }
        return 1;
    }
    if (rdx_record_engine_take_destination_ready(&destination_ready)) {
        if (rdx_record_engine.session.state == RDX_RECORD_STATE_ACTIVE
            && destination_ready.session_id == rdx_record_engine.session.session_id
            && destination_ready.capture_generation ==
                rdx_record_engine.session.capture_generation
            && rdx_record_engine.destination_ops
            && rdx_record_engine.destination_ops->poll) {
            rdx_record_engine.destination_ops->poll(
                rdx_record_engine.destination_priv,
                destination_ready.session_id,
                destination_ready.capture_generation);
        }
        return 1;
    }
    if (rdx_record_engine_take_candidate(&candidate)) {
        rdx_record_engine_process_candidate(&candidate);
        return 1;
    }
    return 0;
}

static void rdx_record_engine_task(void *priv)
{
    int msg[8];

    (void)priv;
    rdx_record_engine.task_running = 1;
    while (rdx_record_engine.initialized) {
        while (rdx_record_engine_process_one()) {
        }
        if (!rdx_record_engine.initialized) {
            break;
        }
        os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        /* Clear the edge before processing.  A producer arriving while the
         * worker drains current work can then enqueue exactly one successor
         * wake; producers in the same burst coalesce behind that edge. */
        spin_lock(&rdx_record_engine.lock);
        rdx_record_engine.worker_wake_pending = 0;
        spin_unlock(&rdx_record_engine.lock);
    }
    rdx_record_engine.task_running = 0;
}

int rdx_record_engine_init(rdx_record_engine_event_callback_t callback,
                           void *callback_priv)
{
    uint32_t generation;
    int ret;

    if (rdx_record_engine.initialized) {
        return 0;
    }
    generation = rdx_record_engine_next_generation(
        rdx_record_engine.engine_generation);
    memset(&rdx_record_engine, 0, sizeof(rdx_record_engine));
    rdx_record_engine.engine_generation = generation;
    rdx_record_session_init(&rdx_record_engine.session);
    rdx_record_engine.event_callback = callback;
    rdx_record_engine.event_priv = callback_priv;
    spin_lock_init(&rdx_record_engine.lock);
    os_sem_create(&rdx_record_engine.stopped_sem, 0);
    rdx_record_engine.accepting = 1;
    rdx_record_engine.initialized = 1;
    ret = task_create(rdx_record_engine_task, NULL, RDX_RECORD_TASK_NAME);
    if (ret) {
        rdx_record_engine.initialized = 0;
        return ret;
    }
    rdx_record_engine.worker_created = 1;
    return 0;
}

int rdx_record_engine_submit(const struct rdx_record_request *request)
{
    uint8_t tail;

    if (!request || !request->request_id) {
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    spin_lock(&rdx_record_engine.lock);
    if (!rdx_record_engine.accepting) {
        spin_unlock(&rdx_record_engine.lock);
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    if (request->type == RDX_RECORD_REQUEST_STOP) {
        if (!rdx_record_engine.stop_latch.pending) {
            rdx_record_engine.stop_latch.pending = 1;
            rdx_record_engine.stop_latch.request_id = request->request_id;
            rdx_record_engine.stop_latch.controller_id =
                request->controller_id;
            rdx_record_engine.stop_latch.cause =
                RDX_RECORD_CAUSE_USER_REQUEST;
            rdx_record_engine.frame_gate = 0;
        } else {
            rdx_record_engine.stop_latch.last_request_id =
                request->request_id;
            if (rdx_record_engine.stop_latch.duplicate_count != 0xffff) {
                rdx_record_engine.stop_latch.duplicate_count++;
            }
        }
        spin_unlock(&rdx_record_engine.lock);
        rdx_record_engine_wake();
        return RDX_RECORD_RESULT_OK;
    }
    if (request->type != RDX_RECORD_REQUEST_START
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
        && request->type != RDX_RECORD_REQUEST_PAUSE
        && request->type != RDX_RECORD_REQUEST_RESUME
#endif
#if RDX_CFG_RECORD_MARK_ENABLE
        && request->type != RDX_RECORD_REQUEST_MARK) {
#else
        ) {
#endif
        spin_unlock(&rdx_record_engine.lock);
        return RDX_RECORD_RESULT_BAD_STATE;
    }

    if (rdx_record_engine.request_count >= RDX_RECORD_CONTROL_QUEUE_DEPTH) {
        spin_unlock(&rdx_record_engine.lock);
        return RDX_RECORD_RESULT_CONTROL_QUEUE_FULL;
    }
    tail = rdx_record_engine.request_tail;
    rdx_record_engine.requests[tail] = *request;
    rdx_record_engine.request_tail =
        (tail + 1) % RDX_RECORD_CONTROL_QUEUE_DEPTH;
    rdx_record_engine.request_count++;
    spin_unlock(&rdx_record_engine.lock);
    rdx_record_engine_wake();
    return RDX_RECORD_RESULT_OK;
}

int rdx_record_engine_submit_system_event(
    enum rdx_record_system_event_source source,
    enum rdx_record_system_event_type type,
    enum rdx_record_cause cause)
{
    if (!rdx_record_engine.initialized) {
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    spin_lock(&rdx_record_engine.lock);
    if (!rdx_record_engine.accepting
        && type != RDX_RECORD_SYSTEM_SHUTDOWN) {
        spin_unlock(&rdx_record_engine.lock);
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    if (!rdx_record_engine.system_latch.pending) {
        rdx_record_engine.system_latch.pending = 1;
        rdx_record_engine.system_latch.source = source;
        rdx_record_engine.system_latch.type = type;
        rdx_record_engine.system_latch.cause = cause;
        rdx_record_engine.frame_gate = 0;
    }
    spin_unlock(&rdx_record_engine.lock);
    rdx_record_engine_wake();
    return RDX_RECORD_RESULT_OK;
}

int rdx_record_engine_notify_destination_ready(
    uint32_t session_id, uint32_t capture_generation)
{
    uint8_t wake = 0;
    int result = RDX_RECORD_RESULT_BAD_STATE;

    if (!rdx_record_engine.initialized) {
        return result;
    }
    spin_lock(&rdx_record_engine.lock);
    if (rdx_record_engine.accepting
        && rdx_record_engine.session.state == RDX_RECORD_STATE_ACTIVE
        && rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->poll
        && session_id == rdx_record_engine.session.session_id
        && capture_generation == rdx_record_engine.session.capture_generation) {
        if (!rdx_record_engine.destination_latch.pending) {
            rdx_record_engine.destination_latch.pending = 1;
            rdx_record_engine.destination_latch.session_id = session_id;
            rdx_record_engine.destination_latch.capture_generation =
                capture_generation;
            wake = 1;
        }
        result = RDX_RECORD_RESULT_OK;
    }
    spin_unlock(&rdx_record_engine.lock);
    if (wake) {
        rdx_record_engine_wake();
    }
    return result;
}

int rdx_record_engine_shutdown(uint16_t timeout_ms)
{
    uint16_t timeout_ticks;
    int ret;

    if (!rdx_record_engine.initialized) {
        return 0;
    }
    if (!timeout_ms) {
        return RDX_RECORD_RESULT_BAD_STATE;
    }
    timeout_ticks = msecs_to_jiffies((uint32_t)timeout_ms - 1) + 1;
    spin_lock(&rdx_record_engine.lock);
    rdx_record_engine.accepting = 0;
    rdx_record_engine.system_latch.pending = 1;
    rdx_record_engine.system_latch.source =
        RDX_RECORD_SYSTEM_SOURCE_LIFECYCLE;
    rdx_record_engine.system_latch.type = RDX_RECORD_SYSTEM_SHUTDOWN;
    rdx_record_engine.system_latch.cause = RDX_RECORD_CAUSE_SHUTDOWN;
    rdx_record_engine.frame_gate = 0;
    os_sem_set(&rdx_record_engine.stopped_sem, 0);
    spin_unlock(&rdx_record_engine.lock);
    rdx_record_engine_wake();

    ret = os_sem_pend(&rdx_record_engine.stopped_sem, timeout_ticks);
    if (ret || rdx_record_engine.session.state != RDX_RECORD_STATE_IDLE) {
        printf("[RDX][ENGINE] shutdown_timeout ret=%d state=%u\n",
               ret, rdx_record_engine.session.state);
        return RDX_RECORD_RESULT_SHUTDOWN_TIMEOUT;
    }

    rdx_record_engine.initialized = 0;
    rdx_record_engine_wake();
    task_kill(RDX_RECORD_TASK_NAME);
    rdx_record_engine.worker_created = 0;
    rdx_record_engine.event_callback = NULL;
    rdx_record_engine.event_priv = NULL;
    return 0;
}

enum rdx_record_state rdx_record_engine_get_state(void)
{
    return rdx_record_engine.session.state;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE */
