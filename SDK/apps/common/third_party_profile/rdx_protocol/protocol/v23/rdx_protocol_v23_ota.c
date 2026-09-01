#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_OTA_ENABLE

#include "rdx/ports/rdx_product_profile_port.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx/rdx_ota_service.h"
#include "rdx/rdx_wire_defs.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23_internal.h"

#define RDX_OTA_RESPONSE_SIZE                   48

static const uint8_t rdx_cmd_ota_upgrade[] = RDX_CMD_DL_OTA_UPGRADE;
static const uint8_t rdx_cmd_ota_data[] = RDX_CMD_DL_OTA_DATA;
static struct rdx_protocol_v23_context *rdx_v23_ota_context;

static int rdx_v23_ota_parse_field(const uint8_t *data, uint16_t len,
                                   uint16_t *offset,
                                   const uint8_t **field,
                                   uint16_t *field_len)
{
    uint16_t start;
    uint16_t end;

    if (!data || !offset || !field || !field_len || *offset >= len) {
        return -1;
    }
    start = *offset;
    end = start;
    while (end < len && data[end] != '#') {
        end++;
    }
    if (end == start || end >= len) {
        return -1;
    }
    *field = data + start;
    *field_len = end - start;
    *offset = end + 1;
    return 0;
}

static int rdx_v23_ota_parse_u32(const uint8_t *data, uint16_t len,
                                 uint32_t *value)
{
    uint32_t parsed = 0;
    uint16_t i;

    if (!data || !len || !value) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        uint8_t digit;

        if (data[i] < '0' || data[i] > '9') {
            return -1;
        }
        digit = data[i] - '0';
        if (parsed > (0xffffffffUL - digit) / 10UL) {
            return -1;
        }
        parsed = parsed * 10UL + digit;
    }
    *value = parsed;
    return 0;
}

static int rdx_v23_ota_parse_version(const uint8_t *data, uint16_t len,
                                     uint32_t *version)
{
    uint32_t parts[3] = {0};
    uint8_t part = 0;
    uint8_t digits = 0;
    uint16_t i;

    if (!data || !len || !version) {
        return -1;
    }
    for (i = 0; i < len; i++) {
        if (data[i] == '.') {
            if (!digits || part >= 2) {
                return -1;
            }
            part++;
            digits = 0;
            continue;
        }
        if (data[i] < '0' || data[i] > '9') {
            return -1;
        }
        parts[part] = parts[part] * 10 + data[i] - '0';
        if (parts[part] > 255) {
            return -1;
        }
        digits++;
    }
    if (part != 2 || !digits) {
        return -1;
    }
    *version = (parts[0] << 16) | (parts[1] << 8) | parts[2];
    return *version ? 0 : -1;
}

static int rdx_v23_ota_send(uint32_t connection_generation,
                            const uint8_t *data, uint16_t len)
{
    if (!rdx_v23_ota_context || !rdx_v23_ota_context->initialized
        || connection_generation
                != rdx_v23_ota_context->session.connection_generation
        || !rdx_protocol_session_is_ota_ready(
                &rdx_v23_ota_context->session)) {
        return RDX_TRANSPORT_UNAVAILABLE;
    }
    return rdx_transport_ota_send(data, len, connection_generation);
}

static void rdx_v23_ota_send_request(uint32_t connection_generation,
                                     enum rdx_ota_wire_state state,
                                     uint32_t next_packet_number)
{
    uint8_t response[RDX_OTA_RESPONSE_SIZE];
    int len;
    int ret;

    len = snprintf((char *)response, sizeof(response),
                   RDX_CMD_UP_OTA_DATA_REQUEST "%u#%u#",
                   state, (unsigned int)next_packet_number);
    if (len <= 0 || len >= sizeof(response)) {
        return;
    }
    ret = rdx_v23_ota_send(connection_generation, response, len);
    printf("[RDX][OTA][TX] cmd=otareq state=%u next=%u len=%u ret=%d\n",
           state, (unsigned int)next_packet_number,
           (unsigned int)len, ret);
}

static void rdx_v23_ota_send_busy(uint32_t connection_generation)
{
    static const uint8_t response[] = RDX_CMD_UP_OTA_UPGRADE "1#";
    int ret;

    ret = rdx_v23_ota_send(connection_generation, response,
                           sizeof(response) - 1);
    printf("[RDX][OTA][TX] cmd=upgrade state=busy len=%u ret=%d\n",
           (unsigned int)(sizeof(response) - 1), ret);
}

static void rdx_v23_ota_service_event(
    const struct rdx_ota_service_event *event, void *context)
{
    (void)context;
    if (!event) {
        return;
    }
    rdx_v23_ota_send_request(event->connection_generation,
                             event->wire_state,
                             event->next_packet_number);
}

