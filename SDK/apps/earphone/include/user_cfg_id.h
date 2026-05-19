#ifndef _USER_CFG_ID_H_
#define _USER_CFG_ID_H_

//=================================================================================//
//                            与APP CASE相关配置项[1 ~ 60]                         //
//=================================================================================//

#define 	CFG_EARTCH_ENABLE_ID			 1
#define 	CFG_PBG_MODE_INFO				 2

#define		CFG_MIC_ARRAY_DIFF_CMP_VALUE     3//麦克风阵列算法前补偿值
#define		CFG_MIC_ARRAY_TRIM_EN		     4//麦克风阵列校准补偿使能
#define     CFG_DMS_MALFUNC_STATE_ID         5//dms故障麦克风检测默认使用哪个mic的参数id
#define     CFG_MIC_TARGET_DIFF_CMP          6//目标MIC补偿值
#define     CFG_LE_AUDIO_EN                  7
#define     CFG_ANC_MIC_CMP_GAIN			 8//ANC MIC补偿配置

#define     CFG_ANC_ADAPTIVE_DUT_ID   		 9//保存ANC自适应产测参数id
#define     CFG_VBG_TRIM                    10//保存VBG配置参数id
#define     CFG_DACLDO_TRIM                 11//保存DACLDO配置参数id

#define     TUYA_SYNC_KEY_INFO              12
#define     CFG_USER_COMMON_ADDR            13


#define     CFG_EQ0_INDEX               	19
#define     CFG_MIC_EFF_VOLUME_INDEX        20

#define     CFG_USER_TUYA_INFO_AUTH         21
#define     CFG_USER_TUYA_INFO_AUTH_BK      22
#define     CFG_USER_TUYA_INFO_SYS          23
#define     CFG_USER_TUYA_INFO_SYS_BK       24

#define    	CFG_IMU_GYRO_OFFEST_ID          28	//空间音频imu陀螺仪偏置
#define    	CFG_IMU_ACC_OFFEST_ID           29	//空间音频imu加速度偏置

#define     VM_LP_TOUCH_KEY0_ALOG_CFG       31
#define     VM_LP_TOUCH_KEY1_ALOG_CFG       32
#define     VM_LP_TOUCH_KEY2_ALOG_CFG       33
#define     VM_LP_TOUCH_KEY0_IDTY_ALGO      34
#define     VM_LP_TOUCH_KEY1_IDTY_ALGO      35
#define     VM_LP_TOUCH_KEY2_IDTY_ALGO      36

#define     CFG_SPK_EQ_SEG_SAVE             37
#define     CFG_SPK_EQ_GLOBAL_GAIN_SAVE     38

#define 	CFG_BCS_MAP_WEIGHT				 39
#define     CFG_RCSP_ADV_HIGH_LOW_VOL        40
#define     CFG_RCSP_ADV_EQ_MODE_SETTING     41
#define     CFG_RCSP_ADV_EQ_DATA_SETTING     42
#define     ADV_SEQ_RAND                     43
#define     CFG_RCSP_ADV_TIME_STAMP          44
#define     CFG_RCSP_ADV_WORK_SETTING        45
#define     CFG_RCSP_ADV_MIC_SETTING         46
#define     CFG_RCSP_ADV_LED_SETTING         47
#define     CFG_RCSP_ADV_KEY_SETTING         48

#define     CFG_CHARGE_FULL_VBAT_VOLTAGE     49//充满电后记当前的VBAT的ADC值
#define     VM_CHARGE_PROGI_VOLT             50//恒流充电的PROGI

#define 	CFG_HAVE_MASS_STORAGE       52
#define     CFG_MUSIC_MODE              53

#define		LP_KEY_EARTCH_TRIM_VALUE	54

#define     CFG_RCSP_ADV_ANC_VOICE      55
#define     CFG_RCSP_ADV_ANC_VOICE_MODE 56
#define     CFG_RCSP_ADV_ANC_VOICE_KEY  57

#define     CFG_VOLUME_ENHANCEMENT_MODE        58
#define     CFG_ANC_ADAPTIVE_DATA_ID   		59//保存ANC自适应参数id

#define     VM_BATTERT_PRODUCT          60


// ll sync
#define     CFG_LLSYNC_RECORD_ID        170
#define     CFG_LLSYNC_OTA_INFO_ID      171

// findmy
#define     CFG_FMNA_BLE_ADDRESS_INFO        175
#define     CFG_FMNA_SOFTWARE_AUTH_START     176
#define     CFG_FMNA_SOFTWARE_AUTH_END       (CFG_FMNA_SOFTWARE_AUTH_START + 4)
#define     CFG_FMNA_SOFTWARE_AUTH_FLAG      181
#define     CFG_FMY_INFO                     182

// le_audio
#define		CFG_JL_UNICAST_EDR_MODE			 186 // jlunicast模式切换
#define		CFG_JL_CIS_DUAL_UAC_VOL	         187 // jlunicast双声卡音量

//tuya
#define		VM_TUYA_TRIPLE	          190

#endif /* #ifndef _USER_CFG_ID_H_ */
