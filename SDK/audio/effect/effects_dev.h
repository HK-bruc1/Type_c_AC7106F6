#ifndef __EFFECTS_DEV__H
#define __EFFECTS_DEV__H

#include "system/includes.h"

struct packet_ctrl {
    void *node_hdl;
    s16 *remain_buf;
    void (*effect_run)(void *, s16 *, s16 *, u32);
    u32 sample_rate;    //采样率
    u16 remain_len;     //记录算法消耗的剩余的数据长度
    u16 frame_len;
    u8 in_ch_num;       //输入声道数,单声道:1，立体声:2, 四声道:4
    u8 out_ch_num;      //输出声道数,单声道:1，立体声:2, 四声道:4
    u8 bit_width;       //位宽, 0:16bit  1:32bit
    u8 qval;            //q, 15:16bit, 23:24bit
};

void effect_dev_init(struct packet_ctrl *hdl, u32 process_points_per_ch);
void effect_dev_process(struct packet_ctrl *hdl, struct stream_iport *iport,  struct stream_note *note);
void effect_dev_close(struct packet_ctrl *hdl);


/*
 *                  第三方音效节点 双核流程使能
 * 数据流串接：effect_dev0(算法前处理) + effect_dev1(算法后处理)
 *
 * 注意事项：
 * （1）在数据流打开(jlstream_start)之前，需要注册双jlstream线程
 *      建议使用mic_effect1、2(优先级6, 与eff线程相同)，代码如下：
 *      jlstream_add_thread(player->stream, "mic_effect1");
 *      jlstream_add_thread(player->stream, "mic_effect2");
 *
 * （2）在数据流关闭(jlstream_stop)之前，需要调用 "effect_dev_sem_post()"
 *      防止算法关闭之后，前级不释放信号量导致后级死等
*
 * （3）需要保持链路(source->effect_dev0->effect_dev1->sync)数据长度相同
 *      根据source数据帧长进行算法处理数据帧长度配置。
 *      播放同步串在第三方节点之后。
 */
#define EFFECT_DEV_MULTI_TASK_ENABLE          0

/*
 * 算法处理数据帧长度(byte)
 */
#define EFFECT_DEV_TASK_FRAME_SIZE      256

/*
 * 算法process0与process1中间传递数据长度(byte)
 */
#define EFFECT_DEV_TASK_TEMP_FRAME_SIZE 256

#define DATA_BULK_MAX		3
struct data_bulk {
    struct list_head entry;
    void *addr;
    u16 len;
    u16 used;
};
struct audio_eff_hdl {
    volatile u8 start;				//eff模块状态
    volatile u8 busy;
    OS_SEM sem[2];
    /*数据复用相关数据结构*/
    struct data_bulk in_bulk[DATA_BULK_MAX];
    struct list_head in_head;
    struct data_bulk eff_bulk[DATA_BULK_MAX]; //中间数据
    struct list_head eff_head;
    OS_MUTEX eff_mutex;
    u8 *buf;           //输入缓存
    u8 *eff_frame;      //中间数据
    u16 eff_frame_byte;  //中间数据长度
    u16 in_frame_byte;  //输入数据长度
    u16 input_buf_byte; //输入缓存长度
    u8 buf_cnt;         //输入缓存buf位置
};

void effect_dev_sem_post();
void effect_dev_process0(void *frame_in, u16 len);
void effect_dev_process1(void *frame_out, u16 len);
void effect_dev_task_inbuf(void *buf, u16 len);
int effect_dev_task_open();
void effect_dev_task_close();

#endif
