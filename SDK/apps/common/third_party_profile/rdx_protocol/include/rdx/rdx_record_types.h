#ifndef RDX_RECORD_TYPES_H
#define RDX_RECORD_TYPES_H

#include <stdint.h>

#define RDX_RECORD_MEETING_V1_SAMPLE_RATE       16000
#define RDX_RECORD_MEETING_V1_CHANNELS          1
#define RDX_RECORD_MEETING_V1_BIT_RATE          32000
#define RDX_RECORD_MEETING_V1_FRAME_MS          20
#define RDX_RECORD_MEETING_V1_PAYLOAD_SIZE      80
#define RDX_RECORD_MEETING_V1_WARMUP_FRAMES     10
#define RDX_RECORD_CALL_V1_SAMPLE_RATE           16000
#define RDX_RECORD_CALL_V1_CHANNELS              1
#define RDX_RECORD_CALL_V1_BIT_RATE              32000
#define RDX_RECORD_CALL_V1_FRAME_MS              20
#define RDX_RECORD_CALL_V1_PAYLOAD_SIZE          80
#define RDX_RECORD_CALL_V1_WARMUP_FRAMES         10
#define RDX_RECORD_CANDIDATE_MAX_SIZE           96
#define RDX_RECORD_CANDIDATE_QUEUE_DEPTH        16
#define RDX_RECORD_CONTROL_QUEUE_DEPTH          4
#define RDX_RECORD_DESTINATION_OPTIONAL         0
#define RDX_RECORD_DESTINATION_REQUIRED         1
/* Negative values are reserved for destinations that want Engine to retain
 * the failure class.  Existing online destinations return positive/nonzero
 * values and keep their legacy queue-full handling. */
#define RDX_RECORD_DESTINATION_ERROR_QUEUE_FULL (-1)
#define RDX_RECORD_DESTINATION_ERROR_STORAGE   (-2)

#define RDX_RECORD_FRAME_FIRST_AFTER_RESUME     (1u << 0)

enum rdx_record_format_id {
    RDX_RECORD_FORMAT_NONE = 0,
    RDX_RECORD_FORMAT_MEETING_V1 = 1,
    RDX_RECORD_FORMAT_CALL_V1 = 2,
};

enum rdx_record_scene {
    RDX_RECORD_SCENE_MEETING = 0,
    RDX_RECORD_SCENE_CALL = 1,
};

enum rdx_record_controller_id {
    RDX_RECORD_CONTROLLER_NONE = 0,
    RDX_RECORD_CONTROLLER_ONLINE = 1,
};

enum rdx_record_request_type {
    RDX_RECORD_REQUEST_START = 1,
    RDX_RECORD_REQUEST_STOP = 2,
    RDX_RECORD_REQUEST_PAUSE = 3,
    RDX_RECORD_REQUEST_RESUME = 4,
    RDX_RECORD_REQUEST_MARK = 5,
};

enum rdx_record_state {
    RDX_RECORD_STATE_IDLE = 0,
    RDX_RECORD_STATE_STARTING = 1,
    RDX_RECORD_STATE_ACTIVE = 2,
    RDX_RECORD_STATE_PAUSING_BY_USER = 3,
    RDX_RECORD_STATE_PAUSED_BY_USER = 4,
    RDX_RECORD_STATE_RESUMING = 5,
    RDX_RECORD_STATE_SUSPENDING_BY_RESOURCE = 6,
    RDX_RECORD_STATE_SUSPENDED_BY_RESOURCE = 7,
    RDX_RECORD_STATE_STOPPING = 8,
};

enum rdx_record_result {
    RDX_RECORD_RESULT_OK = 0,
    RDX_RECORD_RESULT_BUSY,
    RDX_RECORD_RESULT_NO_MEMORY,
    RDX_RECORD_RESULT_UNSUPPORTED_FORMAT,
    RDX_RECORD_RESULT_FORMAT_MISMATCH,
    RDX_RECORD_RESULT_START_FAILED,
    RDX_RECORD_RESULT_CONTROL_QUEUE_FULL,
    RDX_RECORD_RESULT_BAD_STATE,
    RDX_RECORD_RESULT_NOT_CONTROLLER,
    RDX_RECORD_RESULT_RESOURCE_PREEMPTED,
    RDX_RECORD_RESULT_QUEUE_FULL,
    RDX_RECORD_RESULT_LINK_LOST,
    RDX_RECORD_RESULT_CANCELLED,
    RDX_RECORD_RESULT_SOURCE_LOST,
    RDX_RECORD_RESULT_SHUTDOWN_TIMEOUT,
    /* Destination seal failed after capture stopped.  This is distinct from
     * a capture-side queue overflow so APP can retry/report storage faults. */
    RDX_RECORD_RESULT_STORAGE_ERROR,
};

struct rdx_record_request;

