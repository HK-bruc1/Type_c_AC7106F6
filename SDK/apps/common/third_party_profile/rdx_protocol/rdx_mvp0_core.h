#ifndef RDX_MVP0_CORE_H
#define RDX_MVP0_CORE_H

#include "system/includes.h"

typedef int (*rdx_mvp0_send_callback_t)(const u8 *data, u16 len);
typedef void (*rdx_mvp0_disconnect_callback_t)(void);

void rdx_mvp0_core_init(rdx_mvp0_send_callback_t send_callback,
                        rdx_mvp0_disconnect_callback_t disconnect_callback);
void rdx_mvp0_core_exit(void);
void rdx_mvp0_core_set_connected(u8 connected);
void rdx_mvp0_core_set_identity_read(void);
void rdx_mvp0_core_set_ccc(u8 enabled);
void rdx_mvp0_core_receive(const u8 *data, u16 len);

#endif
