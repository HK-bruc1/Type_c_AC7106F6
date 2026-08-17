#include "app_config.h"

#if TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE

#include "system/includes.h"
#include "spinlock.h"
#include "jlstream.h"
#include "media/audio_base.h"
#include "encoder_node.h"
#include "adc_file.h"
#include "effects/effects_adj.h"
#include "rdx_protocol_defs.h"
#include "rdx_record_audio_br56.h"

#define RDX_RECORD_PIPELINE_UUID              0x5475
#define RDX_RECORD_ADC_IRQ_POINTS             320
#define RDX_RECORD_ADC_NODE_SUBID              1
#define RDX_RECORD_ADC_INDEX                   0
#define RDX_RECORD_BR56_ADC_GAIN_LEVEL_MAX      7

#if RDX_MIC_GAIN_LEVEL_MAX != 19
#error "Review the RDX-to-BR56 ADC gain mapping for the new wire range"
#endif

struct rdx_record_audio_br56_runtime {
    spinlock_t lock;
    struct jlstream *stream;
    u8 initialized;
    u8 accepting;
    u8 recorder_open;
    u32 engine_generation;
    u32 session_id;
    u32 capture_generation;
    rdx_record_audio_frame_callback_t frame_callback;
    void *frame_priv;
};

static struct rdx_record_audio_br56_runtime rdx_record_audio;

/*
 * RDX keeps the BR28-compatible 0..19 wire range while BR56 exposes the
 * eight ADC presets 0..7.  Map the complete ranges with nearest rounding so
 * both endpoints remain stable: RDX 0/19 <-> BR56 0/7.
 */
static u8 rdx_record_audio_br56_gain_to_adc(u8 gain)
{
    return ((u16)gain * RDX_RECORD_BR56_ADC_GAIN_LEVEL_MAX
            + RDX_MIC_GAIN_LEVEL_MAX / 2)
           / RDX_MIC_GAIN_LEVEL_MAX;
}

static u8 rdx_record_audio_br56_gain_from_adc(u8 adc_gain)
{
    return ((u16)adc_gain * RDX_MIC_GAIN_LEVEL_MAX
            + RDX_RECORD_BR56_ADC_GAIN_LEVEL_MAX / 2)
           / RDX_RECORD_BR56_ADC_GAIN_LEVEL_MAX;
}

static int rdx_record_audio_br56_read_adc_cfg(
    struct adc_file_cfg *adc_cfg, char *node_name)
{
    int ret;

    if (!adc_cfg) {
        return -EINVAL;
    }

    memset(adc_cfg, 0, sizeof(*adc_cfg));
    ret = jlstream_read_node_data_new(NODE_UUID_ADC,
                                      RDX_RECORD_ADC_NODE_SUBID,
                                      adc_cfg, node_name);
    if (ret != sizeof(*adc_cfg)) {
        printf("[RDX][AUDIO] adc_config_read result=failed subid=%u ret=%d expected=%u\n",
               RDX_RECORD_ADC_NODE_SUBID, ret,
               (unsigned int)sizeof(*adc_cfg));
        return -EFAULT;
    }
    if (adc_cfg->mic_en_map != BIT(RDX_RECORD_ADC_INDEX)) {
        printf("[RDX][AUDIO] adc_config_read result=failed reason=topology subid=%u mic_en_map=0x%x expected=0x%x\n",
               RDX_RECORD_ADC_NODE_SUBID,
               (unsigned int)adc_cfg->mic_en_map,
               (unsigned int)BIT(RDX_RECORD_ADC_INDEX));
        return -EINVAL;
    }
    if (adc_cfg->param[RDX_RECORD_ADC_INDEX].mic_gain
        > RDX_RECORD_BR56_ADC_GAIN_LEVEL_MAX) {
        printf("[RDX][AUDIO] adc_config_read result=failed reason=gain_range subid=%u adc_gain=%u max=%u\n",
               RDX_RECORD_ADC_NODE_SUBID,
               adc_cfg->param[RDX_RECORD_ADC_INDEX].mic_gain,
               RDX_RECORD_BR56_ADC_GAIN_LEVEL_MAX);
        return -ERANGE;
    }
    return 0;
}

int rdx_record_audio_br56_get_factory_gain(u8 *gain)
{
    struct adc_file_cfg adc_cfg;
    char node_name[16] = {0};
    int ret;

    if (!gain) {
        return -EINVAL;
    }

    ret = rdx_record_audio_br56_read_adc_cfg(&adc_cfg, node_name);
    if (ret) {
        return ret;
    }
    *gain = rdx_record_audio_br56_gain_from_adc(
        adc_cfg.param[RDX_RECORD_ADC_INDEX].mic_gain);
    printf("[RDX][AUDIO] factory_gain result=ok subid=%u node=%s adc=%u adc_gain=%u rdx_gain=%u pre_gain=%u\n",
           RDX_RECORD_ADC_NODE_SUBID, node_name,
           RDX_RECORD_ADC_INDEX,
           adc_cfg.param[RDX_RECORD_ADC_INDEX].mic_gain, *gain,
           adc_cfg.param[RDX_RECORD_ADC_INDEX].mic_pre_gain);
    return 0;
}

