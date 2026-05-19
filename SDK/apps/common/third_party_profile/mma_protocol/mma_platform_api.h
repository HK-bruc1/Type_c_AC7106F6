#ifndef _MMA_PLATFORM_API_H_
#define _MMA_PLATFORM_API_H_

#include "system/includes.h"

//////////////////////// MMA ATTR SET/GET  /////////////////////

///////////////////////   MMA ATTR INDEX   ///////////////////////
#define XM_ATTR_IDX_AUDIO_MODE              (0x0001)
#define XM_ATTR_IDX_KEY_MAPPING             (0x0002)
#define XM_ATTR_IDX_AUTO_ANSWER             (0x0003)
#define XM_ATTR_IDX_MULTI_POINTS            (0x0004)
#define XM_ATTR_IDX_COMPACTNESS_CHECK       (0x0005)
#define XM_ATTR_IDX_COMPACTNESS             (0x0006)
#define XM_ATTR_IDX_EQ_MODE                 (0x0007)
#define XM_ATTR_IDX_DEVICE_NAME             (0x0008)
#define XM_ATTR_IDX_FIND                    (0x0009)
#define XM_ATTR_IDX_ANC_MODE_MAPPING        (0x000A)
#define XM_ATTR_IDX_ANC_LEVEL               (0x000B)
#define XM_ATTR_IDX_ANTI_LOST               (0x000C)
#define XM_ATTR_IDX_LP_MODE_DETECT          (0x000D)
#define XM_ATTR_IDX_APP_NAME                (0x000E)
#define XM_ATTR_IDX_WIND_NOISE              (0x000F)

#define XM_ATTR_IDX_SPATIAL_AUDIO_SET       (0x001D)
#define XM_ATTR_IDX_SPATIAL_AUDIO_GET       (0x001E)
#define XM_ATTR_IDX_SILENCE_OTA             (0x0023)
#define XM_ATTR_IDX_AUTO_ANC                (0x0025)
#define XM_ATTR_IDX_SN                      (0x0027)
#define XM_ATTR_IDX_DEVICE_TIME             (0x0028)



///////////////////////  MMA ATTR PARAM   ///////////////////////

// XM_ATTR_IDX_AUDIO_MODE              (0x0001)
#define XM_AUDIO_MODE_NOT_SUPPORT       (-1)
#define XM_AUDIO_MODE_NORMAL            (0)
#define XM_AUDIO_MODE_LP                (1)
#define XM_AUDIO_MODE_HD                (2)

// XM_ATTR_IDX_KEY_MAPPING             (0x0002)
//     KK:手势类型
#define XM_KEY_CLICK                    (0x04)  // 单击
#define XM_KEY_DOUBLE_CLICK             (0x01)  // 双击
#define XM_KEY_TREBLE_CLICK             (0x02)  // 三击
#define XM_KEY_LONG_PRESS               (0x03)  // 长按
//     LL:左耳的自定义按键
#define XM_KEY_WAKEUP_XIAO_AI           (0x00)  //拉起小爱
#define XM_KEY_PLAY_OR_STOP             (0x01)  //播放/暂停
#define XM_KEY_PREV_SONG                (0x02)  //上一曲
#define XM_KEY_NEXT_SONG                (0x03)  //下一曲
#define XM_KEY_VOL_UP                   (0x04)  //增加音量
#define XM_KEY_VOL_DOWN                 (0x05)  //减少音量
#define XM_KEY_ANC_CONTROL              (0x06)  //降噪控制
#define XM_KEY_NULL                     (0x08)  //无操作
#define XM_KEY_INVILID                  (0xFF)

// XM_ATTR_IDX_AUTO_ANSWER             (0x0003)
#define XM_AUTO_ANSWER_NOT_SUPPORT      (-1)
#define XM_AUTO_ANSWER_OFF              (0)
#define XM_AUTO_ANSWER_ON               (1)

// XM_ATTR_IDX_MULTI_POINTS            (0x0004)
#define XM_MULTI_POINTS_NOT_SUPPORT     (-1)
#define XM_MULTI_POINTS_OFF             (0)
#define XM_MULTI_POINTS_ON              (1)

// XM_ATTR_IDX_COMPACTNESS_CHECK       (0x0005)
#define XM_COMPACTNESS_CHECK_START      (1)

