#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rdx/rdx_local_storage_service.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_session.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23_internal.h"

#define TEST_TX_MAX             16
#define TEST_TX_PACKET_MAX      192
#define TEST_FILE_SIZE          83
#define TEST_FILE_CAPACITY      245
#define TEST_LIST_FILE_MAX      32
#define TEST_LIST_JSON_MAX      16384

struct test_tx_packet {
    uint16_t len;
    uint8_t data[TEST_TX_PACKET_MAX + 1];
};

struct test_state {
    struct rdx_storage_file_info file;
    struct rdx_storage_file_info list_files[TEST_LIST_FILE_MAX];
    uint8_t file_data[TEST_FILE_CAPACITY];
    struct test_tx_packet packets[TEST_TX_MAX];
    enum rdx_transport_result send_result;
    uint32_t packet_count;
    uint32_t send_attempt_count;
    uint32_t can_send_request_count;
    uint32_t can_send_owner;
    uint32_t can_send_token;
    uint32_t operation_count;
    uint32_t delete_count;
    uint32_t format_count;
    uint32_t list_file_count;
    uint32_t storage_generation;
    uint32_t upload_connection_generation;
    uint32_t upload_session_token;
    uint32_t upload_serial_number;
    uint32_t upload_identity_resolve_count;
    uint32_t upload_open_count;
    uint32_t upload_read_count;
    uint32_t upload_close_count;
    enum rdx_storage_result submit_result_once;
    enum rdx_storage_result completion_result_once;
    uint8_t file_present;
    uint8_t upload_active;
};

static struct test_state test_state;

static uint32_t test_crc32(const uint8_t *data, uint32_t len)
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

