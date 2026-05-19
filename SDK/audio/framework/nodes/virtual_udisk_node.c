
#include "jlstream.h"
#include "media/audio_base.h"
#include "sync/audio_syncts.h"
#include "circular_buf.h"
#include "audio_splicing.h"
#include "app_config.h"
#include "gpio.h"
#include "audio_cvp.h"
#include "media/audio_general.h"
#include "audio_config_def.h"
#include "effects/convert_data.h"
#include "media/audio_dev_sync.h"
#include "media/audio_cfifo.h"
#include "reference_time.h"
#include "le_audio_player.h"


#if TCFG_VIR_UDISK_ENABLE

#define VIRTUAL_UDISK_MP3_SYNC 1
#define ENC_CACHE_BUF_SIZE	1024 * 40
#define CACHE_READ_BUF_SIZE 192
#define FILL_ZERO_CIRCLE_CNT 20	//补0循环的上限, 超出这个循环将补0包
static DEFINE_SPINLOCK(vir_udisk_lock);

struct virtual_udisk_node_hdl {
    char name[16];
    enum stream_scene scene;
    cbuffer_t enc_cbuffer;
    int last_mp3_enc_time;
    u8 *cache_buf;
    u8 *read_tmp_buf;	//暂存读取的数据
    u8 reference_network;
    u8 start;
    u8 cache_flag;
    struct list_head sync_list;
};

struct virtual_udisk_sync_node {
    u8 trigger;
    void *syncts;
    struct list_head entry;
};

static struct virtual_udisk_node_hdl *g_vir_udisk_hdl = NULL;

static const unsigned char mp3_default_data[] = {
    0xFF, 0xFD, 0x44, 0x04, 0xF0, 0x90, 0x00, 0x00, 0x00, 0x03, 0xF0, 0x0F, 0xBE, 0xFB, 0xEF, 0xBE,
    0xFB, 0xEF, 0xBE, 0xFB, 0xE7, 0xFF, 0xEF, 0xFF, 0xDF, 0xFF, 0xBF, 0xDF, 0xEF, 0xF7, 0xEF, 0xDF,
    0xBF, 0x7E, 0xFD, 0xFF, 0xFB, 0xFF, 0xF7, 0xFF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF,
    0xBF, 0x7F, 0xFE, 0xFF, 0xFD, 0xFF, 0xFB, 0xFD, 0xFE, 0xFF, 0x7E, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF,
    0xFF, 0xBF, 0xFF, 0x7F, 0xFE, 0xFF, 0x7F, 0xBF, 0xDF, 0xBF, 0x7E, 0xFD, 0xFB, 0xF7, 0xFF, 0xEF,
    0xFF, 0xDF, 0xFF, 0xBF, 0xDF, 0xEF, 0xF7, 0xEF, 0xDF, 0xBF, 0x7E, 0xFD, 0xFF, 0xFB, 0xFF, 0xF7,
    0xFF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F, 0xFE, 0xFF, 0xFD, 0xFF, 0xFB,
    0xFD, 0xFE, 0xFF, 0x7E, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xFF, 0xBF, 0xFF, 0x7F, 0xFE, 0xFF, 0x7F,
    0xBF, 0xDF, 0xBF, 0x7E, 0xFD, 0xFB, 0xF7, 0xFF, 0xEF, 0xFF, 0xDF, 0xFF, 0xBF, 0xDF, 0xEF, 0xF7,
    0xEF, 0xDF, 0xBF, 0x7E, 0xFD, 0xFF, 0xFB, 0xFF, 0xF7, 0xFF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFB, 0xF7,
    0xEF, 0xDF, 0xBF, 0x7F, 0xFE, 0xFF, 0xFD, 0xFF, 0xFB, 0xFD, 0xFE, 0xFF, 0x7E, 0xFD, 0xFB, 0xF7,
    0xEF, 0xDF, 0xFF, 0xBF, 0xFF, 0x7F, 0xFE, 0xFF, 0x7F, 0xBF, 0xDF, 0xBF, 0x7E, 0xFD, 0xFB, 0xF0,
};
static u16 s_cntj = 0;
static void virtual_fill_zero_data(u16 *s_cnt, void *buf, u32 len)
{
    putchar('0');
    u8 *data = (u8 *)buf;
    int mp3_default_data_size = sizeof(mp3_default_data);
    u32 remain = len;
    while (remain--) {
        if (*s_cnt >= mp3_default_data_size) {
            *s_cnt = 0;
        }
        *data++ = mp3_default_data[*s_cnt];
        (*s_cnt)++;
    }
}

