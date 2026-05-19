#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".tuya_app_func.data.bss")
#pragma data_seg(".tuya_app_func.data")
#pragma const_seg(".tuya_app_func.text.const")
#pragma code_seg(".tuya_app_func.text")
#endif
#include "app_config.h"
#include "tuya_ble_app_demo.h"
#include "effects/eq_config.h"
#include "audio_config.h"
#include "app_tone.h"
#include "vol_sync.h"
#include "key_driver.h"
#include "app_msg.h"
#include "bt_tws.h"
#include "btstack/avctp_user.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & TUYA_DEMO_EN)

static struct VM_INFO_MODIFY {
    char eq_info[11];
    char bt_name[LOCAL_NAME_LEN];
    u8 key_recored[2][6];
} vm_info_modify;

static struct TUYA_SYNC_INFO tuya_sync_info;


/*********************************************************/
/* 涂鸦功能同步到对耳 */
/* tuya_info:对应type的同步数据 */
/* 不同type的info都放到tuya_sync_info进行统一同步 */
/* data_type:对应不同功能 */
/* 索引号对应APP_TWS_TUYA_SYNC_XXX (tuya_ble_app_demo.h) */
/*********************************************************/
void tuya_sync_info_send(void *tuya_info, u8 data_type)
{
#if TCFG_USER_TWS_ENABLE
    tuya_sync_flag_update_before_send(&tuya_sync_info);
    if (get_bt_tws_connect_status()) {
        printf("data_type:%d\n", data_type);
        switch (data_type) {
        case APP_TWS_TUYA_SYNC_EQ:
            printf("sync eq info!\n");
            memcpy(tuya_sync_info.eq_info, tuya_info, eq_get_table_nsection(EQ_MODE_CUSTOM) + 1);
            tuya_sync_info.tuya_eq_flag = 1;
            break;
        case APP_TWS_TUYA_SYNC_ANC:
            printf("sync anc info!\n");
            break;
        case APP_TWS_TUYA_SYNC_VOLUME:
            tuya_sync_info.volume = *((u8 *)tuya_info);
            tuya_sync_info.volume_flag = 1;
            printf("sync volume info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R1:
            tuya_sync_info.key_r1 = *((u8 *)tuya_info);
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_r1 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R2:
            tuya_sync_info.key_r2 = *((u8 *)tuya_info);
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_r2 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_R3:
            tuya_sync_info.key_r3 = *((u8 *)tuya_info);
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_r3 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L1:
            tuya_sync_info.key_l1 = *((u8 *)tuya_info);
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_l1 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L2:
            tuya_sync_info.key_l2 = *((u8 *)tuya_info);
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_l2 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_KEY_L3:
            tuya_sync_info.key_l3 = *((u8 *)tuya_info);
            tuya_sync_info.key_change_flag = 1;
            printf("sync key_l3 info!\n");
            break;
        case APP_TWS_TUYA_SYNC_FIND_DEVICE:
            tuya_sync_info.find_device = *((u8 *)tuya_info);
            printf("sync find_device info!\n");
            break;
        case APP_TWS_TUYA_SYNC_DEVICE_CONN_FLAG:
            tuya_sync_info.device_conn_flag = *((u8 *)tuya_info);
            printf("sync device_conn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_DEVICE_DISCONN_FLAG:
            tuya_sync_info.device_disconn_flag = *((u8 *)tuya_info);
            printf("sync device_disconn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_PHONE_CONN_FLAG:
            tuya_sync_info.phone_conn_flag = *((u8 *)tuya_info);
            printf("sync phone_conn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_PHONE_DISCONN_FLAG:
            tuya_sync_info.phone_disconn_flag = *((u8 *)tuya_info);
            printf("sync phone_disconn_flag info!\n");
            break;
        case APP_TWS_TUYA_SYNC_BT_NAME:
            printf("sync bt name!\n");
            memcpy(tuya_sync_info.bt_name, tuya_info, LOCAL_NAME_LEN);
            tuya_sync_info.tuya_bt_name_flag = 1;
            printf("tuya bt name:%s\n", tuya_sync_info.bt_name);
            break;
        case APP_TWS_TUYA_SYNC_KEY_RESET:
            printf("sync key_reset!\n");
            tuya_sync_info.key_reset = 1;
            break;
        default:
            break;
        }
        /* if (tws_api_get_role() == TWS_ROLE_MASTER) { */
        printf("this is tuya master!\n");
        u8 status = tws_api_send_data_to_sibling(&tuya_sync_info, sizeof(tuya_sync_info), TWS_FUNC_ID_TUYA_STATE);
        printf("status:%d\n", status);
        /* } */
    }
#endif
}

void tuya_sync_flag_update_before_send(struct TUYA_SYNC_INFO *tuya_sync_info)
{
    tuya_sync_info->key_reset = 0;
    tuya_sync_info->tuya_eq_flag = 0;
    tuya_sync_info->volume_flag = 0;
    tuya_sync_info->tuya_bt_name_flag = 0;
    tuya_sync_info->key_change_flag = 0;
    tuya_sync_info->device_conn_flag = 0;
    tuya_sync_info->device_disconn_flag = 0;
    tuya_sync_info->phone_conn_flag = 0;
    tuya_sync_info->phone_disconn_flag = 0;
    tuya_sync_info->key_r1 = key_table_r[0];
    tuya_sync_info->key_r2 = key_table_r[2];
    tuya_sync_info->key_r3 = key_table_r[4];
    tuya_sync_info->key_l1 = key_table_l[0];
    tuya_sync_info->key_l2 = key_table_l[2];
    tuya_sync_info->key_l3 = key_table_l[4];
}

void find_device()
{
    play_tone_file(get_tone_files()->normal);
}

void tuya_set_music_volume(int volume)
{
    s16 music_volume;
    u8 flag = 1;
    music_volume = ((volume + 1) * 16) / 100;
    music_volume--;
    if (music_volume < 0) {
        music_volume = 0;
        flag = 0;
    }
    printf("phone_vol:%d,dac_vol:%d", volume, music_volume);
    opid_play_vol_sync_fun(&music_volume, flag);
    bt_cmd_prepare(USER_CTRL_AVCTP_OPID_SEND_VOL, 0, NULL);
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, music_volume, 1);
}

void tuya_eq_data_deal(char *eq_info_data)
{
    int data;
    // 自定义修改EQ参数
    for (u16 i = 0; i < eq_get_table_nsection(EQ_MODE_CUSTOM); i++) {
        data = eq_info_data[i];
        eq_mode_set_custom_param(i, data);
    }
    eq_mode_set(EQ_MODE_CUSTOM);
}

/* 涂鸦app对应功能索引映射到sdk按键枚举 */
u8 tuya_key_event_swith(u8 event)
{
    u8 ret = 0;
    switch (event) {
    case 0:
        ret = APP_MSG_VOL_DOWN;
        break;
    case 1:
        ret = APP_MSG_VOL_UP;
        break;
    case 2:
        ret = APP_MSG_MUSIC_NEXT;
        break;
    case 3:
        ret = APP_MSG_MUSIC_PREV;
        break;
    case 4:
        ret = APP_MSG_MUSIC_PP;
        break;
    case 5:
    case 6:
        ret = APP_MSG_OPEN_SIRI;
        break;
    }
    return ret;
}

void tuya_change_bt_name(char *name, u8 name_len)
{
    extern BT_CONFIG bt_cfg;
    extern const char *bt_get_local_name();
    extern void lmp_hci_write_local_name(const char *name);
    memset(bt_cfg.edr_name, 0, name_len);
    memcpy(bt_cfg.edr_name, name, name_len);
    lmp_hci_write_local_name(bt_get_local_name());
}

void tuya_eq_data_setting(char *eq_setting, char eq_mode)
{
    if (!eq_setting) {
        ASSERT(0, "without eq_data!");
    } else {
        char eq_info[11] = {0};
        memcpy(eq_info, eq_setting, 10);
        eq_info[10] = eq_mode;
#if TCFG_USER_TWS_ENABLE
        if (get_bt_tws_connect_status()) {
            printf("start eq sync!");
            tuya_sync_info_send(eq_info, APP_TWS_TUYA_SYNC_EQ);
        }
#endif
        syscfg_write(CFG_RCSP_ADV_EQ_DATA_SETTING, eq_info, sizeof(eq_info));
        tuya_eq_data_deal(eq_setting);
    }
}

void tuya_find_device(u8 data)
{
    static u16 find_device_timer = 0;
    if (data) {
        find_device_timer = sys_timer_add(NULL, find_device, 1000);
    } else {
        sys_timer_del(find_device_timer);
    }
}

void tuya_ctrl_music_state(u8 state)
{
    if (bt_a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
        bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    } else {
        if (state) {
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        } else {
            bt_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        }
    }
}

void tuya_eq_info_deal(__tuya_info tuya_info, char *data)
{
    tuya_info.eq_info.eq_mode = data[2];
    printf("tuya eq_mode:0x%x\n", tuya_info.eq_info.eq_mode);
    memcpy(tuya_info.eq_info.eq_data, &data[3], EQ_CNT);
    for (int i = 0; i < 10; i++) {
        tuya_info.eq_info.eq_data[i] -= 0xc0;
        /* printf("tuya eq_data[%d]:%d\n", i, tuya_info.eq_info.eq_data[i]); */
    }
    tuya_eq_data_setting(tuya_info.eq_info.eq_data, tuya_info.eq_info.eq_mode);
}

void tuya_eq_info_reset(__tuya_info tuya_info, u8 *data)
{
    if (data) {
        tuya_info.eq_info.eq_onoff = data[0];
    } else {
        tuya_info.eq_info.eq_onoff = 0;
    }
    for (int i = 0; i < 10; i++) {
        tuya_info.eq_info.eq_data[i] = 0;
    }
    char eq_info[11] = {0};
    tuya_sync_info_send(eq_info, APP_TWS_TUYA_SYNC_EQ);
    eq_mode_set(EQ_MODE_NORMAL);
    syscfg_write(CFG_RCSP_ADV_EQ_DATA_SETTING, eq_info, 11);
}

void tuya_bt_name_deal(u8 *data, u16 data_len)
{
    char *name = malloc(data_len + 1);
    memcpy(name, &data[0], data_len);
    name[data_len] = '\0';
    tuya_change_bt_name(name, LOCAL_NAME_LEN);
    syscfg_write(CFG_BT_NAME, name, LOCAL_NAME_LEN);
    tuya_sync_info_send(name, APP_TWS_TUYA_SYNC_BT_NAME);
    printf("tuya bluetooth name: %s, len:%d\n", name, data_len);
    free(name);
}


void _tuya_tone_deal(char *tone_files)
{
    play_tone_file(tone_files);
}

void tuya_earphone_key_init()
{
    u8 key_value_record[2][6] = {0};
    u8 value = syscfg_read(TUYA_SYNC_KEY_INFO, key_value_record, sizeof(key_value_record));
    eq_mode_set(EQ_MODE_NORMAL);
    if (value == sizeof(key_value_record)) {
        for (int i = 0; i < 6; i++) {
            key_table_l[i] = key_value_record[0][i];
            key_table_r[i] = key_value_record[1][i];
        }
    }
    printf("%d %d %d %d %d %d\n", key_table_l[0], key_table_l[2], key_table_l[4], key_table_r[0], key_table_r[2], key_table_r[4]);
}

void tuya_earphone_key_remap(int *value, int *msg)
{
    struct key_event *key = (struct key_event *)msg;
    int index = key->event;
    g_printf("key_remap: 0x%x, 0x%x, 0x%x, 0x%x\n", index, msg[0], msg[1], APP_KEY_MSG_FROM_TWS);
#if TCFG_USER_TWS_ENABLE
    if (get_bt_tws_connect_status()) {
        if (tws_api_get_local_channel() == 'R') {
            if (msg[1] == APP_KEY_MSG_FROM_TWS) {
                *value = key_table_l[index];
            } else {
                *value = key_table_r[index];
            }
        } else {
            if (msg[1] == APP_KEY_MSG_FROM_TWS) {
                *value = key_table_r[index];
            } else {
                *value = key_table_l[index];
            }
        }
    } else
#endif
    {
        *value = key_table_l[index];
    }
}


void tuya_volume_indicate(s8 volume)
{
    u8 max_vol = app_audio_get_max_volume();
    printf("cur_vol is:%d, max:%d\n", volume, app_audio_get_max_volume());
    u8 tuya_sync_valume = (int)(volume * 100 / max_vol);
    tuya_valume_indicate(tuya_sync_valume);
}

void tuya_tone_post(const char *tone_files)
{
    int argv[3];
    argv[0] = (int)_tuya_tone_deal;
    argv[1] = 1;
    argv[2] = (int)tone_files;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
}

void tuya_eq_info_post(char *eq_info)
{
    int argv[3];
    argv[0] = (int)tuya_eq_data_deal;
    argv[1] = 1;
    argv[2] = (int)eq_info;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
}

void tuya_eq_mode_post(char eq_mode)
{
    int argv[3];
    argv[0] = (int)eq_mode_set;
    argv[1] = 1;
    argv[2] = (int)eq_mode;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
}

void _tuya_vm_info_modify(u8 mode)
{
    u8 value = 0;
    printf("coming in app_core sync tuya_info! mode:%d\n", mode);
    switch (mode) {
    case TUYA_KEY_SYNC_VM:
        printf("write key_info to vm!\n");
        value = syscfg_write(TUYA_SYNC_KEY_INFO, vm_info_modify.key_recored, sizeof(vm_info_modify.key_recored));
        printf("value:%d\n", value);
        break;
    case TUYA_BT_NAME_SYNC_VM:
        printf("write bt_name to vm!:%s\n", vm_info_modify.bt_name);
        value = syscfg_write(CFG_BT_NAME, vm_info_modify.bt_name, LOCAL_NAME_LEN);
        printf("value:%d\n", value);
        break;
    case TUYA_EQ_INFO_SYNC_VM:
        printf("write eq_info to vm!\n");
        value = syscfg_write(CFG_RCSP_ADV_EQ_DATA_SETTING, vm_info_modify.eq_info, 11);
        printf("value:%d\n", value);
        break;
    default:
        printf("key bt_name modify mode err!\n");
        break;
    }
}

void tuya_vm_info_post(char *info, u8 mode, u8 len)
{
    int argv[3];
    switch (mode) {
    case TUYA_KEY_SYNC_VM:
        memcpy(vm_info_modify.key_recored, info, len);
        break;
    case TUYA_BT_NAME_SYNC_VM:
        memcpy(vm_info_modify.bt_name, info, len);
        break;
    case TUYA_EQ_INFO_SYNC_VM:
        memcpy(vm_info_modify.eq_info, info, len);
        break;
    default:
        printf("key bt_name modify mode err!\n");
        break;
    }
    argv[0] = (int)_tuya_vm_info_modify;
    argv[1] = 1;
    argv[2] = mode;
    os_taskq_post_type("app_core", Q_CALLBACK, 3, argv);
}

void tuya_sync_key_info(struct TUYA_SYNC_INFO *tuya_sync_info)
{
    key_table_l[0] = tuya_sync_info->key_l1;
    key_table_l[2] = tuya_sync_info->key_l2;
    key_table_l[4] = tuya_sync_info->key_l3;
    key_table_r[0] = tuya_sync_info->key_r1;
    key_table_r[2] = tuya_sync_info->key_r2;
    key_table_r[4] = tuya_sync_info->key_r3;
}

void tuya_reset_key_info()
{
    key_table_l[0] = tuya_key_event_swith(0);
    key_table_l[2] = tuya_key_event_swith(1);
    key_table_l[4] = tuya_key_event_swith(2);
    key_table_r[0] = tuya_key_event_swith(0);
    key_table_r[2] = tuya_key_event_swith(1);
    key_table_r[4] = tuya_key_event_swith(2);
}

void tuya_update_vm_key_info(u8 key_value_record[][6])
{
    for (int i = 0; i < 6; i++) {
        key_value_record[0][i] = key_table_l[i];
        key_value_record[1][i] = key_table_r[i];
    }
}

#endif

