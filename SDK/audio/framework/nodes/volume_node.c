#include "jlstream.h"
#include "classic/tws_api.h"
#include "media/audio_base.h"
#include "app_main.h"
#include "audio_config.h"
#include "audio_dvol.h"
#include "effects/effects_adj.h"
#include "volume_node.h"
#include "scene_switch.h"
#include "app_config.h"

#define SYS_LOG_ENABLE          0
#if SYS_LOG_ENABLE
#define LOG_TAG     "[VOL-NODE]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"
#else
#define log_info(fmt, ...)
#define log_error(fmt, ...)
#define log_debug(fmt, ...)
#endif


#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".volume_node.data.bss")
#pragma data_seg(".volume_node.data")
#pragma const_seg(".volume_node.text.const")
#pragma code_seg(".volume_node.text")
#endif

struct volume_hdl {
    enum stream_scene scene;
    u8 state;
    u8 online_update_disable;       //是否支持在线音量更新
    u8 read_cfg;
#if VOL_NOISE_OPTIMIZE
    u16 target_dig_volume;          //优化底噪时的固定数字音量
#endif
    u16 *vol_table;
    dvol_handle *dvol_hdl;          //音量操作句柄
    struct volume_cfg vol_cfg;
#if defined(ANA_VOL_DYNAMIC_SWITCH) && ANA_VOL_DYNAMIC_SWITCH
    u16 avol_set_lvl;               //该音量档位开始切换模拟
#endif
};

__NODE_CACHE_CODE(volume)
static void volume_handle_frame(struct stream_iport *iport, struct stream_note *note)
{
    struct stream_frame *frame;
    struct stream_node *node = iport->node;
    struct volume_hdl *hdl = (struct volume_hdl *)node->private_data;

    frame = jlstream_pull_frame(iport, note);
    if (!frame) {
        return;
    }
    if (!hdl->vol_cfg.bypass) {
        audio_digital_vol_run(hdl->dvol_hdl, (s16 *)frame->data, frame->len);
    }
    jlstream_push_frame(node->oport, frame);
}

static int volume_bind(struct stream_node *node, u16 uuid)
{
    struct volume_hdl *hdl = (struct volume_hdl *)node->private_data;

    hdl->vol_cfg.cfg_level_max = 16;
    hdl->vol_cfg.cfg_vol_min = -45;
    hdl->vol_cfg.cur_vol = 10;

    return 0;
}

static void volume_open_iport(struct stream_iport *iport)
{
}


