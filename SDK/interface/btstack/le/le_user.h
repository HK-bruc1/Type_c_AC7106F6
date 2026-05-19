
#ifndef _LE_USER_H_
#define _LE_USER_H_

#include "typedef.h"
#include "btstack/btstack_typedef.h"
#include "ble_api.h"

#if defined __cplusplus
extern "C" {
#endif



#define BT_NAME_LEN_MAX		      29
#define ADV_RSP_PACKET_MAX        31

// hci con handles (12 bit): 0x0000..0x0fff
#define HCI_CON_HANDLE_INVALID 0xffff

#define BTSTACK_EVENT_STATE                                0x60
#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE   0x77

#define SM_EVENT_NUMERIC_COMPARISON_REQUEST                0xD6

#define SM_EVENT_JUST_WORKS_REQUEST                        0xD0
#define SM_EVENT_JUST_WORKS_CANCEL                         0xD1
#define SM_EVENT_PASSKEY_DISPLAY_NUMBER                    0xD2
#define SM_EVENT_PASSKEY_DISPLAY_CANCEL                    0xD3
#define SM_EVENT_PASSKEY_INPUT_NUMBER                      0xD4
#define SM_EVENT_PASSKEY_INPUT_CANCEL                      0xD5
#define SM_EVENT_PAIR_PROCESS                              0xDF
//0xdf 's sub
#define SM_EVENT_PAIR_SUB_RECONNECT_START                        0x01
#define SM_EVENT_PAIR_SUB_PIN_KEY_MISS                           0x02
#define SM_EVENT_PAIR_SUB_PAIR_FAIL                              0x03
#define SM_EVENT_PAIR_SUB_PAIR_TIMEOUT                           0x04
#define SM_EVENT_PAIR_SUB_ENCRYPTION_FAIL                        0x05
#define SM_EVENT_PAIR_SUB_SEND_DISCONN                           0x0f
#define SM_EVENT_PAIR_SUB_ADD_LIST_SUCCESS                       0x10
#define SM_EVENT_PAIR_SUB_ADD_LIST_FAILED                        0x11


#define GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NONE          0
#define GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION  1
#define GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION    2


#define GATT_EVENT_NOTIFICATION                                  0xA7
#define GATT_EVENT_INDICATION                                    0xA8
#define GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT             0xA5
#define GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT        0xA6
// #define GATT_EVENT_SERVICE_QUERY_RESULT                          0xA1
// #define GATT_EVENT_CHARACTERISTIC_QUERY_RESULT                   0xA2
#define GATT_EVENT_QUERY_COMPLETE                                0xA0
#define GAP_EVENT_ADVERTISING_REPORT                             0xE2

// Authentication requirement flags
#define SM_AUTHREQ_NO_BONDING           0x00
#define SM_AUTHREQ_BONDING              0x01
#define SM_AUTHREQ_MITM_PROTECTION      0x04
#define SM_AUTHREQ_SECURE_CONNECTION    0x08
#define SM_AUTHREQ_KEYPRESS             0x10
#define SM_AUTHREQ_CT2                  0x20


#define L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE   0x77


#define  BT_OP_SUCCESS                             0x00
#define  BT_ERR_ADVERTISING_TIMEOUT                0x3C


//--------------------------------------------

    struct ble_server_operation_t {
        int(*adv_enable)(void *priv, u32 enable);
        int(*disconnect)(void *priv);
        int(*get_buffer_vaild)(void *priv);
        int(*send_data)(void *priv, void *buf, u16 len);
        int(*regist_wakeup_send)(void *priv, void *cbk);
        int(*regist_recieve_cbk)(void *priv, void *cbk);
        int(*regist_state_cbk)(void *priv, void *cbk);
        int(*latency_enable)(void *priv, u32 enable);
    };

    void ble_get_server_operation_table(struct ble_server_operation_t **interface_pt);

//--------------------------------------------

    struct ble_client_operation_t {
        int(*scan_enable)(void *priv, u32 enable);
        int(*disconnect)(void *priv);
        int(*get_buffer_vaild)(void *priv);
        int(*write_data)(void *priv, void *buf, u16 len);
        int(*read_do)(void *priv);
        int(*regist_wakeup_send)(void *priv, void *cbk);
        int(*regist_recieve_cbk)(void *priv, void *cbk);
        int(*regist_state_cbk)(void *priv, void *cbk);
        int (*init_config)(void *priv, void *cfg);
        int (*opt_comm_send)(u16 handle, u8 *data, u16 len, u8 att_op_type);
        int (*set_force_search)(u8 onoff, s8 rssi);
        int (*create_connect)(u8 *addr, u8 addr_type, u8 mode);
        int (*create_connect_cannel)(void);
        int (*get_work_state)(void);
        int (*opt_comm_send_ext)(u16 conn_handle, u16 handle, u8 *data, u16 len, u8 att_op_type);
    };

    struct ble_client_operation_t *ble_get_client_operation_table(void);


    static inline uint32_t ble_min(uint32_t a, uint32_t b)
    {
        return a < b ? a : b;
    }

//---------------------------------------------------------------------------------------------------


//----------------------------------------
//----------------------------------------



    extern int get_ble_btstack_state(void);
    extern int get_indicate_state(void);


    extern u8 get_ble_local_name(u8 *name_buf);
    extern u8 get_ble_local_name_len();




    extern void hci_event_callback_set(void(*cbk_ph)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));
    extern void ll_hci_connection_updata(u8 *data);
    extern void le_hci_disconnect_all_connections(void);

    /*************************************************************************************************/
