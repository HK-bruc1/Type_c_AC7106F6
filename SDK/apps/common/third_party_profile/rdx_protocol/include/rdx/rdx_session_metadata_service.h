#ifndef RDX_SESSION_METADATA_SERVICE_H
#define RDX_SESSION_METADATA_SERVICE_H

#include "rdx/rdx_record_types.h"

enum rdx_session_mark_result {
    RDX_SESSION_MARK_OK = 0,
    RDX_SESSION_MARK_BAD_STATE = 1,
    RDX_SESSION_MARK_BUSY = 2,
    RDX_SESSION_MARK_FULL = 3,
};

struct rdx_session_mark {
    uint32_t session_id;
    uint32_t offset_ms;
    uint8_t index;
    uint8_t source;
    uint8_t result;
};

void rdx_session_metadata_begin(uint32_t session_id);
void rdx_session_metadata_end(uint32_t session_id);
void rdx_session_metadata_apply_mark(
    uint32_t session_id, uint64_t active_pts, uint8_t source,
    enum rdx_record_result engine_result,
    struct rdx_session_mark *mark);

#endif