/* START preparation may involve filesystem work.  Protocol adapters only
 * enqueue this hook; Record Engine invokes it from its application worker
 * before allocating the runtime Session and attaching the Destination. */
typedef enum rdx_record_result (*rdx_record_start_prepare_callback_t)(
    void *priv, struct rdx_record_request *request);

enum rdx_record_termination_mode {
    RDX_RECORD_TERMINATION_NONE = 0,
    RDX_RECORD_TERMINATION_NORMAL,
    RDX_RECORD_TERMINATION_FORCED,
};

enum rdx_record_cause {
    RDX_RECORD_CAUSE_NONE = 0,
    RDX_RECORD_CAUSE_USER_REQUEST,
    RDX_RECORD_CAUSE_USB_PREEMPT,
    RDX_RECORD_CAUSE_LINK_LOST,
    RDX_RECORD_CAUSE_CCC_OFF,
    RDX_RECORD_CAUSE_QUEUE_FULL,
    RDX_RECORD_CAUSE_FORMAT_MISMATCH,
    RDX_RECORD_CAUSE_SOURCE_LOST,
    RDX_RECORD_CAUSE_START_FAILED,
    RDX_RECORD_CAUSE_SHUTDOWN,
    RDX_RECORD_CAUSE_STORAGE_ERROR,
};

enum rdx_record_system_event_source {
    RDX_RECORD_SYSTEM_SOURCE_USB = 1,
    RDX_RECORD_SYSTEM_SOURCE_TRANSPORT = 2,
    RDX_RECORD_SYSTEM_SOURCE_LIFECYCLE = 3,
    RDX_RECORD_SYSTEM_SOURCE_ENGINE = 4,
};

enum rdx_record_system_event_type {
    RDX_RECORD_SYSTEM_USB_PREEMPT = 1,
    RDX_RECORD_SYSTEM_LINK_LOST = 2,
    RDX_RECORD_SYSTEM_CCC_OFF = 3,
    RDX_RECORD_SYSTEM_SHUTDOWN = 4,
    RDX_RECORD_SYSTEM_ENGINE_FAULT = 5,
    RDX_RECORD_SYSTEM_SOURCE_LOST = 6,
};

enum rdx_record_engine_event_type {
    RDX_RECORD_ENGINE_START_COMPLETE = 1,
    RDX_RECORD_ENGINE_SESSION_STOPPED = 2,
    RDX_RECORD_ENGINE_PAUSE_COMPLETE = 3,
    RDX_RECORD_ENGINE_RESUME_COMPLETE = 4,
    RDX_RECORD_ENGINE_MARK_COMPLETE = 5,
};

enum rdx_record_mark_source {
    RDX_RECORD_MARK_SOURCE_KEY = 0,
    RDX_RECORD_MARK_SOURCE_APP = 1,
};

struct rdx_record_frame {
    uint32_t session_id;
    uint32_t capture_generation;
    uint32_t session_frame_seq;
    uint32_t capture_frame_seq;
    uint64_t active_pts;
    uint32_t duration;
    uint32_t flags;
    const uint8_t *payload;
    uint16_t payload_len;
};

struct rdx_record_destination_ops {
    int (*attach)(void *priv, uint32_t session_id,
                  uint32_t capture_generation);
    int (*consume)(void *priv, const struct rdx_record_frame *frame);
    void (*poll)(void *priv, uint32_t session_id,
                 uint32_t capture_generation);
    /* Migration contract: normal STOP uses seal/detach_capture. */
    int (*seal)(void *priv, uint32_t session_id,
                uint32_t capture_generation);
    int (*abort)(void *priv, uint32_t session_id,
                 uint32_t capture_generation, enum rdx_record_cause cause);
    void (*cancel)(void *priv, uint32_t session_id,
                   uint32_t capture_generation);
    void (*detach)(void *priv, uint32_t session_id,
                   uint32_t capture_generation);
    void (*detach_capture)(void *priv, uint32_t session_id,
                           uint32_t capture_generation);
    /* A required destination must never be silently overrun by Engine. */
    uint8_t requirement;
};

struct rdx_record_request {
    uint32_t request_id;
    uint32_t session_id;
    enum rdx_record_request_type type;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_scene scene;
    enum rdx_record_format_id format_id;
    uint8_t source;
    rdx_record_start_prepare_callback_t prepare_start;
    void *prepare_priv;
    const struct rdx_record_destination_ops *destination_ops;
    void *destination_priv;
};

struct rdx_record_engine_event {
    enum rdx_record_engine_event_type type;
    uint32_t request_id;
    uint32_t session_id;
    uint32_t capture_generation;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_scene scene;
    enum rdx_record_result result;
    enum rdx_record_termination_mode termination_mode;
    enum rdx_record_cause cause;
    uint64_t active_pts;
    uint8_t source;
};

typedef void (*rdx_record_engine_event_callback_t)(
    void *priv, const struct rdx_record_engine_event *event);

#endif
