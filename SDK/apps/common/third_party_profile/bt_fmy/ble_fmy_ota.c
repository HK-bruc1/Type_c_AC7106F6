/*********************************************************************************************
    *   Filename        : .c

    *   Description     :

    *   Author          : JM

    *   Email           : zh-jieli.com

    *   Last modifiled  : 2017-01-17 11:14

    *   Copyright:(c)JIELI  2011-2016  @ , All Rights Reserved.
*********************************************************************************************/
/* #include "system/app_core.h" */
#include "system/includes.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/btstack_task.h"
#include "btstack/bluetooth.h"
#include "user_cfg.h"
#include "vm.h"
#include "btcontroller_modules.h"
#include "bt_common.h"
#include "3th_profile_api.h"
#include "le_common.h"
/* #include "rcsp_bluetooth.h" */
#include "JL_rcsp_api.h"
#include "custom_cfg.h"
#include "btstack/btstack_event.h"
#include "gatt_common/le_gatt_common.h"
#include "ble_fmy.h"
#include "ble_fmy_ota.h"
#include "system/malloc.h"
#include "dual_bank_updata_api.h"
#include "update/update.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & FMNA_EN)

#define rr_printf(x, ...)  printf("\e[31m\e[1m" x "\e[0m", ## __VA_ARGS__)
#define gg_printf(x, ...)  printf("\e[32m\e[1m" x "\e[0m", ## __VA_ARGS__)
#define yy_printf(x, ...)  printf("\e[33m\e[1m" x "\e[0m", ## __VA_ARGS__)

#if 0
#define log_info(x, ...)  printf("[FMY_OTA]" x "\r\n", ## __VA_ARGS__)
#define log_error(x, ...) printf("[FMY_OTA_ERR]" x "\r\n", ## __VA_ARGS__)
#define log_info_hexdump  put_buf

#else
#define log_info(...)
#define log_error(...)
#define log_info_hexdump(...)
#endif

#define OTA_WRITE_FLASH_SIZE       (1024)

typedef struct {
    u32 old_sys_clk;
    u32 file_size;
    u32 version;
    u32 recv_size;
    u16 ota_packet_num;
    volatile u8 ota_state;
    u8 res_byte;
    volatile u32 buff_size;
    u8  buff[OTA_WRITE_FLASH_SIZE + 32];
} fmy_ota_t;

static fmy_ota_t *fmy_ota;

#define FMY_OTA_IS_ENABLE()   (fmy_ota != NULL)

//--------------------------------------------------------------------------------------------

static void fmy_ota_set_state(u8 state);
extern u8 dual_bank_update_verify_without_crc(int (*verify_result_hdl)(int calc_crc));

//--------------------------------------------------------------------------------------------

extern u32 dual_bank_passive_update_exit(void *priv);
extern void set_ota_status(u8 stu);
static int fmy_ota_init(void)
{
    log_info("%s", __FUNCTION__);
    // 关闭flash写保护
    norflash_set_write_protect_remove();
    dual_bank_passive_update_exit(NULL);
    if (!fmy_ota) {
        fmy_ota = malloc(sizeof(fmy_ota_t));
    }
    if (fmy_ota) {
        memset(fmy_ota, 0, sizeof(fmy_ota_t));
        set_ota_status(1);
        fmy_state_idle_set_active(true);
        return FMNA_OTA_OP_SUCC;
    }
    log_error("%s malloc fail", __FUNCTION__);
    return FMNA_OTA_OP_MALLOC_FAIL;
}

static int fmy_ota_exit(void)
{
    log_info("%s", __FUNCTION__);
    dual_bank_passive_update_exit(NULL);
    set_ota_status(0);
    if (fmy_ota) {
        local_irq_disable();
        free(fmy_ota);
        fmy_ota = NULL;
        local_irq_enable();
    }
    fmy_ota_set_state(FMY_OTA_STATE_IDLE);
    fmy_state_idle_set_active(false);
    norflash_set_write_protect_en();
    return FMNA_OTA_OP_SUCC;
}

static void fmy_ota_set_state(u8 state)
{
    if (FMY_OTA_IS_ENABLE()) {
        fmy_ota->ota_state = state;
    }
}

u8 fmy_ota_get_state(void)
{
    if (FMY_OTA_IS_ENABLE()) {
        return fmy_ota->ota_state;
    }
    return FMY_OTA_STATE_IDLE;
}

static void fmy_ota_reset(void *priv)
{
    log_info("cpu_reset!");
    cpu_reset();
}

static int fmy_ota_boot_info_cb(int err)
{
    log_info("fmy_ota_boot_info_cb:%d", err);
    if (err == 0) {
        sys_timeout_add(NULL, fmy_ota_reset, 2000);
    } else {
        log_error("update head fail");
    }
    return 0;
}

static int fmy_clk_resume(int priv)
{
    /* clk_set("sys", fmy_ota->old_sys_clk);     //恢复时钟 */
    return 0;
}


//传输写入结束
extern u32 dual_bank_update_burn_boot_info(int (*burn_boot_info_result_hdl)(int err));
static int fmy_ota_write_complete(void)
{
    int status = FMNA_OTA_OP_SUCC;
    //把最后一次OTA_DATA的信号量pend到再开始校验
    /* fmy_ota->old_sys_clk = clk_get("sys"); */
    /* clk_set("sys", 96 * 1000000L); //提升系统时钟提高校验速度 */
    if (dual_bank_update_verify_without_crc(fmy_clk_resume) == 0) {
        log_info("update succ");
        fmy_ota_set_state(FMY_OTA_STATE_IDLE);
        dual_bank_update_burn_boot_info(fmy_ota_boot_info_cb);
    } else {
        log_error("update fail");
        status = FMNA_OTA_OP_CRC_FAIL;
    }

    set_ota_status(0);
    return status;
}