static void rdx_v23_ota_handle_upgrade(const uint8_t *data, uint16_t len,
                                       uint32_t connection_generation)
{
    struct rdx_ota_begin_request request;
    const uint8_t *field;
    uint16_t field_len;
    uint16_t offset = sizeof(rdx_cmd_ota_upgrade) - 1;
    uint32_t packet_size;
    enum rdx_ota_service_result result;

    memset(&request, 0, sizeof(request));
    request.connection_generation = connection_generation;
    if (rdx_v23_ota_parse_field(data, len, &offset,
                                &field, &field_len)
        || rdx_v23_ota_parse_version(field, field_len,
                                     &request.firmware_version)
        || rdx_v23_ota_parse_field(data, len, &offset,
                                   &field, &field_len)
        || rdx_v23_ota_parse_u32(field, field_len,
                                 &request.image_size)
        || rdx_v23_ota_parse_field(data, len, &offset,
                                   &field, &field_len)
        || rdx_v23_ota_parse_u32(field, field_len, &packet_size)
        || packet_size > 0xffff
        || rdx_v23_ota_parse_field(data, len, &offset,
                                   &field, &field_len)
        || rdx_v23_ota_parse_u32(field, field_len,
                                 &request.packet_count)
        || offset != len) {
        printf("[RDX][OTA][DROP] cmd=upgrade reason=bad_format len=%u\n",
               len);
        rdx_v23_ota_send_request(connection_generation,
                                 RDX_OTA_WIRE_STATE_OTHER, 0);
        return;
    }
    request.packet_size = packet_size;
    result = rdx_ota_service_begin(&request);
    if (result == RDX_OTA_SERVICE_ERR_BUSY) {
        rdx_v23_ota_send_busy(connection_generation);
    } else if (result == RDX_OTA_SERVICE_ERR_ARGUMENT) {
        rdx_v23_ota_send_request(connection_generation,
                                 RDX_OTA_WIRE_STATE_OTHER, 0);
    }
    printf("[RDX][OTA][RX] cmd=upgrade version=%08x image_size=%u"
           " packet_size=%u packets=%u result=%d\n",
           (unsigned int)request.firmware_version,
           (unsigned int)request.image_size,
           request.packet_size,
           (unsigned int)request.packet_count, result);
}

static void rdx_v23_ota_handle_data(const uint8_t *data, uint16_t len,
                                    uint32_t connection_generation)
{
    struct rdx_ota_data_packet packet;
    const uint8_t *field;
    uint16_t field_len;
    uint16_t offset = sizeof(rdx_cmd_ota_data) - 1;
    uint32_t packet_len;
    enum rdx_ota_service_result result;

    memset(&packet, 0, sizeof(packet));
    packet.connection_generation = connection_generation;
    if (rdx_v23_ota_parse_field(data, len, &offset,
                                &field, &field_len)
        || rdx_v23_ota_parse_u32(field, field_len,
                                 &packet.packet_number)
        || rdx_v23_ota_parse_field(data, len, &offset,
                                   &field, &field_len)
        || rdx_v23_ota_parse_u32(field, field_len, &packet_len)
        || packet_len > 0xffff
        || rdx_v23_ota_parse_field(data, len, &offset,
                                   &field, &field_len)
        || rdx_v23_ota_parse_u32(field, field_len,
                                 &packet.expected_crc32)
        || packet_len != len - offset) {
        printf("[RDX][OTA][DROP] cmd=ota reason=bad_format len=%u\n",
               len);
        rdx_v23_ota_send_request(connection_generation,
                                 RDX_OTA_WIRE_STATE_OTHER, 0);
        return;
    }
    packet.data = data + offset;
    packet.len = packet_len;
    result = rdx_ota_service_receive(&packet);
    printf("[RDX][OTA][RX] cmd=ota packet=%u data_len=%u result=%d\n",
           (unsigned int)packet.packet_number, packet.len, result);
}

int rdx_protocol_v23_ota_init(
    struct rdx_protocol_v23_context *context)
{
    const struct rdx_product_profile *profile;
    uint32_t current_version;

    if (!context) {
        return -1;
    }
    profile = rdx_product_profile_get();
    if (!profile || rdx_v23_ota_parse_version(
            (const uint8_t *)profile->firmware_version,
            strlen(profile->firmware_version), &current_version)) {
        return -1;
    }
    if (rdx_ota_service_init(current_version,
                             rdx_v23_ota_service_event, NULL)) {
        return -1;
    }
    rdx_v23_ota_context = context;
    return 0;
}

void rdx_protocol_v23_ota_exit(void)
{
    rdx_ota_service_exit();
    rdx_v23_ota_context = NULL;
}

void rdx_protocol_v23_ota_handle_link_change(
    uint8_t connected, uint32_t connection_generation)
{
    if (!connected) {
        rdx_ota_service_handle_link_lost(connection_generation);
    }
}

void rdx_protocol_v23_ota_handle_session_change(void)
{
    if (rdx_v23_ota_context
        && !rdx_protocol_session_is_ota_ready(
                &rdx_v23_ota_context->session)) {
        rdx_ota_service_handle_link_lost(
            rdx_v23_ota_context->session.connection_generation);
    }
}

void rdx_protocol_v23_ota_handle_write(
    const uint8_t *data, uint16_t len,
    uint32_t connection_generation)
{
    if (!data || !len || !rdx_v23_ota_context
        || connection_generation
                != rdx_v23_ota_context->session.connection_generation
        || !rdx_protocol_session_is_ota_ready(
                &rdx_v23_ota_context->session)) {
        printf("[RDX][OTA][DROP] reason=session_not_ready len=%u\n", len);
        return;
    }

    if (rdx_protocol_v23_data_starts_with(
            data, len, rdx_cmd_ota_upgrade,
            sizeof(rdx_cmd_ota_upgrade) - 1)) {
        rdx_v23_ota_handle_upgrade(data, len,
                                   connection_generation);
    } else if (rdx_protocol_v23_data_starts_with(
                   data, len, rdx_cmd_ota_data,
                   sizeof(rdx_cmd_ota_data) - 1)) {
        rdx_v23_ota_handle_data(data, len, connection_generation);
    } else {
        printf("[RDX][OTA][DROP] reason=unknown len=%u type=%02x%02x\n",
               len, data[0], len > 1 ? data[1] : 0);
    }
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_OTA_ENABLE */
