#include <string.h>
#include "os/os_api.h"
#include "lp_touch_key_common.h"
#include "sdk_config.h"
#include "asm/lp_touch_key_api.h"
#include "asm/lp_touch_key_tool.h"
#include "asm/power_interface.h"
#include "debug.h"
#include "system/timer.h"

#define MAGIC1         "EPH"
#define MAGIC2         "EPE"
#define MAGIC3         "DT"
#define CMD_ENTER_PT   0xB0
#define CMD_START_TRIM 0xB1
#define CMD_GET_RES    0xB2
#define CMD_EXIT_PT    0xB3
#define CMD_GET_EAR_ST 0xB4
#define CMD_RES_EOF    0xBE
#define CMD_UNKNOWN    0xFF
#define RESP_OK        0xD0
#define RESP_TRIM_OK   0xD1
#define RESP_EOF       0xD2
#define RESP_BUSY      0xD3 // 进产测时在线调试正在运行或退产测时校准未结束
#define RESP_ERR       0xD4 // 错误的指令

#define SIZEOF_CONST_STR(str) (sizeof(str) - 1)
#define U16_TO_U8L(x) ((u8)(x & 0xff))
#define U16_TO_U8H(x) ((u8)((x >> 8) & 0xff))

enum {
    PT_STATE_EXIT = 0,
    PT_STATE_FREE,
    PT_STATE_GETTING_RES,
    PT_STATE_OUT_TRIMMING,
    PT_STATE_IN_TRIMMING
};

#if TCFG_LP_EARTCH_KEY_ENABLE && TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE

static u8 lp_touch_key_product_testing_state = PT_STATE_EXIT;
static u8 lp_touch_key_product_test_trim_timer = 0;

u32 lp_touch_key_product_test_get_state(void)
{
    return lp_touch_key_product_testing_state;
}
static u32 lp_touch_key_product_test_set_state(u32 flag)
{
    log_debug("set pt state:0x%02X\n", flag);

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE
    if (!lp_touch_key_online_get_state()) {
#endif
        lp_touch_key_product_testing_state = flag;

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE
    } else {
        lp_touch_key_product_testing_state = PT_STATE_EXIT;
    }
#endif
    return lp_touch_key_product_testing_state;
}

static void lp_touch_key_enable_debug(u32 enable)
{
#ifdef CONFIG_CPU_BR56
#else
    if (enable) {
        M2P_CTMU_CH_DEBUG |= 0xff;
    } else {
        M2P_CTMU_CH_DEBUG = 0;
    }
#endif
}

int online_spp_send_data(u8 *data, u16 len);
static void lp_touch_key_eartch_product_test_send_handler(u8 *dat, u16 size)
{
    online_spp_send_data(dat, size);
}

static void lp_touch_key_eartch_product_test_response(u32 resp, u32 cmd)
{
    u8 resp_buf[24] = {0};
    u32 offset = SIZEOF_CONST_STR(MAGIC1);
    u16 wear_dc[LPCTMU_CHANNEL_SIZE] = {0};
    u32 eartch_ch_num = lp_touch_key_eartch_get_ch_num();
    lp_touch_key_eartch_trim_get_all_chs_dc(wear_dc);

    memcpy(resp_buf, MAGIC1, SIZEOF_CONST_STR(MAGIC1));
    switch (resp) {
    case RESP_OK:
    case RESP_TRIM_OK: {
        resp_buf[offset++] = resp;
        if (cmd == CMD_START_TRIM || cmd == CMD_GET_RES || cmd == CMD_ENTER_PT) {
            resp_buf[offset++] = (u8)(eartch_ch_num * 2);
            for (int i = 0; i < eartch_ch_num; i++) {
                u16 wear_ch     = lp_touch_key_get_wear_ch(i);
                u16 wear_ref_ch = lp_touch_key_get_wear_ref_ch(i);
                resp_buf[offset++] = U16_TO_U8L(wear_dc[wear_ch]);
                resp_buf[offset++] = U16_TO_U8H(wear_dc[wear_ch]);
                resp_buf[offset++] = U16_TO_U8L(wear_dc[wear_ref_ch]);
                resp_buf[offset++] = U16_TO_U8H(wear_dc[wear_ref_ch]);
            }
        }
        if (cmd == CMD_GET_EAR_ST) {
            resp_buf[offset++] = (u8)(eartch_ch_num * 2);
            u16 res[LPCTMU_CHANNEL_SIZE] = {0};
            u32 in_ear_state = lp_touch_key_eartch_get_each_ch_state();
            lp_touch_key_get_res(res);
            for (int i = 0; i < eartch_ch_num; i++) {
                u16 wear_ch     = lp_touch_key_get_wear_ch(i);
                u16 wear_ref_ch = lp_touch_key_get_wear_ref_ch(i);
                u8 ear_mask = (in_ear_state & BIT(wear_ch)) ? 0x80 : 0x00;
                resp_buf[offset++] = U16_TO_U8L(res[wear_ch]);
                resp_buf[offset++] = U16_TO_U8H(res[wear_ch]) | ear_mask;
                resp_buf[offset++] = U16_TO_U8L(res[wear_ref_ch]);
                resp_buf[offset++] = U16_TO_U8H(res[wear_ref_ch]);
            }
        }
        break;
    }
    case RESP_EOF:
    case RESP_BUSY: {
        resp_buf[offset++] = resp;
        break;
    }
    default: {
        resp_buf[offset++] = RESP_ERR;
        break;
    }

    }
    memcpy(resp_buf + offset, MAGIC2, SIZEOF_CONST_STR(MAGIC2));
    offset += SIZEOF_CONST_STR(MAGIC2);
    lp_touch_key_eartch_product_test_send_handler(resp_buf, offset);
}

