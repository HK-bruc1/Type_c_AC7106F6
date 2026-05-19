#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".file_transfer.data.bss")
#pragma data_seg(".file_transfer.data")
#pragma const_seg(".file_transfer.text.const")
#pragma code_seg(".file_transfer.text")
#endif
#include "file_transfer.h"
#include "file_transfer_sync.h"
#include "rcsp_config.h"
#include "poweroff.h"

#if (RCSP_MODE && ((TCFG_DEV_MANAGER_ENABLE && RCSP_FILE_OPT) || RCSP_TONE_FILE_TRANSFER_ENABLE))

#if (TCFG_USER_TWS_ENABLE)

#define NO_SEM_PEND_TIME 200   //主机PEND等待时间，避免卡死
#define CALC_CRC_PEND_TIME	1000	// crc超时时间

#define FTP_FILE_DATA_RECIEVE_TIMEOUT			(3*1000)  //超时拉取数据等待时长， 超时时间到了，自动拉取

#define THIS_TASK_NAME	"rcsp_ft_tws"

#define TWS_FUNC_ID_FILE_TRANSFER_SYNC \
	(((u8)('F' + 'I' + 'L' + 'E') << (3 * 8)) | \
	 ((u8)('T' + 'R' + 'A' + 'N') << (2 * 8)) | \
	 ((u8)('S' + 'F' + 'E' + 'R') << (1 * 8)) | \
	 ((u8)('S' + 'Y' + 'N' + 'C') << (0 * 8)))

enum {
    FILE_TRANS_TWS_SYNC_INIT,
    FILE_TRANS_TWS_SYNC_INIT_RSP,
    FILE_TRANS_TWS_SYNC_SPECIAL_FLAG,
    FILE_TRANS_TWS_SYNC_SPECIAL_FLAG_RSP,
    FILE_TRANS_TWS_SYNC_CLOSE,
    FILE_TRANS_TWS_SYNC_CLOSE_RSP,
    FILE_TRANS_TWS_SYNC_OPEN,
    FILE_TRANS_TWS_SYNC_OPEN_RSP,
    FILE_TRANS_TWS_SYNC_ERASE,
    FILE_TRANS_TWS_SYNC_ERASE_RSP,
    FILE_TRANS_TWS_SYNC_WRITE,
    FILE_TRANS_TWS_SYNC_WRITE_RSP,
    FILE_TRANS_TWS_SYNC_CALC_CRC,
    FILE_TRANS_TWS_SYNC_CALC_CRC_RSP,
    FILE_TRANS_TWS_SYNC_SEEK,
    FILE_TRANS_TWS_SYNC_SEEK_RSP,
    FILE_TRANS_TWS_SYNC_PARM_EXTRA,
    FILE_TRANS_TWS_SYNC_PARM_EXTRA_RSP,
};

typedef struct file_trans_tws_opt_t {
    union {
        struct { // 主机
            OS_SEM ft_tws_sem;
            int result;
            u8 *data;
            u32 data_len;
            void *priv;
            u8 *spp_remote_addr;
            u16 ble_con_handle;
            u8 OpCode_SN;
            u8 status;
            u8 type;
        };
        struct { // 从机
            void *file_p;
            file_trans_tws_op ft_tws_op;
        };
    };
} ft_tws_opt;

static ft_tws_opt *g_ft_tws_opt;
#define __this	(g_ft_tws_opt)

extern int rcsp_file_tws_info_fill(file_trans_tws_op *ft_tws_op);
extern void rcsp_file_transfer_special_flag_set(u8 flag);
extern void tws_sniff_controle_check_disable(void);

