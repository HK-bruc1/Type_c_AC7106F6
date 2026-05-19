#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".esco_tx_node.data.bss")
#pragma data_seg(".esco_tx_node.data")
#pragma const_seg(".esco_tx_node.text.const")
#pragma code_seg(".esco_tx_node.text")
#endif
#include "jlstream.h"
#include "classic/hci_lmp.h"
#include "media/audio_base.h"
#include "sync/audio_syncts.h"
#include "app_config.h"
#include "audio_time.h"

struct esco_tx_sync_node {
    u8 trigger;
    void *syncts;
    struct list_head entry;
};

struct esco_tx_hdl {
    u8 start;
    u8 reference_network;
    u8 swb;
    u8 sync_index;
    u8 tx_frames;
    u8 bt_addr[6];
    struct stream_node *node;
    struct list_head sync_list;
};

static void esco_tx_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct esco_tx_hdl *hdl = (struct esco_tx_hdl *)iport->node->private_data;
    struct stream_frame *frame;

    while (1) {
        frame = jlstream_pull_frame(iport, note);
        if (!frame) {
            break;
        }
        /*printf("esco_tx: %d\n", frame->len);*/
        if (hdl->swb) {
            u8 sco_frame[60];
            u8 H2_header_index[4] = {0x08, 0x38, 0xC8, 0xF8};
            sco_frame[0] = 0x01;
            sco_frame[1] = H2_header_index[hdl->sync_index++];
            if (hdl->sync_index >= 4) {
                hdl->sync_index = 0;
            }
            memcpy(&sco_frame[2], frame->data, frame->len);
            lmp_private_send_esco_packet(NULL, sco_frame, 60);
        } else {
            lmp_private_send_esco_packet(NULL, frame->data, frame->len);
        }

        jlstream_free_frame(frame);
    }
}

void esco_tx_tick_handler(void *priv, u32 clkn)
{
    struct esco_tx_hdl *hdl = (struct esco_tx_hdl *)priv;
    struct esco_tx_sync_node *node = NULL;

    u32 time = bt_edr_conn_master_to_local_time(hdl->bt_addr, (clkn * 625));
    list_for_each_entry(node, &hdl->sync_list, entry) {
        if (!node->trigger) {
            node->trigger = 1;
            sound_pcm_syncts_latch_trigger(node->syncts);
        }
        sound_pcm_update_frame_num(node->syncts, hdl->tx_frames);
        sound_pcm_update_frame_num_and_time(node->syncts, 0, time, 0);
    }

}

static int esco_tx_bind(struct stream_node *node, u16 uuid)
{
    return 0;
}

static void esco_tx_open_iport(struct stream_iport *iport)
{
    struct esco_tx_hdl *hdl = (struct esco_tx_hdl *)iport->node->private_data;

    INIT_LIST_HEAD(&hdl->sync_list);
    hdl->reference_network = 0xff;
    hdl->node = iport->node;
}

static void esco_tx_set_bt_addr(struct esco_tx_hdl *hdl, void *bt_addr)
{
    int media_type = lmp_private_get_esco_packet_type() & 0xff;

    memcpy(hdl->bt_addr, bt_addr, 6);

    if (media_type == 2) {
        struct stream_enc_fmt fmt = {
            .channel = 1,           /*Mono*/
            .bit_rate = 62000,      /*61867bits/s*/
            .sample_rate = 32000,   /*32Khz*/
            .frame_dms = 75,        /*7.5ms*/
            .coding_type = AUDIO_CODING_LC3,
        };
        jlstream_node_ioctl(jlstream_for_node(hdl->node), NODE_UUID_ENCODER, NODE_IOC_SET_ENC_FMT, (int)&fmt);
    }

    bt_edr_conn_system_clock_init(hdl->bt_addr, 1);
}

static int esco_tx_ioc_fmt_nego(struct stream_iport *iport)
{
    struct esco_tx_hdl *hdl = (struct esco_tx_hdl *)iport->node->private_data;
    struct stream_fmt *in_fmt = &iport->prev->fmt;

    int type = lmp_private_get_esco_packet_type();
    int frame_time = (lmp_private_get_esco_packet_type() >> 8) & 0xff;
    int media_type = type & 0xff;
    if (media_type == 0) {
        in_fmt->sample_rate = 8000;
        in_fmt->coding_type = AUDIO_CODING_CVSD;
        hdl->tx_frames = frame_time == 12 ? 60 : 30;
    } else if (media_type == 2) {
        in_fmt->sample_rate = 32000;
        in_fmt->coding_type = AUDIO_CODING_LC3;
        hdl->tx_frames = 60;
        hdl->swb = 1;
    } else {
        // 快速通话挂断会有临界状态lmp_private_get_esco_packet_type会返回-1，
        // 需要保证else下面的编码格式，比如else配置LC3会导致没有LC3编码的项目出现无法获取编码器的断言
        in_fmt->sample_rate = 16000;
        in_fmt->coding_type = AUDIO_CODING_MSBC;
        hdl->tx_frames = frame_time == 12 ? 120 : 60;
    }
    in_fmt->channel_mode = AUDIO_CH_MIX;

    return NEGO_STA_ACCPTED;
}

