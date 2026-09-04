#include <stdint.h>
#include <string.h>

#include "app_config.h"

#if !CONFIG_CPU_BR56
#include <stdio.h>
#else
#include "system/timer.h"
#endif

#if TCFG_RDX_ENABLE && RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE

#include "rdx/rdx_local_storage_service.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23_internal.h"

#define RDX_LOCAL_WIRE_FILE_MAX             32u
#define RDX_LOCAL_WIRE_LIST_CHUNK_SIZE      96u
#define RDX_LOCAL_WIRE_FILE_CHUNK_SIZE      80u
#define RDX_LOCAL_WIRE_TX_PACKET_SIZE       192u
#define RDX_LOCAL_WIRE_TX_OWNER             0x4c4f434cUL /* LOCL */
#define RDX_LOCAL_WIRE_FRAME_SIZE           80u
#define RDX_LOCAL_WIRE_OPUS_FORMAT          0x40a80800UL
#ifndef RDX_CFG_LOCAL_UPLOAD_SESSION_TIMEOUT_MS
#define RDX_CFG_LOCAL_UPLOAD_SESSION_TIMEOUT_MS 30000u
#endif

enum rdx_local_wire_tx_stage {
    RDX_LOCAL_WIRE_TX_NONE = 0,
    RDX_LOCAL_WIRE_TX_GETLIST_ACK,
    RDX_LOCAL_WIRE_TX_FLIST,
    RDX_LOCAL_WIRE_TX_LIST_CHUNK,
    RDX_LOCAL_WIRE_TX_CAPACITY,
    RDX_LOCAL_WIRE_TX_FORMAT_ACK,
    RDX_LOCAL_WIRE_TX_FORMAT_WAIT,
    RDX_LOCAL_WIRE_TX_FORMAT_RESULT,
    RDX_LOCAL_WIRE_TX_DELETE_RESULT,
    RDX_LOCAL_WIRE_TX_FILE_CHUNK,
    RDX_LOCAL_WIRE_TX_LIST_ERROR,
    RDX_LOCAL_WIRE_TX_FILE_ERROR,
    RDX_LOCAL_WIRE_TX_DELETE_ERROR,
    RDX_LOCAL_WIRE_TX_GETLIST_ERROR,
    RDX_LOCAL_WIRE_TX_CAPACITY_ERROR,
    RDX_LOCAL_WIRE_TX_FORMAT_ERROR,
};

struct rdx_local_wire_context {
    struct rdx_protocol_v23_context *protocol;
    struct rdx_local_storage_service *service;
    enum rdx_local_storage_operation_type operation_type;
    enum rdx_local_wire_tx_stage tx_stage;
    enum rdx_local_wire_tx_stage deferred_tx_stage;
    uint32_t operation_token;
    uint32_t operation_connection_generation;
    uint32_t tx_token;
    uint32_t tx_connection_generation;
    uint32_t list_count;
    uint32_t list_length;
    uint32_t list_pack_count;
    uint32_t list_request_pack;
    uint32_t list_generation;
    uint32_t file_request_pack;
    uint32_t file_total_packs;
    uint64_t file_base_offset;
    uint32_t upload_connection_generation;
    uint32_t upload_token_seed;
    uint32_t upload_session_token;
    uint32_t upload_serial_number;
    struct rdx_storage_file_info upload_file;
    uint64_t total_bytes;
    uint64_t free_bytes;
    uint32_t delete_serial_number;
    char delete_filename[RDX_STORAGE_NAME_MAX];
    uint32_t error_serial_number;
    uint32_t error_request_pack;
    char error_filename[RDX_STORAGE_NAME_MAX];
    uint32_t file_chunk_length;
    uint8_t getlist_result;
    uint8_t format_ack_result;
    uint8_t format_result;
    uint8_t format_completed;
    uint8_t delete_result;
    uint8_t file_auto_delete;
    uint8_t upload_session_valid;
    uint8_t list_valid;
#if CONFIG_CPU_BR56
    uint16_t upload_timeout_id;
#endif
    struct rdx_storage_file_info files[RDX_LOCAL_WIRE_FILE_MAX];
    uint8_t file_chunk[RDX_LOCAL_WIRE_FILE_CHUNK_SIZE];
    uint8_t tx_packet[RDX_LOCAL_WIRE_TX_PACKET_SIZE];
};

struct rdx_local_wire_json_emitter {
    uint8_t *target;
    uint32_t target_offset;
    uint32_t target_capacity;
    uint32_t target_length;
    uint32_t encoded_length;
    uint8_t complete;
};

static struct rdx_local_storage_service *rdx_local_wire_bound_service;
static struct rdx_local_wire_context rdx_local_wire;

static uint32_t rdx_local_wire_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xffffffffUL;
    uint32_t i;

    for (i = 0; i < len; i++) {
        uint32_t bit;

        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1)
                  ^ (0xedb88320UL & (uint32_t)-(int)(crc & 1));
        }
    }
    return crc ^ 0xffffffffUL;
}

static uint32_t rdx_local_wire_next_token(uint32_t token)
{
    token++;
    return token ? token : 1;
}

static const char *rdx_local_wire_basename(const char *name)
{
    const char *slash = name ? strrchr(name, '/') : NULL;

    return slash ? slash + 1 : name;
}

static int rdx_local_wire_json_emit(
    struct rdx_local_wire_json_emitter *emitter,
    const char *data, uint32_t length)
{
    uint32_t segment_start;
    uint32_t segment_end;
    uint32_t copy_start;
    uint32_t copy_end;

    if (!emitter || (!data && length)
        || emitter->encoded_length > UINT32_MAX - length) {
        return -1;
    }
    if (emitter->complete) {
        return 0;
    }
    segment_start = emitter->encoded_length;
    segment_end = segment_start + length;
    if (emitter->target && emitter->target_capacity) {
        copy_start = segment_start > emitter->target_offset
                         ? segment_start : emitter->target_offset;
        copy_end = segment_end;
        if (copy_end > emitter->target_offset
                       + emitter->target_capacity) {
            copy_end = emitter->target_offset
                       + emitter->target_capacity;
        }
        if (copy_end > copy_start) {
            uint32_t copy_length = copy_end - copy_start;

            memcpy(emitter->target + emitter->target_length,
                   data + copy_start - segment_start, copy_length);
            emitter->target_length += copy_length;
        }
        if (emitter->target_length == emitter->target_capacity) {
            emitter->complete = 1;
        }
    }
    emitter->encoded_length = segment_end;
    return 0;
}

static int rdx_local_wire_json_emit_literal(
    struct rdx_local_wire_json_emitter *emitter, const char *literal)
{
    if (!literal) {
        return -1;
    }
    return rdx_local_wire_json_emit(
        emitter, literal, (uint32_t)strlen(literal));
}

