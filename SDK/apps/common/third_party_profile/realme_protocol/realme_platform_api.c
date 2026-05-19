#include "system/includes.h"
#include "sdk_config.h"
#include "app_msg.h"
// #include "earphone.h"
#include "bt_tws.h"
#include "app_main.h"
#include "battery_manager.h"
#include "app_power_manage.h"
#include "btstack/avctp_user.h"
#include "a2dp_player.h"
#include "esco_player.h"
#include "clock_manager/clock_manager.h"
#include "btstack/third_party/app_protocol_event.h"
#include "multi_protocol_main.h"
#include "app_ble_spp_api.h"
#include "asm/charge.h"
#include "realme_platform_api.h"
#include "fs/sdfile.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & REALME_EN)

int realme_platform_feature_switch(u8 *remote_addr, u8 feature_id, u8 state)
{
    switch (feature_id) {
    case 0x01:    //降噪
        break;
    case 0x02:    //智能降噪
        break;
    case 0x03:    //鼾声峰噪
        break;
    case 0x04:    //佩戴检测(智能播放暂停)
        break;
    case 0x05:    //LHDC or LDAC(Device 通常只会两选一)
        break;
    case 0x06:    //游戏模式
        break;
    case 0x07:    //聚会模式
        break;
    case 0x08:    //自动接听电话
        break;
    case 0x09:    //人声增强
        break;
    case 0x0A:    //耳机操控
        break;
    case 0x0B:    //听力增强(个性化听感)
        break;
    case 0x0C:    //个性化降噪
        break;
    case 0x0D:    //耳机拍照
        break;
    case 0x0E:    //Debug 功能开关
        break;
    case 0x0F:    //耳机禅定功能
        break;
    case 0x10:    //-键开关(打开/关闭所有耳机操控)
        break;
    case 0x11:    //耳机一拖二
        break;
    case 0x12:    //聆听模式
        break;
    case 0x13:    //双耳录音
        break;
    case 0x14:    //语音唤醒
        break;
    case 0x15:    //免摘对话
        break;
    case 0x16:    //耳机安全提醒
        break;
    case 0x17:    //长续航模式
        break;
    case 0x18:    //高音质模式
        break;
    case 0x19:    //语音快捷指今
        break;
    case 0x1A:    //抗风噪开关
        break;
    case 0x1B:    //环绕音效(即耳机端空间音频)
        break;
    case 0x1C:    //智能音量
        break;
    case 0x1D:    //低音引擎
        break;
    case 0x1E:    //保存 log
        break;
    case 0x1F:    //BLE Audio
        break;
    case 0x20:    //无缝切换
        break;
    case 0x21:    //游戏 EQ
        break;
    case 0x22:    //颈椎姿态实时监测
        break;
    case 0x23:    //颈椎疲劳提醒
        break;
    case 0x24:    //颈椎操
        break;
    case 0x25:    //AI 空间音频
        break;
    case 0x26:    //查找手机
        break;
    default:
        break;
    }
    return 0;
}

extern int norflash_write(struct device *device, void *buf, u32 len, u32 offset);
extern int norflash_read(struct device *device, void *buf, u32 len, u32 offset);
extern int norflash_erase(u32 cmd, u32 addr);
static u32 realme_ota_firmware_id_addr = 0;
static u32 realme_ota_breakpoint_addr = 0;
static u8  realme_ota_area_vaild = 0;
#define REALME_OTA_BREAKPOINT_PATH          "mnt/sdfile/app/REALME"

static u32 realme_ota_firmware_id_read(void)
{
    if (realme_ota_area_vaild == 0) {
        return 0;
    }
    u8 buf[4] = {0};
    u32 firmware_id = 0;
    memset(buf, 0xff, 4);
    norflash_read(NULL, buf, 4, realme_ota_firmware_id_addr);

    firmware_id = buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
    return firmware_id;
}

