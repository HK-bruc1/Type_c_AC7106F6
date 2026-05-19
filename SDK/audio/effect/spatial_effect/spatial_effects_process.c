#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".spatial_effects_process.data.bss")
#pragma data_seg(".spatial_effects_process.data")
#pragma const_seg(".spatial_effects_process.text.const")
#pragma code_seg(".spatial_effects_process.text")
#endif
#include "app_config.h"
#include "spatial_effects_process.h"
#include "task.h"
#include "spatial_effect.h"
#include "circular_buf.h"
#include "jlstream.h"
#include "clock_manager/clock_manager.h"
#include "node_param_update.h"
#include "a2dp_player.h"
#include "ascii.h"
#include "scene_switch.h"
#include "app_tone.h"
#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)

typedef struct {
    volatile u8 state;
    volatile u8 busy;
    void *priv;
    void *spatial_audio;
    s16 *tmp_buf;
    int tmp_buf_len;
    u8 fade_flag;
    u8 bit_width;
} aud_effect_t;
static aud_effect_t *aud_effect = NULL;

struct spatial_effect_global_param {
    struct jlsream_crossfade crossfade;
    enum SPATIAL_EFX_MODE spatial_audio_mode;
    volatile u8 spatial_audio_fade_finish;
    volatile u8 frame_pack_disable;
    u32 breaker_timer;
    u32 switch_timer;
};
static struct spatial_effect_global_param g_param = {
    .spatial_audio_mode = SPATIAL_EFX_FIXED,//空间音效初始化状态
    .spatial_audio_fade_finish = SPATIAL_EFX_FIXED,//是否完成淡入淡出的标志
    .frame_pack_disable = 0,//外部数据流是否需要拼帧标志位。关闭空间音效时使能
    .breaker_timer = 0,  //关闭音效后数据流插入断点定时器
    .switch_timer = 0,   //恢复断点后打开空间音效定时器
};

static void *audio_spatial_effects_open()
{
    printf("%s", __func__);
    if (g_param.spatial_audio_mode == SPATIAL_EFX_TRACKED) {
        clock_alloc("SPATIAL_EFX_TRACKED", SPATIAL_EFX_TRACKED_CLK);
    }
    clock_alloc("SPATIAL_EFX_FIXED", SPATIAL_EFX_FIXED_CLK);
    return spatial_audio_open();
}

static void audio_spatial_effects_close(void *spatial_audio)
{
    printf("%s", __func__);
    g_param.spatial_audio_fade_finish = 1;
    spatial_audio_close(spatial_audio);
    clock_free("SPATIAL_EFX_FIXED");
    clock_free("SPATIAL_EFX_TRACKED");
}

int audio_spatial_effects_frame_pack_disable()
{
    return g_param.frame_pack_disable;
}

int audio_spatial_effects_data_handler(u8 out_channel, s16 *data, u16 len)
{
    aud_effect_t *effect = (aud_effect_t *)aud_effect;
    int wlen;
    static u8 last_bypass = 0;;
    u8 bypass = get_spatial_effect_node_bypass();
    /*开空间音效的时候，切bypass才做淡入淡出处理*/
    if (g_param.spatial_audio_mode && (last_bypass != bypass)) {
        g_param.spatial_audio_fade_finish = 0;
    }
    last_bypass = bypass;

    /*关闭空间音效 或者bypass时*/
    if (((!g_param.spatial_audio_mode || bypass) && g_param.spatial_audio_fade_finish)) {
        wlen = spatial_audio_remapping_data_handler(out_channel, get_spatial_effect_node_bit_width(), data, len);
        /* printf("r"); */
        return wlen;
    }
    /*空间音效初始化完成了才可跑音效*/
    if ((!effect || !effect->state || !effect->spatial_audio)) {
        wlen = spatial_audio_remapping_data_handler(out_channel, get_spatial_effect_node_bit_width(), data, len);
        /* printf("r"); */
    } else {

        effect->busy = 1;

        if (!g_param.spatial_audio_fade_finish)  {
            effect->fade_flag = 1;
            if (!g_param.crossfade.enable) {
                g_param.crossfade.sample_rate = get_spatial_effect_node_sample_rate();
                g_param.crossfade.msec = 200;
                g_param.crossfade.channel = 2;
                g_param.crossfade.bit_width = effect->bit_width;
                jlstream_frames_cross_fade_init(&g_param.crossfade);
            }
        }
        // printf("s");
        if (effect->fade_flag) {

            if (effect->tmp_buf_len < len) {
                free(effect->tmp_buf);
                effect->tmp_buf = NULL;
                effect->tmp_buf_len = len;
                effect->tmp_buf = zalloc(effect->tmp_buf_len);
            }

            memcpy(effect->tmp_buf, data, len);
            spatial_audio_remapping_data_handler(out_channel, effect->bit_width, effect->tmp_buf, len);
        }

        /*设置输出声道*/
        spatial_audio_set_mapping_channel(effect->spatial_audio, out_channel);
        /*音效处理*/
        wlen = spatial_audio_filter(effect->spatial_audio, data, len);

        if (effect->fade_flag) {
            u8 ret = 0;
            if (g_param.spatial_audio_mode && !bypass) {
                /*打开空间音效时*/
                ret = jlstream_frames_cross_fade_run(&g_param.crossfade, data, effect->tmp_buf, data, wlen);
            } else {
                /*关闭空间音效时*/
                ret = jlstream_frames_cross_fade_run(&g_param.crossfade, effect->tmp_buf, data, data, wlen);
            }
            if (ret == STREAM_FADE_END) {
                effect->fade_flag = 0;
                g_param.spatial_audio_fade_finish = 1;
            }
        }
        effect->busy = 0;
    }

#if (TCFG_AUDIO_SPATIAL_EFFECT_ENABLE && TCFG_SENSOR_DATA_READ_IN_DEC_TASK)
    aud_spatial_sensor_run(NULL, NULL, 0);
#endif
    return wlen;
}