enum rdx_storage_result rdx_local_storage_service_submit_operation(
    struct rdx_local_storage_service *service,
    const struct rdx_local_storage_operation_request *request,
    rdx_local_storage_operation_callback_t callback, void *priv)
{
    struct rdx_local_storage_operation_completion completion;
    uint32_t available;

    assert(service);
    assert(request);
    assert(callback);
    if (test_state.submit_result_once != RDX_STORAGE_RESULT_READY) {
        enum rdx_storage_result result = test_state.submit_result_once;

        test_state.submit_result_once = RDX_STORAGE_RESULT_READY;
        return result;
    }
    memset(&completion, 0, sizeof(completion));
    completion.type = request->type;
    completion.token = request->token;
    completion.result = RDX_STORAGE_RESULT_READY;
    test_state.operation_count++;
    if (test_state.completion_result_once != RDX_STORAGE_RESULT_READY) {
        completion.result = test_state.completion_result_once;
        test_state.completion_result_once = RDX_STORAGE_RESULT_READY;
        callback(priv, &completion);
        return RDX_STORAGE_RESULT_READY;
    }

    if (request->type != RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ
        && test_state.upload_active) {
        test_state.upload_active = 0;
        test_state.upload_close_count++;
    }

    switch (request->type) {
    case RDX_LOCAL_STORAGE_OPERATION_CAPACITY:
        completion.total_bytes = 8ULL * 1024 * 1024;
        completion.free_bytes = test_state.file_present
                                    ? 3ULL * 1024 * 1024
                                    : 8ULL * 1024 * 1024;
        break;
    case RDX_LOCAL_STORAGE_OPERATION_ENUMERATE:
        completion.storage_generation = test_state.storage_generation;
        if (test_state.file_present) {
            assert(request->data.enumerate.capacity
                   >= test_state.list_file_count);
            memcpy(request->data.enumerate.items,
                   test_state.list_files,
                   test_state.list_file_count
                       * sizeof(test_state.list_files[0]));
            completion.count = test_state.list_file_count;
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_UPLOAD_READ:
        if (!test_state.file_present
            || request->data.read.serial_number
                   != test_state.file.file_id.serial_number
            || request->data.read.offset >= test_state.file.file_size) {
            completion.result = RDX_STORAGE_RESULT_BAD_ARGUMENT;
            break;
        }
        if (request->data.read.open_session) {
            if (test_state.upload_active) {
                test_state.upload_close_count++;
            }
            test_state.upload_active = 1;
            test_state.upload_connection_generation =
                request->data.read.connection_generation;
            test_state.upload_session_token =
                request->data.read.session_token;
            test_state.upload_serial_number =
                request->data.read.serial_number;
            test_state.upload_identity_resolve_count++;
            test_state.upload_open_count++;
        } else {
            assert(test_state.upload_active);
            assert(test_state.upload_connection_generation
                   == request->data.read.connection_generation);
            assert(test_state.upload_session_token
                   == request->data.read.session_token);
            assert(test_state.upload_serial_number
                   == request->data.read.serial_number);
        }
        completion.file = test_state.file;
        completion.storage_generation = test_state.storage_generation;
        completion.upload_session_token =
            test_state.upload_session_token;
        available = (uint32_t)test_state.file.file_size
                    - (uint32_t)request->data.read.offset;
        completion.read_length = request->data.read.length;
        if (completion.read_length > available) {
            completion.read_length = available;
        }
        memcpy(request->data.read.buffer,
               test_state.file_data + (uint32_t)request->data.read.offset,
               completion.read_length);
        test_state.upload_read_count++;
        break;
    case RDX_LOCAL_STORAGE_OPERATION_DELETE_BY_IDENTITY:
        assert(request->data.delete_file.serial_number
               == test_state.file.file_id.serial_number);
        assert(!strcmp(request->data.delete_file.filename,
                       test_state.file.file_id.name));
        completion.file = test_state.file;
        if (!test_state.file_present) {
            completion.result = RDX_STORAGE_RESULT_BAD_ARGUMENT;
        } else {
            test_state.file_present = 0;
            test_state.delete_count++;
            test_state.storage_generation++;
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_FORMAT:
        test_state.file_present = 0;
        test_state.format_count++;
        test_state.storage_generation++;
        completion.total_bytes = 8ULL * 1024 * 1024;
        completion.free_bytes = completion.total_bytes;
        break;
    default:
        assert(0);
        break;
    }
    callback(priv, &completion);
    return RDX_STORAGE_RESULT_READY;
}

enum rdx_storage_result rdx_local_storage_service_close_upload(
    struct rdx_local_storage_service *service,
    uint32_t connection_generation, uint32_t session_token)
{
    assert(service);
    if (test_state.upload_active
        && test_state.upload_connection_generation == connection_generation
        && test_state.upload_session_token == session_token) {
        test_state.upload_active = 0;
        test_state.upload_close_count++;
    }
    return RDX_STORAGE_RESULT_READY;
}

uint32_t rdx_local_storage_service_index_generation(
    struct rdx_local_storage_service *service)
{
    assert(service);
    return test_state.storage_generation;
}

int rdx_protocol_v23_send(struct rdx_protocol_v23_context *context,
                          const uint8_t *data, uint16_t len)
{
    struct test_tx_packet *packet;

    assert(context);
    assert(data);
    assert(len && len <= TEST_TX_PACKET_MAX);
    test_state.send_attempt_count++;
    if (test_state.send_result != RDX_TRANSPORT_ACCEPTED) {
        return test_state.send_result;
    }
    assert(test_state.packet_count < TEST_TX_MAX);
    packet = &test_state.packets[test_state.packet_count++];
    packet->len = len;
    memcpy(packet->data, data, len);
    packet->data[len] = 0;
    return RDX_TRANSPORT_ACCEPTED;
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
    (void)command;
    (void)len;
    return rdx_protocol_session_is_ready(&context->session);
}

uint8_t rdx_protocol_session_can_send(
    const struct rdx_protocol_session *session)
{
    return session && session->connected && session->ccc_ready;
}

uint8_t rdx_protocol_session_is_ready(
    const struct rdx_protocol_session *session)
{
    return rdx_protocol_session_can_send(session)
           && session->authenticated;
}

int rdx_transport_request_can_send_now(uint32_t owner, uint32_t token)
{
    assert(owner);
    assert(token);
    test_state.can_send_request_count++;
    test_state.can_send_owner = owner;
    test_state.can_send_token = token;
    return 0;
}

static void reset_packets(void)
{
    memset(test_state.packets, 0, sizeof(test_state.packets));
    test_state.packet_count = 0;
    test_state.send_attempt_count = 0;
    test_state.can_send_request_count = 0;
    test_state.can_send_owner = 0;
    test_state.can_send_token = 0;
    test_state.send_result = RDX_TRANSPORT_ACCEPTED;
}

static void assert_text_packet(uint32_t index, const char *expected)
{
    assert(index < test_state.packet_count);
    assert(test_state.packets[index].len == strlen(expected));
    assert(!memcmp(test_state.packets[index].data,
                   expected, strlen(expected)));
}

static void assert_binary_packet(uint32_t index, const char *header,
                                 const uint8_t *payload,
                                 uint32_t payload_len)
{
    uint32_t header_len = (uint32_t)strlen(header);

    assert(index < test_state.packet_count);
    assert(test_state.packets[index].len == header_len + payload_len);
    assert(!memcmp(test_state.packets[index].data, header, header_len));
    assert(!memcmp(test_state.packets[index].data + header_len,
                   payload, payload_len));
}

static uint32_t build_expected_list_json(char *json, uint32_t capacity)
{
    uint32_t length = 0;
    uint32_t i;

    assert(json);
    assert(capacity);
    if (!test_state.file_present || !test_state.list_file_count) {
        return 0;
    }
    json[length++] = '[';
    for (i = 0; i < test_state.list_file_count; i++) {
        const struct rdx_storage_file_info *file =
            &test_state.list_files[i];
        const char *basename = strrchr(file->file_id.name, '/');
        char escaped[RDX_STORAGE_NAME_MAX * 2];
        uint32_t source = 0;
        uint32_t target = 0;
        int written;

        basename = basename ? basename + 1 : file->file_id.name;
        while (basename[source]) {
            char value = basename[source++];

            assert((uint8_t)value >= 0x20 && (uint8_t)value <= 0x7e);
            if (value == '"' || value == '\\') {
                assert(target + 1 < sizeof(escaped));
                escaped[target++] = '\\';
            }
            assert(target + 1 < sizeof(escaped));
            escaped[target++] = value;
        }
        escaped[target] = 0;
        written = snprintf(
            json + length, capacity - length,
            "%s{\"sn\":%u,\"name\":\"%s\",\"size\":%u,"
            "\"scene\":0,\"start_time\":0,\"end_time\":0,"
            "\"crc\":\"%lu\",\"frame_size\":80,"
            "\"opus\":1084753920,\"marks\":[]}",
            i ? "," : "", (unsigned int)file->file_id.serial_number,
            escaped, (unsigned int)file->file_size,
            (unsigned long)file->audio_crc);
        assert(written > 0 && (uint32_t)written < capacity - length);
        length += (uint32_t)written;
    }
    assert(length + 1 < capacity);
    json[length++] = ']';
    json[length] = 0;
    return length;
}

static void assert_list_pack(const char *json, uint32_t json_length,
                             uint32_t previous_pack,
                             uint32_t requested_pack,
                             uint8_t *reassembled)
{
    char command[64];
    char header[64];
    uint32_t offset = (requested_pack - 1) * 96;
    uint32_t chunk_length = json_length - offset;
    uint32_t crc;

    if (chunk_length > 96) {
        chunk_length = 96;
    }
    snprintf(command, sizeof(command), "*APP#listreq#%u#%u#",
             (unsigned int)previous_pack,
             (unsigned int)requested_pack);
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               (const uint8_t *)command,
               (uint16_t)strlen(command)) == 1);
    assert(test_state.packet_count == 1);
    crc = test_crc32((const uint8_t *)json + offset, chunk_length);
    snprintf(header, sizeof(header), "*DEV#listreq#%u#%u#%lu#",
             (unsigned int)requested_pack,
             (unsigned int)chunk_length, (unsigned long)crc);
    assert_binary_packet(0, header, (const uint8_t *)json + offset,
                         chunk_length);
    if (reassembled) {
        memcpy(reassembled + offset, json + offset, chunk_length);
    }
}

