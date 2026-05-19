#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".rcsp_auracast.data.bss")
#pragma data_seg(".rcsp_auracast.data")
#pragma const_seg(".rcsp_auracast.text.const")
#pragma code_seg(".rcsp_auracast.text")
#endif

#include "rcsp_auracast.h"

#if (defined(RCSP_ADV_AURCAST_SOURCE) && RCSP_ADV_AURCAST_SOURCE)

struct _auracast_tx_vm_config *tx_config = NULL;

void auracast_app_reset_vm_config(struct _auracast_tx_vm_config *config)
{
    int wlen, ret;
    u8 tmp[128] = {0};
    // to do reset config
    printf("auracast_app_reset_vm_config\n");
    if (config == NULL) {
        printf("config == NULL\n");
        return;
    }

    //-----------------------------CFG_AURACAST_NAME----------------------------------//
    ret = syscfg_read(CFG_BIS_NAME, tmp, AURACAST_NAME_MAX_LEN);

    if (ret < 0) {
        printf("read auracast_name err, set default");
        memcpy(config->auracast_name, "JL_auracast", 11);
        config->auracast_name_len = 11;
    } else if (ret >= AURACAST_NAME_MAX_LEN) {
        memset(config->auracast_name, 0x00, AURACAST_NAME_MAX_LEN);
        memcpy(config->auracast_name, tmp, AURACAST_NAME_MAX_LEN);
        config->auracast_name[AURACAST_NAME_MAX_LEN - 1] = 0;
        config->auracast_name_len = AURACAST_NAME_MAX_LEN;
    } else {
        memset(config->auracast_name, 0x00, AURACAST_NAME_MAX_LEN);
        memcpy(config->auracast_name, tmp, ret);
        config->auracast_name_len = strlen((const char *)config->auracast_name);
    }

    //-----------------------------CFG_AURACAST_CODE----------------------------------//
    ret = syscfg_read(CFG_BIS_CODE, tmp, AURACAST_CODE_MAX_LEN);

    if (ret < 0) {
        printf("read auracast_broadcast_code err, set default");
        memcpy(config->auracast_broadcast_code, "abcdefghijklmnop", 16);
    } else if (ret >= AURACAST_CODE_MAX_LEN) {
        memset(config->auracast_broadcast_code, 0x00, AURACAST_CODE_MAX_LEN);
        memcpy(config->auracast_broadcast_code, tmp, AURACAST_CODE_MAX_LEN);
        config->auracast_broadcast_code[AURACAST_CODE_MAX_LEN - 1] = 0;
    } else {
        memset(config->auracast_broadcast_code, 0x00, AURACAST_CODE_MAX_LEN);
        memcpy(config->auracast_broadcast_code, tmp, ret);
    }

    //-----------------------------CFG_AURACAST_PASSWORD----------------------------------//
    ret = syscfg_read(CFG_BIS_PASSWORD, tmp, AURACAST_PASSWORD_MAX_LEN);

    if (ret < 0) {
        printf("read auracast_password err, set default");
        memcpy(config->auracast_password, "888888", 6);
        config->auracast_password_len = 6;
    } else if (ret >= AURACAST_PASSWORD_MAX_LEN) {
        memset(config->auracast_password, 0x00, AURACAST_PASSWORD_MAX_LEN);
        memcpy(config->auracast_password, tmp, AURACAST_PASSWORD_MAX_LEN);
        config->auracast_password[AURACAST_PASSWORD_MAX_LEN - 1] = 0;
        config->auracast_password_len = AURACAST_PASSWORD_MAX_LEN;
    } else {
        memset(config->auracast_password, 0x00, AURACAST_PASSWORD_MAX_LEN);
        memcpy(config->auracast_password, tmp, ret);
        config->auracast_password_len = strlen((const char *)config->auracast_password);
    }

    //-----------------------------CFG_AURACAST_OTHER_PARAM----------------------------------//
    config->auracast_encryption_en = AURACAST_BIS_ENCRYPTION_ENABLE;
    config->auracast_audio_index = AURACAST_BIS_SAMPLING_RATE * 2 + AURACAST_BIS_VARIANT;
    config->auracast_tx_power_level = AURACAST_BIS_RF_POWER;

    printf("config->auracast_encryption_en : %d\n", config->auracast_encryption_en);
    printf("config->auracast_password_len  : %d\n", config->auracast_password_len);
    printf("config->auracast_name_len      : %d\n", config->auracast_name_len);
    printf("config->auracast_audio_index   : %d\n", config->auracast_audio_index);
    printf("config->auracast_tx_power_level: %d\n", config->auracast_tx_power_level);
    wlen = syscfg_write(VM_AURACAST_TX_CONFIG, config, sizeof(struct _auracast_tx_vm_config));
    if (wlen != sizeof(struct _auracast_tx_vm_config)) {
        printf("VM_AURACAST_TX_CONFIG save faild !!\n");
        return;
    }
}

