#ifndef RDX_APPKEY_VERIFIER_H
#define RDX_APPKEY_VERIFIER_H

#include "system/includes.h"

#define RDX_APPKEY_PLAINTEXT_SIZE              16

int rdx_appkey_verify(const u8 *payload, u16 len,
                      const char *const *expected_keys,
                      u8 expected_key_count,
                      char *plaintext_output,
                      u16 plaintext_output_size);

#endif
