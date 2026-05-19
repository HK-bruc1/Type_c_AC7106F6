#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_cvp_elevoc.data.bss")
#pragma data_seg(".audio_cvp_elevoc.data")
#pragma const_seg(".audio_cvp_elevoc.text.const")
#pragma code_seg(".audio_cvp_elevoc.text")
#endif
/*
 ***************************************************************************
 *							Audio CVP Elevoc
 *
 * Brief : Elevoc algorithm integration 算法集成
 * Notes :
 ***************************************************************************
 */
#include "audio_cvp.h"
#include "aec_uart_debug.h"
#include "system/includes.h"
#include "media/includes.h"
#include "effects/eq_config.h"
#include "circular_buf.h"
#include "overlay_code.h"
#include "audio_config.h"
#include "debug.h"
#include "cvp_develop_node.h"
#include "sdk_config.h"

#if TCFG_AUDIO_CVP_SYNC
#include "audio_cvp_sync.h"
#endif/*TCFG_AUDIO_CVP_SYNC*/
#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif/*TCFG_AUDIO_DUT_ENABLE*/


#define ELEVOC
#ifdef ELEVOC
// #define ELEVOC_TX_MIPS_PROFILE
#include "elevoc/ele_vocplus_api.h"
#include "elevoc/elevoc_profile.h"
#include "timer.h"
#include "clock_manager/clock_manager.h"

#include "jiffies.h"

// typedef int (*print_func)(const char *, ...);
// extern int elevoc_print_register(print_func func);
#ifdef ELEVOC_TX_MIPS_PROFILE
ele_profile profiler;
#endif
#endif

#if defined(TCFG_CVP_DEVELOP_ENABLE) && (CVP_THIRD_ALGO_TYPE & CVP_ELEVOC_ALGO_BITMAP)

#define AEC_CLK				(160 * 1000000L)	/*模块运行时钟(MaxFre:160MHz)*/
#define AEC_FRAME_POINTS	256					/*AEC处理帧长，跟mic采样长度关联*/
#define AEC_FRAME_SIZE		(AEC_FRAME_POINTS << 1)

//#define ELEVOC_DEBUG_MIPS

/*数据输出开头丢掉的数据包数*/
#define CVP_OUT_DUMP_PACKET		15

//*********************************************************************************//
//                                预处理配置(Pre-process Config)               	   //
//*********************************************************************************//
/*预增益配置*/
#define CVP_PRE_GAIN_ENABLE				0	  //算法处理前预加数字增益放大使能
#define CVP_PRE_GAIN                    6.f   //算法前处理数字增益（Gain = 10^(dB_diff/20))

/*AEC输入buf复用mic_adc采样buf*/
#define MIC_BULK_MAX		3
struct mic_bulk {
    struct list_head entry;
    s16 *addr;
    u16 len;
    u16 used;
};

struct audio_cvp_dev {
    volatile u8 start;		//aec模块状态
    u8 mic_num;				//MIC的数量
    volatile u8 busy;
    volatile u8 ref_busy;
    volatile u8 ref_ok;		//模块状态
    u8 output_fade_in;		//aec输出淡入使能
    u8 output_fade_in_gain;	//aec输出淡入增益
    u8 output_sel;			//数据输出通道选择
    u16 dump_packet;		//前面如果有杂音，丢掉几包
    s16 *mic;				/*主mic数据地址*/
    s16 *mic_ref;			/*参考mic数据地址*/
    s16 *mic_ref_1;         /*参考mic数据地址*/
    s16 *mic_ref_2;         /*参考mic数据地址*/
    s16 *pFar;				/*参考数据地址*/
    s16 *free_ram;			/*当前可用内存*/
    /* s16 spk_ref[AEC_FRAME_POINTS];	#<{(|扬声器参考数据|)}># */
    s16 *spk_ref;	/*扬声器参考数据*/
    s16 out[AEC_FRAME_POINTS];		/*运算输出地址*/
    OS_SEM sem;
    /*数据复用相关数据结构*/
    struct mic_bulk in_bulk[MIC_BULK_MAX];
    struct mic_bulk inref_bulk[MIC_BULK_MAX];
    struct mic_bulk inref_1_bulk[MIC_BULK_MAX];
    struct mic_bulk inref_2_bulk[MIC_BULK_MAX];
    struct mic_bulk ref0_bulk[MIC_BULK_MAX];
    struct list_head in_head;
    struct list_head inref_head;
    struct list_head inref_1_head;
    struct list_head inref_2_head;
    struct list_head ref0_head;
    u8 output_way;
    u8 fm_tx_start;
    u8 ref_channel;
    u8 adc_ref_en;
    u16 ref_size;
    u32 ref_sr;
    u32 algo_type;
    int (*output_handle)(s16 *dat, u16 len);//输出回调函数
};
struct audio_cvp_dev *cvp_dev = NULL;

static spinlock_t cvp_lock;
#define AEC_REF_CBUF_SIZE         (AEC_FRAME_POINTS * 6)
#define AEC_REF_CBUF_DOOR_SIZE    (AEC_REF_CBUF_SIZE / 2)
static cbuffer_t *aec_ref_cbuf = NULL;

extern void aec_code_movable_load(void);
extern void aec_code_movable_unload(void);
extern u32 usb_mic_is_running();

void audio_aec_ref_start(u8 en)
{
    if (cvp_dev) {
        if (en != cvp_dev->fm_tx_start) {
            /* if (esco_adc_mic_en() == 0) { */
            cvp_dev->fm_tx_start = en;
            y_printf("fm_tx_start:%d\n", en);
            /* } */
        }
    }
}

