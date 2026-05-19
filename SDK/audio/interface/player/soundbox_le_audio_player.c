#include "jlstream.h"
#include "classic/tws_api.h"
#include "media/audio_base.h"
#include "media/bt_audio_timestamp.h"
#include "sdk_config.h"
#include "scene_switch.h"
#include "volume_node.h"
#include "app_main.h"
#include "le_audio_player.h"
#include "le_audio_stream.h"
#include "uac_stream.h"
#if LE_AUDIO_LOCAL_MIC_EN
#include "le_audio_mix_mic_recorder.h"
#endif

#if (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_AURACAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SOURCE_EN | LE_AUDIO_UNICAST_SINK_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_BIS_TX_EN | LE_AUDIO_JL_BIS_RX_EN)) || \
    (TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_JL_CIS_CENTRAL_EN | LE_AUDIO_JL_CIS_PERIPHERAL_EN))

struct le_audio_player {
    struct jlstream *stream;
    void *le_audio;
    u16 timer;
    u8 inused;
#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
    u8 *stream_addr;
    char name[16];
    u8 le_audio_num;
    s16 dvol;
    u8 dvol_index;
    u8 save_vol_cnt;
    u16 save_vol_timer;
#endif
};


static struct le_audio_player g_le_audio_player = {0};


#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)

static struct le_audio_player g_le_audio_player_file[2] = {0};

