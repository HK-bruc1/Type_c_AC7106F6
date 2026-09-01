#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_OTA_ENABLE

#include "rdx/ports/rdx_firmware_update_port.h"
#include "rdx/rdx_ota_service.h"
#if RDX_CFG_ONLINE_RECORDING_ENABLE
#include "rdx/rdx_record_engine.h"
#endif

#define RDX_OTA_FLASH_BLOCK_SIZE                1024
#define RDX_OTA_DATA_TIMEOUT_MS                 10000
#define RDX_OTA_RESET_DELAY_MS                  1000

enum rdx_ota_runtime_state {
    RDX_OTA_RUNTIME_IDLE = 0,
    RDX_OTA_RUNTIME_RECEIVING,
    RDX_OTA_RUNTIME_FINALIZING,
    RDX_OTA_RUNTIME_COMMITTED,
};

struct rdx_ota_runtime {
    enum rdx_ota_runtime_state state;
    uint32_t current_firmware_version;
    uint32_t connection_generation;
    uint32_t image_size;
    uint32_t packet_count;
    uint32_t next_packet_number;
    uint32_t received_size;
    uint16_t packet_size;
    uint16_t flash_buffer_size;
    uint8_t flash_buffer[RDX_OTA_FLASH_BLOCK_SIZE];
    rdx_ota_service_event_callback_t event_callback;
    void *event_context;
};

static struct rdx_ota_runtime rdx_ota;

