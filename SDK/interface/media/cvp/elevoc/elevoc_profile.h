
/*
 * File: elevoc_profile.h
 * Created Date: 2021-02-23
 * Author: Ruihong Luo
 * Contact: <ruihong.luo@elevoc.com>
 *
 * Last Modified: Tuesday February 23rd 2021 11:53:03 am
 *
 * Copyright (c) 2021 ELEVOC Inc.
 * All right reserved
 * -----
 * HISTORY:
 * Date      	 By	Comments
 * ----------	---	----------------------------------------------------------
 */

#ifndef ELEVOC_PROFILE_H
#define ELEVOC_PROFILE_H

#include "generic/typedef.h"
#include "ele_typedef.h"

// #define ELEVOC_TIMER2_PROFILE
typedef struct _ele_profile {
    ele_int64 tick_start;
    ele_int64 tick_total;
    ele_int64 tick_min;
    ele_int64 tick_max;
    ele_int32 prf_reload;
    ele_int32 prf_cnt;
    ele_int32 frame_len_ms;
    ele_int32 ops;
    ele_char tag[16];
} ele_profile;

int elevoc_profile_init(ele_profile *prf, ele_char *tag, ele_int32 prf_reload, ele_int32 frame_len_ms, ele_int32 ops);
void elevoc_profile_tic(ele_profile *prf);
ele_int64 elevoc_profile_toc(ele_profile *prf);

ele_int64 get_tick(void);
ele_int64 get_timer_frequency(void);
void user_timer2_init(void);
u32 get_timer2_cnt(void);
void user_print_tim2_mips(u32 tim2_ticks);

extern void put_float(double fv);
extern void clr_wdt();
extern void wdt_close();
#endif
