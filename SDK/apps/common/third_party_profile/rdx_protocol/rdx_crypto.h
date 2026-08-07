#ifndef RDX_CRYPTO_H
#define RDX_CRYPTO_H

#include "system/includes.h"

int rdx_crypto_verify_appkey(const u8 *payload, u16 len,
                             const char *const *expected_keys,
                             u8 expected_key_count);

#endif
