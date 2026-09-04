#ifndef RDX_ONLINE_RECORD_SERVICE_H
#define RDX_ONLINE_RECORD_SERVICE_H

#include "rdx/rdx_record_types.h"

/* Online Record Service use cases.  The return value only describes whether
 * the request was accepted by the service/engine control plane; completion is
 * reported asynchronously through the engine event callback. */
int rdx_online_record_request_start(
    enum rdx_record_scene scene,
    enum rdx_record_request_source source);
int rdx_online_record_request_stop(
    enum rdx_record_request_source source);
int rdx_online_record_request_toggle(
    enum rdx_record_scene scene,
    enum rdx_record_request_source source);

#endif