/*
*********************************************************************
*                  Audio AEC Output Handle
* Description: AEC模块数据输出回调
* Arguments  : data 输出数据地址
*			   len	输出数据长度
* Return	 : 数据输出消耗长度
* Note(s)    : None.
*********************************************************************
*/
static int audio_aec_output(s16 *data, u16 len)
{
    u16 wlen = 0;
    if (cvp_dev && cvp_dev->start) {
#if TCFG_AUDIO_CVP_SYNC
        audio_cvp_sync_run(data, len);
        return len;
#endif/*TCFG_AUDIO_CVP_SYNC*/

        if (cvp_dev->dump_packet) {
            cvp_dev->dump_packet--;
            memset(data, 0, len);
        } else  {
            if (cvp_dev->output_fade_in) {
                s32 tmp_data;
                //printf("fade:%d\n",cvp_dev->output_fade_in_gain);
                for (int i = 0; i < len / 2; i++) {
                    tmp_data = data[i];
                    data[i] = tmp_data * cvp_dev->output_fade_in_gain >> 7;
                }
                cvp_dev->output_fade_in_gain += 12;
                if (cvp_dev->output_fade_in_gain >= 128) {
                    cvp_dev->output_fade_in = 0;
                }
            }
        }

        return cvp_dev_node_output_handle(data, len);
    }
    return wlen;
}

/*
 *跟踪系统内存使用情况:physics memory size xxxx bytes
 *正常的系统运行过程，应该至少有3k bytes的剩余空间给到系统调度开销
 */
static void sys_memory_trace(void)
{
    static int cnt = 0;
    if (cnt++ > 200) {
        cnt = 0;
        mem_stats();
    }
}

#include "Resample_api.h"
static RS_STUCT_API *sw_src_api = NULL;
static unsigned int *sw_src_buf = NULL;
#include "audio_src.h"
static struct audio_src_handle *ref_hw_src = NULL;;
static u16 ref_hw_src_len = 0;

static int ref_hw_src_output(void *p, s16 *data, u16 len)
{
    struct audio_cvp_dev *aec = (struct audio_cvp_dev *)p;
    if (aec) {
        memcpy(aec->pFar + (ref_hw_src_len >> 1), data, len);
        ref_hw_src_len += len;
    }
    return len;
}

__CVP_BANK_CODE
static int sw_src_init(u8 nch, u16 insample, u16 outsample)
{
    if (CONST_REF_SRC == 1) {
        if (insample != outsample) {
            sw_src_api = get_rs16_context();
            g_printf("sw_src_api:0x%x\n", sw_src_api);
            ASSERT(sw_src_api);
            u32 sw_src_need_buf = sw_src_api->need_buf();
            g_printf("sw_src_buf:%d\n", sw_src_need_buf);
            sw_src_buf = malloc(sw_src_need_buf);
            ASSERT(sw_src_buf);
            RS_PARA_STRUCT rs_para_obj;
            rs_para_obj.nch = nch;

            rs_para_obj.new_insample = insample;//48000;
            rs_para_obj.new_outsample = outsample;//16000;
            rs_para_obj.dataTypeobj.IndataBit = 0;
            rs_para_obj.dataTypeobj.OutdataBit = 0;
            rs_para_obj.dataTypeobj.IndataInc = (nch == 1) ? 1 : 2;
            rs_para_obj.dataTypeobj.OutdataInc = (nch == 1) ? 1 : 2;
            rs_para_obj.dataTypeobj.Qval = 15;

            printf("sw src,ch = %d, in = %d,out = %d\n", rs_para_obj.nch, rs_para_obj.new_insample, rs_para_obj.new_outsample);
            sw_src_api->open(sw_src_buf, &rs_para_obj);
        }
    } else if (CONST_REF_SRC == 2) {
        if (insample != outsample) {
            printf("hw src ch %d, in %d, out %d\n", nch, insample, outsample);
            ref_hw_src = zalloc(sizeof(struct audio_src_handle));
            if (ref_hw_src) {
                u8 channel = nch;
                audio_hw_src_open(ref_hw_src, channel, AUDIO_RESAMPLE_SYNC_OUTPUT);
                audio_hw_src_set_rate(ref_hw_src, insample, outsample);
                audio_src_set_output_handler(ref_hw_src, cvp_dev, (int (*)(void *, void *, int))ref_hw_src_output);
                printf("audio hw src open succ %x", (int)ref_hw_src);
            } else {
                printf("ref_hw_src malloc fail !!!\n");
            }
        }
    }

    return 0;
}

static int sw_src_run(s16 *indata, s16 *outdata, u16 len)
{
    int outlen = len;
    if (CONST_REF_SRC == 1) {
        if (sw_src_api && sw_src_buf) {
            outlen = sw_src_api->run(sw_src_buf, indata, len >> 1, outdata);
            /* ASSERT(outlen <= (sizeof(outdata) >> 1));  */
            outlen = outlen << 1;
            /* printf("%d\n",outlen); */
        }
    } else if (CONST_REF_SRC == 2) {
        if (ref_hw_src) {
            ref_hw_src_len = 0;;
            outlen = audio_src_resample_write(ref_hw_src, indata, len);
            outlen = ref_hw_src_len;
        }
    }
    return outlen;
}

static void sw_src_exit(void)
{
    if (CONST_REF_SRC == 1) {
        if (sw_src_buf) {
            free(sw_src_buf);
            sw_src_buf = NULL;
            sw_src_api = NULL;
        }
    } else if (CONST_REF_SRC == 2) {
        printf("[HW]ref_src_exit\n");
        if (ref_hw_src) {
            audio_hw_src_close(ref_hw_src);
            free(ref_hw_src);
            ref_hw_src = NULL;
        }
    }
}

