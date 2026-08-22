#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE

#include "system/includes.h"
#include "spinlock.h"
#include "audio_capture_lease.h"
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
#include "rdx_mic_gain_service.h"
#include "rdx_record_audio_br56.h"
#endif
#if RDX_CFG_CALL_RECORDING_ENABLE
#include "rdx_record_call_audio_br56.h"
#endif
#include "rdx_record_engine.h"

#define RDX_RECORD_TASK_NAME                    "rdx_record"
#define RDX_RECORD_TASK_WAKE                    0x52445801

struct rdx_record_candidate {
    u32 engine_generation;
    u32 session_id;
    u32 capture_generation;
    u16 len;
    u8 payload[RDX_RECORD_CANDIDATE_MAX_SIZE];
};

struct rdx_record_system_latch {
    u8 pending;
    enum rdx_record_system_event_source source;
    enum rdx_record_system_event_type type;
    enum rdx_record_cause cause;
};

struct rdx_record_stop_latch {
    u8 pending;
    u32 request_id;
    u32 last_request_id;
    u16 duplicate_count;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_cause cause;
};

struct rdx_record_engine_runtime {
    spinlock_t lock;
    OS_SEM stopped_sem;
    struct rdx_record_request requests[RDX_RECORD_CONTROL_QUEUE_DEPTH];
    struct rdx_record_candidate candidates[RDX_RECORD_CANDIDATE_QUEUE_DEPTH];
    struct rdx_record_system_latch system_latch;
    struct rdx_record_stop_latch stop_latch;
    struct audio_capture_lease capture_lease;
    const struct rdx_record_destination_ops *destination_ops;
    void *destination_priv;
    rdx_record_engine_event_callback_t event_callback;
    void *event_priv;
    volatile u8 initialized;
    volatile u8 accepting;
    volatile u8 frame_gate;
    volatile u8 task_running;
    u8 request_head;
    u8 request_tail;
    u8 request_count;
    u8 candidate_head;
    u8 candidate_tail;
    u8 candidate_count;
    u8 session_mic_gain_override_valid;
    u8 session_mic_gain;
    enum rdx_record_state state;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_scene scene;
    enum rdx_record_format_id format_id;
    enum rdx_record_termination_mode termination_mode;
    enum rdx_record_cause termination_cause;
    u32 engine_generation;
    u32 next_session_id;
    u32 next_capture_generation;
    u32 session_id;
    u32 capture_generation;
    u32 start_request_id;
    u32 session_frame_seq;
    u32 capture_frame_seq;
    u32 warmup_count;
    u32 produced_count;
    u32 enqueued_count;
    u32 overflow_count;
    u32 stale_count;
    u32 stop_discarded;
    u32 format_mismatch_count;
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
    u8 first_frame_after_resume;
#endif
};

static struct rdx_record_engine_runtime rdx_record_engine;

