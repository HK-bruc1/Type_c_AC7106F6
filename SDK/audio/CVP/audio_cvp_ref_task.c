#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_cvp_ref_task.data.bss")
#pragma data_seg(".audio_cvp_ref_task.data")
#pragma const_seg(".audio_cvp_ref_task.text.const")
#pragma code_seg(".audio_cvp_ref_task.text")
#endif
/*
 ****************************************************************
 *							AUDIO CVP REF SRC
 * File  : audio_cvp_ref_src.c
 * By    :
 * Notes : CVP回音消除的外部参考数变采样
 *
 ****************************************************************
 */
#include "audio_cvp.h"
#include "system/includes.h"
#include "media/includes.h"
#include "circular_buf.h"
#include "Resample_api.h"
#include "media/audio_general.h"
#include "effects/convert_data.h"
#include "jlstream.h"
#include "esco_player.h"
#include "pc_spk_player.h"
#include "audio_link.h"
#include "media/audio_iis.h"

#define CVP_REF_SRC_TASK_NAME "CVP_RefTask"

#define ALINK_CH_IDX    0   //iis输出使用的通道

#define CVP_REF_SRC_FRAME_SIZE  (480 * 2)//512
typedef struct {
    volatile u8 state;
    volatile u8 busy;
    RS_STUCT_API *sw_src_api;
    u32 *sw_src_buf;
    u16 input_rate;
    u16 output_rate;
    u8 channel;
    s16 *ref_tmp_buf;
    cbuffer_t cbuf;
    u8 *ref_buf;	//根据参考数据的格式动态申请
    u8 align_flag;
    u8 bit_width;
    u8 data_multiple;//输入输出数据倍数
} cvp_ref_src_t;
static cvp_ref_src_t *cvp_ref_src = NULL;
DEFINE_SPINLOCK(cvp_ref_src_lock);
extern void iis_node_write_callback_add(const char *name, u8 is_after_write_over, u8 scene, void (*cb)(void *, int));
extern void iis_node_write_callback_del(const char *name);
extern void muliti_ch_iis_node_write_callback_add(const char *name, u8 is_after_write_over, u8 scene, void (*cb)(void *, void *, int));
extern void muliti_ch_iis_node_write_callback_del(const char *name);

#define IIS_READ_MAGIC  0x55AA
static int iis_read_pos = IIS_READ_MAGIC;

static const u32 *ALNK0_HWPTR[ALINK_CH_NUM_MAX] = {
    (u32 *)(&(JL_ALNK0->HWPTR0)),
#if (ALINK_CH_NUM_MAX > 1)
    (u32 *)(&(JL_ALNK0->HWPTR1)),
#endif
#if (ALINK_CH_NUM_MAX > 2)
    (u32 *)(&(JL_ALNK0->HWPTR2)),
#endif
#if (ALINK_CH_NUM_MAX > 3)
    (u32 *)(&(JL_ALNK0->HWPTR3)),
#endif
};

static const u32 *ALNK0_SHN[ALINK_CH_NUM_MAX] = {
    (u32 *)(&(JL_ALNK0->SHN0)),
#if (ALINK_CH_NUM_MAX > 1)
    (u32 *)(&(JL_ALNK0->SHN1)),
#endif
#if (ALINK_CH_NUM_MAX > 2)
    (u32 *)(&(JL_ALNK0->SHN2)),
#endif
#if (ALINK_CH_NUM_MAX > 3)
    (u32 *)(&(JL_ALNK0->SHN3)),
#endif
};