static void assert_current_list_snapshot(const char *expected,
                                         uint32_t expected_length)
{
    static const uint8_t getlist[] = "*APP#getlist#";
    static uint8_t reassembled[TEST_LIST_JSON_MAX];
    char flist[64];
    uint32_t pack_count = (expected_length + 95) / 96;
    uint32_t pack;

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               getlist, sizeof(getlist) - 1) == 1);
    assert(test_state.packet_count == 2);
    assert_text_packet(0, "*DEV#getlist#0#");
    snprintf(flist, sizeof(flist), "*DEV#flist#%u#%u#",
             (unsigned int)expected_length,
             (unsigned int)pack_count);
    assert_text_packet(1, flist);
    if (!pack_count) {
        return;
    }

    memset(reassembled, 0, sizeof(reassembled));
    for (pack = pack_count; pack; pack--) {
        assert_list_pack(expected, expected_length,
                         pack == pack_count ? 0 : pack + 1,
                         pack, reassembled);
    }
    assert(!memcmp(reassembled, expected, expected_length));

    /* The same frozen snapshot must support retransmission and a jump back
     * to an earlier packet without retaining a forward-only JSON cursor. */
    pack = pack_count > 2 ? pack_count / 2 : 1;
    assert_list_pack(expected, expected_length, pack, pack, NULL);
    assert_list_pack(expected, expected_length, pack, 1, NULL);
}