static void lp_touch_key_eartch_product_test_trim_handler(void *arg)
{
    if (!lp_touch_key_eartch_get_trim_dc_flag()) {
        switch (lp_touch_key_product_test_get_state()) {
        case PT_STATE_FREE:
            lp_touch_key_product_test_set_state(PT_STATE_OUT_TRIMMING);
            lp_touch_eartch_trim_dc_start(1);
            goto __continue;
        case PT_STATE_OUT_TRIMMING:
            lp_touch_key_product_test_set_state(PT_STATE_IN_TRIMMING);
            lp_touch_eartch_trim_dc_start(0);
            goto __continue;
        case PT_STATE_IN_TRIMMING:
            lp_touch_key_enable_debug(false);
            lp_touch_key_eartch_product_test_response(RESP_EOF,     CMD_START_TRIM);
            lp_touch_key_eartch_product_test_response(RESP_TRIM_OK, CMD_START_TRIM);
            lp_touch_eartch_set_trim_valid(1);
        /* fall-through */
        default:
            lp_touch_key_product_test_set_state(PT_STATE_FREE);
            goto __break;
        }
    } else {
        goto __continue;
    }
__continue:
    sys_timeout_add(NULL, lp_touch_key_eartch_product_test_trim_handler, 200);
__break:
    return;
}

static void lp_touch_key_eartch_product_test_cmd_handler(u32 CMD)
{
    switch (CMD) {
    case CMD_ENTER_PT: {
        u32 RESP = lp_touch_key_product_test_set_state(PT_STATE_FREE) == PT_STATE_FREE ? RESP_OK : RESP_BUSY;
        lp_touch_key_eartch_product_test_response(RESP, CMD);
        break;
    }
    case CMD_EXIT_PT: {
        u32 RESP = lp_touch_key_product_test_set_state(PT_STATE_EXIT) == PT_STATE_EXIT ? RESP_OK : RESP_BUSY;
        lp_touch_key_eartch_product_test_response(RESP, CMD);
        break;
    }
    case CMD_START_TRIM:
        if (lp_touch_key_product_test_get_state() != PT_STATE_FREE) {
            break;
        }
        lp_touch_key_enable_debug(true);
        lp_touch_key_eartch_product_test_response(RESP_OK, CMD);
        lp_touch_key_eartch_product_test_trim_handler(NULL);
        break;
    case CMD_GET_RES:
        if (lp_touch_key_product_test_get_state() != PT_STATE_FREE) {
            break;
        }
        lp_touch_key_product_test_set_state(PT_STATE_GETTING_RES);
        lp_touch_key_enable_debug(true);
        lp_touch_key_eartch_product_test_response(RESP_OK, CMD);
        break;
    case CMD_RES_EOF:
        lp_touch_key_enable_debug(false);
        lp_touch_key_eartch_product_test_response(RESP_EOF, CMD);
        break;
    case CMD_GET_EAR_ST:
        lp_touch_key_eartch_product_test_response(RESP_OK, CMD);
        break;
    default:
        lp_touch_key_eartch_product_test_response(RESP_ERR, CMD_UNKNOWN);
        break;
    }
}