static int tws_file_trans_to_sibling(u8 *data, u16 len)
{
    if (!(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return -1;
    }
    tws_api_tx_unsniff_req();
    tws_sniff_controle_check_disable();
    sys_auto_shut_down_disable();
    return tws_api_send_data_to_sibling(data, len, TWS_FUNC_ID_FILE_TRANSFER_SYNC);
}

static void deal_sibling_tws_file_trans(void *data, u16 len)
{
    int ret = 0;
    u32 offset = 0;
    u8 rsp_data[2] = {0};
    u8 *recv_data = (u8 *)data;
    u8 file_trans_op = recv_data[offset++];
    u8 reserved = recv_data[offset++];
    switch (file_trans_op) {
    case FILE_TRANS_TWS_SYNC_OPEN:
        char *path = (char *)&recv_data[offset];
        offset += strlen(path) + 1;
        char *mode = (char *)&recv_data[offset];
        offset += strlen(mode) + 1;

        if (__this) {
            if (__this->ft_tws_op.start_timerout) {
                sys_timer_modify(__this->ft_tws_op.start_timerout, FTP_FILE_DATA_RECIEVE_TIMEOUT * 2);
            }
            __this->file_p = __this->ft_tws_op.ft_open(path, mode);

            if (__this->file_p) {
                __this->ft_tws_op.ft_fill_filp(__this->file_p);
                rsp_data[1] = 0;
            } else {
                rsp_data[1] = -1;
            }
        } else {
            printf("slave file trans state is illegal, __this is NULL\n");
            rsp_data[1] = -1;
        }
        rsp_data[0] = FILE_TRANS_TWS_SYNC_OPEN_RSP;
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_ERASE:
        u32 erase_addr = 0;
        u32 erase_size = 0;
        memcpy(&erase_addr, recv_data + offset, sizeof(erase_addr));
        offset += sizeof(erase_addr);
        memcpy(&erase_size, recv_data + offset, sizeof(erase_size));
        offset += sizeof(erase_size);
        if (__this) {
            ret = __this->ft_tws_op.ft_erase(__this->file_p, erase_addr, erase_size);
        } else {
            printf("slave file trans state is illegal, __this is NULL\n");
            ret = -1;
        }

        rsp_data[0] = FILE_TRANS_TWS_SYNC_ERASE_RSP;
        rsp_data[1] = (u8)ret;
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_WRITE:
        u32 count = 0;
        u32 write_size = 0;
        memcpy(&count, recv_data + offset, sizeof(count));
        offset += sizeof(count);
        write_size = len - offset;
        if (__this) {
            if (__this->ft_tws_op.start_timerout) {
                sys_timer_re_run(__this->ft_tws_op.start_timerout);
            }
            ret = __this->ft_tws_op.ft_write(&recv_data[offset], write_size, count, __this->file_p);
            if (write_size != ret) {
                rsp_data[1] = -1;
            } else {
                rsp_data[1] = 0;
            }
        } else {
            printf("slave file trans state is illegal, __this is NULL\n");
            rsp_data[1] = -1;
        }
        rsp_data[0] = FILE_TRANS_TWS_SYNC_WRITE_RSP;
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_CALC_CRC:
        u16 crc = 0;
        u32 calc_size = 0;
        memcpy(&crc, recv_data + offset, sizeof(crc));
        offset += sizeof(crc);
        memcpy(&calc_size, recv_data + offset, sizeof(calc_size));
        offset += sizeof(calc_size);
        if (__this) {
            __this->ft_tws_op.ft_last_pack_set(1);
            if (__this->ft_tws_op.ft_slave_file_check(crc, calc_size)) {
                rsp_data[0]	= FILE_TRANS_TWS_SYNC_CALC_CRC_RSP;
                rsp_data[1] = -1;
                tws_file_trans_to_sibling(rsp_data, 2);
            }
        } else {
            printf("slave file trans state is illegal, __this is NULL\n");
            rsp_data[0]	= FILE_TRANS_TWS_SYNC_CALC_CRC_RSP;
            rsp_data[1] = -1;
            tws_file_trans_to_sibling(rsp_data, 2);
        }
        break;
    case FILE_TRANS_TWS_SYNC_CLOSE:
        if (__this) {
            __this->ft_tws_op.ft_close();
            rsp_data[1] = 0;
        } else {
            printf("slave file trans state is illegal, __this is NULL\n");
            rsp_data[1] = -1;
        }

        rsp_data[0] = FILE_TRANS_TWS_SYNC_CLOSE_RSP;
        if (__this) {
            free(__this);
            __this = NULL;
        }
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_INIT:
        rsp_data[0]	= FILE_TRANS_TWS_SYNC_INIT_RSP;
        if (NULL == __this) {
            rcsp_file_transfer_init(NULL, 0, NULL);
            __this = (ft_tws_opt *)zalloc(sizeof(ft_tws_opt));
            if (__this && rcsp_file_tws_info_fill(&__this->ft_tws_op)) {
                printf("%s, alloc fail\n", __func__);
                rsp_data[1] = -1;
            } else {
                printf("%s, init succ\n", __func__);
                rsp_data[1] = 0;
            }
        } else {
            printf("slave file trans fp is exist\n");
        }
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_SPECIAL_FLAG:
        u8 flag = recv_data[offset];
        rcsp_file_transfer_special_flag_set(flag);

        rsp_data[0] = FILE_TRANS_TWS_SYNC_SPECIAL_FLAG_RSP;
        rsp_data[1] = 0;
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_SEEK:
        int file_offset = 0;
        int orig = 0;
        memcpy(&file_offset, recv_data + offset, sizeof(file_offset));
        offset += sizeof(file_offset);
        memcpy(&orig, recv_data + offset, sizeof(orig));
        offset += sizeof(orig);
        if (__this) {
            __this->ft_tws_op.ft_seek(__this->file_p, file_offset, orig);
            rsp_data[1] = 0;
        } else {
            printf("slave file trans state is illegal, __this is NULL\n");
            rsp_data[1] = -1;
        }

        rsp_data[0] = FILE_TRANS_TWS_SYNC_PARM_EXTRA_RSP;
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_PARM_EXTRA:
        u16 data_size = len - offset;
        rcsp_file_transfer_download_parm_extra(0, recv_data + offset, data_size, 0, NULL);

        rsp_data[0] = FILE_TRANS_TWS_SYNC_PARM_EXTRA_RSP;
        rsp_data[1] = 0;
        tws_file_trans_to_sibling(rsp_data, 2);
        break;
    case FILE_TRANS_TWS_SYNC_OPEN_RSP:
    case FILE_TRANS_TWS_SYNC_CLOSE_RSP:
    case FILE_TRANS_TWS_SYNC_ERASE_RSP:
    case FILE_TRANS_TWS_SYNC_WRITE_RSP:
    case FILE_TRANS_TWS_SYNC_CALC_CRC_RSP:
    case FILE_TRANS_TWS_SYNC_INIT_RSP:
    case FILE_TRANS_TWS_SYNC_SPECIAL_FLAG_RSP:
    case FILE_TRANS_TWS_SYNC_SEEK_RSP:
    case FILE_TRANS_TWS_SYNC_PARM_EXTRA_RSP:
        __this->result = recv_data[1];
        os_sem_post(&__this->ft_tws_sem);
        break;
    }

    if (data) {
        free(data);
    }
}

static void deal_sibling_tws_file_trans_in_irq(void *data, u16 len, bool rx)
{
    if (rx)	 {
        void *buf = malloc(len);
        if (NULL == buf) {
            printf("%s, malloc fail len %d\n", __func__, len);
            return;
        }
        memcpy(buf, data, len);
        int msg[] = {(int)deal_sibling_tws_file_trans, 2, (int)buf, len};
        os_taskq_post_type("app_core", Q_CALLBACK, ARRAY_SIZE(msg), msg);
    }
}

REGISTER_TWS_FUNC_STUB(file_transfer_sync_stub) = {
    .func_id = TWS_FUNC_ID_FILE_TRANSFER_SYNC,
    .func    = deal_sibling_tws_file_trans_in_irq,
};

u8 file_trans_request_sem(u8 cmd)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return 0;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return 0;
    }

    int timeout = NO_SEM_PEND_TIME;
    switch (cmd) {
    case FILE_TRANS_TWS_SYNC_CALC_CRC:
        timeout = CALC_CRC_PEND_TIME;
    case FILE_TRANS_TWS_SYNC_OPEN:
    case FILE_TRANS_TWS_SYNC_CLOSE:
    case FILE_TRANS_TWS_SYNC_ERASE:
    case FILE_TRANS_TWS_SYNC_WRITE:
    case FILE_TRANS_TWS_SYNC_INIT:
    case FILE_TRANS_TWS_SYNC_SPECIAL_FLAG:
    case FILE_TRANS_TWS_SYNC_SEEK:
    case FILE_TRANS_TWS_SYNC_PARM_EXTRA:
        if (OS_TIMEOUT == os_sem_pend(&__this->ft_tws_sem, timeout)) {
            printf("err:%s, %x timeout\n", __func__, cmd);
            return -1;
        }
        if (__this->result) {
            printf("err:%s, slave opt is fail %x, %x\n", __func__, __this->result, cmd);
            return __this->result;
        }
        break;
    }
    return 0;
}

