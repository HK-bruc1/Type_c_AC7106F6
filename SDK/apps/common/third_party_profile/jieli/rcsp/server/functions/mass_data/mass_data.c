#include "mass_data.h"
#include "rcsp_config.h"
#include "system/includes.h"
#include "JL_rcsp_packet.h"
#include "JL_rcsp_protocol.h"

#if (RCSP_MODE && (TCFG_DEV_MANAGER_ENABLE || RCSP_TONE_FILE_TRANSFER_ENABLE))

#define MASS_MEMORY_USE_PSRAM			0

#define MASS_DATA_OP_PARAM              0 // 参数
#define MASS_DATA_OP_DATA               1 // 数据

#define MASS_DATA_WAY_READ              0 // 获取数据
#define MASS_DATA_WAY_WRITE             1 // 发送数据

#define MASS_DATA_ERR_NO                0 // 成功
#define MASS_DATA_ERR_CRC               1 // CRC检验失败
#define MASS_DATA_ERR_SEQ               2 // SEQ不对
#define MASS_DATA_ERR_TYPE              3 // 数据类型不支持
#define MASS_DATA_ERR_WAY               4 // 传输方式不支持
#define MASS_DATA_ERR_TOTAL_LEN         5 // 数据长度超范围
#define MASS_DATA_ERR_WRITE             6 // 写数据失败
#define MASS_DATA_ERR_NO_PARAM          7 // 没有配置参数
#define MASS_DATA_ERR_OTHER             0xff // 其他类型错误

#define MASS_DATA_OP_DATA_SEQ_END       BIT(15) // 结束包标识

#define MASS_DATA_SEND_EVENT_OK         0 // OK
#define MASS_DATA_SEND_EVENT_TO         1 // 超时
#define MASS_DATA_SEND_EVENT_ERR        2 // 发送失败
#define MASS_DATA_SEND_EVENT_RESPOND    3 // 回复出错

#define MASS_DATA_VERSION               0 // 版本号

// JL_packet_get_tx_max_mtu() - sizeof(JL_PACKET) - 4 - sizeof(struct _MASSDATA_OP_DATA)
// JL_packet_get_tx_max_mtu() 通过 set_jl_mtu_send() 配置
#define MASS_DATA_DATA_LIMIT            200 // 数据长度限制

#define MASS_DATA_SEND_TIMEOUT_MS       5000 // 发送超时

#if 0
#define DEBUG_PUT_BUF                   put_buf
#else
#define DEBUG_PUT_BUF(...)
#endif

#pragma pack(1)
// OP-PARAM
struct _MASSDATA_OP_PARAM {
    u8 way;
    u8 type;
    u8 version;
    u8 dat[0];
};

struct _MASSDATA_OP_PARAM_READ {
    u16 send_data_limit;
    u16 recv_data_limit;
};

struct _MASSDATA_OP_PARAM_WRITE {
    u32 data_len;
    u16 crc;
    u16 send_data_limit;
    u16 recv_data_limit;
};

// OP-PARAM 回复
struct _MASSDATA_OP_PARAM_RESPOND {
    u8 way;
    u8 type;
    u8 version;
    u8 dat[0];
};

struct _MASSDATA_OP_PARAM_RESPOND_READ {
    u8  result;
    u32 data_len;
    u16 crc;
    u16 send_data_limit;
    u16 recv_data_limit;
};

struct _MASSDATA_OP_PARAM_RESPOND_WRITE {
    u8  result;
    u16 send_data_limit;
    u16 recv_data_limit;
};

// OP-DATA
struct _MASSDATA_OP_DATA {
    u8 type;
    u16 seq;
    u16 crc;
    u32 offset;
    u16 len;
    u8 dat[0];
};

// OP-DATA 回复
struct _MASSDATA_OP_DATA_RESPOND {
    u8 type;
    u16 seq;
    u8 result;
    u8 dat[0];
};
#pragma pack()

// recv
struct _MASSDATA_RECV {
    u32 total_len;
    u16 crc;
    u16 seq;
    u16 w;
    u8 dat[0];
};

struct _MASSDATA_RECV_CB {
    void (*cb)(void *priv, u8 *data, int len);
    void *cb_priv;
};