static int rdx_local_wire_json_emit_u32(
    struct rdx_local_wire_json_emitter *emitter, uint32_t value)
{
    char number[16];
    int length;

    if (!emitter) {
        return -1;
    }
    if (emitter->complete) {
        return 0;
    }
    length = snprintf(number, sizeof(number), "%lu",
                      (unsigned long)value);
    if (length <= 0 || (uint32_t)length >= sizeof(number)) {
        return -1;
    }
    return rdx_local_wire_json_emit(
        emitter, number, (uint32_t)length);
}

static int rdx_local_wire_json_emit_filename(
    struct rdx_local_wire_json_emitter *emitter, const char *name)
{
    uint32_t source = 0;

    if (!emitter || !name
        || !memchr(name, '\0', RDX_STORAGE_NAME_MAX)) {
        return -1;
    }
    while (name[source]) {
        char value = name[source++];

        if ((uint8_t)value < 0x20 || (uint8_t)value > 0x7e) {
            return -1;
        }
        if ((value == '"' || value == '\\')
            && rdx_local_wire_json_emit_literal(emitter, "\\")) {
            return -1;
        }
        if (rdx_local_wire_json_emit(emitter, &value, 1)) {
            return -1;
        }
        if (emitter->complete) {
            return 0;
        }
    }
    return 0;
}

static int rdx_local_wire_encode_list(
    uint32_t count, struct rdx_local_wire_json_emitter *emitter)
{
    uint32_t i;

    if (!emitter || count > RDX_LOCAL_WIRE_FILE_MAX) {
        return -1;
    }
    if (!count) {
        return 0;
    }
    if (rdx_local_wire_json_emit_literal(emitter, "[")) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        const struct rdx_storage_file_info *file =
            &rdx_local_wire.files[i];
        const char *basename = rdx_local_wire_basename(file->file_id.name);

        if (!file->file_id.serial_number || !basename || !basename[0]
            || file->file_size > UINT32_MAX) {
            return -1;
        }
        if (rdx_local_wire_json_emit_literal(
                emitter, i ? ",{\"sn\":" : "{\"sn\":")
            || rdx_local_wire_json_emit_u32(
                   emitter, file->file_id.serial_number)
            || rdx_local_wire_json_emit_literal(emitter, ",\"name\":\"")
            || rdx_local_wire_json_emit_filename(emitter, basename)
            || rdx_local_wire_json_emit_literal(emitter, "\",\"size\":")
            || rdx_local_wire_json_emit_u32(
                   emitter, (uint32_t)file->file_size)
            || rdx_local_wire_json_emit_literal(
                   emitter,
                   ",\"scene\":0,\"start_time\":0,\"end_time\":0,"
                   "\"crc\":\"")
            || rdx_local_wire_json_emit_u32(emitter, file->audio_crc)
            || rdx_local_wire_json_emit_literal(
                   emitter, "\",\"frame_size\":")
            || rdx_local_wire_json_emit_u32(
                   emitter, RDX_LOCAL_WIRE_FRAME_SIZE)
            || rdx_local_wire_json_emit_literal(emitter, ",\"opus\":")
            || rdx_local_wire_json_emit_u32(
                   emitter, RDX_LOCAL_WIRE_OPUS_FORMAT)
            || rdx_local_wire_json_emit_literal(
                   emitter, ",\"marks\":[]}")) {
            return -1;
        }
        if (emitter->complete) {
            return 0;
        }
    }
    return rdx_local_wire_json_emit_literal(emitter, "]");
}

static int rdx_local_wire_build_list(uint32_t count)
{
    struct rdx_local_wire_json_emitter emitter;

    rdx_local_wire.list_count = 0;
    rdx_local_wire.list_length = 0;
    rdx_local_wire.list_pack_count = 0;
    rdx_local_wire.list_valid = 0;
    memset(&emitter, 0, sizeof(emitter));
    if (rdx_local_wire_encode_list(count, &emitter)) {
        return -1;
    }
    rdx_local_wire.list_count = count;
    rdx_local_wire.list_length = emitter.encoded_length;
    rdx_local_wire.list_pack_count =
        (rdx_local_wire.list_length
         + RDX_LOCAL_WIRE_LIST_CHUNK_SIZE - 1)
        / RDX_LOCAL_WIRE_LIST_CHUNK_SIZE;
    rdx_local_wire.list_valid = 1;
    return 0;
}

static int rdx_local_wire_encode_list_range(
    uint32_t offset, uint8_t *target, uint32_t length)
{
    struct rdx_local_wire_json_emitter emitter;

    if (!target || !length || offset > UINT32_MAX - length
        || offset + length > rdx_local_wire.list_length) {
        return -1;
    }
    memset(&emitter, 0, sizeof(emitter));
    emitter.target = target;
    emitter.target_offset = offset;
    emitter.target_capacity = length;
    if (rdx_local_wire_encode_list(
            rdx_local_wire.list_count, &emitter)
        || emitter.target_length != length) {
        return -1;
    }
    return 0;
}

static uint32_t rdx_local_wire_to_kib(uint64_t bytes)
{
    uint64_t kib = bytes / 1024ULL;

    return kib > UINT32_MAX ? UINT32_MAX : (uint32_t)kib;
}

