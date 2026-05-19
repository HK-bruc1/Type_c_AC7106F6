/*************************************************************************************************/
/*!
*  \file      le_audio_file.c
*
*  \brief
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#include "source_node.h"
#include "le_audio_stream.h"
#include "reference_time.h"
#include "app_config.h"
#include "sync/audio_syncts.h"
#include "system/timer.h"
#include "effects/effects_adj.h"

#if LE_AUDIO_STREAM_ENABLE

struct le_audio_file_handle {
    u8 start;
    u8 step;
    u8 reference;
    u8 timestamp_enable;
    void *file;
    struct stream_node *node;
    int play_latency;
    u16 timer;
    u8 ble_to_local_time;
    u8 ble_to_local_id;
};

struct le_audio_file_params {
    u8 ble_to_local_time;
} __attribute__((packed));

_WEAK_ int CONFIG_LE_AUDIO_PLAY_LATENCY = 30000;

static void abandon_le_audio_data(void *p)
{
    struct le_audio_file_handle *hdl = (struct le_audio_file_handle *)p;
    while (le_audio_stream_get_frame_num(hdl->file) > 0) {
        struct le_audio_frame *le_audio_frame = le_audio_stream_get_frame(hdl->file);
        if (le_audio_frame) {
            le_audio_stream_free_frame(hdl->file, le_audio_frame);
        }
    }
}

static void le_audio_file_start_abandon_data(struct le_audio_file_handle *hdl)
{
    if (hdl->timer == 0) {
        hdl->timer = sys_timer_add(hdl, abandon_le_audio_data, 50);
        puts("start_abandon_le_audio_data\n");
    }
}

static void le_audio_file_stop_abandon_data(struct le_audio_file_handle *hdl)
{
    if (hdl->timer) {
        abandon_le_audio_data(hdl);
        puts("stop_abandon_le_audio_data\n");
        sys_timer_del(hdl->timer);
        hdl->timer = 0;
    }
}

static enum stream_node_state le_audio_get_frame(void *file, struct stream_frame **pframe)
{
    struct le_audio_file_handle *hdl = (struct le_audio_file_handle *)file;
    struct le_audio_frame *le_audio_frame = NULL;
    struct stream_frame *frame;

    /* if (hdl->step == 0) {  */
    /* if (!hdl->timestamp_enable && le_audio_stream_get_frame_num(hdl->file) < 3) { */
    /* *pframe = NULL; */
    /* return NODE_STA_RUN | NODE_STA_SOURCE_NO_DATA; */
    /* } */
    /* hdl->step = 1; */
    /* } */
    le_audio_frame = le_audio_stream_get_frame(hdl->file);
    if (!le_audio_frame) {
        *pframe = NULL;
        return NODE_STA_RUN | NODE_STA_SOURCE_NO_DATA;
    }

    frame = jlstream_get_frame(hdl->node->oport, le_audio_frame->len);
    if (frame == NULL) {
        *pframe = NULL;
        return NODE_STA_RUN;
    }

    memcpy(frame->data, le_audio_frame->data, le_audio_frame->len);
    /* if (hdl->ble_to_local_time) { */
    /*     le_audio_frame->timestamp = le_audio_ble_to_local_time(hdl->ble_to_local_id, le_audio_frame->timestamp); */
    /* } */
    /*put_buf(frame->data, 16);*/
    frame->len = le_audio_frame->len;
    frame->timestamp = ((le_audio_frame->timestamp + hdl->play_latency) & 0xfffffff) * TIMESTAMP_US_DENOMINATOR;
    frame->flags |= FRAME_FLAG_TIMESTAMP_ENABLE | FRAME_FLAG_UPDATE_TIMESTAMP;

    le_audio_stream_free_frame(hdl->file, le_audio_frame);
    *pframe = frame;

    if (le_audio_stream_get_frame_num(hdl->file) == 0) {
        return NODE_STA_RUN | NODE_STA_SOURCE_NO_DATA;
    }

    return NODE_STA_RUN;

}


static void *le_audio_file_init(void *priv, struct stream_node *node)
{
    struct le_audio_file_handle *hdl = (struct le_audio_file_handle *)zalloc(sizeof(struct le_audio_file_handle));

    hdl->node = node;
#if (defined(WIRELESS_MIC_PRODUCT_MODE)&& WIRELESS_MIC_PRODUCT_MODE)
    node->type |= NODE_TYPE_IRQ | NODE_TYPE_FLOW_CTRL;		//无线mic应用 使用irq类型,优化效率,需要要保证所有节点没有拆包输出.
#else
    node->type |= /* NODE_TYPE_IRQ | */ NODE_TYPE_FLOW_CTRL;//关闭irq类型的配置，避免解码器拆包的情况下，激活不及时的问题
#endif


    u16 plug_uuid = get_source_node_plug_uuid(priv);
    struct le_audio_file_params params = {0};
    jlstream_read_node_data_by_cfg_index(plug_uuid, hdl->node->subid, 0, (void *)&params, NULL);
    hdl->ble_to_local_time = params.ble_to_local_time;
    printf("le_audio_file_init hdl->ble_to_local_time : %d\n", hdl->ble_to_local_time);
    return hdl;
}

