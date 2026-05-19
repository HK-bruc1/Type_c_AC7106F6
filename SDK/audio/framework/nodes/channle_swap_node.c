#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".channel_swap_node.data.bss")
#pragma data_seg(".channel_swap_node.data")
#pragma const_seg(".channel_swap_node.text.const")
#pragma code_seg(".channel_swap_node.text")
#endif
#include "jlstream.h"
#include "overlay_code.h"
#include "app_config.h"
#include "audio_splicing.h"

#if TCFG_CHANNEL_SWAP_NODE_ENABLE

static void channel_swap_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct stream_frame *frame;
    struct stream_node *node = iport->node;

    while (1) {
        frame = jlstream_pull_frame(iport, note);
        if (!frame) {
            break;
        }
        channel_swap_run(frame->data, frame->len, frame->bit_wide);
        if (node->oport) {
            jlstream_push_frame(node->oport, frame);
        } else {
            jlstream_free_frame(frame);
        }
    }
}

/*节点预处理-在ioctl之前*/
static int channel_swap_adapter_bind(struct stream_node *node, u16 uuid)
{
    return 0;
}

static void channel_swap_ioc_open_iport(struct stream_iport *iport)
{
}


/*节点start函数*/
static void channel_swap_ioc_start(void)
{

}

/*节点stop函数*/
static void channel_swap_ioc_stop(void)
{

}

/*节点ioctl函数*/
static int channel_swap_adapter_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    int ret = 0;
    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        channel_swap_ioc_open_iport(iport);
        break;
    case NODE_IOC_START:
        channel_swap_ioc_start();
        break;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        channel_swap_ioc_stop();
        break;
    }
    return ret;
}

/*节点用完释放函数*/
static void channel_swap_adapter_release(struct stream_node *node)
{

}

REGISTER_STREAM_NODE_ADAPTER(channel_swap_node_adapter) = {
    .uuid       = NODE_UUID_CHANNLE_SWAP,
    .bind       = channel_swap_adapter_bind,
    .ioctl      = channel_swap_adapter_ioctl,
    .release    = channel_swap_adapter_release,
    .handle_frame = channel_swap_handle_frame,
    .hdl_size   = 0,
    //固定要求输出为双声道
    .ability_channel_in = 2,
    .ability_channel_out = 2,
};
__attribute__((used))
int channel_swap_version_1_0_0(void)
{
    return 0x100;
}


#endif
