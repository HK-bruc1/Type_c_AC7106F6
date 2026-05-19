#ifndef TWS_DUAL_CONN_H
#define TWS_DUAL_CONN_H

/**
 * @brief 设置inquiry scan和page scan
 *
 * @param scan_enable inquiry_scan_en
 * @param conn_enable page_scan_en
 */
void write_scan_conn_enable(bool scan_enable, bool conn_enable);

/**
 * @brief 根据tws、手机连接状态和连接记录设置可发现可连接
 */
void tws_dual_conn_state_handler();

/**
 * @brief tws不允许进入sniff
 */
void tws_sniff_controle_check_disable(void);

/**
 * @brief 同步可发现可连接及配对信息到对耳
 */
void send_page_device_addr_2_sibling();

/**
 * @brief 清除回连配对列表的设备信息
 */
void clr_device_in_page_list();

void dual_conn_page_device_phone_dev_timeout(u8 dev_type);

/**
 * @brief 添加设备地址进入回连列表
 *
 * @param mac_addr 设备地址
 * @param timeout 回连时间
 * @param dev_type 设备类型
 */
int add_device_2_page_list(u8 *mac_addr, u32 timeout, u8 dev_type);

/**
 * @brief 根据设备类型移除回连列表
 */
void del_device_type_from_page_list(u8 dev_type);

/**
 * @brief 根据回连链表回连设备
 */
void dual_conn_page_device();

/**
 * @brief 判断回连列表是否为空
 *
 * @return bool
 */
bool page_list_empty();

#endif
