#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".tuya_ota.data.bss")
#pragma data_seg(".tuya_ota.data")
#pragma const_seg(".tuya_ota.text.const")
#pragma code_seg(".tuya_ota.text")
#endif
#include "app_config.h"
#include "tuya_ota.h"
#include "tuya_ble_app_demo.h"
#include "dual_bank_updata_api.h"
#include "update_loader_download.h"
#include "update_tws_new.h"
#include "timer.h"
#include "clock.h"
#include "clock_manager/clock_manager.h"
#include "classic/tws_api.h"
#include "update_tws_new.h"
#include "btstack/avctp_user.h"
#include "os/os_api.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & TUYA_DEMO_EN)

u8 dual_bank_update_verify_without_crc(int (*verify_result_hdl)(int calc_crc));
tuya_ble_status_t tuya_ble_ota_response(tuya_ble_ota_response_t *p_data);
extern void tuya_post_key_event(u8 type);
extern int tws_ota_data_send_pend(void);

static int old_sys_clk;

#define OTA_WRITE_FLASH_SIZE            (1024)

typedef struct {
    u32 file_size;
    u32 recv_len;
    u32 file_crc;
    u16 tuya_ota_packet_num;
    u8  buff[OTA_WRITE_FLASH_SIZE];
    u32 buff_size;
} tuya_ota_t;

tuya_ota_t tuya_ota;

__attribute__((weak))
u8 tuya_start_ota_check(void)
{
    return 0;
}

void tuya_ota_reset(void *priv)
{
    printf("\ncpu_reset!");
    cpu_reset();
}

int tuya_ota_boot_info_cb(int err)
{
    printf("tuya_ota_boot_info_cb:%d", err);
#if (TCFG_USER_TWS_ENABLE && OTA_TWS_SAME_TIME_ENABLE)
    extern int tws_ota_result(u8 err);
    tws_ota_result(err);
#else
    if (err == 0) {
        sys_timeout_add(NULL, tuya_ota_reset, 2000);
    }
#endif
    return 0;
}

int tuya_clk_resume(int priv)
{
    clock_free("tuya");     //恢复时钟
    return 0;
}

int tuya_ota_file_end_response(void *priv)
{
    tuya_ble_ota_response_t response;
    tuya_ota_end_response_t ota_end_response;
    ota_end_response.reserve = 0;

#if (OTA_TWS_SAME_TIME_ENABLE)
    // 把最后一次OTA_DATA的信号量pend到再开始校验
    if (tws_ota_data_send_pend()) {
        printf("last ota data pend error");
        ota_end_response.state = 2;
        goto TUYA_VERIFY_END;
    }
#endif

    clock_alloc("tuya", 160 * 1000000L);     //提升系统时钟提高校验速度
    if (dual_bank_update_verify_without_crc(tuya_clk_resume) == 0) {
        printf("UPDATE SUCCESS");
#if (OTA_TWS_SAME_TIME_ENABLE)
        if (tws_ota_enter_verify_without_crc(NULL)) {
            printf("SIBLING VERIFY_WITHOUT_CRC ERROR");
            ota_end_response.state = 3;
            goto TUYA_VERIFY_END;
        } else {
            printf("SIBLING VERIFY_SUCCESS");
            ota_end_response.state = 0;
        }
        u8 res, up_flg;
        if (!tws_ota_exit_verify(&res, &up_flg)) {
            printf("SIBLING BURN_BOOT ERROR");
            ota_end_response.state = 3;
        } else {
            printf("SIBLING BURN_BOOT SUCCESS");
            ota_end_response.state = 0;
        }
#else
        ota_end_response.state = 0;
#endif
        dual_bank_update_burn_boot_info(tuya_ota_boot_info_cb);
    } else {
        printf("UPDATE FAILURE");
        ota_end_response.state = 2;
    }

//cppcheck-suppress unusedLabel
TUYA_VERIFY_END:
    response.type = TUYA_BLE_OTA_END;
    response.p_data = (uint8_t *)&ota_end_response;
    response.data_len = 2;
    return tuya_ble_ota_response(&response);
}

int tuya_ota_data_response()
{
    tuya_ble_ota_response_t response;
    tuya_ota_data_response_t ota_data_response;
    response.type = TUYA_BLE_OTA_DATA;
    ota_data_response.reserve = 0;
    ota_data_response.state = TUYA_OTA_DATA_SUCC;
    response.p_data = (uint8_t *)&ota_data_response;
    response.data_len = 2;
    tuya_ble_ota_response(&response);
    return 0;
}

