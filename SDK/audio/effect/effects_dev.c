
#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "audio_config.h"
#include "jlstream.h"
#include "effects/effects_adj.h"
#include "effects_dev.h"

#define LOG_TAG_CONST EFFECTS
#define LOG_TAG     "[EFFECTS_ADJ]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"


#ifdef SUPPORT_MS_EXTENSIONS
#pragma   bss_seg(".effect_dev.data.bss")
#pragma  data_seg(".effect_dev.data")
#pragma const_seg(".effect_dev.text.const")
#pragma  code_seg(".effect_dev.text")
#endif
#if (TCFG_EFFECT_DEV0_NODE_ENABLE || TCFG_EFFECT_DEV1_NODE_ENABLE || TCFG_EFFECT_DEV2_NODE_ENABLE || TCFG_EFFECT_DEV3_NODE_ENABLE || TCFG_EFFECT_DEV4_NODE_ENABLE)


void effect_dev_init(struct packet_ctrl *hdl, u32 process_points_per_ch)
{
    if (process_points_per_ch) {
        hdl->frame_len = process_points_per_ch * hdl->in_ch_num * (2 << hdl->bit_width);
        hdl->remain_buf = zalloc(hdl->frame_len);
    }
}

void effect_dev_process(struct packet_ctrl *hdl, struct stream_iport *iport,  struct stream_note *note)
{
    struct stream_frame *frame;
    struct stream_frame *out_frame = NULL;
    struct stream_node *node = iport->node;

    if (hdl->remain_buf) {
        frame = jlstream_pull_frame(iport, note);		//从iport读取数据
        if (!frame) {
            return;
        }

        /*算法出来一帧的数据长度，byte*/
        int total_len = hdl->remain_len + frame->len;  //记录目前还有多少数据
        u8 pack_frame_num = total_len / hdl->frame_len;//每次数据需要跑多少帧
        u16 pack_frame_len = pack_frame_num * hdl->frame_len;       //记录本次需要跑多少数据
        u16 out_frame_len = hdl->out_ch_num * pack_frame_len / hdl->in_ch_num;

        if (pack_frame_num) {
            if (!out_frame) {
                out_frame = jlstream_get_frame(node->oport, out_frame_len);
                if (!out_frame) {
                    return;
                }
            }
            int out_offset = 0;
            if (hdl->remain_len) {
                int need_size = hdl->frame_len - hdl->remain_len;
                /*拷贝上一次剩余的数据*/
                memcpy((u8 *)hdl->remain_buf + hdl->remain_len, frame->data, need_size);
                if (hdl->out_ch_num == hdl->in_ch_num) {
                    memcpy(out_frame->data, hdl->remain_buf, hdl->frame_len);
                }
                hdl->effect_run(hdl->node_hdl, (s16 *)hdl->remain_buf, (s16 *)out_frame->data,
                                hdl->frame_len);
                hdl->remain_len = 0;
                frame->offset += need_size;
                out_offset = hdl->frame_len * hdl->out_ch_num / hdl->in_ch_num;
                pack_frame_num--;
            }
            while (pack_frame_num--) {
                if (hdl->out_ch_num == hdl->in_ch_num) {
                    memcpy(out_frame->data + out_offset, frame->data + frame->offset,
                           hdl->frame_len);
                }
                hdl->effect_run(hdl->node_hdl, (s16 *)(frame->data + frame->offset),
                                (s16 *)(out_frame->data + out_offset), hdl->frame_len);
                frame->offset += hdl->frame_len;
                out_offset += hdl->frame_len * hdl->out_ch_num  / hdl->in_ch_num;
            }
            if (frame->offset < frame->len) {
                memcpy(hdl->remain_buf, frame->data + frame->offset, frame->len - frame->offset);
                hdl->remain_len = frame->len - frame->offset;
            }
            out_frame->len = out_frame_len;
            jlstream_push_frame(node->oport, out_frame);	//将数据推到oport
            jlstream_free_frame(frame);	//释放iport资源
        } else {
            /*当前数据不够跑一帧算法时*/
            memcpy((void *)((int)hdl->remain_buf + hdl->remain_len), frame->data, frame->len);
            hdl->remain_len += frame->len;
            jlstream_free_frame(frame);	//释放iport资源
        }


    } else {
        frame = jlstream_pull_frame(iport, note);		//从iport读取数据
        if (!frame) {
            return;
        }

        if (hdl->out_ch_num != hdl->in_ch_num) {
            int out_len = hdl->out_ch_num * frame->len / hdl->in_ch_num;
            out_frame = jlstream_get_frame(node->oport, out_len);
            if (!out_frame) {
                return;
            }
            hdl->effect_run(hdl->node_hdl, (s16 *)frame->data, (s16 *)out_frame->data, frame->len);
            out_frame->len = out_len;
            jlstream_push_frame(node->oport, out_frame);	//将数据推到oport
            jlstream_free_frame(frame);	//释放iport资源
        } else {
            hdl->effect_run(hdl->node_hdl, (s16 *)frame->data, (s16 *)frame->data, frame->len);
            jlstream_push_frame(node->oport, frame);	//将数据推到oport
        }
    }
}
void effect_dev_close(struct packet_ctrl *hdl)
{
    hdl->remain_len = 0;
    if (hdl->remain_buf) {
        free(hdl->remain_buf);
        hdl->remain_buf = NULL;
    }
}

