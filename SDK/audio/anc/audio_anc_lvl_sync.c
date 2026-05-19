#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_anc_lvl_sync.data.bss")
#pragma data_seg(".audio_anc_lvl_sync.data")
#pragma const_seg(".audio_anc_lvl_sync.text.const")
#pragma code_seg(".audio_anc_lvl_sync.text")
#endif

#include "app_config.h"
#if (TCFG_AUDIO_ANC_ENABLE || TCFG_AUDIO_AVC_NODE_ENABLE)

#include "audio_anc_includes.h"
#include "system/includes.h"
#include "bt_tws.h"
#include "audio_config.h"
#include "sniff.h"
#include "audio_anc_lvl_sync.h"

#if 0
#define lvl_sync_log printf
#else
#define lvl_sync_log(...)
#endif

struct lvl_sync_list {
    struct list_head head;            //链表头
};
static struct lvl_sync_list lvl_sync_list_hdl = {0};

//anc_init初始化链表
int audio_anc_lvl_sync_list_init()
{
    INIT_LIST_HEAD(&lvl_sync_list_hdl.head);
    return 0;
}
early_initcall(audio_anc_lvl_sync_list_init);

struct audio_anc_lvl_sync *audio_anc_lvl_sync_get_hdl_by_name(u8 name)
{
    local_irq_disable();
    if (list_empty(&lvl_sync_list_hdl.head)) {
        local_irq_enable();
        return NULL;
    }
    struct audio_anc_lvl_sync *lvl_sync;
    list_for_each_entry(lvl_sync, &lvl_sync_list_hdl.head, hentry) {
        /* lvl_sync_log("name %d, %d\n", name, lvl_sync->name); */
        if (name == lvl_sync->name) {
            local_irq_enable();
            return lvl_sync;
        }
    }
    local_irq_enable();
    lvl_sync_log("anc lvl sync cur name %d NULL\n", name);
    return NULL;
}

static int audio_anc_lvl_sync_add_check(struct audio_anc_lvl_sync *hdl, u8 mode, int num)
{
    if (!hdl) {
        return 0;
    }
    if (!hdl->sync_check) {
        return 0;
    }
    for (int i = 0; i < ARRAY_SIZE(hdl->sync_cnt); i++) {
        if (mode == BIT(i)) {
            hdl->sync_cnt[i] += num;
            if (hdl->sync_cnt[i] < 0) {
                hdl->sync_cnt[i] = 0;
                lvl_sync_log("Err: audio_anc_lvl_sync_add_check cnt error, name %d\n", hdl->name);
            }
            if (hdl->sync_cnt[i] > 3) {	//同一条消息TWS发送多次未收到
                hdl->sync_cnt[i] = 3;
                lvl_sync_log("Err: audio_anc_lvl_sync_add_check full, name %d\n", hdl->name);
                return -1;
            }
        }
        break;
    }
    return 0;
}

static void audio_anc_lvl_sync_info_cb(void *_data, u16 len, bool rx)
{
    u8 *data = (u8 *)_data;
    u8 mode = data[0];
    u8 noise_lvl = data[1];
    u8 name = data[2];
    /* lvl_sync_log("audio_anc_lvl_sync_info_cb rx:%d, mode:%d, name %d", rx, mode, name); */

    //由于该tws回调没有句柄传参，需要根据传入的name查询句柄
    struct audio_anc_lvl_sync *hdl = audio_anc_lvl_sync_get_hdl_by_name(name);
    if (!hdl) {
        lvl_sync_log("audio_anc_lvl_sync hdl NULL\n");
        return;
    }

#if TCFG_USER_TWS_ENABLE
    if (!rx) {
        audio_anc_lvl_sync_add_check(hdl, mode, -1);
    }
#endif

    switch (mode) {
    case AUDIO_ANC_LVL_SYNC_CMP:
        hdl->busy = 1;
        if (rx) {
            lvl_sync_log("AUDIO_ANC_LVL_SYNC_CMP noise_lvl %d, local_lvl %d, name %d\n", noise_lvl, hdl->local_lvl, hdl->name);
            if ((hdl->high_lvl_sync && (hdl->local_lvl <= noise_lvl)) || \
                (!hdl->high_lvl_sync && (hdl->local_lvl >= noise_lvl))) {  //同步高/低档位
                //双耳均做判断，符合条件的一边发起比较结果同步消息
                u8 tmp_data[4] = {AUDIO_ANC_LVL_SYNC_RESULT, noise_lvl, name, 0};
                audio_anc_lvl_sync_info(hdl, tmp_data, 4);
            }
        }
        hdl->busy = 0;
        break;
    case AUDIO_ANC_LVL_SYNC_RESULT:
        hdl->busy = 1;
        if (hdl->cur_lvl == noise_lvl) {
            /* lvl_sync_log("AUDIO_ANC_LVL_SYNC_RESULT same lvl %d, name %d\n", hdl->cur_lvl, hdl->name); */
            hdl->busy = 0;
            break;
        }
        lvl_sync_log("AUDIO_ANC_LVL_SYNC_RESULT noise_lvl %d send to task, name %d\n", noise_lvl, hdl->name);
        hdl->cur_lvl = noise_lvl;
        hdl->first_sync = 0;
        hdl->sync_result_cb(hdl);
        hdl->busy = 0;
        break;
    case AUDIO_ANC_LVL_SYNC_FIRST: //tws首次连接时同步档位
        hdl->busy = 1;
        lvl_sync_log("AUDIO_ANC_LVL_SYNC_FIRST %d %d\n", hdl->cur_lvl, noise_lvl);
        if ((hdl->high_lvl_sync && (hdl->cur_lvl > noise_lvl)) || \
            (!hdl->high_lvl_sync && (hdl->cur_lvl < noise_lvl))) {
            u8 tmp_data[4] = {AUDIO_ANC_LVL_SYNC_FIRST_RESULT, hdl->cur_lvl, 0, 0};
            audio_anc_lvl_sync_info(hdl, tmp_data, 4);
        }
        hdl->busy = 0;
        break;
    case AUDIO_ANC_LVL_SYNC_FIRST_RESULT:
        hdl->busy = 1;
        if (hdl->cur_lvl == noise_lvl) {
            hdl->busy = 0;
            break;
        }
        lvl_sync_log("AUDIO_ANC_LVL_SYNC_FIRST_RESULT %d %d\n", hdl->cur_lvl, noise_lvl);
        hdl->cur_lvl = noise_lvl;
        hdl->first_sync = 1;
        hdl->sync_result_cb(hdl);
        hdl->busy = 0;
        break;
    };
}