int auracast_app_read_vm_config(struct _auracast_tx_vm_config *config)
{
    printf("auracast_app_read_vm_config\n");
    int rlen = 0;
    rlen = syscfg_read(VM_AURACAST_TX_CONFIG, config, sizeof(struct _auracast_tx_vm_config));
    if (rlen != sizeof(struct _auracast_tx_vm_config)) {
        printf("VM_AURACAST_TX_CONFIG no record, use default\n");
        auracast_app_reset_vm_config(config);
        return 1;
    }
    printf("config->auracast_encryption_en : %d\n", config->auracast_encryption_en);
    printf("config->auracast_password_len  : %d\n", config->auracast_password_len);
    printf("config->auracast_name_len      : %d\n", config->auracast_name_len);
    printf("config->auracast_audio_index   : %d\n", config->auracast_audio_index);
    printf("config->auracast_tx_power_level: %d\n", config->auracast_tx_power_level);
    return 0;
}

int auracast_app_login_authentication_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    printf("auracast_app_login_authentication_deal %d\n", payload_len);
    put_buf(payload, payload_len);
    u8 result = 0x00;
    printf("len %d -> %d\n", payload[0], tx_config->auracast_password_len);

    if (tx_config == NULL) {
        printf("tx_config == NULL\n");
        return -1;
    }

    if (payload[0] != tx_config->auracast_password_len) {
        result = 0x01;
        return auracast_app_packet_response(0x01, opcode, sn, &result, 1);
    }
    if (0 != memcmp(tx_config->auracast_password, &payload[1], tx_config->auracast_password_len)) {
        result = 0x01;
        return auracast_app_packet_response(0x01, opcode, sn, &result, 1);
    }
    return auracast_app_packet_response(0x00, opcode, sn, &result, 1);
}

int auracast_app_login_password_set_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    printf("auracast_app_login_password_set_deal %d\n", payload_len);
    put_buf(payload, payload_len);
    u8 result = 0x00;
    u8 old_password_len = payload[0];
    u8 *old_password_ptr = &payload[1];
    u8 new_password_len = payload[old_password_len + 1];
    u8 *new_password_ptr = &payload[old_password_len + 2];;

    if (tx_config == NULL) {
        printf("tx_config == NULL\n");
        return -1;
    }

    printf("len %d -> %d\n", old_password_len, new_password_len);

    if ((old_password_len != tx_config->auracast_password_len) \
        || (0 != memcmp(tx_config->auracast_password, old_password_ptr, tx_config->auracast_password_len))) {
        result = 0x01;
    } else if ((new_password_len == tx_config->auracast_password_len) \
               && (0 == memcmp(tx_config->auracast_password, new_password_ptr, new_password_len))) {
        result = 0x02;
    } else if (new_password_len > 32) {
        result = 0x03;
    } else {
        tx_config->auracast_password_len = new_password_len;
        memcpy(tx_config->auracast_password, new_password_ptr, new_password_len);
    }
    printf("result = %d\n", result);
    return auracast_app_packet_response(0x00, opcode, sn, &result, 1);
}