// XM_ATTR_IDX_COMPACTNESS             (0x0006)
#define XM_COMPACTNESS_UNKNOW           (0x00)
#define XM_COMPACTNESS_SUITABLE         (0x01)
#define XM_COMPACTNESS_UNSUITABLE       (0x02)
#define XM_COMPACTNESS_IS_WEARING       (0x03)
#define XM_COMPACTNESS_ISN_WEARING      (0x09)

// XM_ATTR_IDX_EQ_MODE                 (0x0007)
#define XM_EQ_MODE_NORMAL               (0x00)
#define XM_EQ_MODE_VOCAL                (0x01)
#define XM_EQ_MODE_ROCK                 (0x02)
#define XM_EQ_MODE_CLASSIC              (0x03)
#define XM_EQ_MODE_POP                  (0x04)
#define XM_EQ_MODE_BASS                 (0x05)

// XM_ATTR_IDX_DEVICE_NAME             (0x0008)

// XM_ATTR_IDX_FIND                    (0x0009)
#define XM_FIND_ENABLE                  (1)
#define XM_FIND_DISABLE                 (0)

#define XM_FIND_LEFT                    (1)
#define XM_FIND_RIGHT                   (2)
#define XM_FIND_ALL                     (3)

// XM_ATTR_IDX_ANC_MODE_MAPPING        (0x000A)
#define XM_ANC_MODE_OFF_ENABLE          (0x01)
#define XM_ANC_MODE_ANC_ENABLE          (0x02)
#define XM_ANC_MODE_TRANS_ENABLE        (0x04)

// XM_ATTR_IDX_ANC_LEVEL               (0x000B)
#define XM_ANC_MODE_OFF                 (0)
#define XM_ANC_MODE_ANC                 (1)
#define XM_ANC_MODE_TRANS               (2)
//      mode off
#define XM_ANC_MODE_OFF_OFF             (0)
//      mode anc
#define XM_ANC_MODE_ANC_BALANCE         (0)
#define XM_ANC_MODE_ANC_COMFORT         (1)
#define XM_ANC_MODE_ANC_DEEP            (2)
#define XM_ANC_MODE_ANC_ADAPT           (3)
//      mode trans
#define XM_ANC_MODE_TRANS_NORMAL        (0)
#define XM_ANC_MODE_TRANS_VOCAL         (1)

// XM_ATTR_IDX_ANTI_LOST               (0x000C)
#define XM_ANTI_LOST_R_IN_BOX           (0x01)
#define XM_ANTI_LOST_L_IN_BOX           (0x02)
#define XM_ANTI_LOST_R_WEARING          (0x04)
#define XM_ANTI_LOST_L_WEARING          (0x08)

// XM_ATTR_IDX_LP_MODE_DETECT          (0x000D)
#define XM_LP_MODE_DETECT_OFF           (0)
#define XM_LP_MODE_DETECT_ON            (1)

// XM_ATTR_IDX_APP_NAME                (0x000E)



// XM_ATTR_IDX_SPATIAL_AUDIO_SET           (0x001D)
// XM_ATTR_IDX_SPATIAL_AUDIO_GET           (0x001E)
#define XM_SPATIAL_AUDIO_DISABLE        (0x00)
#define XM_SPATIAL_AUDIO_ENABLE         (0x01)

#define XM_SPATIAL_AUDIO_HD             (0x00)
#define XM_SPATIAL_AUDIO_LOW_LATENCA    (0x02)

#define XM_SPATIAL_AUDIO_HEAD_TRACKING_ON       (0x04)
#define XM_SPATIAL_AUDIO_HEAD_TRACKING_OFF      (0x00)

#define XM_SPATIAL_AUDIO_VIRTUAL_SURROUND_ON    (0x08)
#define XM_SPATIAL_AUDIO_VIRTUAL_SURROUND_OFF   (0x00)

#define XM_SPATIAL_AUDIO_DATA_TYPE_ORIGINAL     (0x10)
#define XM_SPATIAL_AUDIO_DATA_TYPE_EULER        (0x00)

// XM_ATTR_IDX_SILENCE_OTA             (0x0023)
// #define XM_SILENCE_OTA_NOT_SUPPORT      (-1)
// #define XM_SILENCE_OTA_OFF              (0)
// #define XM_SILENCE_OTA_ON               (1)

