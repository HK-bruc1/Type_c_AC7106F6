#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_RECORD_MARK_ENABLE
#include "rdx/rdx_session_metadata_service.h"

#define RDX_SESSION_MARK_MAX                    20
#define RDX_SESSION_MARK_DEDUP_WINDOW_MS        6000UL

struct rdx_session_metadata_runtime {
    uint32_t session_id;
    uint32_t marks[RDX_SESSION_MARK_MAX];
    uint8_t mark_count;
};

static struct rdx_session_metadata_runtime rdx_session_metadata;

void rdx_session_metadata_begin(uint32_t session_id)
{
    memset(&rdx_session_metadata, 0, sizeof(rdx_session_metadata));
    rdx_session_metadata.session_id = session_id;
    printf("[RDX][METADATA] begin session=%u\n", (unsigned int)session_id);
}

void rdx_session_metadata_end(uint32_t session_id)
{
    if (session_id != rdx_session_metadata.session_id) {
        return;
    }
    printf("[RDX][METADATA] end session=%u marks=%u\n",
           (unsigned int)session_id, rdx_session_metadata.mark_count);
    memset(&rdx_session_metadata, 0, sizeof(rdx_session_metadata));
}

void rdx_session_metadata_apply_mark(
    uint32_t session_id, uint64_t active_pts, uint8_t source,
    enum rdx_record_result engine_result,
    struct rdx_session_mark *mark)
{
    uint32_t offset_ms = (uint32_t)active_pts;

    memset(mark, 0, sizeof(*mark));
    mark->session_id = session_id;
    mark->source = source;
    if (engine_result || !session_id
        || session_id != rdx_session_metadata.session_id
        || active_pts > 0xffffffffULL) {
        mark->result = RDX_SESSION_MARK_BAD_STATE;
        return;
    }
    if (rdx_session_metadata.mark_count >= RDX_SESSION_MARK_MAX) {
        mark->result = RDX_SESSION_MARK_FULL;
        return;
    }
    if (rdx_session_metadata.mark_count
        && offset_ms - rdx_session_metadata.marks[
               rdx_session_metadata.mark_count - 1]
           < RDX_SESSION_MARK_DEDUP_WINDOW_MS) {
        mark->result = RDX_SESSION_MARK_BUSY;
        return;
    }

    rdx_session_metadata.marks[rdx_session_metadata.mark_count++] =
        offset_ms;
    mark->result = RDX_SESSION_MARK_OK;
    mark->index = rdx_session_metadata.mark_count;
    mark->offset_ms = offset_ms;
    printf("[RDX][METADATA] mark session=%u index=%u offset=%u"
           " source=%u\n",
           (unsigned int)session_id, mark->index,
           (unsigned int)offset_ms, source);
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_RECORD_MARK_ENABLE */