static int esco_tx_mount_syncts(struct esco_tx_hdl *hdl, void *syncts, u32 timestamp, u8 network)
{
    struct esco_tx_sync_node *node = NULL;
    list_for_each_entry(node, &hdl->sync_list, entry) {
        if ((u32)node->syncts == (u32)syncts) {
            return 0;
        }
    }

    node = (struct esco_tx_sync_node *)zalloc(sizeof(struct esco_tx_sync_node));
    node->syncts = syncts;

    /*g_printf("esco tx mount syncts : 0x%x, %u\n", (u32)syncts, timestamp);*/
    if (hdl->reference_network == 0xff) {
        hdl->reference_network = network;
    }
    list_add(&node->entry, &hdl->sync_list);

    return 0;
}

static void esco_tx_unmount_syncts(struct esco_tx_hdl *hdl, void *syncts)
{
    struct esco_tx_sync_node *node;

    list_for_each_entry(node, &hdl->sync_list, entry) {
        if (node->syncts == syncts) {
            goto unmount;
        }
    }

    return;

unmount:
    /*g_printf("esco tx unmount syncts : 0x%x\n", syncts);*/
    list_del(&node->entry);
    free(node);
}

static int esco_tx_syncts_handler(struct esco_tx_hdl *hdl, struct audio_syncts_ioc_params *params)
{
    if (!params) {
        return 0;
    }

    switch (params->cmd) {
    case AUDIO_SYNCTS_MOUNT_ON_SNDPCM:
        esco_tx_mount_syncts(hdl, (void *)params->data[0], params->data[1], params->data[2]);
        break;
    case AUDIO_SYNCTS_UMOUNT_ON_SNDPCM:
        esco_tx_unmount_syncts(hdl, (void *)params->data[0]);
        break;
    }

    return 0;
}

static int esco_tx_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    struct esco_tx_hdl *hdl = (struct esco_tx_hdl *)iport->node->private_data;

    switch (cmd) {
    case NODE_IOC_SET_BTADDR:
        esco_tx_set_bt_addr(hdl, (void *)arg);
        break;
    case NODE_IOC_OPEN_IPORT:
        esco_tx_open_iport(iport);
        break;
    case NODE_IOC_START:
        lmp_esco_set_tx_notify(hdl->bt_addr, hdl, esco_tx_tick_handler);
        break;
    case NODE_IOC_STOP:
        lmp_esco_set_tx_notify(hdl->bt_addr, NULL, NULL);
        break;
    case NODE_IOC_CLOSE_IPORT:
        break;
    case NODE_IOC_NEGOTIATE:
        *(int *)arg |= esco_tx_ioc_fmt_nego(iport);
        break;
    case NODE_IOC_SYNCTS:
        esco_tx_syncts_handler(hdl, (struct audio_syncts_ioc_params *)arg);
        break;
    case NODE_IOC_GET_DELAY:
        return lmp_private_get_esco_tx_packet_num() * 75;
    }

    return 0;
}

static void esco_tx_release(struct stream_node *node)
{

}

/*
 * 1、没有定义仅显示电量，TCFG_BT_SUPPORT_HFP使能则有通话功能
 * 2、定义仅显示电量，则TCFG_BT_HFP_ONLY_DISPLAY_BAT_ENABLE不使能且TCFG_BT_SUPPORT_HFP使能有通话功能
 */
#if ((!defined TCFG_BT_HFP_ONLY_DISPLAY_BAT_ENABLE) && TCFG_BT_SUPPORT_HFP) || \
	((defined TCFG_BT_HFP_ONLY_DISPLAY_BAT_ENABLE) && (!TCFG_BT_HFP_ONLY_DISPLAY_BAT_ENABLE) && TCFG_BT_SUPPORT_HFP)

REGISTER_STREAM_NODE_ADAPTER(esco_tx_adapter) = {
    .uuid       = NODE_UUID_ESCO_TX,
    .bind       = esco_tx_bind,
    .ioctl      = esco_tx_ioctl,
    .release    = esco_tx_release,
    .handle_frame = esco_tx_handle_frame,
    .hdl_size   = sizeof(struct esco_tx_hdl),
};
#endif
