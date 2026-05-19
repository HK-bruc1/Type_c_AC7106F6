#ifndef __LOCAL_TWS_H__
#define __LOCAL_TWS_H__

#include "app_main.h"

#define TWS_FUNC_ID_LOCAL_TWS	TWS_FUNC_ID('L', 'O', 'T', 'S')

enum {
    LOCAL_TWS_ROLE_NULL,				// 没有进入非蓝牙模式时候的角色
    LOCAL_TWS_ROLE_SOURCE,				// source端
    LOCAL_TWS_ROLE_SINK,				// sink端
};

// LOCAL_TWS间通讯命令
enum {
    CMD_TWS_OPEN_LOCAL_DEC_REQ = 0x1,
    CMD_TWS_OPEN_LOCAL_DEC_RSP,
    CMD_TWS_CLOSE_LOCAL_DEC_REQ,		// source通知sink关闭解码
    CMD_TWS_CLOSE_LOCAL_DEC_RSP,		// sink告诉source已关闭解码
    CMD_TWS_ENTER_SINK_MODE_REQ,		// 本地通知对端要变成sink
    CMD_TWS_ENTER_SINK_MODE_RSP,		// sink设备切到sink模式之后回复source
    CMD_TWS_BACK_TO_BT_MODE_REQ,		// 通知对方进入蓝牙模式
    CMD_TWS_BACK_TO_BT_MODE_RSP,		// 回复对方已进入蓝牙模式
    CMD_TWS_CONNECT_MODE_REPORT,		// 模式同步，在TWS连接的时候使用，根据TWS双方状态决定哪边是source和sink
    CMD_TWS_PLAYER_STATUS_REPORT,		// sink回复解码状态
    CMD_TWS_ENTER_NO_SOURCE_MODE_REPORT,// 两边都不能成为source的命令
    CMD_TWS_VOL_UP,						// sink给source发音量增大命令
    CMD_TWS_VOL_DOWN,					// sink给source发音量减小命令
    CMD_TWS_VOL_REPORT,					// source给sink同步音量
    CMD_TWS_MUSIC_PP,					// sink给source发PP命令
    CMD_TWS_MUSIC_NEXT,					// sink给source发下一首命令
    CMD_TWS_MUSIC_PREV,					// sink给source发上一首命令
};

typedef struct _local_tws_info {
    u8 role;
    u8 sync_goto_bt_mode;
    u8 cmd_record;                       //用于记录上一条命令
    volatile u32 cmd_timestamp;          //用于记录命令的时间戳
    const char *sync_tone_name;          //记录同步播放的提示音
    u32 timer;                           //用于音量同步
    void *priv;                          //回调给local_audio_open的参数
    volatile u8 remote_dec_status;       //对方是否开启local_tws_dec
    u8 local_tws_mode_en;
} local_tws_info;

struct local_tws_mode_ops {
    enum app_mode_t name;
    void (*local_audio_open)(void *priv);
    bool (*get_play_status)(void);
};

#define REGISTER_LOCAL_TWS_OPS(local_tws_ops) \
    struct local_tws_mode_ops  __##local_tws_ops sec(.local_tws)

extern struct local_tws_mode_ops   local_tws_ops_begin[];
extern struct local_tws_mode_ops   local_tws_ops_end[];

#define list_for_each_local_tws_ops(ops) \
    for (ops = local_tws_ops_begin; ops < local_tws_ops_end; ops++)

/**
 * @brief local_tws初始化
 */
void local_tws_init(void);

/**
 * @brief 在每个模式init调用, 切到蓝牙模式通知对端设备切回蓝牙，非蓝牙模式则通知对端设备进行Sink模式
 *
 * @param file_name 当前模式提示音文件名
 * @param priv 提示音播放完之后回调本地音乐解码时的参数
 *
 * @return 0：表示执行成功  -1：tws未连接，走原本模式的流程
 */
int local_tws_enter_mode(const char *file_name, void *priv);

/**
 * @brief 在每个模式exit调用, 用于通知对方关闭本地解码
 */
void local_tws_exit_mode(void);

/**
 * @brief 用于同步模式，TWS连接的时候调用，会根据当前双方所在的模式决定哪端切到source和sink，
 * 如果都在蓝牙模式则不进行切换
 */
void local_tws_connect_mode_report(void);

/**
 * @brief tws断开的时候调用，tws断开，Sink设备需要返回蓝牙模式
 */
void local_tws_disconnect_deal(void);

/**
 * @brief Sink端进入Sink模式时回复Source端的接口
 */
void local_tws_enter_sink_mode_rsp(enum app_mode_t mode);

/**
 * @brief 用于回复Source端关闭本地解码的消息
 */
void local_tws_close_dec_rsp(void);

/**
 * @brief 用于获取Sink端本地解码状态
 */
u8 local_tws_get_remote_dec_status(void);

/**
 * @brief local_tws 音量调节接口，Sink设备调用，Sink设备调节音量并不会调节本地音量，
 * 通过发送命令调节Source端的音量值
 *
 * @param operate CMD_TWS_VOL_UP, CMD_TWS_VOL_DOWN
 */
void local_tws_vol_operate(u8 operate);

/**
 * @brief local_tws 音乐上下曲接口，Sink设备调用，Sink设备无法自行控制音乐上下曲，
 * 通过发送命令让Source端切换上下曲
 *
 * @param operate APP_MSG_MUSIC_PP, APP_MSG_MUSIC_NEXT, APP_MSG_MUSIC_PREV
 */
void local_tws_music_operate(u8 operate, void *arg);

/**
 * @brief 用于音量同步
 */
void local_tws_sync_vol(void);

/**
 * @brief 用于获取当前的LOCAL_TWS的角色
 *
 * @return LOCAL_TWS_ROLE
 */
u8 local_tws_get_role(void);

/**
 * @brief 判断是否退出LOCAL_TWS source模式
 *
 * @result 1:退出; 0:source模式
 */
u8 local_tws_mode_exit();

/**
 * @brief local_tws不开蓝牙后台，进入蓝牙模式的处理
 */
void local_tws_without_background_enter_bt_mode(void);

/**
 * @brief local_tws不开蓝牙后台，退出蓝牙模式的处理
 */
void local_tws_without_background_exit_bt_mode(void);

#endif

