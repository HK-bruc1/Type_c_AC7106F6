
#include "typedef.h"
#include "syscfg_id.h"
#include "btctrler_task.h"
#include "init.h"

#define LOG_TAG             "[FRE_OFFSET]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

extern void bt_fre_offset_compenstation_api_init(void *func);
extern u16 CRC16(unsigned char *ptr, int len);

#define ABS(a)                      ((a)>0?(a):-(a))

//the new frequency offset value that have a difference of less than 2 are not stored.
#define MIN_DIFF_VAL		2

//static bt_fre_offset_compensation_api_t app_bt_fre_offset_api;
static const u16 fre_offset_store_id_tab[]  = {
    CFG_BT_FRE_OFFSET,
    CFG_BT_FRE_OFFSET1,
    CFG_BT_FRE_OFFSET2,
    CFG_BT_FRE_OFFSET3,
    CFG_BT_FRE_OFFSET4,
};

struct fre_offset_t {
    u16 crc;
    s32 offset;
} _GNU_PACKED_;

struct fre_offset_v2_t {
    u16 crc;
    u8 data[4];
} _GNU_PACKED_;


/* *****************************************************************************/
/**
 * @brief :        bt_fre_offset_trim_flag_get
 *
 * @return :       The flag of frequency offset trim value stored in the BTIF area.
 * 				   Note:This api is compatible with one-time trim procedures.
 */
/* *****************************************************************************/
u8 bt_fre_offset_trim_flag_get(void)
{
    u8 flag = 0;

    struct fre_offset_v2_t fre_offset_item;
    u8 item_len;

    item_len = syscfg_read(CFG_BT_FRE_OFFSET, (u8 *)&fre_offset_item, sizeof(struct fre_offset_v2_t));
    if ((item_len == sizeof(struct fre_offset_v2_t)) && (fre_offset_item.crc == CRC16((u8 *)&fre_offset_item.data[0], item_len - 2))) {
        flag = 1;
    }

    return flag;
}

static u8 bt_fre_offset_ex_init(u8 mode, u8 *offset, u8 len, u8 *once_trim_flag)
{
    u8 tab_num = sizeof(fre_offset_store_id_tab) / sizeof(fre_offset_store_id_tab[0]);
    u8 i;

    struct fre_offset_v2_t fre_offset_item;

    u8 item_len = sizeof(struct fre_offset_v2_t);
    for (i = 0; i < tab_num; i++) {
        if (item_len != syscfg_read(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len)) {
            break;
        }
    }

    if ((0 == i) || (fre_offset_item.crc != CRC16((u8 *)&fre_offset_item.data[0], item_len - 2))) {
        if (offset) {
            memset(offset, 0xff, len);
        }

        *once_trim_flag = 0;
    } else {
        if (offset) {
            memcpy(offset, fre_offset_item.data, len);
        }

        *once_trim_flag = (i == tab_num) ? 1 : 0;
    }

    log_info("FRE OFFSET INIT:%d  %d\n", i,  *once_trim_flag);

    return 0;

}

static u8 bt_fre_offset_ex_write(u8 mode, u8 *offset, u8 len, u8 *once_trim_flag)
{
    u8 ret = 0;

    u8 tab_num = sizeof(fre_offset_store_id_tab) / sizeof(fre_offset_store_id_tab[0]);
    u8 i;

    struct fre_offset_v2_t fre_offset_item;

    u8 item_len = sizeof(struct fre_offset_v2_t);

    for (i = 0; i < tab_num; i++) {
        if (item_len != syscfg_read(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len)) {
            break;
        }
    }

    if (i == tab_num) {
        *once_trim_flag = 1;
        ret = (u8) - 1;
    } else {
#if 0 //MIN_DIFF_VAL
        u16 diff_val = ABS((s16)fre_offset_item.offset - fre_offset);
        if ((0 == i) || (diff_val > MIN_DIFF_VAL)) {
            fre_offset_item.offset = fre_offset;
            fre_offset_item.crc = CRC16((u8 *)&fre_offset_item.offset, item_len - 2);
            int err = syscfg_write(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len);
            log_info("FRE OFFSET WRITE:%d %d %d\n", i, fre_offset, *once_trim_flag);
            if (err != item_len) {
                ret = (u8) - 3;
            }
        } else {
            log_info("DIFF VAL:%d\n", diff_val);
            ret = (u8) - 2;
        }
#else
        if (0 == memcmp((u8 *)&fre_offset_item.data, offset, 4)) {
            log_info("SAME VAL\n");
            /* ret = (u8) - 2; */
        } else {
            memcpy(fre_offset_item.data, offset, item_len - 2);
            fre_offset_item.crc = CRC16((u8 *)&fre_offset_item.data, item_len - 2);
            int err = syscfg_write(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len);
            log_info("FRE OFFSET WRITE:%d %d\n", i,  *once_trim_flag);
            if (err != item_len) {
                ret = (u8) - 3;
            }
        }
#endif
        *once_trim_flag = (i == (tab_num - 1)) ? 1 : 0;
    }

    if (0 != ret) {
        log_error("FRE OFFSET WR ERR:%x\n", ret);
    }

    return ret;

}