void file_trans_open_tws_sync_to_slave(const char *path, const char *mode)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return;
    }

    int ret = 0;
    u8 retry = 5;
    u32 data_len = 2 + strlen(path) + 1 + strlen(mode) + 1;
    u8 *data = zalloc(data_len);
    data[0] = FILE_TRANS_TWS_SYNC_OPEN;
    data[1] = 0;
    memcpy(&data[2], path, strlen(path) + 1);
    memcpy(&data[2 + strlen(path) + 1], mode, strlen(mode) + 1);
    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }
    if (data) {
        free(data);
    }
}

u8 file_trans_open_tws_sync_pend(void)
{
    return file_trans_request_sem(FILE_TRANS_TWS_SYNC_OPEN);
}

void file_trans_close_tws_sync_to_slave(void)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return;
    }

    int ret = 0;
    u8 retry = 5;
    u32 data_len = 2;
    u8 *data = zalloc(data_len);
    data[0] = FILE_TRANS_TWS_SYNC_CLOSE;
    data[1] = 0;

    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }
    if (data) {
        free(data);
    }
}

u8 file_trans_close_tws_sync_pend(void)
{
    u8 ret = file_trans_request_sem(FILE_TRANS_TWS_SYNC_CLOSE);
    if (__this) {
        // 退出线程
        int msg[2] = {0};
        os_taskq_post_type(THIS_TASK_NAME, -1, sizeof(msg) / sizeof(msg[0]), msg);
    }
    return ret;
}