static void test_list_golden_trace(void)
{
    static const uint8_t getlist[] = "*APP#getlist#";
    static const char list_json[] =
        "[{\"sn\":7,\"name\":\"abc.raw\",\"size\":83,"
        "\"scene\":0,\"start_time\":0,\"end_time\":0,"
        "\"crc\":\"891568578\",\"frame_size\":80,"
        "\"opus\":1084753920,\"marks\":[]}]";
    char expected[TEST_TX_PACKET_MAX + 1];

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               getlist, sizeof(getlist) - 1) == 1);
    assert(test_state.packet_count == 2);
    assert_text_packet(0, "*DEV#getlist#0#");
    snprintf(expected, sizeof(expected), "*DEV#flist#%u#%u#",
             (unsigned int)strlen(list_json),
             (unsigned int)((strlen(list_json) + 95) / 96));
    assert_text_packet(1, expected);

    assert_current_list_snapshot(list_json,
                                 (uint32_t)strlen(list_json));
}

static void test_list_boundaries_and_escaping(void)
{
    static char expected[TEST_LIST_JSON_MAX];
    uint32_t expected_length;
    uint32_t i;

    test_state.file_present = 0;
    assert_current_list_snapshot("", 0);
    test_state.file_present = 1;

    test_state.list_file_count = 1;
    test_state.storage_generation = 9;
    test_state.list_files[0] = test_state.file;
    strcpy(test_state.list_files[0].file_id.name,
           "dir/a\"b\\c.raw");
    expected_length = build_expected_list_json(
        expected, sizeof(expected));
    assert_current_list_snapshot(expected, expected_length);

    for (i = 0; i < TEST_LIST_FILE_MAX; i++) {
        struct rdx_storage_file_info *file =
            &test_state.list_files[i];

        memset(file, 0, sizeof(*file));
        file->file_id.artifact_id.value = i + 1;
        file->file_id.serial_number = i + 1;
        file->file_size = TEST_FILE_SIZE + i;
        file->audio_crc = 891568578UL + i;
        snprintf(file->file_id.name, sizeof(file->file_id.name),
                 "record-%02u.raw", (unsigned int)i);
    }
    for (i = 0; i < RDX_STORAGE_NAME_MAX - 1; i++) {
        test_state.list_files[TEST_LIST_FILE_MAX - 1]
            .file_id.name[i] = (i & 1) ? '\\' : '"';
    }
    test_state.list_files[TEST_LIST_FILE_MAX - 1]
        .file_id.name[RDX_STORAGE_NAME_MAX - 1] = 0;

    test_state.list_file_count = TEST_LIST_FILE_MAX - 1;
    expected_length = build_expected_list_json(
        expected, sizeof(expected));
    assert_current_list_snapshot(expected, expected_length);

    test_state.list_file_count = TEST_LIST_FILE_MAX;
    expected_length = build_expected_list_json(
        expected, sizeof(expected));
    assert_current_list_snapshot(expected, expected_length);

    test_state.list_file_count = 1;
    test_state.list_files[0] = test_state.file;
}