static void rdx_ota_emit(enum rdx_ota_wire_state wire_state,
                         uint32_t next_packet_number)
{
    struct rdx_ota_service_event event;

    if (!rdx_ota.event_callback || !rdx_ota.connection_generation) {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.connection_generation = rdx_ota.connection_generation;
    event.next_packet_number = next_packet_number;
    event.wire_state = wire_state;
    rdx_ota.event_callback(&event, rdx_ota.event_context);
}

static void rdx_ota_reset_session(void)
{
    rdx_ota.state = RDX_OTA_RUNTIME_IDLE;
    rdx_ota.connection_generation = 0;
    rdx_ota.image_size = 0;
    rdx_ota.packet_count = 0;
    rdx_ota.next_packet_number = 0;
    rdx_ota.received_size = 0;
    rdx_ota.packet_size = 0;
    rdx_ota.flash_buffer_size = 0;
    memset(rdx_ota.flash_buffer, 0, sizeof(rdx_ota.flash_buffer));
}

static void rdx_ota_abort(void)
{
    rdx_firmware_update_watchdog_cancel();
    rdx_firmware_update_abort();
    rdx_ota_reset_session();
}

static enum rdx_ota_service_result rdx_ota_fail(
    enum rdx_ota_wire_state wire_state,
    enum rdx_ota_service_result result,
    const char *reason)
{
    printf("[RDX][OTA] failed reason=%s state=%u next=%u received=%u\n",
           reason, wire_state,
           (unsigned int)rdx_ota.next_packet_number,
           (unsigned int)rdx_ota.received_size);
    rdx_ota_emit(wire_state, 0);
    rdx_ota_abort();
    return result;
}

static uint32_t rdx_ota_crc32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = 0xffffffffUL;
    uint16_t i;
    uint8_t bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (crc >> 1)
                ^ (0xedb88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

static enum rdx_ota_service_result rdx_ota_flush_flash_buffer(void)
{
    enum rdx_firmware_update_result result;

    if (!rdx_ota.flash_buffer_size) {
        return RDX_OTA_SERVICE_OK;
    }
    result = rdx_firmware_update_write(rdx_ota.flash_buffer,
                                       rdx_ota.flash_buffer_size);
    if (result != RDX_FIRMWARE_UPDATE_OK) {
        return RDX_OTA_SERVICE_ERR_PLATFORM;
    }
    memset(rdx_ota.flash_buffer, 0, sizeof(rdx_ota.flash_buffer));
    rdx_ota.flash_buffer_size = 0;
    return RDX_OTA_SERVICE_OK;
}

static void rdx_ota_timeout(void *context)
{
    uint32_t generation = (uint32_t)(uintptr_t)context;

    if (rdx_ota.state != RDX_OTA_RUNTIME_RECEIVING
        || generation != rdx_ota.connection_generation) {
        return;
    }
    rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                 RDX_OTA_SERVICE_ERR_PLATFORM, "data_timeout");
}

static void rdx_ota_handle_firmware_event(
    const struct rdx_firmware_update_event *event, void *context)
{
    uint32_t generation = (uint32_t)(uintptr_t)context;

    if (!event || rdx_ota.state == RDX_OTA_RUNTIME_IDLE
        || generation != rdx_ota.connection_generation) {
        return;
    }

    switch (event->type) {
    case RDX_FIRMWARE_UPDATE_EVENT_VERIFIED:
        if (rdx_ota.state != RDX_OTA_RUNTIME_FINALIZING) {
            rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                         RDX_OTA_SERVICE_ERR_PLATFORM,
                         "verified_in_wrong_state");
            break;
        }
        printf("[RDX][OTA] image_verified image_size=%u packets=%u\n",
               (unsigned int)rdx_ota.image_size,
               (unsigned int)rdx_ota.packet_count);
        rdx_ota_emit(RDX_OTA_WIRE_STATE_OK, 0);
        break;
    case RDX_FIRMWARE_UPDATE_EVENT_COMMITTED:
        rdx_firmware_update_watchdog_cancel();
        rdx_ota.state = RDX_OTA_RUNTIME_COMMITTED;
        printf("[RDX][OTA] committed image_size=%u packets=%u\n",
               (unsigned int)rdx_ota.image_size,
               (unsigned int)rdx_ota.packet_count);
        rdx_ota_emit(RDX_OTA_WIRE_STATE_OK, 0);
        if (rdx_firmware_update_schedule_reset(RDX_OTA_RESET_DELAY_MS)) {
            printf("[RDX][OTA] reset_schedule_failed\n");
        }
        break;
    case RDX_FIRMWARE_UPDATE_EVENT_WRITE_FAILED:
        rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                     RDX_OTA_SERVICE_ERR_PLATFORM, "flash_write");
        break;
    case RDX_FIRMWARE_UPDATE_EVENT_VERIFY_FAILED:
        rdx_ota_fail(RDX_OTA_WIRE_STATE_VERSION_LOW,
                     RDX_OTA_SERVICE_ERR_PLATFORM, "image_verify");
        break;
    case RDX_FIRMWARE_UPDATE_EVENT_COMMIT_FAILED:
    default:
        rdx_ota_fail(RDX_OTA_WIRE_STATE_VERSION_LOW,
                     RDX_OTA_SERVICE_ERR_PLATFORM, "boot_info_commit");
        break;
    }
}

int rdx_ota_service_init(uint32_t current_firmware_version,
                         rdx_ota_service_event_callback_t callback,
                         void *context)
{
    if (!current_firmware_version || !callback
        || rdx_firmware_update_check_structure()) {
        return -1;
    }
    memset(&rdx_ota, 0, sizeof(rdx_ota));
    rdx_ota.current_firmware_version = current_firmware_version;
    rdx_ota.event_callback = callback;
    rdx_ota.event_context = context;
    return 0;
}

void rdx_ota_service_exit(void)
{
    rdx_ota_abort();
    memset(&rdx_ota, 0, sizeof(rdx_ota));
}