/*
*********************************************************************
*                  Audio AEC RUN
* Description: AEC数据处理核心
* Arguments  : in 		Talk Mic采样数据
*			   inref	FF Mic采样数据
*			   inref1	FB Mic采样数据
*			   ref		speaker回采参考数据
*			   out		数据输出
*			   points   数据点数，单位short
* Return	 : 数据运算输出长度
* Note(s)    : 在这里实现AEC_core
*********************************************************************
*/
/**
 * in---->talk
 * inref---->ff
 * inref1---->fb
 * ref---->ref参考
 */
static s16 talk1_mic[256];	//ff
static s16 talk2_mic[256];	//talk
static s16 talk3_mic[256];	//fb
static s16 talk4_mic[256];	//vpu
static s16 ref_data[256];	//ref
static s16 *near_in_2[2];
static s16 *near_in_3[3];
static s16 *near_in_4[4];

static int audio_aec_run(s16 *in, s16 *inref, s16 *inref1, s16 *inref2, s16 *ref, s16 *out, u16 points)
{
    int out_size = 0;
    short **near_in = NULL;
#if CVP_PRE_GAIN_ENABLE
    GainProcess_16Bit(in, in, CVP_PRE_GAIN, 1, 1, 1, points);
    GainProcess_16Bit(inref, inref, CVP_PRE_GAIN, 1, 1, 1, points);
    //    GainProcess_16Bit_test(inref1, inref1, 0.f, 1, 1, 1, points);
#endif/*CVP_PRE_GAIN_ENABLE*/
    short *far_in[1]    = {ref_data};
    short *near_out[1]  = {out};
    for (int i = 0; i < points; i++) {
        talk2_mic[i] = in[i]; 		//TALK
        ref_data[i]  = ref[i];
        if (cvp_dev->mic_num == 2) {
            talk4_mic[i] = inref[i];	//VPU
        } else if (cvp_dev->mic_num == 3) {
            talk1_mic[i] = inref[i];	//FF
            talk4_mic[i] = inref1[i];	//VPU
        } else if (cvp_dev->mic_num == 4) {
            talk1_mic[i] = inref[i];	//FF
            talk3_mic[i] = inref1[i];	//FB
            talk4_mic[i] = inref2[i];	//VPU
        }
    }
    if (cvp_dev->algo_type == CVP_CFG_ELEVOC_1MIC_VPU) {
        /* short *near_in[2]   = {talk2_mic, talk4_mic}; */
        near_in_2[0] = talk2_mic;
        near_in_2[1] = talk4_mic;
        near_in = near_in_2;
    } else if ((cvp_dev->algo_type == CVP_CFG_ELEVOC_2MIC_VPU) || (cvp_dev->algo_type == CVP_CFG_ELEVOC_2MIC_VPU_CLIP)) {
        /* short *near_in[3]   = {talk1_mic, talk2_mic, talk4_mic}; */
        near_in_3[0] = talk1_mic;
        near_in_3[1] = talk2_mic;
        near_in_3[2] = talk4_mic;
        near_in = near_in_3;
    } else if (cvp_dev->algo_type == CVP_CFG_ELEVOC_3MIC_VPU) {
        /* short *near_in[4]   = {talk1_mic, talk3_mic, talk2_mic, talk4_mic}; // 算法顺序: FF FB TALK VPU */
        near_in_4[0] = talk1_mic;
        near_in_4[1] = talk3_mic;
        near_in_4[2] = talk2_mic;
        near_in_4[3] = talk4_mic;
        near_in = near_in_4;
    }
#ifdef ELEVOC
    // printf("audio_aec_run elevoc_vocplus_process\n");
#if defined(ELEVOC_TX_MIPS_PROFILE)
    elevoc_profile_tic(&profiler);
#endif

#ifdef ELEVOC_DEBUG_MIPS
    unsigned long us_start = jiffies_usec();
#endif
    elevoc_vocplus_process(near_in, far_in, near_out, points);

#ifdef ELEVOC_DEBUG_MIPS
    unsigned long us_end = jiffies_usec();

    static uint64_t total_cost_us = 0;
    static uint32_t max_us = 0;
    static uint32_t min_us = 0xffffffff;
    static int cnt = 0;
    uint64_t cost_us = us_end - us_start;
    total_cost_us += cost_us;
    if (max_us < cost_us) {
        max_us = cost_us;
    }
    if (min_us > cost_us) {
        min_us = cost_us;
    }

    if (++cnt % 67 * 2 == 0) {
        uint64_t aver_us = total_cost_us / cnt;
        printf("[elevoc]aver_us:%d %dus max_us:%d %dus min_us:%d %dus", \
               (uint32_t)aver_us, (uint32_t)aver_us, \
               (uint32_t)max_us, (uint32_t)max_us, \
               (uint32_t)min_us, (uint32_t)min_us);
        total_cost_us = 0;
        max_us = 0;
        min_us = 0xffffffff;
        cnt = 0;
    }
#endif

#if defined(ELEVOC_TX_MIPS_PROFILE)
    elevoc_profile_toc(&profiler);
#endif

#else
    memcpy(out, inref, (points << 1));
#endif

    out_size = points << 1;

#if TCFG_AUDIO_DUT_ENABLE
    switch (cvp_dev->output_sel) {
    case CVP_OUTPUT_SEL_TALK_MIC:
        memcpy(out, talk2_mic, (points << 1));     //TALK_MIC
        break;
    case CVP_OUTPUT_SEL_FF_MIC:
        if (cvp_dev->mic_num > 1) {
            memcpy(out, talk1_mic, (points << 1)); //FF_MIC
        }
        break;
    case CVP_OUTPUT_SEL_FB_MIC:
        if (cvp_dev->mic_num > 2) {
            memcpy(out, talk3_mic, (points << 1)); //FB_MIC
        }
        break;
    case CVP_OUTPUT_SEL_VPU:
        memcpy(out, talk4_mic, (points << 1));     //VPU
        break;
    default:
        break;
    }
#endif/*TCFG_AUDIO_DUT_ENABLE*/

    sys_memory_trace();
    return out_size;
}

