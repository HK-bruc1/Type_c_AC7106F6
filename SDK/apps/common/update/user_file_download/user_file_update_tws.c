#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "init.h"
#include "update_tws_new.h"
#include "dual_bank_updata_api.h"
#include "update.h"
#include "app_main.h"
#include "update_loader_download.h"

#define LOG_TAG_CONST 	UPDATE
#define LOG_TAG             "[UPDATE_TWS]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define TWS_OTA_USERFILE_TRAN_HEAD_LEN (2)

enum {
    OTA_TWS_USERFILE_START,
    OTA_TWS_USERFILE_DATA,
    OTA_TWS_USERFILE_VERIFY,
    OTA_TWS_USERFILE_EXIT,
    OTA_TWS_USERFILE_RSP,
};

struct user_file_update_ctrl {
    update_op_tws_api_t *update_tws_api;
    OS_SEM tws_ota_sem;
};

struct user_file_update_ctrl *ufuc;
#define __this (ufuc)

extern int pupdate_user_chip_init(struct userfile_info *pinfo);
extern int pupdate_user_chip_release(void);
extern int pupdate_user_chip_write(u8 *buff, u32 len);
extern int pupdate_user_chip_verify(void);
extern void tws_update_register_user_chip_update_handle(void (*update_handle)(void *data, u32 len));
extern void user_file_flash_file_download_init(void);

static int user_file_tws_ota_resp(void);

static int user_file_tws_prepare(u8 *buf, u32 len)
{
    int ret = 0;
    struct userfile_info info;
    if (!buf || len < sizeof(struct userfile_info)) {
        return -1;
    }
    memcpy(&info, buf, sizeof(struct userfile_info));
    ret = pupdate_user_chip_init(&info);

    return ret;
}


static void user_file_tws_update_handle(void *data, u32 len)
{
    int ret = 0;
    u8 *recv_data = (u8 *)data;
    u16 recv_size;
    u8 cmd;
    u16 crc;



    if (len < TWS_OTA_USERFILE_TRAN_HEAD_LEN + 1) {
        return;
    }
    cmd = recv_data[0];
    crc = recv_data[1] | (recv_data[2] << 8);

    recv_data += (TWS_OTA_USERFILE_TRAN_HEAD_LEN + 1);
    recv_size = len - (TWS_OTA_USERFILE_TRAN_HEAD_LEN + 1);

    if (recv_size && crc != CRC16(recv_data, recv_size)) {
        log_error("tran crc fail\n");
        return;
    }

    switch (cmd) {
    case OTA_TWS_USERFILE_START:
        ret = user_file_tws_prepare(recv_data, recv_size);
        if (!ret) {
            user_file_tws_ota_resp();
        }
        break;
    case OTA_TWS_USERFILE_DATA:
        ret = pupdate_user_chip_write(recv_data, recv_size);
        if (!ret) {
            user_file_tws_ota_resp();
        }
        break;
    case OTA_TWS_USERFILE_VERIFY:
        ret = pupdate_user_chip_verify();
        if (!ret) {
            user_file_tws_ota_resp();
        }
        break;
    case OTA_TWS_USERFILE_EXIT:
        ret = pupdate_user_chip_release();
        if (!ret) {
            user_file_tws_ota_resp();
        }
        break;
    case OTA_TWS_USERFILE_RSP:
        os_sem_post(&__this->tws_ota_sem);
        break;
    default:
        break;
    }
}


static int tws_ota_userfile_send_data(u8 cmd, u8 *buff, u16 len)
{
    int ret = 0;
    u16 crc = 0;
    u8 try_cnt = 1;

    u8 *data = malloc(len + TWS_OTA_USERFILE_TRAN_HEAD_LEN);
    log_debug("tws_ota_userfile_send_data\n");


    if (len) {
        crc = CRC16(buff, len);
        data[0] = crc & 0xFF;
        data[1] = (crc >> 8) & 0xFF;
        memcpy(data + TWS_OTA_USERFILE_TRAN_HEAD_LEN, buff, len);
    }


retry:
    if (__this->update_tws_api->tws_ota_user_chip_update_send(cmd, data, len + TWS_OTA_USERFILE_TRAN_HEAD_LEN)) {
        ret = -1;
        goto _ERR_RET;
    }

    if (cmd != OTA_TWS_USERFILE_RSP) {
        if (os_sem_pend(&__this->tws_ota_sem, 300) == OS_TIMEOUT) {
            log_info("userfile send data pend timeout, cnt:%d\n", try_cnt);
            if (try_cnt--) {
                goto retry;
            }
            ret = -2;
            goto _ERR_RET;
        }
    }
    log_debug("userfile send succ...\n");
_ERR_RET:
    if (data) {
        free(data);
    }
    return ret;
}