static int le_audio_file_set_bt_addr(struct le_audio_file_handle *hdl, void *bt_addr)
{
    hdl->file = (void *)bt_addr;

    return 0;
}
static void le_audio_rx_tick_handler(void *priv)
{
    struct le_audio_file_handle *hdl = (struct le_audio_file_handle *)priv;

    if (hdl->start) {
        if (!(hdl->node->type & NODE_TYPE_IRQ) &&
            !(hdl->node->state & NODE_STA_SOURCE_NO_DATA)) {
            return;
        }
        jlstream_wakeup_thread(NULL, hdl->node, NULL);
    }
}

static int le_audio_file_get_fmt(struct le_audio_file_handle *hdl, struct stream_fmt *fmt)
{
    struct le_audio_stream_format le_audio_fmt = {0};

    if (hdl->file) {
        int err = le_audio_stream_get_rx_format(hdl->file, &le_audio_fmt);
        if (err) {
            return -EAGAIN;
        }
    }
    fmt->frame_dms = le_audio_fmt.frame_dms;
    fmt->bit_rate = le_audio_fmt.bit_rate;
    fmt->coding_type = le_audio_fmt.coding_type;
    fmt->sample_rate = le_audio_fmt.sample_rate;
    fmt->channel_mode = le_audio_fmt.nch == 2 ? AUDIO_CH_LR : AUDIO_CH_MIX;
    return 0;
}

static int le_audio_file_start(struct le_audio_file_handle *hdl)
{
    if (hdl->start) {
        return 0;
    }

    le_audio_stream_set_rx_tick_handler(hdl->file, hdl, le_audio_rx_tick_handler);

    hdl->start = 1;
    int err = stream_node_ioctl(hdl->node, NODE_UUID_BT_AUDIO_SYNC, NODE_IOC_SYNCTS, 0);
    if (err) {
        err = stream_node_ioctl(hdl->node, NODE_UUID_PLAY_SYNC, NODE_IOC_SYNCTS, 0);
        if (err) {
            return 0;
        }
    }
    hdl->play_latency = CONFIG_LE_AUDIO_PLAY_LATENCY;
    hdl->timestamp_enable = 1;
    if (!hdl->reference) {
        hdl->reference = audio_reference_clock_select(hdl->file, 2);
    }
    /* if (hdl->ble_to_local_time) { */
    /*     hdl->ble_to_local_id = le_audio_ble_to_local_time_init(); */
    /* } */


    return 0;
}

static int le_audio_file_stop(struct le_audio_file_handle *hdl)
{
    if (hdl->reference) {
        audio_reference_clock_exit(hdl->reference);
    }
    /* if (hdl->ble_to_local_time) { */
    /*     le_audio_ble_to_local_time_close(hdl->ble_to_local_id); */
    /* } */
    hdl->reference = 0;

    if (hdl->start) {
        le_audio_stream_set_rx_tick_handler(hdl->file, NULL, NULL);

        le_audio_file_stop_abandon_data(hdl);
        hdl->start = 0;
    }

    return 0;
}

static int le_audio_file_ioctl(void *file, int cmd, int arg)
{
    int err = 0;
    struct le_audio_file_handle *hdl = (struct le_audio_file_handle *)file;

    switch (cmd) {
    case NODE_IOC_SET_BTADDR:
        le_audio_file_set_bt_addr(hdl, (void *)arg);
        break;
    case NODE_IOC_GET_FMT:
        le_audio_file_get_fmt(hdl, (struct stream_fmt *)arg);
        stream_node_ioctl(hdl->node, NODE_UUID_BT_AUDIO_SYNC, NODE_IOC_SET_SYNC_NETWORK, hdl->ble_to_local_time ? AUDIO_NETWORK_LOCAL : AUDIO_NETWORK_BLE);
        break;
    case NODE_IOC_START:
        le_audio_file_start(hdl);
        le_audio_file_stop_abandon_data(hdl);
        break;
    case NODE_IOC_SUSPEND:
        le_audio_file_stop(hdl);
        le_audio_file_start_abandon_data(hdl);
        break;
    case NODE_IOC_STOP:
        le_audio_file_stop(hdl);
        break;
    }

    return 0;
}

static void le_audio_file_release(void *file)
{
    struct le_audio_file_handle *hdl = (struct le_audio_file_handle *)file;

    le_audio_file_stop_abandon_data(hdl);

    free(hdl);
}

REGISTER_SOURCE_NODE_PLUG(le_audio_file_plug) = {
    .uuid       = NODE_UUID_LE_AUDIO_SINK,
    .init       = le_audio_file_init,
    .get_frame  = le_audio_get_frame,
    .ioctl      = le_audio_file_ioctl,
    .release    = le_audio_file_release,
};

#endif/*LE_AUDIO_STREAM_ENABLE*/