/*
*********************************************************************
*                  Audio AEC Task
* Description: AEC任务
* Arguments  : priv	私用参数
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
static void audio_aec_task(void *priv)
{
    printf("==Audio AEC Task==\n");
    struct mic_bulk *bulk = NULL;
    struct mic_bulk *bulk_ref = NULL;
    struct mic_bulk *bulk_ref_1 = NULL;
    struct mic_bulk *bulk_ref_2 = NULL;
    struct mic_bulk *ref0_bulk = NULL;
    u8 pend = 1;
    while (1) {
        if (cvp_dev->output_way == 1) {
            if (!list_empty(&cvp_dev->in_head) && (aec_ref_cbuf->data_len >= cvp_dev->ref_size)) {
                cbuf_read(aec_ref_cbuf, cvp_dev->spk_ref, cvp_dev->ref_size);
            } else {
                os_sem_pend(&cvp_dev->sem, 0);
                continue;
            }
        } else {
            if (pend) {
                os_sem_pend(&cvp_dev->sem, 0);
            }
        }
        pend = 1;
        if (cvp_dev->start) {
            if ((!list_empty(&cvp_dev->in_head)) &&
                (!list_empty(&cvp_dev->ref0_head))) {
                cvp_dev->busy = 1;
                spin_lock(&cvp_lock);
                /*1.获取主mic数据*/
                bulk = list_first_entry(&cvp_dev->in_head, struct mic_bulk, entry);
                list_del(&bulk->entry);
                cvp_dev->mic = bulk->addr;
                if (cvp_dev->mic_num >= 2) {
                    /*获取ff mic数据*/
                    bulk_ref = list_first_entry(&cvp_dev->inref_head, struct mic_bulk, entry);
                    list_del(&bulk_ref->entry);
                    cvp_dev->mic_ref = bulk_ref->addr;
                }
                if (cvp_dev->mic_num >= 3) {
                    /*获取fb mic数据*/
                    bulk_ref_1 = list_first_entry(&cvp_dev->inref_1_head, struct mic_bulk, entry);
                    list_del(&bulk_ref_1->entry);
                    cvp_dev->mic_ref_1 = bulk_ref_1->addr;
                }
                if (cvp_dev->mic_num >= 4) {
                    /*获取vpu mic数据*/
                    bulk_ref_2 = list_first_entry(&cvp_dev->inref_2_head, struct mic_bulk, entry);
                    list_del(&bulk_ref_2->entry);
                    cvp_dev->mic_ref_2 = bulk_ref_2->addr;
                }
                /*获取dac数据*/
                cvp_dev->pFar = cvp_dev->spk_ref;
                if (cvp_dev->output_way == 0) {
                    ref0_bulk = list_first_entry(&cvp_dev->ref0_head, struct mic_bulk, entry);
                    list_del(&ref0_bulk->entry);
                    cvp_dev->pFar = (s16 *)ref0_bulk->addr;
                }
                spin_unlock(&cvp_lock);

                int rlen = cvp_dev->ref_size;
                /*dac参考数据是立体声数据时，合成一个声道*/
                if (cvp_dev->ref_channel == 2) {
                    for (int i = 0; i < rlen / 4; i++) {
                        cvp_dev->pFar[i] = (short)(((int)cvp_dev->pFar[i * 2] + (int)cvp_dev->pFar[i * 2 + 1]) >> 1);
                    }
                    rlen = rlen >> 1;
                }

                /*参考数据变采样*/
                if (CONST_REF_SRC) {
                    rlen = sw_src_run(cvp_dev->pFar, cvp_dev->pFar, rlen);
                }
                /*4.算法处理*/
                int out_len = audio_aec_run(cvp_dev->mic, cvp_dev->mic_ref, cvp_dev->mic_ref_1, cvp_dev->mic_ref_2, cvp_dev->pFar, cvp_dev->out, AEC_FRAME_POINTS);

                /*5.结果输出*/
                cvp_dev->output_handle(cvp_dev->out, out_len);

                /*6.数据导出*/
                if (CONST_AEC_EXPORT) {
                    aec_uart_fill(0, cvp_dev->mic, 512);                //主mic数据
                    if (cvp_dev->mic_num == 1) {
                        aec_uart_fill(1, cvp_dev->pFar, 512);           //扬声器数据
                        aec_uart_fill(2, cvp_dev->out, out_len);        //算法运算结果
                    } else if (cvp_dev->mic_num == 2) {
                        if (CONST_AEC_EXPORT == 3) {
                            aec_uart_fill(1, cvp_dev->pFar, 512);
                        } else {
                            aec_uart_fill(1, cvp_dev->mic_ref, 512);
                        }
                        if (CONST_AEC_EXPORT == 1) {
                            aec_uart_fill(2, cvp_dev->pFar, 512);
                        } else {
                            aec_uart_fill(2, cvp_dev->out, out_len);
                        }
                    } else if (cvp_dev->mic_num == 3) {
                        aec_uart_fill(1, cvp_dev->mic_ref, 512);    //ff mic数据
                        aec_uart_fill(2, cvp_dev->mic_ref_1, 512);  //fb mic数据
                        aec_uart_fill(3, cvp_dev->pFar, 512);       //扬声器数据
                        aec_uart_fill(4, cvp_dev->out, out_len);    //算法运算结果
                    } else {
                        aec_uart_fill(1, cvp_dev->mic_ref, 512);    //ff mic数据
                        aec_uart_fill(2, cvp_dev->mic_ref_1, 512);  //fb mic数据
                        aec_uart_fill(3, cvp_dev->mic_ref_2, 512);  //vpu mic数据
                        aec_uart_fill(4, cvp_dev->pFar, 512);       //扬声器数据
                        aec_uart_fill(5, cvp_dev->out, out_len);    //算法运算结
                    }
                    aec_uart_write();
                }

                bulk->used = 0;
                if (cvp_dev->mic_num >= 2) {
                    bulk_ref->used = 0;
                }
                if (cvp_dev->mic_num >= 3) {
                    bulk_ref_1->used = 0;
                }
                if (cvp_dev->mic_num >= 4) {
                    bulk_ref_2->used = 0;
                }
                if (cvp_dev->output_way == 0) {
                    ref0_bulk->used = 0;
                }
                cvp_dev->busy = 0;
                pend = 0;
            }
        }
    }
}