static int virtual_udisk_adapter_bind(struct stream_node *node, u16 uuid)
{
    struct virtual_udisk_node_hdl *hdl = (struct virtual_udisk_node_hdl *)node->private_data;
    g_vir_udisk_hdl = hdl;
    return 0;
}


static void virtual_udisk_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct virtual_udisk_node_hdl *hdl = (struct virtual_udisk_node_hdl *)iport->node->private_data;

    if (hdl->start == 0) {
        putchar('z');
        return;
    }
    struct stream_frame *frame = NULL;

    frame = jlstream_pull_frame(iport, NULL);
    if (!frame) {
        return;
    }

    spin_lock(&vir_udisk_lock);

    u8 *data = (u8 *)frame->data;
    int wlen = cbuf_write(&hdl->enc_cbuffer, data, frame->len);
    if (cbuf_get_data_len(&hdl->enc_cbuffer) >= ENC_CACHE_BUF_SIZE / 8 && hdl->cache_flag == 0) {
        hdl->cache_flag = 1;
    }

    if (hdl->cache_flag == 0) {
        putchar('C');
    }

    if (wlen != frame->len) {
        //此时数据溢出，有可能Virtual Disk 没有来拿数
        //此时需要覆盖旧数据
        do {
            putchar('W');
            int rlen = cbuf_read(&hdl->enc_cbuffer, hdl->read_tmp_buf, CACHE_READ_BUF_SIZE);	//每次取出CACHE_READ_BUF_SIZE 这么长的数据
            /* os_time_dly(1); */	//中断里不可调用os_time_dly
            wlen = cbuf_write(&hdl->enc_cbuffer, data, frame->len);
        } while (wlen != frame->len);
    }
    spin_unlock(&vir_udisk_lock);

    jlstream_free_frame(frame);
}
/* extern volatile int mp3_enc_time; */

int virtual_file_read(void *buf, u32 addr, u32 len)
{
    struct virtual_udisk_node_hdl *hdl = g_vir_udisk_hdl;
    int rlen = 0;
    struct virtual_udisk_sync_node *node = NULL;
    static u32 read_cnt = 0;
    if (hdl && hdl->start && hdl->cache_flag) {

        rlen = cbuf_read(&hdl->enc_cbuffer, buf, len);

#if VIRTUAL_UDISK_MP3_SYNC
        read_cnt++;
        int mp3_enc_time = virtual_udisk_get_enc_time();
        int diff_time = mp3_enc_time - hdl->last_mp3_enc_time;
        if (diff_time == 1) {
            printf("read_cnt:%d, enc_time:%d, %d\n", read_cnt, mp3_enc_time, hdl->last_mp3_enc_time);
            u32 time = audio_jiffies_usec();
            int points = diff_time * 64000 / 4 * 2;
            read_cnt = 0;
            list_for_each_entry(node, &hdl->sync_list, entry) {
                if (!node->trigger) {
                    node->trigger = 1;
                    sound_pcm_syncts_latch_trigger(node->syncts);
                }
                sound_pcm_update_frame_num(node->syncts, points);
                sound_pcm_update_frame_num_and_time(node->syncts, 0, time, 0);
            }
        } else if (diff_time >= 2) {
            read_cnt = 0;
        }
        hdl->last_mp3_enc_time = mp3_enc_time;
#endif

        if (rlen != len) {
            putchar('A');
            int cbuf_remain = 0;
            int circle_cnt = 0;	//循环次数上限
            do {
                cbuf_remain = cbuf_get_data_len(&hdl->enc_cbuffer);
                if (cbuf_remain >= len) {
                    rlen = cbuf_read(&hdl->enc_cbuffer, buf, len);
                    break;
                }
                putchar('B');
                circle_cnt ++;
                os_time_dly(1);
            } while (cbuf_remain < len && circle_cnt < FILL_ZERO_CIRCLE_CNT);
            if (circle_cnt >= FILL_ZERO_CIRCLE_CNT) {
                r_printf("...fill zero packet!\n");
                virtual_fill_zero_data(&s_cntj, buf, len);
            }
        }
    } else {
        virtual_fill_zero_data(&s_cntj, buf, len);
        rlen = len;
    }
__exit:
    return rlen;
}