void file_trans_erase_tws_sync_to_slave(u32 erase_addr, u32 erase_size)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return;
    }

    int ret = 0;
    u8 retry = 5;
    u32 data_len = 2 + sizeof(erase_addr) + sizeof(erase_size);
    u8 *data = zalloc(data_len);
    data[0] = FILE_TRANS_TWS_SYNC_ERASE;
    data[1] = 0;
    memcpy(&data[2], &erase_addr, sizeof(erase_addr));
    memcpy(&data[2 + sizeof(erase_addr)], &erase_size, sizeof(erase_size));

    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }
    if (data) {
        free(data);
    }
}

u8 file_trans_erase_tws_sync_pend(void)
{
    return file_trans_request_sem(FILE_TRANS_TWS_SYNC_ERASE);
}

void file_trans_write_tws_sync_to_slave(void *buf, u32 size, u32 count)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return;
    }
    u32 offset = 0;
    u32 data_len = 2 + size + sizeof(count);
    u8 *data = zalloc(data_len);
    data[offset++] = FILE_TRANS_TWS_SYNC_WRITE;
    data[offset++] = 0;
    memcpy(data + offset, &count, sizeof(count));
    offset += sizeof(count);
    memcpy(data + offset, buf, size);
    offset += size;

    int ret = 0;
    u8 retry = 5;
    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }
    if (data) {
        free(data);
    }
}

u8 file_trans_write_tws_sync_pend(void)
{
    return file_trans_request_sem(FILE_TRANS_TWS_SYNC_WRITE);
}

