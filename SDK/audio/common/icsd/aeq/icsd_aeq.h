#ifndef _ICSD_AEQ_H
#define _ICSD_AEQ_H

#include "generic/typedef.h"
#include "os/os_type.h"
#include "os/os_api.h"
#include "asm/math_fast_function.h"
#include "icsd_common_v2.h"
#include "icsd_aeq_config.h"

#if AEQ_PRINTF_EN
#define _aeq_printf printf                  //打开自适应ANC参数调试信息
#else
#define _aeq_printf icsd_printf_off
#endif
extern int (*aeq_printf)(const char *format, ...);


#define AEQ_FLEN 120// (TARLEN2+TARLEN2_L-DRPPNT2)

typedef struct {
    u8 state;
    float *h_freq;
    float *target;
} _aeq_output;

#define AEG_SEG_NUM         5

//
struct icsd_aeq_infmt {
    void *alloc_ptr;                     //外部申请的ram地址
    u8 seg_num;                          //SEG数量
    // struct eq_seg_info seg[AEG_SEG_NUM]; //AEQ结果
};

struct icsd_aeq_libfmt {
    int lib_alloc_size;  //算法ram需求大小
};

void icsd_aeq_get_libfmt(struct icsd_aeq_libfmt *libfmt);
void icsd_aeq_set_infmt(struct icsd_aeq_infmt *fmt);
void icsd_aeq_force_exit();
_aeq_output *icsd_aeq_run(float *sz_goal, float *sz_in, float *EQ_data, float *delta_data, float maxgain_dB, float *fgq_best, __adpt_aeq_cfg *aeq_cfg);
//
void icsd_aeq_demo(float *_sz_in);
//

void aeq_get_par(struct aeq_default_seg_tab *aeq_def, void *eq_par);
//
struct eq_function {
    void (*aeq_seg_design)(void *seg_tmp, int sample_rate, void *coef);

};

extern struct eq_function *EQ_FUNC;



extern char lib_aeq_version[];
#endif
