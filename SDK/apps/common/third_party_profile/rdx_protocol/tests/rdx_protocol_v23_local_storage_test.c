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

struct test_tx_packet {
    uint16_t len;
    uint8_t data[TEST_TX_PACKET_MAX + 1];
};

struct test_state {
    struct rdx_storage_file_info file;
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
    uint8_t file_present;
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
    memset(&completion, 0, sizeof(completion));
    completion.type = request->type;
    completion.token = request->token;
    completion.result = RDX_STORAGE_RESULT_READY;
    test_state.operation_count++;

    switch (request->type) {
    case RDX_LOCAL_STORAGE_OPERATION_CAPACITY:
        completion.total_bytes = 8ULL * 1024 * 1024;
        completion.free_bytes = test_state.file_present
                                    ? 3ULL * 1024 * 1024
                                    : 8ULL * 1024 * 1024;
        break;
    case RDX_LOCAL_STORAGE_OPERATION_ENUMERATE:
        if (test_state.file_present) {
            assert(request->data.enumerate.capacity >= 1);
            request->data.enumerate.items[0] = test_state.file;
            completion.count = 1;
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_READ_BY_SERIAL:
        assert(test_state.file_present);
        assert(request->data.read.serial_number
               == test_state.file.file_id.serial_number);
        assert(request->data.read.offset < test_state.file.file_size);
        completion.file = test_state.file;
        available = (uint32_t)test_state.file.file_size
                    - (uint32_t)request->data.read.offset;
        completion.read_length = request->data.read.length;
        if (completion.read_length > available) {
            completion.read_length = available;
        }
        memcpy(request->data.read.buffer,
               test_state.file_data + (uint32_t)request->data.read.offset,
               completion.read_length);
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
        }
        break;
    case RDX_LOCAL_STORAGE_OPERATION_FORMAT:
        test_state.file_present = 0;
        test_state.format_count++;
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

static void test_list_golden_trace(void)
{
    static const uint8_t getlist[] = "*APP#getlist#";
    static const uint8_t listreq[] = "*APP#listreq#0#1#";
    static const char list_json[] =
        "[{\"sn\":7,\"name\":\"abc.raw\",\"size\":83,"
        "\"scene\":0,\"start_time\":0,\"end_time\":0,"
        "\"crc\":\"891568578\",\"frame_size\":80,"
        "\"opus\":1084753920,\"marks\":[]}]";
    char expected[TEST_TX_PACKET_MAX + 1];
    uint32_t chunk_len = (uint32_t)strlen(list_json);
    uint32_t crc;

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               getlist, sizeof(getlist) - 1) == 1);
    assert(test_state.packet_count == 2);
    assert_text_packet(0, "*DEV#getlist#0#");
    snprintf(expected, sizeof(expected), "*DEV#flist#%u#%u#",
             (unsigned int)strlen(list_json),
             (unsigned int)((strlen(list_json) + 95) / 96));
    assert_text_packet(1, expected);

    reset_packets();
    assert(rdx_protocol_v23_local_storage_handle_command(
               listreq, sizeof(listreq) - 1) == 1);
    assert(test_state.packet_count == 1);
    if (chunk_len > 96) {
        chunk_len = 96;
    }
    crc = test_crc32((const uint8_t *)list_json, chunk_len);
    snprintf(expected, sizeof(expected), "*DEV#listreq#1#%u#%lu#",
             (unsigned int)chunk_len, (unsigned long)crc);
    assert_binary_packet(0, expected, (const uint8_t *)list_json,
                         chunk_len);
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
    assert(test_state.packet_count == 0);
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

    rdx_protocol_v23_local_storage_handle_can_send(owner, token + 1);
    assert(test_state.send_attempt_count == 1);
    test_state.send_result = RDX_TRANSPORT_ACCEPTED;
    rdx_protocol_v23_local_storage_handle_can_send(owner, token);
    assert(test_state.send_attempt_count == 2);
    assert(test_state.packet_count == 1);
    assert_text_packet(0, "*DEV#mem#3072#8192#");
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
    test_capacity_and_file_golden_trace();
    test_long_file_offset_and_auto_delete();
    test_delete_and_format_golden_trace();
    test_malformed_and_disconnect();
    test_retryable_can_send();

    rdx_protocol_v23_local_storage_exit();
    assert(rdx_protocol_v23_local_storage_bind_service(NULL) == 0);
    return 0;
}