#if EFFECT_DEV_MULTI_TASK_ENABLE

static struct audio_eff_hdl *eff_hdl = NULL;

//算法资源初始化
void effect_dev_algo_init()
{
}
//算法资源释放
void effect_dev_algo_close()
{
}
//算法part0
void effect_dev_algo_proces0(void *in, void *temp, u16 len)
{
    /* printf("cpu %d, process0\n", current_cpu_id()); */
    /* udelay(5000); //引入延时模拟算法运行时间，不可超过帧长 */
    memcpy(temp, in, len);
}
//算法part1
void effect_dev_algo_proces1(void *temp, void *out, u16 len)
{
    /* printf("cpu %d, process1\n", current_cpu_id()); */
    /* udelay(5000); //引入延时模拟算法运行时间，不可超过帧长 */
    memcpy(out, temp, len);
}

void effect_dev_sem_post()
{
    if (eff_hdl) {
        os_sem_set(&eff_hdl->sem[1], 0);
        os_sem_post(&eff_hdl->sem[1]);
    }
}
void effect_dev_process0(void *frame_in, u16 len)
{
    if (!eff_hdl || !eff_hdl->start) {
        return;
    }
    u8 i = 0;
    u8 *eff_out;
    for (i = 0; i < DATA_BULK_MAX; i++) {
        if (eff_hdl->eff_bulk[i].used == 0) {
            eff_out = eff_hdl->eff_frame + (eff_hdl->eff_frame_byte * i);
            effect_dev_algo_proces0(frame_in, eff_out, len);
            break;
        }
    }

    os_mutex_pend(&eff_hdl->eff_mutex, 0);
    if (i < DATA_BULK_MAX) {
        /* printf("bulk:%d\n",i); */
        eff_hdl->eff_bulk[i].addr = (void *)eff_out;
        eff_hdl->eff_bulk[i].used = 0x55;
        eff_hdl->eff_bulk[i].len = eff_hdl->eff_frame_byte;
        list_add_tail(&eff_hdl->eff_bulk[i].entry, &eff_hdl->eff_head);
    } else {
        printf(">>>eff_bulk_in_full\n");
        /*align reset*/
        struct data_bulk *bulk;
        list_for_each_entry(bulk, &eff_hdl->eff_head, entry) {
            bulk->used = 0;
            __list_del_entry(&bulk->entry);
        }
    }
    os_mutex_post(&eff_hdl->eff_mutex);
    effect_dev_sem_post();
}

void effect_dev_process1(void *frame_out, u16 len)
{
    if (!eff_hdl || !eff_hdl->start) {
        return;
    }
    os_sem_pend(&eff_hdl->sem[1], 0);
    if (list_empty(&eff_hdl->eff_head)) {
        log_error("eff-nn process1 read err\n");
        memset(frame_out, 0x00, len);
        return;
    }
    struct data_bulk *eff_bulk = NULL;
    os_mutex_pend(&eff_hdl->eff_mutex, 0);
    eff_bulk = list_first_entry(&eff_hdl->eff_head, struct data_bulk, entry);
    list_del(&eff_bulk->entry);
    os_mutex_post(&eff_hdl->eff_mutex);
    void *frame_temp = eff_bulk->addr;
    effect_dev_algo_proces1(frame_temp, frame_out, len);
    eff_bulk->used = 0;
}


