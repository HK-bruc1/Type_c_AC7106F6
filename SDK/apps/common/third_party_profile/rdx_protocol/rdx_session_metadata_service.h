#ifndef RDX_SESSION_METADATA_SERVICE_H
#define RDX_SESSION_METADATA_SERVICE_H

#include "rdx_record_defs.h"

enum rdx_session_mark_result {
    RDX_SESSION_MARK_OK = 0,
    RDX_SESSION_MARK_BAD_STATE = 1,
    RDX_SESSION_MARK_BUSY = 2,
    RDX_SESSION_MARK_FULL = 3,
};

struct rdx_session_mark {
    u32 session_id;
    u32 offset_ms;
    u8 index;
    u8 source;
    u8 result;
};

void rdx_session_metadata_begin(u32 session_id);
void rdx_session_metadata_end(u32 session_id);
void rdx_session_metadata_apply_mark(
    u32 session_id, u64 active_pts, u8 source,
    enum rdx_record_result engine_result,
    struct rdx_session_mark *mark);

#endif