#define RES_COUNT_REQUEST 50
static void lp_touch_key_eartch_product_test_report_res(u16 *res_buf)
{
    static u8 update_counter = 0;
    static u8 reported_count = 0;
    static u8 last_state = PT_STATE_EXIT;
    u8 report_buf[16] = {0};
    u32 offset = SIZEOF_CONST_STR(MAGIC3);
    u32 eartch_ch_pair_num = lp_touch_key_eartch_get_ch_num();
    memcpy(report_buf, MAGIC3, SIZEOF_CONST_STR(MAGIC3));
    update_counter = (update_counter + 1) % eartch_ch_pair_num;
    if (lp_touch_key_product_test_get_state() != last_state) {
        last_state = lp_touch_key_product_test_get_state();
        reported_count = 0;
    }
    if (!update_counter) {
        return;
    }

    if (lp_touch_key_product_test_get_state() == PT_STATE_GETTING_RES) {
        reported_count ++;
    }
    if (reported_count > RES_COUNT_REQUEST) {
        reported_count = 0;
        if (lp_touch_key_product_test_get_state() == PT_STATE_GETTING_RES) {
            lp_touch_key_product_test_set_state(PT_STATE_FREE);
            lp_touch_key_eartch_product_test_cmd_handler(CMD_RES_EOF);
        }
        free(res_buf);
        return;
    }
    report_buf[offset++] = eartch_ch_pair_num * 2;

    for (int i = 0; i < eartch_ch_pair_num; i++) {
        u16 wear_ch = lp_touch_key_get_wear_ch(i);
        u16 wear_ref_ch = lp_touch_key_get_wear_ref_ch(i);
        report_buf[offset++] = U16_TO_U8L(res_buf[wear_ch]);
        report_buf[offset++] = U16_TO_U8H(res_buf[wear_ch]);
        report_buf[offset++] = U16_TO_U8L(res_buf[wear_ref_ch]);
        report_buf[offset++] = U16_TO_U8H(res_buf[wear_ref_ch]);
    }
    lp_touch_key_eartch_product_test_send_handler(report_buf, offset);
    free(res_buf);
}



static void lp_touch_key_eartch_product_test_recieve_callback(u8 *dat, u16 size)
{
    lp_touch_key_eartch_product_test_cmd_handler(*dat);
    free(dat);
}

void lp_touch_key_eartch_product_test_debug_handler(u16 *res_buf)
{
    if ((lp_touch_key_product_test_get_state() == PT_STATE_EXIT) || (lp_touch_key_product_test_get_state() == PT_STATE_FREE)) {
        return;
    }
    u8 *res_buf_clone = malloc(sizeof(u16) * LPCTMU_CHANNEL_SIZE);
    memcpy(res_buf_clone, res_buf, LPCTMU_CHANNEL_SIZE * sizeof(u16));
    int msg[4];
    msg[0] = (int) lp_touch_key_eartch_product_test_report_res;
    msg[1] = (int) 1;
    msg[2] = (int) res_buf_clone;
    msg[3] = (int) 1;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
}

void lp_touch_key_eartch_product_test_recieve_handler(u8 *dat, u16 size)
{
    if (size < SIZEOF_CONST_STR(MAGIC1) + SIZEOF_CONST_STR(MAGIC2)) {
        return;
    }
    if (memcmp(MAGIC1, dat, SIZEOF_CONST_STR(MAGIC1)) || memcmp(MAGIC2, dat + size - SIZEOF_CONST_STR(MAGIC2), SIZEOF_CONST_STR(MAGIC2))) {
        return;
    }
    size = size - SIZEOF_CONST_STR(MAGIC1) - SIZEOF_CONST_STR(MAGIC2);
    u8 *dat_clone = malloc(size);
    memcpy(dat_clone, dat + SIZEOF_CONST_STR(MAGIC1), size);
    int msg[4] = {
        (int) lp_touch_key_eartch_product_test_recieve_callback,
        (int) 2,
        (int) dat_clone,
        (int) size
    };
    os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
}
#endif
