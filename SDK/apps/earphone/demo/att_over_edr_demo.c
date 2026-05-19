#include "system/includes.h"
#include "app_config.h"
#include "app_msg.h"
#include "btcontroller_config.h"
#include "bt_common.h"
#include "btstack/avctp_user.h"
#include "btstack/le/ble_api.h"
#include "btstack/le/att.h"


/*
开启功能需要在apps/common/config/bt_profile_config.c中修改
const u8 adt_profile_support = 1;

edr att接入中间层：
CUSTOM_DEMO_EN demo中配置了gatt over edr的初始化
开启 CUSTOM_DEMO_EN 和 ATT_OVER_EDR_DEMO_EN 即可测试
使用中间层的注册接口，实现发送和接收数据；
#define EDR_ATT_HDL_UUID \
	(((u8)('E' + 'D' + 'R') << (1 * 8)) | \
	 ((u8)('A' + 'T' + 'T') << (0 * 8)))
if (rcsp_adt_support) {
if (rcsp_server_edr_att_hdl == NULL) {
    rcsp_server_edr_att_hdl = app_ble_hdl_alloc();
}
    app_ble_adv_address_type_set(rcsp_server_edr_att_hdl, 0);
    app_ble_gatt_over_edr_connect_type_set(rcsp_server_edr_att_hdl, 1);
    app_ble_hdl_uuid_set(rcsp_server_edr_att_hdl, EDR_ATT_HDL_UUID);
    app_ble_profile_set(rcsp_server_edr_att_hdl, rcsp_profile_data);
    app_ble_callback_register(rcsp_server_edr_att_hdl);
}

注意：edr连接之后app连接需要开ble广播出来用于发现设备，广播类型可连接和不可连接都可以
app连接成功后可关闭广播。
*/

#if ATT_OVER_EDR_DEMO_EN
#define log_info(x, ...)  printf("\n[###edr_att_demo@@@]" x " ", ## __VA_ARGS__)
extern const u8 adt_profile_support;
u8 rcsp_adt_support = 1; //edr att是否接入rcsp


const u8 sdp_att_service_data[60] = {                           //
    0x36, 0x00, 0x31, 0x09, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x21, 0x09, 0x00, 0x01, 0x35, 0x03,
    0x19, 0x18, 0x01, 0x09, 0x00, 0x04, 0x35, 0x13, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x1F,
    0x35, 0x09, 0x19, 0x00, 0x07, 0x09, 0x00, 0x01, 0x09, 0x00, 0x04, 0x09, 0x00, 0x05, 0x35, 0x03,
    0x19, 0x10, 0x02, 0x00                    //                //
};
const u8 sdp_att_service_data1[60] = {
    0x36, 0x00, 0x31, 0x09, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x22, 0x09, 0x00, 0x01, 0x35, 0x03,
    0x19, 0xAE, 0x00, 0x09, 0x00, 0x04, 0x35, 0x13, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x1F,
    0x35, 0x09, 0x19, 0x00, 0x07, 0x09, 0x00, 0x05, 0x09, 0x00, 0x0a, 0x09, 0x00, 0x05, 0x35, 0x03,
    0x19, 0x10, 0x02, 0x00
};
const u8 sdp_att_service_data2[60] = {
    0x36, 0x00, 0x31, 0x09, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x23, 0x09, 0x00, 0x01, 0x35, 0x03,
    0x19, 0xBF, 0x00, 0x09, 0x00, 0x04, 0x35, 0x13, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x1F,
    0x35, 0x09, 0x19, 0x00, 0x07, 0x09, 0x00, 0x0b, 0x09, 0x00, 0x10, 0x09, 0x00, 0x05, 0x35, 0x03,
    0x19, 0x10, 0x02, 0x00
};

typedef struct {
    // linked list - assert: first field
    void *offset_item;
    // data is contained in same memory
    u32        service_record_handle;
    u8         *service_record;
} service_record_item_t;
#define SDP_RECORD_HANDLER_REGISTER(handler) \
    const service_record_item_t  handler \
    sec(.sdp_record_item)
SDP_RECORD_HANDLER_REGISTER(spp_att_record_item) = {
    .service_record = (u8 *)sdp_att_service_data,
    .service_record_handle = 0x00010021,
};
SDP_RECORD_HANDLER_REGISTER(spp_att_record_item1) = {
    .service_record = (u8 *)sdp_att_service_data1,
    .service_record_handle = 0x00010022,
};
SDP_RECORD_HANDLER_REGISTER(spp_att_record_item2) = {
    .service_record = (u8 *)sdp_att_service_data2,
    .service_record_handle = 0x00010023,
};

void att_profile_init()
{
    bredr_adt_init();
}

static int edr_att_btstack_event_handler(int *msg)
{
    struct bt_event *bt = (struct bt_event *)msg;

    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        att_profile_init();
        break;
    }
    return 0;
}
APP_MSG_HANDLER(edr_att_stack_msg_entry) = {
    .owner      = 0xff,
    .from       = MSG_FROM_BT_STACK,
    .handler    = edr_att_btstack_event_handler,
};

#endif

