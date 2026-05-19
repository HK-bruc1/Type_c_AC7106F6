#ifndef _ICSD_VDT_H
#define _ICSD_VDT_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "timer.h"
#include "math.h"
#include "icsd_common.h"
#include "icsd_common_v2.h"

#if VDT_PRINTF_EN
#define _vdt_printf printf
#else
#define _vdt_printf icsd_printf_off
#endif
extern int (*vdt_printf)(const char *format, ...);


#define VDT_USE_ANCDMAL_DATA  0

#define	VDT_EXT_NOISE_PWR               BIT(0)
#define	VDT_EXT_DIT              		BIT(1)
#define	VDT_EXT_COR              		BIT(2)
#define VDT_EXT_FCXY                    BIT(3)

struct icsd_vdt_libfmt {
    int lib_alloc_size;  //算法ram需求大小
};

struct icsd_vdt_infmt {
    void *alloc_ptr;     //外部申请的ram地址
    int lib_alloc_size;  //算法ram需求大小
};

typedef struct {
    u8 pnc_ref_pwr_thr;
    u8 tpc_ref_pwr_thr;
    u8 anc_ref_pwr_thr;

    u8 pnc_err_pwr_thr;
    u8 tpc_err_pwr_thr;
    u8 anc_err_pwr_thr;
} __vdt_config;

struct vdt_function {
    void (*vdt_config_init)(__vdt_config *_vdt_config);
};
extern struct vdt_function *VDT_FUNC;

typedef struct {
    s16 *refmic;
    s16 *errmic;
    s16 *tlkmic;
    s16 *dmah;
    s16 *dmal;
    s16 *dac_data;
    float fbgain;
    float tfbgain;
    u8 mic_num;
    u8 anc_mode_ind;
} __icsd_vdt_run_parm;

typedef struct {
    u8 voice_state;
} __icsd_vdt_output;

void icsd_vdt_get_libfmt(struct icsd_vdt_libfmt *libfmt);
void icsd_vdt_set_infmt(struct icsd_vdt_infmt *fmt);
void icsd_alg_vdt_run(__icsd_vdt_run_parm *run_parm, __icsd_vdt_output *output);
void icsd_vdt_data_init(u8 _anc_mode_ind, float refl_mgain, float errl_mgain, float refr_mgain, float errr_mgain, float tlk_mgain, u8 mic_ver);


extern const u8 VDT_TRAIN_EN;
extern const u8 ICSD_VDT_ALG_BT_INF;
extern const u8 ICSD_VDT_BT_DATA;
extern const float vdt_refgain;
extern const float vdt_errgain;
extern const float vdt_tlkgain;
extern const float vdt_dmahgain;


void vdt_config_init(__vdt_config *_vdt_config);

extern char lib_vdt_version[];
#endif
