#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".mic_effect.data.bss")
#pragma data_seg(".mic_effect.data")
#pragma const_seg(".mic_effect.text.const")
#pragma code_seg(".mic_effect.text")
#endif
#include "system/includes.h"
#include "jlstream.h"
#include "sdk_config.h"
#include "effects/audio_echo.h"
#include "effects/effects_adj.h"
#include "media/audio_general.h"
#include "media/audio_def.h"
#include "volume_node.h"
#include "app_main.h"
#include "mic_effect.h"
#include "audio_config_def.h"
#include "audio_dac.h"
#include "audio_anc_includes.h"

static void mic_effect_ram_code_load();
static void mic_effect_ram_code_unload();
struct mic_effect_player {
    struct jlstream *stream;
    s16 dvol;
    u8 dvol_index;
    unsigned int echo_delay;                      //回声的延时时间 0-300ms
    unsigned int echo_decayval;                   // 0-70%

};

u8 vol_table[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static struct mic_effect_player *g_mic_effect_player = NULL;

static u16 mic_irq_point_unit = AUDIO_ADC_IRQ_POINTS;
#define HEARING_AID_MIC_IRQ_POINT_UNIT  AUDIO_ADC_IRQ_POINTS

static void mic_effect_player_callback(void *private_data, int event)
{
    struct mic_effect_player *player = g_mic_effect_player;
    struct jlstream *stream = (struct jlstream *)private_data;

    switch (event) {
    case STREAM_EVENT_START:
        char *vol_name = "VolEff";
        struct volume_cfg cfg = {0};
        cfg.bypass = VOLUME_NODE_CMD_SET_VOL;
        cfg.cur_vol = app_var.mic_eff_volume;
        jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg));
        break;
    }
}

int mic_effect_player_create(enum MIC_EFX_MODE mode)
{
    if (g_mic_effect_player) {
        return 0;
    }
    mic_effect_ram_code_load();
    int err;
    struct mic_effect_player *player;

    u16 uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
    if (uuid == 0) {
        return -EFAULT;
    }

    player = malloc(sizeof(*player));
    if (!player) {
        return -ENOMEM;
    }

    player->stream = jlstream_pipeline_parse_by_node_name(uuid, "IIS0_RX1");
    if (!player->stream) {
        player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_ADC);
    }

    if (!player->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_MIC_EFFECT, 1);
    audio_icsd_adt_reset(ADT_SCENE_MIC_EFFECT);
#endif

    if (mode == MIC_EFX_DHA) {
        //设置中断点数
        jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, HEARING_AID_MIC_IRQ_POINT_UNIT);
        //设置场景
        jlstream_set_scene(player->stream, STREAM_SCENE_HEARING_AID);
    } else {
        jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PRIV_FMT, mic_irq_point_unit);
        jlstream_set_scene(player->stream, STREAM_SCENE_MIC_EFFECT);
    }
    jlstream_set_callback(player->stream, player->stream, mic_effect_player_callback);
    jlstream_add_thread(player->stream, "mic_effect1");
    if (CONFIG_JLSTREAM_MULTI_THREAD_ENABLE) {
        jlstream_add_thread(player->stream, "mic_effect2");
    }

    err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }

    //记录echo、Dvol节点的参数 供按键调节使用
    echo_param_tool_set echo_cfg = {0};
    char *node_name       = "EchoEff";     //节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    int ret = jlstream_read_form_data(0, node_name, 0, &echo_cfg);
    if (ret) {
        printf("read echo parm delay %d\n", echo_cfg.parm.delay);
        player->echo_delay = echo_cfg.parm.delay;
        player->echo_decayval = echo_cfg.parm.decayval;
    }

    player->dvol = app_var.mic_eff_volume;
    player->dvol_index = player->dvol / 10;
    if (player->dvol_index >= sizeof(vol_table) / sizeof(vol_table[0])) {
        player->dvol_index = sizeof(vol_table) / sizeof(vol_table[0]) - 1;
    }
    printf("\n mic dvol %d \n", player->dvol);
    g_mic_effect_player = player;
    return 0;

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_MIC_EFFECT, 0);
    audio_icsd_adt_reset(ADT_SCENE_MIC_EFFECT);
#endif

__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);
    return err;
}

void mic_effect_player_delete()
{
    struct mic_effect_player *player = g_mic_effect_player;

    if (!player) {
        return;
    }
    jlstream_stop(player->stream, 0);
    jlstream_release(player->stream);

    mic_effect_ram_code_unload();
    free(player);
    g_mic_effect_player = NULL;

#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    audio_icsd_adt_scene_set(ADT_SCENE_MIC_EFFECT, 0);
    audio_icsd_adt_reset(ADT_SCENE_MIC_EFFECT);
#endif

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic_effect");
}

#if defined(TCFG_CODE_RUN_RAM_MIC_EFF_CODE) && TCFG_CODE_RUN_RAM_MIC_EFF_CODE
static spinlock_t mic_eff_code_ram;
static u8 *mic_eff_code_run_addr = NULL;
//代码动态加载
static void mic_effect_ram_code_load()
{
    int code_size = __mic_eff_movable_region_end - __mic_eff_movable_region_start;
    //printf("mic_eff code size %d\n", code_size);
    //mem_stats();
    if (code_size && !mic_eff_code_run_addr) {
        mic_eff_code_run_addr = phy_malloc(code_size);
    }
    spin_lock(&mic_eff_code_ram);
    if (mic_eff_code_run_addr) {
        //printf("mic_eff_code_run_addr: %x\n", (unsigned int)mic_eff_code_run_addr);
        code_movable_load(__mic_eff_movable_region_start, code_size, mic_eff_code_run_addr, __mic_eff_movable_slot_start, __mic_eff_movable_slot_end);
    }
    spin_unlock(&mic_eff_code_ram);
}