static int realme_ota_firmware_id_save(u32 firmware_id)
{
    if (realme_ota_area_vaild == 0) {
        return -1;
    }
    u8 buf[4] = {0};
    buf[0] =  firmware_id & 0xFF;
    buf[1] = (firmware_id >> 8) & 0xFF;
    buf[2] = (firmware_id >> 16) & 0xFF;
    buf[3] = (firmware_id >> 24) & 0xFF;
    printf("%s, %d, %x, %x\n", __func__, __LINE__, firmware_id, realme_ota_firmware_id_addr);
    norflash_write(NULL, &buf, 4, realme_ota_firmware_id_addr);
    return 0;
}

static u32 realme_ota_breakpoint_read(void)
{
    if (realme_ota_area_vaild == 0) {
        return 0;
    }
    // 每个0为一个4k，统计0的个数，计算出偏移量
    u8 buf[32];
    memset(buf, 0xff, sizeof(buf));
    u32 offset = 0;
    u8 i = 0;
    while (offset < (0x1000 - 4)) {  // 0x1000-4 realme ota ini area size
        norflash_read(NULL, buf, sizeof(buf), realme_ota_breakpoint_addr + offset);
        for (i = 0; i < 32; i++) {
            if (buf[i] != 0) {
                return (offset + i) * 0x1000;
            }
        }
        offset += 32;
    }
    return 0;
}

static int realme_ota_breakpoint_save(u32 offset)
{
    if (realme_ota_area_vaild == 0) {
        return -1;
    }
    u8 buf = 0;
    printf("%s, %d, %x, %x\n", __func__, __LINE__, offset, realme_ota_breakpoint_addr + offset / 0x1000 - 1);
    // 地址 / 4k获取标志位并记录
    if (0 == (offset % 0x1000)) {
        norflash_write(NULL, &buf, 1, realme_ota_breakpoint_addr + offset / 0x1000 - 1);
    }
    return 0;
}

static int realme_ota_breakpoint_clear(void)
{
    // 清除当前区域
    if (realme_ota_area_vaild == 0) {
        return -1;
    }
    norflash_erase(200, realme_ota_breakpoint_addr);
    return 0;
}

void realme_ota_breakpoint_init(void)
{
    if (realme_ota_area_vaild != 0) {
        printf("realme_ota_area_vaild(%d) != 0", realme_ota_area_vaild);
        return;
    }
    FILE *file = fopen(REALME_OTA_BREAKPOINT_PATH, "r");
    struct vfs_attr attr = {0};
    if (file == NULL) {
        printf("realme_ota_breakpoint_init file == NULL\n");
        return;
    }
    fget_attrs(file, &attr);
    extern u32 sdfile_cpu_addr2flash_addr(u32 offset);
    realme_ota_firmware_id_addr = sdfile_cpu_addr2flash_addr(attr.sclust); // 前 4 个字节存 升级文件 ID
    realme_ota_breakpoint_addr = realme_ota_firmware_id_addr + 4;
    realme_ota_area_vaild = 1;

    u32 firmware_id = realme_ota_firmware_id_read();
    u32 save_offset = realme_ota_breakpoint_read();
    printf("realme_ota_firmware_id_addr:0x%x id:0x%x\n", (u32)realme_ota_firmware_id_addr, firmware_id);
    printf("realme_ota_breakpoint addr:0x%x len:%d, save_offset:0x%x\n", (u32)realme_ota_breakpoint_addr, attr.fsize, save_offset);
}

int realme_check_upgrade_area(int update)
{
    u32 realme_breakpoint;
    if (update) {
        dual_bank_erase_other_bank(0);
    } else {
        realme_ota_breakpoint_init();
        realme_breakpoint = realme_ota_breakpoint_read();
        if (realme_breakpoint == 0) {
            realme_check_other_upgrade_bank();
        }
    }
    return 0;
}

/*****************************
        tws sync
*****************************/
#if TCFG_USER_TWS_ENABLE

#define REALME_TWS_SYNC_HDL_UUID \
	(((u8)('R' + 'E' + 'A' + 'L') << (3 * 8)) | \
	 ((u8)('M' + 'E') << (2 * 8)) | \
	 ((u8)('T' + 'W' + 'S') << (1 * 8)) | \
	 ((u8)('S' + 'Y' + 'N' + 'C') << (0 * 8)))