static int rdx_local_wire_build_tx(uint16_t *packet_length)
{
    int header_length;
    uint32_t chunk_length;
    uint32_t offset;
    uint32_t crc;

    if (!packet_length) {
        return -1;
    }
    switch (rdx_local_wire.tx_stage) {
    case RDX_LOCAL_WIRE_TX_GETLIST_ACK:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet), "*DEV#getlist#%u#",
            rdx_local_wire.getlist_result);
        break;
    case RDX_LOCAL_WIRE_TX_GETLIST_ERROR:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet), "*DEV#getlist#1#");
        break;
    case RDX_LOCAL_WIRE_TX_FLIST:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet), "*DEV#flist#%u#%u#",
            (unsigned int)rdx_local_wire.list_length,
            (unsigned int)rdx_local_wire.list_pack_count);
        break;
    case RDX_LOCAL_WIRE_TX_LIST_CHUNK:
        offset = (rdx_local_wire.list_request_pack - 1)
                 * RDX_LOCAL_WIRE_LIST_CHUNK_SIZE;
        chunk_length = rdx_local_wire.list_length - offset;
        if (chunk_length > RDX_LOCAL_WIRE_LIST_CHUNK_SIZE) {
            chunk_length = RDX_LOCAL_WIRE_LIST_CHUNK_SIZE;
        }
        if (rdx_local_wire_encode_list_range(
                offset,
                rdx_local_wire.tx_packet
                    + sizeof(rdx_local_wire.tx_packet)
                      - RDX_LOCAL_WIRE_LIST_CHUNK_SIZE,
                chunk_length)) {
            return -1;
        }
        crc = rdx_local_wire_crc32(
            rdx_local_wire.tx_packet
                + sizeof(rdx_local_wire.tx_packet)
                  - RDX_LOCAL_WIRE_LIST_CHUNK_SIZE,
            chunk_length);
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet),
            "*DEV#listreq#%u#%u#%lu#",
            (unsigned int)rdx_local_wire.list_request_pack,
            (unsigned int)chunk_length, (unsigned long)crc);
        if (header_length <= 0
            || (uint32_t)header_length + chunk_length
               > sizeof(rdx_local_wire.tx_packet)) {
            return -1;
        }
        memmove(rdx_local_wire.tx_packet + header_length,
                rdx_local_wire.tx_packet
                    + sizeof(rdx_local_wire.tx_packet)
                      - RDX_LOCAL_WIRE_LIST_CHUNK_SIZE,
                chunk_length);
        *packet_length = (uint16_t)(header_length + chunk_length);
        return 0;
    case RDX_LOCAL_WIRE_TX_LIST_ERROR:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet),
            "*DEV#listreq#%u#0#0#",
            (unsigned int)rdx_local_wire.error_request_pack);
        break;
    case RDX_LOCAL_WIRE_TX_CAPACITY:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet), "*DEV#mem#%u#%u#",
            (unsigned int)rdx_local_wire_to_kib(rdx_local_wire.free_bytes),
            (unsigned int)rdx_local_wire_to_kib(rdx_local_wire.total_bytes));
        break;
    case RDX_LOCAL_WIRE_TX_CAPACITY_ERROR:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet), "*DEV#mem#0#0#");
        break;
    case RDX_LOCAL_WIRE_TX_FORMAT_ACK:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet), "*DEV#format#%u#",
            rdx_local_wire.format_ack_result);
        break;
    case RDX_LOCAL_WIRE_TX_FORMAT_ERROR:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet), "*DEV#format#1#");
        break;
    case RDX_LOCAL_WIRE_TX_FORMAT_RESULT:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet),
            "*DEV#fmtret#%u#%u#%u#", rdx_local_wire.format_result,
            (unsigned int)rdx_local_wire_to_kib(rdx_local_wire.free_bytes),
            (unsigned int)rdx_local_wire_to_kib(rdx_local_wire.total_bytes));
        break;
    case RDX_LOCAL_WIRE_TX_DELETE_RESULT:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet),
            "*DEV#delfile#%u#%u#%s#", rdx_local_wire.delete_result,
            (unsigned int)rdx_local_wire.delete_serial_number,
            rdx_local_wire.delete_filename);
        break;
    case RDX_LOCAL_WIRE_TX_FILE_CHUNK:
        crc = rdx_local_wire_crc32(rdx_local_wire.file_chunk,
                                   rdx_local_wire.file_chunk_length);
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet),
            "*DEV#opusfile#%u#%u#%u#%u#%lu#",
            (unsigned int)rdx_local_wire.upload_file.file_id.serial_number,
            (unsigned int)rdx_local_wire.file_total_packs,
            (unsigned int)rdx_local_wire.file_request_pack,
            (unsigned int)rdx_local_wire.file_chunk_length,
            (unsigned long)crc);
        if (header_length <= 0
            || (uint32_t)header_length
                   + rdx_local_wire.file_chunk_length
               > sizeof(rdx_local_wire.tx_packet)) {
            return -1;
        }
        memcpy(rdx_local_wire.tx_packet + header_length,
               rdx_local_wire.file_chunk,
               rdx_local_wire.file_chunk_length);
        *packet_length = (uint16_t)(
            header_length + rdx_local_wire.file_chunk_length);
        return 0;
    case RDX_LOCAL_WIRE_TX_FILE_ERROR:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet),
            "*DEV#opusfile#%u#0#%u#0#0#",
            (unsigned int)rdx_local_wire.error_serial_number,
            (unsigned int)rdx_local_wire.error_request_pack);
        break;
    case RDX_LOCAL_WIRE_TX_DELETE_ERROR:
        header_length = snprintf(
            (char *)rdx_local_wire.tx_packet,
            sizeof(rdx_local_wire.tx_packet),
            "*DEV#delfile#1#%u#%s#",
            (unsigned int)rdx_local_wire.error_serial_number,
            rdx_local_wire.error_filename);
        break;
    default:
        return -1;
    }
    if (header_length <= 0
        || (uint32_t)header_length >= sizeof(rdx_local_wire.tx_packet)) {
        return -1;
    }
    *packet_length = (uint16_t)header_length;
    return 0;
}

static void rdx_local_wire_operation_completion(
    void *priv,
    const struct rdx_local_storage_operation_completion *completion);

static void rdx_local_wire_forget_upload(
    const char *reason, uint8_t request_close)
{
    uint32_t connection_generation =
        rdx_local_wire.upload_connection_generation;
    uint32_t session_token = rdx_local_wire.upload_session_token;

#if CONFIG_CPU_BR56
    if (rdx_local_wire.upload_timeout_id) {
        sys_timeout_del(rdx_local_wire.upload_timeout_id);
        rdx_local_wire.upload_timeout_id = 0;
    }
#endif
    rdx_local_wire.upload_session_valid = 0;
    rdx_local_wire.upload_connection_generation = 0;
    rdx_local_wire.upload_session_token = 0;
    rdx_local_wire.upload_serial_number = 0;
    if (request_close && connection_generation && session_token
        && rdx_local_wire.service) {
        (void)rdx_local_storage_service_close_upload(
            rdx_local_wire.service, connection_generation,
            session_token);
    }
    if (connection_generation && session_token) {
        printf("[RDX][LOCAL_WIRE] upload_session_close reason=%s"
               " connection=%u token=%u\n",
               reason ? reason : "unknown",
               (unsigned int)connection_generation,
               (unsigned int)session_token);
    }
}

static void rdx_local_wire_cancel_upload_tx(void)
{
    if (rdx_local_wire.tx_stage == RDX_LOCAL_WIRE_TX_FILE_CHUNK) {
        rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
        rdx_local_wire.deferred_tx_stage = RDX_LOCAL_WIRE_TX_NONE;
        rdx_local_wire.tx_token = rdx_local_wire_next_token(
            rdx_local_wire.tx_token);
    } else if (rdx_local_wire.deferred_tx_stage
               == RDX_LOCAL_WIRE_TX_FILE_CHUNK) {
        rdx_local_wire.deferred_tx_stage = RDX_LOCAL_WIRE_TX_NONE;
    }
    rdx_local_wire.file_auto_delete = 0;
}