static u8 bt_fre_offset_init(s16 *fre_offset, u8 *once_trim_flag)
{
    u8 tab_num = sizeof(fre_offset_store_id_tab) / sizeof(fre_offset_store_id_tab[0]);
    u8 i;

    struct fre_offset_t fre_offset_item;

    u8 item_len = sizeof(struct fre_offset_t);
    for (i = 0; i < tab_num; i++) {
        if (item_len != syscfg_read(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len)) {
            break;
        }
        /* log_info("Freq offset item[%d]: %d\n", i, fre_offset_item.offset); */
    }

    if ((0 == i) || (fre_offset_item.crc != CRC16((u8 *)&fre_offset_item.offset, item_len - 2))) {
        *fre_offset = 0;
        *once_trim_flag = 0;
    } else {
        *fre_offset = fre_offset_item.offset;
        *once_trim_flag = (i == tab_num) ? 1 : 0;
    }

    log_info("FRE OFFSET INIT:%d %d %d\n", i, *fre_offset, *once_trim_flag);

    return 0;
}

static u8 bt_fre_offset_write(s16 fre_offset, u8 *once_trim_flag)
{
    u8 ret = 0;

    u8 tab_num = sizeof(fre_offset_store_id_tab) / sizeof(fre_offset_store_id_tab[0]);
    u8 i;

    struct fre_offset_t fre_offset_item;

    u8 item_len = sizeof(struct fre_offset_t);

    for (i = 0; i < tab_num; i++) {
        if (item_len != syscfg_read(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len)) {
            break;
        }
    }

    if (i == tab_num) {
        *once_trim_flag = 1;
        ret = (u8) - 1;
    } else {
#if MIN_DIFF_VAL
        u16 diff_val = ABS((s16)fre_offset_item.offset - fre_offset);
        if ((0 == i) || (diff_val > MIN_DIFF_VAL)) {
            fre_offset_item.offset = fre_offset;
            fre_offset_item.crc = CRC16((u8 *)&fre_offset_item.offset, item_len - 2);
            int err = syscfg_write(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len);
            log_info("FRE OFFSET WRITE:%d %d %d\n", i, fre_offset, *once_trim_flag);
            if (err != item_len) {
                ret = (u8) - 3;
            }
        } else {
            log_info("DIFF VAL:%d\n", diff_val);
            ret = (u8) - 2;
        }
#else
        fre_offset_item.offset = fre_offset;
        fre_offset_item.crc = CRC16((u8 *)&fre_offset_item.offset, item_len - 2);
        int err = syscfg_write(fre_offset_store_id_tab[i], (u8 *)&fre_offset_item, item_len);
        log_info("FRE OFFSET WRITE:%d %d %d\n", i, fre_offset, *once_trim_flag);
        if (err != item_len) {
            ret = (u8) - 3;
        }
#endif
        *once_trim_flag = (i == (tab_num - 1)) ? 1 : 0;
    }

    if (0 != ret) {
        log_error("FRE OFFSET WR ERR:%x\n", ret);
    }

    return ret;
}

static bt_fre_offset_compensation_api_t app_bt_fre_offset_api = {
#if defined(CONFIG_CPU_BR52)
    .init =	NULL,
    .write = NULL,
    .ex_init = bt_fre_offset_ex_init,
    .ex_write = bt_fre_offset_ex_write,
#else
    .init =	bt_fre_offset_init,
    .write = bt_fre_offset_write,
    .ex_init = NULL,
    .ex_write = NULL,
#endif
};

int bt_fre_offset_storage_init(void)
{
    log_info("\n--func=%s\n", __FUNCTION__);
    bt_fre_offset_compenstation_api_init(&app_bt_fre_offset_api);

    return 0;
}

//late_initcall(bt_fre_offset_storage_init);
#if defined(CONFIG_CPU_BR52) || defined(CONFIG_CPU_BR56) || defined(CONFIG_CPU_BR50)
late_initcall(bt_fre_offset_storage_init);
#endif