// send
struct _MASSDATA_SEND {
    u8  result;
    volatile u8 tx_flag : 1; // 1-发送中。0-完成
        volatile u8 asyn : 1; // 1-异步
        u16 send_limit;
        u16 seq;
        u16 tm;
        u16 resend_tm;
        void (*evt_cb)(void *priv, int event);
        void *evt_cb_priv;
        u8 *pkt_buf;
        u16 pkt_len;
        u16 offset;
        u16 dat_len;
        u16 ble_con_handle;
        u8 *spp_remote_addr;
        u8  dat[0];
    };

    static struct _MASSDATA_RECV *massdat_rx[MASS_DATA_TYPE_MAX] = {0};
static struct _MASSDATA_RECV_CB rx_cb[MASS_DATA_TYPE_MAX] = {0};

static struct _MASSDATA_SEND *massdat_tx[MASS_DATA_TYPE_MAX] = {0};

static void mass_data_asyn_send_ready_data(u8 type);
#ifdef TONE_FILE_RESERVED_AREA_NAME
static int mass_data_asyn_send_response_no_wait(u8 type, u8 OpCode_SN, u16 ble_con_handle, u8 *spp_remote_addr);
#endif
static int mass_data_asyn_send_no_wait(u8 type, u16 ble_con_handle, u8 *spp_remote_addr);
static void mass_data_asyn_send_release(u8 type, int event);

