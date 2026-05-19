#include "key_driver.h"
#include "app_main.h"
#include "app_config.h"


#if TCFG_ADKEY_ENABLE
//短按                  //长按                  //hold                       //长按抬起                  //双击                       //三击
const int key_pc_ad_num0_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_GOTO_NEXT_MODE,     		APP_MSG_KEY_POWER_OFF,           APP_MSG_NULL,           APP_MSG_NULL,            APP_MSG_GOTO_NEXT_MODE,        APP_MSG_NULL,
};
const int key_pc_ad_num1_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PP,     		APP_MSG_VOL_UP,       	APP_MSG_VOL_UP,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num2_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_NEXT,     		APP_MSG_VOL_DOWN,       	APP_MSG_VOL_DOWN,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num3_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PREV,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num4_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num5_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num6_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num7_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num8_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_ad_num9_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
#endif

#if TCFG_MICKEY_ENABLE
const int key_pc_mic_num0_msg_table[KEY_ACTION_MAX] = {
    /* 按下     */ [KEY_ACTION_DOWN]            = APP_MSG_PP_KEY_PRESS,
    /* 短按抬起 */ [KEY_ACTION_CLICK]           = APP_MSG_PP_KEY_LIFT,
    /* 长按     */ [KEY_ACTION_LONG]            = APP_MSG_NULL,
    /* 按住     */ [KEY_ACTION_HOLD]            = APP_MSG_NULL,
    /* 长按抬起 */ [KEY_ACTION_UP]              = APP_MSG_PP_KEY_LIFT,
    /* 双击     */ [KEY_ACTION_DOUBLE_CLICK]    = APP_MSG_MUSIC_NEXT,
    /* 三击     */ [KEY_ACTION_TRIPLE_CLICK]    = APP_MSG_MUSIC_PREV,
};
const int key_pc_mic_num1_msg_table[KEY_ACTION_MAX] = {
    /* 按下     */ [KEY_ACTION_DOWN]            = APP_MSG_UP_KEY_PRESS,
    /* 短按抬起 */ [KEY_ACTION_CLICK]           = APP_MSG_UP_KEY_LIFT,
    /* 长按     */ [KEY_ACTION_LONG]            = APP_MSG_NULL,
    /* 按住     */ [KEY_ACTION_HOLD]            = APP_MSG_NULL,
    /* 长按抬起 */ [KEY_ACTION_UP]              = APP_MSG_UP_KEY_LIFT,
    /* 双击     */ [KEY_ACTION_DOUBLE_CLICK]    = APP_MSG_NULL,
    /* 三击     */ [KEY_ACTION_TRIPLE_CLICK]    = APP_MSG_NULL,
};
const int key_pc_mic_num2_msg_table[KEY_ACTION_MAX] = {
    /* 按下     */ [KEY_ACTION_DOWN]            = APP_MSG_DOWN_KEY_PRESS,
    /* 短按抬起 */ [KEY_ACTION_CLICK]           = APP_MSG_DOWN_KEY_LIFT,
    /* 长按     */ [KEY_ACTION_LONG]            = APP_MSG_NULL,
    /* 按住     */ [KEY_ACTION_HOLD]            = APP_MSG_NULL,
    /* 长按抬起 */ [KEY_ACTION_UP]              = APP_MSG_DOWN_KEY_LIFT,
    /* 双击     */ [KEY_ACTION_DOUBLE_CLICK]    = APP_MSG_NULL,
    /* 三击     */ [KEY_ACTION_TRIPLE_CLICK]    = APP_MSG_NULL,
};
const int key_pc_mic_num3_msg_table[KEY_ACTION_MAX] = {
    /* 按下     */ [KEY_ACTION_DOWN]            = APP_MSG_NULL,
    /* 短按抬起 */ [KEY_ACTION_CLICK]           = APP_MSG_NULL,
    /* 长按     */ [KEY_ACTION_LONG]            = APP_MSG_NULL,
    /* 按住     */ [KEY_ACTION_HOLD]            = APP_MSG_NULL,
    /* 长按抬起 */ [KEY_ACTION_UP]              = APP_MSG_NULL,
    /* 双击     */ [KEY_ACTION_DOUBLE_CLICK]    = APP_MSG_NULL,
    /* 三击     */ [KEY_ACTION_TRIPLE_CLICK]    = APP_MSG_NULL,
};
#endif

#if TCFG_IOKEY_ENABLE
//短按                //长按                  //hold                       //长按抬起                   //双击                       //三击
const int key_pc_io_num0_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PP,     		APP_MSG_KEY_POWER_OFF,           APP_MSG_NULL,           APP_MSG_NULL,         	 APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num1_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_NEXT,     		APP_MSG_VOL_UP,       	APP_MSG_VOL_UP,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num2_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_MUSIC_PREV,     		APP_MSG_VOL_DOWN,       	APP_MSG_VOL_DOWN,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num3_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_GOTO_NEXT_MODE,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num4_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num5_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num6_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num7_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num8_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
const int key_pc_io_num9_msg_table[KEY_ACTION_MAX] = {
    APP_MSG_NULL,     		APP_MSG_NULL,       	APP_MSG_NULL,       	APP_MSG_NULL,            APP_MSG_NULL,           APP_MSG_NULL,
};
#endif

const struct key_remap_table pc_mode_key_table[] = {
#if TCFG_ADKEY_ENABLE
    { .key_value = KEY_AD_NUM0, .remap_table = key_pc_ad_num0_msg_table },
    { .key_value = KEY_AD_NUM1, .remap_table = key_pc_ad_num1_msg_table },
    { .key_value = KEY_AD_NUM2, .remap_table = key_pc_ad_num2_msg_table },
    { .key_value = KEY_AD_NUM3, .remap_table = key_pc_ad_num3_msg_table },
    //{ .key_value = KEY_AD_NUM4, .remap_table = key_pc_ad_num4_msg_table },
    //{ .key_value = KEY_AD_NUM5, .remap_table = key_pc_ad_num5_msg_table },
    //{ .key_value = KEY_AD_NUM6, .remap_table = key_pc_ad_num6_msg_table },
    //{ .key_value = KEY_AD_NUM7, .remap_table = key_pc_ad_num7_msg_table },
    //{ .key_value = KEY_AD_NUM8, .remap_table = key_pc_ad_num8_msg_table },
    //{ .key_value = KEY_AD_NUM9, .remap_table = key_pc_ad_num9_msg_table },
#endif
#if TCFG_IOKEY_ENABLE
    { .key_value = KEY_POWER,   .remap_table = key_pc_io_num0_msg_table },
    { .key_value = KEY_NEXT,    .remap_table = key_pc_io_num1_msg_table },
    { .key_value = KEY_PREV,    .remap_table = key_pc_io_num2_msg_table },
    { .key_value = KEY_MODE,    .remap_table = key_pc_io_num3_msg_table },
#endif

#if TCFG_MICKEY_ENABLE
    { .key_value = KEY_MIC_NUM0, .remap_table = key_pc_mic_num0_msg_table },
    { .key_value = KEY_MIC_NUM1, .remap_table = key_pc_mic_num1_msg_table },
    { .key_value = KEY_MIC_NUM2, .remap_table = key_pc_mic_num2_msg_table },
    { .key_value = KEY_MIC_NUM2, .remap_table = key_pc_mic_num2_msg_table },
#endif
    { .key_value = 0xff }
};


