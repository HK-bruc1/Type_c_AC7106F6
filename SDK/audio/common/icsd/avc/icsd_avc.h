#ifndef _ICSD_AVC_H
#define _ICSD_AVC_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "timer.h"
#include "math.h"
#include "icsd_common.h"
#include "icsd_common_v2.h"

#if AVC_PRINTF_EN
#define _avc_printf printf
#else
#define _avc_printf icsd_printf_off
#endif
extern int (*avc_printf)(const char *format, ...);

struct icsd_avc_libfmt {
    int lib_alloc_size;  //算法ram需求大小
    u8  type;
};

struct icsd_avc_infmt {
    void *alloc_ptr;     //外部申请的ram地址
    int lib_alloc_size;  //算法ram需求大小
    u8  type;
};

typedef struct {
    float alpha_db;
    float alpha_db_l;
    float db_cali;
    float sc_cali;
    float dac_cali;
    int   flen;
} __avc_config;

struct avc_function {
    void (*avc_config_init)(__avc_config *_avc_config);
    s16(*app_audio_get_volume)(u8 state);
};
extern struct avc_function *AVC_FUNC;

typedef struct {
    s16 *refmic;
    s16 *dac_data;
    u8  type;
} __icsd_avc_run_parm;

typedef struct {
    int ctl_lvl;
    float spldb_iir;
} __icsd_avc_output;


void icsd_avc_get_libfmt(struct icsd_avc_libfmt *libfmt);
void icsd_avc_set_infmt(struct icsd_avc_infmt *fmt);
void icsd_alg_avc_run(__icsd_avc_run_parm *run_parm, __icsd_avc_output *output);
void icsd_avc_ram_clean();
void icsd_avc_run(__icsd_avc_run_parm *_run_parm, __icsd_avc_output *_output);
void avc_config_update(__avc_config *_avc_config);
float icsd_adt_get_avc_spldb_iir();
void icsd_alg_avc_debug_free();

extern const float tidy_scale;
extern const float tidy_offset;


extern char lib_avc_version[];
#endif