#if MASS_MEMORY_USE_PSRAM
#include "asm/psram_api.h"
#define MASS_MALLOC     malloc_psram
/* #define MASS_ZALLOC      zalloc */
#define MASS_FREE       free_psram
static inline void *MASS_ZALLOC(size_t size)
{
    void *ptr = MASS_MALLOC(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
#else
#define MASS_MALLOC     malloc
#define MASS_ZALLOC     zalloc
#define MASS_FREE       free
#endif

static void mass_data_asyn_send_release(u8 type, int event)
{
    int evt_func = 0;
    void *evt_priv;
    local_irq_disable();
    if (massdat_tx[type]) {
        evt_func = (int)massdat_tx[type]->evt_cb;
        evt_priv = massdat_tx[type]->evt_cb_priv;
        if (massdat_tx[type]->resend_tm) {
            sys_timeout_del(massdat_tx[type]->resend_tm);
            massdat_tx[type]->resend_tm = 0;
        }
        if (massdat_tx[type]->tm) {
            sys_timeout_del(massdat_tx[type]->tm);
            massdat_tx[type]->tm = 0;
        }
        if (massdat_tx[type]->pkt_buf) {
            MASS_FREE(massdat_tx[type]->pkt_buf);
            massdat_tx[type]->pkt_buf = NULL;
        }
        MASS_FREE(massdat_tx[type]);
        massdat_tx[type] = NULL;
    }
    local_irq_enable();

    log_i("send[%d] relese evt:%d \n", type, event);

    if (evt_func) {
        ((void (*)(void *, int))evt_func)(evt_priv, event);
    }
}

static void mass_data_asyn_send_to(void *priv)
{
    u8 type = (u8)priv;
    if (type >= MASS_DATA_TYPE_MAX) {
        return ;
    }
    log_e("send[%d] timeout\n", type);

    mass_data_asyn_send_release(type, MASS_DATA_SEND_EVENT_TO);
}

static void mass_data_asyn_resend_no_wait(void *priv)
{
    u8 type = (u8)priv;
    if (type >= MASS_DATA_TYPE_MAX) {
        return ;
    }
    log_i("resend \n");
    if (massdat_tx[type]->resend_tm) {
        sys_timeout_del(massdat_tx[type]->resend_tm);
        massdat_tx[type]->resend_tm = 0;
    }
    int result = mass_data_asyn_send_no_wait(type, massdat_tx[type]->ble_con_handle, massdat_tx[type]->spp_remote_addr);
    if (result == false) {
        mass_data_asyn_send_release(type, MASS_DATA_SEND_EVENT_ERR);
    }
}

#ifdef TONE_FILE_RESERVED_AREA_NAME
static void mass_data_asyn_send_read_ready_param(u8 type, u8 *data, u32 len)
{
    if (massdat_tx[type]->pkt_buf) {
        MASS_FREE(massdat_tx[type]->pkt_buf);
        massdat_tx[type]->pkt_buf = NULL;
    }
    massdat_tx[type]->pkt_len = 1 + sizeof(struct _MASSDATA_OP_PARAM) + sizeof(struct _MASSDATA_OP_PARAM_RESPOND_READ);
    massdat_tx[type]->pkt_buf = MASS_ZALLOC(massdat_tx[type]->pkt_len);
    ASSERT(massdat_tx[type]->pkt_buf);
    struct _MASSDATA_OP_PARAM *p_op_param = (struct _MASSDATA_OP_PARAM *)(massdat_tx[type]->pkt_buf + 1);
    struct _MASSDATA_OP_PARAM_RESPOND_READ *p_op_r;
    p_op_r = (struct _MASSDATA_OP_PARAM_RESPOND_READ *)p_op_param->dat;

    massdat_tx[type]->pkt_buf[0] = MASS_DATA_OP_PARAM;

    p_op_param->way = MASS_DATA_WAY_READ;
    p_op_param->type = type;
    p_op_param->version = MASS_DATA_VERSION;

    p_op_r->result = 0;
    WRITE_BIG_U16(&p_op_r->send_data_limit, MASS_DATA_DATA_LIMIT);
    WRITE_BIG_U16(&p_op_r->recv_data_limit, MASS_DATA_DATA_LIMIT);
    p_op_r->crc = CRC16(data, len);
    p_op_r->crc = READ_BIG_U16(&p_op_r->crc);
    p_op_r->data_len = READ_BIG_U32(&len);

    massdat_tx[type]->send_limit = MASS_DATA_DATA_LIMIT;
}
#endif

static void mass_data_asyn_send_ready_param(u8 type)
{
    if (massdat_tx[type]->pkt_buf) {
        MASS_FREE(massdat_tx[type]->pkt_buf);
        massdat_tx[type]->pkt_buf = NULL;
    }
    massdat_tx[type]->pkt_len = 1 + sizeof(struct _MASSDATA_OP_PARAM) + sizeof(struct _MASSDATA_OP_PARAM_WRITE);
    massdat_tx[type]->pkt_buf = MASS_ZALLOC(massdat_tx[type]->pkt_len);
    ASSERT(massdat_tx[type]->pkt_buf);
    struct _MASSDATA_OP_PARAM *p_op_param = (struct _MASSDATA_OP_PARAM *)(massdat_tx[type]->pkt_buf + 1);
    struct _MASSDATA_OP_PARAM_WRITE *p_op_w;
    p_op_w = (struct _MASSDATA_OP_PARAM_WRITE *)p_op_param->dat;

    massdat_tx[type]->pkt_buf[0] = MASS_DATA_OP_PARAM;

    p_op_param->way = MASS_DATA_WAY_WRITE;
    p_op_param->type = type;
    p_op_param->version = MASS_DATA_VERSION;

    WRITE_BIG_U32(&p_op_w->data_len, massdat_tx[type]->dat_len);
    WRITE_BIG_U16(&p_op_w->crc, CRC16(massdat_tx[type]->dat, massdat_tx[type]->dat_len));
    WRITE_BIG_U16(&p_op_w->send_data_limit, MASS_DATA_DATA_LIMIT);
    WRITE_BIG_U16(&p_op_w->recv_data_limit, MASS_DATA_DATA_LIMIT);

    massdat_tx[type]->send_limit = MASS_DATA_DATA_LIMIT;

    u32 tmp_type = type;
    massdat_tx[type]->tm = sys_timeout_add((void *)tmp_type, mass_data_asyn_send_to, MASS_DATA_SEND_TIMEOUT_MS);
}

static int mass_data_asyn_send_no_wait(u8 type, u16 ble_con_handle, u8 *spp_remote_addr)
{
    int to = 0;
    massdat_tx[type]->tx_flag = 1;

    log_i("%s, type:%d, len:%d \n", __func__, type, massdat_tx[type]->pkt_len);
    DEBUG_PUT_BUF(massdat_tx[type]->pkt_buf, massdat_tx[type]->pkt_len);

    int result = 0;
    result = JL_CMD_send(JL_OPCODE_MASS_DATA, massdat_tx[type]->pkt_buf, massdat_tx[type]->pkt_len, JL_NEED_RESPOND, ble_con_handle, spp_remote_addr);
    if (result) {
        if (result == JL_ERR_SEND_BUSY) {
            log_i("send busy \n");
            u32 tmp_type = type;
            massdat_tx[type]->ble_con_handle = ble_con_handle;
            massdat_tx[type]->spp_remote_addr = spp_remote_addr;
            massdat_tx[type]->resend_tm = sys_timeout_add((void *)tmp_type, mass_data_asyn_resend_no_wait, 20);
            return true;
        }
        log_e("send err:%d \n", result);
        return false;
    }
    return true;
}

#ifdef TONE_FILE_RESERVED_AREA_NAME
static int mass_data_asyn_send_response_no_wait(u8 type, u8 OpCode_SN, u16 ble_con_handle, u8 *spp_remote_addr)
{
    int to = 0;
    massdat_tx[type]->tx_flag = 1;

    log_i("%s, type:%d, len:%d \n", __func__, type, massdat_tx[type]->pkt_len);
    DEBUG_PUT_BUF(massdat_tx[type]->pkt_buf, massdat_tx[type]->pkt_len);

    int result = 0;
    result = JL_CMD_response_send(JL_OPCODE_MASS_DATA, JL_PRO_STATUS_SUCCESS, OpCode_SN, massdat_tx[type]->pkt_buf, massdat_tx[type]->pkt_len, ble_con_handle, spp_remote_addr);
    if (result) {
        if (result == JL_ERR_SEND_BUSY) {
            log_i("send busy \n");
            u32 tmp_type = type;
            massdat_tx[type]->ble_con_handle = ble_con_handle;
            massdat_tx[type]->spp_remote_addr = spp_remote_addr;
            massdat_tx[type]->resend_tm = sys_timeout_add((void *)tmp_type, mass_data_asyn_resend_no_wait, 20);
            return true;
        }
        log_e("send err:%d \n", result);
        return false;
    }
    return true;
}

int mass_data_async_send_read_opt(u8 type, u8 OpCode_SN, u8 *head, u16 head_len, u8 *data, u32 len, void *evt_cb_priv, void (*evt_cb)(void *priv, int event), u16 ble_con_handle, u8 *spp_remote_addr)
{
    log_i("%s, task:%s, type:%d,len:%d \n", __func__, os_current_task(), type, len);
    DEBUG_PUT_BUF(data, len);

    if (type >= MASS_DATA_TYPE_MAX) {
        return false;
    }
    struct _MASSDATA_SEND *tx_hdl = MASS_ZALLOC(sizeof(struct _MASSDATA_SEND) + head_len + len);
    local_irq_disable();
    if (massdat_tx[type]) {
        local_irq_enable();
        MASS_FREE(tx_hdl);
        log_e("why massdat_tx[%d]:0x%x \n", type, massdat_tx[type]);
        return false;
    }
    ASSERT(tx_hdl);
    massdat_tx[type] = tx_hdl;
    massdat_tx[type]->asyn = 1;
    massdat_tx[type]->dat_len = head_len + len;
    massdat_tx[type]->evt_cb = evt_cb;
    massdat_tx[type]->evt_cb_priv = evt_cb_priv;
    local_irq_enable();
    u8 *dat = massdat_tx[type]->dat;
    if (head_len) {
        memcpy(dat, head, head_len);
        dat += head_len;
    }
    memcpy(dat, data, len);
    mass_data_asyn_send_read_ready_param(type, data, len);
    int result = mass_data_asyn_send_response_no_wait(type, OpCode_SN, ble_con_handle, spp_remote_addr);
    if (result == false) {
        return false;
    }
    mass_data_asyn_send_ready_data(type);
    result = mass_data_asyn_send_no_wait(type, ble_con_handle, spp_remote_addr);
    if (result == false) {
        return false;
    }

    return true;
}
#endif

int mass_data_asyn_send_ex(u8 type, u8 *head, u16 head_len, u8 *data, u16 len, void *evt_cb_priv, void (*evt_cb)(void *priv, int event), u16 ble_con_handle, u8 *spp_remote_addr)
{
    log_i("%s, task:%s, type:%d,len:%d \n", __func__, os_current_task(), type, len);
    DEBUG_PUT_BUF(data, len);

    if (type >= MASS_DATA_TYPE_MAX) {
        return false;
    }
    struct _MASSDATA_SEND *tx_hdl = MASS_ZALLOC(sizeof(struct _MASSDATA_SEND) + head_len + len);
    local_irq_disable();
    if (massdat_tx[type]) {
        local_irq_enable();
        MASS_FREE(tx_hdl);
        log_e("why massdat_tx[%d]:0x%x \n", type, massdat_tx[type]);
        return false;
    }
    ASSERT(tx_hdl);
    massdat_tx[type] = tx_hdl;
    massdat_tx[type]->asyn = 1;
    massdat_tx[type]->dat_len = head_len + len;
    massdat_tx[type]->evt_cb = evt_cb;
    massdat_tx[type]->evt_cb_priv = evt_cb_priv;
    local_irq_enable();

    u8 *dat = massdat_tx[type]->dat;
    if (head_len) {
        memcpy(dat, head, head_len);
        dat += head_len;
    }
    memcpy(dat, data, len);

    mass_data_asyn_send_ready_param(type);
    int result = mass_data_asyn_send_no_wait(type, ble_con_handle, spp_remote_addr);
    if (result == false) {
        return false;
    }

    return true;
}

static void mass_data_asyn_send_ready_data(u8 type)
{
    if (massdat_tx[type]->pkt_buf) {
        MASS_FREE(massdat_tx[type]->pkt_buf);
        massdat_tx[type]->pkt_buf = NULL;
    }
    u16 limit_len = massdat_tx[type]->send_limit;

    massdat_tx[type]->pkt_len = 1 + sizeof(struct _MASSDATA_OP_DATA) + limit_len;
    massdat_tx[type]->pkt_buf = MASS_ZALLOC(massdat_tx[type]->pkt_len);
    ASSERT(massdat_tx[type]->pkt_buf);
    struct _MASSDATA_OP_DATA *p_op_data = (struct _MASSDATA_OP_DATA *)(massdat_tx[type]->pkt_buf + 1);

    u16 seq = massdat_tx[type]->seq;
    p_op_data->type = type;
    massdat_tx[type]->pkt_buf[0] = MASS_DATA_OP_DATA;

    u16 send_len = massdat_tx[type]->dat_len - massdat_tx[type]->offset;
    if (send_len > limit_len) {
        send_len = limit_len;
    } else {
        seq |= MASS_DATA_OP_DATA_SEQ_END;
    }
    memcpy(p_op_data->dat, &massdat_tx[type]->dat[massdat_tx[type]->offset], send_len);
    WRITE_BIG_U16(&p_op_data->seq, seq);
    WRITE_BIG_U16(&p_op_data->crc, CRC16(p_op_data->dat, send_len));
    WRITE_BIG_U32(&p_op_data->offset, massdat_tx[type]->offset);
    WRITE_BIG_U16(&p_op_data->len, send_len);
    massdat_tx[type]->pkt_len = 1 + sizeof(struct _MASSDATA_OP_DATA) + send_len;

    if (massdat_tx[type]->tm) {
        sys_timeout_del(massdat_tx[type]->tm);
        massdat_tx[type]->tm = 0;
    }
    u32 tmp_type = type;
    massdat_tx[type]->tm = sys_timeout_add((void *)tmp_type, mass_data_asyn_send_to, MASS_DATA_SEND_TIMEOUT_MS);
}

#ifdef TONE_FILE_RESERVED_AREA_NAME
extern int tone_info_analysis(char *reserve_name, char *tone_name, const char *filter_tab[]);
extern int tone_info_get(u8 *tone_info[]);
extern int tone_info_release(void);
static void mass_data_tone_release_evt_func(void *priv, int event)
{
    tone_info_release();
}

static const char *g_tone_filter_tab[] = {
    /* "conn.wts", */
    // 往上加
    0,
};

static int mass_data_asyn_send_read_ready_data_handle(u8 type, u8 OpCode_SN, u16 ble_con_handle, u8 *spp_remote_addr)
{
    int ret = 0;
    switch (type) {
    case MASS_DATA_TYPE_TONE:
        u8 *data = NULL;
        ret = tone_info_analysis(TONE_FILE_RESERVED_AREA_NAME, TONE_FILE_DEFAULT_NAME, g_tone_filter_tab);
        if (0 == ret) {
            ret = tone_info_get(&data);
            if (ret) {
                ret = mass_data_async_send_read_opt(type, OpCode_SN, NULL, 0, data, ret, NULL, mass_data_tone_release_evt_func, ble_con_handle, spp_remote_addr);
            } else {
                tone_info_release();
            }
        }
        break;
    }
    return ret;
}
#endif

void mass_data_recieve(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u8 *pkt_buf;
    u16 res_len;
    u8 result = 0;
    u8 offset = 0;
    u8 op = data[offset++];
    struct _MASSDATA_RECV *p_md_rx;

    log_i("%s, op:%d \n", __func__, op);
    DEBUG_PUT_BUF(data, len);

    if (op == MASS_DATA_OP_PARAM) {
        struct _MASSDATA_OP_PARAM op_param;
        struct _MASSDATA_OP_PARAM_RESPOND *p_respond;
        struct _MASSDATA_OP_PARAM_RESPOND_READ *p_respond_read;
        struct _MASSDATA_OP_PARAM_RESPOND_WRITE *p_respond_write;

        memcpy(&op_param, &data[offset], sizeof(struct _MASSDATA_OP_PARAM));
        offset += sizeof(struct _MASSDATA_OP_PARAM);

        if (op_param.type >= MASS_DATA_TYPE_MAX) {
            // 不支持数据类型
            log_e("no support way:%d \n", op_param.type);
            result = MASS_DATA_ERR_TYPE;
            goto __op_param_end;
        }
        if (op_param.way != MASS_DATA_WAY_WRITE) {
#ifdef TONE_FILE_RESERVED_AREA_NAME
            if (mass_data_asyn_send_read_ready_data_handle(op_param.type, OpCode_SN, ble_con_handle, spp_remote_addr)) {
                return;
            }
#endif
            // 不支持拉取
            log_e("no support way:%d \n", op_param.way);
            result = MASS_DATA_ERR_WAY;
            goto __op_param_end;
        }
        if (massdat_rx[op_param.type]) {
            MASS_FREE(massdat_rx[op_param.type]);
            massdat_rx[op_param.type] = NULL;
        }
        struct _MASSDATA_OP_PARAM_WRITE op_w;
        memcpy(&op_w, &data[offset], sizeof(struct _MASSDATA_OP_PARAM_WRITE));
        offset += sizeof(struct _MASSDATA_OP_PARAM_WRITE);
        op_w.data_len = READ_BIG_U32(&op_w.data_len);
        op_w.crc = READ_BIG_U16(&op_w.crc);
        op_w.send_data_limit = READ_BIG_U16(&op_w.send_data_limit);
        op_w.recv_data_limit = READ_BIG_U16(&op_w.recv_data_limit);

        log_i("massdat_rx[%d]:%d, crc:0x%x \n", op_param.type, op_w.data_len, op_w.crc);

        massdat_rx[op_param.type] = MASS_ZALLOC(sizeof(struct _MASSDATA_RECV) + op_w.data_len);
        ASSERT(massdat_rx[op_param.type]);
        massdat_rx[op_param.type]->total_len = op_w.data_len;
        massdat_rx[op_param.type]->crc = op_w.crc;

        result = 0;
        /* goto __op_param_end; */

__op_param_end:
        log_e("result:%d \n", result);
        if (op_param.way == MASS_DATA_WAY_READ) {
            res_len = 1 + sizeof(struct _MASSDATA_OP_PARAM_RESPOND) + sizeof(struct _MASSDATA_OP_PARAM_RESPOND_READ);
            pkt_buf = MASS_ZALLOC(res_len);
            ASSERT(pkt_buf);
            p_respond = (struct _MASSDATA_OP_PARAM_RESPOND *)(pkt_buf + 1);
            p_respond_read = (struct _MASSDATA_OP_PARAM_RESPOND_READ *)p_respond->dat;
            p_respond_read->result = result;
            WRITE_BIG_U16(&p_respond_read->send_data_limit, MASS_DATA_DATA_LIMIT);
            WRITE_BIG_U16(&p_respond_read->recv_data_limit, MASS_DATA_DATA_LIMIT);
        } else {
            res_len = 1 + sizeof(struct _MASSDATA_OP_PARAM_RESPOND) + sizeof(struct _MASSDATA_OP_PARAM_RESPOND_WRITE);
            pkt_buf = MASS_ZALLOC(res_len);
            ASSERT(pkt_buf);
            p_respond = (struct _MASSDATA_OP_PARAM_RESPOND *)(pkt_buf + 1);
            p_respond_write = (struct _MASSDATA_OP_PARAM_RESPOND_WRITE *)p_respond->dat;
            p_respond_write->result = result;
            WRITE_BIG_U16(&p_respond_write->send_data_limit, MASS_DATA_DATA_LIMIT);
            WRITE_BIG_U16(&p_respond_write->recv_data_limit, MASS_DATA_DATA_LIMIT);
        }
        p_respond->way = op_param.way;
        p_respond->type = op_param.type;
        p_respond->version = op_param.version;
        pkt_buf[0] = MASS_DATA_OP_PARAM;

        JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, (u8 *)pkt_buf, res_len, ble_con_handle, spp_remote_addr);
        MASS_FREE(pkt_buf);
        return ;
    } else if (op == MASS_DATA_OP_DATA) {
        struct _MASSDATA_OP_DATA op_data;
        struct _MASSDATA_OP_DATA_RESPOND *p_data_respond;

        memcpy(&op_data, &data[offset], sizeof(struct _MASSDATA_OP_DATA));
        offset += sizeof(struct _MASSDATA_OP_DATA);
        op_data.seq = READ_BIG_U16(&op_data.seq);
        op_data.crc = READ_BIG_U16(&op_data.crc);
        op_data.offset = READ_BIG_U16(&op_data.offset);
        op_data.len = READ_BIG_U16(&op_data.len);

        if (op_data.type >= MASS_DATA_TYPE_MAX) {
            // 不支持数据类型
            log_e("no support way:%d \n", op_data.type);
            result = MASS_DATA_ERR_TYPE;
            goto __op_data_end;
        }

        if (massdat_rx[op_data.type] == NULL) {
            // 没有配置参数
            result = MASS_DATA_ERR_NO_PARAM;
            goto __op_data_end;
        }
        p_md_rx = massdat_rx[op_data.type];
        u16 seq = op_data.seq & ~MASS_DATA_OP_DATA_SEQ_END;
        if (seq != p_md_rx->seq) {
            // 序列号错误
            log_e("err seq:%d, %d \n", seq, p_md_rx->seq);
            result = MASS_DATA_ERR_SEQ;
            goto __op_data_end;
        }
        if ((p_md_rx->w + op_data.len) > p_md_rx->total_len) {
            // 长度错误
            log_e("err len0:%d, %d, %d \n", p_md_rx->w, op_data.len, p_md_rx->total_len);
            result = MASS_DATA_ERR_TOTAL_LEN;
            goto __op_data_end;
        }
        u16 crc = CRC16(&data[offset], op_data.len);
        if (op_data.crc != crc) {
            // CRC错误
            log_e("err crc0:%x, %x \n", op_data.crc, crc);
            result = MASS_DATA_ERR_CRC;
            goto __op_data_end;
        }
        memcpy(&p_md_rx->dat[p_md_rx->w], &data[offset], op_data.len);
        p_md_rx->w += op_data.len;
        p_md_rx->seq = op_data.seq + 1;
        if (op_data.seq & MASS_DATA_OP_DATA_SEQ_END) {
            // 结束
            if (p_md_rx->w != p_md_rx->total_len) {
                // 长度错误
                log_e("err len1:%d, %d \n", p_md_rx->w, p_md_rx->total_len);
                result = MASS_DATA_ERR_TOTAL_LEN;
                goto __op_data_end;
            }
            crc = CRC16(p_md_rx->dat, p_md_rx->total_len);
            if (p_md_rx->crc != crc) {
                // CRC错误
                log_e("err crc1:%x, %x \n", op_data.crc, crc);
                result = MASS_DATA_ERR_CRC;
                goto __op_data_end;
            }

            log_i("datlen[%d]:%d \n", op_data.type, p_md_rx->total_len);
            DEBUG_PUT_BUF(p_md_rx->dat, p_md_rx->total_len);

            // 回调处理
            if (rx_cb[op_data.type].cb) {
                rx_cb[op_data.type].cb(rx_cb[op_data.type].cb_priv, p_md_rx->dat, p_md_rx->total_len);
            }

            // 清除本次传输
            MASS_FREE(massdat_rx[op_data.type]);
            massdat_rx[op_data.type] = NULL;
        }
        result = 0;
        /* goto __op_param_end; */

__op_data_end:
        log_e("result:%d \n", result);

        res_len = 1 + sizeof(struct _MASSDATA_OP_DATA_RESPOND) + 4;
        pkt_buf = MASS_ZALLOC(res_len);
        ASSERT(pkt_buf);
        p_data_respond = (struct _MASSDATA_OP_DATA_RESPOND *)(pkt_buf + 1);
        p_data_respond->type = op_data.type;
        p_data_respond->seq = op_data.seq;
        p_data_respond->result = result;
        WRITE_BIG_U16(&p_data_respond->seq, op_data.seq);
        pkt_buf[0] = MASS_DATA_OP_DATA;

        JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, (u8 *)pkt_buf, res_len, ble_con_handle, spp_remote_addr);
        MASS_FREE(pkt_buf);
        return ;
    }
}

