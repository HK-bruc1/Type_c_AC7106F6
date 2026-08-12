#ifndef RDX_MVP0_PROTOCOL_H
#define RDX_MVP0_PROTOCOL_H

#include "system/includes.h"

typedef int (*rdx_mvp0_send_callback_t)(const u8 *data, u16 len);
typedef void (*rdx_mvp0_disconnect_callback_t)(void);

int rdx_mvp0_protocol_init(rdx_mvp0_send_callback_t send_callback,
                           rdx_mvp0_disconnect_callback_t disconnect_callback);
void rdx_mvp0_protocol_exit(void);
void rdx_mvp0_protocol_set_connected(u8 connected);
void rdx_mvp0_protocol_set_identity_read(void);
void rdx_mvp0_protocol_set_ccc(u8 enabled);
void rdx_mvp0_protocol_receive(const u8 *data, u16 len);

#endif
