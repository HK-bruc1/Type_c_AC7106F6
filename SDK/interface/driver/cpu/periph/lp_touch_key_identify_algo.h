#ifndef  __LP_TOUCH_KEY_IDENTIFY_ALGO_H__
#define  __LP_TOUCH_KEY_IDENTIFY_ALGO_H__


#include "typedef.h"


u32 lp_touch_key_identify_algorithm_get_tia_size(void);

void lp_touch_key_identify_algorithm_set_tia_addr(u32 ch_idx, void *addr);

void *lp_touch_key_identify_algorithm_get_tia_addr(u32 ch_idx);

void lp_touch_key_identify_algorithm_init(u32 ch_idx, u32 temp_th, u32 edge_down_th);

u32 lp_touch_key_identify_algorithm_analyze(u32 ch_idx, u32 res);

void lp_touch_key_identify_algorithm_reinit(u32 ch_idx, u32 res);

void lp_touch_key_identify_algo_set_temp_th(u32 ch_idx, u32 new_temp_th);

void lp_touch_key_identify_algo_set_edge_down_th(u32 ch_idx, u32 new_edge_down_th);

u32 lp_touch_key_identify_algo_get_edge_down_th(u32 ch_idx);

void lp_touch_key_identify_algo_reset(u32 ch_idx);

u32 lp_touch_key_identify_algo_count_status(u32 ch_idx);

void lp_touch_key_identify_algo_get_ref_lim(u32 ch_idx, u16 *lim_l, u16 *lim_h, u32 *algo_valid);


#endif