void file_trans_calc_crc_tws_sync_to_slave(u16 crc, u32 size)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return;
    }

    u32 offset = 0;
    u32 data_len = 2 + sizeof(crc) + sizeof(size);
    u8 *data = zalloc(data_len);
    data[offset++] = FILE_TRANS_TWS_SYNC_CALC_CRC;
    data[offset++] = 0;
    memcpy(data + offset, &crc, sizeof(crc));
    offset += sizeof(crc);
    memcpy(data + offset, &size, sizeof(size));
    offset += sizeof(size);

    int ret = 0;
    u8 retry = 5;
    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }
    if (data) {
        free(data);
    }
}

u8 file_trans_calc_crc_tws_sync_pend(void)
{
    return file_trans_request_sem(FILE_TRANS_TWS_SYNC_CALC_CRC);
}

void file_trans_slave_calc_crc_tws_sync_post(u8 reason)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return;
    }
    if (TWS_ROLE_MASTER == tws_api_get_role()) {
        return;
    }
    u32 offset = 0;
    u8 rsp_data[2];

    rsp_data[0] = FILE_TRANS_TWS_SYNC_CALC_CRC_RSP;
    rsp_data[1] = reason;
    tws_file_trans_to_sibling(rsp_data, 2);
}

extern void bt_sniff_enable();
u8 file_trans_init_tws_sync(void)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return 0;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return 0;
    }

    // 初始化sem
    /* os_sem_create(&__this->ft_tws_sem, 0); */
    tws_api_auto_role_switch_disable();
    bt_sniff_enable();

    int ret = 0;
    u32 offset = 0;
    u32 data_len = 2;
    u8 *data = zalloc(data_len);
    data[offset++] = FILE_TRANS_TWS_SYNC_INIT;
    data[offset++] = 0;
    u8 retry = 5;

    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }

    if (0 == ret) {
        ret = file_trans_request_sem(FILE_TRANS_TWS_SYNC_INIT);
        if (__this->result) {
            ret = -1;
        }
    }

    if (data) {
        free(data);
    }
    return ret;
}

u8 file_trans_special_flag_tws_sync(u8 flag)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return 0;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return 0;
    }

    int ret = 0;
    u32 offset = 0;
    u32 data_len = 3;
    u8 *data = zalloc(data_len);
    data[offset++] = FILE_TRANS_TWS_SYNC_SPECIAL_FLAG;
    data[offset++] = 0;
    data[offset++] = flag;
    u8 retry = 5;

    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }

    if (0 == ret) {
        ret = file_trans_request_sem(FILE_TRANS_TWS_SYNC_SPECIAL_FLAG);
        if (__this->result) {
            ret = -1;
        }
    }

    if (data) {
        free(data);
    }
    return ret;
}

u8 file_trans_seek_tws_sync(int file_offset, int orig)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return 0;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return 0;
    }

    int ret = 0;
    u32 offset = 0;
    u32 data_len = 2 + sizeof(file_offset) + sizeof(orig);
    u8 *data = zalloc(data_len);
    data[offset++] = FILE_TRANS_TWS_SYNC_SEEK;
    data[offset++] = 0;
    memcpy(data + offset, &file_offset, sizeof(file_offset));
    offset += sizeof(file_offset);
    memcpy(data + offset, &orig, sizeof(orig));
    offset += sizeof(orig);

    u8 retry = 5;
    while (retry--) {
        ret = tws_file_trans_to_sibling(data, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }

    if (0 == ret) {
        ret = file_trans_request_sem(FILE_TRANS_TWS_SYNC_SEEK);
        if (__this->result) {
            ret = -1;
        }
    }

    if (data) {
        free(data);
    }
    return ret;
}

u8 file_trans_parm_extra_tws_sync(u8 *data, u16 len)
{
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        return 0;
    }
    if (TWS_ROLE_SLAVE == tws_api_get_role()) {
        return 0;
    }

    int ret = 0;
    u32 offset = 0;
    u32 data_len = 2 + len;
    u8 *data_buf = zalloc(data_len);
    data_buf[offset++] = FILE_TRANS_TWS_SYNC_PARM_EXTRA;
    data_buf[offset++] = 0;
    memcpy(data_buf + offset, data, len);
    offset += len;

    u8 retry = 5;
    while (retry--) {
        ret = tws_file_trans_to_sibling(data_buf, data_len);
        if (ret < 0 && ret != -1) {
            os_time_dly(5);
        } else {
            break;
        }
    }

    if (0 == ret) {
        ret = file_trans_request_sem(FILE_TRANS_TWS_SYNC_PARM_EXTRA);
        if (__this->result) {
            ret = -1;
        }
    }

    if (data_buf) {
        free(data_buf);
    }
    return ret;
}

