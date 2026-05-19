#ifndef __AURACAST_SINK_API_H__
#define __AURACAST_SINK_API_H__

#include "typedef.h"

#define AURACAST_SINK_API_VERSION    (20260408)

// max config
#define NO_PAST_MAX_BASS_NUM_SOURCES 5

typedef struct {
    u8 max_bis_num;
} auracast_sink_user_config_t;

enum {
    FRAME_DURATION_7_5,
    FRAME_DURATION_10,
};

typedef struct {
    uint8_t source_mac_addr[6];
    uint8_t broadcast_name[33];
    uint8_t Address_Type;
    uint8_t Advertising_SID;
    uint8_t feature;
    uint16_t con_handle;
    uint32_t broadcast_id;
    s16 sdu_period;
    s16 frame_len;
    uint8_t enc;
    int sample_rate;
    int bit_rate;
    uint8_t frame_duration;
    uint8_t BIG_Handle;
    uint8_t Num_BIS;
    uint8_t BIS[8];
    uint8_t BIS_CODEC_TRACK[8];
    uint8_t BIS_id;
    uint16_t Connection_Handle[2];
    uint32_t presentation_delay_us;
    uint8_t bn;
    uint8_t broadcast_code[16];
    uint16_t adv_data_len;
    uint8_t adv_data[0];
} auracast_sink_source_info_t;

typedef enum {
    SINK_SCAN_STATE_IDLE = 0x00,
    SINK_SCAN_STATE_EXT_SCANNING,
    SINK_SCAN_STATE_EXT_REPORTED,
    SINK_SCAN_STATE_PADV_SCANNING,
    SINK_SCAN_STATE_PADV_REPORTED,
} auracast_sink_scan_state_t;

typedef enum {
    SINK_BIG_STATE_IDLE = 0x00,
    SINK_BIG_STATE_EXT_SCANNING,
    SINK_BIG_STATE_EXT_REPORTED,
    SINK_BIG_STATE_PADV_TRY_SYNC,
    SINK_BIG_STATE_PADV_SYNC,
    SINK_BIG_STATE_BIG_TRY_SYNC,
    SINK_BIG_STATE_BIG_SYNC,
} auracast_sink_big_state_t;

extern auracast_sink_scan_state_t auracast_sink_scan_state_get(void);
extern auracast_sink_big_state_t auracast_sink_big_state_get(void);
extern auracast_sink_source_info_t *auracast_sink_listening_source_info_get(void);

enum {
    AURACAST_SINK_SOURCE_INFO_REPORT_EVENT,
    AURACAST_SINK_BLE_CONNECT_EVENT,
    AURACAST_SINK_BIG_SYNC_CREATE_EVENT,
    AURACAST_SINK_BIG_SYNC_TERMINATE_EVENT,
    AURACAST_SINK_BIG_SYNC_LOST_EVENT,
    AURACAST_SINK_PERIODIC_ADVERTISING_SYNC_LOST_EVENT,
    AURACAST_SINK_BIG_INFO_REPORT_EVENT,
    AURACAST_SINK_ISO_RX_CALLBACK_EVENT,
    AURACAST_SINK_BIG_SYNC_FAIL_EVENT,
    AURACAST_SINK_EXT_SCAN_COMPLETE_EVENT,
    AURACAST_SINK_PADV_REPORT_EVENT,
};

typedef void (*auracast_sink_event_callback_t)(uint16_t event, uint8_t *packet, uint16_t length);

extern int auracast_sink_init(u32 version);
extern int auracast_sink_uninit(void);
extern int auracast_sink_scan_start(void);
extern int auracast_sink_scan_stop(void);
extern int auracast_sink_big_sync_create(auracast_sink_source_info_t *param);
extern int auracast_sink_big_sync_terminate(void);
extern void auracast_sink_set_broadcast_code(u8 *key);
extern void auracast_sink_event_callback_register(auracast_sink_event_callback_t callback);


////////////////////////////// bass

// event
typedef enum {
    BASS_SERVER_EVENT_SCAN_STOPPED = 0x00,
    BASS_SERVER_EVENT_SCAN_STARTED,
    BASS_SERVER_EVENT_SOURCE_ADDED,
    BASS_SERVER_EVENT_SOURCE_MODIFIED,
    BASS_SERVER_EVENT_SOURCE_DELETED,
    BASS_SERVER_EVENT_BROADCAST_CODE,
} bass_server_event_t;

// pa_sync_state
typedef enum {
    BASS_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA = 0x00,
    BASS_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE,
    BASS_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE,
    BASS_PA_SYNC_RFU
} bass_pa_sync_cmd_t;

struct le_audio_bass_add_source_info_t {
    u32 broadcast_id;
    u8  address[6];
    u8  source_id;
    u8  sid;

    // 4-octet bitfield. Shall not exist if num_subgroups = 0
    // Bit 0-30 = BIS_index[1-31]
    // 0x00000000: 0 = Not synchronized to BIS_index[x]
    // 0xxxxxxxxx: 1 = Synchronized to BIS_index[x]
    // 0xFFFFFFFF: Failed to sync to BIG
    // state send to client by server
    u32  bis_sync_state;

    u8  pa_sync;

};

typedef int (*le_audio_bass_server_event_callback_t)(uint8_t event, uint8_t *packet, uint16_t size);
extern void le_audio_bass_event_callback_register(le_audio_bass_server_event_callback_t callback);


typedef enum {
    BASS_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA = 0x00,
    BASS_PA_SYNC_STATE_SYNCINFO_REQUEST,
    BASS_PA_SYNC_STATE_SYNCHRONIZED_TO_PA,
    BASS_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA,
    BASS_PA_SYNC_STATE_NO_PAST,
    BASS_PA_SYNC_STATE_RFU
} bass_pa_sync_state_t;

typedef enum {
    BASS_BIG_ENCRYPTION_NOT_ENCRYPTED = 0x00,
    BASS_BIG_ENCRYPTION_BROADCAST_CODE_REQUIRED,
    BASS_BIG_ENCRYPTION_DECRYPTING,
    BASS_BIG_ENCRYPTION_BAD_CODE,
    BASS_BIG_ENCRYPTION_RFU
} bass_big_encryption_t;

extern void app_le_audio_bass_notify_pa_sync_state(u8 pa_sync_state, u8 big_encryption, u32 bis_sync_state);


#endif /* __AURACAST_SINK_API_H__ */