static void rdx_local_wire_cancel_upload_operation(void)
{
    if (rdx_local_wire.operation_type
        != RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ) {
        return;
    }
    /* The storage worker may already own this read.  Invalidate the Wire
     * completion token now; close_upload() will close a reader opened by a
     * late completion on the storage owner instead of emitting stale data. */
    rdx_local_wire.operation_type = RDX_LOCAL_STORAGE_OPERATION_NONE;
    rdx_local_wire.operation_token = rdx_local_wire_next_token(
        rdx_local_wire.operation_token);
}

#if CONFIG_CPU_BR56
static void rdx_local_wire_upload_timeout(void *priv)
{
    uint32_t session_token = (uint32_t)(uintptr_t)priv;

    rdx_local_wire.upload_timeout_id = 0;
    if (!rdx_local_wire.upload_session_valid
        || session_token != rdx_local_wire.upload_session_token) {
        return;
    }
    rdx_local_wire_cancel_upload_tx();
    rdx_local_wire_forget_upload("timeout", 1);
}
#endif

static void rdx_local_wire_refresh_upload_timeout(void)
{
#if CONFIG_CPU_BR56
    uint16_t timer_id;

    if (rdx_local_wire.upload_timeout_id) {
        sys_timeout_del(rdx_local_wire.upload_timeout_id);
        rdx_local_wire.upload_timeout_id = 0;
    }
    timer_id = sys_timeout_add(
        (void *)(uintptr_t)rdx_local_wire.upload_session_token,
        rdx_local_wire_upload_timeout,
        RDX_CFG_LOCAL_UPLOAD_SESSION_TIMEOUT_MS);
    rdx_local_wire.upload_timeout_id = timer_id;
    if (!timer_id) {
        printf("[RDX][LOCAL_WIRE] upload_timer_failed token=%u\n",
               (unsigned int)rdx_local_wire.upload_session_token);
    }
#endif
}

static void rdx_local_wire_submit_auto_delete(void)
{
    struct rdx_local_storage_operation_request request;
    enum rdx_storage_result result;
    uint32_t upload_connection_generation;
    uint32_t upload_session_token;

    if (!rdx_local_wire.file_auto_delete
        || rdx_local_wire.file_request_pack
           != rdx_local_wire.file_total_packs
        || !rdx_local_wire.upload_file.file_id.serial_number) {
        return;
    }
    upload_connection_generation =
        rdx_local_wire.upload_connection_generation;
    upload_session_token = rdx_local_wire.upload_session_token;
    rdx_local_wire_forget_upload("auto_delete", 0);
    memset(&request, 0, sizeof(request));
    rdx_local_wire.operation_token = rdx_local_wire_next_token(
        rdx_local_wire.operation_token);
    request.type = RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY;
    request.token = rdx_local_wire.operation_token;
    request.data.delete_file.serial_number =
        rdx_local_wire.upload_file.file_id.serial_number;
    strncpy(request.data.delete_file.filename,
            rdx_local_wire.upload_file.file_id.name,
            sizeof(request.data.delete_file.filename) - 1);
    rdx_local_wire.operation_type = request.type;
    rdx_local_wire.operation_connection_generation =
        rdx_local_wire.protocol->session.connection_generation;
    result = rdx_local_storage_service_submit_operation(
        rdx_local_wire.service, &request,
        rdx_local_wire_operation_completion, &rdx_local_wire);
    if (result != RDX_STORAGE_RESULT_READY) {
        rdx_local_wire.operation_type = RDX_LOCAL_STORAGE_OPERATION_NONE;
        rdx_local_wire.file_auto_delete = 0;
        (void)rdx_local_storage_service_close_upload(
            rdx_local_wire.service, upload_connection_generation,
            upload_session_token);
        printf("[RDX][LOCAL_WIRE] auto_delete_submit_failed result=%u"
               " sn=%u\n", result,
               (unsigned int)request.data.delete_file.serial_number);
    }
}

static void rdx_local_wire_tx_advance(void)
{
    enum rdx_local_wire_tx_stage completed_stage =
        rdx_local_wire.tx_stage;

    switch (rdx_local_wire.tx_stage) {
    case RDX_LOCAL_WIRE_TX_GETLIST_ACK:
        rdx_local_wire.tx_stage = rdx_local_wire.getlist_result
                                      ? RDX_LOCAL_WIRE_TX_NONE
                                      : RDX_LOCAL_WIRE_TX_FLIST;
        break;
    case RDX_LOCAL_WIRE_TX_FORMAT_ACK:
        if (rdx_local_wire.format_ack_result) {
            rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
        } else if (rdx_local_wire.format_completed) {
            rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_FORMAT_RESULT;
        } else {
            rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_FORMAT_WAIT;
        }
        break;
    case RDX_LOCAL_WIRE_TX_FILE_CHUNK:
        rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
        if (rdx_local_wire.file_request_pack
            == rdx_local_wire.file_total_packs) {
            if (rdx_local_wire.file_auto_delete) {
                rdx_local_wire_submit_auto_delete();
            } else {
                rdx_local_wire_forget_upload("complete", 1);
            }
        }
        break;
    default:
        rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
        break;
    }
    if (rdx_local_wire.tx_stage == RDX_LOCAL_WIRE_TX_NONE
        && rdx_local_wire.deferred_tx_stage
              != RDX_LOCAL_WIRE_TX_NONE) {
        rdx_local_wire.tx_stage = rdx_local_wire.deferred_tx_stage;
        rdx_local_wire.deferred_tx_stage = RDX_LOCAL_WIRE_TX_NONE;
        rdx_local_wire.tx_token = rdx_local_wire_next_token(
            rdx_local_wire.tx_token);
        rdx_local_wire.tx_connection_generation =
            rdx_local_wire.protocol->session.connection_generation;
        printf("[RDX][LOCAL_WIRE] tx_deferred_resume after=%u stage=%u\n",
               completed_stage, rdx_local_wire.tx_stage);
    }
}