static void test_capacity_and_file_golden_trace(void)
{
    static const uint8_t capacity[] = "*APP#mem#";
    static const uint8_t opusfile[] = "*APP#opusfile#0#7#1#0#";
    char header[TEST_TX_PACKET_MAX + 1];
    uint32_t crc;

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               capacity, sizeof(capacity) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#mem#3072#8192#");

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               opusfile, sizeof(opusfile) - 1) == 1);
    assert(test_state.packet_count == 1);
    crc = test_crc32(test_state.file_data, 80);
    snprintf(header, sizeof(header), "*DEV#opusfile#7#2#1#80#%lu#",
             (unsigned long)crc);
    assert_binary_packet(0, header, test_state.file_data, 80);
    assert(test_state.upload_identity_resolve_count == 1);
    assert(test_state.upload_open_count == 1);
    assert(test_state.upload_read_count == 1);
    assert(test_state.upload_close_count == 0);
    assert(test_state.upload_active);
}

static void test_long_file_offset_and_auto_delete(void)
{
    static const uint8_t last_short_pack[] =
        "*APP#opusfile#1#7#2#0#";
    static const uint8_t offset_middle_pack[] =
        "*APP#opusfile#0#7#2#0#80#";
    static const uint8_t offset_last_auto_delete[] =
        "*APP#opusfile#2#7#3#1#80#";
    char header[TEST_TX_PACKET_MAX + 1];
    uint32_t crc;
    uint32_t delete_before = test_state.delete_count;
    uint32_t owner;
    uint32_t token;
    uint32_t opens_before = test_state.upload_open_count;
    uint32_t resolves_before = test_state.upload_identity_resolve_count;
    uint32_t closes_before = test_state.upload_close_count;

    /* Freeze the short final-chunk contract first: an 83-byte file is two
     * 80-byte packs and the final packet carries only the remaining bytes. */
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               last_short_pack, sizeof(last_short_pack) - 1) == 1);
    assert(test_state.packet_count == 1);
    crc = test_crc32(test_state.file_data + 80, 3);
    snprintf(header, sizeof(header), "*DEV#opusfile#7#2#2#3#%lu#",
             (unsigned long)crc);
    assert_binary_packet(0, header, test_state.file_data + 80, 3);
    assert(test_state.upload_open_count == opens_before);
    assert(test_state.upload_identity_resolve_count == resolves_before);
    assert(test_state.upload_close_count == closes_before + 1);
    assert(!test_state.upload_active);

    /* A resumed transfer computes total packs relative to the supplied base
     * offset while still reading the requested absolute file position. */
    test_state.file.file_size = TEST_FILE_CAPACITY;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               offset_middle_pack, sizeof(offset_middle_pack) - 1) == 1);
    assert(test_state.packet_count == 1);
    crc = test_crc32(test_state.file_data + 160, 80);
    snprintf(header, sizeof(header), "*DEV#opusfile#7#3#2#80#%lu#",
             (unsigned long)crc);
    assert_binary_packet(0, header, test_state.file_data + 160, 80);
    assert(test_state.upload_open_count == opens_before + 1);
    assert(test_state.upload_identity_resolve_count == resolves_before + 1);
    assert(test_state.upload_active);

    /* Retransmitting an old/current pack reuses the same verified reader. */
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               offset_middle_pack, sizeof(offset_middle_pack) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_binary_packet(0, header, test_state.file_data + 160, 80);
    assert(test_state.upload_open_count == opens_before + 1);
    assert(test_state.upload_identity_resolve_count == resolves_before + 1);

    /* Auto-delete is submitted only after the final chunk has been accepted
     * by Transport, so a retryable TX cannot erase an unread recording. */
    reset_packets();
    test_state.send_result = RDX_TRANSPORT_RETRYABLE;
    assert(rdx_protocol_v23_local_storage_handle_command(
               offset_last_auto_delete,
               sizeof(offset_last_auto_delete) - 1) == 1);
    assert(test_state.packet_count == 0);
    assert(test_state.send_attempt_count == 1);
    assert(test_state.can_send_request_count == 1);
    assert(test_state.delete_count == delete_before);
    assert(test_state.file_present);
    owner = test_state.can_send_owner;
    token = test_state.can_send_token;

    test_state.send_result = RDX_TRANSPORT_ACCEPTED;
    rdx_protocol_v23_local_storage_handle_can_send(owner, token);
    assert(test_state.packet_count == 1);
    crc = test_crc32(test_state.file_data + 240, 5);
    snprintf(header, sizeof(header), "*DEV#opusfile#7#3#3#5#%lu#",
             (unsigned long)crc);
    assert_binary_packet(0, header, test_state.file_data + 240, 5);
    assert(test_state.delete_count == delete_before + 1);
    assert(!test_state.file_present);
    assert(test_state.upload_open_count == opens_before + 1);
    assert(test_state.upload_identity_resolve_count == resolves_before + 1);
    assert(test_state.upload_close_count == closes_before + 2);
    assert(!test_state.upload_active);

    test_state.file.file_size = TEST_FILE_SIZE;
    test_state.file_present = 1;
}

