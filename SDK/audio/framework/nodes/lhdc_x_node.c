#include "jlstream.h"
#include "media/audio_base.h"
#include "effects/effects_adj.h"
#include "lhdc_x_node.h"
#include "sdk_config.h"
#include "jlstream.h"

#define LOG_TAG_CONST EFFECTS
#define LOG_TAG     "[LHDC_X-NODE]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"

#if (defined(TCFG_LHDC_X_NODE_ENABLE) && TCFG_LHDC_X_NODE_ENABLE)

#ifdef SUPPORT_MS_EXTENSIONS
#pragma   code_seg(".lhdc_x.text")
#pragma   data_seg(".lhdc_x.data")
#pragma  const_seg(".lhdc_x.text.const")
#endif

struct lhdc_x_node_hdl {
    struct stream_iport *iport;
    struct jlstream_fade fade;
    void *lhdc_x_buf;   //算法buf
    u8 is_bypass;       //bypass控制
    u8 mute_en;
    u8 dump_frame_num;  //算法打开有po声，启动时mute的帧数
};

static void lhdc_x_open(struct lhdc_x_node_hdl *hdl)
{
    if (!hdl->lhdc_x_buf) {
        struct stream_fmt *fmt = &hdl_node(hdl)->oport->fmt;
        int size = 0;
        int ret = lhdcx_util_get_mem_size(&size);
        /* printf("lhdcx_util_get_mem_size, ret %d, size %d\n", ret, size); */
        hdl->lhdc_x_buf = media_malloc(AUD_MODULE_LHDC_X, size);
        ASSERT(hdl->lhdc_x_buf);
        lhdcx_util_init(hdl->lhdc_x_buf, fmt->sample_rate, fmt->bit_wide); //only support 44.1k 16bit
        lhdcx_util_set_out_channel(hdl->lhdc_x_buf, LHDCX_OUT_STEREO);
        jlstream_fade_init(&hdl->fade, 1, fmt->sample_rate, AUDIO_CH_NUM(fmt->channel_mode), 50);
        hdl->mute_en = 1;
        hdl->dump_frame_num = 2;
    }
}

static void lhdc_x_close(struct lhdc_x_node_hdl *hdl)
{
    if (hdl->lhdc_x_buf) {
        media_free(hdl->lhdc_x_buf);
        hdl->lhdc_x_buf = NULL;
    }
}

static void lhdc_x_run(struct lhdc_x_node_hdl *hdl, s16 *in, s16 *out, u16 points)
{
    if (hdl->lhdc_x_buf) {
        lhdcx_util_process(hdl->lhdc_x_buf, in, points, out);
    }
}
/*节点输出回调处理，可处理数据或post信号量*/
__NODE_CACHE_CODE(lhdc_x)
static void lhdc_x_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct lhdc_x_node_hdl *hdl = (struct lhdc_x_node_hdl *)iport->node->private_data;
    struct stream_node *node = iport->node;
    struct stream_oport *oport = node->oport;
    struct stream_frame *oport_frame = NULL;

    struct stream_frame *frame = jlstream_pull_frame(iport, note);;
    if (!frame) {
        return;
    }
    if (hdl->is_bypass) {
        lhdc_x_close(hdl);
    } else {
        lhdc_x_open(hdl);
        lhdc_x_run(hdl, (s16 *)frame->data, (s16 *)frame->data, frame->len / 4);
        if (hdl->mute_en) {
            hdl->dump_frame_num--;
            if (!hdl->dump_frame_num) {
                hdl->mute_en = 0;
            }
            memset(frame->data, 0x00, frame->len);
        } else {
            jlstream_fade_data(&hdl->fade, frame->data, frame->len);
        }
    }

    jlstream_push_frame(node->oport, frame);	//将数据推到oport
}

/*节点预处理-在ioctl之前*/
static int lhdc_x_adapter_bind(struct stream_node *node, u16 uuid)
{
    return 0;
}

