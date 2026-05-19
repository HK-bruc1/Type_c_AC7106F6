
/*
 * Copyright (c) 2025 ELEVOC. All Rights Reserved.
 *
 * This source code is proprietary and confidential to ELEVOC.
 * Unauthorized copying, modification, distribution, or use of this code,
 * in whole or in part, is strictly prohibited without the express written
 * permission of ELEVOC.
 */
#ifndef ELE_VOCPLUS_API_H
#define ELE_VOCPLUS_API_H


#include "ele_typedef.h"

#ifdef __cplusplus
extern "C" {
#endif


ele_state elevoc_vocplus_set_param(ele_uint32 t_id, void *data, ele_uint32 t_size);
ele_state elevoc_vocplus_process(short **near_in, short **far_in, short **near_out, unsigned int size);
ele_state elevoc_vocplus_release(void);
ele_state elevoc_vocplus_tm_initial(int sample_rate);

#ifdef __cplusplus
}
#endif

#endif /* ELE_VOCPLUS_API_H */