static void rdx_record_audio_br56_init(void)
{
    if (!rdx_record_audio.initialized) {
        spin_lock_init(&rdx_record_audio.lock);
        rdx_record_audio.initialized = 1;
    }
}

static void rdx_record_audio_br56_clear_locked(void)
{
    rdx_record_audio.accepting = 0;
    rdx_record_audio.recorder_open = 0;
    rdx_record_audio.stream = NULL;
    rdx_record_audio.engine_generation = 0;
    rdx_record_audio.session_id = 0;
    rdx_record_audio.capture_generation = 0;
    rdx_record_audio.frame_callback = NULL;
    rdx_record_audio.frame_priv = NULL;
}

static int rdx_record_audio_br56_prepare_mic_gain(
    struct jlstream *stream,
    const struct adc_file_cfg *adc_cfg,
    const struct rdx_record_audio_open_params *params,
    const char *node_name)
{
    int ret = jlstream_node_ioctl(stream, NODE_UUID_SOURCE,
                                  NODE_IOC_SET_PARAM, (int)adc_cfg);
    if (ret == true) {
        printf("[RDX][AUDIO] adc_gain_prepare result=ok session=%u capture=%u subid=%u node=%s adc=%u rdx_gain=%u adc_gain=%u timing=before_start\n",
               (unsigned int)params->session_id,
               (unsigned int)params->capture_generation,
               RDX_RECORD_ADC_NODE_SUBID, node_name,
               RDX_RECORD_ADC_INDEX, params->mic_gain,
               adc_cfg->param[RDX_RECORD_ADC_INDEX].mic_gain);
        return 0;
    }

    printf("[RDX][AUDIO] adc_gain_prepare result=failed session=%u capture=%u subid=%u adc=%u rdx_gain=%u adc_gain=%u ret=%d\n",
           (unsigned int)params->session_id,
           (unsigned int)params->capture_generation,
           RDX_RECORD_ADC_NODE_SUBID, RDX_RECORD_ADC_INDEX,
           params->mic_gain,
           adc_cfg->param[RDX_RECORD_ADC_INDEX].mic_gain, ret);
    return -EFAULT;
}

static int rdx_record_audio_br56_ai_tx(u8 *data, u32 len)
{
    rdx_record_audio_frame_callback_t callback;
    void *priv;
    u32 engine_generation;
    u32 session_id;
    u32 capture_generation;

    spin_lock(&rdx_record_audio.lock);
    if (!rdx_record_audio.accepting || !rdx_record_audio.frame_callback) {
        spin_unlock(&rdx_record_audio.lock);
        return 0;
    }
    callback = rdx_record_audio.frame_callback;
    priv = rdx_record_audio.frame_priv;
    engine_generation = rdx_record_audio.engine_generation;
    session_id = rdx_record_audio.session_id;
    capture_generation = rdx_record_audio.capture_generation;
    spin_unlock(&rdx_record_audio.lock);

    return callback(priv, engine_generation, session_id,
                    capture_generation, data, len);
}

