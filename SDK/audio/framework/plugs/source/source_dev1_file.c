#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".source_dev1_file.data.bss")
#pragma data_seg(".source_dev1_file.data")
#pragma const_seg(".source_dev1_file.text.const")
#pragma code_seg(".source_dev1_file.text")
#endif

#include "app_config.h"

#if TCFG_SOURCE_DEV1_NODE_ENABLE

#include "source_node.h"
#include "source_dev1_file.h"
#include "spinlock.h"

struct source_dev1_file_hdl {
    struct stream_node *node;
    u8 started;
};

static struct source_dev1_file_hdl *source_dev1_hdl;
static const struct source_dev1_provider *source_dev1_provider;
static DEFINE_SPINLOCK(source_dev1_lock);

int source_dev1_provider_register(
    const struct source_dev1_provider *provider)
{
    int ret = 0;

    if (!provider || !provider->read || !provider->frame_bytes
        || !provider->format.sample_rate
        || provider->format.coding_type == AUDIO_CODING_UNKNOW) {
        return -EINVAL;
    }

    spin_lock(&source_dev1_lock);
    if (source_dev1_provider && source_dev1_provider != provider) {
        ret = -EBUSY;
    } else {
        source_dev1_provider = provider;
    }
    spin_unlock(&source_dev1_lock);
    return ret;
}

void source_dev1_provider_unregister(
    const struct source_dev1_provider *provider)
{
    spin_lock(&source_dev1_lock);
    if (source_dev1_provider == provider
        && (!source_dev1_hdl || !source_dev1_hdl->started)) {
        source_dev1_provider = NULL;
    }
    spin_unlock(&source_dev1_lock);
}

void source_dev1_data_notify(void)
{
    struct stream_node *node = NULL;

    spin_lock(&source_dev1_lock);
    if (source_dev1_hdl && source_dev1_hdl->started) {
        node = source_dev1_hdl->node;
    }
    spin_unlock(&source_dev1_lock);

    if (node) {
        jlstream_wakeup_thread(NULL, node, NULL);
    }
}

static enum stream_node_state source_dev1_get_frame(
    void *_hdl, struct stream_frame **output)
{
    struct source_dev1_file_hdl *hdl = _hdl;
    const struct source_dev1_provider *provider;
    struct stream_frame *frame;
    int len;

    *output = NULL;
    spin_lock(&source_dev1_lock);
    provider = hdl->started ? source_dev1_provider : NULL;
    spin_unlock(&source_dev1_lock);
    if (!provider) {
        return NODE_STA_RUN | NODE_STA_SOURCE_NO_DATA;
    }

    frame = jlstream_get_frame(hdl->node->oport, provider->frame_bytes);
    if (!frame) {
        return NODE_STA_RUN | NODE_STA_SOURCE_NO_DATA;
    }
    len = provider->read(provider->priv, frame->data,
                         provider->frame_bytes);
    if (len <= 0 || len > provider->frame_bytes) {
        jlstream_free_frame(frame);
        return NODE_STA_RUN | NODE_STA_SOURCE_NO_DATA;
    }

    frame->len = len;
    *output = frame;
    return NODE_STA_RUN;
}

static void *source_dev1_init(void *priv, struct stream_node *node)
{
    struct source_dev1_file_hdl *hdl = zalloc(sizeof(*hdl));

    (void)priv;
    if (!hdl) {
        return NULL;
    }
    hdl->node = node;
    node->type |= NODE_TYPE_IRQ;

    spin_lock(&source_dev1_lock);
    source_dev1_hdl = hdl;
    spin_unlock(&source_dev1_lock);
    return hdl;
}

static int source_dev1_ioctl(void *_hdl, int cmd, int arg)
{
    struct source_dev1_file_hdl *hdl = _hdl;
    const struct source_dev1_provider *provider;

    switch (cmd) {
    case NODE_IOC_GET_FMT:
        spin_lock(&source_dev1_lock);
        provider = source_dev1_provider;
        if (provider) {
            *(struct stream_fmt *)arg = provider->format;
        }
        spin_unlock(&source_dev1_lock);
        return provider ? 0 : -EFAULT;
    case NODE_IOC_START:
        spin_lock(&source_dev1_lock);
        hdl->started = source_dev1_provider != NULL;
        spin_unlock(&source_dev1_lock);
        return hdl->started ? 0 : -EFAULT;
    case NODE_IOC_SUSPEND:
    case NODE_IOC_STOP:
        spin_lock(&source_dev1_lock);
        hdl->started = 0;
        spin_unlock(&source_dev1_lock);
        break;
    case NODE_IOC_SET_BTADDR:
    case NODE_IOC_SET_SCENE:
        break;
    default:
        break;
    }
    return 0;
}

static void source_dev1_release(void *_hdl)
{
    struct source_dev1_file_hdl *hdl = _hdl;

    spin_lock(&source_dev1_lock);
    hdl->started = 0;
    if (source_dev1_hdl == hdl) {
        source_dev1_hdl = NULL;
    }
    spin_unlock(&source_dev1_lock);
    free(hdl);
}

REGISTER_SOURCE_NODE_PLUG(source_dev1_file_plug) = {
    .uuid = NODE_UUID_SOURCE_DEV1,
    .init = source_dev1_init,
    .get_frame = source_dev1_get_frame,
    .ioctl = source_dev1_ioctl,
    .release = source_dev1_release,
};

#endif /* TCFG_SOURCE_DEV1_NODE_ENABLE */
