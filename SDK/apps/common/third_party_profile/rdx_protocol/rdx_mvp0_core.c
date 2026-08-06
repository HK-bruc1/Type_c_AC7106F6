#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_mvp0_core.h"

static const u8 rdx_mvp0_ping[] = "RDX_MVP0_PING";
static const u8 rdx_mvp0_pong[] = "RDX_MVP0_PONG";

static rdx_mvp0_send_callback_t rdx_mvp0_send_callback;
static u8 rdx_mvp0_connected;

void rdx_mvp0_core_init(rdx_mvp0_send_callback_t send_callback)
{
    rdx_mvp0_send_callback = send_callback;
    rdx_mvp0_connected = 0;
}

void rdx_mvp0_core_exit(void)
{
    rdx_mvp0_send_callback = NULL;
    rdx_mvp0_connected = 0;
}

void rdx_mvp0_core_set_connected(u8 connected)
{
    rdx_mvp0_connected = !!connected;
    printf("[RDX] link %s\n", rdx_mvp0_connected ? "connected" : "disconnected");
}

void rdx_mvp0_core_receive(const u8 *data, u16 len)
{
    if (!data || !len) {
        return;
    }

    printf("[RDX] rx len=%u type=%02x%02x\n", len, data[0], len > 1 ? data[1] : 0);

    if (len == sizeof(rdx_mvp0_ping) - 1
        && !memcmp(data, rdx_mvp0_ping, sizeof(rdx_mvp0_ping) - 1)
        && rdx_mvp0_connected
        && rdx_mvp0_send_callback) {
        rdx_mvp0_send_callback(rdx_mvp0_pong, sizeof(rdx_mvp0_pong) - 1);
    }
}

#endif