int tuya_update_write_cb(void *priv)
{
    tuya_ota_data_response();
    return 0;
}

void tuya_ota_proc(u16 type, u8 *recv_data, u32 recv_len)
{
    tuya_ble_ota_response_t response;
    static bool first_packet_come = 0;
    static u32 current_file_size = 0;

    switch (type) {
    case TUYA_BLE_OTA_REQ:
        printf("TUYA_BLE_OTA_REQ\n");
        if (bt_a2dp_get_status() == 1) {
            //tuya_post_key_event(TUYA_MUSIC_PP);
        }
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_STOP, 0, NULL);
        dual_bank_passive_update_exit(NULL);            //tuya APP退到后台会中断发数，重新开始REQ要退出上一次的任务
        tuya_ota_req_response_t ota_req_response;
        ota_req_response.flag = tuya_start_ota_check();
        ota_req_response.ota_version = 3;
        ota_req_response.reserve = 0;
        ota_req_response.frame_version = TY_APP_VER_NUM;
        ota_req_response.max_pkt_len   = OTA_MAX_DATA_LEN;
        first_packet_come = 0;
        current_file_size = 0;
        response.p_data = (uint8_t *)&ota_req_response;
        response.data_len = 9;
        memset(&tuya_ota, 0, sizeof(tuya_ota));
        break;
    case TUYA_BLE_OTA_FILE_INFO:
        printf("TUYA_BLE_OTA_FILE_INFO\n");
        tuya_ota_file_info_response_t ota_file_info_response;
        ota_file_info_response.reserve = 0;
        tuya_ota.file_size = READ_BIG_U32(recv_data + 29);
        uint32_t file_version = READ_BIG_U32(recv_data + 9);
        printf("TUYA_BLE_OTA_FILE_INFO file_size%d file_version%x", tuya_ota.file_size, file_version);
        printf("task:%s", os_current_task());
#if (OTA_TWS_SAME_TIME_ENABLE)
        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
            struct __tws_ota_para ota_para;
            ota_para.fm_crc = 0;
            ota_para.fm_size = tuya_ota.file_size;
            ota_para.max_pkt_len = OTA_WRITE_FLASH_SIZE;
            tws_ota_open(&ota_para);
        }