int virtual_file_write(void *buf, u32 len)
{
    struct virtual_udisk_node_hdl *hdl = g_vir_udisk_hdl;
    int wlen = 0;

    spin_lock(&vir_udisk_lock);
    if (hdl && hdl->cache_buf) {
        wlen = cbuf_write(&hdl->enc_cbuffer, buf, len);
    }
    spin_lock(&vir_udisk_lock);
    return wlen;
}

static void virtual_udisk_adapter_open_iport(struct stream_iport *iport)
{
    struct virtual_udisk_node_hdl *hdl = (struct virtual_udisk_node_hdl *)iport->node->private_data;
    INIT_LIST_HEAD(&hdl->sync_list);
    hdl->reference_network = 0xff;
    iport->handle_frame = virtual_udisk_handle_frame;
}

static int virtual_udisk_ioc_negotiate(struct stream_iport *iport)
{
    struct stream_fmt *in_fmt = &iport->prev->fmt;
    u8 bit_width = audio_general_out_dev_bit_width();
    int ret = NEGO_STA_ACCPTED;
    printf("bit_width:%d\n", bit_width);
    if (bit_width) {
        if (in_fmt->bit_wide != DATA_BIT_WIDE_24BIT) {
            in_fmt->bit_wide = DATA_BIT_WIDE_24BIT;
            ret = NEGO_STA_CONTINUE;
        }
    } else {
        if (in_fmt->bit_wide != DATA_BIT_WIDE_16BIT) {
            in_fmt->bit_wide = DATA_BIT_WIDE_16BIT;
            ret = NEGO_STA_CONTINUE;
        }
    }

    if (in_fmt->coding_type != AUDIO_CODING_MP3) {
        r_printf(">>>>>>> in_fmt->coding_type: %x\n", in_fmt->coding_type);
        in_fmt->coding_type = AUDIO_CODING_MP3;
        ret = NEGO_STA_CONTINUE;
    }

    return ret;
}

static void virtual_udisk_ioc_start(struct virtual_udisk_node_hdl *hdl)
{
    y_printf(">>>>>>>>>>>>>>>>>>>>>>>>> VIR UDISK START!\n");
    if (hdl->cache_buf == NULL) {
        hdl->cache_buf = zalloc(ENC_CACHE_BUF_SIZE);
    }

    if (hdl->read_tmp_buf == NULL) {
        hdl->read_tmp_buf = zalloc(CACHE_READ_BUF_SIZE);
    }

    if (hdl->cache_buf && hdl->read_tmp_buf) {
        cbuf_init(&hdl->enc_cbuffer, hdl->cache_buf, ENC_CACHE_BUF_SIZE);
    } else {
        ASSERT(0, "%s, %d, cache buf is NULL!\n", __func__, __LINE__);
    }

    hdl->start = 1;
    hdl->cache_flag = 0;
    hdl->last_mp3_enc_time = 0;
}

static void virtual_udisk_ioc_stop(struct virtual_udisk_node_hdl *hdl)
{
    r_printf(">>>>>>>>>>>>>>>>>>>>>>>>> VIR UDISK STOP!\n");
    if (hdl->start) {
        hdl->start = 0;
    }
}

