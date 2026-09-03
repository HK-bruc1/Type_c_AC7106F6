#ifndef RDX_FIRMWARE_UPDATE_PORT_H
#define RDX_FIRMWARE_UPDATE_PORT_H

#include <stdint.h>

enum rdx_firmware_update_result {
    RDX_FIRMWARE_UPDATE_OK = 0,
    RDX_FIRMWARE_UPDATE_ERR_ARGUMENT = -1,
    RDX_FIRMWARE_UPDATE_ERR_INIT = -2,
    RDX_FIRMWARE_UPDATE_ERR_NO_SPACE = -3,
    RDX_FIRMWARE_UPDATE_ERR_WRITE = -4,
    RDX_FIRMWARE_UPDATE_ERR_VERIFY = -5,
    RDX_FIRMWARE_UPDATE_ERR_COMMIT = -6,
};

enum rdx_firmware_update_event_type {
    RDX_FIRMWARE_UPDATE_EVENT_WRITE_FAILED = 0,
    RDX_FIRMWARE_UPDATE_EVENT_VERIFY_FAILED,
    RDX_FIRMWARE_UPDATE_EVENT_COMMIT_FAILED,
    RDX_FIRMWARE_UPDATE_EVENT_VERIFIED,
    RDX_FIRMWARE_UPDATE_EVENT_COMMITTED,
};

struct rdx_firmware_update_event {
    enum rdx_firmware_update_event_type type;
    int platform_result;
};

typedef void (*rdx_firmware_update_event_callback_t)(
    const struct rdx_firmware_update_event *event, void *context);
typedef void (*rdx_firmware_update_timeout_callback_t)(void *context);

int rdx_firmware_update_check_structure(void);
enum rdx_firmware_update_result rdx_firmware_update_begin(
    uint32_t image_size,
    rdx_firmware_update_event_callback_t event_callback,
    void *event_context);
enum rdx_firmware_update_result rdx_firmware_update_write(
    const uint8_t *data, uint16_t len);
enum rdx_firmware_update_result rdx_firmware_update_finish(void);
void rdx_firmware_update_abort(void);
int rdx_firmware_update_watchdog_arm(
    rdx_firmware_update_timeout_callback_t callback,
    void *context, uint32_t timeout_ms);
void rdx_firmware_update_watchdog_cancel(void);
int rdx_firmware_update_schedule_reset(uint32_t delay_ms);
void rdx_firmware_update_reset_now(void);

#endif /* RDX_FIRMWARE_UPDATE_PORT_H */
