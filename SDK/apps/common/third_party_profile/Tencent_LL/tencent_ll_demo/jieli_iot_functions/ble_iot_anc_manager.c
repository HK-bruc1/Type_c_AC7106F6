#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".ble_iot_anc_manager.data.bss")
#pragma data_seg(".ble_iot_anc_manager.data")
#pragma const_seg(".ble_iot_anc_manager.text.const")
#pragma code_seg(".ble_iot_anc_manager.text")
#endif
#include "ble_iot_anc_manager.h"
#include "app_config.h"
#include "app_power_manage.h"
#include "ble_iot_msg_manager.h"
#include "ble_iot_tws_manager.h"
#include "ble_iot_utils.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & LL_SYNC_EN)

#if CONFIG_ANC_ENABLE

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif
#include "audio_anc.h"

#if 0
#define log_info(x, ...)       printf("[LL_LOG_ANC]" x " ", ## __VA_ARGS__)
#define log_info_hexdump       put_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif


// anc信息：mode(1byte/0/1/2) + ( left_max(2byte) + right_max(2byte) + left_cur_val(2byte) + right_cur_val(2byte) ) * 3
static u8 g_anc_info[25] = {0};
static u8 iot_anc_event_flag = 0x0;

static void iot_enable_anc_event(void)
{
    iot_anc_event_flag = 1;
}

static u8 iot_get_anc_event_status(void)
{
    return iot_anc_event_flag;
}

/**
 * @brief 获取anc效果信息
 *
 * @param *anc_info[25]
 */
void iot_get_anc_effect_info(u8 *anc_info)
{
    memcpy(anc_info, g_anc_info, sizeof(g_anc_info));
}

/**
 * @brief 设置anc效果信息
 *
 * @param *anc_info[25]
 */
void iot_set_anc_effect_info(u8 *anc_info)
{
    memcpy(g_anc_info, anc_info, sizeof(g_anc_info));
}

/**
 * @brief 获取anc模式
 *
 * @result anc模式，0 - 关闭模式，1 - 降噪模式，2 - 通透模式
 */
u8 iot_get_anc_mode()
{
    u8 anc_vm_info[1] = {0};
    int ret = syscfg_read(CFG_RCSP_ADV_ANC_VOICE_MODE, anc_vm_info, 1);
    if (ret) {
        return anc_vm_info[0];
    } else {
        return 0;
    }
}

static void iot_update_anc_effect_vm_value(u8 *anc_info)
{
    // 与RCSP共用vm id
    log_info("-------------%s--------\n", __FUNCTION__);
    log_info_hexdump(anc_info, 25);
    u8 anc_vm_info[25] = {-1};
    int ret = syscfg_read(CFG_RCSP_ADV_ANC_VOICE_MODE, anc_vm_info, 1);
    if (0 != memcmp(anc_vm_info, anc_info, 1)) {
        ret = syscfg_write(CFG_RCSP_ADV_ANC_VOICE_MODE, anc_info, 1);
    }

    ret = syscfg_read(CFG_RCSP_ADV_ANC_VOICE, anc_vm_info + 1, sizeof(anc_vm_info) - 1);
    if (0 != memcmp(anc_vm_info + 1, anc_info + 1, sizeof(anc_vm_info) - 1)) {
        ret = syscfg_write(CFG_RCSP_ADV_ANC_VOICE, anc_info + 1, sizeof(anc_vm_info) - 1);
    }
}

static u8 iot_count_the_num_of_positions(u16 value)
{
    u8 cnt;
    for (cnt = 0; value; value = value & (value - 1), cnt++);
    return cnt;
}

