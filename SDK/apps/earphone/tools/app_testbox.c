#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".app_testbox.data.bss")
#pragma data_seg(".app_testbox.data")
#pragma const_seg(".app_testbox.text.const")
#pragma code_seg(".app_testbox.text")
#endif
#include "init.h"
#include "app_config.h"
#include "system/includes.h"
#include "chargestore/chargestore.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "app_chargestore.h"
#include "app_testbox.h"
#include "device/vm.h"
#include "btstack/avctp_user.h"
#include "app_action.h"
#include "app_main.h"
#include "app_charge.h"
#include "classic/tws_api.h"
#include "update.h"
#include "bt_ble.h"
#include "bt_tws.h"
#include "app_action.h"
#include "app_power_manage.h"
#include "bt_common.h"
#include "asm/lp_touch_key_api.h"
#include "asm/lp_touch_key_tool.h"
#include "user_cfg.h"
#include "earphone.h"
#include "fs/sdfile.h"
#include "update/testbox_uart_update.h"


#if TCFG_AUDIO_SPATIAL_EFFECT_ENABLE
#include "spatial_effect_imu.h"
#endif

#if ((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN)))
#include "app_le_connected.h"
#endif

#define LOG_TAG_CONST       APP_TESTBOX
#define LOG_TAG             "[APP_TESTBOX]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_TEST_BOX_ENABLE

//testbox sub cmd
#define CMD_BOX_BT_NAME_INFO		0x01 //获取蓝牙名
#define CMD_BOX_SDK_VERSION			0x02 //获取sdk版本信息
#define CMD_BOX_BATTERY_VOL			0x03 //获取电量
#define CMD_BOX_ENTER_DUT			0x04 //进入dut模式
#define CMD_BOX_FAST_CONN			0x05 //进入快速链接
#define CMD_BOX_VERIFY_CODE			0x06 //获取校验码
#define CMD_BOX_ENTER_STORAGE_MODE  0x0a //进入仓储模式
#define CMD_BOX_GLOBLE_CFG			0x0b //测试盒配置命令(测试盒收到CMD_BOX_TWS_CHANNEL_SEL命令后发送,需使能测试盒某些配置)
#define CMD_BOX_GET_TWS_PAIR_INFO   0x0c //测试盒获取配对信息
#if TCFG_JL_UNICAST_BOUND_PAIR_EN
#define CMD_BOX_GET_SET_MAC_ADDR	0x0d //获取mac地址并设置comm地址
#endif
#define CMD_BOX_GET_AUDIO_CHANNEL	0x0e //左右声道信息
#define CMD_BOX_CUSTOM_CODE			0xf0 //客户自定义命令处理

#define WRITE_LIT_U16(a,src)   {*((u8*)(a)+1) = (u8)(src>>8); *((u8*)(a)+0) = (u8)(src&0xff); }
#define WRITE_LIT_U32(a,src)   {*((u8*)(a)+3) = (u8)((src)>>24);  *((u8*)(a)+2) = (u8)(((src)>>16)&0xff);*((u8*)(a)+1) = (u8)(((src)>>8)&0xff);*((u8*)(a)+0) = (u8)((src)&0xff);}

#define READ_LIT_U32(a)   (*((u8*)(a))  + (*((u8*)(a)+1)<<8) + (*((u8*)(a)+2)<<16) + (*((u8*)(a)+3)<<24))

enum {
    UPDATE_MASK_FLAG_INQUIRY_VBAT_LEVEL = 0,
    UPDATE_MASK_FLAG_INQUIRY_VERIFY_CODE,
    UPDATE_MASK_FLAG_INQUIRY_SDK_VERSION,
};

static const u32 support_update_mask = BIT(UPDATE_MASK_FLAG_INQUIRY_VBAT_LEVEL) | \
                                       BIT(UPDATE_MASK_FLAG_INQUIRY_SDK_VERSION) | \
                                       BIT(UPDATE_MASK_FLAG_INQUIRY_VERIFY_CODE);

struct testbox_info {
    u8 bt_init_ok;//蓝牙协议栈初始化成功
    u8 testbox_status;//测试盒在线状态
    u8 connect_status;//通信成功
    u8 channel;//左右
    u8 event_hdl_flag;
    volatile u32 global_cfg;       //全局配置信息,控制耳机在/离仓行为
    volatile u8 keep_tws_conn_flag; //保持tws连接标志
    u8 tws_paired_flag;
};