static void audio_eff_task(void *priv)
{
    printf("==Audio eff Task==\n");
    struct data_bulk *bulk = NULL;
    u8 pend = 1;
    while (1) {
        if (pend) {
            os_sem_pend(&eff_hdl->sem[0], 0);
        }
        pend = 1;
        if (eff_hdl->start) {
            if (!list_empty(&eff_hdl->in_head)) {
                eff_hdl->busy = 1;
                local_irq_disable();
                /*1.获取主mic数据*/
                bulk = list_first_entry(&eff_hdl->in_head, struct data_bulk, entry);
                list_del(&bulk->entry);
                void *data = bulk->addr;
                u16 len = bulk->len;
                local_irq_enable();
                effect_dev_process0(data, len);
                local_irq_disable();
                bulk->used = 0;
                local_irq_enable();
                eff_hdl->busy = 0;
                pend = 0;
            }
        }
    }
}

void effect_dev_task_inbuf(void *buf, u16 len)
{
    if (eff_hdl && eff_hdl->start) {
        u8 *dat = eff_hdl->buf + (len * eff_hdl->buf_cnt);
        memcpy(dat, buf, len);
        if (++eff_hdl->buf_cnt > ((eff_hdl->input_buf_byte / eff_hdl->in_frame_byte) - 1)) {	//计算下一个ADCbuffer位置
            eff_hdl->buf_cnt = 0;
        }
        u8 i = 0;
        for (i = 0; i < DATA_BULK_MAX; i++) {
            if (eff_hdl->in_bulk[i].used == 0) {
                break;
            }
        }

        if (i < DATA_BULK_MAX) {
            eff_hdl->in_bulk[i].addr = (void *)dat;
            eff_hdl->in_bulk[i].used = 0x55;
            eff_hdl->in_bulk[i].len = len;
            list_add_tail(&eff_hdl->in_bulk[i].entry, &eff_hdl->in_head);
        } else {
            printf(">>>eff_in_full\n");
            struct data_bulk *bulk;
            list_for_each_entry(bulk, &eff_hdl->in_head, entry) {
                bulk->used = 0;
                __list_del_entry(&bulk->entry);
            }
            return;
        }
        os_sem_set(&eff_hdl->sem[0], 0);
        os_sem_post(&eff_hdl->sem[0]);
    }
}

int effect_dev_task_open()
{
    ASSERT(CPU_CORE_NUM > 1); //cpu核心数检查

    if (eff_hdl) {
        printf("eff_hdl is already open");
        return -1;
    }
    eff_hdl = zalloc(sizeof(struct audio_eff_hdl));
    if (eff_hdl == NULL) {
        printf("eff_hdl malloc failed");
        return -ENOMEM;
    }
    printf("eff_hdl size:%ld\n", sizeof(struct audio_eff_hdl));
    INIT_LIST_HEAD(&eff_hdl->in_head);
    os_sem_create(&eff_hdl->sem[0], 0);
    os_sem_create(&eff_hdl->sem[1], 0);
    task_create(audio_eff_task, NULL, "eff_mtask");
    eff_hdl->start = 1;
    effect_dev_algo_init();
    os_mutex_create(&eff_hdl->eff_mutex);
    //双核：申请process0 与 process1 的中间数据
    INIT_LIST_HEAD(&eff_hdl->eff_head);
    eff_hdl->in_frame_byte = EFFECT_DEV_TASK_FRAME_SIZE;
    eff_hdl->input_buf_byte = eff_hdl->in_frame_byte * 3;
    eff_hdl->buf = zalloc(eff_hdl->input_buf_byte);
    ASSERT(eff_hdl->buf);
    eff_hdl->buf_cnt = 0;
    eff_hdl->eff_frame_byte = EFFECT_DEV_TASK_TEMP_FRAME_SIZE;
    eff_hdl->eff_frame = zalloc(eff_hdl->eff_frame_byte * DATA_BULK_MAX);
    ASSERT(eff_hdl->eff_frame);
    printf("audio_eff_open succ\n");
    return 0;
}

void effect_dev_task_close()
{
    printf("audio_eff_close:%x", (u32)eff_hdl);
    if (eff_hdl) {
        eff_hdl->start = 0;
        while (eff_hdl->busy) {
            os_time_dly(2);
        }
        task_kill("eff_mtask");
    }
    effect_dev_algo_close();
    if (eff_hdl->eff_frame) {
        free(eff_hdl->eff_frame);
        eff_hdl->eff_frame = NULL;
    }
    if (eff_hdl->buf) {
        free(eff_hdl->buf);
        eff_hdl->buf = NULL;
    }
    os_mutex_del(&eff_hdl->eff_mutex, 0);
    local_irq_disable();
    free(eff_hdl);
    eff_hdl = NULL;
    local_irq_enable();
    printf("audio_eff_close succ\n");
}


#endif
#endif


