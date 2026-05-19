
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ai_rx_player.data.bss")
#pragma data_seg(".ai_rx_player.data")
#pragma const_seg(".ai_rx_player.text.const")
#pragma code_seg(".ai_rx_player.text")
#endif

#include "jlstream.h"
#include "sdk_config.h"
#include "audio_config_def.h"
#include "ai_rx_player.h"
#include "ai_player.h"
#include "classic/tws_api.h"
#include "audio_dac.h"

#if  TCFG_AI_RX_NODE_ENABLE

struct ai_rx_player {
    struct jlstream *stream;
    u8 *bt_addr;
    u8 source;
    struct list_head entry;
    u8 type;
    int channel; //记录当前是tws左声道还是右声道
};

extern int CONFIG_BTCTLER_TWS_ENABLE;

static struct list_head g_ai_rx_player_list = LIST_HEAD_INIT(g_ai_rx_player_list);

#define list_for_each_ai_rx_player(p) \
    list_for_each_entry(p, &g_ai_rx_player_list, entry)

static void ai_rx_player_callback(void *private_data, int event)
{
    struct ai_rx_player *player = (struct ai_rx_player *)private_data;

    printf("ai_rx_player_callback, event: %d\n", event);
    switch (event) {
    case STREAM_EVENT_START:
#if TCFG_AI_PLAYER_ENABLE
        ai_player_update_play_volume((u8)player->source);
#endif
        //清除AEC参考DAC数据缓存，否则AEC采集到上次的DAC数据缓存，超出了算法
        //允许的缓存时间
        audio_dac_read_reset();
        break;
    }
}


int ai_rx_player_open(u8 *bt_addr, u8 source, struct ai_rx_player_param *param)
{
    int err;
    struct ai_rx_player *player;
    u16 uuid = 0;
    if (!param) {
        printf("ai_rx_player param is NULL \n");
        return -EFAULT;
    }

    u8 type = param->type;

    list_for_each_ai_rx_player(player) {
        if (player->source == source) {
            printf("ai_rx_player id %x is opened\n", (u32)source);
            return 0;
        }
    }

    if (type == AI_SERVICE_CALL_DOWNSTREAM || type == AI_SERVICE_CALL_UPSTREAM) {
        uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"ai_rx_call");
    } else if (type == AI_SERVICE_MEDIA) {
        uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"ai_rx_media");
    } else {
        uuid = jlstream_event_notify(STREAM_EVENT_GET_PIPELINE_UUID, (int)"ai_voice");
    }
    if (uuid == 0) {
        return -EFAULT;
    }

    player = malloc(sizeof(*player));
    if (!player) {
        return -ENOMEM;
    }

    if (type == AI_SERVICE_CALL_DOWNSTREAM) {
        player->stream = jlstream_pipeline_parse_by_node_name(uuid, "AI_RX_ESCO_DOWN");
    } else if (type == AI_SERVICE_CALL_UPSTREAM) {
        player->stream = jlstream_pipeline_parse_by_node_name(uuid, "AI_RX_ESCO_UP");
    } else {
        player->stream = jlstream_pipeline_parse(uuid, NODE_UUID_AI_RX);
    }
    if (!player->stream) {
        err = -ENOMEM;
        goto __exit0;
    }

    list_add_tail(&player->entry, &g_ai_rx_player_list);
    player->bt_addr = bt_addr;
    player->source = source;
    player->type = type;

    jlstream_set_callback(player->stream, player, ai_rx_player_callback);
    if (type == AI_SERVICE_CALL_DOWNSTREAM || type == AI_SERVICE_CALL_UPSTREAM) {
        jlstream_set_scene(player->stream, STREAM_SCENE_ESCO);
    } else if (type == AI_SERVICE_MEDIA) {
        jlstream_set_scene(player->stream, STREAM_SCENE_A2DP);
    } else {
        jlstream_set_scene(player->stream, STREAM_SCENE_AI_VOICE);

    }
    if (CONFIG_BTCTLER_TWS_ENABLE) {
        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
            player->channel = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
            jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, player->channel);
        } else {
            jlstream_ioctl(player->stream, NODE_IOC_SET_CHANNEL, AUDIO_CH_MIX);
        }
    }
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_BTADDR, (int)bt_addr);
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_PARAM, (int)source);
    jlstream_node_ioctl(player->stream, NODE_UUID_SOURCE, NODE_IOC_SET_FMT, (int)param);

    err = jlstream_start(player->stream);
    if (err) {
        goto __exit1;
    }

    return 0;

__exit1:
    jlstream_release(player->stream);
__exit0:
    free(player);
    return err;
}

bool ai_rx_player_runing(u8 source)
{
    struct ai_rx_player *player;
    list_for_each_ai_rx_player(player) {
        if (player->source == source) {
            return 1;
        }
    }
    return 0;
}


void ai_rx_player_close(u8 source)
{
    struct ai_rx_player *player;
    u8 found = 0;
    const char *pipeline;

    list_for_each_ai_rx_player(player) {
        if (player->source == source) {
            found = 1;
            break;
        }
    }
    if (!found) {
        return;
    }
    if (player->type == AI_SERVICE_CALL_DOWNSTREAM || player->type == AI_SERVICE_CALL_UPSTREAM) {
        pipeline = "ai_rx_call";
    } else if (player->type == AI_SERVICE_MEDIA) {
        pipeline = "ai_rx_media";
    } else {
        pipeline = "ai_voice";
    }
    jlstream_stop(player->stream, 0);
    jlstream_release(player->stream);

    list_del(&player->entry);
    free(player);

    jlstream_event_notify(STREAM_EVENT_CLOSE_PLAYER, (int)pipeline);
}

#endif /*TCFG_AI_RX_NODE_ENABLE*/
