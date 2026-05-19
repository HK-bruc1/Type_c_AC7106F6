#ifndef _AUDIO_ANC_lvl_SYNC_
#define _AUDIO_ANC_lvl_SYNC_

#include "generic/typedef.h"
#include "icsd_adt_app.h"

#define AUDIO_ANC_LVL_SYNC_CMP          BIT(0)
#define AUDIO_ANC_LVL_SYNC_RESULT       BIT(1)
#define AUDIO_ANC_LVL_SYNC_FIRST        BIT(2)
#define AUDIO_ANC_LVL_SYNC_FIRST_RESULT BIT(3)

/*
 * 新加的模块
 * 需要在这里添加name，在open时传参
 */
enum anc_lvl_sync_name {
    ANC_LVL_SYNC_WIND = 0,
    ANC_LVL_SYNC_ENV,
    ANC_LVL_SYNC_AVC,
    COMMON_LVL_SYNC_AVC, //通用avc模块，不依赖anc
};

struct audio_anc_lvl_sync_param {
    void (*sync_result_cb)(void *);
    void *priv;
    u8 sync_check;      //实时性检查，某些tws消息同步有实时性要求
    u8 high_lvl_sync;   //1:tws对齐高等级 0:对齐低等级
    u8 default_lvl;     //初始设置等级
    u8 name;
};

struct audio_anc_lvl_sync {
    struct list_head hentry;
    void (*sync_result_cb)(void *);
    void *priv;
    u8 sync_check;      //实时性检查，某些tws消息同步有实时性要求
    u8 high_lvl_sync;   //1:tws对齐高等级 0:对齐低等级
    s8 sync_cnt[2];     //0:cmp 1:result
    u8 cur_lvl;         //当前生效等级
    u8 local_lvl;       //本地检测等级
    u8 name;            //句柄链表管理标识
    u8 first_sync;
    u8 busy;
};

struct audio_anc_lvl_sync *audio_anc_lvl_sync_get_hdl_by_name(u8 name);
int audio_anc_lvl_sync_list_init();
u8 audio_anc_lvl_sync_busy_get(struct audio_anc_lvl_sync *hdl);
void audio_anc_lvl_sync_info(struct audio_anc_lvl_sync *hdl, u8 *data, int len); //0:mode 1:noise_lvl 2:name
struct audio_anc_lvl_sync *audio_anc_lvl_sync_open(struct audio_anc_lvl_sync_param *open_param);
void audio_anc_lvl_sync_close(struct audio_anc_lvl_sync *hdl);



#endif

