#ifndef _REALME_PLATFORM_API_H_
#define _REALME_PLATFORM_API_H_

#include "system/includes.h"

extern struct __realme_info *user_realme_info;

// app api
int realme_message_callback_handler(u8 *remote_addr, int id, int opcode, u8 *data, u32 len);
u8  realme_platform_feature_default_state(u8 feature_id);
void realme_remote_name_callback(u8 status, u8 *addr, u8 *name);
void realme_remote_connect_complete_callback(u8 *addr);
void realme_ota_breakpoint_init(void);
u8 realme_platform_get_battery_left(void);
u8 realme_platform_get_battery_right(void);
u8 realme_platform_get_battery_box(void);
int realme_tws_sync_state_send(void);
int realme_check_upgrade_area(int update);

// lib api
int  realme_connected_num_get(void);
int  realme_connected_addr_get(u8 *addr);
int  realme_disconnect(void *addr);
int  realme_update_tws_state_to_lib(int state);
void realme_info_register(struct __realme_info *ptr);
void realme_product_test_set(uint8_t enable, int (*func)(uint8_t *buf, uint16_t len));
void realme_message_callback_register(int (*handler)(u8 *remote_addr, int id, int opcode, u8 *data, u32 len));
void realme_is_tws_master_callback_register(bool (*handler)(void));
int realme_tws_sync_data_recv(void *priv, u8 *data, u32 len);
int realme_tws_sync_state_manually(void);
void realme_set_ota_delay_ms(u16 delay_ms);
int  realme_all_init(u8 melody_id, u8 realme_link_id);
int  realme_all_exit();
int  realme_ble_adv_enable(u8 enable);
int  realme_ble_set_adv_interval(u32 adv_interval);
int  realme_ble_disconnect(void);
void realme_debug_log_param_set(u32 frame_size, u32 timer_ms);
void realme_get_host_utc_time(u8 *remote_addr);
int  dual_bank_erase_other_bank(u8 opt);
int  realme_check_other_upgrade_bank(void);
void realme_app_set_local_utc_time(u32 time_stamp);
/*
 realme_send_operation_to_host

 operation:
     1 = 打开相机应用
     2 = 执行拍照动作（此行为前提是相机应用已经打开）
     3 = 切换到上一个模式(比如视频模式)
     4 = 切换到下一个模式(比如照片模式)
     5 = 通知手机响铃或者震动
     6 = 关闭手机响铃或者震动
*/
void realme_send_operation_to_host(u8 *remote_addr, u8 operation);

/*
    realme_event_notify

    上层状态改变需要发送消息通知手机，使用这个函数
    当 remote_addr 传 NULL 时，发给所有连接的手机
*/
#define OPPO_EVENT_CODE_BATTERY_CHANGE                  0x01
#define OPPO_EVENT_CODE_EARBUDS_CHANGE                  0x02
#define OPPO_EVENT_CODE_NOISE_REDUCTION_LEVEL_CHANGE    0x03
#define OPPO_EVENT_CODE_COMPACTNESS_DETECTION_COMPLETE  0x04
#define OPPO_EVENT_CODE_GAME_MODE_CHANGE                0x05
#define OPPO_EVENT_CODE_MULTI_CONN_STATE_CHANGE         0x06
#define OPPO_EVENT_CODE_USER_UI_INTERACTION             0xF1
#define OPPO_EVENT_CODE_CONN_DEV_INFO                   0xF2
#define OPPO_EVENT_CODE_DEVICES_EXCEPTION_INFO          0xF3
#define OPPO_EVENT_CODE_DEVICE_BURIED_POINT             0xF4
void realme_event_notify(uint8_t *remote_addr, uint8_t event_type, uint8_t *data, uint16_t len);


