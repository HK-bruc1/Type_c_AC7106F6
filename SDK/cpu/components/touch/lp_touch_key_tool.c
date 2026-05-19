
#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".lp_touch_key_tool.data.bss")
#pragma data_seg(".lp_touch_key_tool.data")
#pragma const_seg(".lp_touch_key_tool.text.const")
#pragma code_seg(".lp_touch_key_tool.text")
#endif

#include "app_config.h"

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE

#include "system/includes.h"
#include "asm/lp_touch_key_tool.h"
#include "asm/lp_touch_key_api.h"
#include "btstack/avctp_user.h"
#include "classic/tws_api.h"
#include "key_driver.h"
#include "online_db_deal.h"
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
#include "lp_touch_key_identify_algo.h"
#endif


#define LOG_TAG_CONST       LP_KEY
#define LOG_TAG             "[LP_KEY]"
/* #define LOG_ERROR_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"



//LP KEY在线调试工具版本号管理
const u8 lp_key_sdk_name[16] = "BRxx_SDK";
#if LPCTMU_ANA_CFG_ADAPTIVE
const u8 lp_key_bt_ver[4]    = {0, 0, 2, 0};
#else
const u8 lp_key_bt_ver[4]    = {0, 0, 1, 0};
#endif
struct lp_key_ver_info {
    char sdkname[16];
    u8 lp_key_ver[4];
};

//   通道号           按键事件个数
const char ch_content[] = {
    0x01,                                       //版本号
    'c', 'h', '0', '\0', 6, 0, 1, 2, 3, 4, 5,
    'c', 'h', '1', '\0', 6, 0, 1, 2, 3, 4, 5,
    'c', 'h', '2', '\0', 6, 0, 1, 2, 3, 4, 5,
    'c', 'h', '3', '\0', 6, 0, 1, 2, 3, 4, 5,
    'c', 'h', '4', '\0', 6, 0, 1, 2, 3, 4, 5,
    'c', 'h', '5', '\0', 6, 0, 1, 2, 3, 4, 5,
};
static u32 ch_content_size = 0;
static u32 ch_content_offset = 0;

enum {
    TOUCH_RECORD_GET_VERSION                = 0x05,
    TOUCH_RECORD_GET_CH_SIZE                = 0x0B,
    TOUCH_RECORD_GET_CH_CONTENT             = 0x0C,
    TOUCH_RECORD_CHANGE_MODE                = 0x0E,
    TOUCH_RECORD_COUNT                      = 0x200,
    TOUCH_RECORD_START,
    TOUCH_RECORD_STOP,
    ONLINE_OP_QUERY_RECORD_PACKAGE_LENGTH,
    TOUCH_CH_CFG_UPDATE                     = 0x3000,
    TOUCH_CH_VOL_CFG_UPDATE                 = 0x3001,
    TOUCH_CH_CFG_CONFIRM                    = 0x3100,
    TOUCH_CH_GET_VOL_CFG                    = 0x3101,

    TOUCH_CH_TRIM_OUT_START                 = 0x3102,
    TOUCH_CH_TRIM_IN_START                  = 0x3103,
    TOUCH_CH_TRIM_CLEAR                     = 0x3104,
    TOUCH_CH_TRIM_ENABLE                    = 0x3105,
    TOUCH_CH_TRIM_DISABLE                   = 0x3106,
};

enum {
    LP_KEY_ONLINE_ST_INIT = 0,
    LP_KEY_ONLINE_ST_READY,
    LP_KEY_ONLINE_ST_CH_RES_DEBUG_START,
    LP_KEY_ONLINE_ST_CH_RES_DEBUG_STOP,
    LP_KEY_ONLINE_ST_CH_KEY_DEBUG_START,
    LP_KEY_ONLINE_ST_CH_KEY_DEBUG_CONFIRM,
};

//小机接收命令包格式, 使用DB_PKT_TYPE_TOUCH通道接收
typedef struct {
    int cmd_id;
    int mode;
    int data[];
} touch_receive_cmd_t;

//小机发送按键消息格式, 使用DB_PKT_TYPE_TOUCH通道发送
typedef struct {
    u32 cmd_id;
    u32 mode;
    u32 key_event;
} lp_touch_online_key_event_t;


struct ch_ana_cfg {
    u8 isel;
    u8 vhsel;
    u8 vlsel;
};

struct ch_algo_cfg {
    u16 cfg0;
    u16 cfg1;
    u16 cfg2;
};

struct lp_touch_key_online {
    u8 state;
    u8 current_record_ch;
    u16 res_packet;
    lp_touch_online_key_event_t online_key_event;
};

static struct lp_touch_key_online lp_key_online;

int lp_touch_key_online_debug_key_event_handle(u32 ch, u32 event)
{
    if ((lp_key_online.state == LP_KEY_ONLINE_ST_CH_KEY_DEBUG_START) && (lp_key_online.current_record_ch == ch)) {
        lp_key_online.online_key_event.cmd_id = 0x3100;
        lp_key_online.online_key_event.mode = 0;
        lp_key_online.online_key_event.key_event = event;
        log_debug("send %d event to PC", lp_key_online.online_key_event.key_event);
        app_online_db_send(DB_PKT_TYPE_TOUCH, (u8 *)(&(lp_key_online.online_key_event)), sizeof(lp_touch_online_key_event_t));
    }

    if ((lp_key_online.state == LP_KEY_ONLINE_ST_CH_KEY_DEBUG_CONFIRM) ||
        (lp_key_online.state <= LP_KEY_ONLINE_ST_READY) ||
        (lp_key_online.current_record_ch == LPCTMU_CHANNEL_SIZE)
       ) {
        return 0;
    }

    return 1;
}

enum {
    TLV_TYPE_EVENT_MASK   = 0x40,
    TLV_TYPE_EVENT_TOUCH  = TLV_TYPE_EVENT_MASK + 0,
    TLV_TYPE_EVENT_EAR    = TLV_TYPE_EVENT_MASK + 1,
    TLV_TYPE_EVENT_IR     = TLV_TYPE_EVENT_MASK + 2,
    TLV_TYPE_EVENT_STATUS = TLV_TYPE_EVENT_MASK + 3,
    TLV_TYPE_DATA_MASK   = 0x80,
    TLV_TYPE_DATA_RES    = TLV_TYPE_DATA_MASK + 0,
    TLV_TYPE_DATA_DC     = TLV_TYPE_DATA_MASK + 1,
    TLV_TYPE_DATA_DIFF   = TLV_TYPE_DATA_MASK + 2,
    TLV_TYPE_DATA_NTC    = TLV_TYPE_DATA_MASK + 3,
};

enum {
    TLV_DATA_TYPE_S8_LEFT       = 0x00,
    TLV_DATA_TYPE_S8_RIGHT      = 0x80,
    TLV_DATA_TYPE_U8_LEFT       = 0x01,
    TLV_DATA_TYPE_U8_RIGHT      = 0x81,
    TLV_DATA_TYPE_S16_LEFT      = 0x02,
    TLV_DATA_TYPE_S16_RIGHT     = 0x82,
    TLV_DATA_TYPE_U16_LEFT      = 0x03,
    TLV_DATA_TYPE_U16_RIGHT     = 0x83,
    TLV_DATA_TYPE_S32_LEFT      = 0x04,
    TLV_DATA_TYPE_S32_RIGHT     = 0x84,
    TLV_DATA_TYPE_U32_LEFT      = 0x05,
    TLV_DATA_TYPE_U32_RIGHT     = 0x85,
    TLV_DATA_TYPE_FLOAT_LEFT    = 0x06,
    TLV_DATA_TYPE_FLOAT_RIGHT   = 0x86
};

struct tlv_data_item {
    void *data;
    u8 len;
};

typedef struct {
    u8 type;
    u8 data_type;
    struct tlv_data_item *(*get_data)(u8 tlv_type, u16 *res_buf);
} lp_touch_key_app_debug_packet_t;


static struct tlv_data_item *online_db_get_ch_res(u8 tlv_type, u16 *res_buf)
{
    static struct tlv_data_item item = {0};
    item.data = res_buf;
    item.len =  LPCTMU_CHANNEL_SIZE * sizeof(u16);
    return &item;
}

#if TCFG_LP_EARTCH_KEY_ENABLE
static struct tlv_data_item *online_db_get_ch_dc(u8 tlv_type, u16 *res_buf)
{
    static u16 trim_dc[LPCTMU_CHANNEL_SIZE] = {0};
    static struct tlv_data_item item = {0};
    if (lp_touch_key_eartch_trim_get_all_chs_dc(trim_dc)) {
        memset(trim_dc, 0, sizeof(trim_dc));
    }
    item.data = trim_dc;
    item.len  = LPCTMU_CHANNEL_SIZE * sizeof(u16);
    return &item;
}

static struct tlv_data_item *online_db_get_diff(u8 tlv_type, u16 *res_buf)
{
    static struct tlv_data_item item = {0};
    static s16 diff[2] = {0};
    u16 trim_dc[LPCTMU_CHANNEL_SIZE] = {0};
    lp_touch_key_eartch_trim_get_all_chs_dc(trim_dc);
    u8 wear0_ch = lp_touch_key_get_wear_ch(0);
    u8 wear0_ref_ch = lp_touch_key_get_wear_ref_ch(0);
    u8 wear1_ch = lp_touch_key_get_wear_ch(1);
    u8 wear1_ref_ch = lp_touch_key_get_wear_ref_ch(1);
    diff[0] = (res_buf[wear0_ch] - trim_dc[wear0_ch]) - (res_buf[wear0_ref_ch] - trim_dc[wear0_ref_ch]);
    diff[1] = (res_buf[wear1_ch] - trim_dc[wear1_ch]) - (res_buf[wear1_ref_ch] - trim_dc[wear1_ref_ch]);
    item.data = diff;
    item.len = sizeof(diff);
    return &item;
}

static struct tlv_data_item *online_db_get_ear_status(u8 tlv_type, u16 *res_buf)
{
    static u8 ear_event = 0x00;
    static struct tlv_data_item item = {0};
    ear_event = lp_touch_key_eartch_state_is_out_ear() ? 0 : 1;
    item.data = &ear_event;
    item.len = sizeof(ear_event);
    return &item;
}
#endif

static struct tlv_data_item *online_db_get_touch_status(u8 tlv_type, u16 *res_buf)
{
    static u8 touch_status = 0;
    static struct tlv_data_item item = {0};
    u32 key_event = lp_touch_key_get_last_key_action();
    switch (key_event) {
    case KEY_ACTION_CLICK:
        touch_status = 0x01;
        break;
    case KEY_ACTION_DOUBLE_CLICK:
        touch_status = 0x02;
        break;
    case KEY_ACTION_TRIPLE_CLICK:
        touch_status = 0x03;
        break;
    case KEY_ACTION_FOURTH_CLICK:
        touch_status = 0x04;
        break;
    case KEY_ACTION_FIRTH_CLICK:
        touch_status = 0x05;
        break;
    case KEY_ACTION_LONG:
        touch_status = 0x10;
        break;
    default:
        touch_status = 0;
    }
    item.data = &touch_status;
    item.len  = sizeof(touch_status);
    return &item;
}

#if TCFG_LP_EARTCH_KEY_ENABLE
static struct tlv_data_item *online_db_get_algo_status(u8 tlv_type, u16 *res_buf)
{
    static u8 algo_status = 0;
    static struct tlv_data_item item = {0};
    u32 trim_valid = 0;
    trim_valid = lp_touch_key_eartch_trim_get_valid();
    if (trim_valid) {
        algo_status |= BIT(0);
    } else {
        algo_status &= ~BIT(0);
    }
    if (lp_touch_key_eartch_get_trim_dc_flag()) {
        algo_status |= BIT(1);
    } else {
        algo_status	&= ~BIT(1);
    }
    item.data = &algo_status;
    item.len  = sizeof(algo_status);
    return &item;
}
#endif

lp_touch_key_app_debug_packet_t app_debug_packet_tabel[] = {
    {TLV_TYPE_DATA_RES, TLV_DATA_TYPE_U16_LEFT,  online_db_get_ch_res},
#if TCFG_LP_EARTCH_KEY_ENABLE
    {TLV_TYPE_DATA_DC,     TLV_DATA_TYPE_U16_LEFT,  online_db_get_ch_dc},
    {TLV_TYPE_DATA_DIFF, TLV_DATA_TYPE_S16_RIGHT, online_db_get_diff},
#endif
    {TLV_TYPE_EVENT_TOUCH, TLV_DATA_TYPE_U8_LEFT,  online_db_get_touch_status},
#if TCFG_LP_EARTCH_KEY_ENABLE
    {TLV_TYPE_EVENT_EAR, TLV_DATA_TYPE_U8_LEFT,  online_db_get_ear_status},
    {TLV_TYPE_EVENT_STATUS, TLV_DATA_TYPE_U8_LEFT,  online_db_get_algo_status},
#endif
};


#define DATA_HEAD_SIZE 3
#define PACKET_HEAD_TAIL_SIZE 4
static u16 single_item_size = 0; // lazy static
static void calculate_single_item_size(void)
{
    u16 tmp[LPCTMU_CHANNEL_SIZE] = {0};
    struct tlv_data_item *item = NULL;
    single_item_size = 0;
    u32 packet_num = sizeof(app_debug_packet_tabel) / sizeof(lp_touch_key_app_debug_packet_t);
    for (int i = 0; i < packet_num; i++) {
        single_item_size += DATA_HEAD_SIZE;
        item = app_debug_packet_tabel[i].get_data(app_debug_packet_tabel[i].type, tmp);
        if (item) {
            single_item_size += item->len;
        }
    }
    single_item_size += PACKET_HEAD_TAIL_SIZE;
}


int append_tlv_data_packet(u8 *data_buf, u32 tlv_type, u32 tlv_data_type, struct tlv_data_item *value)
{
    int ret = 0;
    if (data_buf == NULL) {
        return 0;
    }
    if (value == NULL) {
        return 0;
    }

    data_buf[ret++] = tlv_type;
    data_buf[ret++] = tlv_data_type;
    data_buf[ret++] = value->len;

    memcpy(data_buf + ret, value->data, value->len);
    ret += value->len;
    return ret;
}

u32 lp_touch_key_debug_send_table(u8 *send_buf, u16 *res_buf, lp_touch_key_app_debug_packet_t *packet_table, int table_item_num)
{
    if (res_buf == NULL) {
        return 0;
    }
    if (packet_table == NULL) {
        return 0;
    }
    int offset = 2;
    send_buf[0] = 0x5A;
    send_buf[1] = 0xA5;
    for (int i = 0; i < table_item_num; i++) {
        offset += append_tlv_data_packet(send_buf + offset,
                                         packet_table[i].type,
                                         packet_table[i].data_type,
                                         packet_table[i].get_data(packet_table[i].type, res_buf));
    }
    send_buf[offset] = 0xA5;
    send_buf[offset + 1] = 0x5A;
    offset += 2;
    return offset;
}



static u8 *online_db_buf = NULL;
#define TLV_DATA_ITEM_NUM_PER_PACKET  5
static void lp_touch_key_online_debug_send_qcallback(u32 ch, u16 *res_buf)
{
    static int offset = 0;
    static int packet_num_stored = 0;

    if (online_db_buf == NULL) {
        online_db_buf = malloc(single_item_size * TLV_DATA_ITEM_NUM_PER_PACKET);
        offset = 0;
        packet_num_stored = 0;
        // log_debug("alloc online_db_buf :%08X\n", (u32)online_db_buf );
        if (online_db_buf == NULL) {
            return ;
        }
    }
    if (res_buf == NULL) {
        free(online_db_buf);
        online_db_buf = NULL;
        return;
    }

    u32 packet_num = sizeof(app_debug_packet_tabel) / sizeof(lp_touch_key_app_debug_packet_t);
    offset += lp_touch_key_debug_send_table(online_db_buf + offset, res_buf, app_debug_packet_tabel, packet_num);
    // log_debug("offset:%d\n", offset);
    packet_num_stored += 1;
    if (packet_num_stored >= TLV_DATA_ITEM_NUM_PER_PACKET) {
        app_online_db_send(DB_PKT_TYPE_DAT_CH0, (u8 *)online_db_buf, (offset + 1) & 0xfffffffe);
        free(online_db_buf);
        online_db_buf = NULL;
        // log_debug("free online_db_buf \n");
    }
    free(res_buf);
    // log_debug("free res_buf clone\n");
}

int lp_touch_key_online_debug_send(u32 ch, u16 *res_buf)
{
    int err = 0;
    putchar('s');
    if ((lp_key_online.state == LP_KEY_ONLINE_ST_CH_RES_DEBUG_START) && ((lp_key_online.current_record_ch == ch) || (lp_key_online.current_record_ch == LPCTMU_CHANNEL_SIZE))) {
        if (lp_key_online.current_record_ch == LPCTMU_CHANNEL_SIZE) {
            int msg[4] ;
            u16 *res_buf_clone = malloc(sizeof(u16) * LPCTMU_CHANNEL_SIZE);
            memcpy(res_buf_clone, res_buf, sizeof(u16) * LPCTMU_CHANNEL_SIZE);
            msg[0] = (int)lp_touch_key_online_debug_send_qcallback;
            msg[1] = 2;
            msg[2] = ch;
            msg[3] = (int)res_buf_clone;
            os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
        } else {
            lp_key_online.res_packet = res_buf[ch];
            err = app_online_db_send(DB_PKT_TYPE_DAT_CH0, (u8 *)(&(lp_key_online.res_packet)), 2);
        }
    }
    return err;
}

static int lp_touch_key_online_debug_parse(u8 *packet, u8 size, u8 *ext_data, u16 ext_size)
{
    touch_receive_cmd_t *touch_cmd;
    u8 parse_seq = ext_data[1];

    struct ch_algo_cfg *algo_cfg;
    struct ch_ana_cfg *ana_cfg;

    struct lp_key_ver_info ver_info = {0};
    //log_debug("Packet:\n");
    //put_buf(packet, size);
    //log_debug("ext_data:\n");
    //put_buf(ext_data, ext_size);

#if TCFG_LP_EARTCH_KEY_ENABLE
    if (lp_touch_key_product_test_get_state()) {
        return 0;
    }
#endif
    log_debug("%s", __func__);
    put_buf(packet, size);
    put_buf(ext_data, ext_size);
    touch_cmd = (touch_receive_cmd_t *)packet;

    switch (touch_cmd->cmd_id) {

    case TOUCH_RECORD_GET_VERSION://第一步
        log_debug("TOUCH_RECORD_GET_VERSION");
        memcpy(ver_info.sdkname, lp_key_sdk_name, sizeof(lp_key_sdk_name));
        memcpy(ver_info.lp_key_ver, lp_key_bt_ver, sizeof(lp_key_bt_ver));
        app_online_db_ack(parse_seq, (u8 *)(&ver_info), sizeof(ver_info)); //回复版本号数据结构
        break;

    case TOUCH_RECORD_GET_CH_SIZE:
        log_debug("TOUCH_RECORD_GET_CH_SIZE");//第二步
        ch_content_size = sizeof(ch_content);
        ch_content_offset = 0;
        app_online_db_ack(parse_seq, (u8 *)&ch_content_size, 4); //回复对应的通道数据长度
        break;

    case TOUCH_RECORD_GET_CH_CONTENT:
        log_debug("TOUCH_RECORD_GET_CH_CONTENT");//第三步
        u32 ack_len = ch_content_size - ch_content_offset;
        ack_len = ack_len > 32 ? 32 : ack_len;
        app_online_db_ack(parse_seq, (u8 *)(&ch_content[ch_content_offset]), ack_len);
        ch_content_offset += ack_len;
        break;

    case TOUCH_RECORD_CHANGE_MODE://第四步
        log_debug("TOUCH_RECORD_CHANGE_MODE, cmd_mode = %d", touch_cmd->mode);
        lp_key_online.current_record_ch = touch_cmd->mode;
        app_online_db_ack(parse_seq, (u8 *)"OK", 2); //回"OK"字符串
        break;

    case TOUCH_CH_GET_VOL_CFG:
        log_debug("TOUCH_CH_GET_VOL_CFG");//界面按钮触发，在第4步之后
        u8 tmp_ana_cfg[6];
#if LPCTMU_ANA_CFG_ADAPTIVE
        tmp_ana_cfg[0] = 7;//最大值
        tmp_ana_cfg[1] = lpctmu_get_ana_hv_level();
        tmp_ana_cfg[2] = 0;//最大值
        tmp_ana_cfg[3] = 0;
#else
        tmp_ana_cfg[0] = 3;//最大值
        tmp_ana_cfg[1] = lpctmu_get_ana_hv_level();
        tmp_ana_cfg[2] = 3;//最大值
        tmp_ana_cfg[3] = lpctmu_get_ana_lv_level();
#endif
        tmp_ana_cfg[4] = 7;//最大值
        tmp_ana_cfg[5] = lpctmu_get_ana_cur_level(lp_key_online.current_record_ch);
        log_debug("hv_max:%d hv:%d lv_max:%d lv:%d ic_max:%d ic:%d", tmp_ana_cfg[0],
                  tmp_ana_cfg[1],
                  tmp_ana_cfg[2],
                  tmp_ana_cfg[3],
                  tmp_ana_cfg[4],
                  tmp_ana_cfg[5]);
        app_online_db_ack(parse_seq, tmp_ana_cfg, 6); //回数据
        break;

    case TOUCH_CH_VOL_CFG_UPDATE://第五步
        log_debug("TOUCH_CH_VOL_CFG_UPDATE");
        app_online_db_ack(parse_seq, (u8 *)"OK", 2); //回"OK"字符串
        ana_cfg = (struct ch_ana_cfg *)(touch_cmd->data);
        log_debug("update, isel = %d, vhsel = %d, vlsel = %d", ana_cfg->isel, ana_cfg->vhsel, ana_cfg->vlsel);
        if (lp_key_online.current_record_ch < LPCTMU_CHANNEL_SIZE) {
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
            u32 ch_idx = lp_touch_key_get_idx_by_cur_ch(lp_key_online.current_record_ch);
            lp_touch_key_identify_algo_reset(ch_idx);
#else
            lpctmu_send_m2p_cmd(RESET_IDENTIFY_ALGO);
#endif
            lpctmu_set_ana_cur_level(lp_key_online.current_record_ch, ana_cfg->isel);
            lpctmu_set_ana_hv_level(ana_cfg->vhsel);
        }
        break;

    case TOUCH_RECORD_START://第六步
        log_debug("TOUCH_RECORD_START");
        app_online_db_ack(parse_seq, (u8 *)"OK", 1); //该命令随便ack一个byte即可
        lp_key_online.state = LP_KEY_ONLINE_ST_CH_RES_DEBUG_START;
        break;

    case TOUCH_CH_CFG_UPDATE://第七步
        log_debug("TOUCH_CH_CFG_UPDATE");
        app_online_db_ack(parse_seq, (u8 *)"OK", 2); //回"OK"字符串
        lp_key_online.state = LP_KEY_ONLINE_ST_CH_KEY_DEBUG_START;
        algo_cfg = (struct ch_algo_cfg *)(touch_cmd->data);
        log_debug("update, cfg0 = %d, cfg1 = %d, cfg2 = %d", algo_cfg->cfg0, algo_cfg->cfg1, algo_cfg->cfg2);
        if (lp_key_online.current_record_ch < LPCTMU_CHANNEL_SIZE) {
#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
            u32 ch_idx = lp_touch_key_get_idx_by_cur_ch(lp_key_online.current_record_ch);
            lp_touch_key_identify_algorithm_init(ch_idx, algo_cfg->cfg0, algo_cfg->cfg2);
#else
            u32 ch = lp_key_online.current_record_ch;
            M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG0L + ch * 8) = (algo_cfg->cfg0 >> 0) & 0xff;
            M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG0H + ch * 8) = (algo_cfg->cfg0 >> 8) & 0xff;
            M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG1L + ch * 8) = ((algo_cfg->cfg0 + 5) >> 0) & 0xff;
            M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG1H + ch * 8) = ((algo_cfg->cfg0 + 5) >> 8) & 0xff;
            M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG2L + ch * 8) = (algo_cfg->cfg2 >> 0) & 0xff;
            M2P_MESSAGE_ACCESS(M2P_MASSAGE_CTMU_CH0_CFG2H + ch * 8) = (algo_cfg->cfg2 >> 8) & 0xff;
            lpctmu_send_m2p_cmd(RESET_IDENTIFY_ALGO);
#endif
        }
        break;

    case TOUCH_CH_CFG_CONFIRM://第八步
        log_debug("TOUCH_CH_CFG_CONFIRM");
        app_online_db_ack(parse_seq, (u8 *)"OK", 2); //回"OK"字符串
        lp_key_online.state = LP_KEY_ONLINE_ST_CH_KEY_DEBUG_CONFIRM;
        break;

    case TOUCH_RECORD_STOP://第九步
        log_debug("TOUCH_RECORD_STOP");
        app_online_db_ack(parse_seq, (u8 *)"OK", 1); //该命令随便ack一个byte即可
        lp_key_online.state = LP_KEY_ONLINE_ST_CH_RES_DEBUG_STOP;
        int msg[4] ;
        msg[0] = (int)lp_touch_key_online_debug_send_qcallback;
        msg[1] = 2;
        msg[2] = 0;
        msg[3] = (int)NULL;
        os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
        break;


#if TCFG_LP_EARTCH_KEY_ENABLE
    /**
      Trim完,enable disable, clear都需要写VM，读VM，更新cfg1，复位算法
      */
    case TOUCH_CH_TRIM_OUT_START:
        log_debug("TOUCH_CH_TRIM_OUT_START");
        app_online_db_ack(parse_seq, (u8 *)"OK", 1); //该命令随便ack一个byte即可
        lp_touch_eartch_trim_dc_start(1);
        break;
    case TOUCH_CH_TRIM_IN_START:
        log_debug("TOUCH_CH_TRIM_IN_START");
        app_online_db_ack(parse_seq, (u8 *)"OK", 1); //该命令随便ack一个byte即可
        lp_touch_eartch_trim_dc_start(0);
        break;
    case TOUCH_CH_TRIM_ENABLE:
        app_online_db_ack(parse_seq, (u8 *)"OK", 1); //该命令随便ack一个byte即可
        lp_touch_eartch_set_trim_valid(1);
        break;
    case TOUCH_CH_TRIM_DISABLE:
        app_online_db_ack(parse_seq, (u8 *)"OK", 1); //该命令随便ack一个byte即可
        lp_touch_eartch_set_trim_valid(0);
        break;
    case TOUCH_CH_TRIM_CLEAR:
        app_online_db_ack(parse_seq, (u8 *)"OK", 1); //该命令随便ack一个byte即可
        lp_touch_eartch_trim_dc_clear();
        break;