static void rdx_local_wire_tx_pump(void)
{
    while (rdx_local_wire.tx_stage != RDX_LOCAL_WIRE_TX_NONE
           && rdx_local_wire.tx_stage != RDX_LOCAL_WIRE_TX_FORMAT_WAIT) {
        enum rdx_transport_result result;
        uint16_t packet_length;

        if (!rdx_local_wire.protocol
            || !rdx_protocol_session_can_send(
                   &rdx_local_wire.protocol->session)
            || rdx_local_wire.tx_connection_generation
               != rdx_local_wire.protocol->session.connection_generation
            || rdx_local_wire_build_tx(&packet_length)) {
            rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
            return;
        }
        result = rdx_protocol_v23_send(
            rdx_local_wire.protocol, rdx_local_wire.tx_packet,
            packet_length);
        if (result == RDX_TRANSPORT_ACCEPTED) {
            rdx_local_wire_tx_advance();
            continue;
        }
        if (result == RDX_TRANSPORT_RETRYABLE) {
            if (rdx_transport_request_can_send_now(
                    RDX_LOCAL_WIRE_TX_OWNER,
                    rdx_local_wire.tx_token)) {
                rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
            }
            return;
        }
        rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
        return;
    }
}

static void rdx_local_wire_begin_tx(enum rdx_local_wire_tx_stage stage)
{
    if (rdx_local_wire.tx_stage != RDX_LOCAL_WIRE_TX_NONE) {
        if (rdx_local_wire.deferred_tx_stage
            == RDX_LOCAL_WIRE_TX_NONE) {
            rdx_local_wire.deferred_tx_stage = stage;
            printf("[RDX][LOCAL_WIRE] tx_deferred active=%u stage=%u\n",
                   rdx_local_wire.tx_stage, stage);
        } else {
            printf("[RDX][LOCAL_WIRE] tx_busy active=%u deferred=%u"
                   " rejected=%u\n", rdx_local_wire.tx_stage,
                   rdx_local_wire.deferred_tx_stage, stage);
        }
        return;
    }
    rdx_local_wire.tx_token = rdx_local_wire_next_token(
        rdx_local_wire.tx_token);
    rdx_local_wire.tx_connection_generation =
        rdx_local_wire.protocol->session.connection_generation;
    rdx_local_wire.tx_stage = stage;
    rdx_local_wire_tx_pump();
}

static void rdx_local_wire_operation_completion(
    void *priv,
    const struct rdx_local_storage_operation_completion *completion)
{
    struct rdx_local_wire_context *wire = priv;

    if (!wire || wire != &rdx_local_wire || !completion
        || completion->type != wire->operation_type
        || completion->token != wire->operation_token) {
        return;
    }
    wire->operation_type = RDX_LOCAL_STORAGE_OPERATION_NONE;
    if (!wire->protocol
        || !rdx_protocol_session_is_ready(&wire->protocol->session)
        || wire->operation_connection_generation
           != wire->protocol->session.connection_generation) {
        return;
    }

    switch (completion->type) {
    case RDX_LOCAL_STORAGE_OPERATION_ENUMERATE:
        wire->getlist_result =
            completion->result == RDX_STORAGE_RESULT_READY
            && completion->count <= RDX_LOCAL_WIRE_FILE_MAX
            && completion->storage_generation
            && !rdx_local_wire_build_list(completion->count)
                ? 0 : 1;
        wire->list_generation = wire->getlist_result
                                    ? 0
                                    : completion->storage_generation;
        printf("[RDX][LOCAL_WIRE] list result=%u count=%u bytes=%u"
               " packs=%u\n", wire->getlist_result,
               (unsigned int)completion->count,
               (unsigned int)wire->list_length,
               (unsigned int)wire->list_pack_count);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_GETLIST_ACK);
        break;
    case RDX_LOCAL_STORAGE_OPERATION_CAPACITY:
        if (completion->result == RDX_STORAGE_RESULT_READY) {
            wire->total_bytes = completion->total_bytes;
            wire->free_bytes = completion->free_bytes;
        } else {
            wire->total_bytes = 0;
            wire->free_bytes = 0;
        }
        printf("[RDX][LOCAL_WIRE] capacity result=%u free_kib=%u"
               " total_kib=%u\n", completion->result,
               (unsigned int)rdx_local_wire_to_kib(wire->free_bytes),
               (unsigned int)rdx_local_wire_to_kib(wire->total_bytes));
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_CAPACITY);
        break;
    case RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ:
        if (completion->result != RDX_STORAGE_RESULT_READY
            || !completion->read_length
            || completion->file.file_size <= wire->file_base_offset
            || completion->upload_session_token
                   != wire->upload_session_token
            || completion->file.file_id.serial_number
                   != wire->upload_serial_number) {
            printf("[RDX][LOCAL_WIRE] upload_failed result=%u sn=%u"
                   " pack=%u\n", completion->result,
                   (unsigned int)wire->delete_serial_number,
                   (unsigned int)wire->file_request_pack);
            wire->file_chunk_length = 0;
            wire->file_total_packs = 0;
            wire->error_serial_number = wire->upload_serial_number
                ? wire->upload_serial_number
                : wire->delete_serial_number;
            wire->error_request_pack = wire->file_request_pack;
            rdx_local_wire_forget_upload("read_failed", 1);
            rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_FILE_ERROR);
            break;
        }
        wire->upload_file = completion->file;
        wire->file_chunk_length = completion->read_length;
        wire->file_total_packs = (uint32_t)(
            (completion->file.file_size - wire->file_base_offset
             + RDX_LOCAL_WIRE_FILE_CHUNK_SIZE - 1)
            / RDX_LOCAL_WIRE_FILE_CHUNK_SIZE);
        if (!wire->file_total_packs
            || wire->file_request_pack > wire->file_total_packs) {
            wire->file_chunk_length = 0;
            wire->file_total_packs = 0;
            wire->error_serial_number =
                wire->upload_file.file_id.serial_number;
            wire->error_request_pack = wire->file_request_pack;
            rdx_local_wire_forget_upload("range_failed", 1);
            rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_FILE_ERROR);
            break;
        }
        wire->upload_session_valid = 1;
        rdx_local_wire_refresh_upload_timeout();
        if (wire->file_request_pack == 1
            || wire->file_request_pack == wire->file_total_packs
            || !(wire->file_request_pack % 128)) {
            printf("[RDX][LOCAL_WIRE] upload sn=%u pack=%u/%u"
                   " offset_hi=%u offset_lo=%u len=%u auto_delete=%u\n",
                   (unsigned int)wire->upload_file.file_id.serial_number,
                   (unsigned int)wire->file_request_pack,
                   (unsigned int)wire->file_total_packs,
                   (unsigned int)(wire->file_base_offset >> 32),
                   (unsigned int)wire->file_base_offset,
                   (unsigned int)wire->file_chunk_length,
                   wire->file_auto_delete);
        }
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_FILE_CHUNK);
        break;
    case RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY:
        if (wire->file_auto_delete) {
            if (completion->result == RDX_STORAGE_RESULT_READY) {
                wire->list_valid = 0;
            }
            printf("[RDX][LOCAL_WIRE] auto_delete result=%u sn=%u"
                   " filename=%s\n", completion->result,
                   (unsigned int)wire->upload_file.file_id.serial_number,
                   rdx_local_wire_basename(
                       wire->upload_file.file_id.name));
            wire->file_auto_delete = 0;
            break;
        }
        wire->delete_result =
            completion->result == RDX_STORAGE_RESULT_READY ? 0 : 1;
        if (!wire->delete_result) {
            wire->list_valid = 0;
        }
        printf("[RDX][LOCAL_WIRE] delete result=%u sn=%u filename=%s\n",
               wire->delete_result,
               (unsigned int)wire->delete_serial_number,
               wire->delete_filename);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_DELETE_RESULT);
        break;
    case RDX_LOCAL_STORAGE_OPERATION_FORMAT:
        wire->format_completed = 1;
        wire->format_result =
            completion->result == RDX_STORAGE_RESULT_READY ? 0 : 1;
        wire->total_bytes = completion->total_bytes;
        wire->free_bytes = completion->free_bytes;
        wire->list_valid = 0;
        printf("[RDX][LOCAL_WIRE] format result=%u free_kib=%u"
               " total_kib=%u\n", wire->format_result,
               (unsigned int)rdx_local_wire_to_kib(wire->free_bytes),
               (unsigned int)rdx_local_wire_to_kib(wire->total_bytes));
        if (wire->tx_stage == RDX_LOCAL_WIRE_TX_FORMAT_WAIT) {
            wire->tx_stage = RDX_LOCAL_WIRE_TX_FORMAT_RESULT;
        }
        rdx_local_wire_tx_pump();
        break;
    default:
        break;
    }
}