typedef struct _tws_paired_info_t {
    u8 version;		//
    union {
        struct {
            u8 common_addr[6];
            u8 remote_addr[6];
            u8 local_addr[6];
        };

        struct {
            u8 search_aa[4];
            u8 pair_aa[4];
        };
    };

} tws_paired_info_t;

enum {
    BOX_CFG_SOFTPWROFF_AFTER_PAIRED_BIT = 0,		//测试盒配对完耳机关机
    BOX_CFG_TOUCH_TRIM_OUT_OF_BOX_BIT = 1,		//离仓执行trim操作
    BOX_CFG_TRIM_START_TIME_BIT = 2,	//离仓多久trim,unit:2s (n+1)*2,即(2~8s)

};

enum {
    PAIR_INFO_VERSION_V1 = 0,
    PAIR_INFO_VERSION_V2,
};

#define BOX_CFG_BITS_GET_FROM_MASK(mask,index,len)  (((mask & BIT(index))>>index) & (0xffffffff>>(32-len)))
static struct testbox_info info = {
    .global_cfg = BIT(BOX_CFG_SOFTPWROFF_AFTER_PAIRED_BIT),
};
#define __this  (&info)

extern const int config_btctler_eir_version_info_len;
static u8 ex_enter_dut_flag = 0;
static u8 ex_enter_storage_mode_flag = 0;//1 仓储模式, 0 普通模式
static u8 local_packet[36];

void testbox_set_bt_init_ok(u8 flag)
{
    __this->bt_init_ok = flag;
}

void testbox_set_testbox_tws_paired(u8 flag)
{
    __this->tws_paired_flag = flag;
}

u8 testbox_get_testbox_tws_paired(void)
{
    return __this->tws_paired_flag;
}

u8 testbox_get_touch_trim_en(u8 *sec)
{
    if (sec) {
        u8 sec_bits = BOX_CFG_BITS_GET_FROM_MASK(__this->global_cfg, BOX_CFG_TRIM_START_TIME_BIT, 2);
        *sec = (sec_bits + 1) * 2;
    }

    return BOX_CFG_BITS_GET_FROM_MASK(__this->global_cfg, BOX_CFG_TOUCH_TRIM_OUT_OF_BOX_BIT, 1);
}

u8 testbox_get_softpwroff_after_paired(void)
{
    return BOX_CFG_BITS_GET_FROM_MASK(__this->global_cfg, BOX_CFG_SOFTPWROFF_AFTER_PAIRED_BIT, 1);
}

u8 testbox_get_ex_enter_storage_mode_flag(void)
{
    return ex_enter_storage_mode_flag;
}

u8 testbox_get_ex_enter_dut_flag(void)
{
    return ex_enter_dut_flag;
}

u8 testbox_get_connect_status(void)
{
    return __this->connect_status;
}

void testbox_clear_connect_status(void)
{
    __this->connect_status = 0;
}

u8 testbox_get_status(void)
{
    return __this->testbox_status;
}

void testbox_clear_status(void)
{
    __this->testbox_status = 0;
    testbox_clear_connect_status();
}

u8 testbox_get_keep_tws_conn_flag(void)
{
    return __this->keep_tws_conn_flag;
}


void testbox_event_to_user(u8 *packet, u8 event, u8 size)
{
    struct testbox_event e;

    if (packet != NULL) {
        if (size > sizeof(local_packet)) {
            return;
        }
        memcpy(local_packet, packet, size);
    }
    e.event     = event;
    e.size      = size;
    e.packet    = local_packet;
    os_taskq_post_type("app_core", MSG_FROM_TESTBOX, (sizeof(e) + 3) / 4, (int *)&e);
}

static int chargestore_get_tws_paired_info(u8 *buf, u8 *len)
{
    tws_paired_info_t _info;
    u8 data_len = 0;

#ifdef CONFIG_NEW_BREDR_ENABLE
    memset((u8 *)&_info, 0x00, sizeof(tws_paired_info_t));
    _info.version = PAIR_INFO_VERSION_V2;
    bt_get_tws_local_addr(_info.local_addr);
    syscfg_read(CFG_TWS_REMOTE_ADDR, _info.remote_addr, 6);
    syscfg_read(CFG_TWS_COMMON_ADDR, _info.common_addr, 6);
    data_len = 6 + 6 + 6 + 1;
#else
    _info.version = PAIR_INFO_VERSION_V1;
#error "Not implentment!\n";
#endif

    if (len) {
        *len = data_len;
    }

    if (buf) {
        memcpy(buf, (u8 *)&_info, data_len);
    }

    //log_info("paired info len:%x\n", data_len);
    //put_buf(buf, data_len);

    return 0;
}

