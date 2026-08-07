#include "app_config.h"

#if TCFG_RDX_ENABLE

#include "system/includes.h"
#include "rdx_appkey_verifier.h"

#define RDX_APPKEY_BLOCK_SIZE                 RDX_APPKEY_PLAINTEXT_SIZE
#define RDX_APPKEY_PAYLOAD_SIZE               (RDX_APPKEY_BLOCK_SIZE * 2)

extern void rdx_appkey_decryption_handle(u8 *ciphertext, u8 *key, u8 *plaintext);

int rdx_appkey_verify(const u8 *payload, u16 len,
                      const char *const *expected_keys,
                      u8 expected_key_count,
                      char *plaintext_output,
                      u16 plaintext_output_size)
{
    u8 key[RDX_APPKEY_BLOCK_SIZE];
    u8 plaintext[RDX_APPKEY_BLOCK_SIZE];
    u8 matched = 0;
    u8 i;

    if (plaintext_output && plaintext_output_size) {
        plaintext_output[0] = '\0';
    }
    if (!payload || len != RDX_APPKEY_PAYLOAD_SIZE
        || !expected_keys || !expected_key_count) {
        return -1;
    }

    memcpy(key, payload, sizeof(key));
    memset(plaintext, 0, sizeof(plaintext));
    rdx_appkey_decryption_handle((u8 *)payload + RDX_APPKEY_BLOCK_SIZE,
                                 key, plaintext);
    if (plaintext_output
        && plaintext_output_size >= RDX_APPKEY_PLAINTEXT_SIZE + 1) {
        memcpy(plaintext_output, plaintext, RDX_APPKEY_PLAINTEXT_SIZE);
        plaintext_output[RDX_APPKEY_PLAINTEXT_SIZE] = '\0';
    }
    for (i = 0; i < expected_key_count; i++) {
        if (expected_keys[i]
            && strlen(expected_keys[i]) == RDX_APPKEY_BLOCK_SIZE
            && !memcmp(plaintext, expected_keys[i], sizeof(plaintext))) {
            matched = 1;
        }
    }

    memset(key, 0, sizeof(key));
    memset(plaintext, 0, sizeof(plaintext));
    return matched ? 0 : -1;
}

#endif
