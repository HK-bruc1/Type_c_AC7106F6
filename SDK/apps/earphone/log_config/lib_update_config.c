#include "app_config.h"
#include "system/includes.h"
#include "update_loader_download.h"

/* #ifdef CONFIG_256K_FLASH */
/* const int config_update_mode = UPDATE_UART_EN; */
/* #else */
/* const int config_update_mode = UPDATE_BT_LMP_EN \ */
/*                                | UPDATE_STORAGE_DEV_EN | UPDATE_BLE_TEST_EN | UPDATE_APP_EN | UPDATE_UART_EN; */
/* #endif */

//是否采用双备份升级方案:0-单备份;1-双备份
#if CONFIG_DOUBLE_BANK_ENABLE
const int support_dual_bank_update_en = 1;
#else
const int support_dual_bank_update_en = 0;
#endif  //CONFIG_DOUBLE_BANK_ENABLE

//是否双备份升级方案，但appcore1的区域相比appcore0更小
#if CONFIG_DOUBLE_BANK_LESS
const int support_dual_bank_less_en = 1;
#else
const int support_dual_bank_less_en = 0;
#endif  //CONFIG_DOUBLE_BANK_ENABLE

// 是否支持双备份升级前和升级失败对升级区域全擦，升级过程只写的功能
// 即允许边升级边播歌
#ifdef TCFG_DUAL_BANK_UPDATE_NO_ERASE
const int support_dual_bank_update_no_erase = TCFG_DUAL_BANK_UPDATE_NO_ERASE;
#else
const int support_dual_bank_update_no_erase = 0;
#endif

#if CONFIG_USER_FILE_UPDATE_V2_EN
const int support_user_file_update_v2_en = 1;
#else
const int support_user_file_update_v2_en = 0;
#endif


// 是否支持双备份断点续传升级
const int support_dual_bank_update_breakpoint = 0;

// 是否支持tws双备份升级，从机收错包重发机制
const int support_dual_bank_slave_recv_again = 1;

// 双备份主动升级模式下不升级资源区(设置为1则不升级资源区)
const int support_dual_bank_not_update_reserve = 1;

// 双备份tws升级主从同步校验使能
const int support_dual_bank_sync_verify_en = 1;

#if OTA_TWS_SAME_TIME_NEW       //使用新的同步升级流程
const int support_ota_tws_same_time_new =  1;
#else
const int support_ota_tws_same_time_new =  0;
#endif
//是否支持升级之后保留vm数据
const int support_vm_data_keep = 1;

//是否支持外挂flash升级,需要打开Board.h中的TCFG_NOR_FS_ENABLE
const int support_norflash_update_en  = 0;

//支持从外挂flash读取ufw文件升级使能
const int support_norflash_ufw_update_en = 0;

// 单备份升级时是否支持检查升级文件是否一致(BIT(0))
const int support_config_update_features = 0;

#if TCFG_UPDATE_ENABLE
const int CONFIG_UPDATE_ENABLE  = 0x1;
const int CONFIG_UPDATE_STORAGE_DEV_EN  = TCFG_UPDATE_STORAGE_DEV_EN;
const int CONFIG_UPDATE_BLE_TEST_EN  = TCFG_UPDATE_BLE_TEST_EN;
const int CONFIG_UPDATE_BT_LMP_EN  = TCFG_UPDATE_BT_LMP_EN;
#ifdef TCFG_UPDATE_MUTIL_CPU_MASTER
const int CONFIG_UPDATE_MUTIL_CPU_MASTER = TCFG_UPDATE_MUTIL_CPU_MASTER; // 是否固定为主机
#endif
#else
const int CONFIG_UPDATE_ENABLE  = 0x0;
const int CONFIG_UPDATE_STORAGE_DEV_EN  = 0;
const int CONFIG_UPDATE_BLE_TEST_EN  = 0;
const int CONFIG_UPDATE_BT_LMP_EN  = 0;
const int CONFIG_UPDATE_MUTIL_CPU_MASTER = 0; // 是否固定为主机
#endif
