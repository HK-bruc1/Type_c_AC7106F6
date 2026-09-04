#ifndef RDX_OTA_SERVICE_H
#define RDX_OTA_SERVICE_H

#include <stdint.h>

#define RDX_OTA_MAX_PACKET_DATA_SIZE           448

enum rdx_ota_wire_state {
    RDX_OTA_WIRE_STATE_OK = 0,
    RDX_OTA_WIRE_STATE_PACKET_NUMBER = 1,
    RDX_OTA_WIRE_STATE_VERSION_LOW = 2,
    RDX_OTA_WIRE_STATE_DATA_LENGTH = 2,
    RDX_OTA_WIRE_STATE_NO_SPACE = 3,
    RDX_OTA_WIRE_STATE_CRC = 3,
    RDX_OTA_WIRE_STATE_OTHER = 4,
};

enum rdx_ota_service_result {
    RDX_OTA_SERVICE_OK = 0,
    RDX_OTA_SERVICE_ERR_ARGUMENT = -1,
    RDX_OTA_SERVICE_ERR_BUSY = -2,
    RDX_OTA_SERVICE_ERR_VERSION = -3,
    RDX_OTA_SERVICE_ERR_NO_SPACE = -4,
    RDX_OTA_SERVICE_ERR_STATE = -5,
    RDX_OTA_SERVICE_ERR_PACKET = -6,
    RDX_OTA_SERVICE_ERR_CRC = -7,
    RDX_OTA_SERVICE_ERR_PLATFORM = -8,
};

struct rdx_ota_begin_request {
    uint32_t connection_generation;
    uint32_t firmware_version;
    uint32_t image_size;
    uint32_t packet_count;
    uint16_t packet_size;
};

struct rdx_ota_data_packet {
    uint32_t connection_generation;
    uint32_t packet_number;
    uint32_t expected_crc32;
    const uint8_t *data;
    uint16_t len;
};

struct rdx_ota_service_event {
    uint32_t connection_generation;
    uint32_t next_packet_number;
    enum rdx_ota_wire_state wire_state;
};

typedef void (*rdx_ota_service_event_callback_t)(
    const struct rdx_ota_service_event *event, void *context);

int rdx_ota_service_init(uint32_t current_firmware_version,
                         rdx_ota_service_event_callback_t callback,
                         void *context);
void rdx_ota_service_exit(void);
enum rdx_ota_service_result rdx_ota_service_begin(
    const struct rdx_ota_begin_request *request);
enum rdx_ota_service_result rdx_ota_service_receive(
    const struct rdx_ota_data_packet *packet);
void rdx_ota_service_handle_link_lost(uint32_t connection_generation);
uint8_t rdx_ota_service_is_active(void);

#endif /* RDX_OTA_SERVICE_H */
