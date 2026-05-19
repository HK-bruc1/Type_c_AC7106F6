/*************************************************************************************************/
/*!
*  \file      audio_time.h
*
*  \brief     音频使用的时间系函数、结构声明
*
*  Copyright (c) 2011-2023 ZhuHai Jieli Technology Co.,Ltd.
*
*/
/*************************************************************************************************/
#ifndef _AUDIO_TIME_H_
#define _AUDIO_TIME_H_

/*
*********************************************************************
*                  audio local time init
* Description: 音频使用的基于本地晶振源的参考时钟初始化
* Arguments  : None.
* Return	 : 0 成功 其他 失败
* Note(s)    : 初始化后提供的audio_jiffies_usec()不会因为系统时钟的
*              切换而发生节拍丢失
*********************************************************************
*/
int audio_local_time_init(void);


/********************************************************************
 *              蓝牙相关的参考时钟设置
 ********************************************************************/


/*
*********************************************************************
*                  bt audio reference_clock select
* Description: 音频选择蓝牙的参考时钟
* Arguments  : addr - 经典蓝牙地址（le audio默认NULL）
*              network - 网络0 - 手机，1 - TWS，2 - le audio
* Return	 : 0 成功 其他 失败
* Note(s)    : 内部提供了时钟与音频信息的关联
*********************************************************************
*/
extern int bt_audio_reference_clock_select(void *addr, u8 network);

/*
*********************************************************************
*                  bt audio reference_clock time
* Description: 读取蓝牙参考时钟当前的即时时间
* Arguments  : network - 网络0 - 手机，1 - TWS
* Return	 : 当前链路主机时间（slot）
* Note(s)    : 仅用于经典蓝牙
*********************************************************************
*/
extern u32 bt_audio_reference_clock_time(u8 network);

/*
*********************************************************************
*                  bt audio reference_clock remapping
* Description: 蓝牙参考时钟不同网络时钟之间的转换/重映射
* Arguments  : src_addr - 源的经典蓝牙地址
*              src_network - 源的网络时钟选择
*              dst_addr - 目标的经典蓝牙地址
*              dst_network - 目标的网络时钟
*              clock - 输入的时间
* Return	 : 转换完后的目标网络时钟时间（slot）
* Note(s)    : 仅用于经典蓝牙
*********************************************************************
*/
extern u32 bt_audio_reference_clock_remapping(void *src_addr, u8 src_network, void *dst_addr, u8 dst_network, u32 clock);

/*
*********************************************************************
*                  bt audio reference link exist
* Description: 查询网络时钟链路是否存在
* Arguments  : addr - 经典蓝牙地址
*              network - 网络0 - 手机，1 - TWS
* Return	 : 0 - 不存在，非0 - 存在
* Note(s)    : 仅用于经典蓝牙
*********************************************************************
*/
extern u8 bt_audio_reference_link_exist(void *addr, u8 network);

/*
*********************************************************************
*                  tws conn system clock init
* Description: 初始化tws连接的参考时钟与本地时钟的映射
* Arguments  : factor - 扩展的倍数
* Return	 : 0 成功 其他 失败
* Note(s)    : 用于本地时钟与经典蓝牙TWS网络时钟的转换处理
*********************************************************************
*/
extern int tws_conn_system_clock_init(u8 factor);

/*
*********************************************************************
*                  tws conn master to local time
* Description: 将tws链路的参考时间转换为本地时间
* Arguments  : usec - 输出的us时间（含放大系数）
* Return	 : 转换后的us时间
* Note(s)    : 用于本地时钟与经典蓝牙TWS网络时钟的转换处理
*********************************************************************
*/
extern u32 tws_conn_master_to_local_time(u32 usec);

/*
*********************************************************************
*                  bt edr conn system clock init
* Description: 初始化与edr主机链接（非TWS）的参考时钟与本地时钟的映射
* Arguments  : addr - 远端主机地址，factor - 扩展的倍数
* Return	 : None.
* Note(s)    : 用于本地时钟与经典蓝牙远端主机网络时钟的转换处理
*********************************************************************
*/
extern void bt_edr_conn_system_clock_init(void *addr, u8 factor);

/*
*********************************************************************
*                 bt edr conn master to local time
* Description: 将tws链路的参考时间转换为本地时间
* Arguments  : usec - 输出的us时间（含放大系数）
* Return	 : 转换后的us时间
* Note(s)    : 用于本地时钟与经典蓝牙TWS网络时钟的转换处理
*********************************************************************
*/
extern u32 bt_edr_conn_master_to_local_time(void *addr, u32 usec);

/*
*********************************************************************
*                  bt audio reference_clock time
* Description: 直接读取蓝牙链路主机的当前即时时间
* Arguments  : addr - 经典蓝牙链路主机地址
* Return	 : 当前链路主机时间（slot）
* Note(s)    : 仅用于经典蓝牙
*********************************************************************
*/
extern u32 bt_audio_conn_clock_time(void *addr);
#endif
