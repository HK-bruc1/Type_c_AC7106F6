#include "pub_mutual_set_cmd_opt.h"
#include "rcsp_config.h"
#include "rcsp.h"
#include "file_transfer.h"
#include "file_transfer_sync.h"

#if (RCSP_MODE && (TCFG_DEV_MANAGER_ENABLE || RCSP_TONE_FILE_TRANSFER_ENABLE))

#pragma pack(1)

typedef struct put_mutual_set_cmd_param_t {
    u8 version;
    u16 function;
    u8 sub_version;
    u8 op;
} cmd_param;

typedef struct put_mutual_set_cmd_notify_t {
    cmd_param param;
    u8 result;
} cmd_notify;

typedef struct pub_mutual_set_cmd_resp_t {
    u8 result;
    cmd_param param;
} cmd_resp;

#pragma pack()

void public_settings_interaction_command(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    u8 offset = 0;
    u16 function = 0;
    cmd_resp resp_data = {0};
    resp_data.param.version = data[offset++];
    function = READ_BIG_U16(data + offset);
    memcpy(&resp_data.param.function, data + offset, sizeof(resp_data.param.function));
    offset += 2;
    resp_data.param.sub_version = data[offset++];
    resp_data.param.op = data[offset++];
    switch (function)    {
    case PUB_MTUAL_SET_CMD_TONE_FUNCTION:
        u32 dev_handle = READ_BIG_U32(data + offset);
        offset += 4;
        u32 file_size = READ_BIG_U32(data + offset);
        offset += 4;
        u16 crc = READ_BIG_U16(data + offset);
        offset += 2;
        u8 file_len = data[offset++];
        u8 *file_name = data + offset;
        file_trans_init(1, ble_con_handle, spp_remote_addr);
        resp_data.result = 0;
        break;
    }
    JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, (u8 *)&resp_data, sizeof(resp_data), ble_con_handle, spp_remote_addr);
}

void public_settings_interaction_command_notify(u16 function, u8 result, u8 op, u16 ble_con_handle, u8 *spp_remote_addr)
{
    cmd_notify notify_data = {0};
    WRITE_BIG_U16(&notify_data.param.function, function);
    notify_data.result = result;
    notify_data.param.op = op;
    JL_CMD_send(JL_OPCODE_PUBLIC_SET_CMD, (u8 *)&notify_data, sizeof(notify_data), JL_NOT_NEED_RESPOND, ble_con_handle, spp_remote_addr);
}

#endif
