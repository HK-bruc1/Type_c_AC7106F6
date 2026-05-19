#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".multi_protocol_common.data.bss")
#pragma data_seg(".multi_protocol_common.data")
#pragma const_seg(".multi_protocol_common.text.const")
#pragma code_seg(".multi_protocol_common.text")
#endif
#include "system/includes.h"
#include "multi_protocol_main.h"
#include "classic/tws_api.h"
#include "bt_tws.h"
#include "resfile.h"
#include "sdfile.h"

#if ((THIRD_PARTY_PROTOCOLS_SEL & (RCSP_MODE_EN | GFPS_EN | MMA_EN | FMNA_EN |  TUYA_DEMO_EN | REALME_EN | SWIFT_PAIR_EN | DMA_EN | ONLINE_DEBUG_EN | CUSTOM_DEMO_EN | XIMALAYA_EN | JL_SBOX_EN | HID_ISO_EN)) || \
		((TCFG_LE_AUDIO_APP_CONFIG & (LE_AUDIO_UNICAST_SINK_EN | LE_AUDIO_JL_UNICAST_SINK_EN | LE_AUDIO_AURACAST_SINK_EN | LE_AUDIO_JL_AURACAST_SINK_EN | LE_AUDIO_AURACAST_SOURCE_EN | LE_AUDIO_JL_AURACAST_SOURCE_EN)))) && \
		!TCFG_THIRD_PARTY_PROTOCOLS_SIMPLIFIED

#if 0
#define log_info(x, ...)       printf("[MULTI_PROTOCOL]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

#if TCFG_USER_TWS_ENABLE

/*******************************************************
               BLE/SPP 中间层句柄 TWS 同步
*******************************************************/

#define TWS_FUNC_ID_MULTI_PROTOCOL_TWS_SYNC \
	(((u8)('M' + 'U' + 'L' + 'T' + 'I') << (3 * 8)) | \
	 ((u8)('P' + 'R' + 'O' + 'T' + 'O') << (2 * 8)) | \
	 ((u8)('C' + 'O' + 'L') << (1 * 8)) | \
	 ((u8)('T' + 'W' + 'S') << (0 * 8)))

#define MULTI_PROTOCOL_TWS_SYNC_BLE     (0x01)
#define MULTI_PROTOCOL_TWS_SYNC_SPP     (0x02)

extern const bool config_tws_le_role_sw;    //是否支持ble跟随tws主从切换
void multi_protocol_tws_sync_send(void)
{
    u8 *temp_buf = 0;
    int size = 0;

    log_info("tws sync : %d %d\n", get_bt_tws_connect_status(), tws_api_get_role());
    if (!get_bt_tws_connect_status() || !(tws_api_get_role() == TWS_ROLE_MASTER)) {
        return;
    }

    if (config_tws_le_role_sw) {
        size = app_ble_all_sync_data_size();
        if (size) {
            temp_buf = zalloc(size + 1);
            temp_buf[0] = MULTI_PROTOCOL_TWS_SYNC_BLE;
            app_ble_all_sync_data_get(&temp_buf[1]);
            tws_api_send_data_to_sibling(temp_buf, size + 1, TWS_FUNC_ID_MULTI_PROTOCOL_TWS_SYNC);
            free(temp_buf);
        }
    }

    size = app_spp_all_sync_data_size();
    if (size) {
        temp_buf = zalloc(size + 1);
        temp_buf[0] = MULTI_PROTOCOL_TWS_SYNC_SPP;
        app_spp_all_sync_data_get(&temp_buf[1]);
        tws_api_send_data_to_sibling(temp_buf, size + 1, TWS_FUNC_ID_MULTI_PROTOCOL_TWS_SYNC);
        free(temp_buf);
    }
}

static void multi_protocol_tws_sync_in_task(u8 *data, int len)
{
    int i;
    log_info("multi_protocol_tws_sync_in_task %d\n", len);
    log_info_hexdump(data, len);
    switch (data[0]) {
    case MULTI_PROTOCOL_TWS_SYNC_BLE:
        app_ble_all_sync_data_set(&data[1], len - 1);
        break;
    case MULTI_PROTOCOL_TWS_SYNC_SPP:
        app_spp_all_sync_data_set(&data[1], len - 1);
        break;
    }
    free(data);
}

static void multi_protocol_tws_sync_in_irq(void *_data, u16 len, bool rx)
{
    int i;
    int argv[4];
    u8 *rx_data = NULL;
    log_info("multi_protocol_tws_sync_in_irq %d tws_state:%d\n", len, tws_api_get_tws_state());
    log_info_hexdump(_data, len);
    if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
        if (rx && (tws_api_get_role() == TWS_ROLE_SLAVE)) {
            rx_data = malloc(len);
            if (rx_data == NULL) {
                return;
            }
            memcpy(rx_data, _data, len);
            argv[0] = (int)multi_protocol_tws_sync_in_task;
            argv[1] = 2;
            argv[2] = (int)rx_data;
            argv[3] = (int)len;
            int ret = os_taskq_post_type("app_core", Q_CALLBACK, 4, argv);
            if (ret) {
                printf("%s taskq post err \n", __func__);
            }
        }
    }
}

