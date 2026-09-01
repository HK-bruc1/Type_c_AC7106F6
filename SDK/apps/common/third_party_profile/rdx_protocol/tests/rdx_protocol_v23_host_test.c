#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "system/includes.h"
#include "rdx/rdx_appkey_verifier.h"
#include "rdx/rdx_ble_name_service.h"
#include "rdx/rdx_device_management_service.h"
#include "rdx/rdx_identity_service.h"
#include "rdx_protocol/protocol/v23/rdx_protocol_v23.h"
#include "rdx/rdx_wire_defs.h"
#include "rdx/ports/rdx_system_action_port.h"
#include "rdx/ports/rdx_transport_port.h"
#include "rdx_firmware_update_fake.h"
#include "rdx_transport_fake.h"

static char fake_ble_name[RDX_BLE_NAME_MAX_LEN + 1] = "IKKO";

struct fake_timeout_state {
    void (*callback)(void *);
    void *priv;
    u16 active_id;
    u16 next_id;
    u32 timeout_ms;
    unsigned int delete_count;
};

static struct fake_timeout_state fake_timeout;

static void assert_last_tx(const char *expected)
{
    const struct rdx_transport_fake_state *fake =
        rdx_transport_fake_get_state();
    size_t expected_len = strlen(expected);

    assert(fake->accepted_len == expected_len);
    assert(!memcmp(fake->accepted_data, expected, expected_len));
}

static void assert_last_ota_tx(const char *expected)
{
    const struct rdx_transport_fake_state *fake =
        rdx_transport_fake_get_state();
    size_t expected_len = strlen(expected);

    assert(fake->ota_accepted_len == expected_len);
    assert(!memcmp(fake->ota_accepted_data, expected, expected_len));
}

static uint32_t test_crc32(const uint8_t *data, uint16_t len)
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
    fake_timeout.next_id++;
    if (!fake_timeout.next_id) {
        fake_timeout.next_id = 1;
    }
    fake_timeout.callback = callback;
    fake_timeout.priv = priv;
    fake_timeout.timeout_ms = timeout_ms;
    fake_timeout.active_id = fake_timeout.next_id;
    return fake_timeout.active_id;
}

void sys_timeout_del(u16 timer_id)
{
    if (timer_id && timer_id == fake_timeout.active_id) {
        fake_timeout.active_id = 0;
        fake_timeout.delete_count++;
    }
}

static void fake_timeout_reset(void)
{
    memset(&fake_timeout, 0, sizeof(fake_timeout));
}

static void fake_timeout_fire(void)
{
    void (*callback)(void *) = fake_timeout.callback;
    void *priv = fake_timeout.priv;

    assert(fake_timeout.active_id);
    assert(callback);
    fake_timeout.active_id = 0;
    callback(priv);
}

static void make_appkey_request(uint8_t *request, uint16_t *len)
{
    static const char prefix[] = "*APP#appkey#";

    memcpy(request, prefix, sizeof(prefix) - 1);
    memset(request + sizeof(prefix) - 1, 'A', 32);
    request[sizeof(prefix) - 1 + 32] = '#';
    *len = sizeof(prefix) - 1 + 32 + 1;
}