//每一笔数据写入的回调
static int fmy_update_write_cb(void *priv)
{
    int err = (int)priv;
    log_info("flash write %d bytes Complete,err %d", fmy_ota->buff_size, err);
    if (err) {
        fmy_ota_set_state(FMY_OTA_STATE_WRITE_ERROR);
    }

    fmy_ota->buff_size = 0;
    return 0;
}

extern u32 dual_bank_passive_update_init(u32 fw_crc, u32 fw_size, u16 max_pkt_len, void *priv);
extern u32 dual_bank_update_allow_check(u32 fw_size);
extern u32 dual_bank_update_write(void *data, u16 len, int (*write_complete_cb)(void *priv));
int fmy_ota_process(uarp_cmd_type_t cmd_type, u8 *recv_data, u32 recv_len)
{
    int ret;
    int status = FMNA_OTA_OP_SUCC;
    log_info("cmd_type= %d,len= %d", cmd_type, recv_len);

    if (!FMY_OTA_SUPPORT_CONFIG) {
        log_error("OTA config err!");
        return FMNA_OTA_OP_OTHER_ERR;
    }

    if (false == fmy_check_capabilities_is_enalbe(FMY_CAPABILITY_SUPPORTS_FW_UPDATE_SERVICE)) {
        log_error("OTA not support!");
        return  FMNA_OTA_OP_INIT_FAIL;
    }

    if (!FMY_OTA_IS_ENABLE() && cmd_type != FMNA_UARP_OTA_REQ) {
        log_error("OTA not running!");
        return  FMNA_OTA_OP_INIT_FAIL;
    }

    switch (cmd_type) {
    case FMNA_UARP_OTA_REQ:
        status = fmy_ota_init();
        if (status == FMNA_OTA_OP_SUCC) {
            fmy_ota_set_state(FMY_OTA_STATE_REQ);
            fmy_ota->version = little_endian_read_32(recv_data, 0);
            log_info("OTA_REQ,version= %08x", fmy_ota->version);
        }
        break;

    case FMNA_UARP_OTA_FILE_INFO:
        fmy_ota_set_state(FMY_OTA_STATE_CHECK_FILESIZE);
        fmy_ota->file_size = little_endian_read_32(recv_data, 0);
        log_info("OTA_FILE_INFO file_size= %d", fmy_ota->file_size);
        /* log_info("task:%s", os_current_task()); */
        ret = dual_bank_passive_update_init(0, fmy_ota->file_size, OTA_WRITE_FLASH_SIZE, NULL);

        if (0 == ret) {
            ret = dual_bank_update_allow_check(fmy_ota->file_size);
            if (ret) {
                log_error("check err: %d", ret);
                status = FMNA_OTA_OP_NO_SPACE;
            }
        } else {
            log_error("init err: %d", ret);
            status = FMNA_OTA_OP_INIT_FAIL;
        }
        break;

    case FMNA_UARP_OTA_DATA: {
        if (fmy_ota_get_state() == FMY_OTA_STATE_WRITE_ERROR) {
            ret = 1;
            goto write_end;
        }

        int wait_cnt = 200;//wait flash write complete, timeout is 2 second
        while (wait_cnt && fmy_ota->buff_size >= OTA_WRITE_FLASH_SIZE) {
            putchar('&');
            wait_cnt--;
            os_time_dly(1);
        }

        if (fmy_ota->buff_size && wait_cnt == 0) {
            ret = 2;
            goto write_end;
        }

        fmy_ota_set_state(FMY_OTA_STATE_WRITE_DATA);
        fmy_ota->ota_packet_num++;
        fmy_ota->recv_size += recv_len;
        memcpy(fmy_ota->buff, recv_data, recv_len);
        fmy_ota->buff_size += recv_len;
        log_info("OTA_DATA: file_size= %d,recv_size= %d,packet_num= %d,frame_size= %d", \
                 fmy_ota->file_size, fmy_ota->recv_size, fmy_ota->ota_packet_num, fmy_ota->buff_size);

        log_info_hexdump(fmy_ota->buff, 32);
        ret = dual_bank_update_write(fmy_ota->buff, fmy_ota->buff_size, fmy_update_write_cb);

write_end:
        if (ret) {
            log_error("dual_write err %d", ret);
            status = FMNA_OTA_OP_WRITE_FAIL;
            /* fmy_ota_boot_info_cb(0);//to reset */
        }
    }
    break;

    case FMNA_UARP_OTA_END:
        log_info("OTA_END");
        fmy_ota_set_state(FMY_OTA_STATE_COMPLETE);
        status = fmy_ota_write_complete();
        break;

    case FMNA_UARP_OTA_DISCONNECT:
        log_info("OTA_DISCONNECT");
        if (fmy_ota_get_state() != FMY_OTA_STATE_IDLE) {
            log_error("OTA fail");
            /* fmy_ota_boot_info_cb(0); //to reset */
        }
        status = fmy_ota_exit();
        break;

    default:
        log_error("unknow uarp cmd");
        break;

    }

    if (status) {
        log_error("ota_process err= %d", status);
        fmy_ota_exit();
    }
    return status;
}


#endif

