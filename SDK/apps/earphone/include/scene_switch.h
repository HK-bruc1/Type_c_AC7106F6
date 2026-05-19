#ifndef __SCENE_SWITCH_H_
#define __SCENE_SWITCH_H_

#include "system/includes.h"
#include "media/includes.h"

/* 需要根据 music_mode 数组进行排列 */
enum {
    BT_MODE,
    AUX_MODE,
    FILE_MODE,
    FM_MODE,
    SPDIF_MODE,
    PC_MODE,
    NOT_SUPPORT_MODE,
};

/* 获取当前场景序号 */
u8 get_current_scene();
/* 获取mic当前场景序号 */
u8 get_mic_current_scene();
/* 设置默认场景序号 */
void set_default_scene(u8 index);

/* 获取EQ0配置序号 */
u8 get_music_eq_preset_index(void);
void set_music_eq_preset_index(u8 index);

/* 根据参数组序号进行场景切换 */
void effect_scene_set(u8 scene);

/* 根据参数组个数顺序切换场景 */
void effect_scene_switch();

void music_vocal_remover_switch(void);

void music_vocal_remover_update_parm();
u8 get_music_vocal_remover_statu(void);

#endif