u8 get_spatial_effects_busy()
{
    aud_effect_t *effect = (aud_effect_t *)aud_effect;
    if (effect) {
        /* printf("busy %d,", effect->busy); */
        return effect->busy;
    }
    return 0;
}

int audio_effect_process_start(void)
{
    int err = 0;
    aud_effect_t *effect = NULL;
    g_param.frame_pack_disable = 0;
    if (spatial_effect_node_is_running() == 0) {
        printf("spatial_effect_node_is_running : %d", spatial_effect_node_is_running());
        return 0;
    }
    if (!g_param.spatial_audio_mode) {
        return 0;
    }
    if (aud_effect) {
        printf("aud_effect is already open !!!");
        return -1;
    }
    effect = zalloc(sizeof(aud_effect_t));
    printf("audio_dec_effect_process_start,need bufsize:%d\n", (int)sizeof(aud_effect_t));
    if (effect == NULL) {
        printf("aud_effect malloc fail !!!");
        return -1;
    }

    effect->bit_width = get_spatial_effect_node_bit_width();
    effect->tmp_buf_len = 1024;
    effect->tmp_buf = zalloc(effect->tmp_buf_len);

    effect->spatial_audio = audio_spatial_effects_open();
    effect->state = 1;
    aud_effect = effect;

    return err;
}

int audio_effect_process_stop(void)
{
    g_param.frame_pack_disable = 1;
    g_param.spatial_audio_fade_finish = 1;
    if (spatial_effect_node_is_running() == 0) {
        printf("spatial_effect_node_is_running : %d", spatial_effect_node_is_running());
        return 0;
    }
    aud_effect_t *effect = (aud_effect_t *)aud_effect;
    if (effect) {
        printf("audio_effect_process_stop\n");
        effect->state = 0;
        while (effect->busy) {
            putchar('w');
            os_time_dly(1);
        }
        audio_spatial_effects_close(effect->spatial_audio);
        effect->spatial_audio = NULL;
        if (effect->tmp_buf) {
            free(effect->tmp_buf);
            effect->tmp_buf = NULL;
        }
        free(effect);
        aud_effect = NULL;
    }
    return 0;
}

void set_a2dp_spatial_audio_mode(enum SPATIAL_EFX_MODE mode)
{
    g_param.spatial_audio_mode = mode;
}

enum SPATIAL_EFX_MODE get_a2dp_spatial_audio_mode(void)
{
    return g_param.spatial_audio_mode;
}

/*添加断点，断开数据流的定时器*/
void spatial_effect_breaker_timer(void *p)
{
    g_param.breaker_timer = 0;
    u8 spatial_mode = (u8)p;
    g_printf("%s()  %d", __func__, spatial_mode);
    a2dp_player_breaker_mode(spatial_mode,
                             BREAKER_SOURCE_NODE_UUID, BREAKER_SOURCE_NODE_NEME,
                             BREAKER_TARGER_NODE_UUID, BREAKER_TARGER_NODE_NEME);
}

/*删除断点恢复数据流后，切换音效的定时器*/
void spatial_effect_switch_timer(void *p)
{
    u8 spatial_mode = (u8)p;
    g_printf("%s()  %d", __func__, spatial_mode);
    audio_spatial_effects_mode_switch(spatial_mode);
    g_param.switch_timer = 0;
}