/*
*********************************************************************
*                  Audio AEC Open
* Description: 初始化AEC模块
* Arguments  : init_param 	sr 			采样率(8000/16000)
*              				ref_sr  	参考采样率
*			   enablebit	使能模块(AEC/NLP/AGC/ANS...)
*			   out_hdl		自定义回调函数，NULL则用默认的回调
* Return	 : 0 成功 其他 失败
* Note(s)    : 该接口是对audio_aec_init的扩展，支持自定义使能模块以及
*			   数据输出回调函数
*********************************************************************
*/
__CVP_BANK_CODE
int audio_aec_open(struct audio_aec_init_param_t *init_param, s16 enablebit, int (*out_hdl)(s16 *data, u16 len))
{
    printf("audio_aec_init,sr = %d\n", init_param->sample_rate);
    s16 sample_rate = init_param->sample_rate;
    u32 ref_sr = init_param->ref_sr;
    mem_stats();
    if (cvp_dev) {
        printf("audio aec is already open!\n");
        return -1;
    }
    overlay_load_code(OVERLAY_AEC);
    aec_code_movable_load();

    cvp_dev = zalloc(sizeof(struct audio_cvp_dev));
    printf("cvp_dev size:%ld\n", sizeof(struct audio_cvp_dev));
    /* clk_set("sys", AEC_CLK); */

    spin_lock_init(&cvp_lock);
    cvp_dev->mic_num = init_param->mic_num;
    cvp_dev->algo_type = init_param->algo_type;

    cvp_dev->dump_packet = CVP_OUT_DUMP_PACKET;
    cvp_dev->output_fade_in = 1;
    cvp_dev->output_fade_in_gain = 0;

#if TCFG_AUDIO_DUT_ENABLE
    cvp_dev->output_sel = CVP_OUTPUT_SEL_DEFAULT;
#endif/*TCFG_AUDIO_DUT_ENABLE*/

#if TCFG_AUDIO_CVP_SYNC
    audio_cvp_sync_open(init_param->sample_rate);
#endif/*TCFG_AUDIO_CVP_SYNC*/

    /*初始化dac read的资源*/
    audio_dac_read_init();

    cvp_dev->output_way = 0;
    cvp_dev->fm_tx_start = 0;
    cvp_dev->ref_channel = 1;
    cvp_dev->adc_ref_en = 0;

    if (ref_sr) {
        cvp_dev->ref_sr  = ref_sr;
    } else {
        cvp_dev->ref_sr  = usb_mic_is_running();
    }
    if (cvp_dev->ref_sr == 0) {
        if (TCFG_ESCO_DL_CVSD_SR_USE_16K && (sample_rate == 8000)) {
            cvp_dev->ref_sr = 16000;	//CVSD 下行为16K
        } else {
            cvp_dev->ref_sr = sample_rate;
        }
    }

    if (out_hdl) {
        cvp_dev->output_handle = out_hdl;
    } else {
        cvp_dev->output_handle = audio_aec_output;
    }

    /*不支持参考数据和输入数据不成倍数关系的运算*/
    if (cvp_dev->ref_sr % sample_rate) {
        printf("AEC error: ref_sr:%d,in_sr:%d\n", cvp_dev->ref_sr, sample_rate);
        return -ENOEXEC;
    }

    if (cvp_dev->ref_channel != 2) {
        cvp_dev->ref_channel = 1;
    }

    INIT_LIST_HEAD(&cvp_dev->in_head);
    INIT_LIST_HEAD(&cvp_dev->inref_head);
    INIT_LIST_HEAD(&cvp_dev->inref_1_head);
    INIT_LIST_HEAD(&cvp_dev->inref_2_head);

    if (cvp_dev->output_way == 0) {
        INIT_LIST_HEAD(&cvp_dev->ref0_head);
    }

    /*adc回采参考数据时，复用adc的buf*/
    cvp_dev->ref_size = AEC_FRAME_POINTS * sizeof(short) * (cvp_dev->ref_sr / sample_rate) * cvp_dev->ref_channel;
    printf("aec ref_size:%d\n", cvp_dev->ref_size);

    if (cvp_dev->adc_ref_en == 0) {
        if (cvp_dev->output_way == 0) {
            cvp_dev->spk_ref = zalloc(cvp_dev->ref_size * MIC_BULK_MAX);
        } else {
            cvp_dev->spk_ref = zalloc(cvp_dev->ref_size);
        }
    }
    if (cvp_dev->output_way == 1) {
        u16 aec_ref_buf_size = AEC_REF_CBUF_SIZE * (cvp_dev->ref_sr / sample_rate) * cvp_dev->ref_channel;
        u8 *aec_ref_buf = zalloc(aec_ref_buf_size + sizeof(cbuffer_t));
        aec_ref_cbuf = (cbuffer_t *)aec_ref_buf;
        printf("aec ref cbuf:%d\n", aec_ref_buf_size);
        if (aec_ref_cbuf) {
            cbuf_init(aec_ref_cbuf, aec_ref_buf + sizeof(cbuffer_t), aec_ref_buf_size);
            /* cbuf_write(aec_ref_cbuf, aec->output, 128); */
        } else {
            printf("aec cbuf zalloc failed\n");
        }
    }

    os_sem_create(&cvp_dev->sem, 0);
    task_create(audio_aec_task, NULL, "aec");

    if (CONST_AEC_EXPORT) {
        if (cvp_dev->mic_num == 4) {
            aec_uart_open(6, 512);
        } else if (cvp_dev->mic_num == 3) {
            aec_uart_open(5, 512);
        } else {
            aec_uart_open(3, 512);
        }
    }
    audio_dac_read_reset();
    if (CONST_REF_SRC) {
        sw_src_init(1, cvp_dev->ref_sr, sample_rate);
    }
    cvp_dev->start = 1;

    mem_stats();
#if	0
    cvp_dev->free_ram = malloc(1024 * 63);
    mem_stats();
#endif

    printf("audio_aec_open elevoc_vocplus_tm_initial enter\n");
#ifdef ELEVOC

    // elevoc_print_register(printf);
    elevoc_vocplus_tm_initial(16000);
#if defined(ELEVOC_TX_MIPS_PROFILE)
    user_timer2_init();
    elevoc_profile_init(&profiler, "vocplus_dm_ns", 67, 16, 1000000);
#endif
    // extern void elevoc_test_fft(void);
    // elevoc_test_fft();
#endif
#if TCFG_AUDIO_DUT_ENABLE
    audio_dut_cvp_control_check();    //产测命令控制检查
#endif
    printf("audio_aec_open succ\n");
    return 0;
}