static u8 vol_table[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

u8 le_audio_player_handle_is_have(u8 *addr)
{
    for (int i = 0; i < 2; i++) {
        if (g_le_audio_player_file[i].inused) {
            if (g_le_audio_player_file[i].stream_addr == addr) {
                return 1;
            }
        }
    }
    return 0;
}
void *le_audio_player_other_handle_is_busy(u8 *addr)
{
    for (int i = 0; i < 2; i++) {
        if (g_le_audio_player_file[i].inused) {
            if (g_le_audio_player_file[i].stream_addr ==  addr) {
                return &g_le_audio_player_file[i];
            }
        }
    }
    return NULL;
}
void *le_audio_player_handle_open(u8 *addr)
{
    local_irq_disable();
    for (int i = 0; i < 2; i++) {
        if (g_le_audio_player_file[i].inused == 0) {
            g_le_audio_player_file[i].inused = 1;
            g_le_audio_player_file[i].le_audio_num = i;
            g_le_audio_player_file[i].stream_addr = addr;
            local_irq_enable();
            return &g_le_audio_player_file[i];
        }
    }
    local_irq_enable();
    return NULL;
}
void le_audio_player_handle_close(u8 *addr)
{
    for (int i = 0; i < 2; i++) {
        if (g_le_audio_player_file[i].inused) {
            if (g_le_audio_player_file[i].stream_addr == addr) {
                g_le_audio_player_file[i].inused = 0;
                g_le_audio_player_file[i].le_audio_num = 0;
                g_le_audio_player_file[i].stream_addr = 0;
            }
        }
    }
}
void *get_le_audio_player_handle(u8 *addr)
{
    local_irq_disable();
    for (int i = 0; i < 2; i++) {
        if (g_le_audio_player_file[i].inused) {
            if (g_le_audio_player_file[i].stream_addr ==  addr) {
                local_irq_enable();
                return &g_le_audio_player_file[i];
            }
        }
    }
    local_irq_enable();
    return NULL;
}
void *get_cur_le_audio_player_handle()
{
    for (int i = 0; i < 2; i++) {
        if (g_le_audio_player_file[i].inused) {
            return &g_le_audio_player_file[i];
        }
    }
    return NULL;
}


int le_audio_player_create(u8 *conn)
{
    int err;
    int uuid;

    uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
    struct le_audio_player *player = get_le_audio_player_handle(conn);
    if (player) {
        if (player->stream) {
            printf("err  le audio stream is exist\n");
            return -EEXIST;
        }
    } else {
        player = le_audio_player_handle_open(conn);
        if (!player) {
            return -ENOMEM;
        }
    }
    if (player->le_audio_num) {
        y_printf("======   LE_Audio_Sink1  ======\n");
        strcpy(player->name, "LE_Audio_Sink1");
    } else {
        y_printf("======   LE_Audio_Sink0  ======\n");
        strcpy(player->name, "LE_Audio_Sink0");
    }
    player->le_audio = conn;
    player->stream = jlstream_pipeline_parse_by_node_name(uuid, player->name);
    if (!player->stream) {
        printf("create le audio  stream faild\n");
        return -EFAULT;
    }

    return 0;
}

static void le_audio_volume_save_do(void *priv)
{
    u8 le_audio_num = (u8)priv;
    if (++g_le_audio_player_file[le_audio_num].save_vol_cnt >= 5) {
        sys_timer_del(g_le_audio_player_file[le_audio_num].save_vol_timer);
        g_le_audio_player_file[le_audio_num].save_vol_timer = 0;
        g_le_audio_player_file[le_audio_num].save_vol_cnt = 0;
        printf("save le audio vol:%d\n", g_le_audio_player_file[le_audio_num].dvol);
        syscfg_write(CFG_WIRELESS_MIC0_VOLUME + le_audio_num, &g_le_audio_player_file[le_audio_num].dvol, 2);
        return;
    }
}

static void le_audio_volume_change(u32 le_audio_num)
{
    g_le_audio_player_file[le_audio_num].save_vol_cnt = 0;
    if (g_le_audio_player_file[le_audio_num].save_vol_timer == 0) {
        g_le_audio_player_file[le_audio_num].save_vol_timer = sys_timer_add((void *)le_audio_num, le_audio_volume_save_do, 1000);//中断里不能操作vm 关中断不能操作vm
    }
}

int le_audio_set_dvol(u8 le_audio_num, u8 vol)
{
    int err = 0;
    char *vol_name = le_audio_num ? "Vol_WMic1" : "Vol_WMic0";
    struct volume_cfg cfg = {0};
    cfg.bypass = VOLUME_NODE_CMD_SET_VOL;
    cfg.cur_vol = vol;
    if (g_le_audio_player_file[le_audio_num].inused) {
        int err = jlstream_set_node_param(NODE_UUID_VOLUME_CTRLER, vol_name, (void *)&cfg, sizeof(struct volume_cfg)) ;
        printf("le audo dvol name: %s, le audo dvol:%d\n", vol_name, vol);
        if (vol != g_le_audio_player_file[le_audio_num].dvol) {
            g_le_audio_player_file[le_audio_num].dvol = vol;
            le_audio_volume_change(le_audio_num);
        }
    }
    return 0;
}

s16 le_audio_get_dvol(u8 le_audio_num)
{
    if (g_le_audio_player_file[le_audio_num].inused) {
        return g_le_audio_player_file[le_audio_num].dvol;
    }
    return 0;
}

void le_audio_dvol_up(u8 le_audio_num)
{
    if (g_le_audio_player_file[le_audio_num].inused) {
        if (g_le_audio_player_file[le_audio_num].dvol_index < sizeof(vol_table) / sizeof(vol_table[0]) - 1) {
            g_le_audio_player_file[le_audio_num].dvol_index++;
            le_audio_set_dvol(le_audio_num, vol_table[g_le_audio_player_file[le_audio_num].dvol_index]);
        } else {
            printf("[WARING]le audio volum is max\n");
        }
    }

}

void le_audio_dvol_down(u8 le_audio_num)
{
    if (g_le_audio_player_file[le_audio_num].inused) {
        if (g_le_audio_player_file[le_audio_num].dvol_index) {
            g_le_audio_player_file[le_audio_num].dvol_index--;
            le_audio_set_dvol(le_audio_num, vol_table[g_le_audio_player_file[le_audio_num].dvol_index]);
        } else {
            printf("[WARING]le audio volum is min\n");
        }
    }
}
#endif

#if TCFG_VIR_UDISK_ENABLE
int virtual_udisk_get_enc_time(void)
{
    struct le_audio_player *player = &g_le_audio_player;
    if (!player) {
        return 0;
    }
    if (!player->inused) {
        return 0;
    }
    int time = 0;
    if (player->stream) {
        jlstream_ioctl(player->stream, NODE_IOC_GET_ENC_TIME, (int)(&time));
    }
    return time;
}
#endif

static void le_audio_player_callback(void *private_data, int event)
{
    struct le_audio_player *player = &g_le_audio_player;
#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
    player = (struct le_audio_player *)private_data;
#endif


    if (!player) {
        return;
    }

    if (!player->inused) {
        return;

    }
    printf("le audio player callback : %d\n", event);
    switch (event) {
    case STREAM_EVENT_START:
#if TCFG_VOCAL_REMOVER_NODE_ENABLE
        music_vocal_remover_update_parm();
#endif
#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
        int ret = syscfg_read(CFG_WIRELESS_MIC0_VOLUME + player->le_audio_num, &player->dvol, 2);
        if (ret < 0) {
            player->dvol = app_var.mic_eff_volume; //先使用mic_effect的音量配置
        }
        player->dvol_index = player->dvol / 10;
        if (player->dvol_index >= sizeof(vol_table) / sizeof(vol_table[0])) {
            player->dvol_index = sizeof(vol_table) / sizeof(vol_table[0]) - 1;
        }
        le_audio_set_dvol(player->le_audio_num, player->dvol);
        printf("le_audio_player_callback, le_audio_num:%d, dvol:%d\n", player->le_audio_num, player->dvol);
#endif
        break;
    case STREAM_EVENT_PREEMPTED:


        break;
    }
}

int le_audio_player_open(u8 *conn, struct le_audio_stream_params *lea_param)
{
    int err;
    int uuid;



#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
    err = le_audio_player_create(conn);
    struct le_audio_player *player = get_le_audio_player_handle(conn);
    if (err) {
        return err;
    }
    jlstream_set_scene(player->stream, STREAM_SCENE_WIRELESS_MIC);
    if (lea_param) {
        int frame_point = 0;
        if (lea_param->fmt.isoIntervalUs) { //多帧传输的时候，避免mixer拆帧输出，mixer_buf需要根据isoIntervalUs
            frame_point = lea_param->fmt.isoIntervalUs *  lea_param->fmt.sample_rate / 1000 / 1000;
        } else {
            frame_point = lea_param->fmt.frame_dms *  lea_param->fmt.sample_rate / 1000 / 10;
        }
        jlstream_node_ioctl(player->stream, NODE_UUID_MIXER, NODE_IOC_SET_PRIV_FMT, (int)frame_point);
    }
#else
    struct le_audio_player *player = &g_le_audio_player;

#ifdef CONFIG_WIRELESS_MIC_CASE_ENABLE
    uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"mic_effect");
#else
    uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"le_audio");