static void realme_tws_sync_recv_in_task(u8 *data, int len)
{
    int ret;
    ret = realme_tws_sync_data_recv(NULL, data, len);
    free(data);
}

static void realme_tws_sync_recv_in_irq(void *_data, u16 len, bool rx)
{
    int msg[4];
    u8 *data = (u8 *)_data;
    if (!rx) {  // 只有 rx 处理
        return;
    }
    u8 *buf = malloc(len);
    if (!buf) {
        return;
    }
    memcpy(buf, data, len);
    msg[0] = (int)realme_tws_sync_recv_in_task;
    msg[1] = 2;
    msg[2] = (int)buf;
    msg[3] = (int)len;
    os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
}

REGISTER_TWS_FUNC_STUB(realme_tws_sync_stub) = {
    .func_id = REALME_TWS_SYNC_HDL_UUID,
    .func = realme_tws_sync_recv_in_irq,
};


int realme_tws_sync_state_send(void)
{
    return realme_tws_sync_state_manually();
}

#endif

int test_realme_log_remain = 0;
//lib收到控制消息后，会传消息上来处理
int realme_message_callback_handler(u8 *remote_addr, int id, int opcode, u8 *data, u32 len)
{
    printf("realme_special_message:0x%x", opcode);
    put_buf(remote_addr, 6);
    int ret = 0;
    u32 offset = 0;
    u32 offset1 = 0;
    u32 firmware_id = 0;
    u32 temp32 = 0;
    switch (opcode) {
    case APP_PROTOCOL_REALME_TWS_SYNC_SEND:
#if TCFG_USER_TWS_ENABLE
        ret = tws_api_send_data_to_sibling(data, len, REALME_TWS_SYNC_HDL_UUID);
#endif
        break;
    case APP_PROTOCOL_REALME_FEATURE_SWITCH:
        //模式切换
        /* uint8_t change_feature = data[0]; */
        /* uint8_t feature_state = data[1]; */
        realme_platform_feature_switch(remote_addr, data[0], data[1]);
        break;
    case APP_PROTOCOL_REALME_FIND_MODE_SET:
        //mode 1:查找模式，0:非查找模式
        uint8_t find_mode = data[0];
        printf("APP_PROTOCOL_REALME_FIND_MODE_SET %d\n", find_mode);
        break;
    case APP_PROTOCOL_REALME_KEY_FUNC_SET:
        //设置按键功能
        break;
    case APP_PROTOCOL_REALME_FREE_MUSIC_SET:
        //开启随心听歌
        break;
    case APP_PROTOCOL_REALME_NOISE_INFO:
        if (data[0] == 1) {
            //设置降噪模式
        } else if (data[0] == 2) {
            //设置降噪范围
        }
        break;
    case APP_PROTOCOL_REALME_COMPACTNESS_DETECT:
        // 启动贴合度检测
        break;
    case APP_PROTOCOL_REALME_EQ_SET:
        // 设置eq模式
        uint8_t eq_mode = user_realme_info->eq_id;
        break;
    case APP_PROTOCOL_REALME_HIGH_VOL_GAIN_SET:
        // 设置音阶
        uint8_t high_vol_gain = user_realme_info->high_vol_gain_level;
        break;
    case APP_PROTOCOL_REALME_MULTI_CONN_INFO:
        //多连消息
        break;
    case APP_PROTOCOL_REALME_BASS_ENGINE_VALUE_SET:
        break;
    case APP_PROTOCOL_REALME_RELATE_DEVICE_INFO:
        // 设置关联设备信息
        break;
    case APP_PROTOCOL_REALME_UTC_TIME_SYNC:
        //UTC时间同步
        uint32_t time_stamp = *((u32 *)data);
        break;
    case APP_PROTOCOL_REALME_POWER_OFF_TIME_SET:
        //设置自动关机时间
        uint8_t power_off_time = user_realme_info->auto_power_off_time;
        break;
    case APP_PROTOCOL_REALME_RESTORE_FACTORY:
        //恢复出厂状态
        break;
    case APP_PROTOCOL_REALME_UPGRADE_OFFSET_SAVE:
        // offset 4 Bytes
        offset = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
        printf("##### realme upgrade save: 0x%x\n", offset);
        realme_ota_breakpoint_save(offset);

#if 0   // test breakpoint offset save
        offset1 = realme_ota_breakpoint_read();
        printf("@@@@@ realme upgrade read: 0x%x\n", offset1);

        if (offset != offset1) {
            printf("offset error\n");
            while (1);
        }
#endif

        break;
    case APP_PROTOCOL_REALME_UPGRADE_OFFSET_READ:
        offset = realme_ota_breakpoint_read();
        data[0] = offset & 0xFF;
        data[1] = (offset >> 8) & 0xFF;
        data[2] = (offset >> 16) & 0xFF;
        data[3] = (offset >> 24) & 0xFF;
        printf("##### realme upgrade read: 0x%x\n", offset);
        // offset 4 Bytes
        break;
    case APP_PROTOCOL_REALME_UPGRADE_OFFSET_CLEAR:
        printf("##### realme upgrade clear\n");
        realme_ota_breakpoint_clear();
        break;
    case APP_PROTOCOL_REALME_UPGRADE_FIRMWARE_ID:
        firmware_id = realme_ota_firmware_id_read();
        temp32 = data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
        printf("##### realme upgrade firmware id: 0x%x -> 0x%x\n", firmware_id, temp32);
        if (firmware_id != temp32) {
            realme_ota_breakpoint_clear();
            realme_ota_firmware_id_save(temp32);
            printf("##### realme firmware id save\n");
        }
        ret = REALME_STATE_SUCCESS;
        break;
    case APP_PROTOCOL_REALME_UPGRADE_SYNC:
        // 如果要控制不让升级，这里返回 error 原因，见 upgrade error code 列表
        ret = REALME_STATE_SUCCESS;
        printf("##### realme upgrade sync ret:0x%x\n", ret);
        break;
    case APP_PROTOCOL_REALME_DEBUG_START:
        printf("##### realme debug start: side(%d) level(%d) module(%d)\n", data[0], data[1], data[2]);
        test_realme_log_remain = 1025;
        ret = test_realme_log_remain; // 返回log的总字节长度，没有log则返回 0
        break;
    case APP_PROTOCOL_REALME_DEBUG_GET:
        printf("##### realme debug get size:%d\n", len); // 协议希望获取的log的字节长度
        memset(data, 'c', len); // 填充 log 数据
        if (test_realme_log_remain > len) {
            test_realme_log_remain -= len;
            ret = len;                      // 返回获取的log的字节长度
        } else {
            ret = test_realme_log_remain;   // 返回获取的log的字节长度
        }
        break;
    case APP_PROTOCOL_REALME_DEBUG_STOP:
        printf("##### realme debug stop\n");
        break;
    case APP_PROTOCOL_REALME_DEBUG_DIAGNOSTIC:
        printf("##### realme debug Diagnostic %d\n", len);
        /* puts((const char *)data); */
        break;
    }
    return ret;
}