/*
*********************************************************************
*                  Audio AEC Init
* Description: 初始化AEC模块
* Arguments  : init_param 初始化参数句柄
* Return	 : 0 成功 其他 失败
* Note(s)    : None.
*********************************************************************
*/
__CVP_BANK_CODE
int audio_aec_init(struct audio_aec_init_param_t *init_param)
{
    return audio_aec_open(init_param, -1, NULL);
}


/*
*********************************************************************
*                  Audio AEC Close
* Description: 关闭AEC模块
* Arguments  : None.
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
__CVP_BANK_CODE
void audio_aec_close(void)
{
    printf("audio_aec_close:%x", (u32)cvp_dev);
    if (cvp_dev) {
        cvp_dev->start = 0;

        while (cvp_dev->busy) {
            os_time_dly(2);
        }
        task_kill("aec");

#if TCFG_AUDIO_CVP_SYNC
        //在AEC关闭之后再关，否则还会跑cvp_sync_run,导致越界
        audio_cvp_sync_close();
#endif/*TCFG_AUDIO_CVP_SYNC*/

        /*释放dac read的资源*/
        audio_dac_read_exit();

        if (CONST_AEC_EXPORT) {
            aec_uart_close();
        }

        if (CONST_REF_SRC) {
            sw_src_exit();
        }
        if (aec_ref_cbuf) {
            free(aec_ref_cbuf);
            aec_ref_cbuf = NULL;
        }

        if (cvp_dev->spk_ref) {
            free(cvp_dev->spk_ref);
            cvp_dev->spk_ref = NULL;
        }

        local_irq_disable();
        if (cvp_dev->free_ram) {
            free(cvp_dev->free_ram);
        }

        free(cvp_dev);
        cvp_dev = NULL;
        local_irq_enable();
        aec_code_movable_unload();

#ifdef ELEVOC
        elevoc_vocplus_release();
#endif

        printf("audio_aec_close succ\n");
    }
}

/*
*********************************************************************
*                  Audio AEC Status
* Description: AEC模块当前状态
* Arguments  : None.
* Return	 : 0 关闭 其他 打开
* Note(s)    : None.
*********************************************************************
*/
u8 audio_aec_status(void)
{
    if (cvp_dev) {
        return cvp_dev->start;
    }
    return 0;
}

//读DAC数据
int cvp_develop_read_ref_data_base(void)
{
    u16 rlen = -1;
    u8 i;
    int err = 0;
    if (cvp_dev == NULL) {
        return 0;
    } else if (cvp_dev->start == 0) {
        return 0;
    }
    if ((cvp_dev->output_way) || (cvp_dev->adc_ref_en)) {
        return 0;
    }
    for (i = 0; i < MIC_BULK_MAX; i++) {
        if (cvp_dev->ref0_bulk[i].used == 0) {
            rlen = audio_dac_read(60,
                                  cvp_dev->spk_ref + ((cvp_dev->ref_size >> 1) * i),
                                  cvp_dev->ref_size / cvp_dev->ref_channel,
                                  cvp_dev->ref_channel);
            break;
        }
    }
    if (i < MIC_BULK_MAX) {
        //AEC_D("bulk:%d-%d\n",i,len);
        cvp_dev->ref0_bulk[i].addr = (cvp_dev->spk_ref + ((cvp_dev->ref_size >> 1) * i));
        cvp_dev->ref0_bulk[i].used = 0x55;
        cvp_dev->ref0_bulk[i].len = rlen;
        list_add_tail(&cvp_dev->ref0_bulk[i].entry, &cvp_dev->ref0_head);
    } else {
        printf(">>>far_in_full\n");
        /*align reset*/
        struct mic_bulk *bulk;
        list_for_each_entry(bulk, &cvp_dev->ref0_head, entry) {
            bulk->used = 0;
            __list_del_entry(&bulk->entry);
        }
        return -1;
    }
    return rlen;
}

int cvp_develop_read_ref_data(void)
{
    spin_lock(&cvp_lock);
    int ret = cvp_develop_read_ref_data_base();
    spin_unlock(&cvp_lock);
    return ret;
}
/*
*********************************************************************
*                  Audio spinlock
* Description: None
* Arguments  : None
*
* Return	 : None.
* Note(s)    : None
*********************************************************************
*/
void audio_cvp_develop_lock()
{
    spin_lock(&cvp_lock);
}