#if (ALINK_MODULE_NUM_MAX > 1)
static const u32 *ALNK1_HWPTR[ALINK_CH_NUM_MAX] = {
    (u32 *)(&(JL_ALNK1->HWPTR0)),
#if (ALINK_CH_NUM_MAX > 1)
    (u32 *)(&(JL_ALNK1->HWPTR1)),
#endif
#if (ALINK_CH_NUM_MAX > 2)
    (u32 *)(&(JL_ALNK1->HWPTR2)),
#endif
#if (ALINK_CH_NUM_MAX > 3)
    (u32 *)(&(JL_ALNK1->HWPTR3)),
#endif
};
static const u32 *ALNK1_SHN[ALINK_CH_NUM_MAX] = {
    (u32 *)(&(JL_ALNK1->SHN0)),
#if (ALINK_CH_NUM_MAX > 1)
    (u32 *)(&(JL_ALNK1->SHN1)),
#endif
#if (ALINK_CH_NUM_MAX > 2)
    (u32 *)(&(JL_ALNK1->SHN2)),
#endif
#if (ALINK_CH_NUM_MAX > 3)
    (u32 *)(&(JL_ALNK1->SHN3)),
#endif
};
#endif

static const u32 **ALNK_HWPTR[ALINK_MODULE_NUM_MAX] = {
    ALNK0_HWPTR,
#if (ALINK_MODULE_NUM_MAX > 1)
    ALNK1_HWPTR,
#endif
};

static const u32 **ALNK_SHN[ALINK_MODULE_NUM_MAX] = {
    ALNK0_SHN,
#if (ALINK_MODULE_NUM_MAX > 1)
    ALNK1_SHN,
#endif
};

static const u32 *ALNK_LEN[ALINK_MODULE_NUM_MAX] = {
    (u32 *)(&(JL_ALNK0->LEN)),
#if (ALINK_MODULE_NUM_MAX > 1)
    (u32 *)(&(JL_ALNK1->LEN)),
#endif
};

static u8 alink_multi_en = 0;
static u8 alink_module_idx = 0;
static u8 alink_ch_idx = 0;
/*设置是否多通道iis*/
void audio_cvp_ref_alink_multi_set(u8 en)
{
    alink_multi_en = en;
    printf("cvp_ref alink_multi_en : %d\n", alink_multi_en);
}
u8 audio_cvp_ref_alink_multi_get()
{
    return alink_multi_en;
}

/*设置是用哪个iis 模块*/
void audio_cvp_ref_alink_module_idx_set(u8 idx)
{
    alink_module_idx = idx;
    printf("cvp_ref alink_module_idx : %d\n", alink_module_idx);
    ASSERT((alink_module_idx < ALINK_MODULE_NUM_MAX) && (alink_ch_idx < ALINK_CH_NUM_MAX), "alink_module_idx %d, alink_ch_idx %d\n", alink_module_idx, alink_ch_idx);
}
u8 audio_cvp_ref_alink_module_idx_get()
{
    return alink_module_idx;
}

/*设置是用iis的哪个ch*/
void audio_cvp_ref_alink_ch_idx_set(u8 idx)
{
    alink_ch_idx = idx;
    printf("cvp_ref alink_ch_idx : %d\n", alink_ch_idx);
    ASSERT((alink_module_idx < ALINK_MODULE_NUM_MAX) && (alink_ch_idx < ALINK_CH_NUM_MAX), "alink_module_idx %d, alink_ch_idx %d\n", alink_module_idx, alink_ch_idx);
}
u8 audio_cvp_ref_alink_ch_idx_get()
{
    return alink_ch_idx;
}
struct node_port {
    u16 uuid;
    u8  number;
    u8  port;
};
struct node_link {
    struct node_port out;
    struct node_port in;
};

