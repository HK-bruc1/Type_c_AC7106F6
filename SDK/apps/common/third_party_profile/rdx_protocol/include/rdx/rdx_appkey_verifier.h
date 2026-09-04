#ifndef RDX_APPKEY_VERIFIER_H
#define RDX_APPKEY_VERIFIER_H

#include <stdint.h>

#define RDX_APPKEY_PLAINTEXT_SIZE              16

/* plaintext_output 仅用于受控诊断，可传 NULL；认证结果不得依赖诊断输出。 */
int rdx_appkey_verify(const uint8_t *payload, uint16_t len,
                      const char *const *expected_keys,
                      uint8_t expected_key_count,
                      char *plaintext_output,
                      uint16_t plaintext_output_size);

#endif