static void mic_effect_ram_code_unload()
{
    if (mic_eff_code_run_addr) {
        //mem_stats();
        spin_lock(&mic_eff_code_ram);
        code_movable_unload(__mic_eff_movable_region_start, __mic_eff_movable_slot_start, __mic_eff_movable_slot_end);
        spin_unlock(&mic_eff_code_ram);
        phy_free(mic_eff_code_run_addr);

        mic_eff_code_run_addr = NULL;
        //mem_stats();
        //printf("mic_eff code unload end\n");
    }
}
#else
static void mic_effect_ram_code_load()
{
}

static void mic_effect_ram_code_unload()
{
}
#endif

/************************** 通用麦克风音效接口 **************************/
void mic_effect_set_irq_point_unit(u16 point_unit)
{
    mic_irq_point_unit = point_unit;
}

int mic_effect_player_open()
{
    return mic_effect_player_create(MIC_EFX_NORMAL);
}

bool mic_effect_player_runing()
{
    return g_mic_effect_player != NULL;
}

int mic_effect_player_is_playing()
{
    return true;
}

void mic_effect_player_close()
{
    mic_effect_player_delete();
}

static u8 pause_mark = 0;
//混响暂停恢复接口
void mic_effect_player_pause(u8 mark)
{
    if (mark) { //暂停
        //混响在运行时才暂停（关闭）
        if (mic_effect_player_runing()) {
            mic_effect_player_close();
            pause_mark  = 1;
        }
    } else {
        if (pause_mark) {
            mic_effect_player_open();
        }
        pause_mark  = 0;
    }
}
//echo 调节接口
void mic_effect_set_echo_delay(u32 delay)
{
    if (!g_mic_effect_player) {
        return;
    }
    echo_param_tool_set cfg = {0};
    /*
     *解析配置文件内效果配置
     * */
    char mode_index       = 0;               //模式序号（当前节点无多参数组，mode_index是0）
    char *node_name       = "EchoEff";     //节点名称（节点内的第一参数，用户自定义,长度小于等于15byte）
    char cfg_index        = 0;               //目标配置项序号（当前节点无多参数组，cfg_index是0）
    int ret = jlstream_read_form_data(mode_index, node_name, cfg_index, &cfg);
    if (!ret) {
        printf("read parm err\n");
        return;
    }
    g_mic_effect_player->echo_delay = delay;
    cfg.parm.delay = g_mic_effect_player->echo_delay;

    /*
     *将配置文件内获取得到的参数更新到目标节点
     * */
    jlstream_set_node_param(NODE_UUID_ECHO, node_name, &cfg, sizeof(cfg));
}
u32 mic_effect_get_echo_delay(void)
{
    if (g_mic_effect_player) {
        return g_mic_effect_player->echo_delay;
    }
    return 0;
}

//mic音量调节接口
void mic_effect_set_dvol(u8 vol)
{
    char *vol_name = "VolEff";
    struct volume_cfg cfg = {0};
    cfg.bypass = VOLUME_NODE_CMD_SET_VOL;
    cfg.cur_vol = vol;

    if (g_mic_effect_player) {
        jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg));
        g_mic_effect_player->dvol = vol;
        app_var.mic_eff_volume = vol;
        syscfg_write(CFG_MIC_EFF_VOLUME_INDEX, &app_var.mic_eff_volume, 2);
    }
}
u8 mic_effect_get_dvol(void)
{

    if (g_mic_effect_player) {
        return g_mic_effect_player->dvol;
    }
    return 0;
}
void mic_effect_dvol_up(void)
{
    if (g_mic_effect_player) {
        if (g_mic_effect_player->dvol_index < sizeof(vol_table) / sizeof(vol_table[0]) - 1) {
            g_mic_effect_player->dvol_index++;
            mic_effect_set_dvol(vol_table[g_mic_effect_player->dvol_index]);
        }
    }
}
void mic_effect_dvol_down(void)
{
    if (g_mic_effect_player) {
        if (g_mic_effect_player->dvol_index) {
            g_mic_effect_player->dvol_index--;
            mic_effect_set_dvol(vol_table[g_mic_effect_player->dvol_index]);
        }
    }
}

/**********************************辅听接口**************************/
int hearing_aid_player_open()
{
    DAC_NOISEGATE_OFF();
    return mic_effect_player_create(MIC_EFX_DHA);
}

bool hearing_aid_player_runing()
{
    return mic_effect_player_runing();
}

void hearing_aid_player_close()
{
    DAC_NOISEGATE_ON();
    mic_effect_player_delete();
}

/*辅听开关接口*/
void hearing_aid_switch()
{
    if (hearing_aid_player_runing()) {
        hearing_aid_player_close();
    } else {
        hearing_aid_player_open();
    }
}

void hearing_aid_set_dvol(u8 vol)
{
    mic_effect_set_dvol(vol);
}

u8 hearing_aid_get_dvol(void)
{
    return mic_effect_get_dvol();
}

void hearing_aid_dvol_up(void)
{
    mic_effect_dvol_up();
}

void hraring_aid_dvol_down(void)
{
    mic_effect_dvol_down();
}