/*获取是用iis的哪个模块，哪个ch*/
int audio_cvp_ref_alink_cfg_get(u16 pipeline)
{
    u8 *pipeline_data;
    struct node_link *link;
    struct audio_iis_channel_attr attr;
    u8 ret = 0;
    int i = 0;
    int len = jlstream_read_pipeline_data(pipeline, &pipeline_data);
    if (len == 0) {
        printf("audio_cvp_ref read_pipeline error !!! \n");
        return -1;
    }
    for (i = 0; i < len; i += 8) {
        link = (struct node_link *)(pipeline_data + i);

        struct node_param ncfg = {0};
        int node_len = jlstream_read_node_data(link->out.uuid, link->out.number, (u8 *)&ncfg);
        if (link->in.uuid == NODE_UUID_MULTI_CH_IIS0_TX) {
            audio_cvp_ref_alink_multi_set(1);
            audio_cvp_ref_alink_module_idx_set(0);
#if (ALINK_MODULE_NUM_MAX > 1)
        } else if (link->in.uuid == NODE_UUID_MULTI_CH_IIS1_TX) {
            audio_cvp_ref_alink_multi_set(1);
            audio_cvp_ref_alink_module_idx_set(1);
#endif
        } else if (link->in.uuid == NODE_UUID_IIS0_TX) {
            audio_cvp_ref_alink_multi_set(0);
            audio_cvp_ref_alink_module_idx_set(0);
#if (ALINK_MODULE_NUM_MAX > 1)
        } else if (link->in.uuid == NODE_UUID_IIS1_TX) {
            audio_cvp_ref_alink_multi_set(0);
            audio_cvp_ref_alink_module_idx_set(1);
#endif
        } else {
            continue;
        }
        printf("read cfg, uuid %x, subid %x\n", link->in.uuid, link->in.number);
        ret = jlstream_read_node_data_by_cfg_index(link->in.uuid, link->in.number, 0, &attr, NULL);
        if (ret == sizeof(attr)) {
            printf("attr.ch_idx %x\n", attr.ch_idx);
            if (audio_cvp_ref_alink_multi_get()) {
                /*是用iis多通道输出的时候*/
                for (i = 0; i < ALINK_CH_NUM_MAX; i++) {
                    if (attr.ch_idx & BIT(i)) {
                        audio_cvp_ref_alink_ch_idx_set(i);
                        break;
                    }
                }
            } else {
                audio_cvp_ref_alink_ch_idx_set(attr.ch_idx);
            }
        } else {
            printf("read node read error, cfg %d, ret %d\n", (int)sizeof(attr), ret);
        }
        break;
    }
    free(pipeline_data);
    return 0;
}

/*获取iis_hdl*/
void *audio_cvp_ref_iis_hdl_get()
{
    ASSERT((alink_module_idx < ALINK_MODULE_NUM_MAX) && (alink_ch_idx < ALINK_CH_NUM_MAX), "alink_module_idx %d, alink_ch_idx %d\n", alink_module_idx, alink_ch_idx);
    return iis_hdl[alink_module_idx];
}


/*获取iis dma里面还有多少数据没有播放*/
static u32 get_alink_data_len()
{
    ASSERT((alink_module_idx < ALINK_MODULE_NUM_MAX) && (alink_ch_idx < ALINK_CH_NUM_MAX), "alink_module_idx %d, alink_ch_idx %d\n", alink_module_idx, alink_ch_idx);
    cvp_ref_src_t *hdl = cvp_ref_src;
    /* point * ch * bit_width*/
    u32 len = audio_iis_get_buffered_frames(audio_cvp_ref_iis_hdl_get(), audio_cvp_ref_alink_ch_idx_get()) * hdl->channel * (hdl->bit_width ? 4 : 2);
    // r_printf("get_alink_data_len : %d\n", len);
    return len;
}

/*iis硬件写指针位置*/
static u32 get_alink_hwptr()
{
    ASSERT((alink_module_idx < ALINK_MODULE_NUM_MAX) && (alink_ch_idx < ALINK_CH_NUM_MAX), "alink_module_idx %d, alink_ch_idx %d\n", alink_module_idx, alink_ch_idx);
    u32 val = *ALNK_HWPTR[alink_module_idx][alink_ch_idx];
    // r_printf("get_alink_hwptr : %d\n", val);
    return val;
}

/*重新对齐iis延时*/
int audio_cvp_ref_data_align_reset(void)
{
    iis_read_pos = IIS_READ_MAGIC;
    cvp_ref_src_t *hdl = cvp_ref_src;
    if (hdl && hdl->state) {
        hdl->busy = 1;
        hdl->align_flag = 0;
        cbuf_clear(&hdl->cbuf);
        hdl->busy = 0;
    }
    return 0;
}

