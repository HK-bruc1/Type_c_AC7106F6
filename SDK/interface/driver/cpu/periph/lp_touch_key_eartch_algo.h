#ifndef  __LP_TOUCH_KEY_EARTCH_ALGO_H__
#define  __LP_TOUCH_KEY_EARTCH_ALGO_H__


#include "typedef.h"


void lp_touch_key_eartch_algorithm_init(u32 group, u32 is_ref, u16 cfg0, u16 cfg1, u16 cfg2);

void lp_touch_key_eartch_algorithm_reset(u32 group);

void lp_touch_key_eartch_algorithm_set_dc_trim(u32 group, u32 ch_dc, u32 ref_dc);

void lp_touch_key_eartch_algorithm_set_kvld_trim(u32 group, u32 kvld);

void lp_touch_key_eartch_algorithm_set_scale(u32 group, u32 scale);

void lp_touch_key_eartch_algorithm_set_state(u32 group, u32 state);

u32 lp_touch_key_eartch_algorithm_get_state(u32 group);

u32 lp_touch_key_eartch_algorithm_analyze(u32 group, u16 ch_res, u16 ref_ch_res);


#endif

