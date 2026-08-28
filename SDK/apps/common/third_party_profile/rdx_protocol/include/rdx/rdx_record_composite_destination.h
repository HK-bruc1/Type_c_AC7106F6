#ifndef RDX_RECORD_COMPOSITE_DESTINATION_H
#define RDX_RECORD_COMPOSITE_DESTINATION_H

#include <stdint.h>

#include "rdx/rdx_record_types.h"

/*
 * Application-level fan-out.  The Engine sees one destination; this object
 * keeps Local REQUIRED and Online OPTIONAL ownership explicit.
 */
struct rdx_record_composite_destination {
    const struct rdx_record_destination_ops *local_ops;
    void *local_priv;
    const struct rdx_record_destination_ops *online_ops;
    void *online_priv;
    uint32_t session_id;
    uint32_t capture_generation;
    uint8_t local_attached;
    uint8_t online_attached;
};

int rdx_record_composite_destination_init(
    struct rdx_record_composite_destination *composite,
    const struct rdx_record_destination_ops *local_ops, void *local_priv,
    const struct rdx_record_destination_ops *online_ops, void *online_priv);
const struct rdx_record_destination_ops *
rdx_record_composite_destination_ops(void);

#endif /* RDX_RECORD_COMPOSITE_DESTINATION_H */
