#ifndef RDX_RECORD_DEFS_H
#define RDX_RECORD_DEFS_H

#include "system/includes.h"

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

#define RDX_RECORD_FRAME_FIRST_AFTER_RESUME     BIT(0)

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
};

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
    u32 session_id;
    u32 capture_generation;
    u32 session_frame_seq;
    u32 capture_frame_seq;
    u64 active_pts;
    u32 duration;
    u32 flags;
    const u8 *payload;
    u16 payload_len;
};

struct rdx_record_destination_ops {
    int (*attach)(void *priv, u32 session_id, u32 capture_generation);
    int (*consume)(void *priv, const struct rdx_record_frame *frame);
    void (*cancel)(void *priv, u32 session_id, u32 capture_generation);
    void (*detach)(void *priv, u32 session_id, u32 capture_generation);
};

struct rdx_record_request {
    u32 request_id;
    u32 session_id;
    enum rdx_record_request_type type;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_scene scene;
    enum rdx_record_format_id format_id;
    u8 source;
    const struct rdx_record_destination_ops *destination_ops;
    void *destination_priv;
};

struct rdx_record_engine_event {
    enum rdx_record_engine_event_type type;
    u32 request_id;
    u32 session_id;
    u32 capture_generation;
    enum rdx_record_controller_id controller_id;
    enum rdx_record_scene scene;
    enum rdx_record_result result;
    enum rdx_record_termination_mode termination_mode;
    enum rdx_record_cause cause;
    u64 active_pts;
    u8 source;
};

typedef void (*rdx_record_engine_event_callback_t)(
    void *priv, const struct rdx_record_engine_event *event);

#endif
