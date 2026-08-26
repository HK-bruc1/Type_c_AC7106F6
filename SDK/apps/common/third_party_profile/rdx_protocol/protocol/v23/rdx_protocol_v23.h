#ifndef RDX_PROTOCOL_V23_H
#define RDX_PROTOCOL_V23_H

#include <stdint.h>

#include "rdx/ports/rdx_transport_port.h"

int rdx_protocol_v23_init(void);
void rdx_protocol_v23_exit(void);
void rdx_protocol_v23_handle_transport_event(
    const struct rdx_transport_event *event, void *context);

#endif