static void test_delete_and_format_golden_trace(void)
{
    static const uint8_t delete_file[] = "*APP#delfile#7#abc.raw#";
    static const uint8_t format[] = "*APP#format#";
    uint32_t delete_before = test_state.delete_count;

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               delete_file, sizeof(delete_file) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#delfile#0#7#abc.raw#");
    assert(test_state.delete_count == delete_before + 1);
    assert(!test_state.file_present);

    test_state.file_present = 1;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               format, sizeof(format) - 1) == 1);
    assert(test_state.packet_count == 2);
    assert_text_packet(0, "*DEV#format#0#");
    assert_text_packet(1, "*DEV#fmtret#0#8192#8192#");
    assert(test_state.format_count == 1);
    assert(!test_state.file_present);
}

static void test_malformed_and_disconnect(void)
{
    static const uint8_t getlist[] = "*APP#getlist#";
    static const uint8_t malformed_list[] = "*APP#listreq#x#1#";
    static const uint8_t malformed_delete[] = "*APP#delfile#0#abc.raw#";
    static const uint8_t malformed_file[] = "*APP#opusfile#0#7#";
    static const uint8_t listreq[] = "*APP#listreq#0#1#";

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               malformed_list, sizeof(malformed_list) - 1) == 1);
    assert(rdx_protocol_v23_local_storage_handle_command(
               malformed_delete, sizeof(malformed_delete) - 1) == 1);
    assert(rdx_protocol_v23_local_storage_handle_command(
               malformed_file, sizeof(malformed_file) - 1) == 1);
    assert(test_state.packet_count == 0);

    test_state.file_present = 1;
    assert(rdx_protocol_v23_local_storage_handle_command(
               getlist, sizeof(getlist) - 1) == 1);
    assert(test_state.packet_count == 2);
    rdx_protocol_v23_local_storage_handle_link_change(0);
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               listreq, sizeof(listreq) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#listreq#1#0#0#");
}

static void test_list_generation_and_invalid_pack(void)
{
    static const uint8_t getlist[] = "*APP#getlist#";
    static const uint8_t pack_one[] = "*APP#listreq#0#1#";
    static const uint8_t invalid_pack[] = "*APP#listreq#1#99#";

    test_state.file_present = 1;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               getlist, sizeof(getlist) - 1) == 1);
    assert(test_state.packet_count == 2);
    test_state.storage_generation++;

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               pack_one, sizeof(pack_one) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#listreq#1#0#0#");

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               getlist, sizeof(getlist) - 1) == 1);
    assert(test_state.packet_count == 2);
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               invalid_pack, sizeof(invalid_pack) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#listreq#99#0#0#");
}

