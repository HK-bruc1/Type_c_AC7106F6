#ifndef RDX_MVP0_PROTOCOL_H
#define RDX_MVP0_PROTOCOL_H

#include <stdint.h>

#include "rdx_transport.h"

int rdx_mvp0_protocol_init(void);
void rdx_mvp0_protocol_exit(void);
void rdx_mvp0_protocol_handle_transport_event(
    const struct rdx_transport_event *event, void *context);

#endif