void audio_cvp_develop_unlock()
{
    spin_unlock(&cvp_lock);
}
/*
*********************************************************************
*                  Audio AEC Input
* Description: AEC源数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 输入一帧数据，唤醒一次运行任务处理数据，默认帧长256点
*********************************************************************
*/
void audio_aec_inbuf(s16 *buf, u16 len)
{
    if (cvp_dev && cvp_dev->start) {

        if (cvp_dev->output_way == 1) {
            if ((cvp_dev->ref_ok == 0) || (cvp_dev->fm_tx_start == 0)) {
                if (cvp_dev->ref_ok && !cvp_dev->fm_tx_start) {
                    printf("[aec]fm_tx_start == 0\n");
                }
                return;
            }
        }

        int i = 0;
        for (i = 0; i < MIC_BULK_MAX; i++) {
            if (cvp_dev->in_bulk[i].used == 0) {
                break;
            }
        }

        if (i < MIC_BULK_MAX) {
            cvp_dev->in_bulk[i].addr = buf;
            cvp_dev->in_bulk[i].used = 0x55;
            cvp_dev->in_bulk[i].len = len;
            list_add_tail(&cvp_dev->in_bulk[i].entry, &cvp_dev->in_head);
        } else {
            printf(">>>aec_in_full\n");
            /*align reset*/
            if (cvp_dev->output_way == 0) {
                audio_dac_read_reset();
            }
            struct mic_bulk *bulk;
            list_for_each_entry(bulk, &cvp_dev->in_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }

            if (cvp_dev->mic_num >= 2) {
                list_for_each_entry(bulk, &cvp_dev->inref_head, entry) {
                    bulk->used = 0;
                    __list_del_entry(&bulk->entry);
                }
            }

            if (cvp_dev->mic_num >= 3) {
                list_for_each_entry(bulk, &cvp_dev->inref_1_head, entry) {
                    bulk->used = 0;
                    __list_del_entry(&bulk->entry);
                }
            }

            if (cvp_dev->mic_num >= 4) {
                list_for_each_entry(bulk, &cvp_dev->inref_2_head, entry) {
                    bulk->used = 0;
                    __list_del_entry(&bulk->entry);
                }
            }

            if (cvp_dev->adc_ref_en == 1) {
                list_for_each_entry(bulk, &cvp_dev->ref0_head, entry) {
                    bulk->used = 0;
                    __list_del_entry(&bulk->entry);
                }

            }
            return;
        }
        os_sem_set(&cvp_dev->sem, 0);
        os_sem_post(&cvp_dev->sem);
    }
}

/*
*********************************************************************
*                  Audio AEC Input Reference
* Description: AEC源参考数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 双mic ENC的参考mic数据输入,单mic的无须调用该接口
*********************************************************************
*/
void audio_aec_inbuf_ref(s16 *buf, u16 len)
{
    if (cvp_dev && cvp_dev->start) {
        if (cvp_dev->output_way == 1) {
            if ((cvp_dev->ref_ok == 0) || (cvp_dev->fm_tx_start == 0)) {
                if (cvp_dev->ref_ok && !cvp_dev->fm_tx_start) {
                    printf("[aec]fm_tx_start == 0\n");
                }
                return;
            }
        }

        int i = 0;
        for (i = 0; i < MIC_BULK_MAX; i++) {
            if (cvp_dev->inref_bulk[i].used == 0) {
                break;
            }
        }
        if (i < MIC_BULK_MAX) {
            cvp_dev->inref_bulk[i].addr = buf;
            cvp_dev->inref_bulk[i].used = 0x55;
            cvp_dev->inref_bulk[i].len = len;
            list_add_tail(&cvp_dev->inref_bulk[i].entry, &cvp_dev->inref_head);
        } else {
            printf(">>>aec_inref_full\n");
            /*align reset*/
            if (cvp_dev->output_way == 0) {
                audio_dac_read_reset();
            }
            struct mic_bulk *bulk;
            list_for_each_entry(bulk, &cvp_dev->inref_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }
            return;
        }
    }
}

/*
*********************************************************************
*                  Audio AEC Input Reference
* Description: AEC源参考数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 双mic ENC的参考mic数据输入,单mic的无须调用该接口
*********************************************************************
*/
void audio_aec_inbuf_ref_1(s16 *buf, u16 len)
{
    if (cvp_dev && cvp_dev->start) {
        if (cvp_dev->output_way == 1) {
            if ((cvp_dev->ref_ok == 0) || (cvp_dev->fm_tx_start == 0)) {
                if (cvp_dev->ref_ok && !cvp_dev->fm_tx_start) {
                    printf("[aec]fm_tx_start == 0\n");
                }
                return;
            }
        }

        int i = 0;
        for (i = 0; i < MIC_BULK_MAX; i++) {
            if (cvp_dev->inref_1_bulk[i].used == 0) {
                break;
            }
        }
        if (i < MIC_BULK_MAX) {
            cvp_dev->inref_1_bulk[i].addr = buf;
            cvp_dev->inref_1_bulk[i].used = 0x55;
            cvp_dev->inref_1_bulk[i].len = len;
            list_add_tail(&cvp_dev->inref_1_bulk[i].entry, &cvp_dev->inref_1_head);
        } else {
            printf(">>>aec_inref_1_full\n");
            /*align reset*/
            struct mic_bulk *bulk;
            list_for_each_entry(bulk, &cvp_dev->inref_1_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }
            return;
        }
    }
}