static int user_file_tws_ota_resp(void)
{
    int ret = 0;
    if (!__this) {
        return 0;
    }

    ret = tws_ota_userfile_send_data(OTA_TWS_USERFILE_RSP, NULL, 0);
    if (ret) {
        log_error("userfile tws rsp fail\n");
        return ret;
    }

    return ret;
}

static int user_file_tws_ota_exit(void)
{
    int ret = 0;
    if (!__this) {
        return 0;
    }

    ret = tws_ota_userfile_send_data(OTA_TWS_USERFILE_EXIT, NULL, 0);
    if (ret) {
        log_error("userfile tws exit fail\n");
        return ret;
    }

    return ret;
}



int user_file_tws_ota_prepare(struct userfile_info *info)
{
    int ret;
    if (!__this) {
        return 0;
    }

    if (!info) {
        return -1;
    }
    ret = tws_ota_userfile_send_data(OTA_TWS_USERFILE_START, (u8 *)info, sizeof(*info));
    if (ret) {
        log_error("%s tws prepare fail\n", info->file_name);
        return ret;
    }

    return ret;
}

int user_file_tws_ota_data_send(u8 *buf, u16 len)
{
    int ret = 0;
    if (!__this) {
        return 0;
    }

    ret = tws_ota_userfile_send_data(OTA_TWS_USERFILE_DATA, buf, len);
    if (ret) {
        log_error("userfile tws data fail\n");
        return ret;
    }

    return ret;
}

int user_file_tws_ota_check(void)
{
    int ret = 0;
    if (!__this) {
        return 0;
    }

    ret = tws_ota_userfile_send_data(OTA_TWS_USERFILE_VERIFY, NULL, 0);
    if (ret) {
        log_error("userfile tws verify fail\n");
        return ret;
    }

    return ret;

}

int user_file_update_tws_init(void)
{
    if (__this) {
        log_error("reinit userfile tws\n");
        return -1;
    }
    __this = zalloc(sizeof(*__this));
    if (__this == NULL) {
        return -2;
    }

#if (OTA_TWS_SAME_TIME_ENABLE && OTA_TWS_SAME_TIME_NEW && !OTA_TWS_SAME_TIME_NEW_LESS)
    __this->update_tws_api = get_tws_update_api();
#endif
    if (!__this->update_tws_api || !__this->update_tws_api->tws_ota_user_chip_update_send) {
        log_error("tws not connect\n");
        free(__this);
        __this = NULL;
        return 0;
    }
#if (OTA_TWS_SAME_TIME_ENABLE && OTA_TWS_SAME_TIME_NEW && !OTA_TWS_SAME_TIME_NEW_LESS)
    tws_update_register_user_chip_update_handle(user_file_tws_update_handle);
#endif
    os_sem_create(&__this->tws_ota_sem, 0);

    return 0;
}

int user_file_update_tws_deinit(void)
{
    if (!__this) {
        return 0;
    }

    user_file_tws_ota_exit();

    free(__this);
    __this = NULL;
    return 0;
}

void user_file_update_tws_init_slave(void)
{
    if (__this) {
        log_error("reinit userfile tws\n");
        return;
    }
    __this = zalloc(sizeof(*__this));
    if (__this == NULL) {
        return;
    }

#if (OTA_TWS_SAME_TIME_ENABLE && OTA_TWS_SAME_TIME_NEW && !OTA_TWS_SAME_TIME_NEW_LESS)
    __this->update_tws_api = get_tws_update_api();
#endif
    if (!__this->update_tws_api || !__this->update_tws_api->tws_ota_user_chip_update_send) {
        log_error("tws not connect\n");
        free(__this);
        __this = NULL;
        return;
    }
#if (OTA_TWS_SAME_TIME_ENABLE && OTA_TWS_SAME_TIME_NEW && !OTA_TWS_SAME_TIME_NEW_LESS)
    tws_update_register_user_chip_update_handle(user_file_tws_update_handle);
#endif
    user_file_flash_file_download_init();
}

void user_file_update_tws_deinit_slave(void)
{
    if (!__this) {
        return;
    }

    pupdate_user_chip_release();

    free(__this);
    __this = NULL;
    return;
}
