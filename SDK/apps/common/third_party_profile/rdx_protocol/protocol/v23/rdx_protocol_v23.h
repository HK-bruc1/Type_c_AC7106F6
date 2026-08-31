#ifndef RDX_PROTOCOL_V23_H
#define RDX_PROTOCOL_V23_H

#include <stdint.h>

#include "rdx/ports/rdx_transport_port.h"

struct rdx_record_coordinator;
struct rdx_record_destination_ops;

int rdx_protocol_v23_init(void);
void rdx_protocol_v23_exit(void);
void rdx_protocol_v23_handle_transport_event(
    const struct rdx_transport_event *event, void *context);

/* Recording composition hooks used only by the product Composition Root.
 * V23 keeps ownership of the Online OPTIONAL Wire destination while the
 * Runtime owns the Local REQUIRED coordinator. */
const struct rdx_record_destination_ops *
rdx_protocol_v23_record_online_destination_ops(void);
int rdx_protocol_v23_record_bind_coordinator(
    struct rdx_record_coordinator *coordinator);

#endif