struct rdx_record_format_spec {
    u16 payload_size;
    u16 frame_ms;
    u8 warmup_frames;
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

static u32 rdx_record_next_token(u32 token)
{
    token++;
    return token ? token : 1;
}

static void rdx_record_engine_wake(void)
{
    if (rdx_record_engine.task_running) {
        os_taskq_post_msg(RDX_RECORD_TASK_NAME, 1, RDX_RECORD_TASK_WAKE);
    }
}

static void rdx_record_engine_emit(
    enum rdx_record_engine_event_type type,
    u32 request_id,
    enum rdx_record_result result,
    enum rdx_record_termination_mode termination_mode,
    enum rdx_record_cause cause)
{
    struct rdx_record_engine_event event = {
        .type = type,
        .request_id = request_id,
        .session_id = rdx_record_engine.session_id,
        .capture_generation = rdx_record_engine.capture_generation,
        .controller_id = rdx_record_engine.controller_id,
        .scene = rdx_record_engine.scene,
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
        rdx_record_engine_format_spec(rdx_record_engine.format_id);
    struct rdx_record_engine_event event = {
        .type = RDX_RECORD_ENGINE_MARK_COMPLETE,
        .request_id = request->request_id,
        .session_id = rdx_record_engine.session_id,
        .capture_generation = rdx_record_engine.capture_generation,
        .controller_id = request->controller_id,
        .scene = rdx_record_engine.scene,
        .result = result,
        .termination_mode = RDX_RECORD_TERMINATION_NONE,
        .cause = RDX_RECORD_CAUSE_NONE,
        .active_pts = (u64)rdx_record_engine.session_frame_seq
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
    spin_unlock(&rdx_record_engine.lock);
}

static int rdx_record_engine_audio_frame(void *priv,
                                         u32 engine_generation,
                                         u32 session_id,
                                         u32 capture_generation,
                                         const u8 *data,
                                         u32 len)
{
    struct rdx_record_candidate *candidate;
    u8 tail;

    if (priv != &rdx_record_engine) {
        return 0;
    }
    rdx_record_engine.produced_count++;
    spin_lock(&rdx_record_engine.lock);
    if (engine_generation != rdx_record_engine.engine_generation
        || session_id != rdx_record_engine.session_id
        || capture_generation != rdx_record_engine.capture_generation) {
        rdx_record_engine.stale_count++;
        spin_unlock(&rdx_record_engine.lock);
        return 0;
    }
    if (!rdx_record_engine.frame_gate
        || (rdx_record_engine.state != RDX_RECORD_STATE_STARTING
            && rdx_record_engine.state != RDX_RECORD_STATE_ACTIVE)) {
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

static u8 rdx_record_engine_format_supported(
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

static u8 rdx_record_engine_uses_capture_lease(void)
{
    return rdx_record_engine.scene == RDX_RECORD_SCENE_MEETING;
}

static int rdx_record_engine_capture_probe(void)
{
#if RDX_CFG_CALL_RECORDING_ENABLE
    if (rdx_record_engine.scene == RDX_RECORD_SCENE_CALL) {
        return rdx_record_call_audio_br56_probe(
            rdx_record_engine.format_id);
    }
#endif
    return 0;
}

static int rdx_record_engine_capture_open(
    const struct rdx_record_audio_open_params *params, u8 resume)
{
#if RDX_CFG_CALL_RECORDING_ENABLE
    if (rdx_record_engine.scene == RDX_RECORD_SCENE_CALL) {
        return resume
               ? rdx_record_call_audio_br56_resume(params)
               : rdx_record_call_audio_br56_open(params);
    }
#endif
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    (void)resume;
    return rdx_record_audio_br56_open(params);
#else
    (void)params;
    (void)resume;
    return -EFAULT;
#endif
}

static void rdx_record_engine_capture_close(u8 pause)
{
#if RDX_CFG_CALL_RECORDING_ENABLE
    if (rdx_record_engine.scene == RDX_RECORD_SCENE_CALL) {
        if (pause) {
            rdx_record_call_audio_br56_pause();
        } else {
            rdx_record_call_audio_br56_close();
        }
        return;
    }
#endif
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    (void)pause;
    rdx_record_audio_br56_close();
#else
    (void)pause;
#endif
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
                                   u32 request_id,
                                   enum rdx_record_result result)
{
    u8 was_running = rdx_record_engine.state != RDX_RECORD_STATE_IDLE;

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
    rdx_record_engine.state = RDX_RECORD_STATE_STOPPING;
    rdx_record_engine_capture_close(0);
    rdx_record_engine_clear_queues();
    if (rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->cancel) {
        rdx_record_engine.destination_ops->cancel(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session_id,
            rdx_record_engine.capture_generation);
    }
    if (rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->detach) {
        rdx_record_engine.destination_ops->detach(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session_id,
            rdx_record_engine.capture_generation);
    }
    rdx_record_engine_release_capture_lease();
    rdx_record_engine.termination_mode = mode;
    rdx_record_engine.termination_cause = cause;
    rdx_record_engine.state = RDX_RECORD_STATE_IDLE;

    printf("[RDX][ENGINE] stopped session=%u capture=%u scene=%u gain_override=%u mic_gain=%u mode=%u cause=%u"
           " produced=%u enqueued=%u overflow=%u mismatch=%u stale=%u"
           " stop_discarded=%u\n",
           (unsigned int)rdx_record_engine.session_id,
           (unsigned int)rdx_record_engine.capture_generation,
           rdx_record_engine.scene,
           rdx_record_engine.session_mic_gain_override_valid,
           rdx_record_engine.session_mic_gain, mode, cause,
           (unsigned int)rdx_record_engine.produced_count,
           (unsigned int)rdx_record_engine.enqueued_count,
           (unsigned int)rdx_record_engine.overflow_count,
           (unsigned int)rdx_record_engine.format_mismatch_count,
           (unsigned int)rdx_record_engine.stale_count,
           (unsigned int)rdx_record_engine.stop_discarded);
    rdx_record_engine_emit(RDX_RECORD_ENGINE_SESSION_STOPPED,
                           request_id, result, mode, cause);

    rdx_record_engine.destination_ops = NULL;
    rdx_record_engine.destination_priv = NULL;
    rdx_record_engine.controller_id = RDX_RECORD_CONTROLLER_NONE;
    rdx_record_engine.scene = RDX_RECORD_SCENE_MEETING;
    rdx_record_engine.format_id = RDX_RECORD_FORMAT_NONE;
    rdx_record_engine.start_request_id = 0;
    rdx_record_engine.session_mic_gain_override_valid = 0;
    rdx_record_engine.session_mic_gain = 0;
}

static void rdx_record_engine_revoke(void *priv, u32 preempt_token,
                                     u32 lease_generation)
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

static void rdx_record_engine_start(const struct rdx_record_request *request)
{
    struct rdx_record_audio_open_params audio_params;
    enum rdx_record_result failure_result = RDX_RECORD_RESULT_START_FAILED;
    enum rdx_record_termination_mode failure_mode =
        RDX_RECORD_TERMINATION_FORCED;
    enum rdx_record_cause failure_cause = RDX_RECORD_CAUSE_START_FAILED;
    u8 cancelled;
    u8 destination_attached = 0;
    u8 recorder_open_attempted = 0;
    int ret;

    if (!rdx_record_engine_format_supported(request->scene,
                                             request->format_id)) {
        rdx_record_engine_emit(RDX_RECORD_ENGINE_START_COMPLETE,
                               request->request_id,
                               RDX_RECORD_RESULT_UNSUPPORTED_FORMAT,
                               RDX_RECORD_TERMINATION_NONE,
                               RDX_RECORD_CAUSE_NONE);
        return;
    }
    if (rdx_record_engine.state != RDX_RECORD_STATE_IDLE) {
        rdx_record_engine_emit(RDX_RECORD_ENGINE_START_COMPLETE,
                               request->request_id,
                               RDX_RECORD_RESULT_BUSY,
                               RDX_RECORD_TERMINATION_NONE,
                               RDX_RECORD_CAUSE_NONE);
        return;
    }

    rdx_record_engine.state = RDX_RECORD_STATE_STARTING;
    rdx_record_engine.session_id = rdx_record_next_token(
        rdx_record_engine.next_session_id);
    rdx_record_engine.next_session_id = rdx_record_engine.session_id;
    rdx_record_engine.capture_generation = rdx_record_next_token(
        rdx_record_engine.next_capture_generation);
    rdx_record_engine.next_capture_generation =
        rdx_record_engine.capture_generation;
    rdx_record_engine.start_request_id = request->request_id;
    rdx_record_engine.controller_id = request->controller_id;
    rdx_record_engine.scene = request->scene;
    rdx_record_engine.format_id = request->format_id;
    rdx_record_engine.destination_ops = request->destination_ops;
    rdx_record_engine.destination_priv = request->destination_priv;
    rdx_record_engine.session_frame_seq = 0;
    rdx_record_engine.capture_frame_seq = 0;
    rdx_record_engine.warmup_count = 0;
    rdx_record_engine.produced_count = 0;
    rdx_record_engine.enqueued_count = 0;
    rdx_record_engine.overflow_count = 0;
    rdx_record_engine.stale_count = 0;
    rdx_record_engine.stop_discarded = 0;
    rdx_record_engine.format_mismatch_count = 0;
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
    rdx_record_engine.first_frame_after_resume = 0;
#endif
    rdx_record_engine_clear_queues();

    rdx_record_engine.session_mic_gain_override_valid = 0;
    rdx_record_engine.session_mic_gain = 0;
#if RDX_CFG_CONFERENCE_RECORDING_ENABLE
    if (rdx_record_engine.scene == RDX_RECORD_SCENE_MEETING) {
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
           (unsigned int)rdx_record_engine.session_id,
           rdx_record_engine.scene,
           rdx_record_engine.session_mic_gain_override_valid,
           rdx_record_engine.session_mic_gain);

    ret = rdx_record_engine_capture_probe();
    if (ret) {
        failure_result = RDX_RECORD_RESULT_SOURCE_LOST;
        failure_cause = RDX_RECORD_CAUSE_SOURCE_LOST;
        goto rollback;
    }

    if (rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->attach) {
        ret = rdx_record_engine.destination_ops->attach(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session_id,
            rdx_record_engine.capture_generation);
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
    audio_params.format_id = rdx_record_engine.format_id;
    audio_params.mic_gain_override_valid =
        rdx_record_engine.session_mic_gain_override_valid;
    audio_params.mic_gain = rdx_record_engine.session_mic_gain;
    audio_params.engine_generation = rdx_record_engine.engine_generation;
    audio_params.session_id = rdx_record_engine.session_id;
    audio_params.capture_generation =
        rdx_record_engine.capture_generation;
    audio_params.frame_callback = rdx_record_engine_audio_frame;
    audio_params.fault_callback = rdx_record_engine_audio_fault;
    audio_params.frame_priv = &rdx_record_engine;
    recorder_open_attempted = 1;
    ret = rdx_record_engine_capture_open(&audio_params, 0);
    if (ret) {
        rdx_record_engine.frame_gate = 0;
        if (ret == -ENODEV) {
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
        rdx_record_engine.state = RDX_RECORD_STATE_ACTIVE;
    }
    spin_unlock(&rdx_record_engine.lock);
    if (cancelled) {
        failure_result = RDX_RECORD_RESULT_CANCELLED;
        goto rollback;
    }
    printf("[RDX][ENGINE] started request=%u session=%u capture=%u"
           " scene=%u format=opus_16k_mono_v1 gain_override=%u mic_gain=%u\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session_id,
           (unsigned int)rdx_record_engine.capture_generation,
           rdx_record_engine.scene,
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
    if (destination_attached && rdx_record_engine.destination_ops
        && rdx_record_engine.destination_ops->detach) {
        rdx_record_engine.destination_ops->detach(
            rdx_record_engine.destination_priv,
            rdx_record_engine.session_id,
            rdx_record_engine.capture_generation);
    }
    rdx_record_engine.state = RDX_RECORD_STATE_IDLE;
    rdx_record_engine_emit(RDX_RECORD_ENGINE_START_COMPLETE,
                           request->request_id,
                           failure_result,
                           failure_mode,
                           failure_cause);
    rdx_record_engine.destination_ops = NULL;
    rdx_record_engine.destination_priv = NULL;
    rdx_record_engine.controller_id = RDX_RECORD_CONTROLLER_NONE;
    rdx_record_engine.scene = RDX_RECORD_SCENE_MEETING;
    rdx_record_engine.format_id = RDX_RECORD_FORMAT_NONE;
    rdx_record_engine.session_mic_gain_override_valid = 0;
    rdx_record_engine.session_mic_gain = 0;
}

static u8 rdx_record_engine_take_system(
    struct rdx_record_system_latch *event)
{
    u8 taken = 0;

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

static u8 rdx_record_engine_take_stop(struct rdx_record_stop_latch *stop)
{
    u8 taken = 0;

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

static u8 rdx_record_engine_take_request(struct rdx_record_request *request)
{
    u8 taken = 0;

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

static u8 rdx_record_engine_take_candidate(
    struct rdx_record_candidate *candidate)
{
    u8 taken = 0;

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
        rdx_record_engine_format_spec(rdx_record_engine.format_id);
    struct rdx_record_frame frame;
    int ret;

    if (candidate->engine_generation != rdx_record_engine.engine_generation
        || candidate->session_id != rdx_record_engine.session_id
        || candidate->capture_generation !=
        rdx_record_engine.capture_generation
        || (rdx_record_engine.state != RDX_RECORD_STATE_ACTIVE
#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
            && rdx_record_engine.state
            != RDX_RECORD_STATE_PAUSING_BY_USER
#endif
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
        return;
    }

    memset(&frame, 0, sizeof(frame));
    frame.session_id = rdx_record_engine.session_id;
    frame.capture_generation = rdx_record_engine.capture_generation;
    frame.session_frame_seq = rdx_record_engine.session_frame_seq++;
    frame.capture_frame_seq = rdx_record_engine.capture_frame_seq++;
    frame.active_pts = (u64)frame.session_frame_seq * format->frame_ms;
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
        rdx_record_engine_stop(RDX_RECORD_TERMINATION_FORCED,
                               RDX_RECORD_CAUSE_QUEUE_FULL, 0,
                               RDX_RECORD_RESULT_QUEUE_FULL);
    }
}

#if RDX_CFG_RECORD_PAUSE_RESUME_ENABLE
static void rdx_record_engine_pause(const struct rdx_record_request *request)
{
    const struct rdx_record_format_spec *format =
        rdx_record_engine_format_spec(rdx_record_engine.format_id);
    struct rdx_record_candidate candidate;

    if (rdx_record_engine.state != RDX_RECORD_STATE_ACTIVE
        || request->controller_id != rdx_record_engine.controller_id
        || request->session_id != rdx_record_engine.session_id
        || request->scene != rdx_record_engine.scene) {
        rdx_record_engine_emit(RDX_RECORD_ENGINE_PAUSE_COMPLETE,
                               request->request_id,
                               request->controller_id
                               != rdx_record_engine.controller_id
                               ? RDX_RECORD_RESULT_NOT_CONTROLLER
                               : RDX_RECORD_RESULT_BAD_STATE,
                               RDX_RECORD_TERMINATION_NONE,
                               RDX_RECORD_CAUSE_NONE);
        return;
    }

    spin_lock(&rdx_record_engine.lock);
    rdx_record_engine.frame_gate = 0;
    rdx_record_engine.state = RDX_RECORD_STATE_PAUSING_BY_USER;
    spin_unlock(&rdx_record_engine.lock);

    rdx_record_engine_capture_close(1);
    while (rdx_record_engine_take_candidate(&candidate)) {
        rdx_record_engine_process_candidate(&candidate);
        if (rdx_record_engine.state == RDX_RECORD_STATE_IDLE) {
            return;
        }
    }
    rdx_record_engine_release_capture_lease();
    rdx_record_engine.state = RDX_RECORD_STATE_PAUSED_BY_USER;
    printf("[RDX][ENGINE] paused request=%u session=%u capture=%u scene=%u"
           " active_pts=%u\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session_id,
           (unsigned int)rdx_record_engine.capture_generation,
           rdx_record_engine.scene,
           (unsigned int)(rdx_record_engine.session_frame_seq
                          * (format ? format->frame_ms : 0)));
    rdx_record_engine_emit(RDX_RECORD_ENGINE_PAUSE_COMPLETE,
                           request->request_id, RDX_RECORD_RESULT_OK,
                           RDX_RECORD_TERMINATION_NONE,
                           RDX_RECORD_CAUSE_NONE);
}

static void rdx_record_engine_resume(const struct rdx_record_request *request)
{
    struct rdx_record_audio_open_params audio_params;
    u8 recorder_open_attempted = 0;
    int ret;

    if (rdx_record_engine.state != RDX_RECORD_STATE_PAUSED_BY_USER
        || request->controller_id != rdx_record_engine.controller_id
        || request->session_id != rdx_record_engine.session_id
        || request->scene != rdx_record_engine.scene) {
        rdx_record_engine_emit(RDX_RECORD_ENGINE_RESUME_COMPLETE,
                               request->request_id,
                               request->controller_id
                               != rdx_record_engine.controller_id
                               ? RDX_RECORD_RESULT_NOT_CONTROLLER
                               : RDX_RECORD_RESULT_BAD_STATE,
                               RDX_RECORD_TERMINATION_NONE,
                               RDX_RECORD_CAUSE_NONE);
        return;
    }

    rdx_record_engine.state = RDX_RECORD_STATE_RESUMING;
    rdx_record_engine.capture_generation = rdx_record_next_token(
        rdx_record_engine.next_capture_generation);
    rdx_record_engine.next_capture_generation =
        rdx_record_engine.capture_generation;
    rdx_record_engine.capture_frame_seq = 0;
    rdx_record_engine.warmup_count = 0;
    rdx_record_engine_clear_queues();

    ret = rdx_record_engine_capture_probe();
    if (ret) {
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
    audio_params.format_id = rdx_record_engine.format_id;
    audio_params.mic_gain_override_valid =
        rdx_record_engine.session_mic_gain_override_valid;
    audio_params.mic_gain = rdx_record_engine.session_mic_gain;
    audio_params.engine_generation = rdx_record_engine.engine_generation;
    audio_params.session_id = rdx_record_engine.session_id;
    audio_params.capture_generation = rdx_record_engine.capture_generation;
    audio_params.frame_callback = rdx_record_engine_audio_frame;
    audio_params.fault_callback = rdx_record_engine_audio_fault;
    audio_params.frame_priv = &rdx_record_engine;
    recorder_open_attempted = 1;
    ret = rdx_record_engine_capture_open(&audio_params, 1);
    if (ret) {
        rdx_record_engine.frame_gate = 0;
        goto release_capture;
    }

    rdx_record_engine.first_frame_after_resume = 1;
    rdx_record_engine.state = RDX_RECORD_STATE_ACTIVE;
    printf("[RDX][ENGINE] resumed request=%u session=%u capture=%u scene=%u gain_override=%u mic_gain=%u\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session_id,
           (unsigned int)rdx_record_engine.capture_generation,
           rdx_record_engine.scene,
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
    if (ret == -ENODEV) {
        printf("[RDX][ENGINE] resume_source_lost request=%u session=%u\n",
               (unsigned int)request->request_id,
               (unsigned int)rdx_record_engine.session_id);
        rdx_record_engine_stop(RDX_RECORD_TERMINATION_FORCED,
                               RDX_RECORD_CAUSE_SOURCE_LOST,
                               request->request_id,
                               RDX_RECORD_RESULT_SOURCE_LOST);
        return;
    }
    rdx_record_engine.state = RDX_RECORD_STATE_PAUSED_BY_USER;
    printf("[RDX][ENGINE] resume_failed request=%u session=%u ret=%d\n",
           (unsigned int)request->request_id,
           (unsigned int)rdx_record_engine.session_id, ret);
    rdx_record_engine_emit(RDX_RECORD_ENGINE_RESUME_COMPLETE,
                           request->request_id,
                           ret == -EBUSY ? RDX_RECORD_RESULT_BUSY
                           : ret == -ENODEV
                             ? RDX_RECORD_RESULT_SOURCE_LOST
                             : RDX_RECORD_RESULT_START_FAILED,
                           RDX_RECORD_TERMINATION_NONE,
                           ret == -ENODEV
                           ? RDX_RECORD_CAUSE_SOURCE_LOST
                           : RDX_RECORD_CAUSE_START_FAILED);
}
#endif

#if RDX_CFG_RECORD_MARK_ENABLE
static void rdx_record_engine_mark(const struct rdx_record_request *request)
{
    enum rdx_record_result result = RDX_RECORD_RESULT_OK;

    if (request->controller_id != rdx_record_engine.controller_id) {
        result = RDX_RECORD_RESULT_NOT_CONTROLLER;
    } else if (rdx_record_engine.state != RDX_RECORD_STATE_ACTIVE
               || request->session_id != rdx_record_engine.session_id
               || request->scene != rdx_record_engine.scene) {
        result = RDX_RECORD_RESULT_BAD_STATE;
    }
    rdx_record_engine_emit_mark(request, result);
}
#endif

static u8 rdx_record_engine_process_one(void)
{
    struct rdx_record_system_latch system_event;
    struct rdx_record_stop_latch stop;
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
        if (rdx_record_engine.state != RDX_RECORD_STATE_IDLE
            && stop.controller_id != rdx_record_engine.controller_id) {
            rdx_record_engine_emit(RDX_RECORD_ENGINE_SESSION_STOPPED,
                                   stop.request_id,
                                   RDX_RECORD_RESULT_NOT_CONTROLLER,
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
    }
    rdx_record_engine.task_running = 0;
}

int rdx_record_engine_init(rdx_record_engine_event_callback_t callback,
                           void *callback_priv)
{
    u32 generation;
    int ret;

    if (rdx_record_engine.initialized) {
        return 0;
    }
    generation = rdx_record_next_token(rdx_record_engine.engine_generation);
    memset(&rdx_record_engine, 0, sizeof(rdx_record_engine));
    rdx_record_engine.engine_generation = generation;
    rdx_record_engine.state = RDX_RECORD_STATE_IDLE;
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
    return 0;
}

int rdx_record_engine_submit(const struct rdx_record_request *request)
{
    u8 tail;

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

int rdx_record_engine_shutdown(u16 timeout_ticks)
{
    int ret;

    if (!rdx_record_engine.initialized) {
        return 0;
    }
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
    if (ret || rdx_record_engine.state != RDX_RECORD_STATE_IDLE) {
        printf("[RDX][ENGINE] shutdown_timeout ret=%d state=%u\n",
               ret, rdx_record_engine.state);
        return RDX_RECORD_RESULT_SHUTDOWN_TIMEOUT;
    }

    rdx_record_engine.initialized = 0;
    rdx_record_engine_wake();
    task_kill(RDX_RECORD_TASK_NAME);
    rdx_record_engine.event_callback = NULL;
    rdx_record_engine.event_priv = NULL;
    return 0;
}

enum rdx_record_state rdx_record_engine_get_state(void)
{
    return rdx_record_engine.state;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_ONLINE_RECORDING_ENABLE */