int rdx_record_audio_br56_open(
    const struct rdx_record_audio_open_params *params)
{
    struct adc_file_cfg adc_cfg;
    char adc_node_name[16] = {0};
    struct stream_enc_fmt stream_fmt = {
        .channel = RDX_RECORD_MEETING_V1_CHANNELS,
        .bit_width = DATA_BIT_WIDE_16BIT,
        .frame_dms = RDX_RECORD_MEETING_V1_FRAME_MS * 10,
        .sample_rate = RDX_RECORD_MEETING_V1_SAMPLE_RATE,
        .bit_rate = RDX_RECORD_MEETING_V1_BIT_RATE,
        .coding_type = AUDIO_CODING_OPUS,
    };
    struct encoder_fmt encoder_fmt = {
        .complexity = 0,
        .ch_num = RDX_RECORD_MEETING_V1_CHANNELS,
        .format = 0,
        .bit_width = DATA_BIT_WIDE_16BIT,
        .frame_dms = RDX_RECORD_MEETING_V1_FRAME_MS * 10,
        .bit_rate = RDX_RECORD_MEETING_V1_BIT_RATE,
        .sample_rate = RDX_RECORD_MEETING_V1_SAMPLE_RATE,
    };
    struct stream_fmt ai_tx_fmt = {
        .bit_wide = DATA_BIT_WIDE_16BIT,
        .channel_mode = AUDIO_CH_MIX,
        .frame_dms = RDX_RECORD_MEETING_V1_FRAME_MS * 10,
        .bit_rate = RDX_RECORD_MEETING_V1_BIT_RATE,
        .sample_rate = RDX_RECORD_MEETING_V1_SAMPLE_RATE,
        .coding_type = AUDIO_CODING_OPUS,
    };
    struct jlstream *stream;
    int ret;

    if (!params || !params->frame_callback
        || params->format_id != RDX_RECORD_FORMAT_MEETING_V1
        || params->mic_gain_override_valid > 1
        || (params->mic_gain_override_valid
            && params->mic_gain > RDX_MIC_GAIN_LEVEL_MAX)
        || !params->engine_generation || !params->session_id
        || !params->capture_generation) {
        return -EINVAL;
    }

    if (params->mic_gain_override_valid) {
        ret = rdx_record_audio_br56_read_adc_cfg(&adc_cfg, adc_node_name);
        if (ret) {
            return ret;
        }
        adc_cfg.param[RDX_RECORD_ADC_INDEX].mic_gain =
            rdx_record_audio_br56_gain_to_adc(params->mic_gain);
    }

    rdx_record_audio_br56_init();
    spin_lock(&rdx_record_audio.lock);
    if (rdx_record_audio.accepting || rdx_record_audio.recorder_open) {
        spin_unlock(&rdx_record_audio.lock);
        return -EBUSY;
    }
    rdx_record_audio.recorder_open = 1;
    rdx_record_audio.engine_generation = params->engine_generation;
    rdx_record_audio.session_id = params->session_id;
    rdx_record_audio.capture_generation = params->capture_generation;
    rdx_record_audio.frame_callback = params->frame_callback;
    rdx_record_audio.frame_priv = params->frame_priv;
    spin_unlock(&rdx_record_audio.lock);

    stream = jlstream_pipeline_parse(RDX_RECORD_PIPELINE_UUID,
                                     NODE_UUID_ADC);
    if (!stream) {
        ret = -EFAULT;
        goto open_failed;
    }

    if (params->mic_gain_override_valid) {
        ret = rdx_record_audio_br56_prepare_mic_gain(
            stream, &adc_cfg, params, adc_node_name);
        if (ret) {
            goto release_stream;
        }
    } else {
        printf("[RDX][AUDIO] adc_gain_prepare result=skip session=%u capture=%u source=factory\n",
               (unsigned int)params->session_id,
               (unsigned int)params->capture_generation);
    }

    ret = jlstream_ioctl(stream, NODE_IOC_SET_ENC_FMT, (int)&stream_fmt);
    if (ret) {
        goto release_stream;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_ENCODER,
                              NODE_IOC_SET_PRIV_FMT, (int)&encoder_fmt);
    if (ret) {
        goto release_stream;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_AI_TX,
                              NODE_IOC_SET_FMT, (int)&ai_tx_fmt);
    if (ret) {
        goto release_stream;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_AI_TX,
                              NODE_IOC_SET_PRIV_FMT,
                              (int)rdx_record_audio_br56_ai_tx);
    if (ret) {
        goto release_stream;
    }
    ret = jlstream_node_ioctl(stream, NODE_UUID_SOURCE,
                              NODE_IOC_SET_PRIV_FMT,
                              RDX_RECORD_ADC_IRQ_POINTS);
    if (ret) {
        goto release_stream;
    }

    jlstream_set_scene(stream, STREAM_SCENE_AI_VOICE);
    ret = jlstream_start(stream);
    if (ret) {
        goto release_stream;
    }

    spin_lock(&rdx_record_audio.lock);
    rdx_record_audio.stream = stream;
    rdx_record_audio.accepting = 1;
    spin_unlock(&rdx_record_audio.lock);
    return 0;

release_stream:
    jlstream_release(stream);
open_failed:
    spin_lock(&rdx_record_audio.lock);
    rdx_record_audio_br56_clear_locked();
    spin_unlock(&rdx_record_audio.lock);
    return ret;
}

void rdx_record_audio_br56_close(void)
{
    struct jlstream *stream;

    rdx_record_audio_br56_init();
    spin_lock(&rdx_record_audio.lock);
    stream = rdx_record_audio.stream;
    rdx_record_audio.accepting = 0;
    rdx_record_audio.stream = NULL;
    spin_unlock(&rdx_record_audio.lock);

    if (stream) {
        jlstream_stop(stream, 0);
        jlstream_release(stream);
    }

    spin_lock(&rdx_record_audio.lock);
    rdx_record_audio_br56_clear_locked();
    spin_unlock(&rdx_record_audio.lock);
}

#endif /* TCFG_RDX_ENABLE && RDX_CFG_CONFERENCE_RECORDING_ENABLE */
