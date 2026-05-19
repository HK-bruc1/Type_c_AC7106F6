#ifndef APP_BT_SHARE_TWS_H
#define APP_BT_SHARE_TWS_H

#include "classic/tws_api.h"
#include "classic/tws_event.h"
#include "system/includes.h"
enum {
    PAGE_DEV_PHONE = 0,
    PAGE_DEV_SHARE,

};
enum {
    SHARE_TWS_SLAVE = 0,
    SHARE_CONN_PHONE_DISCONN = 1,
    SHARE_CONN_PHONE_1_CONN,
    SHARE_DISCON_PHONE_1_CONN,
    SHARE_DISCON_PHONE_DISCONN,
    SHARE_DISCON_PHONE_2_CONN,
};

enum {
    DATA_ID_SHARE_TO_SHARE_PP = 1,
    DATA_ID_SHARE_TO_SHARE_PREV,
    DATA_ID_SHARE_TO_SHARE_NEXT,
    DATA_ID_SHARE_A2DP_SLAVE_SWITCH_TO_SHARE_MASTER = 0XFE,
};

//开机初始化从vm获取共享设备 info，共享主:开启回连，共享从:开启page_scan等待连接
int bt_tws_share_device_init(u8 open_master, u8 open_slave);

//等待共享配对用公共地址，根据名字匹配进行被连接
void bt_tws_audio_share_wait_pair(u8 enable);

//等待共享配对用记忆地址,进行被连接
void bt_tws_audio_share_wait_page_scan();

void del_device_type_from_page_list(u8 dev_type);
void close_share_scan(void *p);
int get_share_phone_conn_state();
u8 *get_share_addr_for_vm();
int get_share_role_for_vm(u8 *addr);
void set_share_audio_pair_addr(u8 *addr);

//共享搜索配对，用公共地址，根据名字匹配进行
void bt_tws_audio_share_search_pair_start(u8 enable);

//开启共享连接，快速连接用的地址
u8 *bt_get_share_audio_pair_addr();

//共享主机收到共享从机avctp控制命令处理
void share_rx_avctp_opid_deal(u8 *addr, u8 cmd);

//根据地址获取共享设备 info，共享主:开启回连，共享从:开启page_scan等待连接
int bt_tws_share_device_disconn_deal(u8 reason, u8 *addr);
void share_a2dp_slave_clr_preempted_flag(u8 *bt_addr, u8 disconn);//clear 共享从机a2dp播歌被打断 flag
bool share_a2dp_preempted_resume(u8 *bt_addr);//共享从a2dp播放打断恢复播放

void share_a2dp_slave_set_preempted_flag(u8 *bt_addr);//set 共享从机a2dp播歌被打断 flag
u8 check_is_share_addr_for_vm(u8 *addr);//根据地址判断当前设备是否共享设备
void bt_tws_share_function_select_init();
bool check_open_share_wait_page_scan();
void bt_tws_share_master_sync_vol_to_share_slave();//共享主机同步手机音量给共享从机

int bt_tws_share_connect_disconn_tone_play(u8 connect, u8 *bt_addr);//tws共享连接成功和断开提示音播放
void bt_share_tx_data_to_other_share(u8 cmd, u8 value); //共享设备之间发送命令控制
int check_phone_a2dp_play_share_auto_slave_to_master(u8 *bt_addr);
int share_a2dp_slave_switch_to_share_master();
void bt_tws_share_function_select_close();
void set_rec_share_device(u8 rec_share_dev);
bool check_have_rec_share_device();


#endif