#define TWS_FUNC_ID_ANC_LVL_M2S    TWS_FUNC_ID('A', 'A', 'L', 'S')
REGISTER_TWS_FUNC_STUB(audio_anc_lvl_m2s) = {
    .func_id = TWS_FUNC_ID_ANC_LVL_M2S,
    .func    = audio_anc_lvl_sync_info_cb,
};

u8 audio_anc_lvl_sync_busy_get(struct audio_anc_lvl_sync *hdl)
{
    if (hdl) {
        return hdl->busy;
    }
    return 0;
}

void audio_anc_lvl_sync_info(struct audio_anc_lvl_sync *hdl, u8 *data, int len)
{
    if (!hdl) {
        return;
    }
    if (len < 4) {
        lvl_sync_log("audio_anc_lvl_sync_info len err\n");
        return;
    }
    data[2] = hdl->name; //name赋值，用于tws_cb查询句柄
    if (data[0] == AUDIO_ANC_LVL_SYNC_CMP) {
        hdl->local_lvl = data[1];
    }
#if TCFG_USER_TWS_ENABLE
    if (audio_anc_lvl_sync_add_check(hdl, data[0], 1)) {
        return;
    }
    if (get_tws_sibling_connect_state()) {
        int ret = tws_api_send_data_to_sibling(data, len, TWS_FUNC_ID_ANC_LVL_M2S);
    } else {
        audio_anc_lvl_sync_info_cb(data, len, 0);
    }
#else
    audio_anc_lvl_sync_info_cb(data, len, 0);
#endif
}

struct audio_anc_lvl_sync *audio_anc_lvl_sync_open(struct audio_anc_lvl_sync_param *open_param)
{
    struct audio_anc_lvl_sync *hdl = zalloc(sizeof(struct audio_anc_lvl_sync));
    if (!hdl) {
        return NULL;
    }
    hdl->sync_check = open_param->sync_check;
    hdl->high_lvl_sync = open_param->high_lvl_sync;
    hdl->cur_lvl = open_param->default_lvl;
    hdl->name = open_param->name;
    hdl->sync_result_cb = open_param->sync_result_cb;
    hdl->priv = open_param->priv;
    local_irq_disable();
    list_add(&hdl->hentry, &lvl_sync_list_hdl.head);
    local_irq_enable();
#if TCFG_USER_TWS_ENABLE
    if (get_tws_sibling_connect_state()) {
        u8 data[4] = {AUDIO_ANC_LVL_SYNC_FIRST, hdl->cur_lvl, 0, 0};
        audio_anc_lvl_sync_info(hdl, data, 4);
    }
#endif
    lvl_sync_log("audio_anc_lvl_sync_open name %d\n", hdl->name);
    return hdl;
}

void audio_anc_lvl_sync_close(struct audio_anc_lvl_sync *hdl)
{
    if (!hdl) {
        return;
    }
    while (hdl->busy) {
        os_time_dly(1);
    }
    local_irq_disable();
    list_del(&hdl->hentry);
    local_irq_enable();
    lvl_sync_log("audio_anc_lvl_sync_close name %d\n", hdl->name);
    free(hdl);
}

#endif