static enum rdx_storage_result rdx_local_wire_submit(
    struct rdx_local_storage_operation_request *request)
{
    enum rdx_storage_result result;

    if (rdx_local_wire.operation_type
        != RDX_LOCAL_STORAGE_OPERATION_NONE) {
        return RDX_STORAGE_RESULT_BUSY;
    }
    rdx_local_wire.operation_token = rdx_local_wire_next_token(
        rdx_local_wire.operation_token);
    request->token = rdx_local_wire.operation_token;
    rdx_local_wire.operation_type = request->type;
    rdx_local_wire.operation_connection_generation =
        rdx_local_wire.protocol->session.connection_generation;
    result = rdx_local_storage_service_submit_operation(
        rdx_local_wire.service, request,
        rdx_local_wire_operation_completion, &rdx_local_wire);
    if (result != RDX_STORAGE_RESULT_READY) {
        rdx_local_wire.operation_type = RDX_LOCAL_STORAGE_OPERATION_NONE;
    }
    return result;
}

static int rdx_local_wire_parse_u32(const uint8_t **cursor,
                                    const uint8_t *end,
                                    uint32_t *value)
{
    uint32_t parsed = 0;
    uint8_t digits = 0;

    while (*cursor < end && **cursor != '#') {
        uint8_t digit = **cursor;

        if (digit < '0' || digit > '9'
            || parsed > (UINT32_MAX - (digit - '0')) / 10) {
            return -1;
        }
        parsed = parsed * 10 + (digit - '0');
        (*cursor)++;
        digits = 1;
    }
    if (!digits || *cursor >= end || **cursor != '#') {
        return -1;
    }
    (*cursor)++;
    *value = parsed;
    return 0;
}

static uint8_t rdx_local_wire_transport_busy(void)
{
    return rdx_local_wire.tx_stage != RDX_LOCAL_WIRE_TX_NONE
           || rdx_local_wire.deferred_tx_stage
                  != RDX_LOCAL_WIRE_TX_NONE;
}

static void rdx_local_wire_handle_getlist(void)
{
    struct rdx_local_storage_operation_request request;
    enum rdx_storage_result result;

    if (rdx_local_wire_transport_busy()) {
        printf("[RDX][LOCAL_WIRE] getlist_rejected reason=transport_busy\n");
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_GETLIST_ERROR);
        return;
    }
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_ENUMERATE;
    request.data.enumerate.items = rdx_local_wire.files;
    request.data.enumerate.capacity = RDX_LOCAL_WIRE_FILE_MAX;
    rdx_local_wire.list_valid = 0;
    result = rdx_local_wire_submit(&request);
    if (result == RDX_STORAGE_RESULT_READY) {
        rdx_local_wire_forget_upload("getlist", 0);
    } else {
        rdx_local_wire.list_valid = 0;
        rdx_local_wire.list_generation = 0;
        printf("[RDX][LOCAL_WIRE] getlist_rejected result=%u\n", result);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_GETLIST_ERROR);
    }
}

static void rdx_local_wire_handle_capacity(void)
{
    struct rdx_local_storage_operation_request request;
    enum rdx_storage_result result;

    if (rdx_local_wire_transport_busy()) {
        printf("[RDX][LOCAL_WIRE] capacity_rejected"
               " reason=transport_busy\n");
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_CAPACITY_ERROR);
        return;
    }
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_CAPACITY;
    result = rdx_local_wire_submit(&request);
    if (result == RDX_STORAGE_RESULT_READY) {
        rdx_local_wire_forget_upload("capacity", 0);
    } else {
        printf("[RDX][LOCAL_WIRE] capacity_rejected result=%u\n", result);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_CAPACITY_ERROR);
    }
}

static void rdx_local_wire_handle_format(void)
{
    struct rdx_local_storage_operation_request request;
    enum rdx_storage_result result;

    if (rdx_local_wire_transport_busy()
        || rdx_local_wire.operation_type
               != RDX_LOCAL_STORAGE_OPERATION_NONE) {
        printf("[RDX][LOCAL_WIRE] format_rejected reason=%s\n",
               rdx_local_wire_transport_busy()
                   ? "transport_busy" : "storage_busy");
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_FORMAT_ERROR);
        return;
    }
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_FORMAT;
    rdx_local_wire.format_ack_result = 0;
    rdx_local_wire.format_result = 1;
    rdx_local_wire.format_completed = 0;
    result = rdx_local_wire_submit(&request);
    if (result == RDX_STORAGE_RESULT_READY) {
        rdx_local_wire_forget_upload("format", 0);
    } else {
        printf("[RDX][LOCAL_WIRE] format_rejected result=%u\n", result);
    }
    rdx_local_wire_begin_tx(
        result == RDX_STORAGE_RESULT_READY
            ? RDX_LOCAL_WIRE_TX_FORMAT_ACK
            : RDX_LOCAL_WIRE_TX_FORMAT_ERROR);
}

