#ifndef _ICSD_AFQ_H
#define _ICSD_AFQ_H

#include "timer.h"
#include "math.h"
#include "asm/math_fast_function.h"
#include "icsd_common_v2.h"

#define ICSD_AFQ_DSF8_DATA_DEBUG    0
#define AFQ_DSF8_DATA_DEBUG_TYPE	5//AFQ_DMA_BELONG_TO_TONEL


#define AFQ_DMA_DOUBLE_LEN      (512*4) //DMA一次拿多少数据
#define AFQ_DMA_DOUBLE_CNT       32
#define AFQ_DOUBLE_TRAIN_LEN     512    //拿够多少数进行一次part1
//contral
#define AFQ_FORCED_EXIT        	 BIT(0)
#define AFQ_SUSPENDED            BIT(1)
#define AFQ_PART1_END            BIT(2)

#if AFQ_PRINTF_EN
#define _afq_printf printf                  //打开自适应ANC参数调试信息
#else
#define _afq_printf icsd_printf_off
#endif
extern int (*afq_printf)(const char *format, ...);


struct icsd_afq_libfmt {
    int lib_alloc_size; //算法ram需求大小
};

struct icsd_afq_infmt {
    void *alloc_ptr;    //外部申请的ram地址
    u16   tone_delay;
};

enum {
    AFQ_OUTPUT_NORMAL = 0,
    AFQ_OUTPUT_TIMEOUT,
    AFQ_OUTPUT_FORCED,
};

struct afq_function {
    //sys
    void (*local_irq_disable)();
    void (*local_irq_enable)();
    void (*os_time_dly)(int tick);
    u16(*icsd_afq_timeout_add)(void *priv, void (*func)(void *priv), u32 msec);
    void (*icsd_afq_timeout_del)(u32 timeout_handler);
    int (*jiffies_usec2offset)(unsigned long begin, unsigned long end);
    unsigned long (*jiffies_usec)(void);
    void (*icsd_afq_cmd)(u8 cmd);
    //anc
    u8(*anc_dma_done_ppflag)();
    void (*icsd_afq_dma_2ch_on)(u8 out_sel, int *buf, int len);
    void (*icsd_afq_dma_4ch_on)(u8 out_sel_ch01, u8 out_sel_ch23, int *buf, int len);
    void (*icsd_afq_dma_stop)();
    u8(*icsd_afq_set_pnc)(u8 ff_yorder, float bypass_vol, s8 bypass_sign);
};
extern struct afq_function *AFQ_FUNC;

typedef struct {
    u8  contral;
    u32 sample_rate;
    u32 slience_frames;
    int icsd_afq_id;
    void *ram_addr;
    void (*icsd_afq_output)(__afq_output *_AFQ_OUT);
} __icsd_afq_reg;
extern __icsd_afq_reg *AFQ_REG;

extern const u8 afq_dma_ch_num;
extern const u8 AFQ_EP_TYPE;

void icsd_afq_get_libfmt(struct icsd_afq_libfmt *libfmt);
void icsd_afq_set_infmt(struct icsd_afq_infmt *fmt);
void icsd_afq_dma_handler();
void icsd_afq_run(u32 slience_frames, u32 sample_rate, void (*output_cb)(__afq_output *_AFQ_OUT));
void icsd_afq_forced_exit();
void icsd_afq_anctask_handler(void *param, int *msg);

void icsd_afq_dsf8_data_debug(u8 belong, s16 *dsf_out_ch0, s16 *dsf_out_ch1, s16 *dsf_out_ch2, s16 *dsf_out_ch3);

extern char lib_afq_version[];
#endif