/*
    realme_upgrade_error

    升级中途有状态改变，需要中断 OTA，调用这个接口
    upgrade error code 见下表
*/
#define REALME_STATE_SUCCESS                   (0x00)    // 成功
#define REALME_STATE_FAILURE                   (0x01)    // 失败
#define REALME_STATE_INVALID_PARAMETER         (0x02)
#define REALME_STATE_VERIFICATION_FAILURE      (0x03)    // BLOCK 校验失败
#define REALME_STATE_VALIDATION_FAILURE        (0x04)    // 整个固件校验失败
#define REALME_STATE_TIME_OUT_WITHOUT_ACK      (0x05)    // 没有收到 response 超时
#define REALME_STATE_TIME_OUT                  (0x06)    // 期待某个命令超时失败。
#define REALME_STATE_WRITTING_FAILURE          (0x07)    // Flash 写失败
#define REALME_STATE_FAILURE_PLAYING_MUSIC     (0x08)    // 由于正在播放音乐,返回失败
#define REALME_STATE_FAILURE_CALLING           (0x09)    // 由于正在通话,返回失败
#define REALME_STATE_TWS_IS_DISCONNECTED       (0x0A)    // 双耳之间未连接
#define REALME_STATE_DONT_WEAR_EARPHONES       (0x0B)    // 双耳没有同时佩戴
#define REALME_STATE_VERSION_ERROR             (0x0C)    // 双耳之间版本不支持
#define REALME_STATE_FAILURE_DUE_TO_NOISE      (0x0D)    // 由于环境嘈杂， 返回检测失败
#define REALME_STATE_DONT_WEAR_ANY_EARPHONES   (0x0E)    // 没有佩戴任意耳机
#define REALME_STATE_DURING_PROCESS            (0x0F)    // 表示当前操作正在进行中
#define REALME_STATE_BATTERY_IS_LOW            (0x10)    // 表示电量低
#define REALME_STATE_FAIL_AND_RETRY            (0x11)    // 失败， 且耳机请求重发对应命令
#define REALME_STATE_REQUEST_FAILURE           (0x12)    // 由于耳机被另一个手机占用导致请求失败（ 比如： OTA 请求失败）
#define REALME_STATE_FAIL_AND_RETRANSFER       (0x13)    // 失败， 且耳机请求从特定的偏移量重传
void realme_upgrade_error(u8 error_code);

/*
    realme_upgrade_request

    耳机需要请求手机发起升级流程，调用这个接口
    upgrade type 见下表
*/
#define UPGRADE_TYPE_EARPHONE       (0x01) // 升级耳机固件
#define UPGRADE_TYPE_CHARGE_BOX     (0x02) // 升级充电盒固件
#define UPGRADE_TYPE_TONE_CHAN      (0x03) // 升级耳机禅定声音文件
#define UPGRADE_TYPE_TONE           (0x04) // 升级耳机提示音文件
void realme_upgrade_request(uint8_t *remote_addr, u8 type);


/*
    realme debug ear side
*/
#define REALME_DEBUG_SIDE_LEFT      (0x01) // 左耳
#define REALME_DEBUG_SIDE_RIGHT     (0x02) // 右耳
#define REALME_DEBUG_SIDE_BOX       (0x03) // 盒子
#define REALME_DEBUG_SIDE_LR        (0x04) // 左右耳


/*
    realme debug level
*/
#define REALME_DEBUG_LEVEL_OFF      (0x00) // 最高级别，用于关闭日志
#define REALME_DEBUG_LEVEL_ERROR    (0x01) // 指明错误事件，但应用可能还能继续运行
#define REALME_DEBUG_LEVEL_WARN     (0x02) // 指明潜在的有害状况
#define REALME_DEBUG_LEVEL_INFO     (0x03) // 指明描述信息，从粗粒度上描述了应用运行过程
#define REALME_DEBUG_LEVEL_DEBUG    (0x04) // 指明细致的事件信息，对调试应用最有用

/*
    realme debug module
*/
#define REALME_DEBUG_MODULE_UI      (0x00)
#define REALME_DEBUG_MODULE_A2DP    (0x01)
#define REALME_DEBUG_MODULE_HFP     (0x02)
#define REALME_DEBUG_MODULE_GSENSOR (0x03)
#define REALME_DEBUG_MODULE_TOUCH   (0x04)
#define REALME_DEBUG_MODULE_BATTERY (0x05)
#define REALME_DEBUG_MODULE_ALL     (0xFF) // 所有模块

#endif

