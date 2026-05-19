#ifndef _DB_UPDATE_API_H_
#define _DB_UPDATE_API_H_

/* @brief:struct for firmware crc16&crc32
*/
struct crc_info {
    u32 crc32;
    u16 crc16;
};

/* @brief:Initializes the update task,and setting the crc value and file size of new fw;
 * @param max_ptk_len: Supported maxium length of every programming,it decides the max size of programming every time
 */
int db_update_init(u16 max_pkt_len);

/* @brief:exit the update task
 * @param priv:reserved
 */
int db_update_exit();

/* @brief:get the local flash.bin crc
 * @param lcrc: local crc_info write back
 * @return: 0 if no err occurred
 */
int db_update_local_crc_info(struct crc_info *lcrc);

/* @brief:prase the db package crc_info
 * @param fst_pkt: the first packet of the db-package , which includes the header information
 * @param fst_len: first packet len ,at least 128
 * @param dcrc: db-package crc_info write back
 * @return: 0 if no err occurred
 */
int db_update_db_package_crc_info(u8 *fst_pkt, u32 fst_len, struct crc_info *dcrc);


/* @brief:Judge whether enough space for new fw file
 * @note: it should be called after db_update_init(...);
 * @param fw_size:fw size of new fw file
 */
int db_update_allow_check(u32 fw_size);


/* @brief:copy the data to temporary buffer and notify task to write non-volatile storage
 * @param data:the pointer to download data
 * @param len:the length to download data, no larger than max_pkt_len
 * @param write_complete_cb:callback for programming done,return 0 if no err occurred
*/
int db_update_write(void *data, u16 len, int (*write_complete_cb)(int err));

/* @brief:read the updated fw data
 * @param data:the pointer to read buff
 * @param len:the length to read
 * @param offset:offset base on target bank
*/
int db_update_read(void *data, u32 len, u32 offset);


/* @brief:After the new fw verification succeed,call this api to program the new boot info for new fw
 * @param bak_addr:bakup area for firmware-patch process
 * @param bak_size:bak area size
 * @param burn_boot_info_result_hdl:this callback for error notification
 *                                  if err equals 0,the operate to burn boot info succeed,other value means to fail.
 */
int db_update_burn_boot_info(u32 bak_addr, u32 bak_size, int (*burn_boot_info_result_hdl)(int err));

/* @brief:Get the update target bank base and size
 * @param bank_base: update target bank base
 * @param bank_size: update target bank size
 *                                  if err equals 0 succeed,other value means to fail.
 */
int db_update_target_base(u32 *bank_base, u32 *bank_size);

/* @brief:Verify the db update process is correct
 * @param verify_result_hdl:this callback for error notification
 *                                  if err equals 0,the operate to burn boot info succeed,other value means to fail.
 */
int db_update_verify(int (*verify_result_hdl)(int ret));

/* @brief:Try to update again without tranfer package
 * @param bak_addr:bakup area for firmware-patch process
 * @param bak_size:bak area size
 * @param try_result_hdl:this callback for error notification
 *                                  ret equals 1 mean can update, 0 mean can not ,other mean error.
 */
int db_update_retry(u32 bak_addr, u32 bak_size, int (*try_result_hdl)(int ret));


/* @brief:Break last update param
 * ret equals 1 mean can update, 0 mean can not ,other mean error.
 */
int db_update_break_last_update_param(void);

/* @brief:Erase update area advance
 * @param erase_state_pause:this callback for if erase pause notification
 *                                  ret equals 1 mean erase end, 0 mean erasing
 */
int db_update_erase_advance(int (*erase_state_pause)(int ret));

/* @brief:Erase whole update area early
 */
void db_update_erase_all(void);


#endif

