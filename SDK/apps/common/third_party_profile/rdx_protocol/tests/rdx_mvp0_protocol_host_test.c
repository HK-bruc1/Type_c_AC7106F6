#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "system/includes.h"
#include "rdx_appkey_verifier.h"
#include "rdx_ble_name.h"
#include "rdx_device_management.h"
#include "rdx_identity.h"
#include "rdx_mvp0_protocol.h"
#include "rdx_protocol_defs.h"
#include "rdx_system_actions.h"
#include "rdx_transport.h"
#include "rdx_transport_fake.h"

static char fake_ble_name[RDX_BLE_NAME_MAX_LEN + 1] = "IKKO";

static void assert_last_tx(const char *expected)
{
    const struct rdx_transport_fake_state *fake =
        rdx_transport_fake_get_state();
    size_t expected_len = strlen(expected);

    assert(fake->accepted_len == expected_len);
    assert(!memcmp(fake->accepted_data, expected, expected_len));
}

int rdx_appkey_verify(const u8 *payload, u16 len,
                      const char *const *expected_keys,
                      u8 expected_key_count,
                      char *plaintext_output,
                      u16 plaintext_output_size)
{
    static const char plaintext[] = "0123456789ABCDEF";

    (void)expected_keys;
    (void)expected_key_count;
    if (!payload || len != 32 || payload[0] != 'A'
        || !plaintext_output
        || plaintext_output_size < sizeof(plaintext)) {
        return -1;
    }
    memcpy(plaintext_output, plaintext, sizeof(plaintext));
    return 0;
}

const char *rdx_ble_name_get(void)
{
    return fake_ble_name;
}

int rdx_ble_name_set(const u8 *name, u16 len, u8 *changed)
{
    if (!name || len > RDX_BLE_NAME_MAX_LEN || !changed) {
        return -1;
    }
    *changed = len != strlen(fake_ble_name)
        || memcmp(name, fake_ble_name, len);
    memcpy(fake_ble_name, name, len);
    fake_ble_name[len] = '\0';
    return 0;
}

int rdx_device_management_bind(void)
{
    return 0;
}

int rdx_device_management_unbind(void)
{
    return 0;
}

int rdx_device_management_restore_defaults(void)
{
    return 0;
}

u8 rdx_device_management_get_bound(void)
{
    return 1;
}

const char *rdx_identity_get_auth_key(void)
{
    return "000000000000000000000000";
}

const char *rdx_identity_get_ble_mac_string(void)
{
    return "001122334455";
}

const char *rdx_identity_get_wifi_mac_string(void)
{
    return "000000000000";
}

const char *rdx_identity_get_label_sn(void)
{
    return "0000000000000000";
}

int rdx_system_action_get_auto_off_time_minutes(uint32_t *minutes)
{
    if (!minutes) {
        return -1;
    }
    *minutes = 0;
    return 0;
}

u16 sys_timeout_add(void *priv, void (*callback)(void *), u32 timeout_ms)
{
    (void)priv;
    (void)callback;
    (void)timeout_ms;
    return 1;
}

void sys_timeout_del(u16 timer_id)
{
    (void)timer_id;
}

static void make_appkey_request(uint8_t *request, uint16_t *len)
{
    static const char prefix[] = "*APP#appkey#";

    memcpy(request, prefix, sizeof(prefix) - 1);
    memset(request + sizeof(prefix) - 1, 'A', 32);
    request[sizeof(prefix) - 1 + 32] = '#';
    *len = sizeof(prefix) - 1 + 32 + 1;
}

static void test_protocol_transport_sequence(void)
{
    static const uint8_t ping[] = "RDX_MVP0_PING";
    static const uint8_t auth_sn[] = "*APP#authsn#";
    static const uint8_t offtime_check[] = "*APP#cofftime#";
    uint8_t appkey_request[64];
    const struct rdx_transport_fake_state *fake;
    uint16_t appkey_len;
    unsigned int accepted_before;
    unsigned int attempts_before;
    uint32_t first_generation;
    uint32_t second_generation;

    assert(rdx_mvp0_protocol_init() == 0);
    assert(rdx_transport_init(rdx_mvp0_protocol_handle_transport_event,
                              NULL) == 0);
    fake = rdx_transport_fake_get_state();
    make_appkey_request(appkey_request, &appkey_len);

    first_generation = rdx_transport_fake_connect();
    rdx_transport_fake_receive(first_generation, ping, sizeof(ping) - 1);
    assert(fake->send_attempt_count == 0);

    rdx_transport_fake_set_ccc(first_generation, 1);
    rdx_transport_fake_receive(first_generation, ping, sizeof(ping) - 1);
    assert(fake->accepted_count == 1);
    assert_last_tx("RDX_MVP0_PONG");

    accepted_before = fake->accepted_count;
    rdx_transport_fake_receive(first_generation,
                               auth_sn, sizeof(auth_sn) - 1);
    assert(fake->accepted_count == accepted_before);

    rdx_transport_fake_receive(first_generation,
                               appkey_request, appkey_len);
    assert_last_tx("*DEV#appkey#0#");
    rdx_transport_fake_receive(first_generation,
                               auth_sn, sizeof(auth_sn) - 1);
    assert(fake->accepted_count == accepted_before + 2);
    assert(fake->accepted_len > strlen("*DEV#authsn#"));
    assert(!memcmp(fake->accepted_data, "*DEV#authsn#",
                   strlen("*DEV#authsn#")));

    rdx_transport_fake_receive(first_generation,
                               offtime_check,
                               sizeof(offtime_check) - 1);
    assert_last_tx("*DEV#cofftime#0#0#");

    rdx_transport_fake_set_ccc(first_generation, 0);
    accepted_before = fake->accepted_count;
    rdx_transport_fake_receive(first_generation,
                               auth_sn, sizeof(auth_sn) - 1);
    assert(fake->accepted_count == accepted_before);

    second_generation = rdx_transport_fake_connect();
    rdx_transport_fake_receive(first_generation, ping, sizeof(ping) - 1);
    assert(fake->accepted_count == accepted_before);
    rdx_transport_fake_set_ccc(second_generation, 1);

    rdx_transport_fake_set_send_result(RDX_TRANSPORT_RETRYABLE);
    attempts_before = fake->send_attempt_count;
    rdx_transport_fake_receive(second_generation,
                               appkey_request, appkey_len);
    assert(fake->send_attempt_count == attempts_before + 2);
    assert(fake->accepted_count == accepted_before);
    assert(fake->disconnect_count == 1);
    assert(!fake->connected);

    rdx_mvp0_protocol_exit();
    rdx_transport_exit();
}

int main(void)
{
    test_protocol_transport_sequence();
    puts("RDX MVP0 protocol host tests passed");
    return 0;
}