REGISTER_TWS_FUNC_STUB(tws_rcsp_bt_hdl_sync) = {
    .func_id = TWS_FUNC_ID_MULTI_PROTOCOL_TWS_SYNC,
    .func = multi_protocol_tws_sync_in_irq,
};

/*******************************************************
               BLE/SPP 中间层句柄 TWS 同步 end
*******************************************************/

#endif


/*******************************************************
               三元组记录相关接口
*******************************************************/

u32 multi_protocol_read_cfg_file(void *buf, u16 len, char *path)
{
    RESFILE *fp = resfile_open(path);
    int rlen;

    if (!fp) {
        printf("read_cfg_file:fopen err!\n");
        return FALSE;
    }

    rlen = resfile_read(fp, buf, len);
    if (rlen <= 0) {
        printf("read_cfg_file:fread err!\n");
        resfile_close(fp);
        return FALSE;
    }

    resfile_close(fp);

    return TRUE;
}

#define LIC_PAGE_OFFSET 		80
#define FORCE_TO_ERASE		    1

typedef enum _FLASH_ERASER {
    CHIP_ERASER = 0,
    BLOCK_ERASER,
    SECTOR_ERASER,
    PAGE_ERASER,
} FLASH_ERASER;

typedef struct __flash_of_lic_para_head {
    s16 crc;
    u16 string_len;
    const u8 para_string[];
} __attribute__((packed)) _flash_of_lic_para_head;

static bool license_para_head_check(u8 *para)
{
    _flash_of_lic_para_head *head;

    //fill head
    head = (_flash_of_lic_para_head *)para;

    ///crc check
    u8 *crc_data = (u8 *)(para + sizeof(((_flash_of_lic_para_head *)0)->crc));
    u32 crc_len = sizeof(_flash_of_lic_para_head) - sizeof(((_flash_of_lic_para_head *)0)->crc)/*head crc*/ + (head->string_len)/*content crc,include end character '\0'*/;
    s16 crc_sum = 0;

    crc_sum = CRC16(crc_data, crc_len);

    if (crc_sum != head->crc) {
        printf("license crc error !!! %x %x \n", (u32)crc_sum, (u32)head->crc);
        return false;
    }

    return true;
}

//获取三元组信息的头指针
const u8 *multi_protocol_get_license_ptr(void)
{
    u32 flash_capacity = sdfile_get_disk_capacity();
    u32 flash_addr = flash_capacity - 256 + LIC_PAGE_OFFSET;
    u8 *lic_ptr = NULL;
    _flash_of_lic_para_head *head;

    printf("flash capacity:%x \n", flash_capacity);
    lic_ptr = (u8 *)sdfile_flash_addr2cpu_addr(flash_addr);

    //head length check
    head = (_flash_of_lic_para_head *)lic_ptr;
    if (head->string_len >= 0xff) {
        printf("license length error !!! \n");
        return NULL;
    }

    ////crc check
    if (license_para_head_check(lic_ptr) == (false)) {
        printf("license head check fail\n");
        return NULL;
    }

    //put_buf(lic_ptr, 128);

    lic_ptr += sizeof(_flash_of_lic_para_head);
    return lic_ptr;
}

extern bool sfc_erase(int cmd, u32 addr);
extern u32 sfc_write(const u8 *buf, u32 addr, u32 len);
//将三元组信息保存到flash里面
int multi_protocol_license2flash(const u8 *data, u16 len)
{
    _flash_of_lic_para_head header;
    u8 buffer[256];
    u32 flash_capacity = sdfile_get_disk_capacity();
    u32 sector = flash_capacity - 4 * 1024;
    u32 page_addr = flash_capacity - 256;
    u8 *auif_ptr = (u8 *)sdfile_flash_addr2cpu_addr(page_addr);

#if (!FORCE_TO_ERASE)
    log_info("not supported flash erase !!! \n");
    return (-1);
#endif

    ///save last 256 byte
    /* memset(buffer, 0xff, sizeof(buffer)); */
    memcpy(buffer, auif_ptr, sizeof(buffer));
    auif_ptr += LIC_PAGE_OFFSET;

    header.string_len = len;
    ///length
    memcpy(&buffer[LIC_PAGE_OFFSET + sizeof(header.crc)], &(header.string_len), sizeof(header.string_len));
    ///context
    memcpy(&buffer[LIC_PAGE_OFFSET + sizeof(header)], data, len);
    ///crc
    header.crc = CRC16(&buffer[LIC_PAGE_OFFSET + sizeof(header.crc)], sizeof(header.string_len) + header.string_len);
    memcpy(&buffer[LIC_PAGE_OFFSET], &(header.crc), sizeof(header.crc));

    ///check if need update data to flash,erease license erea if there are some informations in license erea
    if (!memcmp(auif_ptr, buffer + LIC_PAGE_OFFSET, len + sizeof(_flash_of_lic_para_head))) {
        log_info("flash information the same with license\n");
        return 0;
    }

    log_info("erase license sector \n");
    sfc_erase(SECTOR_ERASER, sector);

    log_info("write license to flash \n");
    sfc_write(buffer, page_addr, 256);

    return 0;
}
/*******************************************************
               三元组记录相关接口 end
*******************************************************/


#endif