static int file_trans_tws_task_start(void)
{
    if (NULL == __this) {
        return -1;
    }
    rcsp_file_transfer_init(NULL, __this->ble_con_handle, __this->spp_remote_addr);
    rcsp_file_transfer_special_flag_set(__this->type);
    return 0;
}

static u32 file_trans_tws_exit_handle(void *priv)
{
    u32 ret = 0;

    if (__this) {
        ret = task_kill(THIS_TASK_NAME);
        printf("kill file_trans task : %s, %x\n", THIS_TASK_NAME, ret);
        if (0 == ret) {
            if (__this->data) {
                free(__this->data);
                __this->data = NULL;
            }
            free(__this);
            __this = NULL;
        }
    } else {
        printf("TASK:%s not running\n", THIS_TASK_NAME);
        ret = -1;
    }
    return ret;
}

static void file_trans_tws_exit_post_app_core(void)
{
    file_trans_tws_exit_handle(NULL);
}

static int active_task_exit(void *priv)
{
    int retry_cnt = 3;
    int argv[3];
    argv[0] = (int)file_trans_tws_exit_post_app_core;
    argv[1] = 1;
    argv[2] = 0;
    int result = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);

    while (1) {
        if (result) {
            printf("file trans exit\n");
        }
        os_time_dly(100);
        if (result && retry_cnt) {
            result = os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
            retry_cnt--;
        }
        if (0 != memcmp(os_current_task(), THIS_TASK_NAME, sizeof(THIS_TASK_NAME)) && (!result || !retry_cnt)) {
            break;
        }
    };
    return result;
}

static u32 file_trans_tws_exit(void *priv)
{
    u32 ret = 0;

#ifdef CONFIG_UCOS_ENABLE
    ret = active_task_exit(priv);
#else
    ret = file_trans_tws_exit_handle(priv);
#endif

    return ret;
}

static void file_trans_tws_loop(void *priv)
{
    int msg[32];
    int ret = 0;
    int app_res = 0;
    printf("%s\n", __func__);
    if (file_trans_tws_task_start()) {
        goto _ERR_RET;
    }

    while (1) {
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg));
        if (ret != OS_TASKQ) {
            continue;
        }

        switch (msg[0]) {
        case JL_OPCODE_FILE_TRANSFER_START:
            rcsp_file_transfer_download_start(__this->priv, __this->OpCode_SN, __this->data, __this->data_len, __this->ble_con_handle, __this->spp_remote_addr);
            break;
        case JL_OPCODE_FILE_TRANSFER_END:
            rcsp_file_transfer_download_end(__this->status, __this->data, __this->data_len, __this->ble_con_handle, __this->spp_remote_addr);
            break;
        case JL_OPCODE_FILE_TRANSFER:
            rcsp_file_transfer_download_doing(__this->data, __this->data_len, __this->ble_con_handle, __this->spp_remote_addr);
            break;
        /* case JL_OPCODE_FILE_TRANSFER_CANCEL: */
        /* 	goto _ERR_RET; */
        /* case JL_OPCODE_FILE_DELETE: */
        /* 	break; */
        case JL_OPCODE_FILE_RENAME:
            rcsp_file_transfer_file_rename(__this->status, __this->data, __this->data_len, __this->ble_con_handle, __this->spp_remote_addr);
            break;
        case JL_OPCODE_DEVICE_PARM_EXTRA:
            rcsp_file_transfer_download_parm_extra(__this->OpCode_SN, __this->data, __this->data_len, __this->ble_con_handle, __this->spp_remote_addr);
            break;
        default:
            goto _ERR_RET;
        }

        if (__this->data) {
            free(__this->data);
            __this->data = NULL;
        }
    }