#endif

    default:
        break;
    }

#ifdef TOUCH_KEY_IDENTIFY_ALGO_IN_MSYS
#else
    log_debug("cmd_id:%04X", touch_cmd->cmd_id);
    if (touch_cmd->cmd_id == TOUCH_RECORD_START || ((touch_cmd->cmd_id >= TOUCH_CH_TRIM_OUT_START) && (touch_cmd->cmd_id <= TOUCH_CH_TRIM_DISABLE))) {
        log_debug("active debug");
        if (lp_key_online.current_record_ch < LPCTMU_CHANNEL_SIZE) {
            M2P_CTMU_CH_DEBUG |= BIT(lp_key_online.current_record_ch);
        } else {
            M2P_CTMU_CH_DEBUG = 0xff;
        }
    } else {
        log_debug("close debug");
        M2P_CTMU_CH_DEBUG = 0;
    }
#endif

    return 0;
}

int lp_touch_key_online_debug_init(void)
{
    log_debug("%s", __func__);

    calculate_single_item_size();

    memset((u8 *)&lp_key_online, 0, sizeof(struct lp_touch_key_online));

    app_online_db_register_handle(DB_PKT_TYPE_TOUCH, lp_touch_key_online_debug_parse);
    lp_key_online.state = LP_KEY_ONLINE_ST_READY;

    return 0;
}

int lp_touch_key_online_debug_exit(void)
{
    return 0;
}

u8 lp_touch_key_online_get_state()
{
    return lp_key_online.state == LP_KEY_ONLINE_ST_CH_RES_DEBUG_START ? 1 : 0;
}

#endif