u8 realme_platform_feature_default_state(u8 feature_id)
{
    switch (feature_id) {
    case 0x01:    //降噪
        break;
    case 0x02:    //智能降噪
        break;
    case 0x03:    //鼾声峰噪
        break;
    case 0x04:    //佩戴检测(智能播放暂停)
        break;
    case 0x05:    //LHDC or LDAC(Device 通常只会两选一)
        break;
    case 0x06:    //游戏模式
        break;
    case 0x07:    //聚会模式
        break;
    case 0x08:    //自动接听电话
        break;
    case 0x09:    //人声增强
        break;
    case 0x0A:    //耳机操控
        break;
    case 0x0B:    //听力增强(个性化听感)
        break;
    case 0x0C:    //个性化降噪
        break;
    case 0x0D:    //耳机拍照
        break;
    case 0x0E:    //Debug 功能开关
        break;
    case 0x0F:    //耳机禅定功能
        break;
    case 0x10:    //-键开关(打开/关闭所有耳机操控)
        break;
    case 0x11:    //耳机一拖二
        break;
    case 0x12:    //聆听模式
        break;
    case 0x13:    //双耳录音
        break;
    case 0x14:    //语音唤醒
        break;
    case 0x15:    //免摘对话
        break;
    case 0x16:    //耳机安全提醒
        break;
    case 0x17:    //长续航模式
        break;
    case 0x18:    //高音质模式
        break;
    case 0x19:    //语音快捷指今
        break;
    case 0x1A:    //抗风噪开关
        break;
    case 0x1B:    //环绕音效(即耳机端空间音频)
        break;
    case 0x1C:    //智能音量
        break;
    case 0x1D:    //低音引擎
        break;
    case 0x1E:    //保存 log
        break;
    case 0x1F:    //BLE Audio
        break;
    case 0x20:    //无缝切换
        break;
    case 0x21:    //游戏 EQ
        break;
    case 0x22:    //颈椎姿态实时监测
        break;
    case 0x23:    //颈椎疲劳提醒
        break;
    case 0x24:    //颈椎操
        break;
    case 0x25:    //AI 空间音频
        break;
    case 0x26:    //查找手机
        break;
    default:
        break;
    }
    return 0;
}


