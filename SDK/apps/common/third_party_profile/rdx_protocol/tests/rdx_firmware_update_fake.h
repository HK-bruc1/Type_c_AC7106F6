#ifndef RDX_FIRMWARE_UPDATE_FAKE_H
#define RDX_FIRMWARE_UPDATE_FAKE_H

#include <stdint.h>

#include "rdx/ports/rdx_firmware_update_port.h"

#define RDX_FIRMWARE_UPDATE_FAKE_DATA_SIZE      2048

struct rdx_firmware_update_fake_state {
    enum rdx_firmware_update_result begin_result;
    rdx_firmware_update_event_callback_t event_callback;
    void *event_context;
    rdx_firmware_update_timeout_callback_t timeout_callback;
    void *timeout_context;
    uint32_t image_size;
    uint32_t watchdog_ms;
    unsigned int begin_count;
    unsigned int write_count;
    unsigned int finish_count;
    unsigned int abort_count;
    unsigned int reset_count;
    uint16_t data_len;
    uint8_t active;
    uint8_t data[RDX_FIRMWARE_UPDATE_FAKE_DATA_SIZE];
};

void rdx_firmware_update_fake_reset(void);
const struct rdx_firmware_update_fake_state *
rdx_firmware_update_fake_get_state(void);
void rdx_firmware_update_fake_emit(
    enum rdx_firmware_update_event_type type, int platform_result);
void rdx_firmware_update_fake_fire_timeout(void);

#endif /* RDX_FIRMWARE_UPDATE_FAKE_H */
