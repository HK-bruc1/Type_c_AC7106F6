#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".key_tone.data.bss")
#pragma data_seg(".key_tone.data")
#pragma const_seg(".key_tone.text.const")
#pragma code_seg(".key_tone.text")
#endif
#include "fs/resfile.h"
#include "app_main.h"
#include "app_tone.h"
#include "key_driver.h"

#if TCFG_KEY_TONE_NODE_ENABLE

static u8 g_have_key_tone_file = 0;

static bool is_key_tone_enable()
{
    if (g_have_key_tone_file == 0) {
        char file_path[48];
        strcpy(file_path, FLASH_RES_PATH);
        strcpy(file_path + strlen(FLASH_RES_PATH), get_tone_files()->key_tone);
        void *file = resfile_open(file_path);
        if (file) {
            g_have_key_tone_file = 1;
            resfile_close(file);
        } else {
            g_have_key_tone_file = 0xff;
        }
    }
    return g_have_key_tone_file == 1 ? true : false;
}

static int key_tone_msg_handler(int *msg)
{
    if (!is_key_tone_enable()) {
        return 0;
    }
    if (msg[0] == APP_MSG_KEY_TONE) {
        play_key_tone_file(get_tone_files()->key_tone);
    }
    return 0;
}
APP_MSG_HANDLER(key_tone_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_APP,
    .handler    = key_tone_msg_handler,
};
#endif


#if TCFG_IOKEY_ENABLE
void key_down_event_handler(u8 key_value)
{
#if TCFG_KEY_TONE_NODE_ENABLE
    if (g_have_key_tone_file == 1) {
        app_send_message(APP_MSG_KEY_TONE, 0);
    }
#endif
}
#endif

#if TCFG_LP_TOUCH_KEY_ENABLE
void touch_key_send_key_tone_msg(void)
{
#if TCFG_KEY_TONE_NODE_ENABLE
    if (g_have_key_tone_file == 1) {
        app_send_message(APP_MSG_KEY_TONE, 0);
    }
#endif
}
#endif