#endif
        set_ota_status(1);
        dual_bank_passive_update_init(0, tuya_ota.file_size, OTA_WRITE_FLASH_SIZE, NULL);
        if (dual_bank_update_allow_check(tuya_ota.file_size) != 0) {
#if (OTA_TWS_SAME_TIME_ENABLE)
            tws_ota_close();
#endif
            dual_bank_passive_update_exit(NULL);
            set_ota_status(0);
            ota_file_info_response.state = TUYA_OTA_STATE_FILE_SIZE_TOO_LARGE;
        } else if (file_version <= TY_APP_VER_NUM) {
#if (OTA_TWS_SAME_TIME_ENABLE)
            tws_ota_close();
#endif
            dual_bank_passive_update_exit(NULL);
            set_ota_status(0);
            printf("ota remote version lower than current version, ota abort");
            ota_file_info_response.state = TUYA_OTA_STATE_VER_LOW;
        } else {
            ota_file_info_response.state = TUYA_OTA_STATE_NORMAL;
        }
        response.p_data = (uint8_t *)&ota_file_info_response;
        response.data_len = 26;
        break;
    case TUYA_BLE_OTA_FILE_OFFSET_REQ:
        printf("TUYA_BLE_OTA_FILE_OFFSET_REQ\n");
        uint8_t data[5] = {0};
        response.type = TUYA_BLE_OTA_FILE_OFFSET_REQ;
        u32 offset_addr = READ_BIG_U32(recv_data + 1);
        data[1] = (u8)(offset_addr >> 24);
        data[2] = (u8)(offset_addr >> 16);
        data[3] = (u8)(offset_addr >> 8);
        data[4] = (u8)(offset_addr >> 0);
        printf("offset_addr = 0x%x", offset_addr);
        response.data_len = 5;
        response.p_data = data;
        tuya_ble_ota_response(&response);
        return;
        /* tuya_ota_file_offset_response_t ota_file_offset_response; */
        /* ota_file_offset_response.reserve = 0; */
        /* ota_file_offset_response.offset = recv_data[1] << 24 | recv_data[2] << 16 | recv_data[3] << 8 | recv_data[4]; */
        /* printf("offset:%d\n", ota_file_offset_response.offset); */
        /* response.p_data = &ota_file_offset_response;  */
        /* response.data_len = 2; */
        break;
    case TUYA_BLE_OTA_DATA:
        printf("TUYA_BLE_OTA_DATA\n");
        tuya_ota_data_response_t ota_data_response;
        ota_data_response.reserve = 0;
        u16 remote_packet_num = (recv_data[1] << 8) + recv_data[2];
        u32 cur_frame_size = (recv_data[3] << 8) + recv_data[4];

        if (remote_packet_num == 0 && first_packet_come == 1) {
            printf("OTA ERROR packet num error, num = %d", remote_packet_num);
            ota_data_response.state = TUYA_OTA_DATA_PKT_NUM_ERR;
            response.p_data = (uint8_t *)&ota_data_response;
            response.data_len = 2;
            response.type = TUYA_BLE_OTA_DATA;
            tuya_ble_ota_response(&response);
            return;
        }

        current_file_size += cur_frame_size;
        printf("OTA DATA info:");
        put_buf(recv_data, 7);
        printf("file_size:%d remote_packet_num:%d, last_packet_num = %d, frame_size:%d\n", current_file_size, remote_packet_num, tuya_ota.tuya_ota_packet_num, recv_len - 7);

        first_packet_come = 1;

        if (remote_packet_num && remote_packet_num != tuya_ota.tuya_ota_packet_num + 1) {
            printf("OTA ERROR packet num error");
            ota_data_response.state = TUYA_OTA_DATA_PKT_NUM_ERR;
            response.p_data = (uint8_t *)&ota_data_response;
            response.data_len = 2;
            tuya_ble_ota_response(&response);
            //dual_bank_passive_update_exit(NULL);
            return;
        } else {
            tuya_ota.recv_len += recv_len - 7;
            memcpy(tuya_ota.buff + tuya_ota.buff_size, recv_data + 7, recv_len - 7);
            tuya_ota.buff_size += recv_len - 7;
            if (tuya_ota.buff_size >= OTA_WRITE_FLASH_SIZE) {
#if (OTA_TWS_SAME_TIME_ENABLE)
                if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
                    tws_ota_data_send_m_to_s(tuya_ota.buff, OTA_WRITE_FLASH_SIZE);
                } else {
                    printf("TWS DISCONNECT, OTA ABORT!");
                    ota_data_response.state = TUYA_OTA_DATA_OTHER_ERR;
                    response.p_data = (uint8_t *)&ota_data_response;
                    response.data_len = 2;
                    response.type = TUYA_BLE_OTA_DATA;
                    tuya_ble_ota_response(&response);
                }
#endif
                dual_bank_update_write(tuya_ota.buff, OTA_WRITE_FLASH_SIZE, tuya_update_write_cb);
                tuya_ota.buff_size -= OTA_WRITE_FLASH_SIZE;
                tuya_ota.tuya_ota_packet_num = remote_packet_num;
                return;
            } else {
                tuya_ota_data_response();
                tuya_ota.tuya_ota_packet_num = remote_packet_num;
                return;
            }
        }
        tuya_ota.tuya_ota_packet_num = remote_packet_num;
        break;
    case TUYA_BLE_OTA_END:
        printf("Pend the second last ota data sibling rsp");
#if TCFG_USER_TWS_ENABLE
        tws_ota_data_send_pend();
#endif
        printf("TUYA_BLE_OTA_END\n");
        if (tuya_ota.buff_size != 0) {          //把剩余的数据写入flash
#if (OTA_TWS_SAME_TIME_ENABLE)
            if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
                tws_ota_data_send_m_to_s(tuya_ota.buff, tuya_ota.buff_size);
            } else {
                printf("TWS DISCONNECT, OTA ABORT!");
                ota_data_response.state = TUYA_OTA_DATA_OTHER_ERR;
                response.p_data = (uint8_t *)&ota_data_response;
                response.data_len = 2;
                response.type = TUYA_BLE_OTA_DATA;
                tuya_ble_ota_response(&response);
            }
#endif
            dual_bank_update_write(tuya_ota.buff, tuya_ota.buff_size, tuya_ota_file_end_response);
        } else {
            tuya_ota_file_end_response(NULL);
        }
        break;
    }
    response.type = type;
    tuya_ble_ota_response(&response);
}

#endif