static uint32_t connect_and_authenticate(void)
{
    uint8_t appkey_request[64];
    uint16_t appkey_len;
    uint32_t generation;

    make_appkey_request(appkey_request, &appkey_len);
    generation = rdx_transport_fake_connect();
    assert(generation);
    rdx_transport_fake_set_ccc(generation, 1);
    rdx_transport_fake_receive(generation,
                               appkey_request, appkey_len);
    assert_last_tx("*DEV#appkey#0#");
    return generation;
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

    fake_timeout_reset();
    assert(rdx_protocol_v23_init() == 0);
    assert(rdx_transport_init(rdx_protocol_v23_handle_transport_event,
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

    rdx_protocol_v23_exit();
    rdx_transport_exit();
}

static void test_protocol_lifecycle_rejects_delayed_events(void)
{
    static const uint8_t ping[] = "RDX_MVP0_PING";
    static const uint8_t set_default[] = "*APP#default#";
    const struct rdx_transport_fake_state *fake;
    void (*stale_callback)(void *);
    void *stale_priv;
    unsigned int accepted_before;
    unsigned int disconnect_before;
    uint32_t generation;

    fake_timeout_reset();
    assert(rdx_protocol_v23_init() == 0);
    assert(rdx_transport_init(rdx_protocol_v23_handle_transport_event,
                              NULL) == 0);
    fake = rdx_transport_fake_get_state();
    generation = connect_and_authenticate();

    assert(rdx_protocol_v23_init() == 0);
    accepted_before = fake->accepted_count;
    rdx_transport_fake_receive(generation, ping, sizeof(ping) - 1);
    assert(fake->accepted_count == accepted_before + 1);
    assert_last_tx("RDX_MVP0_PONG");

    rdx_transport_fake_receive(generation,
                               set_default, sizeof(set_default) - 1);
    assert(fake_timeout.active_id);
    assert(fake_timeout.timeout_ms == 100);
    stale_callback = fake_timeout.callback;
    stale_priv = fake_timeout.priv;

    rdx_protocol_v23_exit();
    assert(fake_timeout.delete_count == 1);
    assert(!fake_timeout.active_id);

    accepted_before = fake->accepted_count;
    generation = rdx_transport_fake_connect();
    rdx_transport_fake_set_ccc(generation, 1);
    rdx_transport_fake_receive(generation, ping, sizeof(ping) - 1);
    assert(fake->accepted_count == accepted_before);

    assert(rdx_protocol_v23_init() == 0);
    generation = connect_and_authenticate();
    disconnect_before = fake->disconnect_count;
    rdx_transport_fake_receive(generation,
                               set_default, sizeof(set_default) - 1);
    assert(fake_timeout.active_id);
    assert(fake_timeout.priv != stale_priv);
    stale_callback(stale_priv);
    assert(fake->disconnect_count == disconnect_before);
    assert(fake->connected);
    assert(fake_timeout.active_id);
    fake_timeout_fire();
    assert(fake->disconnect_count == disconnect_before + 1);
    assert(!fake->connected);

    rdx_protocol_v23_exit();
    rdx_transport_exit();
}

static void test_protocol_ota_sequence_and_session_boundary(void)
{
    static const uint8_t begin[] =
        "*APP#upgrade#9.9.9#453#448#2#";
    static const uint8_t oversized_begin[] =
        "*APP#upgrade#9.9.9#449#449#1#";
    uint8_t image[453];
    uint8_t packet[512];
    const struct rdx_transport_fake_state *transport;
    const struct rdx_firmware_update_fake_state *update;
    uint32_t generation;
    unsigned int i;
    int header_len;

    for (i = 0; i < sizeof(image); i++) {
        image[i] = (uint8_t)(i * 37U + 11U);
    }
    fake_timeout_reset();
    rdx_firmware_update_fake_reset();
    assert(rdx_protocol_v23_init() == 0);
    assert(rdx_transport_init(rdx_protocol_v23_handle_transport_event,
                              NULL) == 0);
    transport = rdx_transport_fake_get_state();
    update = rdx_firmware_update_fake_get_state();

    generation = rdx_transport_fake_connect();
    rdx_transport_fake_set_ota_ccc(generation, 1);
    rdx_transport_fake_ota_write(generation,
                                 begin, sizeof(begin) - 1);
    assert(update->begin_count == 0);
    assert(transport->ota_accepted_count == 0);

    rdx_transport_fake_set_ccc(generation, 1);
    {
        uint8_t appkey_request[64];
        uint16_t appkey_len;

        make_appkey_request(appkey_request, &appkey_len);
        rdx_transport_fake_receive(generation,
                                   appkey_request, appkey_len);
    }
    rdx_transport_fake_ota_write(generation,
                                 oversized_begin,
                                 sizeof(oversized_begin) - 1);
    assert(update->begin_count == 0);
    assert_last_ota_tx("*DEV#otareq#4#0#");

    rdx_transport_fake_ota_write(generation,
                                 begin, sizeof(begin) - 1);
    assert(update->begin_count == 1);
    assert(update->image_size == sizeof(image));
    assert(update->watchdog_ms == 10000);
    assert_last_ota_tx("*DEV#otareq#0#1#");

    header_len = snprintf((char *)packet, sizeof(packet),
                          "*APP#ota#1#448#%u#",
                          (unsigned int)test_crc32(image, 448));
    assert(header_len > 0);
    assert(header_len + 448 <= sizeof(packet));
    memcpy(packet + header_len, image, 448);
    rdx_transport_fake_ota_write(
        generation, packet, header_len + 448);
    assert(update->write_count == 0);
    assert(update->finish_count == 0);
    assert_last_ota_tx("*DEV#otareq#0#2#");

    header_len = snprintf((char *)packet, sizeof(packet),
                          "*APP#ota#2#5#%u#",
                          (unsigned int)test_crc32(image + 448, 5));
    assert(header_len > 0);
    assert(header_len + 5 <= sizeof(packet));
    memcpy(packet + header_len, image + 448, 5);
    rdx_transport_fake_ota_write(generation, packet, header_len + 5);
    assert(update->write_count == 1);
    assert(update->finish_count == 1);
    assert(update->data_len == sizeof(image));
    assert(!memcmp(update->data, image, sizeof(image)));
    assert(transport->ota_accepted_count == 3);

    rdx_firmware_update_fake_emit(
        RDX_FIRMWARE_UPDATE_EVENT_VERIFIED, 0);
    assert_last_ota_tx("*DEV#otareq#0#0#");
    assert(transport->ota_accepted_count == 4);
    assert(update->reset_count == 0);

    rdx_firmware_update_fake_emit(
        RDX_FIRMWARE_UPDATE_EVENT_COMMITTED, 0);
    assert_last_ota_tx("*DEV#otareq#0#0#");
    assert(transport->ota_accepted_count == 5);
    assert(update->reset_count == 1);

    rdx_protocol_v23_exit();
    rdx_transport_exit();

    fake_timeout_reset();
    rdx_firmware_update_fake_reset();
    assert(rdx_protocol_v23_init() == 0);
    assert(rdx_transport_init(rdx_protocol_v23_handle_transport_event,
                              NULL) == 0);
    generation = connect_and_authenticate();
    rdx_transport_fake_set_ota_ccc(generation, 1);
    rdx_transport_fake_ota_write(generation,
                                 begin, sizeof(begin) - 1);
    assert(rdx_firmware_update_fake_get_state()->active);
    rdx_transport_fake_set_ota_ccc(generation, 0);
    assert(rdx_firmware_update_fake_get_state()->abort_count == 1);
    assert(!rdx_firmware_update_fake_get_state()->active);

    rdx_transport_fake_set_ota_ccc(generation, 1);
    rdx_transport_fake_ota_write(generation,
                                 begin, sizeof(begin) - 1);
    header_len = snprintf((char *)packet, sizeof(packet),
                          "*APP#ota#1#447#%u#",
                          (unsigned int)test_crc32(image, 447));
    assert(header_len > 0);
    memcpy(packet + header_len, image, 447);
    rdx_transport_fake_ota_write(generation,
                                 packet, header_len + 447);
    assert_last_ota_tx("*DEV#otareq#2#0#");
    assert(rdx_firmware_update_fake_get_state()->abort_count == 2);

    rdx_transport_fake_ota_write(generation,
                                 begin, sizeof(begin) - 1);
    assert(rdx_firmware_update_fake_get_state()->active);
    rdx_transport_disconnect();
    assert(rdx_firmware_update_fake_get_state()->abort_count == 3);
    assert(!rdx_firmware_update_fake_get_state()->active);

    rdx_protocol_v23_exit();
    rdx_transport_exit();
}

int main(void)
{
    test_protocol_transport_sequence();
    test_protocol_lifecycle_rejects_delayed_events();
    test_protocol_ota_sequence_and_session_boundary();
    puts("RDX V23 protocol host tests passed");
    return 0;
}
