#ifndef _ICSD_CMP_H
#define _ICSD_CMP_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "asm/math_fast_function.h"
#include "icsd_common_v2.h"
#include "icsd_cmp_config.h"

extern int (*cmp_printf)(const char *format, ...);
#if CMP_PRINTF_EN
#define _cmp_printf printf                  //打开自适应ANC参数调试信息
#else
#define _cmp_printf icsd_printf_off
#endif

struct icsd_cmp_infmt {
    void *alloc_ptr;                     //外部申请的ram地址
};

struct icsd_cmp_libfmt {
    int lib_alloc_size;  //算法ram需求大小
};

typedef struct {
    u8 state;
    float *fgq;
} _cmp_output;

void icsd_cmp_get_libfmt(struct icsd_cmp_libfmt *libfmt);
void icsd_cmp_set_infmt(struct icsd_cmp_infmt *fmt);
void icsd_cmp_force_exit();
_cmp_output *icsd_cmp_run(float *target, __adpt_cmp_cfg *cmp_cfg);

extern char lib_cmp_version[];
#endif
