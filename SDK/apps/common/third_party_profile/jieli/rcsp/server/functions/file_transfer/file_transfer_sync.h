#ifndef __FILE_TRANSFER_SYNC_H__
#define __FILE_TRANSFER_SYNC_H__

#include "typedef.h"
#include "classic/tws_api.h"
#include "system/includes.h"

typedef struct file_trans_tws_op_t {
    void *(*ft_open)(const char *path, const char *mode);
    int (*ft_erase)(void *file, u32 erase_addr, u32 erase_size);
    int (*ft_write)(void *buf, u32 size, u32 count, void *file);
    int (*ft_seek)(void *file, int offset, int orig);
    int (*ft_slave_file_check)(u16 crc, u32 size);
    int (*ft_fill_filp)(void *file);
    int (*ft_last_pack_set)(u8 flag);
    void (*ft_close)(void);
    u16 start_timerout;
} file_trans_tws_op;

u8 file_trans_init_tws_sync(void);
u8 file_trans_special_flag_tws_sync(u8 flag);
u8 file_trans_request_sem(u8 cmd);
void file_trans_open_tws_sync_to_slave(const char *path, const char *mode);
u8 file_trans_open_tws_sync_pend(void);
void file_trans_close_tws_sync_to_slave(void);
u8 file_trans_close_tws_sync_pend(void);
void file_trans_erase_tws_sync_to_slave(u32 erase_addr, u32 erase_size);
u8 file_trans_erase_tws_sync_pend(void);
void file_trans_write_tws_sync_to_slave(void *buf, u32 size, u32 count);
u8 file_trans_write_tws_sync_pend(void);
void file_trans_calc_crc_tws_sync_to_slave(u16 crc, u32 size);
u8 file_trans_calc_crc_tws_sync_pend(void);
void file_trans_slave_calc_crc_tws_sync_post(u8 reason);
u8 file_trans_seek_tws_sync(int file_offset, int orig);
u8 file_trans_parm_extra_tws_sync(u8 *data, u16 len);

u32 file_trans_init(u8 type, u16 ble_con_handle, u8 *spp_remote_addr);
u32 file_trans_rename(u8 status, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);
u32 file_trans_handle(u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);
u32 file_trans_end(u8 status, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);
u32 file_trans_start(void *priv, u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);
u32 file_trans_parm_extra(u8 OpCode_SN, u8 *data, u16 len, u16 ble_con_handle, u8 *spp_remote_addr);
#endif
