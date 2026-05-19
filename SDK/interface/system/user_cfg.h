#ifndef __USER_CFG_H__
#define __USER_CFG_H__

#include "typedef.h"

#define LOCAL_NAME_LEN	32	/*BD_NAME_LEN_MAX*/

//bt bin结构
typedef struct __BT_CONFIG {
    u8 edr_name[LOCAL_NAME_LEN];       //经典蓝牙名
    u8 mac_addr[6];                    //蓝牙MAC地址
    u8 rf_power;                       //发射功率
    u16 tws_device_indicate;         /*设置对箱搜索标识，inquiry时候用,搜索到相应的标识才允许连接*/
    u8 tws_local_addr[6];
} _GNU_PACKED_  BT_CONFIG;






//mic type
typedef struct __MIC_TYPE_CONFIG {
    u8 type;     //0:不省电容模式        1:省电容模式
    //1:16K 2:7.5K 3:5.1K 4:6.8K 5:4.7K 6:3.5K 7:2.9K  8:3K  9:2.5K 10:2.1K 11:1.9K  12:2K  13:1.8K 14:1.6K  15:1.5K 16:1K 31:0.6K
    u8 pull_up;
    //00:2.3v 01:2.5v 10:2.7v 11:3.0v
    u8 ldo_lev;
} _GNU_PACKED_ MIC_TYPE_CONFIG;



//自动关机时间配置
typedef struct __AUTO_OFF_TIME_CONFIG {
    u8 auto_off_time;
} _GNU_PACKED_ AUTO_OFF_TIME_CONFIG;



void cfg_file_parse(u8 idx);
const u8 *bt_get_mac_addr();
void bt_get_tws_local_addr(u8 *addr);

void get_random_number(u8 *ptr, u8 len);
void bt_get_vm_mac_addr(u8 *addr);
const char *bt_get_local_name();
u16 bt_get_tws_device_indicate(u8 *tws_device_indicate);
const char *bt_get_local_name();
void bt_update_mac_addr(u8 *addr);
void bt_set_local_name(char *name, u8 len);
void bt_reset_and_get_mac_addr(u8 *addr);
void bt_set_pair_code_en(u8 en);
const char *sdk_version_info_get(void);

#endif