/*对齐iis的数据延时，在第一次mic数据来时调用对齐*/
void audio_cvp_ref_data_align()
{
    cvp_ref_src_t *hdl = cvp_ref_src;
    /* !iis_read_pos ： iis有数据了才么开始对齐
       !hdl->align_flag ： 表示还没有做对齐*/
    spin_lock(&cvp_ref_src_lock);
    if (hdl && hdl->state && !hdl->align_flag && !iis_read_pos) {
        hdl->busy = 1;
        int iis_data_len = get_alink_data_len();
        int cbuf_total_len = cbuf_get_data_len(&hdl->cbuf);
        printf("hdl->cbuf len %d ", cbuf_total_len);
        int need_read_len = cbuf_total_len - iis_data_len;
        printf("adc_iis_data_align: %d %d", iis_data_len, need_read_len);
        if (need_read_len >= 0) {
            cbuf_read_updata(&hdl->cbuf, need_read_len);
        } else {
            cbuf_write_updata(&hdl->cbuf, need_read_len * (-1));
        }
        hdl->align_flag = 1;
        //有参考数据进来，并且对齐后，取消忽略参考数据
        audio_cvp_ioctl(CVP_OUTWAY_REF_IGNORE, 0, NULL);
        hdl->busy = 0;
    }
    spin_unlock(&cvp_ref_src_lock);
}


static void audio_cvp_ref_src_run(s16 *data, int len)
{
    cvp_ref_src_t *hdl = cvp_ref_src;
    u16 ref_len = len;
    if (hdl) {
        if (hdl->bit_width) {
            audio_convert_data_32bit_to_16bit_round((s32 *)data, (s16 *)data, ref_len >> 2);
            ref_len >>= 1;
        }

        if (hdl->channel == 2) {
            /* putchar('2'); */
            /*双变单*/
            for (int i = 0; i < (ref_len >> 2); i++) {
                data[i] = ((int)data[2 * i] + (int)data[2 * i + 1]) / 2;
            }
            ref_len >>= 1;
        }
        if (hdl->sw_src_api) {
            /* putchar('s'); */
            ref_len = hdl->sw_src_api->run(hdl->sw_src_buf, data, ref_len >> 1, data);
            ref_len <<= 1;
        }
        /* printf("ref_len %d : %d", ref_len, len); */
        audio_aec_refbuf(data, NULL, ref_len);
    }
}

static void audio_cvp_ref_src_task(void *p)
{
    int msg[16];
    cvp_ref_src_t *hdl = NULL;
    while (1) {

        os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        hdl = cvp_ref_src;
        if (hdl && hdl->state) {
            hdl->busy = 1;
            s16 *data = (s16 *)msg[1];
            int len = msg[2];
            /* putchar('r'); */
            int cbuf_data_len = cbuf_get_data_len(&hdl->cbuf);
            /*判断cbuf的缓存够一帧数据，并且参考数据可写长度大于1帧时*/
            while (cbuf_data_len >= CVP_REF_SRC_FRAME_SIZE && (get_audio_cvp_output_way_writable_len() * hdl->data_multiple) >= CVP_REF_SRC_FRAME_SIZE) {
                cbuf_data_len -= CVP_REF_SRC_FRAME_SIZE;
                cbuf_read(&hdl->cbuf, hdl->ref_tmp_buf, CVP_REF_SRC_FRAME_SIZE);
                audio_cvp_ref_src_run((s16 *)hdl->ref_tmp_buf, CVP_REF_SRC_FRAME_SIZE);
            }
            hdl->busy = 0;
        }
    }
}

int audio_cvp_ref_src_data_fill(void *p, s16 *data, int len)
{
    cvp_ref_src_t *hdl = cvp_ref_src;
    int ret = 0;
    if ((!esco_player_runing()
#if TCFG_USB_SLAVE_AUDIO_SPK_ENABLE
         && !pc_spk_player_runing()
#endif
        ) || (len == 0)) {
        return 0;
    }

    if (hdl && hdl->state) {
        if (0 == cbuf_write(&hdl->cbuf, data, len) && hdl->align_flag) {
            /* cbuf_clear(&hdl->cbuf); */
            /* hdl->align_flag = 0; */
            printf("ref src cbuf wfail!!");
            /*保证延时不变化*/
            cbuf_read_goback(&hdl->cbuf, len);
        }
        if (hdl->align_flag) {
            if (cbuf_get_data_len(&hdl->cbuf) >= CVP_REF_SRC_FRAME_SIZE) {
                ret = os_taskq_post_msg(CVP_REF_SRC_TASK_NAME, 2, (int)data, len);
            }
        }
    }
    return ret;
}

