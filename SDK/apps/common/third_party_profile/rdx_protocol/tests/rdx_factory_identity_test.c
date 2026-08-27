#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rdx/rdx_factory_identity.h"

static const char valid_record[] =
    "T2620NV20260624XHM000001,E8392C53D3C3,T2620NVXHM000001";

static void assert_valid_identity(const struct rdx_factory_identity *identity,
                                  const char *auth_key,
                                  const char *ble_mac,
                                  const char *label_sn)
{
    assert(!strcmp(identity->auth_key, auth_key));
    assert(!strcmp(identity->ble_mac_string, ble_mac));
    assert(!strcmp(identity->label_sn, label_sn));
    assert(identity->ble_mac[0] == 0xc3);
    assert(identity->ble_mac[1] == 0xd3);
    assert(identity->ble_mac[2] == 0x53);
    assert(identity->ble_mac[3] == 0x2c);
    assert(identity->ble_mac[4] == 0x39);
    assert(identity->ble_mac[5] == 0xe8);
}

static void test_valid_records(void)
{
    struct rdx_factory_identity identity;
    char record[sizeof(valid_record)];

    assert(sizeof(valid_record) - 1 == RDX_FACTORY_IDENTITY_RECORD_LEN);
    assert(!rdx_factory_identity_parse(
        (const uint8_t *)valid_record, sizeof(valid_record) - 1, &identity));
    assert_valid_identity(&identity,
                          "T2620NV20260624XHM000001",
                          "E8392C53D3C3",
                          "T2620NVXHM000001");

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[25] = 'e';
    record[26] = '8';
    record[27] = '3';
    record[28] = '9';
    record[29] = '2';
    record[30] = 'c';
    record[31] = '5';
    record[32] = '3';
    record[33] = 'd';
    record[34] = '3';
    record[35] = 'c';
    record[36] = '3';
    assert(!rdx_factory_identity_parse(
        (const uint8_t *)record, sizeof(valid_record) - 1, &identity));
    assert(!strcmp(identity.ble_mac_string, "E8392C53D3C3"));

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[sizeof(valid_record) - 1] = '\0';
    assert(!rdx_factory_identity_parse(
        (const uint8_t *)record, sizeof(valid_record), &identity));
}

static void test_length_and_delimiter_errors(void)
{
    struct rdx_factory_identity identity;
    char record[sizeof(valid_record)];

    assert(rdx_factory_identity_parse(NULL, 0, &identity)
           == RDX_FACTORY_RECORD_ERR_LENGTH);
    assert(rdx_factory_identity_parse(
               (const uint8_t *)valid_record,
               sizeof(valid_record) - 2, &identity)
           == RDX_FACTORY_RECORD_ERR_LENGTH);

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[sizeof(valid_record) - 1] = 'x';
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record), &identity)
           == RDX_FACTORY_RECORD_ERR_LENGTH);

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[24] = ';';
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_DELIMITER);
    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[37] = ';';
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_DELIMITER);
}

static void test_field_errors(void)
{
    struct rdx_factory_identity identity;
    char record[sizeof(valid_record)];

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[0] = '#';
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_AUTH_KEY);

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[25] = 'G';
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_BLE_MAC);

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    record[38] = '#';
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_LABEL_SN);

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    memcpy(record + 25, "000000000000", RDX_FACTORY_IDENTITY_MAC_TEXT_LEN);
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_MAC_VALUE);

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    memcpy(record + 25, "FFFFFFFFFFFF", RDX_FACTORY_IDENTITY_MAC_TEXT_LEN);
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_MAC_VALUE);

    memcpy(record, valid_record, sizeof(valid_record) - 1);
    memcpy(record + 25, "010000000000", RDX_FACTORY_IDENTITY_MAC_TEXT_LEN);
    assert(rdx_factory_identity_parse(
               (const uint8_t *)record, sizeof(valid_record) - 1, &identity)
           == RDX_FACTORY_RECORD_ERR_MAC_VALUE);
}

int main(void)
{
    test_valid_records();
    test_length_and_delimiter_errors();
    test_field_errors();
    puts("RDX factory identity tests passed");
    return 0;
}