void mass_data_respone(void *priv, u8 OpCode, u8 status, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u8 offset = 0;
    u8 op = data[offset++];

    log_i("%s, op:%d \n", __func__, op);
    DEBUG_PUT_BUF(data, len);

    if (op == MASS_DATA_OP_PARAM) {
        struct _MASSDATA_OP_PARAM_RESPOND op_param_respond;
        memcpy(&op_param_respond, &data[offset], sizeof(struct _MASSDATA_OP_PARAM_RESPOND));
        offset += sizeof(struct _MASSDATA_OP_PARAM_RESPOND);

        if (op_param_respond.way == MASS_DATA_WAY_WRITE) {
            struct _MASSDATA_OP_PARAM_RESPOND_WRITE op_w;
            memcpy(&op_w, &data[offset], sizeof(struct _MASSDATA_OP_PARAM_RESPOND_WRITE));
            offset += sizeof(struct _MASSDATA_OP_PARAM_RESPOND_WRITE);

            u16 send_limit = READ_BIG_U16(&op_w.send_data_limit);
            u16 recv_limit = READ_BIG_U16(&op_w.recv_data_limit);

            log_i("write[%d] result:%d, limit:%d,%d \n", op_param_respond.type, op_w.result, send_limit, recv_limit);

            if (op_param_respond.type >= MASS_DATA_TYPE_MAX) {
                log_e("no support type:%d \n", op_param_respond.type);
                return ;
            }
            u8 asyn = 0;
            local_irq_disable();
            if (massdat_tx[op_param_respond.type]) {
                if (send_limit && (massdat_tx[op_param_respond.type]->send_limit > send_limit)) {
                    massdat_tx[op_param_respond.type]->send_limit = send_limit;
                }
                massdat_tx[op_param_respond.type]->result = op_w.result;
                massdat_tx[op_param_respond.type]->tx_flag = 0;
                if (massdat_tx[op_param_respond.type]->asyn) {
                    asyn = 1;
                }
            }
            local_irq_enable();
            if (asyn) {
                if (op_w.result) {
                    mass_data_asyn_send_release(op_param_respond.type, MASS_DATA_SEND_EVENT_RESPOND);
                } else {
                    mass_data_asyn_send_ready_data(op_param_respond.type);
                    mass_data_asyn_send_no_wait(op_param_respond.type, ble_con_handle, spp_remote_addr);
                }
            }
        } else {
            log_e("err way:%d \n", op_param_respond.way);
        }
    } else if (op == MASS_DATA_OP_DATA) {
        struct _MASSDATA_OP_DATA_RESPOND op_data_respond;
        memcpy(&op_data_respond, &data[offset], sizeof(struct _MASSDATA_OP_DATA_RESPOND));
        offset += sizeof(struct _MASSDATA_OP_DATA_RESPOND);

        u16 seq = READ_BIG_U16(&op_data_respond.seq);
        log_i("data[%d] result:%d, seq:%d \n", op_data_respond.type, op_data_respond.result, seq);

        if (op_data_respond.type >= MASS_DATA_TYPE_MAX) {
            log_e("no support type:%d \n", op_data_respond.type);
            return ;
        }

        u8 asyn = 0;
        local_irq_disable();
        if (massdat_tx[op_data_respond.type]) {
            massdat_tx[op_data_respond.type]->result = op_data_respond.result;
            massdat_tx[op_data_respond.type]->tx_flag = 0;
            if (massdat_tx[op_data_respond.type]->asyn) {
                asyn = 1;
            }
        }
        local_irq_enable();
        if (asyn) {
            if (op_data_respond.result) {
                mass_data_asyn_send_release(op_data_respond.type, MASS_DATA_SEND_EVENT_RESPOND);
            } else {
                massdat_tx[op_data_respond.type]->seq++;
                massdat_tx[op_data_respond.type]->offset += (massdat_tx[op_data_respond.type]->pkt_len - (1 + sizeof(struct _MASSDATA_OP_DATA)));
                if (massdat_tx[op_data_respond.type]->offset >= massdat_tx[op_data_respond.type]->dat_len) {
                    mass_data_asyn_send_release(op_data_respond.type, MASS_DATA_SEND_EVENT_OK);
                } else {
                    mass_data_asyn_send_ready_data(op_data_respond.type);
                    mass_data_asyn_send_no_wait(op_data_respond.type, ble_con_handle, spp_remote_addr);
                }
            }
        }
    }
}

int mass_data_asyn_send(u8 type, u8 *data, u16 len, void *evt_cb_priv, void (*evt_cb)(void *priv, int event), u16 ble_con_handle, u8 *spp_remote_addr)
{
    return mass_data_asyn_send_ex(type, NULL, 0, data, len, evt_cb_priv, evt_cb, ble_con_handle, spp_remote_addr);
}

int tone_mass_data_send(u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    return mass_data_asyn_send(MASS_DATA_TYPE_TONE, data, len, NULL, NULL, ble_con_handle, spp_remote_addr);
}

#endif