_ERR_RET:
    printf("file trans process %x err:%d\n", msg[0], ret);
    file_trans_tws_exit(NULL);
}

u32 file_trans_start(void *priv, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u32 ret = 0;
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        rcsp_file_transfer_download_start(priv, OpCode_SN, data, len, ble_con_handle, spp_remote_addr);
        goto _ERR_RET;
    }
    if (__this) {
        int msg[2] = {0};
        __this->status = 0;
        __this->priv = priv;
        __this->OpCode_SN = OpCode_SN;
        if (__this->data) {
            free(__this->data);
            __this->data = NULL;
        }
        if (len) {
            __this->data = zalloc(len);
            if (NULL == __this->data) {
                ret = -1;
                goto _ERR_RET;
            }
        } else {
            __this->data = NULL;
        }
        __this->data_len = len;
        __this->ble_con_handle = ble_con_handle;
        __this->spp_remote_addr = spp_remote_addr;
        memcpy(__this->data, data, len);
        os_taskq_post_type(THIS_TASK_NAME, JL_OPCODE_FILE_TRANSFER_START, sizeof(msg) / sizeof(msg[0]), msg);
    } else {
        ret = (u32) - 1;
        goto _ERR_RET;
    }
_ERR_RET:
    if (ret) {
        printf("%s err:%x\n", __func__, ret);
    }
    return ret;
}

u32 file_trans_end(u8 status, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u32 ret = 0;
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        rcsp_file_transfer_download_end(status, data, len, ble_con_handle, spp_remote_addr);
        goto _ERR_RET;
    }

    if (__this) {
        int msg[2] = {0};
        __this->priv = NULL;
        __this->OpCode_SN = 0;
        __this->status = status;
        if (__this->data) {
            free(__this->data);
            __this->data = NULL;
        }
        if (len) {
            __this->data = zalloc(len);
            if (NULL == __this->data) {
                ret = -1;
                goto _ERR_RET;
            }
        } else {
            __this->data = NULL;
        }
        __this->data_len = len;
        __this->ble_con_handle = ble_con_handle;
        __this->spp_remote_addr = spp_remote_addr;
        memcpy(__this->data, data, len);
        os_taskq_post_type(THIS_TASK_NAME, JL_OPCODE_FILE_TRANSFER_END, sizeof(msg) / sizeof(msg[0]), msg);
    } else {
        ret = (u32) - 1;
        goto _ERR_RET;
    }
_ERR_RET:
    if (ret) {
        printf("%s err:%x\n", __func__, ret);
    }
    return ret;
}

u32 file_trans_handle(u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u32 ret = 0;
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        rcsp_file_transfer_download_doing(data, len, ble_con_handle, spp_remote_addr);
        goto _ERR_RET;
    }

    if (__this) {
        int msg[2] = {0};
        __this->priv = NULL;
        __this->OpCode_SN = 0;
        __this->status = 0;
        if (__this->data) {
            free(__this->data);
            __this->data = NULL;
        }
        if (len) {
            __this->data = zalloc(len);
            if (NULL == __this->data) {
                ret = -1;
                goto _ERR_RET;
            }
        } else {
            __this->data = NULL;
        }
        __this->data_len = len;
        __this->ble_con_handle = ble_con_handle;
        __this->spp_remote_addr = spp_remote_addr;
        memcpy(__this->data, data, len);
        os_taskq_post_type(THIS_TASK_NAME, JL_OPCODE_FILE_TRANSFER, sizeof(msg) / sizeof(msg[0]), msg);
    } else {
        ret = (u32) - 1;
        goto _ERR_RET;
    }
_ERR_RET:
    if (ret) {
        printf("%s err:%x\n", __func__, ret);
    }
    return ret;
}