static int rdx_local_wire_handle_listreq(const uint8_t *data,
                                         uint16_t len,
                                         uint16_t prefix_length)
{
    const uint8_t *cursor = data + prefix_length;
    const uint8_t *end = data + len;
    uint32_t previous_pack;
    uint32_t requested_pack;

    if (rdx_local_wire_parse_u32(&cursor, end, &previous_pack)
        || rdx_local_wire_parse_u32(&cursor, end, &requested_pack)
        || cursor != end) {
        return -1;
    }
    (void)previous_pack;
    if (!requested_pack) {
        rdx_local_wire.list_valid = 0;
        return 0;
    }
    if (rdx_local_wire_transport_busy()) {
        rdx_local_wire.error_request_pack = requested_pack;
        printf("[RDX][LOCAL_WIRE] list_rejected pack=%u"
               " reason=transport_busy\n",
               (unsigned int)requested_pack);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_LIST_ERROR);
        return 0;
    }
    if (!rdx_local_wire.list_valid
        || requested_pack > rdx_local_wire.list_pack_count
        || !rdx_local_wire.list_generation
        || rdx_local_wire.list_generation
               != rdx_local_storage_service_index_generation(
                      rdx_local_wire.service)) {
        printf("[RDX][LOCAL_WIRE] list_invalid pack=%u snapshot=%u"
               " current=%u\n", (unsigned int)requested_pack,
               (unsigned int)rdx_local_wire.list_generation,
               (unsigned int)rdx_local_storage_service_index_generation(
                   rdx_local_wire.service));
        rdx_local_wire.list_valid = 0;
        rdx_local_wire.list_generation = 0;
        rdx_local_wire.error_request_pack = requested_pack;
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_LIST_ERROR);
        return 0;
    }
    rdx_local_wire.list_request_pack = requested_pack;
    rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_LIST_CHUNK);
    return 0;
}

static int rdx_local_wire_handle_delete(const uint8_t *data,
                                        uint16_t len,
                                        uint16_t prefix_length)
{
    struct rdx_local_storage_operation_request request;
    const uint8_t *cursor = data + prefix_length;
    const uint8_t *end = data + len;
    const uint8_t *filename_start;
    uint32_t filename_length;
    uint32_t serial_number;
    enum rdx_storage_result result;

    if (rdx_local_wire_parse_u32(&cursor, end, &serial_number)
        || !serial_number) {
        return -1;
    }
    filename_start = cursor;
    while (cursor < end && *cursor != '#') {
        if (*cursor < 0x20 || *cursor > 0x7e) {
            return -1;
        }
        cursor++;
    }
    filename_length = (uint32_t)(cursor - filename_start);
    if (!filename_length || filename_length >= RDX_STORAGE_NAME_MAX
        || cursor >= end || *cursor != '#' || cursor + 1 != end) {
        return -1;
    }
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY;
    request.data.delete_file.serial_number = serial_number;
    memcpy(request.data.delete_file.filename, filename_start,
           filename_length);
    if (rdx_local_wire_transport_busy()
        || rdx_local_wire.operation_type
               != RDX_LOCAL_STORAGE_OPERATION_NONE) {
        rdx_local_wire.error_serial_number = serial_number;
        memcpy(rdx_local_wire.error_filename, filename_start,
               filename_length);
        rdx_local_wire.error_filename[filename_length] = 0;
        printf("[RDX][LOCAL_WIRE] delete_rejected reason=%s sn=%u\n",
               rdx_local_wire_transport_busy()
                   ? "transport_busy" : "storage_busy",
               (unsigned int)serial_number);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_DELETE_ERROR);
        return 0;
    }
    rdx_local_wire.delete_serial_number = serial_number;
    memcpy(rdx_local_wire.delete_filename, filename_start,
           filename_length);
    rdx_local_wire.delete_filename[filename_length] = 0;
    result = rdx_local_wire_submit(&request);
    if (result == RDX_STORAGE_RESULT_READY) {
        rdx_local_wire_forget_upload("delete", 0);
    } else {
        printf("[RDX][LOCAL_WIRE] delete_rejected result=%u sn=%u\n",
               result, (unsigned int)serial_number);
        rdx_local_wire.error_serial_number = serial_number;
        strncpy(rdx_local_wire.error_filename,
                rdx_local_wire.delete_filename,
                sizeof(rdx_local_wire.error_filename) - 1);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_DELETE_ERROR);
    }
    return 0;
}

static int rdx_local_wire_handle_opusfile(const uint8_t *data,
                                          uint16_t len,
                                          uint16_t prefix_length)
{
    struct rdx_local_storage_operation_request request;
    const uint8_t *cursor = data + prefix_length;
    const uint8_t *end = data + len;
    uint32_t previous_pack;
    uint32_t serial_number;
    uint32_t requested_pack;
    uint32_t auto_delete;
    uint32_t file_offset = 0;
    uint64_t read_offset;
    uint32_t connection_generation;
    uint8_t open_session;
    enum rdx_storage_result result;

    if (rdx_local_wire_parse_u32(&cursor, end, &previous_pack)
        || rdx_local_wire_parse_u32(&cursor, end, &serial_number)
        || rdx_local_wire_parse_u32(&cursor, end, &requested_pack)
        || rdx_local_wire_parse_u32(&cursor, end, &auto_delete)) {
        return -1;
    }
    if (cursor != end
        && (rdx_local_wire_parse_u32(&cursor, end, &file_offset)
            || cursor != end)) {
        return -1;
    }
    (void)previous_pack;
    if (!requested_pack) {
        rdx_local_wire_cancel_upload_tx();
        rdx_local_wire_cancel_upload_operation();
        rdx_local_wire_forget_upload("cancel", 1);
        return 0;
    }
    if (!serial_number || auto_delete > 1
        || requested_pack > UINT32_MAX / RDX_LOCAL_WIRE_FILE_CHUNK_SIZE) {
        return -1;
    }
    if (rdx_local_wire_transport_busy()
        || rdx_local_wire.operation_type
               != RDX_LOCAL_STORAGE_OPERATION_NONE) {
        rdx_local_wire.error_serial_number = serial_number;
        rdx_local_wire.error_request_pack = requested_pack;
        printf("[RDX][LOCAL_WIRE] upload_rejected reason=%s sn=%u"
               " pack=%u open=0\n",
               rdx_local_wire_transport_busy()
                   ? "transport_busy" : "storage_busy",
               (unsigned int)serial_number,
               (unsigned int)requested_pack);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_FILE_ERROR);
        return 0;
    }
    read_offset = (uint64_t)file_offset
                  + (uint64_t)(requested_pack - 1)
                    * RDX_LOCAL_WIRE_FILE_CHUNK_SIZE;
    connection_generation =
        rdx_local_wire.protocol->session.connection_generation;
    open_session = !rdx_local_wire.upload_session_valid
        || rdx_local_wire.upload_connection_generation
               != connection_generation
        || rdx_local_wire.upload_serial_number != serial_number;
    if (open_session) {
        rdx_local_wire.upload_token_seed = rdx_local_wire_next_token(
            rdx_local_wire.upload_token_seed);
        rdx_local_wire.upload_connection_generation =
            connection_generation;
        rdx_local_wire.upload_session_token =
            rdx_local_wire.upload_token_seed;
        rdx_local_wire.upload_serial_number = serial_number;
        rdx_local_wire.upload_session_valid = 1;
    }
    memset(&request, 0, sizeof(request));
    request.type = RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ;
    request.data.read.serial_number = serial_number;
    request.data.read.connection_generation = connection_generation;
    request.data.read.session_token =
        rdx_local_wire.upload_session_token;
    request.data.read.offset = read_offset;
    request.data.read.buffer = rdx_local_wire.file_chunk;
    request.data.read.length = sizeof(rdx_local_wire.file_chunk);
    request.data.read.open_session = open_session;
    rdx_local_wire.file_request_pack = requested_pack;
    rdx_local_wire.file_base_offset = file_offset;
    rdx_local_wire.file_auto_delete = (uint8_t)auto_delete;
    rdx_local_wire.delete_serial_number = serial_number;
    result = rdx_local_wire_submit(&request);
    if (result != RDX_STORAGE_RESULT_READY) {
        printf("[RDX][LOCAL_WIRE] upload_rejected result=%u sn=%u"
               " pack=%u open=%u\n", result,
               (unsigned int)serial_number,
               (unsigned int)requested_pack, open_session);
        rdx_local_wire.file_auto_delete = 0;
        rdx_local_wire.file_chunk_length = 0;
        rdx_local_wire.file_total_packs = 0;
        rdx_local_wire.error_serial_number = serial_number;
        rdx_local_wire.error_request_pack = requested_pack;
        rdx_local_wire_forget_upload("submit_failed", 1);
        rdx_local_wire_begin_tx(RDX_LOCAL_WIRE_TX_FILE_ERROR);
    }
    return 0;
}