__VOLUME_BANK_CODE
static void volume_ioc_start(struct volume_hdl *hdl)
{
    struct audio_vol_params params = {0};
    struct volume_cfg *vol_cfg;
    struct cfg_info info = {0};

    /*
     * 获取配置文件内的参数,及名字
     */
    char name[16];                  //节点名称
    int ret = jlstream_read_node_info_data(hdl_node(hdl)->uuid, hdl_node(hdl)->subid,
                                           name, &info);
    if (hdl->read_cfg == 0) {
        hdl->read_cfg = 1;
        if (ret) {
            vol_cfg = zalloc(info.size);
            if (vol_cfg) {
                jlstream_read_form_cfg_data(&info, (void *)vol_cfg);
                memcpy(&hdl->vol_cfg, vol_cfg, sizeof(struct volume_cfg));
                if (info.size > sizeof(struct volume_cfg)) {
                    //有自定义音量表,dB转成对应音量
                    hdl->vol_table = malloc(vol_cfg->cfg_level_max * sizeof(u16));
                    audio_digital_vol_db_2_gain(vol_cfg->vol_table, vol_cfg->cfg_level_max,
                                                hdl->vol_table);
                }
                free(vol_cfg);
            }
        }
    }
    vol_cfg = &hdl->vol_cfg;
    log_debug("volume node config: %d,%d,%d,%d,%d\n",
              vol_cfg->cfg_level_max, (int)vol_cfg->cfg_vol_min,
              (int)vol_cfg->cfg_vol_max, vol_cfg->vol_table_custom, vol_cfg->bypass);

    /*
     * 获取在线调试的临时参数
     */
    if (config_audio_cfg_online_enable) {
        u32 online_cfg_len = sizeof(struct volume_cfg) + DIGITAL_VOLUME_LEVEL_MAX * sizeof(float);
        struct volume_cfg *online_vol_cfg = zalloc(online_cfg_len);
        if (jlstream_read_effects_online_param(hdl_node(hdl)->uuid, name,
                                               online_vol_cfg, online_cfg_len)) {
            /* printf("cfg_level_max = %d\n", online_vol_cfg->cfg_level_max); */
            /* printf("cfg_vol_min = %d\n", online_vol_cfg->cfg_vol_min); */
            /* printf("cfg_vol_max = %d\n", online_vol_cfg->cfg_vol_max); */
            /* printf("vol_table_custom = %d\n", online_vol_cfg->vol_table_custom); */
            /* printf("cur_vol = %d\n", online_vol_cfg->cur_vol); */
            /* printf("tab_len = %d\n", online_vol_cfg->tab_len); */
            vol_cfg->cfg_vol_max    = online_vol_cfg->cfg_vol_max;
            vol_cfg->cfg_vol_min    = online_vol_cfg->cfg_vol_min;
            vol_cfg->bypass         = online_vol_cfg->bypass;
#if VOL_TAB_CUSTOM_EN
            if (vol_cfg->tab_len == online_vol_cfg->tab_len && vol_cfg->tab_len) {
                audio_digital_vol_db_2_gain(online_vol_cfg->vol_table, hdl->vol_cfg.cfg_level_max,
                                            hdl->vol_table);
            }
#endif
        }
        free(online_vol_cfg);
    }

#if defined(ANA_VOL_DYNAMIC_SWITCH) && ANA_VOL_DYNAMIC_SWITCH
    hdl->avol_set_lvl = audio_ana_vol_dynamic_switch_init(hdl->vol_table, hdl->vol_cfg.cfg_level_max);
#endif

    switch (hdl->scene) {
#if TCFG_TONE_NODE_ENABLE
    case STREAM_SCENE_TONE:
        hdl->state          = APP_AUDIO_STATE_WTONE;
        params.fade_step    = TONE_DVOL_FS;
#if TONE_BGM_FADEOUT
        audio_digital_vol_bg_fade(1);
#endif
        break;
#endif
#if TCFG_RING_TONE_NODE_ENABLE
    case STREAM_SCENE_RING:
        hdl->state          = APP_AUDIO_STATE_RING;
        params.fade_step    = TONE_DVOL_FS;
#if TONE_BGM_FADEOUT
        audio_digital_vol_bg_fade(1);
#endif
        break;
#endif
#if TCFG_KEY_TONE_NODE_ENABLE
    case STREAM_SCENE_KEY_TONE:
        hdl->state          = APP_AUDIO_STATE_KTONE;
        params.fade_step    = TONE_DVOL_FS;
#if TONE_BGM_FADEOUT
        audio_digital_vol_bg_fade(1);
#endif
        break;
#endif
#if TCFG_AI_PLAYER_ENABLE
    case STREAM_SCENE_AI_VOICE:
        hdl->state = APP_AUDIO_STATE_MUSIC;
        params.fade_step  = MUSIC_DVOL_FS;
        break;
#endif
    case STREAM_SCENE_LEA_CALL:
    case STREAM_SCENE_ESCO:
        hdl->state          = APP_AUDIO_STATE_CALL;
        params.fade_step    = CALL_DVOL_FS;
        break;
#if TCFG_AUDIO_HEARING_AID_ENABLE
    case STREAM_SCENE_HEARING_AID:
        vol_cfg->cfg_level_max = app_audio_get_max_volume();
        /* puts("STREAM_SCENE_HEARING_AID , max_vol:%d\n", vol_cfg->cfg_level_max); */
        hdl->state          = APP_AUDIO_STATE_MUSIC;
        params.fade_step    = MUSIC_DVOL_FS;
        break;
#endif
    default:
        hdl->state          = APP_AUDIO_STATE_MUSIC;
        params.fade_step    = MUSIC_DVOL_FS;
        break;
    }

    params.vol        = app_audio_get_volume(hdl->state);
#if defined(ANA_VOL_DYNAMIC_SWITCH) && ANA_VOL_DYNAMIC_SWITCH
    audio_ana_vol_dynamic_switch_set(params.vol, hdl->avol_set_lvl);
#endif
    params.vol_max    = vol_cfg->cfg_level_max;
    params.min_db     = vol_cfg->cfg_vol_min;
    params.max_db     = vol_cfg->cfg_vol_max;
    params.bit_wide   = hdl_node(hdl)->oport->fmt.bit_wide;
    params.vol_table  = hdl->vol_table;
    if (!hdl->dvol_hdl) {
        hdl->dvol_hdl =  audio_digital_vol_open(&params);
    }
    if (hdl->dvol_hdl) {
        int is_mute = app_audio_get_mute_state(hdl->state);
        if (is_mute) {
            audio_digital_vol_set_mute(hdl->dvol_hdl, 1);
        }
    }

    if ((hdl->scene == STREAM_SCENE_MIC_EFFECT) ||
        (hdl->scene == STREAM_SCENE_MIC_EFFECT2)) {
        return;
    }

#ifdef DVOL_2P1_CH_DVOL_ADJUST_NODE
#if (DVOL_2P1_CH_DVOL_ADJUST_NODE == DVOL_2P1_CH_DVOL_ADJUST_LR)
    char *substr = strstr(name, "Music");
    if (!strcmp(substr, "Music") || !strcmp(substr, "Music2")) { //找到默认初始化为最大音量的节点
        //printf("enter volume_node.c %d,%p,%s\n", __LINE__, hdl->dvol_hdl, substr);
        audio_digital_vol_set(hdl->dvol_hdl, vol_cfg->cfg_level_max);
        audio_digital_vol_set_mute(hdl->dvol_hdl, 0);
        hdl->online_update_disable = 1;
    } else {
        //printf("enter volume_node.c %d,%p,%s\n", __LINE__, hdl->dvol_hdl, substr);
        app_audio_state_switch(hdl->state, vol_cfg->cfg_level_max, hdl->dvol_hdl);
    }
#elif (DVOL_2P1_CH_DVOL_ADJUST_NODE == DVOL_2P1_CH_DVOL_ADJUST_SW)
    char *substr = strstr(name, "Music");
    if (!strcmp(substr, "Music") || !strcmp(substr, "Music1")) { //找到默认初始化为最大音量的节点
        // printf("enter volume_node.c %d,%p,%s\n", __LINE__, hdl->dvol_hdl, substr);
        audio_digital_vol_set(hdl->dvol_hdl, vol_cfg->cfg_level_max);
        audio_digital_vol_set_mute(hdl->dvol_hdl, 0);
        hdl->online_update_disable = 1;
    } else {
        // printf("enter volume_node.c %d,%p,%s\n", __LINE__, hdl->dvol_hdl, substr);
        app_audio_state_switch(hdl->state, vol_cfg->cfg_level_max, hdl->dvol_hdl);
    }
#else
    if (memchr(name, '1', 16) || memchr(name, '2', 16)) { //找到默认初始化为最大音量的节点
        //printf("enter volume_node.c %d,%p\n", __LINE__, hdl->dvol_hdl);
        audio_digital_vol_set(hdl->dvol_hdl, vol_cfg->cfg_level_max);
        audio_digital_vol_set_mute(hdl->dvol_hdl, 0);
        hdl->online_update_disable = 1;
    } else {
        //printf("enter volume_node.c %d,%p\n", __LINE__, hdl->dvol_hdl);
        app_audio_state_switch(hdl->state, vol_cfg->cfg_level_max, hdl->dvol_hdl);
    }
#endif
#else
    app_audio_state_switch(hdl->state, vol_cfg->cfg_level_max, hdl->dvol_hdl);
#endif
}

