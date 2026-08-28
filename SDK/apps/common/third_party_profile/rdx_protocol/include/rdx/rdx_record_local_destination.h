#ifndef RDX_RECORD_LOCAL_DESTINATION_H
#define RDX_RECORD_LOCAL_DESTINATION_H

#include "rdx/rdx_local_storage_service.h"
#include "rdx/rdx_record_types.h"

struct rdx_record_local_destination {
    struct rdx_local_storage_service *service;
};

int rdx_record_local_destination_init(
    struct rdx_record_local_destination *destination,
    struct rdx_local_storage_service *service);
const struct rdx_record_destination_ops *
rdx_record_local_destination_ops(void);

#endif /* RDX_RECORD_LOCAL_DESTINATION_H */