static void iot_anc_voice_effect_set(u8 *anc_setting, u8 mode, s16 value)
{
    static s16 value_old = -1;
    log_info("aiot %s, %d", __FUNCTION__, __LINE__);
#if TCFG_AUDIO_ANC_ENABLE
    if (value != value_old) {
        log_info("aiot %s, %d", __FUNCTION__, __LINE__);
        audio_anc_fade_gain_set(value);
    }
    if (anc_setting) {
        if (-1 == value_old) {
            anc_mode_switch(mode + 1, 0);
        } else {
            anc_mode_switch(mode + 1, 1);
        }
    } else {
#if TCFG_USER_TWS_ENABLE
        if ((-1 == value_old) || (get_bt_tws_connect_status() && (iot_count_the_num_of_positions(iot_get_anc_event_status()) > 1))) {
#else
        if (-1 == value_old) {
#endif
            log_info("aiot %s, %d", __FUNCTION__, __LINE__);
            anc_mode_switch(mode + 1, 0);
        } else {
            log_info("aiot %s, %d", __FUNCTION__, __LINE__);
            anc_mode_switch(mode + 1, 1);
        }
    }
#endif
    value_old = value;
}

static bool iot_check_pos_neg(u8 mode)
{
    bool ret = false;
    u8 offset = 1 + mode * 8;
#if TCFG_USER_TWS_ENABLE
    if ('R' == tws_api_get_local_channel()) {
        offset += 2;
    }
#endif
    s16 val = g_anc_info[offset] << 8 | g_anc_info[offset + 1];
    if (val < 0) {
        ret = true;
    }
    return ret;
}

static void iot_anc_effect_state_update(u8 *anc_setting)
{
    u8 offset = 0;
    u8 mode = g_anc_info[offset++];
    offset += mode * 8;
    s16 left_max = g_anc_info[offset] << 8 | g_anc_info[offset + 1];
    s16 right_max = g_anc_info[offset + 2] << 8 | g_anc_info[offset + 3];
    s16 left_val = g_anc_info[offset + 4] << 8 | g_anc_info[offset + 5];
    s16 right_val = g_anc_info[offset + 6] << 8 | g_anc_info[offset + 7];
    offset += 8;

#if TCFG_USER_TWS_ENABLE
    if ('R' == tws_api_get_local_channel()) {
        if (iot_check_pos_neg(mode) && right_val >= right_max) {
            iot_anc_voice_effect_set(anc_setting, mode, right_val);
        } else if (right_max >= right_val) {
            iot_anc_voice_effect_set(anc_setting, mode, right_val);
        }
        return;
    }
#endif

    if (iot_check_pos_neg(mode) && left_val >= left_max) {
        iot_anc_voice_effect_set(anc_setting, mode, left_val);
    } else if (left_max >= left_val) {
        iot_anc_voice_effect_set(anc_setting, mode, left_val);
    }
}

#if TCFG_USER_TWS_ENABLE
#define IOT_TWS_FUNC_ID_ANC_INFO_SYNC \
	(((u8)('I' + 'O' + 'T') << (3 * 8)) | \
	 ((u8)('A' + 'N' + 'C') << (2 * 8)) | \
	 ((u8)('I' + 'N' + 'F' + 'O') << (1 * 8)) | \
	 ((u8)('S' + 'Y' + 'N' + 'C') << (0 * 8)))

/**
 * @brief 更新缓存anc数据到vm
 */
void iot_anc_effect_update_vm_value()
{
    iot_update_anc_effect_vm_value(g_anc_info);
}

static void iot_anc_voice_state_sync(u8 *data, u16 len)
{
    log_info("tws rx:\n");
    log_info_hexdump(data, len);
    if (NULL == data || 0 == len) {
        return;
    }
    u8 offset = 9;
    // 假如当前是右耳，应该填写左值
    // 假如当前是左耳，应该填写右值
    if ('L' == tws_api_get_local_channel()) {
        offset += 2;
    }
    // 根据左右耳填充对应的最大值信息
    // 降噪(2byte) + 通透(2byte)
    memcpy(g_anc_info + offset, data, 2);
    memcpy(g_anc_info + offset + 4, data + 4, 2);
    offset += 8;
    memcpy(g_anc_info + offset, data + 2, 2);
    memcpy(g_anc_info + offset + 4, data + 6, 2);
    iot_msg_send_to_app_core(IOT_MSG_TYPE_UPDATE_ANC_VOICE_MAX_SYNC);
}

static void iot_anc_effect_tws_func_t(void *data, u16 len, bool rx)
{
    if (rx) {
        iot_anc_voice_state_sync((u8 *)data, len);
    }
}

REGISTER_TWS_FUNC_STUB(adv_tws_sync) = {
    .func_id = IOT_TWS_FUNC_ID_ANC_INFO_SYNC,
    .func    = iot_anc_effect_tws_func_t,
};

// 同步anc effect到对耳
static void iot_anc_effect_tws_sync(u8 *anc_info)
{
    log_info("aiot %s, %d", __FUNCTION__, __LINE__);
    if (get_bt_tws_connect_status()) {
        log_info("aiot %s, %d", __FUNCTION__, __LINE__);
        iot_func_tws_sync_to_sibling(BIT(IotFuncTwsSyncTypeAncEffect));
    }
}
#endif

/**
 * @brief 处理anc效果
 *
 * @param anc_setting[25] anc效果
 * @param write_vm 是否写vm
 * @param tws_sync 是否tws同步
 */
void iot_deal_anc_effect(u8 *anc_setting, u8 write_vm, u8 tws_sync)
{
    u8 anc_info[25] = {0};
    if (!anc_setting) {
        iot_get_anc_effect_info(anc_info);
    } else {
        u8 mode = anc_setting[0];
        g_anc_info[0] = mode;
        memcpy(g_anc_info + 1 + mode * 8, anc_setting + 1, 8);
        memcpy(anc_info, g_anc_info, sizeof(anc_info));
    }
    if (write_vm) {
        iot_update_anc_effect_vm_value(anc_info);
    }

#if TCFG_USER_TWS_ENABLE
    if (tws_sync) {
        log_info("aiot %s, %d", __FUNCTION__, __LINE__);
        iot_anc_effect_tws_sync(anc_info);
    }

    if (get_bt_tws_connect_status()) {
        if (TWS_ROLE_MASTER == tws_api_get_role()) {
            iot_anc_effect_state_update(anc_info);
        } else if (0 == tws_sync) {
            iot_anc_effect_state_update(anc_info);
        }
    } else
#endif
    {
        iot_anc_effect_state_update(anc_info);
    }
    // 更新状态
    iot_enable_anc_event();
}

/**
 * @brief 设置anc模式
 *
 * @param mode - 模式，0 - 关闭模式，1 - 降噪模式，2 - 通透模式
 */
void iot_anc_mode_update(u8 mode)
{
    u8 anc_info[25] = {0};
    iot_get_anc_effect_info(anc_info);
    anc_info[0] = mode;
    if (mode) {
        memcpy(anc_info + 1, anc_info + 1 + mode * 8, 8);
    }

    iot_deal_anc_effect(anc_info, 1, 1);
#if TCFG_USER_TWS_ENABLE
    log_info("aiot %s, %d", __FUNCTION__, __LINE__);
    if (get_bt_tws_connect_status() && TWS_ROLE_SLAVE == tws_api_get_role()) {
        log_info("aiot %s, %d", __FUNCTION__, __LINE__);
        tws_api_send_data_to_sibling(NULL, 0, IOT_TWS_FUNC_ID_ANC_INFO_SYNC);
        return;
    }
#endif

}

/**
 * @brief 获取anc模式对应的音效数据
 *
 * @param mode - 模式，0 - 关闭模式，1 - 降噪模式，2 - 通透模式
 * @result 返回数据，0是失败
 */
u16 iot_anc_effect_value_get(u8 mode)
{
    if ((mode > 3) && (mode < 1)) {
        log_info("%s, %d, mode err!", __FUNCTION__, __LINE__);
        return 16384;
    }
    mode -= 1;
    u8 anc_vm_info[24] = {0};
    if (sizeof(anc_vm_info) != syscfg_read(CFG_RCSP_ADV_ANC_VOICE, anc_vm_info, sizeof(anc_vm_info))) {
        return 16384;
    }
    u8 *anc_ptr = anc_vm_info + mode * 8;
    u8 type = 0;
#if TCFG_USER_TWS_ENABLE
    type = ('R' == tws_api_get_local_channel()) ? 1 : 0;
#endif/*TCFG_USER_TWS_ENABLE*/
    switch (type) {
    case 1:
        // 右耳
        anc_ptr += 2;
    case 0:
        // 左耳
        break;
    default:
        return 16384;
    }
    return *((u16 *)anc_ptr);
}

/**
 * @brief anc效果同步，该函数一般用于初始化或者tws连接的时候
 */
int iot_anc_effect_setting_sync()
{
    log_info("aiot %s, %d", __FUNCTION__, __LINE__);
    int ret = 0;
    u8 anc_info[8] = {0};
#if TCFG_AUDIO_ANC_ENABLE
#ifdef CONFIG_CPU_BR30
    u16 noise_reduction = (u16)audio_anc_fade_gain_get();		// 调用接口获取降噪最大值
    u16 transparent = (u16)audio_anc_fade_gain_get();			// 调用接口获取通透最大值
#else
    u16 noise_reduction = 16384;
    u16 transparent = 16384;
#endif
#else
    u16 noise_reduction = 0;
    u16 transparent = 0;
#endif

    anc_info[0] = ((u8 *)&noise_reduction)[1];
    anc_info[1] = ((u8 *)&noise_reduction)[0];
    anc_info[2] = ((u8 *)&transparent)[1];
    anc_info[3] = ((u8 *)&transparent)[0];

    u8 offset = 9;
#if TCFG_USER_TWS_ENABLE
    if ('R' == tws_api_get_local_channel()) {
        offset += 2;
    }
#endif

    // 初始值最大和当前值都是同一个值
    // 默认是左值
    u8 tmp_value = -1;
    int result = 0;
    if ((result = syscfg_read(CFG_RCSP_ADV_ANC_VOICE_MODE, &tmp_value, sizeof(tmp_value))) != sizeof(tmp_value)) {
        tmp_value = offset;
        memcpy(g_anc_info + tmp_value, anc_info, 2);
        tmp_value += 4;
        memcpy(g_anc_info + tmp_value, anc_info, 2);
        tmp_value += 4;
        memcpy(g_anc_info + tmp_value, anc_info + 2, 2);
        tmp_value += 4;
        memcpy(g_anc_info + tmp_value, anc_info + 2, 2);
    }

#if TCFG_USER_TWS_ENABLE
    // 填写当前降噪/通透当前值
    memcpy(anc_info + 4, g_anc_info + offset + 4, 2);
    memcpy(anc_info + 6, g_anc_info + offset + 8 + 4, 2);
    // 把自己的最大值发送给对端
    if (get_bt_tws_connect_status()) {
        log_info("aiot %s, %d", __FUNCTION__, __LINE__);
        tws_api_send_data_to_sibling(anc_info, sizeof(anc_info), IOT_TWS_FUNC_ID_ANC_INFO_SYNC);
    }
#endif
    return ret;
}

/**
 * 初始化anc效果
 */
int iot_anc_effect_setting_init(void)
{
    int ret = 1;
    u8 anc_mode = -1;
    if (syscfg_read(CFG_RCSP_ADV_ANC_VOICE_MODE, &anc_mode, sizeof(anc_mode)) != sizeof(anc_mode)) {
        log_info("aiot %s, %d", __FUNCTION__, __LINE__);
        iot_anc_effect_setting_sync();
        ret = 0;
    } else {
        log_info("aiot %s, %d", __FUNCTION__, __LINE__);
        u8 anc_voice_setting[25] = {0};
        if (iot_read_data_from_vm(CFG_RCSP_ADV_ANC_VOICE_MODE, anc_voice_setting, 1)) {
            log_info("aiot %s, %d", __FUNCTION__, __LINE__);
            if (iot_read_data_from_vm(CFG_RCSP_ADV_ANC_VOICE, anc_voice_setting + 1, sizeof(anc_voice_setting) - 1)) {
                log_info("aiot %s, %d", __FUNCTION__, __LINE__);
                log_info_hexdump(anc_voice_setting, 25);
                iot_set_anc_effect_info(anc_voice_setting);
                iot_deal_anc_effect(NULL, 0, 0);
            }
        }
        ret = 0;
    }
    return ret;
}


#endif // CONFIG_ANC_ENABLE

#endif // (THIRD_PARTY_PROTOCOLS_SEL == LL_SYNC_EN)