__VOLUME_BANK_CODE
static void volume_ioc_stop(struct volume_hdl *hdl)
{
    if ((hdl->scene != STREAM_SCENE_MIC_EFFECT) && (hdl->scene != STREAM_SCENE_MIC_EFFECT2)) {
        app_audio_state_exit(hdl->state);
    }
#if TONE_BGM_FADEOUT
    log_debug("tone_player.c %d tone play,BGM fade_out close\n", __LINE__);
    audio_digital_vol_bg_fade(0);
#endif
    audio_digital_vol_close(hdl->dvol_hdl);
    hdl->dvol_hdl = NULL;
}

__STREAM_BANK_CODE
int volume_ioc_get_cfg(const char *name, struct volume_cfg *vol_cfg)
{
    struct cfg_info info;
    char mode_index = get_current_scene();

    int ret = jlstream_read_info_data(mode_index, name, 0, &info);
    if (ret) {
        struct volume_cfg *cfg =  zalloc(info.size);
        int len = jlstream_read_form_cfg_data(&info, (void *)cfg);
        if (len == info.size) {
            memcpy(vol_cfg, cfg, sizeof(*cfg));
            free(cfg);
            return 0;
        }
        free(cfg);
    }
    memset(vol_cfg, 0, sizeof(*vol_cfg));
    vol_cfg->cfg_level_max = 16;
    vol_cfg->cfg_vol_min = -45;
    vol_cfg->cur_vol = 10;
    return -EFAULT;
}

