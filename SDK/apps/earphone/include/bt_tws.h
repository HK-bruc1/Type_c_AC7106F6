#ifndef APP_BT_TWS_H
#define APP_BT_TWS_H

#include "classic/tws_api.h"
#include "classic/tws_event.h"
#include "system/includes.h"

#define TWS_FUNC_ID_VBAT_SYNC       TWS_FUNC_ID('V', 'B', 'A', 'T')
#define TWS_FUNC_ID_CHARGE_SYNC     TWS_FUNC_ID('C', 'H', 'G', 'S')
#define TWS_FUNC_ID_BOX_SYNC        TWS_FUNC_ID('B', 'O', 'X', 'S')
#define TWS_FUNC_ID_AI_DMA_RAND     TWS_FUNC_ID('A', 'I', 'D', 'M')
#define TWS_FUNC_ID_AI_SPEECH_STOP  TWS_FUNC_ID('A', 'I', 'S', 'T')
#define TWS_FUNC_ID_APP_MODE  		TWS_FUNC_ID('M', 'O', 'D', 'E')
#define TWS_FUNC_ID_AI_SYNC			TWS_FUNC_ID('A', 'I', 'P', 'A')
#define TWS_FUNC_ID_EAR_DETECT_SYNC	TWS_FUNC_ID('E', 'D', 'E', 'T')
#define TWS_FUNC_ID_LL_SYNC_STATE	TWS_FUNC_ID('L', 'L', 'S', 'S')
#define TWS_FUNC_ID_TUYA_STATE      TWS_FUNC_ID('T', 'U', 'Y', 'A')

#define    BT_TWS_UNPAIRED                      0x0001	// TWS未配对
#define    BT_TWS_PAIRED                        0x0002	// TWS已配对
#define    BT_TWS_WAIT_SIBLING_SEARCH           0x0004	// TWS等待配对
#define    BT_TWS_SEARCH_SIBLING                0x0008	// TWS搜索对端设备中
#define    BT_TWS_CONNECT_SIBLING               0x0010	// TWS连接对端设备
#define    BT_TWS_SIBLING_CONNECTED             0x0020	// TWS连接对端设备成功
#define    BT_TWS_PHONE_CONNECTED               0x0040	// TWS设备已连接手机
#define    BT_TWS_POWER_ON                      0x0080	// TWS已初始化
#define    BT_TWS_TIMEOUT                       0x0100	// TWS连接对端设备超时
#define    BT_TWS_AUDIO_PLAYING                 0x0200  // TWS音频播放

enum {
    SYNC_CMD_EARPHONE_CHAREG_START,		// 开始充电
    SYNC_CMD_IRSENSOR_EVENT_NEAR,		// 近耳
    SYNC_CMD_IRSENSOR_EVENT_FAR,		// 出耳
};

enum {
    SYNC_TONE_ANC_ON,
    SYNC_TONE_ANC_OFF,
    SYNC_TONE_ANC_TRANS,
    SYNC_TONE_EARDET_IN,
};

/**
 * @brief 设置TWS设备的搜索状态
 */
void tws_set_search_sbiling_state(u8 state);

/**
 * @brief 获取TWS是否配对
 *
 * @return true:已配对 false:未配对
 */
bool bt_tws_is_paired();

/**
 * @brief 获取左右声道信息
 *
 * @return 'L': 左声道; 'R': 右声道; 'U': 未知
 */
char bt_tws_get_local_channel();

/**
 * @brief 获取TWS连接状态
 *
 * @return 如BT_TWS_UNPAIRED
 */
u16 bt_tws_get_state(void);

/**
 * @brief TWS初始化
 */
int bt_tws_poweron();

/**
 * @brief TWS功能关闭退出
 */
int bt_tws_poweroff();

/**
 * @brief 设置TWS设备连接手机状态信息
 */
int bt_tws_phone_connected();

/**
 * @brief 获取TWS是否已连接对端设备
 *
 * @return 1:已连接对端设备；0:未连接对端设备
 */
int get_bt_tws_connect_status();
bool get_tws_sibling_connect_state(void);

/**
 * @brief TWS同步播歌、通话音量
 */
void bt_tws_sync_volume();

/**
 * @brief TWS取消对端设备的连接
 */
void tws_cancle_all_noconn();

/**
 * @brief TWS按键消息同步
 *
 * @param key_msg 按键消息
 */
void bt_tws_key_msg_sync(int key_msg);

/**
 * @brief TWS状态消息处理函数
 *
 * @param msg TWS状态消息
 */
int bt_tws_connction_status_event_handler(int *msg);

/**
 * @brief 解除TWS配对，清掉对方地址信息和本地声道信息
 */
void bt_tws_remove_pairs();

/**
 * @brief TWS进入dut测试使用
 */
void bt_page_scan_for_test(u8 inquiry_en);

/**
 * @brief TWS通讯关闭进入sniff检查定时器
 */
void tws_sniff_controle_check_disable(void);

/**
 * @brief TWS通讯开启进入sniff检查定时器
 */
void tws_sniff_controle_check_enable(void);

/**
 * @brief 设置底层TWS任务间隔
 *
 * @param tws_task_interval
 */
void set_tws_task_interval(int tws_task_interval);

/**
 * @brief TWS配对成功
 *
 * @param remote_addr TWS远端地址
 */
void bt_tws_set_pair_suss(const u8 *remote_addr);

int tws_pair_by_chip_conn_init();
/**
 * @brief TWS通过jldtp数据传输协议配对流程
 *
 * @return 0
 */
int tws_start_pair_by_chip_conn();

/**
 * @brief TWS取消通过jldtp数据传输协议配对流程
 */
void tws_stop_pair_by_chip_conn();

/**
 * @brief 获取TWS本地地址
 */
void bt_get_tws_local_addr(u8 *addr);

#endif