// XM_ATTR_IDX_AUTO_ANC                (0x0025)
#define XM_AUTO_ANC_NOT_SUPPORT         (-1)
#define XM_AUTO_ANC_OFF                 (0)
#define XM_AUTO_ANC_ON                  (1)


extern int xm_device_config_attr_get(u16 attr_idx, u8 *buffer);
extern int xm_device_config_attr_set(u16 attr_idx, u8 *buffer, int len);
extern int xm_device_config_attr_notify(u16 attr_idx);

//////////////////////// MIUI AT API  /////////////////////

#define XM_AT_IDX_HEADSET_STATE         0x0000
#define XM_AT_IDX_BASIC_INFO            0x0100
#define XM_AT_IDX_KEY_MAPPING           0x0105
#define XM_AT_IDX_VOICE_TRAINING        0x0106
#define XM_AT_IDX_EQUALIZER             0x0107
#define XM_AT_IDX_GAME_MODE             0x0108
#define XM_AT_IDX_ANTI_LOST             0x010A
#define XM_AT_IDX_DENOISE               0x010B

#define XM_AT_IDX_ADV_RSP_DATA          0x0200
#define XM_AT_IDX_ADV_DATA              0x0201
#define XM_AT_IDX_RSP_DATA              0x0202

#define XM_AT_IDX_ACCOUNT_KEY           0x0303

extern int xm_at_cmd_status_update(u32 index, u8 *ext_data, u32 len);

//////////////////////// OTHERS  /////////////////////

// set
extern int xm_bt_name_set(u8 *name, u32 length);

// get
extern bool xm_left_page_scan_status_get(void);
extern bool xm_right_page_scan_status_get(void);
extern bool xm_left_inquiry_scan_status_get(void);
extern bool xm_right_inquiry_scan_status_get(void);
extern bool xm_tws_side_get(void);
extern bool xm_tws_connecting_status_get(void);
extern bool xm_tws_connected_status_get(void);
extern bool xm_left_leave_box_status_get(void);
extern bool xm_right_leave_box_status_get(void);
extern bool xm_all_leave_box_status_get(void);
extern bool xm_edr_paired_status_get(void);
extern bool xm_edr_connencted_status_get(void);
extern bool xm_box_open_status_get(void);
extern u8 xm_left_battery_get(void);
extern u8 xm_right_battery_get(void);
extern u8 xm_box_battery_get(void);
extern u8 xm_anc_mode_get(void);
extern int xm_bt_name_get(u8 *name);
extern void xm_get_edr_address(u8 *addr_buf);
extern void xm_edr_last_paired_addr_get(u8 *addr);

// update
extern int xm_left_page_scan_status_update(bool status);
extern int xm_right_page_scan_status_update(bool status);
extern int xm_left_inquiry_scan_status_update(bool status);
extern int xm_right_inquiry_scan_status_update(bool status);
extern int xm_tws_side_update(bool status);
extern int xm_tws_connecting_status_update(bool status);
extern int xm_tws_connected_status_update(bool status);
extern int xm_left_leave_box_status_update(bool status);
extern int xm_right_leave_box_status_update(bool status);
extern int xm_all_leave_box_status_update(bool status);
extern int xm_edr_paired_status_update(bool status);
extern int xm_edr_connencted_status_update(bool status);
extern int xm_box_open_status_update(bool status);
extern int xm_left_battery_update(u8 status);
extern int xm_right_battery_update(u8 status);
extern int xm_box_battery_update(u8 status);
extern int xm_anc_mode_update(u8 status);
extern int xm_key_mapping_update(u8 status);
extern int xm_eq_mode_update(u8 status);
extern int xm_game_mode_update(u8 status);

// callback
extern void xm_game_mode_callback(u8 game_mode);

//////////////////////  lib api  ////////////////////////
extern void xm_adv_update_notify(void);
extern bool XM_ble_is_connected(void);
extern bool XM_spp_is_connected(void);
extern void xm_hfp_connect_handler(void);
extern void xm_hfp_disconnect_handler(void);
extern void xm_set_adv_interval(u16 value);
extern void xm_bt_ble_adv_enable(u8 enable);
extern int  xm_bt_ble_disconnect(void *priv);
extern bool xm_get_pair_state(void);
extern void xm_set_pair_state(u8 state);



#endif // _MMA_PLATFORM_API_H_

