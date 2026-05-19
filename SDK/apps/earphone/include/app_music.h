#ifndef CONFIG_APP_MUSIC_H
#define CONFIG_APP_MUSIC_H

#include "system/includes.h"
#include "app_main.h"
#include "music/breakpoint.h"
#include "effect/effects_default_param.h"

#if (TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0)
#define MUSIC_DEV_ONLINE_START_AFTER_MOUNT_EN			0//如果是u盘和SD卡复用， 这里必须为0， 保证usb枚举的时候解码是停止的
#else
#define MUSIC_DEV_ONLINE_START_AFTER_MOUNT_EN			1
#endif

/**
 * @brief   模式参数结构体
 */
struct __music_task_parm {
    u8 type;
    int val;
};

/**
 * @brief   music模式控制结构体
 */
struct __music {
    struct __music_task_parm task_parm;
    u16 file_err_counter;//错误文件统计
    u8 file_play_direct;//0:下一曲， 1：上一曲
    u8 scandisk_break;//扫描设备打断标志
    char device_tone_dev[16];
    struct __breakpoint *breakpoint;
    struct music_player *player_hd;
    enum _speed_level speed_mode;
    enum _pitch_level pitch_mode;
    u8 music_busy;
    u8 scandisk_mark;//扫描设备标志
    u16 timer;
};
/**
 * @brief   music模式首次播放方式
 */
enum {
    MUSIC_TASK_START_BY_NORMAL = 0x0,
    MUSIC_TASK_START_BY_BREAKPOINT,
    MUSIC_TASK_START_BY_SCLUST,
    MUSIC_TASK_START_BY_NUMBER,
    MUSIC_TASK_START_BY_PATH,
};

/**
 * @brief   music模式设备上线启动播放
 * @param   in_logo 设备名
 * @return  none
 */
void music_task_dev_online_start(char *in_logo);
/**
 * @brief   music模式切入前参数设置
 * @param   type 播放方式，如MUSIC_TASK_START_BY_NORMAL
 * @param   val 播放参数
 * @return  none
 */
void music_task_set_parm(u8 type, int val);
/**
 * @brief   music模式播放失败处理
 * @param   err 播放失败的错误号
 * @return  none
 */
void music_player_err_deal(int err);
/**
 * @brief   music模式获取当前设备名
 * @return  设备名
 */
char *music_app_get_dev_cur(void);
/**
 * @brief   music模式获取当前断点地址
 * @return  断点信息结构体
 */
struct audio_dec_breakpoint *music_app_get_dbp_addr(void);
/**
 * @brief   music模式APP_MSG消息处理
 * @param   msg 消息，如APP_MSG_GOTO_NEXT_MODE
 * @return  固定返回0
 */
int music_app_msg_handler(int *msg);
/**
 * @brief   进入music模式
 * @param   arg 附加参数
 * @return  下一个要进入的模式
 */
struct app_mode *app_enter_music_mode(int arg);
/**
 * @brief   获取music模式当前播放的句柄
 * @return  music_player句柄
 */
struct music_player *music_app_get_cur_hdl(void);
/**
 * @brief   music模式设备消息处理
 * @param   msg 消息，如DRIVER_EVENT_FROM_SD0
 * @return  固定返回0
 */
int music_device_msg_handler(int *msg);
/**
 * @brief   music模式获取设备提示音文件路径
 * @param   logo 设备名称
 * @return  设备提示音文件路径
 */
const char *get_music_tone_name_by_logo(const char *logo);
/**
 * @brief   music模式播放设备上线提示音
 * @param   logo 设备名称
 * @return  true 成功
 *          false 失败
 */
int music_device_tone_play(char *logo);

#endif