/*eq时钟设置*/
void spatial_effect_eq_clk_set(u8 mode, u8 eq_index)
{
    static u8 last_mode = 0xff;
    if (mode == last_mode) {
        return ;
    }
    last_mode = mode;
    char eq_clk_name[] = "EQ00000";
    int eq_clk = 12 * 1000000L;
    sprintf(eq_clk_name, "EQ0000%d", eq_index);
    if (mode) {
        printf("clk_add:%s", eq_clk_name);
        clock_alloc(eq_clk_name, eq_clk);
    } else {
        printf("clk_del:%s", eq_clk_name);
        clock_free(eq_clk_name);
    }
}

/*需要切换参数的eq节点名字*/
static char spatial_eq_name[][16] = {"MusicEq", "SpatialEq1", "SpatialEq2", "SpatialEq3"};

#if 0 //v300默认流程不添加动态eq
static char spatial_dy_eq_name[16] = "SpatialDyEq";
int spatial_effect_dy_eq_bypass(u8 is_bypass)
{
    dynamic_eq_pro_param_tool_set cfg = {0};
    int ret = jlstream_read_form_data(0, spatial_dy_eq_name, 0, &cfg);
    if (!ret) {
        printf("read parm err, %s, %s\n", __func__, spatial_dy_eq_name);
        return -1;
    }
    cfg.is_bypass = is_bypass;
    return jlstream_set_node_param(NODE_UUID_DYNAMIC_EQ_PRO, spatial_dy_eq_name, &cfg, sizeof(cfg));
}
#endif

/*空间音频开关时候进行相关节点参数配置
 * 0 ：关闭
 * 1 ：固定模式
 * 注意：eq节点更新参数有淡入淡出，其他的没有，所有只更新eq节点的参数，
 *      如果更新其他节点的参数，切换不连续的杂音*/
void spatial_effect_change_eq(u8 mode)
{
    int spatial_mode = mode ? 1 : 0;
    g_printf("%s()  %d", __func__, spatial_mode);
    /*切换eq效果*/
    for (int i = 0; i < ARRAY_SIZE(spatial_eq_name); i++) {
        printf("eq:%s", spatial_eq_name[i]);
        if (config_audio_eq_xfade_enable) {
            eq_update_tab(0, spatial_eq_name[i], spatial_mode);
        } else {
            eq_update_parm(0, spatial_eq_name[i], spatial_mode);
        }
        spatial_effect_eq_clk_set(spatial_mode, i);
    }
#if 0 //v300默认流程不添加动态eq
    if (CONFIG_SPATIAL_EFFECT_VERSION == SPATIAL_EFFECT_V3) {
        /*v300版本流程中 dy_eq开关*/
        u8 is_bypass = mode ? 0 : 1;
        spatial_effect_dy_eq_bypass(is_bypass);
    }
#endif
}

static u32 effect_strcmp(const char *str1, const char *str2)
{
    return ASCII_StrCmp(str1, str2, strlen(str1) + 1);
}
/*设置播歌打开空间音效eq的默认参数*/
int spatial_effect_eq_default_parm_set(char name[16], struct eq_default_parm *get_eq_parm)
{
    int ret = 0;
    for (int i = 0; i < ARRAY_SIZE(spatial_eq_name); i++) {
        if (!effect_strcmp(name, spatial_eq_name[i])) {
            get_eq_parm->cfg_index = get_a2dp_spatial_audio_mode() ? 1 : 0;
            get_eq_parm->mode_index = get_current_scene();
            ret = 1;
        }
    }
    return ret;
}


/*空间音频开关时候进行相关节点参数配置
 * 0 ：关闭
 * 1 ：固定模式
 * 注意：eq节点更新参数有淡入淡出，其他的没有，所有只更新eq节点的参数，
 *      如果更新其他节点的参数，切换不连续的杂音*/
int spatial_audio_change_effect(u8 mode)
{
    int spatial_mode = mode;
    g_printf("%s()  %d", __func__, spatial_mode);
    if (spatial_mode) {
        if (g_param.breaker_timer) {
            sys_timeout_del(g_param.breaker_timer);
            g_param.breaker_timer = 0;
        }

        if (g_param.switch_timer == 0) {
            /*开空间音效的时候,直接恢复数据流*/
            a2dp_player_breaker_mode(spatial_mode,
                                     BREAKER_SOURCE_NODE_UUID, BREAKER_SOURCE_NODE_NEME,
                                     BREAKER_TARGER_NODE_UUID, BREAKER_TARGER_NODE_NEME);
            /*恢复数据流后，定时1s后切换音效*/
            g_param.switch_timer =  sys_timeout_add((void *)spatial_mode, spatial_effect_switch_timer, EFFECT_DELAY_TIME);
            return 1;
        }
    } else {
        if (g_param.switch_timer) {
            sys_timeout_del(g_param.switch_timer);
            g_param.switch_timer = 0;
        }
        /*关空间音效的时候，需要延时等待淡入淡出完成后，再断开数据流*/
        if (g_param.breaker_timer) {
            sys_timeout_del(g_param.breaker_timer);
            g_param.breaker_timer =  sys_timeout_add((void *)spatial_mode, spatial_effect_breaker_timer, BREAKER_DELAY_TIME);
        } else {
            g_param.breaker_timer =  sys_timeout_add((void *)spatial_mode, spatial_effect_breaker_timer, BREAKER_DELAY_TIME);
        }
    }
    spatial_effect_change_eq(spatial_mode);
    return 0;
}

