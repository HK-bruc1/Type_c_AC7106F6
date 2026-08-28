#ifndef RDX_FACTORY_IDENTITY_PORT_H
#define RDX_FACTORY_IDENTITY_PORT_H

#include <stdint.h>

/* The burner stores an auth-code block in the final 256-byte flash page. */
#define RDX_AUTH_CODE_BLOCK_SIZE             256
#define RDX_AUTH_CODE_BLOCK_OFFSET           (64 + 16)
#define RDX_AUTH_CODE_MAX_PAYLOAD_LEN        128

/* Provider errors are kept separate from record-format errors. */
#define RDX_AUTH_CODE_ERR_INVALID_ARGUMENT   (-101)
#define RDX_AUTH_CODE_ERR_INVALID_CAPACITY   (-102)
#define RDX_AUTH_CODE_ERR_ADDRESS_MAP        (-103)
#define RDX_AUTH_CODE_ERR_INVALID_LENGTH     (-104)
#define RDX_AUTH_CODE_ERR_BUFFER_TOO_SMALL   (-105)
#define RDX_AUTH_CODE_ERR_CRC                (-106)
/* The reserved block is erased/unprogrammed; production data is optional
 * until the burner writes it. Identity Service may use its development
 * identity fallback (zero AuthKey/Label SN plus the SDK BLE address). */
#define RDX_AUTH_CODE_ERR_NOT_PRESENT        (-107)

/*
 * Return the validated raw payload. The provider owns flash layout, header
 * bounds and CRC only; the application layer owns the payload contract.
 * RDX_AUTH_CODE_ERR_NOT_PRESENT is reserved for an erased/unprogrammed block;
 * other non-zero results remain hard read/validation diagnostics. Identity
 * Service owns the product policy for falling back when they occur.
 */
int rdx_platform_read_auth_code(uint8_t *buffer,
                                uint16_t buffer_size,
                                uint16_t *data_len);

#endif