static void iis_write_callback(void *data, int len)
{
    cvp_ref_src_t *hdl = cvp_ref_src;
    if (hdl && hdl->state) {
        hdl->busy = 1;
        if (iis_read_pos == IIS_READ_MAGIC) {
            u32 iis_hwptr = get_alink_hwptr();
            if (iis_hwptr) {
                iis_read_pos = 0;
                printf("JL_ALNK HWPTR %d, data_len %d", iis_hwptr, len);
            }
        }
        if (iis_read_pos == 0) {
            if (esco_player_runing()
#if TCFG_USB_SLAVE_AUDIO_SPK_ENABLE
                || pc_spk_player_runing()
#endif
               ) {
                audio_cvp_ref_start(1);
            }
        }
        audio_cvp_ref_src_data_fill(NULL, data, len);
        hdl->busy = 0;
    }
}

static void multi_ch_iis_write_callback(void *priv, void *data, int len)
{
    /* putchar('M'); */
    u8 ch_idx = audio_cvp_ref_alink_ch_idx_get();
    /* u8 module = audio_cvp_ref_alink_module_idx_get(); */
    int *iis_data = (int *)data;
    int *wlen = (int *)len;

    iis_write_callback((void *)iis_data[ch_idx], wlen[ch_idx]);
}

/*
*********************************************************************
*                  Audio CVP Ref Src
* Description: 打开外部参考数据变采样
* Arguments  : scene	场景
*			   insr	    输入采样率
*			   outsr	输出采样率
*			   nch	    输入数据的通道数
* Return	 : None.
* Note(s)    : 声卡设备是DAC，默认不用外部提供参考数据
*********************************************************************
*/
int audio_cvp_ref_src_open(u8 scene, u32 insr, u32 outsr, u8 nch)
{
    printf("audio_cvp_ref_src_open");
    if (cvp_ref_src) {
        printf("aec_ref_src alreadly open !!!");
        return -1;
    }
    cvp_ref_src_t *hdl = zalloc(sizeof(cvp_ref_src_t));
    if (hdl == NULL) {
        printf("aec_ref_src malloc fail !!!");
        return -1;
    }
    int ref_buf_size = CVP_REF_SRC_FRAME_SIZE * nch * (audio_general_out_dev_bit_width() ? 4 : 2) * (insr / outsr) * 2;
    hdl->ref_buf = (u8 *)malloc(ref_buf_size);
    ASSERT(hdl->ref_buf);
    cbuf_init(&hdl->cbuf, hdl->ref_buf, ref_buf_size);
    int err = task_create(audio_cvp_ref_src_task, NULL, CVP_REF_SRC_TASK_NAME);
    if (err != OS_NO_ERR) {
        printf("task create error!");
        free(hdl);
        hdl = NULL;
        return -1;
    }

    audio_cvp_set_output_way(1);

    if (audio_cvp_ref_alink_multi_get()) {
#if TCFG_MULTI_CH_IIS_NODE_ENABLE
        muliti_ch_iis_node_write_callback_add(CVP_REF_SRC_TASK_NAME, 1, scene, multi_ch_iis_write_callback);
#endif
    } else {
#if TCFG_IIS_NODE_ENABLE
        iis_node_write_callback_add(CVP_REF_SRC_TASK_NAME, 1, scene, iis_write_callback);
#endif
    }

    hdl->input_rate = insr;
    hdl->output_rate = outsr;
    hdl->channel = nch;
    hdl->bit_width = audio_general_out_dev_bit_width();

    hdl->sw_src_api = get_rs16_context();
    printf("sw_src_api:0x%x\n", (int)(hdl->sw_src_api));
    ASSERT(hdl->sw_src_api);
    int sw_src_need_buf = hdl->sw_src_api->need_buf();
    printf("sw_src_buf:%d\n", sw_src_need_buf);
    hdl->sw_src_buf = zalloc(sw_src_need_buf);
    ASSERT(hdl->sw_src_buf, "sw_src_buf zalloc fail");
    RS_PARA_STRUCT rs_para_obj;
    rs_para_obj.nch = 1;
    rs_para_obj.dataTypeobj.IndataBit = 0;
    rs_para_obj.dataTypeobj.OutdataBit = 0;
    rs_para_obj.dataTypeobj.IndataInc = 1;
    rs_para_obj.dataTypeobj.OutdataInc = 1;
    rs_para_obj.dataTypeobj.Qval = 15;

    int multiple_cnt;//倍数
    if ((insr % 8000) == 0) {
        multiple_cnt = insr / 8000;
        rs_para_obj.new_insample = 6250;
        if (outsr == 16000) {
            //48k->16k
            rs_para_obj.new_outsample = 2080 * (6 / multiple_cnt);
        } else {
            //48k->8k
            rs_para_obj.new_outsample = (2080 / 2) * (6 / multiple_cnt);
        }
    } else {
        multiple_cnt = insr / 11025;
        rs_para_obj.new_insample = 12000;
        if (outsr == 16000) {
            //44.1k->16k
            rs_para_obj.new_outsample = (17 * 256) * (4 / multiple_cnt);
        } else {
            //44.1k->8k
            rs_para_obj.new_outsample = (17 * 256 / 2) * (4 / multiple_cnt);
        }
    }
    printf("sw src,ch = %d, in = %d,out = %d\n", rs_para_obj.nch, rs_para_obj.new_insample, rs_para_obj.new_outsample);
    hdl->sw_src_api->open(hdl->sw_src_buf, &rs_para_obj);
    hdl->data_multiple = (insr / outsr) * nch * (1 << hdl->bit_width);
    printf("indata nch %d, bitw %d, data_multiple %d", hdl->channel, hdl->bit_width, hdl->data_multiple);
    hdl->ref_tmp_buf = zalloc(CVP_REF_SRC_FRAME_SIZE);
    audio_cvp_ref_data_align_reset();
    hdl->state = 1;
    cvp_ref_src = hdl;
    return 0;
}