static u8 chargestore_get_tws_channel_info(void)
{
    u8 channel = 'U';
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &channel, 1);
    return channel;
}

static void app_testbox_sub_event_handle(u8 *data, u16 size)
{
    u8 mac = 0;
    switch (data[0]) {
    case CMD_BOX_FAST_CONN:
    case CMD_BOX_ENTER_DUT:
        __this->event_hdl_flag = 0;
        if (!app_in_mode(APP_MODE_BT)) {
            if (!app_var.goto_poweroff_flag) {
                app_var.play_poweron_tone = 0;
                app_send_message(APP_MSG_GOTO_MODE, APP_MODE_BT);
            }
        } else {
            if ((!__this->connect_status) && __this->bt_init_ok) {
                log_info("\n\nbt_page_inquiry_scan_for_test\n\n");
                __this->connect_status = 1;
                log_info("bredr_dut_enbale\n");
                bt_bredr_enter_dut_mode(1, 1);
            }
        }
        break;
    case CMD_BOX_CUSTOM_CODE:
        __this->event_hdl_flag = 0;
        if (data[1] == 0x00) {
            bt_fast_test_api();
        }
        break;
#if TCFG_JL_UNICAST_BOUND_PAIR_EN
    case CMD_BOX_GET_SET_MAC_ADDR:
        y_printf("CMD_BOX_GET_SET_MAC_ADDR 11\n");
        bt_cmd_prepare(USER_CTRL_DEL_ALL_REMOTE_INFO, 0, NULL);
        break;
#endif
    }
}

static void app_testbox_update_event_handle(u8 *data, u16 size)
{
    //如果开启了VM配置项暂存RAM功能则在每次触发升级前保存数据到vm_flash,避免丢失数据
    //vm_flush2flash();时间处理较长，不能够在串口中断处做处理，需要发送到线程进行处理
    if (get_vm_ram_storage_enable() || get_vm_ram_storage_in_irq_enable()) {
        vm_flush2flash(1);
    }
#if (defined CONFIG_CPU_BR50) || (defined CONFIG_CPU_BR52) || (defined CONFIG_CPU_BR56) // 耳机目前仅支持br50\br52

    /* if ((size >= 1) && (data[0])) { */
    if (app_testbox_enter_loader_update()) {
        return;
    }
    /* } */
#endif
    /* 打一个uartkey给maskrom识别 */
    chargestore_set_update_ram();

    /* reset之后在maskrom收loader，然后执行 */
#if defined(CONFIG_CPU_BR52)
    power_set_mode(PWR_LDO15);
#endif
    cpu_reset();
}

//事件执行函数,在任务调用
static int app_testbox_event_handler(int *msg)
{
    struct testbox_event *testbox_dev = (struct testbox_event *)msg;

    switch (testbox_dev->event) {
    case CMD_BOX_MODULE:
        app_testbox_sub_event_handle(testbox_dev->packet, testbox_dev->size);
        break;
    case CMD_BOX_UPDATE:
        app_testbox_update_event_handle(testbox_dev->packet, testbox_dev->size);
        break;
    case CMD_BOX_TWS_CHANNEL_SEL:
        __this->event_hdl_flag = 0;
#if	TCFG_USER_TWS_ENABLE
        chargestore_set_tws_channel_info(__this->channel);
#endif
        if (get_vbat_need_shutdown() == TRUE) {
            //电压过低,不进行测试
            break;
        }
        if (!app_in_mode(APP_MODE_BT)) {
            if (!app_var.goto_poweroff_flag) {
                app_var.play_poweron_tone = 0;
                app_send_message(APP_MSG_GOTO_MODE, APP_MODE_BT);
            }
        } else {
            if ((!__this->connect_status) && __this->bt_init_ok) {
                __this->connect_status = 1;
#if	TCFG_USER_TWS_ENABLE
                if (0 == __this->keep_tws_conn_flag) {
                    log_info("\n\nbt_page_scan_for_test\n\n");
                    bt_page_scan_for_test(0);
                }
#endif
            }
        }
        break;
#if TCFG_USER_TWS_ENABLE
    case CMD_BOX_TWS_REMOTE_ADDR:
        log_info("event_CMD_BOX_TWS_REMOTE_ADDR \n");
        chargestore_set_tws_remote_info(testbox_dev->packet, testbox_dev->size);
        break;
#endif
    //不是测试盒事件,返回0,未处理
    default:
        return 0;
    }
    return 1;
}