u16 volume_ioc_get_max_level(const char *name)
{
    struct volume_cfg vol_cfg;
    volume_ioc_get_cfg(name, &vol_cfg);
    return vol_cfg.cfg_level_max;
}


float volume_ioc_2_dB(struct volume_hdl *hdl, s16 volume)
{
    if (hdl->dvol_hdl) {
        return audio_digital_vol_2_dB(hdl->dvol_hdl, volume);
    }
    return 0;
}

static int volume_ioc_update_parm(struct volume_hdl *hdl, int parm)
{
    struct volume_cfg *vol_cfg = (struct volume_cfg *)parm;
    int ret = false;
    int cmd = (vol_cfg->bypass & 0xf0);
    int value = vol_cfg->cur_vol;

    switch (cmd) {
    case VOLUME_NODE_CMD_SET_VOL:
        s16 volume = value & 0xffff;
        if (volume < 0) {
            volume = 0;
        }
        if (hdl && hdl->dvol_hdl) {
#if defined(VOL_NOISE_OPTIMIZE) &&( VOL_NOISE_OPTIMIZE)
            if (volume) {
                if (volume_ioc_2_dB(hdl, volume) < TARGET_DIG_DB && hdl->state == APP_AUDIO_STATE_MUSIC) {
                    if (!hdl->target_dig_volume) {
                        s16 temp_volume = 0;
                        for (temp_volume = hdl->dvol_hdl->vol_max; volume_ioc_2_dB(hdl, temp_volume) > TARGET_DIG_DB; temp_volume--) {};
                        hdl->target_dig_volume = temp_volume + 1;
                    }

                    //printf("enter volume_node.c %d,%d/100,%d/100,%d,%d,%d\n",__LINE__,(int)(volume_ioc_2_dB(hdl,volume)  * 100),(int)(volume_ioc_2_dB(hdl,hdl->target_dig_volume)* 100), hdl->target_dig_volume,volume,hdl->dvol_hdl->vol);

                    float dac_dB =  volume_ioc_2_dB(hdl, volume) - volume_ioc_2_dB(hdl, hdl->target_dig_volume)  ;

                    app_audio_dac_set_dB(dac_dB);
                    volume = hdl->target_dig_volume;
                } else { //把DAC 设回0dB
                    app_audio_dac_set_dB(0);
                }
            }
#endif
#if defined(ANA_VOL_DYNAMIC_SWITCH) && ANA_VOL_DYNAMIC_SWITCH
            audio_ana_vol_dynamic_switch_set(volume, hdl->avol_set_lvl);
#endif
            audio_digital_vol_set(hdl->dvol_hdl, volume);
            log_debug("SET VOL volume update success : %d\n", volume);
            ret = true;
        }
        break;
    case VOLUME_NODE_CMD_SET_MUTE:
        s16 mute_en = value & 0xffff;
        if (hdl && hdl->dvol_hdl) {
            audio_digital_vol_set_mute(hdl->dvol_hdl, mute_en);
            log_debug("SET MUTE mute update success : %d\n", mute_en);
            ret = true;
        }
        break;
    case VOLUME_NODE_CMD_SET_OFFSET:
        float offset_dB = (float)(value / 100.f);
        if (hdl && hdl->dvol_hdl) {
            audio_digital_vol_offset_dB_set(hdl->dvol_hdl, offset_dB);
            log_debug("SET vol offset update success : %d / 100\n", (int)(offset_dB * 100));
            ret = true;
        }
        break;
    default:
        if (!hdl->dvol_hdl || hdl->online_update_disable) {
            break;
        }
        hdl->vol_cfg.bypass     = vol_cfg->bypass;
        hdl->dvol_hdl->max_db   = vol_cfg->cfg_vol_max;
        hdl->dvol_hdl->min_db   = vol_cfg->cfg_vol_min;
#if VOL_TAB_CUSTOM_EN
        if (hdl->vol_cfg.tab_len == vol_cfg->tab_len && hdl->vol_cfg.tab_len) {
            audio_digital_vol_db_2_gain(vol_cfg->vol_table, hdl->vol_cfg.cfg_level_max,
                                        hdl->vol_table);
        }
#endif
#if defined(ANA_VOL_DYNAMIC_SWITCH) && ANA_VOL_DYNAMIC_SWITCH
        audio_ana_vol_dynamic_switch_set(vol_cfg->cur_vol, hdl->avol_set_lvl);
#endif
        audio_digital_vol_set(hdl->dvol_hdl, vol_cfg->cur_vol);
        if ((hdl->scene != STREAM_SCENE_MIC_EFFECT) &&
            (hdl->scene != STREAM_SCENE_MIC_EFFECT2)) {
            //混响在线调试音量不更新音量状态的值
            app_audio_change_volume(hdl->state, vol_cfg->cur_vol);
        }
        /* printf("cfg_level_max = %d\n", vol_cfg->cfg_level_max); */
        /* printf("cfg_vol_min = %d\n", vol_cfg->cfg_vol_min); */
        /* printf("cfg_vol_max = %d\n", vol_cfg->cfg_vol_max); */
        /* printf("vol_table_custom = %d\n", vol_cfg->vol_table_custom); */
        /* printf("cur_vol = %d\n", vol_cfg->cur_vol); */
        log_debug("volume update success : %d\n", vol_cfg->cur_vol);
        ret = true;
        break;
    }
    return ret;
}

