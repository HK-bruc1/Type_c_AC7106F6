#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".music_key_msg_table.data.bss")
#pragma data_seg(".music_key_msg_table.data")
#pragma const_seg(".music_key_msg_table.text.const")
#pragma code_seg(".music_key_msg_table.text")
#endif
#include "key_driver.h"
#include "app_main.h"
#include "init.h"


#if TCFG_ADKEY_ENABLE
//短按                  //长按                  //hold                       //长按抬起                  //双击                       //三击
const int key_music_ad_num0_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_GOTO_NEXT_MODE,       APP_MSG_KEY_POWER_OFF,  APP_MSG_NULL,   APP_MSG_NULL,   APP_MSG_MUSIC_CHANGE_DEV,   APP_MSG_MUSIC_CHANGE_EQ,
};
const int key_music_ad_num1_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PP,     APP_MSG_VOL_UP,         APP_MSG_VOL_UP,         APP_MSG_NULL,           APP_MSG_MUSIC_PLAYE_NEXT_FOLDER,     APP_MSG_NULL,
};
const int key_music_ad_num2_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_NEXT,     APP_MSG_VOL_DOWN,       APP_MSG_VOL_DOWN,       APP_MSG_NULL,           APP_MSG_MUSIC_PLAYE_PREV_FOLDER,         APP_MSG_NULL,
};
const int key_music_ad_num3_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PREV,    APP_MSG_NULL,           APP_MSG_NULL,           APP_MSG_NULL,           APP_MSG_VOCAL_REMOVE,   APP_MSG_NULL,
};
const int key_music_ad_num4_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_CHANGE_REPEAT,    APP_MSG_NULL,       APP_MSG_MUSIC_FF,   APP_MSG_NULL,        APP_MSG_NULL,       APP_MSG_NULL,
};
const int key_music_ad_num5_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PLAYER_AB_REPEAT_SWITCH,     APP_MSG_NULL,       APP_MSG_MUSIC_FR,   APP_MSG_NULL,  APP_MSG_SWITCH_SOUND_EFFECT, APP_MSG_NULL,
};
const int key_music_ad_num6_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_SWITCH_MIC_EFFECT,     APP_MSG_NULL,       APP_MSG_NULL,   APP_MSG_NULL, APP_MSG_MIC_EFFECT_ON_OFF, APP_MSG_NULL,
};
const int key_music_ad_num7_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_VOCAL_REMOVE,   APP_MSG_NULL,       APP_MSG_NULL,       APP_MSG_NULL,        APP_MSG_NULL,       APP_MSG_NULL,
};
const int key_music_ad_num8_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,       APP_MSG_NULL,       APP_MSG_NULL,   APP_MSG_NULL,        APP_MSG_NULL,       APP_MSG_NULL,
};
const int key_music_ad_num9_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     APP_MSG_NULL,       APP_MSG_NULL,   APP_MSG_NULL,     APP_MSG_NULL,       APP_MSG_NULL,
};
#endif

#if TCFG_IOKEY_ENABLE
//短按                  //长按                  //hold                       //长按抬起                  //双击                       //三击
const int key_music_io_num0_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PP,       APP_MSG_KEY_POWER_OFF,  APP_MSG_NULL,   APP_MSG_NULL,   APP_MSG_MUSIC_CHANGE_DEV,   APP_MSG_VOCAL_REMOVE,
};
const int key_music_io_num1_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_NEXT,     APP_MSG_VOL_UP,         APP_MSG_VOL_UP,         APP_MSG_NULL,           APP_MSG_MUSIC_PLAYE_NEXT_FOLDER,     APP_MSG_NULL,
};
const int key_music_io_num2_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PREV,     APP_MSG_VOL_DOWN,       APP_MSG_VOL_DOWN,       APP_MSG_NULL,           APP_MSG_MUSIC_PLAYE_PREV_FOLDER,         APP_MSG_NULL,
};
const int key_music_io_num3_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_CHANGE_MODE,    APP_MSG_KEY_POWER_OFF,      APP_MSG_NULL,     APP_MSG_NULL,    APP_MSG_NULL,   APP_MSG_NULL,
};
#endif

const struct key_remap_table music_mode_key_table[] = {
#if TCFG_ADKEY_ENABLE
    { .key_value = KEY_AD_NUM0, .remap_table = key_music_ad_num0_msg_table },
    { .key_value = KEY_AD_NUM1, .remap_table = key_music_ad_num1_msg_table },
    { .key_value = KEY_AD_NUM2, .remap_table = key_music_ad_num2_msg_table },
    { .key_value = KEY_AD_NUM3, .remap_table = key_music_ad_num3_msg_table },
    //{ .key_value = KEY_AD_NUM4, .remap_table = key_music_ad_num4_msg_table },
    //{ .key_value = KEY_AD_NUM5, .remap_table = key_music_ad_num5_msg_table },
    //{ .key_value = KEY_AD_NUM6, .remap_table = key_music_ad_num6_msg_table },
    //{ .key_value = KEY_AD_NUM7, .remap_table = key_music_ad_num7_msg_table },
    //{ .key_value = KEY_AD_NUM8, .remap_table = key_music_ad_num8_msg_table },
    //{ .key_value = KEY_AD_NUM9, .remap_table = key_music_ad_num9_msg_table },
#endif
#if TCFG_IOKEY_ENABLE
    { .key_value = KEY_POWER,   .remap_table = key_music_io_num0_msg_table },
    { .key_value = KEY_NEXT,    .remap_table = key_music_io_num1_msg_table },
    { .key_value = KEY_PREV,    .remap_table = key_music_io_num2_msg_table },
    { .key_value = KEY_MODE,    .remap_table = key_music_io_num3_msg_table },
#endif
    { .key_value = 0xff }
};