#if SPATIAL_AUDIO_EFFECT_SW_TONE_PLAY
void audio_spatial_effects_mode_switch_tone_play(enum SPATIAL_EFX_MODE mode)
{
    if (mode == g_param.spatial_audio_mode) {
        //相同模式切换
        return;
    }
    if (mode == SPATIAL_EFX_OFF) {
        play_tone_file_alone(get_tone_files()->num[0]);
    } else if (mode == SPATIAL_EFX_FIXED) {
        play_tone_file_alone(get_tone_files()->num[1]);
    } else if (mode == SPATIAL_EFX_TRACKED) {
        play_tone_file_alone(get_tone_files()->num[2]);
    }
    a2dp_player_reset_spatial_tone_play(mode);
}
#endif


/*空间音频模式切换
 * 0 ：关闭
 * 1 ：固定模式
 * 2 ：跟踪模式*/
void audio_spatial_effects_mode_switch(enum SPATIAL_EFX_MODE mode)
{
#if SPATIAL_AUDIO_EFFECT_SW_TONE_PLAY
    //播放打断提示音，重新创建音频流方式
    audio_spatial_effects_mode_switch_tone_play(mode);
    return;
#endif
    aud_effect_t *effect = (aud_effect_t *)aud_effect;
    /*没有跑节点，不允许切模式*/
    if (spatial_effect_node_is_running() == 0) {
        printf("spatial_effect_node_is_running : %d", spatial_effect_node_is_running());
        return ;
    }
    /*相同模式或者切换淡入淡出没有完成，不允许切模式*/
    if ((mode == g_param.spatial_audio_mode) || (!g_param.spatial_audio_fade_finish)) {
        return;
    }
    /*恢复数据流后，定时1s后切换音效*/
    if (spatial_audio_change_effect(mode)) {
        return;
    }
    if (g_param.spatial_audio_mode && mode && (g_param.spatial_audio_mode != mode)) {
        //固定->跟踪 或 跟踪->固定，不做crossfade
        g_param.spatial_audio_fade_finish = 1;
    } else if (!get_spatial_effect_node_bypass()) {
        /*设置开始淡入淡出的标志*/
        g_param.spatial_audio_fade_finish = 0;
    }
    /*设置全局标志*/
    set_a2dp_spatial_audio_mode(mode);

    if (mode == SPATIAL_EFX_OFF) {
        /*等待空间音效再跑一帧数据做淡入淡出*/
        while (effect && !g_param.spatial_audio_fade_finish) {
            os_time_dly(1);
            /*重新赋值，防止在等待过程，数据流关闭了空间音效的情况*/
            effect = (aud_effect_t *)aud_effect;
        }

        if (effect) {
            /*固定和跟踪模式的切换需要跑*/
            spatial_audio_head_tracked_en(effect->spatial_audio, 0);
            spatial_audio_sensor_sleep(1);
        }
        /*先改变节点标志，再释放资源*/
        audio_effect_process_stop();
        g_param.spatial_audio_fade_finish = 1;
    } else if (mode == SPATIAL_EFX_FIXED) {
        /*先申请资源，再改变节点标志, 关闭和固定的切换跑*/
        audio_effect_process_start();

        /*关闭和固定的切换不会进*/
        if (effect) {
            /*固定和跟踪模式的切换需要跑*/
            spatial_audio_head_tracked_en(effect->spatial_audio, 0);
            spatial_audio_sensor_sleep(1);
        }
        clock_free("SPATIAL_EFX_TRACKED");

    } else if (mode == SPATIAL_EFX_TRACKED) {
        clock_alloc("SPATIAL_EFX_TRACKED", SPATIAL_EFX_TRACKED_CLK);
        /*先申请资源，再改变节点标志, 关闭和跟踪的切换跑*/
        audio_effect_process_start();

        /*关闭和跟踪的切换不会进*/
        if (effect) {
            /*固定和跟踪模式的切换需要跑*/
            spatial_audio_sensor_sleep(0);
            spatial_audio_head_tracked_en(effect->spatial_audio, 1);
        }
    }
}
#endif /*TCFG_AUDIO_SPATIAL_EFFECT_ENABLE*/

