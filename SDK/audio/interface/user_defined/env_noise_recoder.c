
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".env_noise_recoder.data.bss")
#pragma data_seg(".env_noise_recoder.data")
#pragma const_seg(".env_noise_recoder.text.const")
#pragma code_seg(".env_noise_recoder.text")
#endif
#include "jlstream.h"
#include "env_noise_recoder.h"
#include "audio_config_def.h"

struct env_noise_recoder {
    struct jlstream *stream;
};

static struct env_noise_recoder *g_env_noise_recoder = NULL;


static void env_noise_recoder_callback(void *private_data, int event)
{
    struct env_noise_recoder *recoder = g_env_noise_recoder;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:
        break;
    }
}

int env_noise_recoder_open(void)
{
    int err;
    struct env_noise_recoder *recoder;

    if (g_env_noise_recoder) {
        return -EBUSY;
    }
    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"user_defined");
    if (uuid == 0) {
        return -EFAULT;
    }

    recoder = malloc(sizeof(*recoder));
    if (!recoder) {
        return -ENOMEM;
    }

    recoder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_ADC);
    if (!recoder->stream) {
        recoder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_PDM_MIC);
    }
    if (!recoder->stream) {
        recoder->stream = jlstream_pipeline_parse(uuid, NODE_UUID_IIS0_RX);
    }
    if (!recoder->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

    //设置ADC的中断点数
    err = jlstream_node_ioctl(recoder->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, AUDIO_ADC_IRQ_POINTS);
    if (err) {
        goto __exit1;
    }
    jlstream_set_callback(recoder->stream, recoder->stream, env_noise_recoder_callback);
    jlstream_set_scene(recoder->stream, STREAM_SCENE_ENV_NOISE);
    err = jlstream_start(recoder->stream);
    if (err) {
        goto __exit1;
    }

    g_env_noise_recoder = recoder;

    return 0;

__exit1:
    jlstream_release(recoder->stream);
__exit0:
    free(recoder);
    return err;
}

void env_noise_recoder_close()
{
    struct env_noise_recoder *recoder = g_env_noise_recoder;

    if (!recoder) {
        return;
    }
    jlstream_stop(recoder->stream, 0);
    jlstream_release(recoder->stream);

    free(recoder);
    g_env_noise_recoder = NULL;

    jlstream_event_notify(STREAM_EVENT_CLOSE_RECODER, (int)"env_noise");
}

void env_noise_recoder_switch(void)
{
    if (g_env_noise_recoder) {
        env_noise_recoder_close();
    } else {
        env_noise_recoder_open();
    }
}