#endif
    if (!player) {
        return -ENOMEM;
    }
#if LE_AUDIO_LOCAL_MIC_EN
    if (player->inused == 1) {
        printf(">> %s le_audio_player_open, player is used!, return!\n", __func__);
        return 0;
    }
#endif

    player->le_audio = conn;
    player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_LE_AUDIO_SINK);
    if (!player->stream) {
        printf("create le_audio stream faild\n");
        return -EFAULT;
    }
    if (lea_param) {
        if (lea_param->service_type == LEA_SERVICE_CALL) {
            printf("LEA Service Type:Call\n");
            jlstream_set_scene(player->stream, STREAM_SCENE_LEA_CALL);
        } else {
            printf("LEA Service Type:Media\n");
#if TCFG_APP_PC_EN && TCFG_USB_SLAVE_AUDIO_SPK_ENABLE
            if (app_in_mode(APP_MODE_PC)) {
                u16 l_vol = 0, r_vol = 0;
                uac_speaker_stream_get_volume(&l_vol, &r_vol);
                app_audio_set_volume(APP_AUDIO_STATE_MUSIC, (r_vol + l_vol) / 2, 1);
            }
#endif
            jlstream_set_scene(player->stream, STREAM_SCENE_LE_AUDIO);
        }

    } else {
        jlstream_set_scene(player->stream, STREAM_SCENE_LE_AUDIO);
    }
#endif


    player->inused = 1;

    jlstream_set_callback(player->stream, player, le_audio_player_callback);

    int channel = le_audio_stream_get_dec_ch_mode(player->le_audio); //这里设置解码输出的声道类型
    jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, channel);


    err = jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE,
                              NODE_IOC_SET_BTADDR, (int)player->le_audio);

    if (err == 0) {
#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
        if (player->le_audio_num) {
            jlstream_add_thread(player->stream, "wl_mic_effect2");
            //jlstream_add_thread(player->stream, "wl_mic_effect4");
        } else {
            jlstream_add_thread(player->stream, "wl_mic_effect1");
            //jlstream_add_thread(player->stream, "wl_mic_effect3");
        }
#endif
        err = jlstream_start(player->stream);
    } else {
        jlstream_release(player->stream);
        return err;
    }

    printf("le audio player open success : 0x%x, 0x%x, 0x%x\n", (u32)player, (u32)player->le_audio, (u32)player->stream);

    return 0;

}

void le_audio_player_close(u8 *conn)
{
#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
    struct le_audio_player *player = get_le_audio_player_handle(conn);
    /* if (memcmp(player->stream_addr, conn, 6)) { */
    /*     return; */
    /* } */ //不用指针来判断，因为释放后去申请可能会申请到同一个指针
#else
    struct le_audio_player *player = &g_le_audio_player;

#endif

    if (!player) {
        return;
    }

    if (!player->inused) {
        return;
    }

    if (player->le_audio != conn) {
        return;
    }

#if LE_AUDIO_LOCAL_MIC_EN
    if (get_local_le_audio_status() != LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE) {
        printf(">> err, %s, %d, local_le_audio_status() != LOCAL_MIX_MIC_CLOSE_MUSIC_CLOSE\n", __func__, __LINE__);
        return;
    }
#endif

#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
    le_audio_player_handle_close(conn);
#else
    g_le_audio_player.inused = 0;
    g_le_audio_player.le_audio = NULL;
#endif


    printf("le audio player close : 0x%x, 0x%x, 0x%x\n", (u32)player, (u32)player->le_audio, (u32)player->stream);
    if (player->stream) {
        jlstream_stop(player->stream, 20);
        jlstream_release(player->stream);
        player->stream = NULL;
    }
#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"mic_effect");
#else
    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)"le_audio");
#endif


}

bool le_audio_player_is_playing(void)
{
#if (TCFG_KBOX_1T3_MODE_EN || WIRELESS_MIC_PRODUCT_MODE)
    if (get_cur_le_audio_player_handle()) {
        return 1;
    }
#else
    if (g_le_audio_player.stream) {
        return 1;
    }
#endif
    return 0;
}

#endif