enum rdx_ota_service_result rdx_ota_service_begin(
    const struct rdx_ota_begin_request *request)
{
    enum rdx_firmware_update_result platform_result;
    uint32_t expected_packet_count;

    if (!request || !request->connection_generation
        || !request->firmware_version || !request->image_size
        || !request->packet_count || !request->packet_size
        || request->packet_size > RDX_OTA_MAX_PACKET_DATA_SIZE) {
        return RDX_OTA_SERVICE_ERR_ARGUMENT;
    }
    expected_packet_count =
        request->image_size / request->packet_size
        + !!(request->image_size % request->packet_size);
    if (expected_packet_count != request->packet_count) {
        return RDX_OTA_SERVICE_ERR_ARGUMENT;
    }
    if (rdx_ota.state != RDX_OTA_RUNTIME_IDLE) {
        return RDX_OTA_SERVICE_ERR_BUSY;
    }
#if RDX_CFG_ONLINE_RECORDING_ENABLE
    if (rdx_record_engine_get_state() != RDX_RECORD_STATE_IDLE) {
        return RDX_OTA_SERVICE_ERR_BUSY;
    }
#endif

    rdx_ota.connection_generation = request->connection_generation;
    if (request->firmware_version <= rdx_ota.current_firmware_version) {
        rdx_ota_emit(RDX_OTA_WIRE_STATE_VERSION_LOW, 0);
        rdx_ota.connection_generation = 0;
        return RDX_OTA_SERVICE_ERR_VERSION;
    }

    platform_result = rdx_firmware_update_begin(
        request->image_size, rdx_ota_handle_firmware_event,
        (void *)(uintptr_t)request->connection_generation);
    if (platform_result != RDX_FIRMWARE_UPDATE_OK) {
        rdx_ota_emit(platform_result == RDX_FIRMWARE_UPDATE_ERR_NO_SPACE
                         ? RDX_OTA_WIRE_STATE_NO_SPACE
                         : RDX_OTA_WIRE_STATE_OTHER,
                     0);
        rdx_ota.connection_generation = 0;
        return platform_result == RDX_FIRMWARE_UPDATE_ERR_NO_SPACE
            ? RDX_OTA_SERVICE_ERR_NO_SPACE
            : RDX_OTA_SERVICE_ERR_PLATFORM;
    }

    rdx_ota.state = RDX_OTA_RUNTIME_RECEIVING;
    rdx_ota.image_size = request->image_size;
    rdx_ota.packet_count = request->packet_count;
    rdx_ota.packet_size = request->packet_size;
    rdx_ota.next_packet_number = 1;
    if (rdx_firmware_update_watchdog_arm(
            rdx_ota_timeout,
            (void *)(uintptr_t)request->connection_generation,
            RDX_OTA_DATA_TIMEOUT_MS)) {
        return rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                            RDX_OTA_SERVICE_ERR_PLATFORM,
                            "watchdog_start");
    }

    printf("[RDX][OTA] started generation=%u version=%08x"
           " image_size=%u packet_size=%u packets=%u\n",
           (unsigned int)request->connection_generation,
           (unsigned int)request->firmware_version,
           (unsigned int)request->image_size,
           request->packet_size,
           (unsigned int)request->packet_count);
    rdx_ota_emit(RDX_OTA_WIRE_STATE_OK, 1);
    return RDX_OTA_SERVICE_OK;
}