/*打开改节点输入接口*/
static void lhdc_x_ioc_open_iport(struct stream_iport *iport)
{
}


/*节点start函数*/
static void lhdc_x_ioc_start(struct lhdc_x_node_hdl *hdl)
{
    struct stream_fmt *fmt = &hdl_node(hdl)->oport->fmt;
    if (fmt->bit_wide || (AUDIO_CH_NUM(fmt->channel_mode) != 2) || (fmt->sample_rate != 44100)) {
        //格式检查，仅支持16bit、2ch、44.1k
        printf("lhdc-x err: bit_width %d, ch %d, sr %d\n", fmt->bit_wide, AUDIO_CH_NUM(fmt->channel_mode), fmt->sample_rate);
        ASSERT(0);
    }
    struct lhdc_x_param_tool_set cfg = {0};
    /*
     *获取配置文件内的参数,及名字
     * */
    char name[16];
    int len = jlstream_read_node_data_new(hdl_node(hdl)->uuid, hdl_node(hdl)->subid, (void *)&cfg, name);
    if (!len) {
        log_error("%s, read node data err\n", __FUNCTION__);
        hdl->is_bypass = 1;
        return;
    }

    /*
     *获取在线调试的临时参数
     * */
    if (config_audio_cfg_online_enable) {
        if (jlstream_read_effects_online_param(hdl_node(hdl)->uuid, name, &cfg, sizeof(cfg))) {
            log_debug("get echo online param\n");
        }
    }
    hdl->is_bypass = cfg.is_bypass;
    hdl->lhdc_x_buf = NULL;
    hdl->mute_en = 0;
    hdl->dump_frame_num = 0;
    if (!hdl->is_bypass) {
        lhdc_x_open(hdl);
    }
}

/*节点stop函数*/
static void lhdc_x_ioc_stop(struct lhdc_x_node_hdl *hdl)
{
    lhdc_x_close(hdl);
}

static int lhdc_x_ioc_update_parm(struct lhdc_x_node_hdl *hdl, int parm)
{
    if (!hdl) {
        return false;
    }
    struct lhdc_x_param_tool_set *cfg = (struct lhdc_x_param_tool_set *)parm;
    hdl->is_bypass = cfg->is_bypass;
    return true;
}


/*节点ioctl函数*/
static int lhdc_x_adapter_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    struct lhdc_x_node_hdl *hdl = (struct lhdc_x_node_hdl *)iport->node->private_data;
    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        lhdc_x_ioc_open_iport(iport);
        break;
    case NODE_IOC_OPEN_OPORT:
        break;
    case NODE_IOC_CLOSE_IPORT:
        break;
    case NODE_IOC_START:
        hdl->iport = iport;
        lhdc_x_ioc_start(hdl);
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        lhdc_x_ioc_stop(hdl);
        break;
    case NODE_IOC_SET_PARAM:
        ret = lhdc_x_ioc_update_parm(hdl, arg);
        break;
    }

    return ret;
}

/*节点用完释放函数*/
static void lhdc_x_adapter_release(struct stream_node *node)
{

}

/*节点adapter 注意需要在sdk_used_list声明，否则会被优化*/
REGISTER_STREAM_NODE_ADAPTER(lhdc_x_node_adapter) = {
    .uuid       = NODE_UUID_LHDC_X,
    .bind       = lhdc_x_adapter_bind,
    .ioctl      = lhdc_x_adapter_ioctl,
    .release    = lhdc_x_adapter_release,
    .handle_frame = lhdc_x_handle_frame,				//注册输出回调
    .hdl_size   = sizeof(struct lhdc_x_node_hdl),
    .ability_channel_in = 2,
    .ability_channel_out = 2,
};

REGISTER_ONLINE_ADJUST_TARGET(lhdc_x) = {
    .uuid = NODE_UUID_LHDC_X,
};

#endif