static int get_volume_ioc_get_parm(struct volume_hdl *hdl, int parm)
{
    int ret = 0;
    struct volume_cfg *cfg = (struct volume_cfg *)parm;
    if (hdl && hdl->dvol_hdl) {
        hdl->vol_cfg.cur_vol = hdl->dvol_hdl->vol;
    }
    memcpy(cfg, &hdl->vol_cfg, sizeof(struct volume_cfg));
    ret = sizeof(struct volume_cfg);
    return ret;
}

static int volume_ioctl(struct stream_iport *iport, int cmd, int arg)
{
    struct volume_hdl *hdl = (struct volume_hdl *)iport->node->private_data;

    int ret = 0;
    switch (cmd) {
    case NODE_IOC_OPEN_IPORT:
        volume_open_iport(iport);
        break;
    case NODE_IOC_SET_SCENE:
        hdl->scene = (enum stream_scene)arg;
        break;
    case NODE_IOC_START:
        volume_ioc_start(hdl);
        break;
    case NODE_IOC_STOP:
    case NODE_IOC_SUSPEND:
        volume_ioc_stop(hdl);
        break;
    case NODE_IOC_SET_PARAM:
        ret = volume_ioc_update_parm(hdl, arg);
        break;
    case NODE_IOC_GET_PARAM:
        ret = get_volume_ioc_get_parm(hdl, arg);
        break;

    }

    return ret;
}

static void volume_release(struct stream_node *node)
{
    struct volume_hdl *hdl = (struct volume_hdl *)node->private_data;

    if (hdl->vol_table) {
        free(hdl->vol_table);
    }
}


REGISTER_STREAM_NODE_ADAPTER(volume_node_adapter) = {
    .uuid       = NODE_UUID_VOLUME_CTRLER,
    .bind       = volume_bind,
    .ioctl      = volume_ioctl,
    .release    = volume_release,
    .handle_frame = volume_handle_frame,
    .hdl_size   = sizeof(struct volume_hdl),
    .ability_channel_in = 1 | 2 | 4,
    .ability_channel_out = 1 | 2 | 4,
};

REGISTER_ONLINE_ADJUST_TARGET(volume) = {
    .uuid = NODE_UUID_VOLUME_CTRLER,
};

__attribute__((used))
int volume_version_1_0_0(void)
{
    return 0x100;
}



