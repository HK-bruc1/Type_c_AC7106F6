#ifndef _ICSD_WAT_H
#define _ICSD_WAT_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "timer.h"
#include "math.h"
#include "icsd_common.h"
#include "icsd_common_v2.h"

#if WAT_PRINTF_EN
#define _wat_printf printf
#else
#define _wat_printf icsd_printf_off
#endif
extern int (*wat_printf)(const char *format, ...);

struct icsd_wat_libfmt {
    int lib_alloc_size;  //算法ram需求大小
};

struct icsd_wat_infmt {
    void *alloc_ptr;     //外部申请的ram地址
    int lib_alloc_size;  //算法ram需求大小
};

typedef struct {
    float wat_pn_gain;
    float wat_bypass_gain;
    float wat_anc_gain;
} __wat_config;

struct wat_function {
    void (*wat_config_init)(__wat_config *_wat_config);
    void (*tws_tx_unsniff_req)();
};
extern struct wat_function *WAT_FUNC;

typedef struct {
    s16 *data_1_ptr;
    u8 resume_mode;
    u8 anc_onoff;
    float err_gain;
} __icsd_wat_run_parm;

typedef struct {
    u8 wat_output;
    u8 get_max_range;
    int max_range;
} __icsd_wat_output;

void icsd_wat_get_libfmt(struct icsd_wat_libfmt *libfmt);
void icsd_wat_set_infmt(struct icsd_wat_infmt *fmt);
void icsd_alg_wat_run(__icsd_wat_run_parm *run_parm, __icsd_wat_output *output);
void icsd_wat_ram_clean();

extern const u8 wat_data_en;

extern char lib_wat_version[];
#endif