int rdx_protocol_v23_local_storage_bind_service(
    struct rdx_local_storage_service *service)
{
    rdx_local_wire_bound_service = service;
    return 0;
}

int rdx_protocol_v23_local_storage_init(
    struct rdx_protocol_v23_context *context)
{
    if (!context || !rdx_local_wire_bound_service) {
        return -1;
    }
    memset(&rdx_local_wire, 0, sizeof(rdx_local_wire));
    rdx_local_wire.protocol = context;
    rdx_local_wire.service = rdx_local_wire_bound_service;
    return 0;
}

void rdx_protocol_v23_local_storage_exit(void)
{
    rdx_local_wire_forget_upload("exit", 1);
    memset(&rdx_local_wire, 0, sizeof(rdx_local_wire));
}

void rdx_protocol_v23_local_storage_handle_link_change(uint8_t connected)
{
    if (connected) {
        return;
    }
    rdx_local_wire_forget_upload("disconnect", 1);
    rdx_local_wire.operation_type = RDX_LOCAL_STORAGE_OPERATION_NONE;
    rdx_local_wire.tx_stage = RDX_LOCAL_WIRE_TX_NONE;
    rdx_local_wire.deferred_tx_stage = RDX_LOCAL_WIRE_TX_NONE;
    rdx_local_wire.list_valid = 0;
    rdx_local_wire.list_generation = 0;
    rdx_local_wire.file_auto_delete = 0;
}

void rdx_protocol_v23_local_storage_handle_upload_timeout(void)
{
    if (rdx_local_wire.upload_session_valid) {
        rdx_local_wire_cancel_upload_tx();
        rdx_local_wire_forget_upload("timeout_test", 1);
    }
}

void rdx_protocol_v23_local_storage_handle_ccc_change(void)
{
    if (rdx_local_wire.protocol
        && !rdx_local_wire.protocol->session.ccc_ready) {
        rdx_protocol_v23_local_storage_handle_link_change(0);
    }
}

void rdx_protocol_v23_local_storage_handle_can_send(
    uint32_t owner, uint32_t token)
{
    if (owner == RDX_LOCAL_WIRE_TX_OWNER
        && token == rdx_local_wire.tx_token) {
        rdx_local_wire_tx_pump();
    }
}

uint8_t rdx_protocol_v23_local_storage_handle_command(
    const uint8_t *data, uint16_t len)
{
    static const uint8_t getlist[] = "*APP#getlist#";
    static const uint8_t listreq[] = "*APP#listreq#";
    static const uint8_t opusfile[] = "*APP#opusfile#";
    static const uint8_t capacity[] = "*APP#mem#";
    static const uint8_t format[] = "*APP#format#";
    static const uint8_t delete_file[] = "*APP#delfile#";
    const char *command = NULL;
    int result = 0;

    if (rdx_protocol_v23_data_equals(
            data, len, getlist, sizeof(getlist) - 1)) {
        command = "getlist";
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, listreq, sizeof(listreq) - 1)) {
        command = "listreq";
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, opusfile, sizeof(opusfile) - 1)) {
        command = "opusfile";
    } else if (rdx_protocol_v23_data_equals(
                   data, len, capacity, sizeof(capacity) - 1)) {
        command = "mem";
    } else if (rdx_protocol_v23_data_equals(
                   data, len, format, sizeof(format) - 1)) {
        command = "format";
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, delete_file, sizeof(delete_file) - 1)) {
        command = "delfile";
    } else {
        return 0;
    }

    rdx_protocol_v23_log_rx(rdx_local_wire.protocol, command, data, len);
    if (!rdx_protocol_v23_query_ready(
            rdx_local_wire.protocol, command, len)) {
        return 1;
    }
    if (!strcmp(command, "getlist")) {
        rdx_local_wire_handle_getlist();
    } else if (!strcmp(command, "listreq")) {
        result = rdx_local_wire_handle_listreq(
            data, len, sizeof(listreq) - 1);
    } else if (!strcmp(command, "opusfile")) {
        result = rdx_local_wire_handle_opusfile(
            data, len, sizeof(opusfile) - 1);
    } else if (!strcmp(command, "mem")) {
        rdx_local_wire_handle_capacity();
    } else if (!strcmp(command, "format")) {
        rdx_local_wire_handle_format();
    } else {
        result = rdx_local_wire_handle_delete(
            data, len, sizeof(delete_file) - 1);
    }
    if (result) {
        printf("[RDX][DROP] cmd=%s reason=malformed_or_not_ready len=%u\n",
               command, len);
    }
    return 1;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_LOCAL_STORAGE_WIRE_ENABLE */