u32 file_trans_rename(u8 status, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u32 ret = 0;
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        rcsp_file_transfer_file_rename(status, data, len, ble_con_handle, spp_remote_addr);
        goto _ERR_RET;
    }

    if (__this) {
        int msg[2] = {0};
        __this->priv = NULL;
        __this->OpCode_SN = 0;
        __this->status = status;
        if (__this->data) {
            free(__this->data);
            __this->data = NULL;
        }
        if (len) {
            __this->data = zalloc(len);
            if (NULL == __this->data) {
                ret = -1;
                goto _ERR_RET;
            }
        } else {
            __this->data = NULL;
        }
        __this->data_len = len;
        __this->ble_con_handle = ble_con_handle;
        __this->spp_remote_addr = spp_remote_addr;
        memcpy(__this->data, data, len);
        os_taskq_post_type(THIS_TASK_NAME, JL_OPCODE_FILE_RENAME, sizeof(msg) / sizeof(msg[0]), msg);
    } else {
        ret = (u32) - 1;
        goto _ERR_RET;
    }
_ERR_RET:
    if (ret) {
        printf("%s err:%x\n", __func__, ret);
    }
    return ret;
}

u32 file_trans_parm_extra(u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u32 ret = 0;
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        rcsp_file_transfer_download_parm_extra(OpCode_SN, data, len, ble_con_handle, spp_remote_addr);
        goto _ERR_RET;
    }

    if (__this) {
        int msg[2] = {0};
        __this->priv = NULL;
        __this->status = 0;
        __this->OpCode_SN = OpCode_SN;
        if (__this->data) {
            free(__this->data);
            __this->data = NULL;
        }
        if (len) {
            __this->data = zalloc(len);
            if (NULL == __this->data) {
                ret = -1;
                goto _ERR_RET;
            }
        } else {
            __this->data = NULL;
        }
        __this->data_len = len;
        __this->ble_con_handle = ble_con_handle;
        __this->spp_remote_addr = spp_remote_addr;
        memcpy(__this->data, data, len);
        os_taskq_post_type(THIS_TASK_NAME, JL_OPCODE_DEVICE_PARM_EXTRA, sizeof(msg) / sizeof(msg[0]), msg);
    } else {
        ret = (u32) - 1;
        goto _ERR_RET;
    }
_ERR_RET:
    if (ret) {
        printf("%s err:%x\n", __func__, ret);
    }
    return ret;
}

u32 file_trans_init(u8 type, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u32 ret = 0;
    if (0 == (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)) {
        rcsp_file_transfer_init(NULL, ble_con_handle, spp_remote_addr);
        rcsp_file_transfer_special_flag_set(type);
        goto _ERR_RET;
    }
    if (NULL == __this) {
        __this = (ft_tws_opt *)zalloc(sizeof(ft_tws_opt));
        if (NULL == __this) {
            printf("%s, alloc fail\n", __func__);
            return -1;
        }
        __this->spp_remote_addr = spp_remote_addr;
        __this->ble_con_handle = ble_con_handle;
        __this->type = type;
        os_sem_create(&__this->ft_tws_sem, 0);
        task_create(file_trans_tws_loop, NULL, THIS_TASK_NAME);
    } else {
        printf("TASK %s is already start\n", THIS_TASK_NAME);
        ret = -1;
    }
_ERR_RET:
    return ret;
}

#else

u32 file_trans_init(u8 type, u16 ble_con_handle, u8 *spp_remote_addr)
{
    rcsp_file_transfer_init(NULL, ble_con_handle, spp_remote_addr);
    rcsp_file_transfer_special_flag_set(type);
    return 0;
}

u32 file_trans_rename(u8 status, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    rcsp_file_transfer_file_rename(status, data, len, ble_con_handle, spp_remote_addr);
    return 0;
}

u32 file_trans_handle(u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    rcsp_file_transfer_download_doing(data, len, ble_con_handle, spp_remote_addr);
    return 0;
}

u32 file_trans_end(u8 status, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    rcsp_file_transfer_download_end(status, data, len, ble_con_handle, spp_remote_addr);
    return 0;
}

u32 file_trans_start(void *priv, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    rcsp_file_transfer_download_start(priv, OpCode_SN, data, len, ble_con_handle, spp_remote_addr);
    return 0;
}

u32 file_trans_parm_extra(u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    rcsp_file_transfer_download_parm_extra(OpCode_SN, data, len, ble_con_handle, spp_remote_addr);
    return 0;
}
#endif
#endif