APP_MSG_HANDLER(testbox_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_TESTBOX,
    .handler    = app_testbox_event_handler,
};

static const u8 own_private_linkkey[16] = {
    0x06, 0x77, 0x5f, 0x87, 0x91, 0x8d, 0xd4, 0x23,
    0x00, 0x5d, 0xf1, 0xd8, 0xcf, 0x0c, 0x14, 0x2b
};

static void app_testbox_sub_cmd_handle(u8 *send_buf, u8 buf_len, u8 *buf, u8 len)
{
    u8 temp_len;
    u8 send_len = 0;

    send_buf[0] = buf[0];
    send_buf[1] = buf[1];

    log_info("sub_cmd:%x\n", buf[1]);

    switch (buf[1]) {
    case CMD_BOX_BT_NAME_INFO:
        temp_len = strlen(bt_get_local_name());
        if (temp_len < (buf_len - 2)) {
            memcpy(&send_buf[2], bt_get_local_name(), temp_len);
            send_len = temp_len + 2;
            chargestore_api_write(send_buf, send_len);
        } else {
            log_error("bt name buf len err\n");
        }
        break;

    case CMD_BOX_BATTERY_VOL:
        send_buf[2] = get_vbat_value();
        send_buf[3] = get_vbat_value() >> 8;
        send_buf[4] = get_vbat_percent();
        send_len = sizeof(u16) + sizeof(u8) + 2; //vbat_value;u16,vabt_percent:u8,opcode:2 bytes
        log_info("bat_val:%d %d\n", get_vbat_value(), get_vbat_percent());
        chargestore_api_write(send_buf, send_len);
        break;

    case CMD_BOX_SDK_VERSION:
        if (config_btctler_eir_version_info_len) {
            temp_len = strlen(sdk_version_info_get());
            send_len = (temp_len > (buf_len - 2)) ? buf_len : temp_len + 2;
            log_info("version:%s ver_len:%x send_len:%x\n", sdk_version_info_get(), temp_len, send_len);
            memcpy(send_buf + 2, sdk_version_info_get(), temp_len);
            chargestore_api_write(send_buf, send_len);
        }
        break;

    case CMD_BOX_FAST_CONN:
        log_info("enter fast dut\n");
        set_temp_link_key((u8 *)own_private_linkkey);
        bt_get_vm_mac_addr(&send_buf[2]);
        if (0 == __this->event_hdl_flag) {
            testbox_event_to_user(&buf[1], buf[0], 1);
            __this->event_hdl_flag = 1;
        }
        if (__this->bt_init_ok) {
            ex_enter_dut_flag = 1;
            chargestore_api_write(send_buf, 8);
        }
        break;

    case CMD_BOX_ENTER_DUT:
        log_info("enter dut\n");
        //__this->testbox_status = 1;
        ex_enter_dut_flag = 1;

        if (0 == __this->event_hdl_flag) {
            testbox_event_to_user(&buf[1], buf[0], 1);
            __this->event_hdl_flag = 1;
        }
        if (__this->bt_init_ok) {
            chargestore_api_write(send_buf, 2);
        }
        break;

    case CMD_BOX_VERIFY_CODE:
        log_info("get_verify_code\n");
        u8 *p = sdfile_get_burn_code(&temp_len);
        send_len = (temp_len > (buf_len - 2)) ? buf_len : temp_len + 2;
        memcpy(send_buf + 2, p, temp_len);
        chargestore_api_write(send_buf, send_len);
        break;

    case CMD_BOX_CUSTOM_CODE:
        log_info("CMD_BOX_CUSTOM_CODE value=%x", buf[2]);
        if (buf[2] == 0) {//测试盒自定义命令，样机进入快速测试模式
            if (0 == __this->event_hdl_flag) {
                __this->event_hdl_flag = 1;
                testbox_event_to_user(&buf[1], buf[0], 2);
            }
        }
        send_len = 0x3;
#if (defined(TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
        if (buf[2] == 1) {
            __this->testbox_status = 1;
            send_len =  testbox_imu_trim_run(send_buf);
            put_buf(send_buf, 36);
            send_len += 2;
        }
#endif /*TCFG_AUDIO_SPATIAL_EFFECT_ENABLE*/

#if TCFG_LP_TOUCH_KEY_ENABLE
        if (buf[2] == 0x6a) {//测试盒自定义命令， 0x6a 用于触摸动态阈值算法的结果显示，请大家不要冲突
            send_buf[2] = 0x6a;
            send_buf[3] = 'c';
            send_buf[4] = 's';
            send_buf[5] = 'm';
            send_buf[6] = 'r';
            send_buf[7] = lp_touch_key_alog_range_display((u8 *)&send_buf[8]);
            send_len = 8 + send_buf[7];
            log_info("send_len = %d\n", send_len);
        }
#endif

        chargestore_api_write(send_buf, send_len);
        break;

    case CMD_BOX_ENTER_STORAGE_MODE:
        log_info("CMD_BOX_ENTER_STORAGE_MODE");
        ex_enter_storage_mode_flag = 1;
        chargestore_api_write(send_buf, 2);
        break;

    case CMD_BOX_GLOBLE_CFG:
        log_info("CMD_BOX_GLOBLE_CFG:%d %x %x", len, READ_LIT_U32(buf + 2), READ_LIT_U32(buf + 6));
        __this->global_cfg = READ_LIT_U32(buf + 2);
#if 0 //for test
        u8 sec;
        u32 trim_en = testbox_get_touch_trim_en(&sec);
        log_info("box_cfg:%x %x %x\n",
                 testbox_get_softpwroff_after_paired(),
                 trim_en, sec);
#endif

        chargestore_api_write(send_buf, 2);
        break;

    case CMD_BOX_GET_TWS_PAIR_INFO:
        log_info("CMD_BOX_GET_TWS_PAIR_INFO");
        chargestore_get_tws_paired_info(send_buf + 2, &send_len);
        chargestore_api_write(send_buf, send_len + 2);
        break;

#if TCFG_JL_UNICAST_BOUND_PAIR_EN
    case CMD_BOX_GET_SET_MAC_ADDR:		// tool pair
        struct tws_pairtool_info_t pair_info = {0};
        u8 mac_tmp[6] = {0};
        if (!__this->bt_init_ok) {
            break;
        }
        y_printf("CMD_BOX_GET_SET_MAC_ADDR 22");
        put_buf(buf, len);
        __this->testbox_status = 1;
        memcpy((u8 *)&pair_info, &buf[2], sizeof(struct tws_pairtool_info_t));

        send_len = 10;//cmd2+st+tws+mac6
        send_buf[2] = 0;
#if TCFG_USER_TWS_ENABLE&&(!TCFG_TWS_CONN_DISABLE)
        send_buf[3] = 1;
#else
        send_buf[3] = 0;
#endif
        y_printf("le_audio_adv_local_mac_get()");
        put_buf(le_audio_adv_local_mac_get(), 6);
        memcpy(send_buf + 4, le_audio_adv_local_mac_get(), 6);

        if (pair_info.dev_num >= 2) {
            testbox_event_to_user(&buf[1], buf[0], len - 1);
            log_info("dev_num =%d,set comm addr=", pair_info.dev_num);
            put_buf(pair_info.common_addr, 6);

            int err_temp = syscfg_write(CFG_USER_COMMON_ADDR, pair_info.common_addr, 6);    //存在VM
            printf("err_temp:%d,write_custom_common_addr\n", err_temp);
            put_buf(pair_info.common_addr, 6);

            printf("write_custom_common_addr\n\n");
            send_buf[2] |= BIT(0);//写入common mac成功状态
#if TCFG_USER_TWS_ENABLE
            syscfg_read(CFG_TWS_REMOTE_ADDR, mac_tmp, 6);
            if (memcmp(pair_info.tws_addr, mac_tmp, 6) == 0) {
                send_buf[2] |= BIT(1);//写入tws mac成功状态
                log_info("write tws ok");
            }
#endif
        } else {
            log_info("dev offline");
        }
        y_printf("CMD_BOX_GET_SET_MAC_ADDR 33");
        put_buf(send_buf, send_len);
        chargestore_api_write(send_buf, send_len);
        break;
#endif


    case CMD_BOX_GET_AUDIO_CHANNEL:
        log_info("CMD_BOX_GET_AUDIO_CHANNEL");
        __this->testbox_status = 1;
#if TCFG_USER_TWS_ENABLE
        u8 channel = bt_tws_get_local_channel();
        if (channel == 'L') {
            send_buf[2] = 0;
        } else if (channel == 'R') {
            send_buf[2] = 1;
        } else {
            send_buf[2] = 2;
        }
#else
        send_buf[2] = 2;
#endif
        __this->channel = send_buf[2];
        printf("channel = %d\n", send_buf[2]);
        chargestore_api_write(send_buf, 3);
        break;

    default:
        send_buf[0] = CMD_UNDEFINE;
        send_len = 1;
        chargestore_api_write(send_buf, send_len);
        break;
    }

    log_info_hexdump(send_buf, send_len);
}

