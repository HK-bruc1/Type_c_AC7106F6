#ifndef _ICSD_HOWL_H
#define _ICSD_HOWL_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "timer.h"
#include "math.h"
#include "icsd_common.h"
#include "icsd_common_v2.h"

extern const u8 howl_log_en;
#define sd_howl_log(format, ...)  if(howl_log_en){{if(config_ulog_enable){printf(format, ##__VA_ARGS__);}if(config_dlog_enable){dlog_printf((-1 & ~BIT(31)), format, ##__VA_ARGS__);}}}

#if HOWL_PRINTF_EN
#define _howl_printf printf
#else
#define _howl_printf icsd_printf_off
#endif
extern int (*howl_printf)(const char *format, ...);

struct icsd_howl_libfmt {
    int lib_alloc_size;  //算法ram需求大小
};

struct icsd_howl_infmt {
    void *alloc_ptr;     //外部申请的ram地址
    int lib_alloc_size;  //算法ram需求大小
    int debug_ram_size;
};

typedef struct {
    u8    hd_mic_sel;//0:REF 1:ERR
    u8    hd_mode;
    u8    hd_rise_cnt;
    float hd_rise_pwr;
    float hd_scale;
    int   hd_sat_thr;
    float hd_det_thr;
    float hd_diff_pwr_thr;
    int   hd_maxind_thr1;
    int   hd_maxind_thr2;
    float hd_maxvld;
    float hd_maxvld1;
    float hd_maxvld2;
    float hd_path1_thr;
    float hd_maxvld2nd;
    float loud_thr;
    float loud_thr1;
    float loud_thr2;
    float hd_det_cnt;
    float hd_step_pwr;
    float harm_rel_thr_l;
    float harm_rel_thr_h;
    float maxvld_lat_t2_add;
    float param1;
    float param2;
    float param3;
} __howl_config;

struct howl_function {
    void (*howl_config_init)(__howl_config *_howl_config);
    void (*anc_debug_free)(void *pv);
    void *(*anc_debug_malloc)(const char *name, int size);
};
extern struct howl_function *HOWL_FUNC;

typedef struct {
    s16 *ref;
    s16 *anco;
} __icsd_howl_run_parm;

typedef struct {
    u8 howl_output;
} __icsd_howl_output;

void icsd_howl_get_libfmt(struct icsd_howl_libfmt *libfmt);
void icsd_howl_set_infmt(struct icsd_howl_infmt *fmt);
void icsd_alg_howl_run(__icsd_howl_run_parm *run_parm, __icsd_howl_output *output);

u8 icsd_howl_mic_sel();
void icsd_howl_debug_run();
void icsd_howl_ft_debug_run();
u8 icsd_howl_debug_start(u8 howl_output);
u8 icsd_howl_ft_debug_start(u8 howl_output);
void icsd_alg_howl_ftdebug_free();
extern const u8 icsd_howl_debug;
extern const u8 howl_debug_dlen;
extern const u8 howl_ft_debug;
extern const u8 howl_train_en;
extern const u8 howl_train_debug;
extern const u8 howl_parm_correction;
extern char lib_howl_version[];

#endif

