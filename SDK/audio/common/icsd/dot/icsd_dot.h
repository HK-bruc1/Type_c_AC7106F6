#ifndef _ICSD_DOT_H
#define _ICSD_DOT_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "timer.h"
#include "math.h"
#include "icsd_common_v2.h"

#if DOT_PRINTF_EN
#define _dot_printf printf
#else
#define _dot_printf icsd_printf_off
#endif

typedef struct {
    u8    state;
    float dot_db;
    int   len;
    float *sz_db;
} _dot_output;

struct icsd_dot_infmt {
    void *alloc_ptr;     //外部申请的ram地址
};

struct icsd_dot_libfmt {
    int lib_alloc_size;  //算法ram需求大小
};

struct icsd_dot_parm {
    u8 end_point;
    u8 start_point;
    float msc_mean_thr;
    float norm_thr;
    float loose_thr;
};
extern struct icsd_dot_parm *DOT_PARM;

extern int (*dot_printf)(const char *format, ...);
void icsd_dot_get_libfmt(struct icsd_dot_libfmt *libfmt);
void icsd_dot_set_infmt(struct icsd_dot_infmt *fmt);
_dot_output *icsd_dot_run(float *sz_out, float *msc);
void icsd_dot_parm_init();

extern char lib_dot_version[];
#endif
