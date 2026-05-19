#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".rcsp_auracast.data.bss")
#pragma data_seg(".rcsp_auracast.data")
#pragma const_seg(".rcsp_auracast.text.const")
#pragma code_seg(".rcsp_auracast.text")
#endif

#include "rcsp_auracast.h"

#if ((defined(RCSP_ADV_AURCAST_SOURCE) && RCSP_ADV_AURCAST_SOURCE) || (defined(RCSP_ADV_AURCAST_SINK) && RCSP_ADV_AURCAST_SINK))

int auracast_app_packet_receive(u8 opcode, u8 sn, u8 *payload, u32 payload_len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    int ret = JL_PRO_STATUS_SUCCESS;
    printf("auracast_app_packet_receive op:0x%x sn:0x%x cmd:0x%x plen:%d\n", opcode, sn, payload[0], payload_len);

    switch (payload[0]) {
#if (defined(RCSP_ADV_AURCAST_SOURCE) && RCSP_ADV_AURCAST_SOURCE)
    case AURACAST_APP_OPCODE_RECV_LOGIN_AUTHENTICATION:
        auracast_app_login_authentication_deal(opcode, sn, payload, payload_len);
        break;
    case AURACAST_APP_OPCODE_RECV_LOGIN_PASSWORD_SET:
        auracast_app_login_password_set_deal(opcode, sn, payload, payload_len);
        break;
    case AURACAST_APP_OPCODE_RECV_BROADCAST_SETTING:
        auracast_app_broadcast_setting_deal(opcode, sn, payload, payload_len);
        break;
    case AURACAST_APP_OPCODE_RECV_DEVICE_RESET:
        auracast_app_broadcast_reset_deal(opcode, sn, payload, payload_len);
        break;
#endif
#if (defined(RCSP_ADV_AURCAST_SINK) && RCSP_ADV_AURCAST_SINK)
    case AURACAST_APP_OPCODE_RECV_SCAN_CONTROL:
        auracast_app_recv_scan_control_deal(opcode, sn, payload, payload_len, ble_con_handle, spp_remote_addr);
        break;
    case AURACAST_APP_OPCODE_RECV_DEVICE_STATUS:
        auracast_app_recv_device_status_deal(opcode, sn, payload, payload_len);
        break;
    case AURACAST_APP_OPCODE_RECV_LISTENING_CONTROL:
        ret = auracast_app_recv_listening_control_deal(opcode, sn, payload, payload_len, ble_con_handle, spp_remote_addr);
        break;
#endif
    default:
        ret = JL_PRO_STATUS_FAIL;
        break;
    }
    return 0;
}

int auracast_app_packet_response(u8 status, u8 opcode, u8 sn, u8 *payload, u32 payload_len, u16 ble_con_handle, u8 *spp_remote_addr)
{
    JL_CMD_response_send(opcode, status, sn, payload, payload_len, ble_con_handle, spp_remote_addr);
    return 0;
}

int auracast_app_packet_cmd(u8 *payload, u32 payload_len)
{
    JL_CMD_send(JL_OPTCODE_AURACAST_CMD, payload, payload_len, JL_NOT_NEED_RESPOND, 0, NULL);
    return 0;
}

#endif
