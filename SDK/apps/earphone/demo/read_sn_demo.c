#include "typedef.h"
#include "fs/sdfile.h"
#include "cpu/periph/crc.h"

#define SN_DATA_OFFSET             (64 + 16)
#define SN_DATA_MAX_SIZE           (132 - 4)

struct flash_sn {
    u16 crc;
    u16 len;
    u8 data[0];
};


int flash_get_sn_data(u8 *sn_data)
{
    u32 flash_capacity = sdfile_get_disk_capacity();
    u32 flash_addr = flash_capacity - 256 + SN_DATA_OFFSET;
    u8 *sn_ptr;
    u16 sn_crc;
    struct flash_sn *flash_sn_info;

    /* printf("flash capacity:%x \n", flash_capacity); */
    sn_ptr = (u8 *)sdfile_flash_addr2cpu_addr(flash_addr);

    flash_sn_info = (struct flash_sn *)sn_ptr;

    if (flash_sn_info->len <= SN_DATA_MAX_SIZE) {
        sn_crc = CRC16(&flash_sn_info->len, flash_sn_info->len + 2);
        /* printf("sn crc 0x%x : 0x%x\n",sn_crc,flash_sn_info->crc); */
        if (sn_crc == flash_sn_info->crc) {
            memcpy(sn_data, flash_sn_info->data, flash_sn_info->len);
            return flash_sn_info->len;
        }
    }

    return 0;
}


