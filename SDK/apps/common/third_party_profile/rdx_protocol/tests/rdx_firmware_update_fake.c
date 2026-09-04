#include <string.h>

#include "rdx_firmware_update_fake.h"

static struct rdx_firmware_update_fake_state fake_update;

void rdx_firmware_update_fake_reset(void)
{
    memset(&fake_update, 0, sizeof(fake_update));
    fake_update.begin_result = RDX_FIRMWARE_UPDATE_OK;
}

const struct rdx_firmware_update_fake_state *
rdx_firmware_update_fake_get_state(void)
{
    return &fake_update;
}

void rdx_firmware_update_fake_emit(
    enum rdx_firmware_update_event_type type, int platform_result)
{
    struct rdx_firmware_update_event event;
    rdx_firmware_update_event_callback_t callback;
    void *context;

    if (!fake_update.event_callback) {
        return;
    }
    callback = fake_update.event_callback;
    context = fake_update.event_context;
    if (type != RDX_FIRMWARE_UPDATE_EVENT_VERIFIED) {
        fake_update.active = 0;
    }
    event.type = type;
    event.platform_result = platform_result;
    callback(&event, context);
}

void rdx_firmware_update_fake_fire_timeout(void)
{
    rdx_firmware_update_timeout_callback_t callback =
        fake_update.timeout_callback;
    void *context = fake_update.timeout_context;

    fake_update.timeout_callback = NULL;
    fake_update.timeout_context = NULL;
    if (callback) {
        callback(context);
    }
}

int rdx_firmware_update_check_structure(void)
{
    return 0;
}

enum rdx_firmware_update_result rdx_firmware_update_begin(
    uint32_t image_size,
    rdx_firmware_update_event_callback_t event_callback,
    void *event_context)
{
    fake_update.begin_count++;
    if (fake_update.begin_result != RDX_FIRMWARE_UPDATE_OK) {
        return fake_update.begin_result;
    }
    fake_update.image_size = image_size;
    fake_update.event_callback = event_callback;
    fake_update.event_context = event_context;
    fake_update.active = 1;
    return RDX_FIRMWARE_UPDATE_OK;
}

enum rdx_firmware_update_result rdx_firmware_update_write(
    const uint8_t *data, uint16_t len)
{
    if (!fake_update.active || !data || !len
        || fake_update.data_len + len > sizeof(fake_update.data)) {
        return RDX_FIRMWARE_UPDATE_ERR_WRITE;
    }
    memcpy(fake_update.data + fake_update.data_len, data, len);
    fake_update.data_len += len;
    fake_update.write_count++;
    return RDX_FIRMWARE_UPDATE_OK;
}

enum rdx_firmware_update_result rdx_firmware_update_finish(void)
{
    if (!fake_update.active) {
        return RDX_FIRMWARE_UPDATE_ERR_VERIFY;
    }
    fake_update.finish_count++;
    return RDX_FIRMWARE_UPDATE_OK;
}

void rdx_firmware_update_abort(void)
{
    if (fake_update.active) {
        fake_update.abort_count++;
    }
    fake_update.active = 0;
    fake_update.event_callback = NULL;
    fake_update.event_context = NULL;
}

int rdx_firmware_update_watchdog_arm(
    rdx_firmware_update_timeout_callback_t callback,
    void *context, uint32_t timeout_ms)
{
    fake_update.timeout_callback = callback;
    fake_update.timeout_context = context;
    fake_update.watchdog_ms = timeout_ms;
    return callback && timeout_ms ? 0 : -1;
}

void rdx_firmware_update_watchdog_cancel(void)
{
    fake_update.timeout_callback = NULL;
    fake_update.timeout_context = NULL;
}

int rdx_firmware_update_schedule_reset(uint32_t delay_ms)
{
    if (!delay_ms) {
        return -1;
    }
    fake_update.reset_count++;
    return 0;
}

void rdx_firmware_update_reset_now(void)
{
    fake_update.reset_count++;
}