enum rdx_ota_service_result rdx_ota_service_receive(
    const struct rdx_ota_data_packet *packet)
{
    uint16_t copied = 0;
    uint16_t available;
    uint16_t copy_len;
    uint8_t is_last;

    if (!packet || !packet->data || !packet->len) {
        return RDX_OTA_SERVICE_ERR_ARGUMENT;
    }
    if (rdx_ota.state != RDX_OTA_RUNTIME_RECEIVING
        || packet->connection_generation != rdx_ota.connection_generation) {
        return RDX_OTA_SERVICE_ERR_STATE;
    }
    if (packet->packet_number != rdx_ota.next_packet_number) {
        return rdx_ota_fail(RDX_OTA_WIRE_STATE_PACKET_NUMBER,
                            RDX_OTA_SERVICE_ERR_PACKET,
                            "packet_number");
    }
    if (rdx_ota.received_size > rdx_ota.image_size
        || packet->len > rdx_ota.packet_size
        || packet->len > rdx_ota.image_size - rdx_ota.received_size) {
        return rdx_ota_fail(RDX_OTA_WIRE_STATE_DATA_LENGTH,
                            RDX_OTA_SERVICE_ERR_PACKET,
                            "packet_length");
    }
    is_last = packet->packet_number == rdx_ota.packet_count;
    if ((!is_last && packet->len != rdx_ota.packet_size)
        || (is_last
            && rdx_ota.received_size + packet->len != rdx_ota.image_size)) {
        return rdx_ota_fail(RDX_OTA_WIRE_STATE_DATA_LENGTH,
                            RDX_OTA_SERVICE_ERR_PACKET,
                            "image_length");
    }
    if (rdx_ota_crc32(packet->data, packet->len)
        != packet->expected_crc32) {
        return rdx_ota_fail(RDX_OTA_WIRE_STATE_CRC,
                            RDX_OTA_SERVICE_ERR_CRC,
                            "packet_crc32");
    }

    while (copied < packet->len) {
        available = sizeof(rdx_ota.flash_buffer)
                    - rdx_ota.flash_buffer_size;
        copy_len = packet->len - copied;
        if (copy_len > available) {
            copy_len = available;
        }
        memcpy(rdx_ota.flash_buffer + rdx_ota.flash_buffer_size,
               packet->data + copied, copy_len);
        rdx_ota.flash_buffer_size += copy_len;
        copied += copy_len;
        if (rdx_ota.flash_buffer_size == sizeof(rdx_ota.flash_buffer)
            && rdx_ota_flush_flash_buffer() != RDX_OTA_SERVICE_OK) {
            return rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                                RDX_OTA_SERVICE_ERR_PLATFORM,
                                "flash_submit");
        }
    }

    rdx_ota.received_size += packet->len;
    if (is_last) {
        if (rdx_ota_flush_flash_buffer() != RDX_OTA_SERVICE_OK) {
            return rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                                RDX_OTA_SERVICE_ERR_PLATFORM,
                                "finish_submit");
        }
        rdx_ota.state = RDX_OTA_RUNTIME_FINALIZING;
        if (rdx_firmware_update_finish()
                != RDX_FIRMWARE_UPDATE_OK) {
            return rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                                RDX_OTA_SERVICE_ERR_PLATFORM,
                                "finish_submit");
        }
        if (rdx_ota.state == RDX_OTA_RUNTIME_IDLE) {
            return RDX_OTA_SERVICE_ERR_PLATFORM;
        }
        rdx_firmware_update_watchdog_cancel();
        printf("[RDX][OTA] data_complete bytes=%u packets=%u\n",
               (unsigned int)rdx_ota.received_size,
               (unsigned int)rdx_ota.packet_count);
        return RDX_OTA_SERVICE_OK;
    }

    rdx_ota.next_packet_number++;
    if (rdx_firmware_update_watchdog_arm(
            rdx_ota_timeout,
            (void *)(uintptr_t)rdx_ota.connection_generation,
            RDX_OTA_DATA_TIMEOUT_MS)) {
        return rdx_ota_fail(RDX_OTA_WIRE_STATE_OTHER,
                            RDX_OTA_SERVICE_ERR_PLATFORM,
                            "watchdog_rearm");
    }
    rdx_ota_emit(RDX_OTA_WIRE_STATE_OK,
                 rdx_ota.next_packet_number);
    return RDX_OTA_SERVICE_OK;
}

void rdx_ota_service_handle_link_lost(uint32_t connection_generation)
{
    if ((rdx_ota.state == RDX_OTA_RUNTIME_RECEIVING
         || rdx_ota.state == RDX_OTA_RUNTIME_FINALIZING)
        && connection_generation == rdx_ota.connection_generation) {
        printf("[RDX][OTA] abort reason=link_lost generation=%u\n",
               (unsigned int)connection_generation);
        rdx_ota_abort();
    }
}

uint8_t rdx_ota_service_is_active(void)
{
    return rdx_ota.state == RDX_OTA_RUNTIME_RECEIVING
        || rdx_ota.state == RDX_OTA_RUNTIME_FINALIZING
        || rdx_ota.state == RDX_OTA_RUNTIME_COMMITTED;
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_OTA_ENABLE */
