#ifndef _NANDFLASH_H
#define _NANDFLASH_H

#include "device/device.h"
#include "ioctl_cmds.h"
#include "spi.h"
#include "printf.h"
#include "gpio.h"
#include "device_drive.h"
#include "malloc.h"

typedef enum {
    NAND_SUCCESS = 0,
    NAND_ECC_CORRECTED = 1,
    NAND_ERROR_ECC = 2,
    NAND_ERROR_P_FAIL = 3,
    NAND_ERROR_E_FAIL = 4,
    NAND_ERROR_TIME_OUT = 5,
    NAND_ERROR_ADDR = 6,
    NAND_ERROR_EINVAL = 22, /* Invalid argument */
} NAND_Result;


struct nandflash_dev_platform_data {
    int spi_hw_num;         //只支持SPI1或SPI2
    u32 spi_cs_port;        //cs的引脚
    u32 spi_read_width;     //flash读数据的线宽
    const struct spi_platform_data *spi_pdata;
    u32 start_addr;         //分区起始地址
    u32 size;               //分区大小，若只有1个分区，则这个参数可以忽略
};

#define NANDFLASH_DEV_PLATFORM_DATA_BEGIN(data) \
	const struct nandflash_dev_platform_data data

#define NANDFLASH_DEV_PLATFORM_DATA_END()  \
};

void nandflash_test_demo(void);
extern const struct device_operations nandflash_dev_ops;
extern const struct device_operations ftl_dev_ops;

NAND_Result nand_flash_page_write(uint32_t block, uint32_t page, uint32_t offset, uint8_t *data, uint32_t data_size);
NAND_Result nand_flash_page_read(uint32_t block, uint32_t page, uint32_t column_address, uint8_t *data, uint32_t data_size);
NAND_Result nand_flash_erase(uint32_t addr);

#endif