static void test_upload_timeout_disconnect_and_errors(void)
{
    static const uint8_t first_pack[] = "*APP#opusfile#0#7#1#0#";
    static const uint8_t second_pack[] = "*APP#opusfile#1#7#2#0#";
    static const uint8_t cancel[] = "*APP#opusfile#1#7#0#0#";
    static const uint8_t invalid_offset[] =
        "*APP#opusfile#0#7#1#0#999#";
    uint32_t opens_before;
    uint32_t closes_before;
    uint32_t owner;
    uint32_t token;

    test_state.file_present = 1;
    test_state.file.file_size = TEST_FILE_CAPACITY;
    opens_before = test_state.upload_open_count;
    closes_before = test_state.upload_close_count;

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               first_pack, sizeof(first_pack) - 1) == 1);
    assert(test_state.packet_count == 1 && test_state.upload_active);
    rdx_protocol_v23_local_storage_handle_upload_timeout();
    assert(!test_state.upload_active);
    assert(test_state.upload_close_count == closes_before + 1);

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               second_pack, sizeof(second_pack) - 1) == 1);
    assert(test_state.packet_count == 1 && test_state.upload_active);
    assert(test_state.upload_open_count == opens_before + 2);
    rdx_protocol_v23_local_storage_handle_link_change(0);
    assert(!test_state.upload_active);
    assert(test_state.upload_close_count == closes_before + 2);

    /* The next request after disconnect establishes a fresh cursor and seeks
     * directly to the requested pack; no stale connection token is reused. */
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               second_pack, sizeof(second_pack) - 1) == 1);
    assert(test_state.packet_count == 1 && test_state.upload_active);
    assert(test_state.upload_open_count == opens_before + 3);
    rdx_protocol_v23_local_storage_handle_upload_timeout();
    assert(!test_state.upload_active);
    assert(test_state.upload_close_count == closes_before + 3);

    /* Explicit cancel also revokes a retryable FILE_CHUNK token.  A later
     * can-send callback must not leak the canceled payload or auto-delete. */
    reset_packets();
    test_state.send_result = RDX_TRANSPORT_RETRYABLE;
    assert(rdx_protocol_v23_local_storage_handle_command(
               first_pack, sizeof(first_pack) - 1) == 1);
    assert(test_state.packet_count == 0 && test_state.upload_active);
    assert(test_state.can_send_request_count == 1);
    owner = test_state.can_send_owner;
    token = test_state.can_send_token;
    assert(rdx_protocol_v23_local_storage_handle_command(
               cancel, sizeof(cancel) - 1) == 1);
    assert(!test_state.upload_active);
    assert(test_state.upload_close_count == closes_before + 4);
    test_state.send_result = RDX_TRANSPORT_ACCEPTED;
    rdx_protocol_v23_local_storage_handle_can_send(owner, token);
    assert(test_state.packet_count == 0);

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               invalid_offset, sizeof(invalid_offset) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#opusfile#7#0#1#0#0#");

    test_state.completion_result_once = RDX_STORAGE_RESULT_IO_ERROR;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               first_pack, sizeof(first_pack) - 1) == 1);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#opusfile#7#0#1#0#0#");
    assert(!test_state.upload_active);

    test_state.file.file_size = TEST_FILE_SIZE;
}

