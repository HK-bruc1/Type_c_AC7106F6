
#ifndef _BTSTACK_RCSP_USER_H_
#define _BTSTACK_RCSP_USER_H_

#include "typedef.h"
#include "JL_rcsp_api.h"
#include "JL_rcsp_packet.h"
#include "JL_rcsp_protocol.h"
#include "btstack/le/att.h"

extern void *rcsp_server_ble_hdl;		// ble连接handle
extern void *rcsp_server_ble_hdl1;		// ble连接handle，rcsp双连的时候有效
extern void *rcsp_server_edr_att_hdl;   // att连接handle
extern void *rcsp_server_edr_att_hdl1;  // att连接handle，rcsp双连的时候有效

/**
 * @brief rcsp ble/spp handle初始化
 */
void bt_rcsp_interface_init(const uint8_t *rcsp_profile_data);

/**
 * @brief rcsp ble/spp handle退出并销毁
 */
void bt_rcsp_interface_exit(void);

/**
 * @brief 获取rcsp已连接设备数目
 *
 * @result 返回连接的手机设备数
 */
u8 bt_rcsp_device_conn_num(void);
/**
 * @brief 获取当前已连接ble数目
 *
 * @result 返回连接的ble手机设备数
 */
u8 bt_rcsp_ble_conn_num(void);
/**
 * @brief 获取当前已连接spp数目
 *
 * @result 返回连接的spp手机设备数
 */
u8 bt_rcsp_spp_conn_num(void);
/**
 * @brief 获取当前已连接att数目
 *
 * @result 返回连接的att手机设备数
 */
u8 bt_rcsp_edr_att_conn_num(void);

/**
 * @brief 在连接或断开的时候，设置rcsp蓝牙连接信息
 *
 * @param con_handle ble连接或断开时输入
 * @param remote_addr spp连接或断开时输入
 * @param isconn true:连接；false:断开
 */
void bt_rcsp_set_conn_info(u16 con_handle, void *remote_addr, bool isconn);

/**
 * @brief 用于tws同步，从机接收到主机的tws数据后同步到本地
 *
 * @param bt handle数据
 * @param bt handle数据长度
 */
void rcsp_interface_set_bt_hdl_with_tws_data(u8 *recieve_buf, u16 recieve_len);

/**
 *	@brief 用于rcsp接收ble/spp数据使用
 *
 *	@param hdl ble/spp发送句柄
 *	@param remote_addr spp发送地址
 *	@param buf 接收数据
 *	@param len 接收数据的长度
 */
void bt_rcsp_recieve_callback(void *hdl, void *remote_addr, u8 *buf, u16 len);

/**
 *	@brief 用于发送rcsp的数据使用
 *
 *	@param ble_con_hdl ble发送句柄
 *	@param remote_addr spp发送地址	注：当ble_con_hdl与remote_addr都不填时，给所有的设备都发数据
 *	@param buf 发送的数据
 *	@param len 发送的数据长度
 */
int bt_rcsp_data_send(u16 ble_con_hdl, u8 *remote_addr, u8 *buf, u16 len);

/**
 * @brief 获取当前ble连接设备的mac地址
 */
u8 *rcsp_get_ble_hdl_remote_mac_addr(u16 ble_con_handle);

/**
 * @brief 获取tws同步的bt handle的总buf大小
 */
u16 rcsp_interface_tws_sync_buf_size();

/**
 * @brief rcsp主机同步bt handle数据给从机
 *
 * @param send_buf 外部malloc的一个指针，size根据rcsp_interface_tws_sync_buf_size获取
 */
void rcsp_interface_tws_sync_buf_content(u8 *send_buf);

/**
 * @brief rcsp ble_profile初始化
 * 			(三方协议简化版时有效)
 *
 */
void rcsp_simplified_ble_profile_init(const uint8_t *rcsp_profile_data, att_read_callback_t read_callback, att_write_callback_t write_callback);

/**
 * @brief rcsp spp模块初始化
 * 			(三方协议简化版时有效)
 *
 */
void rcsp_simplified_spp_init();

/**
 * @brief rcsp spp连接状态返回
 * 			(三方协议简化版时有效)
 *
 * @return true/false:已/未连接
 */
bool rcsp_spp_conn_state_get();

/**
 * @brief rcsp spp地址返回
 * 			(三方协议简化版时有效)
 *
 * @return spp地址
 */
u8 *rcsp_simplified_spp_addr();

/**
 * @brief rcsp spp info重置
 * 			(三方协议简化版时有效)
 *
 */
void rcsp_simplified_reset_spp_info();

/**
 * @brief rcsp spp连接地址设置
 * 			(三方协议简化版时有效)
 *
 * @param  spp地址
 */
u8 rcsp_simplified_set_spp_conn_addr(u8 *spp_addr);

/**
 * @brief rcsp spp数据发送
 * 			(三方协议简化版时有效)
 *
 * @param data
 * @param len
 *
 * @return send state
 */
int rcsp_spp_send_data(u8 *data, u16 len);

#endif // _BTSTACK_RCSP_USER_H_