// 0-100 如果正在充电最高位要置一,获取不到返回0
u8 realme_platform_get_battery_left(void)
{
    return 0xC7;
}
u8 realme_platform_get_battery_right(void)
{
    return 0;
}
u8 realme_platform_get_battery_box(void)
{
    return 0;
}


/*
Bit0: 为 0 时，表示耳机在盒子里；为 1 时，表示耳机不在盒子里；
Bit1: 为 0 时，表示耳机不在耳；为 1 时，表示耳机在耳
Bit2: 为 0 时，表示盒子是关闭的；为 1 时，表示盒子是打开的(仅当有耳机在盒子时才能获取到盒子开关状态,且针对 Key=3 时，Status 的 Bit0 和 Bit1 不区分左右耳)
Bit3: 为 0 时，表示没有触发耳机安全提醒；为 1 时，表示触发了耳机安全提醒。(不支持耳机安全提醒功能时设置为 0)
*/
u8 realme_platform_get_earbuds_status_left(void)
{
    return 0;
}
u8 realme_platform_get_earbuds_status_right(void)
{
    return 0;
}
u8 realme_platform_get_earbuds_status_box(void)
{
    return 0;
}


/*
Status：(1 byte)
0 = 一般
1 = 良好
2 = 因播放音乐导致异常结束
3 = 因通话导致异常结束
4 = 因佩戴状态变化导致异常结束
5 = host 主动暂停
*/
u8 realme_platform_get_ear_compactness_left(void)
{
    return 0;
}

u8 realme_platform_get_ear_compactness_right(void)
{
    return 0;
}


u8 realme_platform_get_ear_vol_left(void)
{
    return 0;
}
u8 realme_platform_get_ear_vol_right(void)
{
    return 0;
}

/*
Type: (1 byte)表示当前耳机使用的编码方式类型
1 = SBC
2 = AAC
3 = LDAC
4 = aptx
5 = aptx_HD
6 = aptx Adaptive
7 = LHDC(3.0)
8 = LHDC(4.0 及以上)
9 = URLC
*/
u8 realme_platform_get_audio_code(void)
{
    return 1;
}

// 1:EN  2:CN
u8 realme_platform_get_language_type(void)
{
    return 1;
}

static u8 test_personalized_eq_buf[100];
static u8 test_personalized_eq_len = 0;
static u8 test_personalized_eq_num = 0;
u8 realme_platform_get_personalized_eq_idle_index(void)
{
    return 0xA; // 测试，不要和协议原定的 eq id 重复即可
}

/*
    action:(1 byte)表示当前要执行的操作
    1 = 新增
    2 = 应用
    3 = 删除(即删除某一个自定义 EQ)
    4 = 恢复默认
    5 = 删除所有自定义 EQ
   */