static void test_storage_busy_wire_contract(void)
{
    static const uint8_t getlist[] = "*APP#getlist#";
    static const uint8_t capacity[] = "*APP#mem#";
    static const uint8_t format[] = "*APP#format#";
    static const uint8_t delete_file[] = "*APP#delfile#7#abc.raw#";
    static const uint8_t opusfile[] = "*APP#opusfile#0#7#1#0#";

    test_state.file_present = 1;
    test_state.submit_result_once = RDX_STORAGE_RESULT_BUSY;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               getlist, sizeof(getlist) - 1) == 1);
    assert_text_packet(0, "*DEV#getlist#1#");

    test_state.submit_result_once = RDX_STORAGE_RESULT_BUSY;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               capacity, sizeof(capacity) - 1) == 1);
    assert_text_packet(0, "*DEV#mem#0#0#");

    test_state.submit_result_once = RDX_STORAGE_RESULT_BUSY;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               format, sizeof(format) - 1) == 1);
    assert_text_packet(0, "*DEV#format#1#");

    test_state.submit_result_once = RDX_STORAGE_RESULT_BUSY;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               delete_file, sizeof(delete_file) - 1) == 1);
    assert_text_packet(0, "*DEV#delfile#1#7#abc.raw#");

    test_state.submit_result_once = RDX_STORAGE_RESULT_BUSY;
    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               opusfile, sizeof(opusfile) - 1) == 1);
    assert_text_packet(0, "*DEV#opusfile#7#0#1#0#0#");
}

static void test_retryable_can_send(void)
{
    static const uint8_t capacity[] = "*APP#mem#";
    uint32_t owner;
    uint32_t token;

    reset_packets();
    test_state.send_result = RDX_TRANSPORT_RETRYABLE;
    assert(rdx_protocol_v23_local_storage_handle_command(
               capacity, sizeof(capacity) - 1) == 1);
    assert(test_state.send_attempt_count == 1);
    assert(test_state.packet_count == 0);
    assert(test_state.can_send_request_count == 1);
    owner = test_state.can_send_owner;
    token = test_state.can_send_token;

    /* The second command receives a stable transport-busy mapping deferred
     * behind the retryable packet; it cannot overwrite the first payload. */
    assert(rdx_protocol_v23_local_storage_handle_command(
               capacity, sizeof(capacity) - 1) == 1);
    assert(test_state.send_attempt_count == 1);

    rdx_protocol_v23_local_storage_handle_can_send(owner, token + 1);
    assert(test_state.send_attempt_count == 1);
    test_state.send_result = RDX_TRANSPORT_ACCEPTED;
    rdx_protocol_v23_local_storage_handle_can_send(owner, token);
    assert(test_state.send_attempt_count == 3);
    assert(test_state.packet_count == 2);
    assert_text_packet(0, "*DEV#mem#3072#8192#");
    assert_text_packet(1, "*DEV#mem#0#0#");
}

int main(void)
{
    struct rdx_local_storage_service service;
    struct rdx_protocol_v23_context context;
    uint32_t i;

    memset(&test_state, 0, sizeof(test_state));
    test_state.file_present = 1;
    test_state.file.file_id.artifact_id.value = 7;
    test_state.file.file_id.serial_number = 7;
    strcpy(test_state.file.file_id.name, "abc.raw");
    test_state.file.file_size = TEST_FILE_SIZE;
    test_state.file.audio_crc = 891568578UL;
    test_state.list_file_count = 1;
    test_state.storage_generation = 9;
    test_state.list_files[0] = test_state.file;
    for (i = 0; i < TEST_FILE_CAPACITY; i++) {
        test_state.file_data[i] = (uint8_t)(i + 1);
    }

    memset(&service, 0, sizeof(service));
    memset(&context, 0, sizeof(context));
    context.initialized = 1;
    context.session.connection_generation = 1;
    context.session.connected = 1;
    context.session.ccc_ready = 1;
    context.session.authenticated = 1;
    assert(rdx_protocol_v23_local_storage_bind_service(&service) == 0);
    assert(rdx_protocol_v23_local_storage_init(&context) == 0);

    test_list_golden_trace();
    test_list_boundaries_and_escaping();
    test_capacity_and_file_golden_trace();
    test_long_file_offset_and_auto_delete();
    test_delete_and_format_golden_trace();
    test_malformed_and_disconnect();
    test_list_generation_and_invalid_pack();
    test_upload_timeout_disconnect_and_errors();
    test_storage_busy_wire_contract();
    test_retryable_can_send();

    rdx_protocol_v23_local_storage_exit();
    assert(rdx_protocol_v23_local_storage_bind_service(NULL) == 0);
    return 0;
}