/*
*********************************************************************
*                  Audio AEC Input Reference
* Description: AEC源参考数据输入
* Arguments  : buf	输入源数据地址
*			   len	输入源数据长度
* Return	 : None.
* Note(s)    : 双mic ENC的参考mic数据输入,单mic的无须调用该接口
*********************************************************************
*/
void audio_aec_inbuf_ref_2(s16 *buf, u16 len)
{
    if (cvp_dev && cvp_dev->start) {
        if (cvp_dev->output_way == 1) {
            if ((cvp_dev->ref_ok == 0) || (cvp_dev->fm_tx_start == 0)) {
                if (cvp_dev->ref_ok && !cvp_dev->fm_tx_start) {
                    printf("[aec]fm_tx_start == 0\n");
                }
                return;
            }
        }

        int i = 0;
        for (i = 0; i < MIC_BULK_MAX; i++) {
            if (cvp_dev->inref_2_bulk[i].used == 0) {
                break;
            }
        }
        if (i < MIC_BULK_MAX) {
            cvp_dev->inref_2_bulk[i].addr = buf;
            cvp_dev->inref_2_bulk[i].used = 0x55;
            cvp_dev->inref_2_bulk[i].len = len;
            list_add_tail(&cvp_dev->inref_2_bulk[i].entry, &cvp_dev->inref_2_head);
        } else {
            printf(">>>aec_inref_2_full\n");
            /*align reset*/
            struct mic_bulk *bulk;
            list_for_each_entry(bulk, &cvp_dev->inref_2_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }
            return;
        }
    }
}

/*
*********************************************************************
*                  Audio AEC Reference
* Description: AEC模块参考数据输入
* Arguments  : buf	输入参考数据地址
*			   len	输入参考数据长度
* Return	 : None.
* Note(s)    : 声卡设备是DAC，默认不用外部提供参考数据
*********************************************************************
*/
void audio_aec_far_refbuf(s16 *buf, u16 len);
void audio_aec_refbuf(s16 *data0, s16 *data1, u16 len)
{
    if (cvp_dev && cvp_dev->start) {
        if ((cvp_dev != NULL) && (cvp_dev->adc_ref_en == 1) && (cvp_dev->output_way == 0)) {
            /*使用adc回采时*/
            if (data0) {
                audio_aec_far_refbuf(data0, len);
            }
            return;
        }

        /*使用外部参考数据时*/
        if (cvp_dev->output_way != 1) {
            return;
        }
        cvp_dev->ref_busy = 1;

        cvp_dev->ref_ok = 1;
        if (0 == cbuf_write(aec_ref_cbuf, data0, len)) {
            printf("aec wfail:%d\n", len);
        }
#if 1
        static u16 aec_ref_max = 0;
        if (aec_ref_max < aec_ref_cbuf->data_len) {
            aec_ref_max = aec_ref_cbuf->data_len;
            printf("aec_ref_max:%d\n", aec_ref_max);
        }
#endif

        if (!list_empty(&cvp_dev->in_head) && (aec_ref_cbuf->data_len >= cvp_dev->ref_size)) {
            os_sem_set(&cvp_dev->sem, 0);
            os_sem_post(&cvp_dev->sem);
        }
        cvp_dev->ref_busy = 0;
    }
}

/*adc ref0*/
void audio_aec_far_refbuf(s16 *buf, u16 len)
{
    u16 wlen;
    u8 i;
    int err = 0;

    if ((cvp_dev == NULL) || (cvp_dev->adc_ref_en == 0) || (cvp_dev->output_way == 1)) {
        return;
    }
#if 0
    /*AEC暂时挂起，有其他功能要做，比如播题示音，这个时候不用做AEC*/
    if ((aec->state & AEC_STATE_SUSPEND) || (aec->state & AEC_STATE_RESET)) {
        return err;
    }
#endif
    //AEC_D("A:%d\n", len);
    for (i = 0; i < MIC_BULK_MAX; i++) {
        if (cvp_dev->ref0_bulk[i].used == 0) {
            break;
        }
    }
    if (i < MIC_BULK_MAX) {
        //AEC_D("bulk:%d-%d\n",i,len);
        cvp_dev->ref0_bulk[i].addr = buf;
        cvp_dev->ref0_bulk[i].used = 0x55;
        cvp_dev->ref0_bulk[i].len = len;
        list_add_tail(&cvp_dev->ref0_bulk[i].entry, &cvp_dev->ref0_head);
    } else {
        printf(">>>aec_in_ref0_full\n");
        /*align reset*/
        struct mic_bulk *bulk;
        list_for_each_entry(bulk, &cvp_dev->ref0_head, entry) {
            bulk->used = 0;
            __list_del_entry(&bulk->entry);
        }
        return;
    }
}


/*
*********************************************************************
*                  Audio AEC Output Sel
* Description: AEC输出数据选择
* Arguments  : sel 选择输出/算法输出/talk/ff/fb原始数据
*			   agc NULL
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
void audio_aec_output_sel(CVP_OUTPUT_ENUM sel, u8 agc)
{
    if (cvp_dev) {
        cvp_dev->output_sel = sel;
    }
}

/*
*********************************************************************
*                  Audio CVP Toggle Set
* Description: CVP模块算法开关使能
* Arguments  : toggle 0 关闭算法 1 打开算法
* Return	 : None.
* Note(s)    : None.
*********************************************************************
*/
int audio_cvp_toggle_set(u8 toggle)
{
    if (cvp_dev) {
        cvp_dev->output_sel = (toggle) ? 0 : 1;
        return 0;
    }
    return 1;
}

/*
*********************************************************************
*                  			Audio CVP IOCTL
* Description: CVP功能配置
* Arguments  : cmd 		操作命令
*		       value 	操作数
*		       priv 	操作内存地址
* Return	 : 0 成功 其他 失败
* Note(s)    : (1)比如动态开关降噪NS模块:
*********************************************************************
*/
int audio_cvp_ioctl(int cmd, int value, void *priv)
{
    return -1;
}

#endif /*TCFG_CVP_DEVELOP_ENABLE*/
