
#include <stdlib.h>
#include <string.h>

#include "typedef.h"
#include "system/includes.h"
#ifndef __DMA_SETTING_H_
#define __DMA_SETTING_H_

#define DMA_LIB_DEBUG   1
#define DMA_LIB_OTA_EN  0
//#define DMA_LIB_CODE_SIZE_CHECK

#if DMA_LIB_DEBUG
extern void printf_buf(u8 *buf, u32 len);
#define dma_log     printf
#define log_info     printf
#define dma_dump    printf_buf
#define log_info_hexdump  printf_buf
#else
#define dma_log(...)
#define dma_dump(...)
#define log_info(...)
#define log_info_hexdump(...)
#endif

#define PRODUCT_ID_LEN      65
#define PRODUCT_KEY_LEN     65
#define TRIAD_ID_LEN        32
#define SECRET_LEN          32
struct dueros_cfg_t {
    char dma_cfg_file_product_id[PRODUCT_ID_LEN];
    char dma_cfg_file_product_key[PRODUCT_KEY_LEN];
    char dma_cfg_file_triad_id[TRIAD_ID_LEN];
    char dma_cfg_file_secret[SECRET_LEN];
    u8 dma_cfg_read_file_flag;
};

#define DMA_TWS_LIB_SEND_MAGIC_NUMBER 0xAA
#define DMA_LIB_DATA_TYPE_DMA         0xA1
#define DMA_LIB_DATA_TYPE_OTA         0xA2

extern void swapX(const uint8_t *src, uint8_t *dst, int len);
extern struct dueros_cfg_t dueros_global_cfg;
int dma_message(int opcode, u8 *data, u32 len);
bool dma_check_tws_is_master();
void dma_send_process_resume(void);
void dma_default_special_info_init();
void server_register_dueros_send_callbak(int (*send_callbak)(void *priv, u8 *buf, u16 len));
int dueros_sent_raw_audio_data(uint8_t *data, uint32_t len);
int dueros_sent_raw_cmd(uint8_t *data,  uint32_t len);
void dma_recieve_data(u8 *buf, u16 len);
void dma_ota_recieve_data(u8 *buf, u16 len);
void dma_cmd_analysis_resume(void);
bool set_dueros_pair_state(u8 new_state, u8 init_flag);
bool set_dueros_pair_state_2(u8 new_state);
bool check_can_send_data_or_not();
void server_register_dueros_ota_send_callbak(int (*send_callbak)(void *priv, u8 *buf, u16 len));
int dma_pair_state();
int dma_tws_peer_send(u8 type, u8 *data, int len);
void dma_ota_peer_recieve_data(u8 *buf, u16 len);
int dma_check_some_status(int flag);
void dma_tws_sync_pair_state();

// ble api
extern void *dma_app_ble_hdl;
extern int dma_ble_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type);

// spp spi
extern void *dma_app_spp_hdl;

#endif