#if VIRTUAL_UDISK_MP3_SYNC
static int virtual_udisk_mount_syncts(struct virtual_udisk_node_hdl *hdl, void *syncts, u32 timestamp, u8 network)
{
    struct virtual_udisk_sync_node *node = NULL;
    list_for_each_entry(node, &hdl->sync_list, entry) {
        if ((u32)node->syncts == (u32)syncts) {
            return 0;
        }
    }
    node = (struct virtual_udisk_sync_node *)zalloc(sizeof(struct virtual_udisk_sync_node));
    node->syncts = syncts;
    if (hdl->reference_network == 0xff) {
        hdl->reference_network = network;
    }
    list_add(&node->entry, &hdl->sync_list);
    return 0;
}

static void virtual_udisk_unmount_syncts(struct virtual_udisk_node_hdl *hdl, void *syncts)
{
    struct virtual_udisk_sync_node *node = NULL;
    list_for_each_entry(node, &hdl->sync_list, entry) {
        if (node->syncts == syncts) {
            goto unmount;
        }
    }
    return;

unmount:
    list_del(&node->entry);
    free(node);
}


static int virtual_udisk_syncts_handler(struct virtual_udisk_node_hdl *hdl, struct audio_syncts_ioc_params *params)
{
    if (!params) {
        return 0;
    }
    switch (params->cmd) {
    case AUDIO_SYNCTS_MOUNT_ON_SNDPCM:
        printf(">> Virtual Udisk Sync Mount!\n");
        virtual_udisk_mount_syncts(hdl, (void *)params->data[0], params->data[1], params->data[2]);
        break;
    case AUDIO_SYNCTS_UMOUNT_ON_SNDPCM:
        printf(">> Virtual Udisk Sync Unmount!\n");
        virtual_udisk_unmount_syncts(hdl, (void *)params->data[0]);
        break;
    }
    return 0;
}
#endif


static int virtual_udisk_adapter_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    struct virtual_udisk_node_hdl *hdl = (struct virtual_udisk_node_hdl *)iport->node->private_data;

    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        virtual_udisk_adapter_open_iport(iport);
        break;
    case NODE_IOC_NEGOTIATE:
        *(int *)arg |= virtual_udisk_ioc_negotiate(iport);
        break;
    case NODE_IOC_SET_SCENE:
        hdl->scene = arg;
        break;
    case NODE_IOC_START:
        virtual_udisk_ioc_start(hdl);
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        virtual_udisk_ioc_stop(hdl);
        break;
    case NODE_IOC_SYNCTS:
#if VIRTUAL_UDISK_MP3_SYNC
        virtual_udisk_syncts_handler(hdl, (struct audio_syncts_ioc_params *)arg);
#endif
        break;
    case NODE_IOC_SET_PARAM:
        break;

    default:
        break;
    }
    return ret;
}


static void virtual_udisk_adapter_release(struct stream_node *node)
{
    r_printf(">>>>>>>>>>>>>>>>>>>>>>>>> VIR UDISK Release!\n");
    struct virtual_udisk_node_hdl *hdl = (struct virtual_udisk_node_hdl *)node->private_data;
    spin_lock(&vir_udisk_lock);
    if (hdl->start == 0) {
        if (hdl->cache_buf) {
            free(hdl->cache_buf);
            hdl->cache_buf = NULL;
        }
        if (hdl->read_tmp_buf) {
            free(hdl->read_tmp_buf);
            hdl->read_tmp_buf = NULL;
        }
    }
    g_vir_udisk_hdl = NULL;
    spin_unlock(&vir_udisk_lock);
}

REGISTER_STREAM_NODE_ADAPTER(virtual_udisk_node_adapter) = {
    .uuid       = NODE_UUID_VIRTUAL_UDISK,
    .bind       = virtual_udisk_adapter_bind,
    .ioctl      = virtual_udisk_adapter_ioctl,
    .release    = virtual_udisk_adapter_release,
    .hdl_size   = sizeof(struct virtual_udisk_node_hdl),
};

#endif