//数据执行函数,在串口中断调用
static int app_testbox_data_handler(u8 *buf, u8 len)
{
    u8 send_buf[36];
    send_buf[0] = buf[0];
    switch (buf[0]) {
    case CMD_BOX_MODULE:
        app_testbox_sub_cmd_handle(send_buf, sizeof(send_buf), buf, len);
        break;

    case CMD_BOX_UPDATE:
        __this->testbox_status = 1;
        printf(">>>[test]:buf13 = 0x%x, chip_id = 0x%x\n", buf[13], get_jl_chip_id());
        if (buf[13] == 0xff || buf[13] == get_jl_chip_id() || buf[13] == get_jl_chip_id2()) {
            //进行串口升级流程
            //vm_flush2flash();时间处理较长，不能够在串口中断处做处理，需要发送到线程进行处理
#if (defined CONFIG_CPU_BR50) || (defined CONFIG_CPU_BR52) || (defined CONFIG_CPU_BR56) // 耳机目前仅支持br50\br52
            if (buf[15]) { // 存在新的升级流程
                send_buf[1] = 0xAA;//回复0xAA表示使用新的串口升级流程 uart_ota2.bin
                chargestore_api_write(send_buf, 2);
            }
#endif
            testbox_event_to_user(&buf[15], CMD_BOX_UPDATE, 1);
        } else if (buf[13] == 0xff) {
            //进行电量更新流程,需要及时回复
            send_buf[1] = 0xff;
            WRITE_LIT_U32(&send_buf[2], support_update_mask);
            chargestore_api_write(send_buf, 2 + sizeof(support_update_mask));
            log_info("rsp update_mask\n");
        } else {
            send_buf[1] = 0x01;//chip id err
            chargestore_api_write(send_buf, 2);
        }
        break;
    case CMD_BOX_TWS_CHANNEL_SEL:
        __this->testbox_status = 1;
        if (len == 3) {
            __this->keep_tws_conn_flag = buf[2];
            putchar('K');
        }  else {
            __this->keep_tws_conn_flag = 0;
        }
        __this->channel = (buf[1] == TWS_CHANNEL_LEFT) ? 'L' : 'R';
#ifdef UPDATE_VOICE_REMIND
        if (0 == __this->event_hdl_flag || app_var.update_tone_end_flag == 1) {
#else
        if (0 == __this->event_hdl_flag) {
#endif
            testbox_event_to_user(NULL, CMD_BOX_TWS_CHANNEL_SEL, 0);
            __this->event_hdl_flag = 1;
        }
        if (__this->bt_init_ok) {
            len = chargestore_get_tws_remote_info(&send_buf[1]);
            chargestore_api_write(send_buf, len + 1);
        } else {
            send_buf[0] = CMD_UNDEFINE;
            chargestore_api_write(send_buf, 1);
        }
        break;
#if TCFG_USER_TWS_ENABLE
    case CMD_BOX_TWS_REMOTE_ADDR:
        __this->testbox_status = 1;
        testbox_event_to_user((u8 *)&buf[1], buf[0], len - 1);
        chargestore_api_set_timeout(100);
        break;
#endif
    //不是测试盒命令,返回0,未处理
    default:
        return 0;
    }
    return 1;
}

CHARGESTORE_HANDLE_REG(testbox, app_testbox_data_handler);


#if TCFG_CHARGE_ENABLE

#if TCFG_LP_EARTCH_KEY_ENABLE
static u8 _enter_softoff_flag = 0;
void exit_lp_touch_eartch_trim(void *priv)
{
    log_info("<<<<<<<<<< exit_lp_touch_eartch_trim!\n");
    lp_touch_key_testbox_inear_trim(0);

    //trim 结束

    if (_enter_softoff_flag) {
        log_info("<<<<<<<<<<< sys_enter_soft_poweroff after eartch_trim!\n");
        sys_enter_soft_poweroff(POWEROFF_NORMAL);
    } else {
        cpu_reset();
    }
}
void enter_lp_touch_eartch_trim(void *priv)
{
    //trim 开始
    log_info(">>>>>>>>>> enter_lp_touch_eartch_trim!\n");
    lp_touch_key_testbox_inear_trim(1);
    sys_timeout_add(NULL, exit_lp_touch_eartch_trim, 1000);
}
#endif

//外置触摸断电
void __attribute__((weak)) external_touch_key_disable(void)
{

}
//关闭vddio keep
u8 __attribute__((weak)) power_set_vddio_keep(u8 en)
{
    return 0;
}

static int testbox_charge_msg_ldo5v_off(int type)
{
#if TCFG_LP_EARTCH_KEY_ENABLE
    u8 sec = 0;
    u8 eartch_trim_en = testbox_get_touch_trim_en(&sec);
    if (eartch_trim_en) {
        //trim 准备
        log_info("******* testbox_get_touch_trim_en : %ds\n", sec);
        sys_timeout_add(NULL, enter_lp_touch_eartch_trim, sec * 1000);
    }
#endif

    if (testbox_get_status() && !bt_get_total_connect_dev()) {
        if (!testbox_get_keep_tws_conn_flag()) {
            log_info("<<<<<<<<<<<<<<testbox out and bt noconn reset>>>>>>>>>>>>>>>\n");
            if (testbox_get_testbox_tws_paired() && testbox_get_softpwroff_after_paired()) {
#if TCFG_LP_EARTCH_KEY_ENABLE
                if (eartch_trim_en) {
                    _enter_softoff_flag = 1;
                } else
#endif
                {
                    sys_enter_soft_poweroff(POWEROFF_NORMAL);
                }
            } else {
#if TCFG_LP_EARTCH_KEY_ENABLE
                if (eartch_trim_en) {
                } else
#endif
                {
                    cpu_reset();
                }
            }
        } else {
            log_info("testbox out ret\n");
        }
    }
    testbox_clear_status();

    //拔出开关机
    if ((type == LDO5V_OFF_TYPE_NORMAL_OFF) || (type == LDO5V_OFF_TYPE_NORMAL_ON)) {
        if (testbox_get_ex_enter_storage_mode_flag()) {
            power_set_vddio_keep(0);//关VDDIO KEEP
#if TCFG_LP_TOUCH_KEY_ENABLE
            lp_touch_key_disable();//仓储模式关内置触摸
#else
            external_touch_key_disable(); //仓储模式关外置触摸供电
#endif /* #if TCFG_LP_TOUCH_KEY_ENABLE */
            if (type == LDO5V_OFF_TYPE_NORMAL_ON) {
                //测试盒仓储模式使能后，断开测试盒直接关机
                power_set_soft_poweroff();
            }
        }
    }
    return 0;
}

static int app_testbox_charge_msg_handler(int msg, int type)
{
    switch (msg) {
    case CHARGE_EVENT_LDO5V_KEEP:
        if (testbox_get_ex_enter_dut_flag()) {
            putchar('D');
            return 1;//dut模式拦截关机
        }
        if (testbox_get_ex_enter_storage_mode_flag()) {
            putchar('S');
            return 1;//仓储模式拦截关机
        }
        if (testbox_get_status()) {
            log_info("testbox online!\n");
            return 1;//测试盒在线拦截关机
        }
        break;
    case CHARGE_EVENT_LDO5V_IN:
        testbox_clear_status();
        break;
    case CHARGE_EVENT_LDO5V_OFF:
        return testbox_charge_msg_ldo5v_off(type);
    }
    return 0;
}

APP_CHARGE_HANDLER(testbox_charge_msg_entry, 1) = {
    .handler = app_testbox_charge_msg_handler,
};

#endif

#endif