int realme_platform_set_personalized_eq(u8 *data, u16 len)
{
    u8 action = data[0];

    printf(">>> realme_platform_set_personalized_eq len:%d\n", len);
    put_buf(data, len);

    if (action == 3) {
        if (data[3] == 0xA) {
            printf("del eq id %x\n", data[3]);
            test_personalized_eq_num = 0;
            test_personalized_eq_len = 0;
            return 0;
        }
    }

    test_personalized_eq_num = 1;   // test to 1
    test_personalized_eq_buf[0] = test_personalized_eq_num;    // eq num
    test_personalized_eq_buf[1] = 1;    // is using ?
    memcpy(&test_personalized_eq_buf[2], &data[1], len - 1);
    test_personalized_eq_len = 2 + (len - 1);
    return 0;
}

u8 realme_platform_get_personalized_eq_num(void)
{
    return test_personalized_eq_num;
}

int realme_platform_get_personalized_eq_length(void)
{
    return test_personalized_eq_len;
}

u8 *realme_platform_get_personalized_eq_addr(void)
{
    if (user_realme_info->eq_id == 0xA) {
        test_personalized_eq_buf[1] = 1;
    } else {
        test_personalized_eq_buf[1] = 0;
    }

    return test_personalized_eq_buf;
}


static u8 test_remote_name_list[64];
static u8 test_remote_name_list_len = 0;
static u8 test_remote_addr[6];
static u8 test_remote_name[32];
static u8 test_remote_name_len = 0;

int realme_platform_set_multi_conn_info(u8 *remote_addr, u8 *data, u16 len)
{
    printf("realme_platform_set_multi_conn_info\n");
    return 0;
}


u8 realme_platform_get_multi_conn_info_num(u8 *remote_addr)
{
    return 1;
}

int realme_platform_get_multi_conn_info_length(u8 *remote_addr)
{
    return test_remote_name_list_len;
}

u8 *realme_platform_get_multi_conn_info_addr(u8 *remote_addr)
{
    return test_remote_name_list;
}

// state:      0:disconnect  1:connect
void realme_platform_connect_state_callback(u8 state, u8 *remote_addr)
{
    printf(">>>> realme connect state: %d\n", state);
    puts(">>>> remote addr:\n");
    put_buf(remote_addr, 6);
}

void realme_remote_name_callback(u8 status, u8 *addr, u8 *name)
{
    printf("realme_remote_name_callback:");
    printf("status:%x\n", status);
    printf("remote name:%s\n", name);
    printf("address:\n");
    put_buf(addr, 6);

    memcpy(test_remote_addr, addr, 6);
    test_remote_name_len = strlen((const char *)name);
    memcpy(test_remote_name, name, test_remote_name_len);
}

void realme_remote_connect_complete_callback(u8 *addr)
{
    printf("realme_remote_connect_complete_callback:");
    printf("address:\n");
    put_buf(addr, 6);

    if (0 == memcmp(addr, test_remote_addr, 6)) {
        // 下面的代码只是一个例子，只有一个设备名字，实际需要显示多个记录
        u8 offset = 0;
        memcpy(&test_remote_name_list[offset], test_remote_addr, 6);
        offset += 6;
        test_remote_name_list[offset++] = test_remote_name_len + 3; // length
        test_remote_name_list[offset++] = 0x02; // connect state: connected
        test_remote_name_list[offset++] = 0x01; // Flag: 这个手机是当前连接设备
        test_remote_name_list[offset++] = test_remote_name_len; // name length
        memcpy(&test_remote_name_list[offset], test_remote_name, test_remote_name_len);
        offset += test_remote_name_len;
        test_remote_name_list_len = offset;
        printf("offset : %d\n", offset);
    }
}


void realme_platform_get_fast_pairing_flag(u8 *flag)
{
    flag[0] = 0x01;
    flag[1] = 0x98;
}
u8 realme_platform_get_pairing_mode(void)
{
    return 0x01;
}
u8 realme_platform_get_box_earbuds_status(void)
{
    return 0x01;
}

u8 realme_platform_get_default_eq_id(void)
{
    return 0x0;
}

#endif

