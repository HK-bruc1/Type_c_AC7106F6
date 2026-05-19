#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ll_task.data.bss")
#pragma data_seg(".ll_task.data")
#pragma const_seg(".ll_task.text.const")
#pragma code_seg(".ll_task.text")
#endif
#include "ll_task.h"
#include "os/os_api.h"
#include "ble_qiot_config.h"
#include "ble_qiot_service.h"
#include "ble_qiot_import.h"
#include "app_config.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

LL_PACKET_CONTROL LL_packet_c;

#define __this      (&LL_packet_c)

static void tencent_ll_task(void *arg)
{
    LL_PACKET_HEAD_T ll_packet_head;
    u8 *buffer = NULL;
    u8 result = 0;
    while (1) {
        os_sem_pend(&(__this->ll_sem), 0);
        if (cbuf_get_data_len(&(__this->cbuf)) > LL_PACKET_HEAD_LEN) {
            cbuf_read(&(__this->cbuf), &ll_packet_head, LL_PACKET_HEAD_LEN);
            buffer = malloc(ll_packet_head.len);
            if (buffer == NULL) {
                printf("buf malloc err\n");
                break;
            }
            cbuf_read(&(__this->cbuf), buffer, ll_packet_head.len);
            switch (ll_packet_head.packet_channel) {
            case LL_DEVICE_INFO_MSG_CH:
                result = ble_device_info_msg_handle((const char *)buffer, ll_packet_head.len);
                break;
            case LL_DATA_MSG_CH:
                result = ble_lldata_msg_handle((const char *)buffer, ll_packet_head.len);
                break;
            case LL_OTA_MSG_CH:
                result = ble_ota_msg_handle((const char *)buffer, ll_packet_head.len);
                break;
            }
            free(buffer);
        }
    }
}

int tencent_ll_task_init(void)
{
    os_sem_create(&(__this->ll_sem), 0);
    u32 malloc_size = (ble_get_user_data_mtu_size() + LL_PACKET_HEAD_LEN) * BLE_QIOT_TOTAL_PACKAGES;
    __this->tencent_ll_buf = malloc(malloc_size);
    memset(__this->tencent_ll_buf, 0x0, malloc_size);
    cbuf_init(&(__this->cbuf), __this->tencent_ll_buf, malloc_size);
    os_task_create(tencent_ll_task, NULL, 31, 512, 0, "tencent_ll_task");
    return 0;
}

void tencent_ll_packet_recieve(void *buf, u16 len)
{
    if (cbuf_is_write_able(&(__this->cbuf), len) >= len) {
        cbuf_write(&(__this->cbuf), buf, len);
    } else {
        printf("[L]\n");
    }
    os_sem_post(&(__this->ll_sem));
}

#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)