void audio_cvp_ref_src_close()
{
    cvp_ref_src_t *hdl = cvp_ref_src;
    if (hdl) {
        hdl->state = 0;
        while (hdl->busy) {
            putchar('w');
            os_time_dly(2);
        }
        int err = task_kill(CVP_REF_SRC_TASK_NAME);
        if (err) {
            printf("kill task %s: err=%d\n", CVP_REF_SRC_TASK_NAME, err);
        }

        if (audio_cvp_ref_alink_multi_get()) {
#if TCFG_MULTI_CH_IIS_NODE_ENABLE
            muliti_ch_iis_node_write_callback_del(CVP_REF_SRC_TASK_NAME);
#endif
        } else {
#if TCFG_IIS_NODE_ENABLE
            iis_node_write_callback_del(CVP_REF_SRC_TASK_NAME);
#endif
        }

        spin_lock(&cvp_ref_src_lock);
        if (hdl->sw_src_api) {
            hdl->sw_src_api = NULL;
        }
        if (hdl->sw_src_buf) {
            free(hdl->sw_src_buf);
            hdl->sw_src_buf = NULL;
        }
        if (hdl->ref_tmp_buf) {
            free(hdl->ref_tmp_buf);
            hdl->ref_tmp_buf = NULL;
        }
        if (hdl->ref_buf) {
            free(hdl->ref_buf);
            hdl->ref_buf = NULL;
        }

        free(hdl);
        hdl = NULL;
        cvp_ref_src = NULL;
        spin_unlock(&cvp_ref_src_lock);
    }
}