//le_audio_profile.a的对外接口说明
//unicast是连接状态的le audio播放，走CIS链路传输音频数据
//broadcast，auracast是广播的le audio叫法，走BIS链路传输音频数据
    /*************************************************************************************************/
// LEA_SUPPORT_MASK_协议名简称_S/C（S for server，C for client）
// 用来配置变量u32 lea_cfg_support_profile = 0; 的位定义，控制协议节省代码量
#define LEA_SUPPORT_MASK_BAS_C     0x0001     //电量
#define LEA_SUPPORT_MASK_BAS_S     0x0002     //电量
#define LEA_SUPPORT_MASK_DIS_C     0x0004
#define LEA_SUPPORT_MASK_DIS_S     0x0008
#define LEA_SUPPORT_MASK_PACS_C    0x0010
#define LEA_SUPPORT_MASK_PACS_S    0x0020
#define LEA_SUPPORT_MASK_ASCS_C    0x0040
#define LEA_SUPPORT_MASK_ASCS_S    0x0080
#define LEA_SUPPORT_MASK_MCS_C     0x0100
#define LEA_SUPPORT_MASK_MCS_S     0x0200
#define LEA_SUPPORT_MASK_TBS_C     0x0400
#define LEA_SUPPORT_MASK_TBS_S     0x0800
#define LEA_SUPPORT_MASK_VCS_C     0x1000
#define LEA_SUPPORT_MASK_VCS_S     0x2000
#define LEA_SUPPORT_MASK_CSIS_C    0x4000
#define LEA_SUPPORT_MASK_CSIS_S    0x8000
#define LEA_SUPPORT_MASK_BASS_C    0x10000
#define LEA_SUPPORT_MASK_BASS_S    0x20000
#define LEA_SUPPORT_MASK_HAS_C     0x40000
#define LEA_SUPPORT_MASK_HAS_S     0x80000
#define LEA_SUPPORT_MASK_TMAP_C    0x100000
#define LEA_SUPPORT_MASK_TMAP_S    0x200000
#define LEA_SUPPORT_MASK_MICS_C    0x400000
#define LEA_SUPPORT_MASK_MICS_S    0x800000
#define LEA_SUPPORT_MASK_VOCS_S    0x1000000
#define LEA_SUPPORT_MASK_AICS_S    0x2000000
#define LEA_SUPPORT_MASK_UNICAST   0x4000000
#define LEA_SUPPORT_MASK_AURACAST  0x8000000
    /*************************************************************************************************/
    /*!
     *  \brief      配置广播包的名字，一般开了le audio unicast使用的是双模同地址，一般配置成经典蓝牙名字就行
     *              配置不同名字手机设置界面可能会有两个名字切换显示
     *  \function   void le_audio_name_reset(u8 *name, u8 len)
     *  \param      [in] name      名字buffer缓存指针 .
     *              [in] len       名字长度
     *  \return     void.
     */
    /*************************************************************************************************/
    extern void le_audio_name_reset(u8 *name, u8 len);


    /*************************************************************************************************/
    /*!
     *  \brief      le audio 协议流程初始化。
     *  \function   void le_audio_init(u8 mode);
     *  \param      [in] mode  用于以后控制协议的初始化，目前只支持 1，unicast receiver相关协议配置
     *  \return     void.
     */
    /*************************************************************************************************/
    extern void le_audio_init(u8 mode);


    /*************************************************************************************************/
    /*!
     *  \brief      配置一个全局变量，用于控制广播包中flag中discover位，能不能被手机搜索到
     *  \function   void le_audio_set_discover_mode(u8 flag);
     *  \param      [in] flag  1 是可以在手机设置界面显示，0是不能在手机设置界面显示。但也还是能被NRF搜索到
     *  \return     void.
     */
    /*************************************************************************************************/
    extern void le_audio_set_discover_mode(u8 flag);

    /*************************************************************************************************/
    /*!
     *  \brief      获取le audio连接手机的真实地址
     *  \function  u8 get_sm_peer_address(u8 *addr);
     *  \param      [in] add    用于存放6个byte地址的buffer地址，空间至少6个byte
     *  \return      1 获取成功， 0 获取失败
     */
    /*************************************************************************************************/
    extern u8 get_sm_peer_address(u8 *addr);

    /*************************************************************************************************/
    /*!
     *  \brief      le audio 标准的characteristic uuid和handle定义是一个数据表，
     *              用户如果还要接ble app的话使用该接口注册characteristic uuid和handle
     *  \function   void le_audio_user_server_profile_init(const uint8_t *profile_tb);
     *  \param      [in] profile_tb    生成信息表的数值buffer指针
     *  \return     void
     */
    /*************************************************************************************************/
    extern void le_audio_user_server_profile_init(const uint8_t *profile_tb);

    /*************************************************************************************************/
    /*!
     *  \brief     开关le audio广播的接口
     *  \function   void bt_le_audio_adv_enable(u8 enable);
     *  \param      [in] enable   1:开广播   0：关广播
     *  \return     void
     */
    /*************************************************************************************************/
    extern void bt_le_audio_adv_enable(u8 enable);

    /*************************************************************************************************/
    /*!
     *  \brief    库里面是一个weak函数，默认返回了1,
     *            应用层重写这个接口,返回1是广播请求手机主动来连接le audio耳机。返回1广播是不想手机来连接le audio耳机
     *  \function   u8 le_audio_need_requesting_phone_connection();
     *  \param      void
     *  \return     u8   1 希望手机自动来连接le audio， 0 不希望手机自动连接le audio
     */
    /*************************************************************************************************/
    //u8 le_audio_need_requesting_phone_connection();

    /*************************************************************************************************/
    /*!
     *  \brief    提供两个接口在耳机端断开连接，两个接口的错误码不一样，会导致手机有点行为差异
     *  \param      void
     *  \return     u8   0 断开操作成功， 1 断开出错
     */
    /*************************************************************************************************/
    u8 le_audio_disconn_le_audio_link();                              //断开le_audio连接，手机会有回连行为
    u8 le_audio_disconn_le_audio_link_no_reconnect();             //断开le_audio连接,手机会没有回连行为


    /*************************************************************************************************/
    /*!
     *  \brief      类似 le_audio_need_requesting_phone_connection的功能，用于配置广播包的标记，开一次广播之后会清除标记
     *  \function   void le_audio_set_requesting_a_connection_flag(u8 flag);
     *  \param      [flag] 1 希望手机自动来连接le audio， 0 不希望手机自动连接le audio
     *  \return     void
     */
    /*************************************************************************************************/
    extern void le_audio_set_requesting_a_connection_flag(u8 flag);


    /*************************************************************************************************/
    /*!
     *  \brief      提供接口获取连接le audio链路的vcs协议音量值，播歌的时候获取是播歌的，通话的时候获取是通话的
     *  \function   u8 vcs_server_get_volume(u16 con_handle);
     *  \param      [con_handle] ble 连接链路的hci handle值
     *  \return     u8 音量值，范围是0-255，要获取之后要转换一下
     */
    /*************************************************************************************************/
    extern u8 vcs_server_get_volume(u16 con_handle);


#endif