int auracast_app_broadcast_setting_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    printf("auracast_app_broadcast_setting_deal %d\n", payload_len);
    put_buf(payload, payload_len);

    if (tx_config == NULL) {
        printf("tx_config == NULL\n");
        return -1;
    }

    u8 action = payload[0]; // 0:get 1:set 2:notify
    u8 param_len = payload[1];
    u8 *param = &payload[2];
    u8 tbuf[64];
    u16 tlen = 0;
    tbuf[tlen++] = action;
    tbuf[tlen++] = 0;         // info len

    switch (action) {
    case 0x00:    // get
        while (param_len--) {
            switch (*param) {
            case 0x01:      //  Broadcast Name 设置（0x01）
                tbuf[tlen++] = tx_config->auracast_name_len + 1;
                tbuf[tlen++] = *param;
                memcpy(&tbuf[tlen], tx_config->auracast_name, tx_config->auracast_name_len);
                tlen += tx_config->auracast_name_len;
                break;
            case 0x02:      // 音频格式设置（0x02）
                tbuf[tlen++] = 2;
                tbuf[tlen++] = *param;
                tbuf[tlen++] = tx_config->auracast_audio_index + 1;
                break;
            case 0x03:      // 加密设置（0x03）
                tbuf[tlen++] = 18;
                tbuf[tlen++] = *param;
                tbuf[tlen++] = tx_config->auracast_encryption_en;
                memcpy(&tbuf[tlen], tx_config->auracast_broadcast_code, 16);
                tlen += 16;
                break;
            case 0x04:      // 发射功率设置（0x04）
                tbuf[tlen++] = 2;
                tbuf[tlen++] = *param;
                tbuf[tlen++] = tx_config->auracast_tx_power_level;
                break;
            }
            param++;
        }
        break;
    case 0x01:    // set
        while (param_len) {
            if (param[0] + 1 > param_len) {
                printf("ltv format error\n");
                goto __error;
            }
            switch (param[1]) {
            case 0x01:      //  Broadcast Name 设置（0x01）
                tbuf[tlen++] = param[1];
                if ((param[0] - 1) > 32) {
                    tbuf[tlen++] = 0x01;   // error
                } else {
                    tbuf[tlen++] = 0x00;   // suss
                    memcpy(tx_config->auracast_name, &param[2], param[0] - 1);
                    tx_config->auracast_name_len = param[0] - 1;
                }
                break;
            case 0x02:      // 音频格式设置（0x02）
                tbuf[tlen++] = param[1];
                tbuf[tlen++] = 0x00;   // suss
                tx_config->auracast_audio_index = param[2] - 1;
                break;
            case 0x03:      // 加密设置（0x03）
                tbuf[tlen++] = param[1];
                if ((param[0] - 1) == (1 + 16)) {
                    tbuf[tlen++] = 0x00;   // suss
                    tx_config->auracast_encryption_en = param[2];
                    memcpy(tx_config->auracast_broadcast_code, &param[3], 16);
                } else {
                    tbuf[tlen++] = 0x01;   // error
                }
                break;
            case 0x04:      // 发射功率设置（0x04）
                tbuf[tlen++] = param[1];
                tbuf[tlen++] = 0x00;   // suss
                tx_config->auracast_tx_power_level = param[2];
                break;
            }
            param_len -= param[0] + 1;
            param += param[0] + 1;
        }
        break;
    }
    tbuf[1] = tlen - 2;
    return auracast_app_packet_response(0x00, opcode, sn, tbuf, tlen);

__error:
    return auracast_app_packet_response(0x01, opcode, sn, tbuf, tlen);
}

void auracast_app_cpu_reset_do(void *priv)
{
    cpu_reset();
}

int auracast_app_broadcast_reset_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    int wlen = 0;
    u8 action = payload[0];
    printf("auracast_app_broadcast_reset_deal %d\n", payload_len);
    put_buf(payload, payload_len);

    if (tx_config == NULL) {
        printf("tx_config == NULL\n");
        return -1;
    }

    switch (action) {
    case 0x00:      // 关机
        auracast_app_ble_disconnect();
        wlen = syscfg_write(VM_AURACAST_TX_CONFIG, tx_config, sizeof(struct _auracast_tx_vm_config));
        if (wlen != sizeof(struct _auracast_tx_vm_config)) {
            printf("VM_AURACAST_TX_CONFIG save faild !!\n");
            return -1;
        }
        sys_timeout_add(NULL, auracast_app_cpu_reset_do, 1000);
        break;
    case 0x01:      // 重启
        auracast_app_ble_disconnect();
        wlen = syscfg_write(VM_AURACAST_TX_CONFIG, tx_config, sizeof(struct _auracast_tx_vm_config));
        if (wlen != sizeof(struct _auracast_tx_vm_config)) {
            printf("VM_AURACAST_TX_CONFIG save faild !!\n");
            return -1;
        }
        sys_timeout_add(NULL, auracast_app_cpu_reset_do, 1000);
        break;
    case 0x02:      // 恢复出厂设置
        auracast_app_ble_disconnect();
        auracast_app_reset_vm_config(tx_config);
        sys_timeout_add(NULL, auracast_app_cpu_reset_do, 1000);
        break;
    }
    return 0;
}

int auracast_app_source_api_init(void)
{
    if (tx_config != NULL) {
        printf("tx_config != NULL\n");
        return -1;
    }
    tx_config = zalloc(sizeof(struct _auracast_tx_vm_config));
    if (tx_config == NULL) {
        printf("tx_config == NULL\n");
        return -1;
    }

    auracast_app_read_vm_config(tx_config);
    return 0;
}

int auracast_app_source_api_uninit(void)
{
    if (tx_config == NULL) {
        printf("tx_config == NULL\n");
        return -1;
    }
    free(tx_config);
    tx_config = NULL;
    return 0;
}

#else

int auracast_app_read_vm_config(struct _auracast_tx_vm_config *config)
{
    return -1;
}

void auracast_app_reset_vm_config(struct _auracast_tx_vm_config *config)
{
}

int auracast_app_login_authentication_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    return -1;
}

int auracast_app_login_password_set_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    return -1;
}

int auracast_app_broadcast_setting_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    return -1;
}

void auracast_app_cpu_reset_do(void *priv)
{
}

int auracast_app_broadcast_reset_deal(u8 opcode, u8 sn, u8 *payload, u32 payload_len)
{
    return -1;
}

int auracast_app_source_api_init(void)
{
    return -1;
}

int auracast_app_source_api_uninit(void)
{
    return -1;
}

#endif

